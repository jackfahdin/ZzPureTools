#include <array>
#include <memory>
#include <thread>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QBuffer>
#include <QtCore/QCoreApplication>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QPointer>
#include <QtTest/QTest>
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

[[nodiscard]] QByteArray zzMutatedByte(
    QByteArray state,
    qsizetype offset,
    char value)
{
    Q_ASSERT(offset >= 0 && offset < state.size());
    state[offset] = value;
    return state;
}

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
        QVERIFY(!fixture.shell->restoreLayout(
            validEnvelopeWithInvalidQtState));

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

    void survivesHostDestructionBeforeShell()
    {
        auto host = std::make_unique<QMainWindow>();
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

        host.reset();

        QVERIFY(contentGuard.isNull());
        QCOMPARE(shell->workspaceWidget(), nullptr);
        QCOMPARE(shell->tabWidget(), nullptr);
        QCOMPARE(shell->commandPalette(), nullptr);
        QVERIFY(!shell->saveLayout());
        shell.reset();
    }
};

QTEST_MAIN(ZzWorkspaceShellTest)
#include "ZzWorkspaceShellTest.moc"
