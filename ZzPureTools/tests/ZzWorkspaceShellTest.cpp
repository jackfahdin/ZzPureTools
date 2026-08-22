#include <array>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>

#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzSidePaneMode.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

namespace {

constexpr qsizetype zzWorkspaceMaximumLayoutSize = qsizetype{1024} * 1024;

/** @brief 在组件接管对象后释放测试夹具持有的临时所有权。 */
template <typename T>
void zzReleaseAfterAdoption(std::unique_ptr<T> &object) noexcept
{
    [[maybe_unused]] T *const adoptedObject = object.release();
}

struct ZzShellFixture final
{
    QMainWindow host;
    ZzFluentUI::ZzFluentTitleBar titleBar{&host};
    std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;

    ZzShellFixture()
    {
        auto result = ZzPureTools::ZzWorkspaceShell::create(
            &host, &titleBar);
        Q_ASSERT(result);
        shell = std::move(result).value();
    }
};

[[nodiscard]] ZzPureTools::ZzWorkspacePanelId zzPanelId(
    const char *value)
{
    return ZzPureTools::ZzWorkspacePanelId(QString::fromLatin1(value));
}

[[nodiscard]] ZzFluentUI::ZzIconDescriptor zzIcon()
{
    return {};
}

[[nodiscard]] QByteArray zzReplaceQtState(
    const QByteArray &layout,
    const QByteArray &replacement)
{
    QByteArray headerPayload;
    QDataStream outer(layout);
    outer.setVersion(QDataStream::Qt_6_8);
    char magic[4]{};
    quint16 schemaVersion = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (outer.readRawData(magic, 4) != 4) {
        return {};
    }
    outer >> schemaVersion >> streamVersion >> payloadLength;
    headerPayload.resize(static_cast<qsizetype>(payloadLength));
    if (outer.readRawData(
            headerPayload.data(), static_cast<int>(payloadLength))
        != static_cast<int>(payloadLength)) {
        return {};
    }

    QByteArray originalQtState;
    QDataStream payloadIn(headerPayload);
    payloadIn.setVersion(QDataStream::Qt_6_8);
    payloadIn >> originalQtState;
    const qsizetype qtStateFieldSize = payloadIn.device()->pos();
    QByteArray newPayload;
    QDataStream payloadOut(&newPayload, QIODevice::WriteOnly);
    payloadOut.setVersion(QDataStream::Qt_6_8);
    payloadOut << replacement;
    newPayload.append(headerPayload.sliced(qtStateFieldSize));

    QByteArray result;
    QDataStream resultOut(&result, QIODevice::WriteOnly);
    resultOut.setVersion(QDataStream::Qt_6_8);
    if (resultOut.writeRawData("ZZWS", 4) != 4) {
        return {};
    }
    resultOut << schemaVersion << streamVersion
              << static_cast<quint32>(newPayload.size());
    if (resultOut.writeRawData(newPayload.constData(), newPayload.size())
        != newPayload.size()) {
        return {};
    }
    result.append(QCryptographicHash::hash(
        newPayload, QCryptographicHash::Sha256));
    return result;
}

[[nodiscard]] QByteArray zzLayoutWithSideEntries(
    const QByteArray &layout,
    quint32 sideCount,
    const QString &tailDuplicateId = {})
{
    QDataStream outer(layout);
    outer.setVersion(QDataStream::Qt_6_8);
    char magic[4]{};
    quint16 schemaVersion = 0;
    quint16 streamVersion = 0;
    quint32 payloadLength = 0;
    if (outer.readRawData(magic, 4) != 4) {
        return {};
    }
    outer >> schemaVersion >> streamVersion >> payloadLength;
    QByteArray payload(static_cast<qsizetype>(payloadLength), Qt::Uninitialized);
    if (outer.readRawData(payload.data(), static_cast<int>(payloadLength))
        != static_cast<int>(payloadLength)) {
        return {};
    }

    QByteArray qtState;
    bool leftCollapsed = false;
    bool rightCollapsed = false;
    qint32 leftWidth = 0;
    qint32 rightWidth = 0;
    QString leftCurrent;
    QString rightCurrent;
    quint32 originalSideCount = 0;
    qint32 currentTabIndex = -1;
    quint8 titleMode = 0;
    QDataStream payloadIn(payload);
    payloadIn.setVersion(QDataStream::Qt_6_8);
    payloadIn >> qtState >> leftCollapsed >> leftWidth >> rightCollapsed
              >> rightWidth >> leftCurrent >> rightCurrent >> originalSideCount;
    for (quint32 index = 0; index < originalSideCount; ++index) {
        QString id;
        quint8 area = 0;
        qint32 order = 0;
        payloadIn >> id >> area >> order;
    }
    payloadIn >> currentTabIndex >> titleMode;
    if (payloadIn.status() != QDataStream::Ok || !payloadIn.atEnd()) {
        return {};
    }

    QByteArray replacementPayload;
    QDataStream payloadOut(&replacementPayload, QIODevice::WriteOnly);
    payloadOut.setVersion(QDataStream::Qt_6_8);
    payloadOut << qtState << leftCollapsed << leftWidth << rightCollapsed
               << rightWidth << leftCurrent << rightCurrent << sideCount;
    for (quint32 index = 0; index < sideCount; ++index) {
        const QString id = index + 1 == sideCount && !tailDuplicateId.isEmpty()
            ? tailDuplicateId
            : QStringLiteral("side-%1").arg(index, 4, 10, QLatin1Char('0'));
        payloadOut << id
                   << static_cast<quint8>(
                          ZzFluentUI::ZzActivityArea::LeftPrimary)
                   << static_cast<qint32>(index);
    }
    payloadOut << currentTabIndex << titleMode;
    if (payloadOut.status() != QDataStream::Ok) {
        return {};
    }

    QByteArray result;
    QDataStream resultOut(&result, QIODevice::WriteOnly);
    resultOut.setVersion(QDataStream::Qt_6_8);
    if (resultOut.writeRawData("ZZWS", 4) != 4) {
        return {};
    }
    resultOut << schemaVersion << streamVersion
              << static_cast<quint32>(replacementPayload.size());
    if (resultOut.writeRawData(
            replacementPayload.constData(), replacementPayload.size())
        != replacementPayload.size()) {
        return {};
    }
    result.append(QCryptographicHash::hash(
        replacementPayload, QCryptographicHash::Sha256));
    return result;
}

[[nodiscard]] QByteArray zzMutatedByte(
    QByteArray state,
    qsizetype offset,
    char value)
{
    Q_ASSERT(offset >= 0 && offset < state.size());
    state[offset] = value;
    return state;
}

class ZzParentChangeWidget final : public QWidget
{
public:
    std::function<void()> parentChanged;

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if (event != nullptr && event->type() == QEvent::ParentChange
            && parentWidget() != nullptr && parentChanged) {
            parentChanged();
        }
        return handled;
    }
};

class ZzParentRemovedWidget final : public QWidget
{
public:
    std::function<void()> parentRemoved;

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if (event != nullptr && event->type() == QEvent::ParentChange
            && parentWidget() == nullptr && parentRemoved) {
            parentRemoved();
        }
        return handled;
    }
};

/** @brief 在销毁 QObject 子对象前模拟 QMainWindow 已释放内部布局的阶段。 */
class ZzLayoutTornDownMainWindow final : public QMainWindow
{
public:
    ~ZzLayoutTornDownMainWindow() override
    {
        delete layout();
    }
};

} // namespace

/** @brief 验证 Workspace Shell 的装配、所有权、标题和布局事务。 */
class ZzWorkspaceShellTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void validatesFactoryInputsAndThreadsBeforeAllocation()
    {
        auto nullHost = ZzPureTools::ZzWorkspaceShell::create(nullptr);
        QVERIFY(!nullHost);
        QCOMPARE(nullHost.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        QMainWindow outer;
        QMainWindow nested(&outer);
        auto nestedHost = ZzPureTools::ZzWorkspaceShell::create(&nested);
        QVERIFY(!nestedHost);
        QCOMPARE(
            nestedHost.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        QMainWindow host;
        QMainWindow otherHost;
        ZzFluentUI::ZzFluentTitleBar foreignTitleBar(&otherHost);
        auto foreignTitle = ZzPureTools::ZzWorkspaceShell::create(
            &host, &foreignTitleBar);
        QVERIFY(!foreignTitle);
        QCOMPARE(
            foreignTitle.error().code(), ZzCore::ZzErrorCode::InvalidArgument);

        bool crossThreadRejected = false;
        ZzCore::ZzErrorCode crossThreadCode = ZzCore::ZzErrorCode::None;
        std::thread worker([&] {
            auto result = ZzPureTools::ZzWorkspaceShell::create(&host);
            crossThreadRejected = !result;
            if (!result) {
                crossThreadCode = result.error().code();
            }
        });
        worker.join();
        QVERIFY(crossThreadRejected);
        QCOMPARE(crossThreadCode, ZzCore::ZzErrorCode::InvalidState);
    }

    void createsWorkspaceWithoutReplacingTheHostCentralWidget()
    {
        QMainWindow host;
        QWidget existing;
        host.setCentralWidget(&existing);
        auto result = ZzPureTools::ZzWorkspaceShell::create(&host);
        QVERIFY(result);
        auto shell = std::move(result).value();

        QCOMPARE(host.centralWidget(), &existing);
        QVERIFY(shell->workspaceWidget() != nullptr);
        QCOMPARE(shell->workspaceWidget()->parentWidget(), &host);
        QVERIFY(shell->tabWidget() != nullptr);
        QVERIFY(shell->commandPalette() != nullptr);
        QVERIFY(shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left) != nullptr);
        QVERIFY(shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right) != nullptr);

        host.takeCentralWidget();
    }

    void createsSplitWorkspaceAndBottomPane()
    {
        ZzShellFixture fixture;
        auto *const splitWorkspace = fixture.shell->splitWorkspace();
        auto *const bottomPane = fixture.shell->bottomPane();

        QVERIFY(splitWorkspace != nullptr);
        QVERIFY(bottomPane != nullptr);
        QCOMPARE(
            fixture.shell->tabWidget(),
            splitWorkspace->tabWidget(splitWorkspace->activeGroupId()));
        QCOMPARE(splitWorkspace->parentWidget(), bottomPane->parentWidget());
        QWidget *const centerHost = splitWorkspace->parentWidget();
        QVERIFY(centerHost != nullptr);
        QCOMPARE(centerHost->parentWidget(), fixture.shell->workspaceWidget());
        auto *const centerLayout = qobject_cast<QVBoxLayout *>(
            centerHost->layout());
        QVERIFY(centerLayout != nullptr);
        QCOMPARE(centerLayout->count(), 2);
        QCOMPARE(centerLayout->itemAt(0)->widget(), splitWorkspace);
        QCOMPARE(centerLayout->itemAt(1)->widget(), bottomPane);
        QCOMPARE(centerLayout->stretch(0), 1);
        QCOMPARE(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)->mode(),
            ZzFluentUI::ZzSidePaneMode::Stacked);
        QCOMPARE(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Right)->mode(),
            ZzFluentUI::ZzSidePaneMode::Stacked);
    }

    void hidesEmptySideEdgesAndRestoresOnlyTheOccupiedEdge()
    {
        ZzShellFixture fixture;
        auto *const leftPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightPane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right);
        auto *const leftBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left);
        auto *const rightBar = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Right);

        QVERIFY(leftPane->isCollapsed());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(leftBar->isHidden());
        QVERIFY(rightBar->isHidden());

        auto content = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left"), QStringLiteral("Left"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(!leftPane->isCollapsed());
        QVERIFY(!leftBar->isHidden());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightBar->isHidden());

        auto taken = fixture.shell->takePanel(zzPanelId("left"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QVERIFY(leftPane->isCollapsed());
        QVERIFY(leftBar->isHidden());
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightBar->isHidden());
    }

    void rejectsRegistrationErrorsWithoutTakingContent()
    {
        ZzShellFixture fixture;
        const std::array invalidIds{
            ZzPureTools::ZzWorkspacePanelId(),
            ZzPureTools::ZzWorkspacePanelId(QStringLiteral("   "))};
        for (const auto &id : invalidIds) {
            QWidget content;
            auto result = fixture.shell->registerSidePanel(
                id, QStringLiteral("Explorer"), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, &content);
            QVERIFY(!result);
            QCOMPARE(content.parent(), nullptr);
        }

        QWidget emptyTitleContent;
        auto emptyTitle = fixture.shell->registerSidePanel(
            zzPanelId("empty-title"), QStringLiteral("  "), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, &emptyTitleContent);
        QVERIFY(!emptyTitle);
        QCOMPARE(emptyTitleContent.parent(), nullptr);

        auto nullContent = fixture.shell->registerDockPanel(
            zzPanelId("null"), QStringLiteral("Null"), zzIcon(),
            Qt::BottomDockWidgetArea, nullptr);
        QVERIFY(!nullContent);

        QWidget invalidAreaContent;
        auto invalidArea = fixture.shell->registerDockPanel(
            zzPanelId("invalid-area"), QStringLiteral("Invalid"), zzIcon(),
            Qt::NoDockWidgetArea, &invalidAreaContent);
        QVERIFY(!invalidArea);
        QCOMPARE(invalidAreaContent.parent(), nullptr);

        QWidget parent;
        QWidget parentedContent(&parent);
        auto parented = fixture.shell->registerDockPanel(
            zzPanelId("parented"), QStringLiteral("Parented"), zzIcon(),
            Qt::BottomDockWidgetArea, &parentedContent);
        QVERIFY(!parented);
        QCOMPARE(parentedContent.parentWidget(), &parent);
    }

    void enforcesGlobalIdsAndReturnsSideAndDockOwnership()
    {
        ZzShellFixture fixture;
        auto sideContent = std::make_unique<QWidget>();
        QWidget *const sideRaw = sideContent.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("explorer"), QStringLiteral("Explorer"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            sideContent.get()));
        zzReleaseAfterAdoption(sideContent);
        QVERIFY(sideRaw->parentWidget() != nullptr);
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->pageCount(),
            1);

        QWidget duplicateContent;
        auto duplicate = fixture.shell->registerDockPanel(
            zzPanelId("explorer"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, &duplicateContent);
        QVERIFY(!duplicate);
        QCOMPARE(duplicateContent.parent(), nullptr);

        auto dockContent = std::make_unique<QWidget>();
        QWidget *const dockRaw = dockContent.get();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("terminal"), QStringLiteral("Terminal"), zzIcon(),
            Qt::BottomDockWidgetArea, dockContent.get()));
        zzReleaseAfterAdoption(dockContent);
        auto *dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:terminal"));
        QVERIFY(dock != nullptr);
        QCOMPARE(dock->widget(), dockRaw);
        QVERIFY(dockRaw->parentWidget() != nullptr);

        auto takenSide = fixture.shell->takePanel(zzPanelId("explorer"));
        QVERIFY(takenSide);
        std::unique_ptr<QWidget> returnedSide(takenSide.value());
        QCOMPARE(returnedSide.get(), sideRaw);
        QCOMPARE(returnedSide->parent(), nullptr);

        auto takenDock = fixture.shell->takePanel(zzPanelId("terminal"));
        QVERIFY(takenDock);
        std::unique_ptr<QWidget> returnedDock(takenDock.value());
        QCOMPARE(returnedDock.get(), dockRaw);
        QCOMPARE(returnedDock->parent(), nullptr);
        QCOMPARE(
            fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:terminal")),
            nullptr);

        auto missing = fixture.shell->takePanel(zzPanelId("terminal"));
        QVERIFY(!missing);
        QCOMPARE(missing.error().code(), ZzCore::ZzErrorCode::NotFound);
    }

    void registersShowsAndReturnsBottomOwnership()
    {
        ZzShellFixture fixture;
        auto output = std::make_unique<QWidget>();
        QWidget *const outputRaw = output.get();

        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("output"), QStringLiteral("Output"), zzIcon(),
            output.get()));
        zzReleaseAfterAdoption(output);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 1);
        QVERIFY(fixture.shell->showPanel(zzPanelId("output"), true));
        QCOMPARE(fixture.shell->bottomPane()->currentWidget(), outputRaw);
        QVERIFY(!fixture.shell->bottomPane()->isCollapsed());
        QVERIFY(fixture.shell->showPanel(zzPanelId("output"), false));
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());

        auto taken = fixture.shell->takePanel(zzPanelId("output"));
        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QCOMPARE(returned.get(), outputRaw);
        QCOMPARE(returned->parent(), nullptr);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 0);
    }

    void rejectsDuplicateIdsAcrossSideBottomAndDockWithoutTakingContent()
    {
        ZzShellFixture fixture;
        auto side = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QWidget sideToBottom;
        QWidget sideToDock;
        QVERIFY(!fixture.shell->registerBottomPanel(
            zzPanelId("side"), QStringLiteral("Duplicate"), zzIcon(),
            &sideToBottom));
        QVERIFY(!fixture.shell->registerDockPanel(
            zzPanelId("side"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, &sideToDock));
        QCOMPARE(sideToBottom.parent(), nullptr);
        QCOMPARE(sideToDock.parent(), nullptr);

        auto bottom = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            bottom.get()));
        zzReleaseAfterAdoption(bottom);
        QWidget bottomToSide;
        QWidget bottomToDock;
        QVERIFY(!fixture.shell->registerSidePanel(
            zzPanelId("bottom"), QStringLiteral("Duplicate"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, &bottomToSide));
        QVERIFY(!fixture.shell->registerDockPanel(
            zzPanelId("bottom"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, &bottomToDock));
        QCOMPARE(bottomToSide.parent(), nullptr);
        QCOMPARE(bottomToDock.parent(), nullptr);

        auto dock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);
        QWidget dockToSide;
        QWidget dockToBottom;
        QVERIFY(!fixture.shell->registerSidePanel(
            zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, &dockToSide));
        QVERIFY(!fixture.shell->registerBottomPanel(
            zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
            &dockToBottom));
        QCOMPARE(dockToSide.parent(), nullptr);
        QCOMPARE(dockToBottom.parent(), nullptr);
    }

    void reservesBottomIdDuringSynchronousRegistrationSignals()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentChangeWidget>();
        QWidget *const contentRaw = content.get();
        QWidget duplicateContent;
        bool callbackEntered = false;
        bool duplicateRegistrationSucceeded = false;
        bool reentrantTakeSucceeded = false;
        content->parentChanged = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            duplicateRegistrationSucceeded = static_cast<bool>(
                fixture.shell->registerBottomPanel(
                    zzPanelId("bottom"), QStringLiteral("Duplicate"),
                    zzIcon(), &duplicateContent));
            reentrantTakeSucceeded = static_cast<bool>(
                fixture.shell->takePanel(zzPanelId("bottom")));
        };

        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!duplicateRegistrationSucceeded);
        QVERIFY(!reentrantTakeSucceeded);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 1);
        QCOMPARE(fixture.shell->bottomPane()->currentWidget(), contentRaw);
    }

    void externalBottomContentDestructionCleansStateAndAllowsIdReuse()
    {
        ZzShellFixture fixture;
        auto *bottom = new QWidget;
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(), bottom));
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 1);

        delete bottom;

        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Replacement"), zzIcon(),
            replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void cleansBottomIdWhenContentIsDestroyedDuringTakeSignals()
    {
        ZzShellFixture fixture;
        auto *bottom = new QWidget;
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(), bottom));
        bool callbackEntered = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(),
            [&](QWidget *) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete bottom;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("bottom"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QCOMPARE(fixture.shell->bottomPane()->widgetCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Replacement"), zzIcon(),
            replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void cleansSideIdWhenContentIsDestroyedDuringTakeSignals()
    {
        ZzShellFixture fixture;
        auto *side = new QWidget;
        QPointer<QWidget> sideGuard(side);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side));
        auto *const pane = fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left);
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        bool callbackEntered = false;
        QObject::connect(
            pane, &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete side;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("side"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(sideGuard.isNull());
        QCOMPARE(model->rowCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void cleansDockIdWhenContentIsDestroyedDuringTakeSignals()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QPointer<QWidget> contentGuard(contentRaw);
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        bool callbackEntered = false;
        QObject::connect(
            dock, &QObject::destroyed,
            fixture.shell.get(), [&](QObject *) {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete contentRaw;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(contentGuard.isNull());
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement"), zzIcon(),
            Qt::BottomDockWidgetArea, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void dockTakePreservesContentInjectedDuringParentChange()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto injected = std::make_unique<QWidget>();
        QPointer<QWidget> injectedGuard(injected.get());
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        bool callbackEntered = false;
        contentRaw->parentRemoved = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            dock->setWidget(injected.get());
            injected.release();
        };

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(injectedGuard != nullptr);
        QCOMPARE(injectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedInjected(injectedGuard.data());
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement"), zzIcon(),
            Qt::BottomDockWidgetArea, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void dockCleanupRetainsIdUntilRepeatedInjectionIsDetached()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto firstInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const firstInjectedRaw = firstInjected.get();
        QPointer<QWidget> firstInjectedGuard(firstInjectedRaw);
        auto secondInjected = std::make_unique<QWidget>();
        QPointer<QWidget> secondInjectedGuard(secondInjected.get());
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(firstInjected.get());
            firstInjected.release();
        };
        firstInjectedRaw->parentRemoved = [&] {
            dock->setWidget(secondInjected.get());
            secondInjected.release();
        };

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));

        QVERIFY(!interruptedTake);
        QVERIFY(firstInjectedGuard != nullptr);
        QCOMPARE(firstInjectedGuard->parent(), nullptr);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dock);
        auto duplicate = std::make_unique<QWidget>();
        auto duplicateRegistration = fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
            Qt::BottomDockWidgetArea, duplicate.get());
        if (duplicateRegistration) {
            duplicate.release();
        }
        QVERIFY(!duplicateRegistration);

        QCoreApplication::processEvents();

        QVERIFY(dockGuard.isNull());
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::unique_ptr<QWidget> preservedFirst(firstInjectedGuard.data());
        std::unique_ptr<QWidget> preservedSecond(secondInjectedGuard.data());
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement"), zzIcon(),
            Qt::BottomDockWidgetArea, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void shellDestructionPreservesContentWhileDockCleanupIsPending()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto firstInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const firstInjectedRaw = firstInjected.get();
        QPointer<QWidget> firstInjectedGuard(firstInjectedRaw);
        auto secondInjected = std::make_unique<QWidget>();
        QPointer<QWidget> secondInjectedGuard(secondInjected.get());
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(firstInjected.get());
            firstInjected.release();
        };
        firstInjectedRaw->parentRemoved = [&] {
            dock->setWidget(secondInjected.get());
            secondInjected.release();
        };
        auto interruptedTake = fixture.shell->takePanel(zzPanelId("dock"));
        QVERIFY(!interruptedTake);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dock);

        fixture.shell.reset();

        QVERIFY(dockGuard.isNull());
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::unique_ptr<QWidget> preservedFirst(firstInjectedGuard.data());
        std::unique_ptr<QWidget> preservedSecond(secondInjectedGuard.data());
    }

    void hostDestructionPreservesContentWhileDockCleanupIsPending()
    {
        auto host = std::make_unique<QMainWindow>();
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(host.get());
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        auto content = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const contentRaw = content.get();
        auto firstInjected = std::make_unique<ZzParentRemovedWidget>();
        ZzParentRemovedWidget *const firstInjectedRaw = firstInjected.get();
        QPointer<QWidget> firstInjectedGuard(firstInjectedRaw);
        auto secondInjected = std::make_unique<QWidget>();
        QPointer<QWidget> secondInjectedGuard(secondInjected.get());
        QVERIFY(shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto *const dock = host->findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        QPointer<ZzFluentUI::ZzDockPanel> dockGuard(dock);
        contentRaw->parentRemoved = [&] {
            dock->setWidget(firstInjected.get());
            firstInjected.release();
        };
        firstInjectedRaw->parentRemoved = [&] {
            dock->setWidget(secondInjected.get());
            secondInjected.release();
        };
        auto interruptedTake = shell->takePanel(zzPanelId("dock"));
        QVERIFY(!interruptedTake);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dock);

        host.reset();

        QVERIFY(dockGuard != nullptr);
        QCOMPARE(dockGuard->parent(), nullptr);
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), dockGuard.data());
        shell.reset();
        QVERIFY(dockGuard.isNull());
        QVERIFY(secondInjectedGuard != nullptr);
        QCOMPARE(secondInjectedGuard->parent(), nullptr);
        std::unique_ptr<QWidget> preservedContent(contentRaw);
        std::unique_ptr<QWidget> preservedFirst(firstInjectedGuard.data());
        std::unique_ptr<QWidget> preservedSecond(secondInjectedGuard.data());
    }

    void sideTakeRejectsDestroyedContentAfterActivityRemovalSignals()
    {
        ZzShellFixture fixture;
        auto *side = new QWidget;
        QPointer<QWidget> sideGuard(side);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        bool callbackEntered = false;
        QObject::connect(
            model, &QAbstractItemModel::rowsAboutToBeRemoved,
            fixture.shell.get(), [&] {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                delete side;
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("side"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QVERIFY(sideGuard.isNull());
        QCOMPARE(model->rowCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void sideTakeLeavesThirdPartyOwnerAfterActivityRemovalSignals()
    {
        ZzShellFixture fixture;
        QWidget thirdPartyOwner;
        auto side = std::make_unique<QWidget>();
        QWidget *const sideRaw = side.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        bool callbackEntered = false;
        QObject::connect(
            model, &QAbstractItemModel::rowsAboutToBeRemoved,
            fixture.shell.get(), [&] {
                if (callbackEntered) {
                    return;
                }
                callbackEntered = true;
                sideRaw->setParent(&thirdPartyOwner);
            });

        auto interruptedTake = fixture.shell->takePanel(zzPanelId("side"));

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedTake);
        QCOMPARE(sideRaw->parentWidget(), &thirdPartyOwner);
        QCOMPARE(model->rowCount(), 0);
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void rejectsBottomShowDuringRegistrationTransaction()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        bool callbackEntered = false;
        bool reentrantShowSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current != contentRaw) {
                    return;
                }
                callbackEntered = true;
                reentrantShowSucceeded = static_cast<bool>(
                    fixture.shell->showPanel(zzPanelId("bottom"), true));
            });

        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!reentrantShowSucceeded);
    }

    void rejectsBottomShowDuringRemovalTransaction()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);
        bool callbackEntered = false;
        bool reentrantShowSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current == contentRaw) {
                    return;
                }
                callbackEntered = true;
                reentrantShowSucceeded = static_cast<bool>(
                    fixture.shell->showPanel(zzPanelId("bottom"), true));
            });

        auto taken = fixture.shell->takePanel(zzPanelId("bottom"));

        QVERIFY(taken);
        std::unique_ptr<QWidget> returned(taken.value());
        QCOMPARE(returned.get(), contentRaw);
        QVERIFY(callbackEntered);
        QVERIFY(!reentrantShowSucceeded);
    }

    void bottomShowDetectsSynchronousInvalidation_data()
    {
        QTest::addColumn<bool>("takeDuringSignal");
        QTest::newRow("take") << true;
        QTest::newRow("destroy") << false;
    }

    void bottomShowDetectsSynchronousInvalidation()
    {
        QFETCH(bool, takeDuringSignal);
        ZzShellFixture fixture;
        auto *content = new QWidget;
        QPointer<QWidget> contentGuard(content);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(), content));
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        bool callbackEntered = false;
        bool nestedTakeSucceeded = false;
        std::unique_ptr<QWidget> returned;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::collapsedChanged,
            fixture.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                if (takeDuringSignal) {
                    auto nestedTake = fixture.shell->takePanel(
                        zzPanelId("bottom"));
                    nestedTakeSucceeded = static_cast<bool>(nestedTake);
                    if (nestedTake) {
                        returned.reset(nestedTake.value());
                    }
                } else {
                    delete content;
                }
            });

        const auto interruptedShow = fixture.shell->showPanel(
            zzPanelId("bottom"), true);

        QVERIFY(callbackEntered);
        QVERIFY(!interruptedShow);
        QCOMPARE(nestedTakeSucceeded, takeDuringSignal);
        if (takeDuringSignal) {
            QCOMPARE(returned.get(), content);
            QCOMPARE(returned->parent(), nullptr);
        } else {
            QVERIFY(contentGuard.isNull());
        }
        auto replacement = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Replacement"), zzIcon(),
            replacement.get()));
        zzReleaseAfterAdoption(replacement);
    }

    void bottomShowDetectsSameWidgetReregistrationDuringSignal()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("bottom"), QStringLiteral("Bottom"), zzIcon(),
            content.get()));
        zzReleaseAfterAdoption(content);
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        bool callbackEntered = false;
        bool nestedTakeSucceeded = false;
        bool nestedRegistrationSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::collapsedChanged,
            fixture.shell.get(), [&](bool collapsed) {
                if (callbackEntered || collapsed) {
                    return;
                }
                callbackEntered = true;
                auto nestedTake = fixture.shell->takePanel(
                    zzPanelId("bottom"));
                nestedTakeSucceeded = static_cast<bool>(nestedTake);
                if (!nestedTake) {
                    return;
                }
                std::unique_ptr<QWidget> returned(nestedTake.value());
                auto nestedRegistration = fixture.shell->registerBottomPanel(
                    zzPanelId("bottom"), QStringLiteral("Replacement"),
                    zzIcon(), returned.get());
                nestedRegistrationSucceeded =
                    static_cast<bool>(nestedRegistration);
                if (nestedRegistration) {
                    returned.release();
                }
            });

        const auto interruptedShow = fixture.shell->showPanel(
            zzPanelId("bottom"), true);

        QVERIFY(callbackEntered);
        QVERIFY(nestedTakeSucceeded);
        QVERIFY(nestedRegistrationSucceeded);
        QVERIFY(!interruptedShow);
        auto replacementTake = fixture.shell->takePanel(zzPanelId("bottom"));
        QVERIFY(replacementTake);
        std::unique_ptr<QWidget> returned(replacementTake.value());
        QCOMPARE(returned.get(), contentRaw);
        QCOMPARE(returned->parent(), nullptr);
    }

    void bottomShowStopsBeforeCollapseAfterCurrentWidgetReregistration()
    {
        ZzShellFixture fixture;
        auto target = std::make_unique<QWidget>();
        QWidget *const targetRaw = target.get();
        auto other = std::make_unique<QWidget>();
        QWidget *const otherRaw = other.get();
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("target"), QStringLiteral("Target"), zzIcon(),
            target.get()));
        zzReleaseAfterAdoption(target);
        QVERIFY(fixture.shell->registerBottomPanel(
            zzPanelId("other"), QStringLiteral("Other"), zzIcon(),
            other.get()));
        zzReleaseAfterAdoption(other);
        QCOMPARE(fixture.shell->bottomPane()->currentWidget(), otherRaw);
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        bool callbackEntered = false;
        bool nestedTakeSucceeded = false;
        bool nestedRegistrationSucceeded = false;
        QObject::connect(
            fixture.shell->bottomPane(),
            &ZzFluentUI::ZzBottomPane::currentWidgetChanged,
            fixture.shell.get(), [&](QWidget *current) {
                if (callbackEntered || current != targetRaw) {
                    return;
                }
                callbackEntered = true;
                auto nestedTake = fixture.shell->takePanel(
                    zzPanelId("target"));
                nestedTakeSucceeded = static_cast<bool>(nestedTake);
                if (!nestedTake) {
                    return;
                }
                std::unique_ptr<QWidget> returned(nestedTake.value());
                auto nestedRegistration = fixture.shell->registerBottomPanel(
                    zzPanelId("target"), QStringLiteral("Replacement"),
                    zzIcon(), returned.get());
                nestedRegistrationSucceeded =
                    static_cast<bool>(nestedRegistration);
                if (nestedRegistration) {
                    returned.release();
                }
            });

        const auto interruptedShow = fixture.shell->showPanel(
            zzPanelId("target"), true);

        QVERIFY(callbackEntered);
        QVERIFY(nestedTakeSucceeded);
        QVERIFY(nestedRegistrationSucceeded);
        QVERIFY(!interruptedShow);
        QVERIFY(fixture.shell->bottomPane()->isCollapsed());
        auto replacementTake = fixture.shell->takePanel(zzPanelId("target"));
        QVERIFY(replacementTake);
        std::unique_ptr<QWidget> returned(replacementTake.value());
        QCOMPARE(returned.get(), targetRaw);
    }

    void failedTakePreservesRegisteredSideState()
    {
        ZzShellFixture fixture;
        auto side = std::make_unique<QWidget>();
        QWidget *const sideRaw = side.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);

        std::unique_ptr<QWidget> externallyTakenSide(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->takeWidget(sideRaw));
        QCOMPARE(externallyTakenSide.get(), sideRaw);
        auto failedSideTake = fixture.shell->takePanel(zzPanelId("side"));
        QVERIFY(!failedSideTake);
        QCOMPARE(failedSideTake.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QCOMPARE(model->rowCount(), 1);
        QWidget duplicateSide;
        QVERIFY(!fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Duplicate side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, &duplicateSide));
    }

    void failedTakePreservesRegisteredDockState()
    {
        ZzShellFixture fixture;
        auto dockContent = std::make_unique<QWidget>();
        QWidget *const dockRaw = dockContent.get();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, dockContent.get()));
        zzReleaseAfterAdoption(dockContent);
        QPointer<ZzFluentUI::ZzDockPanel> dock =
            fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);
        std::unique_ptr<QWidget> externallyTakenDock(
            dock->takeContentWidget());
        QCOMPARE(externallyTakenDock.get(), dockRaw);

        auto failedDockTake = fixture.shell->takePanel(zzPanelId("dock"));
        QVERIFY(!failedDockTake);
        QCOMPARE(failedDockTake.error().code(), ZzCore::ZzErrorCode::InvalidState);
        QVERIFY(dock != nullptr);
        QCOMPARE(dock->widget(), nullptr);
        QWidget duplicateDock;
        QVERIFY(!fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Duplicate dock"), zzIcon(),
            Qt::BottomDockWidgetArea, &duplicateDock));
    }

    void externalSideContentDestructionCleansStateAndAllowsIdReuse()
    {
        ZzShellFixture fixture;
        auto *side = new QWidget;
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side));
        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->rowCount(), 1);

        delete side;

        QCOMPARE(model->rowCount(), 0);
        auto replacementSide = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Replacement side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            replacementSide.get()));
        zzReleaseAfterAdoption(replacementSide);
    }

    void externalDockContentDestructionCleansStateAndAllowsIdReuse()
    {
        ZzShellFixture fixture;
        auto *dockContent = new QWidget;
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, dockContent));
        QPointer<ZzFluentUI::ZzDockPanel> dock =
            fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dock != nullptr);

        delete dockContent;
        auto replacementDock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Replacement dock"), zzIcon(),
            Qt::RightDockWidgetArea, replacementDock.get()));
        zzReleaseAfterAdoption(replacementDock);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        QVERIFY(dock.isNull());
        QVERIFY(fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock")) != nullptr);
    }

    void reservesSideIdDuringSynchronousRegistrationSignals()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<QWidget>();
        QWidget *const contentRaw = content.get();
        QWidget duplicateContent;
        bool callbackEntered = false;
        bool duplicateRegistrationSucceeded = false;
        bool reentrantTakeSucceeded = false;
        QObject::connect(
            fixture.shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left),
            &ZzFluentUI::ZzSidePane::currentWidgetChanged,
            fixture.shell.get(),
            [&](QWidget *current) {
                if (callbackEntered || current != contentRaw) {
                    return;
                }
                callbackEntered = true;
                duplicateRegistrationSucceeded = static_cast<bool>(
                    fixture.shell->registerSidePanel(
                        zzPanelId("side"), QStringLiteral("Duplicate"),
                        zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                        &duplicateContent));
                reentrantTakeSucceeded = static_cast<bool>(
                    fixture.shell->takePanel(zzPanelId("side")));
            });

        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!duplicateRegistrationSucceeded);
        QVERIFY(!reentrantTakeSucceeded);
        QCOMPARE(fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
        QCOMPARE(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->pageCount(), 1);
    }

    void reservesDockIdDuringSynchronousParentChange()
    {
        ZzShellFixture fixture;
        auto content = std::make_unique<ZzParentChangeWidget>();
        QWidget duplicateContent;
        bool callbackEntered = false;
        bool duplicateRegistrationSucceeded = false;
        bool reentrantTakeSucceeded = false;
        content->parentChanged = [&] {
            if (callbackEntered) {
                return;
            }
            callbackEntered = true;
            duplicateRegistrationSucceeded = static_cast<bool>(
                fixture.shell->registerDockPanel(
                    zzPanelId("dock"), QStringLiteral("Duplicate"), zzIcon(),
                    Qt::BottomDockWidgetArea, &duplicateContent));
            reentrantTakeSucceeded = static_cast<bool>(
                fixture.shell->takePanel(zzPanelId("dock")));
        };

        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);

        QVERIFY(callbackEntered);
        QVERIFY(!duplicateRegistrationSucceeded);
        QVERIFY(!reentrantTakeSucceeded);
        QCOMPARE(
            fixture.host.findChildren<ZzFluentUI::ZzDockPanel *>(
                QStringLiteral("zzWorkspaceDock:dock")).size(),
            1);
    }

    void preservesRegistrationOrderAndUpdatesBadges()
    {
        ZzShellFixture fixture;
        const std::array ids{"one", "two", "three"};
        const std::array titles{"One", "Two", "Three"};
        const std::array areas{
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            ZzFluentUI::ZzActivityArea::RightPrimary,
            ZzFluentUI::ZzActivityArea::LeftSecondary};
        for (std::size_t index = 0; index < ids.size(); ++index) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(fixture.shell->registerSidePanel(
                zzPanelId(ids.at(index)),
                QString::fromLatin1(titles.at(index)), zzIcon(),
                areas.at(index), content.get()));
            zzReleaseAfterAdoption(content);
        }

        QAbstractItemModel *const model = fixture.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QVERIFY(model != nullptr);
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("One"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("Two"));
        QCOMPARE(model->index(2, 0).data().toString(), QStringLiteral("Three"));

        QVERIFY(fixture.shell->setPanelBadge(zzPanelId("two"), 104));
        QCOMPARE(
            model->index(1, 0).data(
                static_cast<int>(ZzFluentUI::ZzActivityItemRole::Badge))
                .toInt(),
            104);
        QWidget unregistered;
        QVERIFY(!fixture.shell->setPanelBadge(zzPanelId("missing"), 1));
        QVERIFY(!fixture.shell->setPanelBadge(zzPanelId("one"), -1));
    }

    void showsSideAndDockPanelsThroughOneApi()
    {
        ZzShellFixture fixture;
        auto side = std::make_unique<QWidget>();
        auto dock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, side.get()));
        zzReleaseAfterAdoption(side);
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);

        QVERIFY(fixture.shell->showPanel(zzPanelId("side"), false));
        QVERIFY(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->isCollapsed());
        QVERIFY(fixture.shell->showPanel(zzPanelId("side"), true));
        QVERIFY(!fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->isCollapsed());

        auto *dockPanel = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        QVERIFY(fixture.shell->showPanel(zzPanelId("dock"), false));
        QVERIFY(dockPanel->isHidden());
        QVERIFY(fixture.shell->showPanel(zzPanelId("dock"), true));
        QVERIFY(!dockPanel->isHidden());
    }

    void appliesAllFourTitleModesAndTabFallback()
    {
        ZzShellFixture fixture;
        auto first = std::make_unique<QWidget>();
        first->setWindowTitle(QStringLiteral("Document"));
        fixture.shell->tabWidget()->addTab(first.release(), QStringLiteral("First"));
        auto second = std::make_unique<QWidget>();
        fixture.shell->tabWidget()->addTab(
            second.release(), QStringLiteral("Fallback"));

        fixture.shell->setApplicationTitle(QStringLiteral("Pure Tools"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Pure Tools"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Pure Tools"));

        fixture.shell->tabWidget()->setCurrentIndex(0);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Document"));

        fixture.shell->tabWidget()->setCurrentIndex(1);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Fallback"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        QCOMPARE(
            fixture.host.windowTitle(),
            QStringLiteral("Fallback - Pure Tools"));

        fixture.shell->setCustomTitle(QStringLiteral("Workspace A"));
        fixture.shell->setTitleMode(ZzPureTools::ZzWorkspaceTitleMode::Custom);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Workspace A"));
        fixture.shell->setCustomTitle({});
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Pure Tools"));
    }

    void refreshesCurrentTabTitlesFromPagePresentationChanges()
    {
        ZzShellFixture fixture;
        auto page = std::make_unique<QWidget>();
        QWidget *const pageRaw = page.get();
        fixture.shell->tabWidget()->addTab(
            page.release(), QStringLiteral("Initial"));
        fixture.shell->setApplicationTitle(QStringLiteral("Pure Tools"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Initial"));
        QSignalSpy presentationSpy(
            fixture.shell->tabWidget(),
            &ZzFluentUI::ZzTabWidget::pagePresentationChanged);

        fixture.shell->tabWidget()->setPageTitle(
            pageRaw, QStringLiteral("Renamed"));

        QCOMPARE(presentationSpy.count(), 1);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Renamed"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        fixture.shell->tabWidget()->setPageTitle(
            pageRaw, QStringLiteral("Final"));
        QCOMPARE(presentationSpy.count(), 2);
        QCOMPARE(
            fixture.host.windowTitle(),
            QStringLiteral("Final - Pure Tools"));
    }

    void followsTheActiveGroupCurrentPageTitle()
    {
        ZzShellFixture fixture;
        auto *const splitWorkspace = fixture.shell->splitWorkspace();
        const ZzFluentUI::ZzTabGroupId firstGroup =
            splitWorkspace->activeGroupId();
        auto *const firstTabs = splitWorkspace->tabWidget(firstGroup);
        auto firstPage = std::make_unique<QWidget>();
        firstPage->setWindowTitle(QStringLiteral("First Window"));
        firstTabs->addTab(firstPage.release(), QStringLiteral("First Tab"));

        const auto secondGroup = splitWorkspace->splitGroup(
            firstGroup, Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroup.has_value());
        auto *const secondTabs = splitWorkspace->tabWidget(*secondGroup);
        auto secondWindowPage = std::make_unique<QWidget>();
        QWidget *const secondWindowRaw = secondWindowPage.get();
        secondWindowPage->setWindowTitle(QStringLiteral("Second Window"));
        secondTabs->addTab(
            secondWindowPage.release(), QStringLiteral("Second Tab"));
        auto secondFallbackPage = std::make_unique<QWidget>();
        QWidget *const secondFallbackRaw = secondFallbackPage.get();
        secondTabs->addTab(
            secondFallbackPage.release(), QStringLiteral("Second Fallback"));

        fixture.shell->setApplicationTitle(QStringLiteral("Pure Tools"));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);
        QVERIFY(splitWorkspace->setActiveGroup(*secondGroup));
        QCOMPARE(fixture.shell->tabWidget(), secondTabs);
        secondTabs->setCurrentWidget(secondWindowRaw);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Second Window"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Second Window"));

        secondWindowRaw->setWindowTitle(QStringLiteral("Changed Window"));
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Changed Window"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Changed Window"));
        secondTabs->setCurrentWidget(secondFallbackRaw);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Second Fallback"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Second Fallback"));
        secondTabs->setPageTitle(
            secondFallbackRaw, QStringLiteral("Renamed Fallback"));
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("Renamed Fallback"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("Renamed Fallback"));

        QVERIFY(splitWorkspace->setActiveGroup(firstGroup));
        QCOMPARE(fixture.shell->tabWidget(), firstTabs);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("First Window"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("First Window"));
    }

    void keepsBothTitleSinksAlignedDuringActiveGroupReentry()
    {
        ZzShellFixture fixture;
        auto *const splitWorkspace = fixture.shell->splitWorkspace();
        const ZzFluentUI::ZzTabGroupId firstGroup =
            splitWorkspace->activeGroupId();
        auto firstPage = std::make_unique<QWidget>();
        firstPage->setWindowTitle(QStringLiteral("First"));
        splitWorkspace->tabWidget(firstGroup)->addTab(
            firstPage.release(), QStringLiteral("First"));
        const auto secondGroup = splitWorkspace->splitGroup(
            firstGroup, Qt::Horizontal,
            ZzFluentUI::ZzSplitPlacement::After);
        QVERIFY(secondGroup.has_value());
        auto secondPage = std::make_unique<QWidget>();
        secondPage->setWindowTitle(QStringLiteral("Second"));
        splitWorkspace->tabWidget(*secondGroup)->addTab(
            secondPage.release(), QStringLiteral("Second"));
        QVERIFY(splitWorkspace->setActiveGroup(firstGroup));
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTab);

        bool callbackEntered = false;
        bool reactivatedFirstGroup = false;
        QObject::connect(
            &fixture.host, &QWidget::windowTitleChanged,
            fixture.shell.get(),
            [&](const QString &title) {
                if (callbackEntered || title != QStringLiteral("Second")) {
                    return;
                }
                callbackEntered = true;
                reactivatedFirstGroup =
                    splitWorkspace->setActiveGroup(firstGroup);
            });

        QVERIFY(splitWorkspace->setActiveGroup(*secondGroup));

        QVERIFY(callbackEntered);
        QVERIFY(reactivatedFirstGroup);
        QCOMPARE(splitWorkspace->activeGroupId(), firstGroup);
        QCOMPARE(fixture.host.windowTitle(), QStringLiteral("First"));
        QCOMPARE(fixture.titleBar.title(), QStringLiteral("First"));
    }

    void appliesAlwaysOnTopRequestsWithoutHidingOrLosingWindowState()
    {
        ZzShellFixture fixture;
        fixture.host.setWindowState(Qt::WindowMaximized);
        fixture.host.show();
        QCoreApplication::processEvents();
        const Qt::WindowStates originalState = fixture.host.windowState();
        QVERIFY(fixture.host.isVisible());

        QVERIFY(fixture.shell->setAlwaysOnTop(true));
        QVERIFY(fixture.host.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(fixture.host.isVisible());
        QCOMPARE(fixture.host.windowState(), originalState);
        QVERIFY(fixture.titleBar.isAlwaysOnTop());

        QVERIFY(QMetaObject::invokeMethod(
            &fixture.titleBar, "alwaysOnTopRequested",
            Qt::DirectConnection, Q_ARG(bool, false)));
        QVERIFY(!fixture.host.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(fixture.host.isVisible());
        QCOMPARE(fixture.host.windowState(), originalState);
        QVERIFY(!fixture.titleBar.isAlwaysOnTop());
    }

    void savesVersionedBoundedLayoutAndRestoresAllShellState()
    {
        ZzShellFixture fixture;
        auto leftOne = std::make_unique<QWidget>();
        auto leftTwo = std::make_unique<QWidget>();
        auto right = std::make_unique<QWidget>();
        auto dock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-one"), QStringLiteral("Left one"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftOne.get()));
        zzReleaseAfterAdoption(leftOne);
        QWidget *const leftTwoRaw = leftTwo.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-two"), QStringLiteral("Left two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, leftTwo.get()));
        zzReleaseAfterAdoption(leftTwo);
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right"), QStringLiteral("Right"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, right.get()));
        zzReleaseAfterAdoption(right);
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(333);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setCurrentWidget(leftTwoRaw);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->setPaneWidth(444);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->setCollapsed(true);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        fixture.shell->tabWidget()->addTab(
            new QWidget, QStringLiteral("First tab"));
        fixture.shell->tabWidget()->addTab(
            new QWidget, QStringLiteral("Second tab"));
        fixture.shell->tabWidget()->setCurrentIndex(1);

        auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        const QByteArray &state = saved.value();
        QVERIFY(state.size() <= zzWorkspaceMaximumLayoutSize);
        QCOMPARE(state.first(4), QByteArrayLiteral("ZZWS"));
        QDataStream envelope(state);
        envelope.setVersion(QDataStream::Qt_6_8);
        char magic[4]{};
        quint16 schemaVersion = 0;
        quint16 streamVersion = 0;
        quint32 payloadLength = 0;
        QVERIFY(envelope.readRawData(magic, 4) == 4);
        envelope >> schemaVersion >> streamVersion >> payloadLength;
        QCOMPARE(schemaVersion, quint16(1));
        QCOMPARE(
            streamVersion,
            static_cast<quint16>(QDataStream::Qt_6_8));
        QCOMPARE(
            state.size(),
            qsizetype(4 + 2 + 2 + 4 + payloadLength + 32));
        const QByteArray payload = state.sliced(12, payloadLength);
        QCOMPARE(
            state.last(32),
            QCryptographicHash::hash(payload, QCryptographicHash::Sha256));

        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(180);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->setCollapsed(false);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        fixture.shell->tabWidget()->setCurrentIndex(0);
        QVERIFY(fixture.shell->restoreLayout(state));
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->paneWidth(),
            333);
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->currentWidget(),
            leftTwoRaw);
        QCOMPARE(
            fixture.shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)
                ->currentSourceIndex().data().toString(),
            QStringLiteral("Left two"));
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Right)->paneWidth(),
            444);
        QVERIFY(fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Right)->isCollapsed());
        QCOMPARE(
            fixture.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);
        QCOMPARE(fixture.shell->tabWidget()->currentIndex(), 1);
    }

    void rejectsMagicVersionLengthDigestAndOversizeCorruption()
    {
        ZzShellFixture fixture;
        auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        const QByteArray &valid = saved.value();

        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, 0, 'X')));
        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, 5, '\x02')));
        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, 11, '\x7f')));
        QVERIFY(!fixture.shell->restoreLayout(
            zzMutatedByte(valid, valid.size() - 1,
                static_cast<char>(valid.back() ^ 0x5a))));
        QVERIFY(!fixture.shell->restoreLayout(
            QByteArray(zzWorkspaceMaximumLayoutSize + 1, 'x')));
    }

    void boundsAndDeduplicatesNearLimitSideLayoutEntries()
    {
        ZzShellFixture fixture;
        const auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);

        QElapsedTimer timer;
        timer.start();
        const auto maximumUnique = zzLayoutWithSideEntries(
            saved.value(), 4096);
        QVERIFY(!maximumUnique.isEmpty());
        QVERIFY(fixture.shell->restoreLayout(maximumUnique));
        QVERIFY2(timer.elapsed() < 1000,
            "The maximum valid side layout must be decoded on the GUI thread promptly");

        const auto excessive = zzLayoutWithSideEntries(saved.value(), 4097);
        QVERIFY(!excessive.isEmpty());
        QVERIFY(!fixture.shell->restoreLayout(excessive));

        const auto duplicateTail = zzLayoutWithSideEntries(
            saved.value(), 4096, QStringLiteral("side-0000"));
        QVERIFY(!duplicateTail.isEmpty());
        QVERIFY(!fixture.shell->restoreLayout(duplicateTail));
    }

    void ignoresUnknownPanelIdsDuringRestore()
    {
        ZzShellFixture source;
        auto ghostSide = std::make_unique<QWidget>();
        auto ghostDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("ghost-side"), QStringLiteral("Ghost side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, ghostSide.get()));
        zzReleaseAfterAdoption(ghostSide);
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("ghost-dock"), QStringLiteral("Ghost dock"), zzIcon(),
            Qt::RightDockWidgetArea, ghostDock.get()));
        zzReleaseAfterAdoption(ghostDock);
        auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        auto known = std::make_unique<QWidget>();
        QWidget *const knownRaw = known.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("known"), QStringLiteral("Known"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, known.get()));
        zzReleaseAfterAdoption(known);

        QVERIFY(target.shell->restoreLayout(saved.value()));
        QCOMPARE(
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->currentWidget(),
            knownRaw);
        QCOMPARE(
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->pageCount(),
            1);
    }

    void restoresSavedSidePanelOrderAcrossRegistrationOrders()
    {
        ZzShellFixture source;
        for (const char *id : {"alpha", "beta"}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(source.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            zzReleaseAfterAdoption(content);
        }
        auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        for (const char *id : {"beta", "alpha"}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(target.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            zzReleaseAfterAdoption(content);
        }
        QVERIFY(target.shell->restoreLayout(saved.value()));

        QAbstractItemModel *const model = target.shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model();
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("alpha"));
        QCOMPARE(model->index(1, 0).data().toString(), QStringLiteral("beta"));
    }

    void rollsBackQtAndShellSnapshotsWhenQtRestoreFails()
    {
        ZzShellFixture fixture;
        auto dock = std::make_unique<QWidget>();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, dock.get()));
        zzReleaseAfterAdoption(dock);
        auto *dockPanel = fixture.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(500);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Custom);
        auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        fixture.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(321);
        fixture.shell->setTitleMode(
            ZzPureTools::ZzWorkspaceTitleMode::Application);
        const QByteArray qtBefore = fixture.host.saveState(1);
        const QByteArray validEnvelopeWithInvalidQtState = zzReplaceQtState(
            saved.value(), QByteArrayLiteral("not-a-qmainwindow-state"));
        const auto restored = fixture.shell->restoreLayout(
            validEnvelopeWithInvalidQtState);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and was rolled back"));

        QCOMPARE(fixture.host.saveState(1), qtBefore);
        QCOMPARE(fixture.host.dockWidgetArea(dockPanel), Qt::LeftDockWidgetArea);
        QCOMPARE(
            fixture.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->paneWidth(),
            321);
        QCOMPARE(
            fixture.shell->titleMode(),
            ZzPureTools::ZzWorkspaceTitleMode::Application);
    }

    void rollsBackQtSnapshotWhenShellApplyFails()
    {
        ZzShellFixture source;
        auto sourceSide = std::make_unique<QWidget>();
        auto sourceDock = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceSide.get()));
        zzReleaseAfterAdoption(sourceSide);
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, sourceDock.get()));
        zzReleaseAfterAdoption(sourceDock);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(800);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(700);
        auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetSide = std::make_unique<QWidget>();
        auto targetDock = std::make_unique<QWidget>();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetSide.get()));
        zzReleaseAfterAdoption(targetSide);
        QVERIFY(target.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, targetDock.get()));
        zzReleaseAfterAdoption(targetDock);
        target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(321);
        auto *dockPanel = target.host.findChild<ZzFluentUI::ZzDockPanel *>(
            QStringLiteral("zzWorkspaceDock:dock"));
        QVERIFY(dockPanel != nullptr);
        const QByteArray qtBefore = target.host.saveState(1);

        QVERIFY(!target.shell->restoreLayout(requested.value()));

        QCOMPARE(target.host.saveState(1), qtBefore);
        QCOMPARE(target.host.dockWidgetArea(dockPanel), Qt::LeftDockWidgetArea);
        QCOMPARE(
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->paneWidth(),
            321);
    }

    void reportsWhenLayoutRollbackFails()
    {
        ZzShellFixture source;
        auto sourceContent = std::make_unique<QWidget>();
        QVERIFY(source.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, sourceContent.get()));
        zzReleaseAfterAdoption(sourceContent);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(800);
        source.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(700);
        const auto requested = source.shell->saveLayout();
        QVERIFY(requested);

        ZzShellFixture target;
        auto targetContent = std::make_unique<ZzParentChangeWidget>();
        bool armed = false;
        bool callbackEntered = false;
        targetContent->parentChanged = [&] {
            if (!armed || callbackEntered) {
                return;
            }
            callbackEntered = true;
            target.shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->setMaximumPaneWidth(100);
        };
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, targetContent.get()));
        zzReleaseAfterAdoption(targetContent);
        target.shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->setPaneWidth(321);
        armed = true;

        const auto restored = target.shell->restoreLayout(requested.value());
        QVERIFY(callbackEntered);
        QVERIFY(!restored);
        QCOMPARE(
            restored.error().technicalMessage(),
            QStringLiteral("Workspace layout restore failed and rollback failed"));
    }

    void survivesHostDestructionBeforeShell()
    {
        auto host = std::make_unique<ZzLayoutTornDownMainWindow>();
        auto *titleBar = new ZzFluentUI::ZzFluentTitleBar(host.get());
        auto result = ZzPureTools::ZzWorkspaceShell::create(
            host.get(), titleBar);
        QVERIFY(result);
        auto shell = std::move(result).value();
        auto content = std::make_unique<QWidget>();
        QPointer<QWidget> contentGuard(content.get());
        QVERIFY(shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::BottomDockWidgetArea, content.get()));
        zzReleaseAfterAdoption(content);
        auto secondContent = std::make_unique<QWidget>();
        QVERIFY(shell->registerDockPanel(
            zzPanelId("second-dock"), QStringLiteral("Second dock"), zzIcon(),
            Qt::RightDockWidgetArea, secondContent.get()));
        zzReleaseAfterAdoption(secondContent);

        host.reset();

        QVERIFY(contentGuard.isNull());
        QCOMPARE(shell->workspaceWidget(), nullptr);
        QCOMPARE(shell->tabWidget(), nullptr);
        QCOMPARE(shell->commandPalette(), nullptr);
        QVERIFY(!shell->saveLayout());
        shell.reset();
    }

    void destroysHostOwnedShellWithMultipleDocksAfterHostLayoutIsTornDown()
    {
        auto host = std::make_unique<QMainWindow>();
        auto result = ZzPureTools::ZzWorkspaceShell::create(host.get());
        QVERIFY(result);
        auto shell = std::move(result).value();
        for (const auto &[id, area] : std::array{
                 std::pair{"first", Qt::BottomDockWidgetArea},
                 std::pair{"second", Qt::RightDockWidgetArea}}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(shell->registerDockPanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(), area,
                content.get()));
            zzReleaseAfterAdoption(content);
        }
        QPointer<ZzPureTools::ZzWorkspaceShell> shellGuard(shell.get());
        shell->setParent(host.get());
        zzReleaseAfterAdoption(shell);

        host.reset();

        QVERIFY(shellGuard.isNull());
    }
};

QTEST_MAIN(ZzWorkspaceShellTest)
#include "ZzWorkspaceShellTest.moc"
