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

#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSidePaneEdge.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>

namespace {

constexpr qsizetype zzWorkspaceMaximumLayoutSize = 1024 * 1024;

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
        content.release();

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
        sideContent.release();
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
        dockContent.release();
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

    void failedTakePreservesRegisteredSideState()
    {
        ZzShellFixture fixture;
        auto side = std::make_unique<QWidget>();
        QWidget *const sideRaw = side.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("side"), QStringLiteral("Side"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, side.get()));
        side.release();
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
        dockContent.release();
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
        replacementSide.release();
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
        replacementDock.release();
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
        content.release();

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
        content.release();

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
            content.release();
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
        side.release();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, dock.get()));
        dock.release();

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
        leftOne.release();
        QWidget *const leftTwoRaw = leftTwo.get();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("left-two"), QStringLiteral("Left two"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftSecondary, leftTwo.get()));
        leftTwo.release();
        QVERIFY(fixture.shell->registerSidePanel(
            zzPanelId("right"), QStringLiteral("Right"), zzIcon(),
            ZzFluentUI::ZzActivityArea::RightPrimary, right.get()));
        right.release();
        QVERIFY(fixture.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, dock.get()));
        dock.release();
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

        auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        const QByteArray state = saved.value();
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
    }

    void rejectsMagicVersionLengthDigestAndOversizeCorruption()
    {
        ZzShellFixture fixture;
        auto saved = fixture.shell->saveLayout();
        QVERIFY(saved);
        const QByteArray valid = saved.value();

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
        ghostSide.release();
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("ghost-dock"), QStringLiteral("Ghost dock"), zzIcon(),
            Qt::RightDockWidgetArea, ghostDock.get()));
        ghostDock.release();
        auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        auto known = std::make_unique<QWidget>();
        QWidget *const knownRaw = known.get();
        QVERIFY(target.shell->registerSidePanel(
            zzPanelId("known"), QStringLiteral("Known"), zzIcon(),
            ZzFluentUI::ZzActivityArea::LeftPrimary, known.get()));
        known.release();

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
            content.release();
        }
        auto saved = source.shell->saveLayout();
        QVERIFY(saved);

        ZzShellFixture target;
        for (const char *id : {"beta", "alpha"}) {
            auto content = std::make_unique<QWidget>();
            QVERIFY(target.shell->registerSidePanel(
                zzPanelId(id), QString::fromLatin1(id), zzIcon(),
                ZzFluentUI::ZzActivityArea::LeftPrimary, content.get()));
            content.release();
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
        dock.release();
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
        sourceSide.release();
        QVERIFY(source.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::RightDockWidgetArea, sourceDock.get()));
        sourceDock.release();
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
        targetSide.release();
        QVERIFY(target.shell->registerDockPanel(
            zzPanelId("dock"), QStringLiteral("Dock"), zzIcon(),
            Qt::LeftDockWidgetArea, targetDock.get()));
        targetDock.release();
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
        sourceContent.release();
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
        targetContent.release();
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
        content.release();
        auto secondContent = std::make_unique<QWidget>();
        QVERIFY(shell->registerDockPanel(
            zzPanelId("second-dock"), QStringLiteral("Second dock"), zzIcon(),
            Qt::RightDockWidgetArea, secondContent.get()));
        secondContent.release();

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
            content.release();
        }
        QPointer<ZzPureTools::ZzWorkspaceShell> shellGuard(shell.get());
        shell->setParent(host.get());
        shell.release();

        host.reset();

        QVERIFY(shellGuard.isNull());
    }
};

QTEST_MAIN(ZzWorkspaceShellTest)
#include "ZzWorkspaceShellTest.moc"
