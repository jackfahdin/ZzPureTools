#include <memory>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzCommandBar.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include "ZzExampleApplicationContext.h"
#include "ZzExampleWindowShell.h"

namespace {

[[nodiscard]] ZzPureTools::ZzPageRegistration zzHomePage()
{
    ZzPureTools::ZzPageRegistration registration;
    registration.routeId = ZzPureTools::ZzRouteId(QStringLiteral("home"));
    registration.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    registration.factory =
        [](QWidget *pageParent)
        -> ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>> {
            auto *view = new QWidget(pageParent);
            return ZzPureTools::ZzPageInstance::create(
                pageParent,
                view,
                std::make_unique<QObject>(),
                std::make_unique<QObject>());
        };
    return registration;
}

[[nodiscard]] QListView *zzPrimaryActivityView(
    ZzFluentUI::ZzActivityBar *bar)
{
    return bar->findChild<QListView *>(
        QStringLiteral("zzActivityPrimaryView"));
}

[[nodiscard]] bool zzHasRenderableActivityIcon(const QVariant &value)
{
    if (!value.canConvert<ZzFluentUI::ZzIconDescriptor>()) {
        return false;
    }
    const auto descriptor =
        value.value<ZzFluentUI::ZzIconDescriptor>();
    return descriptor.source == ZzFluentUI::ZzIconSource::FontGlyph
        ? descriptor.fontIcon != ZzFluentUI::ZzFontIcon::None
        : !descriptor.resourceId.trimmed().isEmpty();
}

[[nodiscard]] ZzPureTools::ZzWorkspacePanelId zzPanelId(const char *value)
{
    return ZzPureTools::ZzWorkspacePanelId(QString::fromLatin1(value));
}

} // namespace

class ZzExampleWorkspaceSmokeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publicWorkspaceRestoresPaneSizes()
    {
        QMainWindow host;
        host.resize(1100, 720);
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(&host);
        QVERIFY(shellResult);
        auto shell = std::move(shellResult).value();
        host.setCentralWidget(shell->workspaceWidget());

        auto *leftPanel = new QWidget;
        QVERIFY(shell->registerSidePanel(
            zzPanelId("workspace-smoke-left"), QStringLiteral("Left"), {},
            ZzFluentUI::ZzActivityArea::LeftPrimary, leftPanel));
        auto *bottomPanel = new QWidget;
        QVERIFY(shell->registerBottomPanel(
            zzPanelId("workspace-smoke-bottom"), QStringLiteral("Bottom"), {},
            bottomPanel));
        auto *page = new QWidget;
        QCOMPARE(shell->tabWidget()->addTab(page, QStringLiteral("Page")), 0);
        QVERIFY(shell->splitWorkspace()->setPageLayoutKey(
            page, QStringLiteral("workspace-smoke-layout-page")));

        auto *leftPane = shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left);
        auto *bottomPane = shell->bottomPane();
        leftPane->setPaneWidth(360);
        bottomPane->setPaneHeight(260);
        const auto saved = shell->saveLayout();
        QVERIFY(saved);

        leftPane->setPaneWidth(220);
        bottomPane->setPaneHeight(180);
        QVERIFY(shell->restoreLayout(saved.value()));
        QCOMPARE(leftPane->paneWidth(), 360);
        QCOMPARE(bottomPane->paneHeight(), 260);
    }

    void actualWindowShellShowsRegisteredFilesPanelForSftpCommand()
    {
        auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        auto contextResult = ZzExample::ZzExampleApplicationContext::create();
        QVERIFY(contextResult);
        auto context = std::move(contextResult).value();

        ZzPureTools::ZzApplicationBuilder builder;
        QVERIFY(builder.addPage(zzHomePage()));
        QVERIFY(builder.addNavigationNode({
            ZzPureTools::ZzRouteId(QStringLiteral("home")),
            QStringLiteral("ZzExampleWorkspaceSmokeTest"),
            QStringLiteral("Home"),
            {}}));
        QVERIFY(builder.setInitialRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("home"))));
        ZzPureTools::ZzApplicationWindow *assembledWindow = nullptr;
        QVERIFY(builder.setWindowSetupCallback(
            [context, application, &assembledWindow](
                ZzPureTools::ZzApplicationWindow &window) {
                assembledWindow = &window;
                return ZzExample::ZzExampleWindowShell::attach(
                    window, context, *application, false);
            }));
        QVERIFY(builder.build(*application));

        auto *window = assembledWindow;
        QVERIFY(window != nullptr);
        QVERIFY(ZzExample::ZzExampleWindowShell::attachedTo(*window) != nullptr);
        auto *palette = window->findChild<ZzFluentUI::ZzCommandPalette *>();
        QVERIFY(palette != nullptr);
        auto *commandBar = window->findChild<ZzFluentUI::ZzCommandBar *>(
            QStringLiteral("zzExampleOutputCommandBar"));
        QVERIFY(commandBar != nullptr);
        auto *bottomPane = window->findChild<ZzFluentUI::ZzBottomPane *>();
        QVERIFY(bottomPane != nullptr);
        auto *splitWorkspace = window->findChild<ZzFluentUI::ZzSplitWorkspace *>();
        QVERIFY(splitWorkspace != nullptr);
        ZzFluentUI::ZzSidePane *leftPane = nullptr;
        for (auto *pane : window->findChildren<ZzFluentUI::ZzSidePane *>()) {
            if (pane->edge() == ZzFluentUI::ZzSidePaneEdge::Left) {
                leftPane = pane;
                break;
            }
        }
        QVERIFY(leftPane != nullptr);
        const auto activityBars =
            window->findChildren<ZzFluentUI::ZzActivityBar *>();
        QCOMPARE(activityBars.size(), 2);
        ZzFluentUI::ZzActivityBar *leftActivityBar = nullptr;
        int activityRows = 0;
        for (auto *bar : activityBars) {
            if (bar->edge() == ZzFluentUI::ZzSidePaneEdge::Left) {
                leftActivityBar = bar;
            }
            for (auto *view : bar->findChildren<QListView *>()) {
                ZzFluentUI::ZzActivityArea expectedArea;
                if (view->objectName()
                    == QStringLiteral("zzActivityPrimaryView")) {
                    expectedArea = bar->edge()
                            == ZzFluentUI::ZzSidePaneEdge::Left
                        ? ZzFluentUI::ZzActivityArea::LeftPrimary
                        : ZzFluentUI::ZzActivityArea::RightPrimary;
                } else if (view->objectName()
                           == QStringLiteral("zzActivitySecondaryView")) {
                    expectedArea = bar->edge()
                            == ZzFluentUI::ZzSidePaneEdge::Left
                        ? ZzFluentUI::ZzActivityArea::LeftSecondary
                        : ZzFluentUI::ZzActivityArea::RightSecondary;
                } else {
                    continue;
                }
                QVERIFY(view->model() != nullptr);
                QCOMPARE(view->model()->rowCount(), 1);
                const QModelIndex index = view->model()->index(0, 0);
                QCOMPARE(index.data(static_cast<int>(
                                     ZzFluentUI::ZzActivityItemRole::Area))
                             .value<ZzFluentUI::ZzActivityArea>(),
                    expectedArea);
                QVERIFY2(
                    zzHasRenderableActivityIcon(index.data(
                        Qt::DecorationRole)),
                    qPrintable(QStringLiteral(
                        "Activity Bar row has no renderable icon: %1")
                                   .arg(index.data().toString())));
                ++activityRows;
            }
        }
        QCOMPARE(activityRows, 4);
        QVERIFY(leftActivityBar != nullptr);
        auto *filesPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleSftpPanel"));
        QVERIFY(filesPanel != nullptr);
        auto *sessionsPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleSessionPanel"));
        QVERIFY(sessionsPanel != nullptr);
        QVERIFY(leftPane->setCurrentWidget(sessionsPanel));
        QCOMPARE(leftPane->currentWidget(), sessionsPanel);

        auto *activityView = zzPrimaryActivityView(leftActivityBar);
        QVERIFY(activityView != nullptr);
        window->show();
        QCoreApplication::processEvents();
        leftPane->setCollapsed(true);
        QTest::mouseClick(
            activityView->viewport(), Qt::LeftButton, Qt::NoModifier,
            activityView->visualRect(
                activityView->model()->index(0, 0)).center());
        QVERIFY(!leftPane->isCollapsed());
        QCOMPARE(leftPane->currentWidget(), sessionsPanel);
        QCOMPARE(leftPane->visibleWidgets(),
            QList<QWidget *>({sessionsPanel, filesPanel}));
        QTest::mouseClick(
            activityView->viewport(), Qt::LeftButton, Qt::NoModifier,
            activityView->visualRect(
                activityView->model()->index(0, 0)).center());
        QVERIFY(leftPane->isCollapsed());

        palette->open();
        palette->setQuery(QStringLiteral("显示 SFTP"));
        QVERIFY(palette->activateCurrent());

        QCOMPARE(leftPane->currentWidget(), filesPanel);
        QVERIFY(!leftPane->isCollapsed());

        const auto rootGroup = splitWorkspace->groupIds().constFirst();
        auto *rootTabs = splitWorkspace->tabWidget(rootGroup);
        QVERIFY(rootTabs != nullptr);
        const int tabCountBeforeCommand = rootTabs->count();
        QSignalSpy commandTriggered(
            commandBar, &ZzFluentUI::ZzCommandBar::actionTriggered);
        QToolBar *const commandToolBar = commandBar->findChild<QToolBar *>();
        QVERIFY(commandToolBar != nullptr);
        QAction *const newTerminalAction = commandBar->primaryActions().constFirst();
        auto *newTerminalButton = qobject_cast<QToolButton *>(
            commandToolBar->widgetForAction(newTerminalAction));
        QVERIFY(newTerminalButton != nullptr);
        QTest::mouseClick(newTerminalButton, Qt::LeftButton);
        QCOMPARE(commandTriggered.count(), 1);
        QCOMPARE(commandTriggered.first().at(0).value<QAction *>(),
            newTerminalAction);
        QCOMPARE(rootTabs->count(), tabCountBeforeCommand + 1);

        auto *terminalPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleTerminalPanel"));
        auto *problemsPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleProblemsPanel"));
        auto *outputPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleOutputPanel"));
        QVERIFY(terminalPanel != nullptr);
        QVERIFY(problemsPanel != nullptr);
        QVERIFY(outputPanel != nullptr);
        QVERIFY(bottomPane->setCurrentWidget(terminalPanel));
        QCOMPARE(bottomPane->currentWidget(), terminalPanel);
        QVERIFY(bottomPane->setCurrentWidget(problemsPanel));
        QCOMPARE(bottomPane->currentWidget(), problemsPanel);
        QVERIFY(bottomPane->setCurrentWidget(outputPanel));
        QCOMPARE(bottomPane->currentWidget(), outputPanel);

        for (int index = 0; index < 4; ++index) {
            rootTabs->addTab(
                new QWidget,
                QStringLiteral("Drop test %1").arg(index + 1));
        }
        for (const auto zone : {ZzFluentUI::ZzWorkspaceDropZone::Top,
                 ZzFluentUI::ZzWorkspaceDropZone::Bottom,
                 ZzFluentUI::ZzWorkspaceDropZone::Left,
                 ZzFluentUI::ZzWorkspaceDropZone::Right}) {
            auto *const sourceTabs = splitWorkspace->tabWidget(rootGroup);
            QVERIFY(sourceTabs != nullptr);
            QVERIFY(sourceTabs->count() > 1);
            QVERIFY(splitWorkspace->moveTabToDropZone(
                rootGroup,
                sourceTabs->count() - 1,
                rootGroup,
                zone));
        }
        QCOMPARE(splitWorkspace->groupIds().size(), 5);
    }
};

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return EXIT_FAILURE;
    }
    QStandardPaths::setTestModeEnabled(true);
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzExampleWorkspaceSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzExampleWorkspaceSmokeTest.moc"
