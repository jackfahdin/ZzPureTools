#include <memory>
#include <string_view>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QStandardPaths>
#include <QtGui/QAction>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QApplication>
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
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzNavigationPlacement.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageLifetimePolicy.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleSettingsWindow.h"
#include "ZzExampleWindowShell.h"

namespace {

[[nodiscard]] QString zzFromUtf8(std::string_view text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzPage(
    const ZzExample::ZzExampleRouteDescriptor &route)
{
    ZzPureTools::ZzPageRegistration registration;
    registration.routeId = ZzPureTools::ZzRouteId(zzFromUtf8(route.routeId));
    registration.lifetime = route.lifetime;
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

[[nodiscard]] QStringList zzActivityTitles(QListView *view)
{
    QStringList titles;
    if (view == nullptr || view->model() == nullptr) {
        return titles;
    }
    for (int row = 0; row < view->model()->rowCount(); ++row) {
        titles.append(view->model()->index(row, 0).data().toString());
    }
    return titles;
}

[[nodiscard]] QListView *zzPrimaryActivityView(
    ZzFluentUI::ZzActivityBar *bar)
{
    return bar->findChild<QListView *>(
        QStringLiteral("zzActivityPrimaryView"));
}

[[nodiscard]] QListView *zzSecondaryActivityView(
    ZzFluentUI::ZzActivityBar *bar)
{
    return bar->findChild<QListView *>(
        QStringLiteral("zzActivitySecondaryView"));
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
        : descriptor.resourceId.startsWith(QStringLiteral(":/"))
            && QFile::exists(descriptor.resourceId);
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
    void cleanupTestCase()
    {
        auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        application->beginShutdown();
    }

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
        context_ = std::move(contextResult).value();

        ZzPureTools::ZzApplicationBuilder builder;
        for (const auto &route : ZzExample::ZzExampleRouteCatalog::routes()) {
            QVERIFY(builder.addPage(zzPage(route)));
            ZzPureTools::ZzNavigationNode node{
                ZzPureTools::ZzRouteId(zzFromUtf8(route.routeId)),
                QStringLiteral("ZzExampleWorkspaceSmokeTest"),
                zzFromUtf8(route.title),
                {}};
            node.placement = route.placement;
            QVERIFY(builder.addNavigationNode(std::move(node)));
        }
        QVERIFY(builder.setInitialRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("home"))));
        QVERIFY(builder.setWindowSetupCallback(
            [this, application](ZzPureTools::ZzApplicationWindow &window) {
                initialWindow_ = initialWindow_ == nullptr
                    ? &window : initialWindow_;
                return ZzExample::ZzExampleWindowShell::attach(
                    window, context_, *application, false);
            }));
        const auto buildResult = builder.build(*application);
        if (!buildResult) {
            const QString diagnostic = QStringLiteral("%1; %2")
                .arg(
                    buildResult.error().technicalMessage(),
                    buildResult.error().context());
            QFAIL(qPrintable(diagnostic));
        }
        baselineWindowCount_ = application->windowCount();
        QCOMPARE(baselineWindowCount_, 1);

        auto *window = initialWindow_;
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
        ZzFluentUI::ZzSidePane *rightPane = nullptr;
        for (auto *pane : window->findChildren<ZzFluentUI::ZzSidePane *>()) {
            if (pane->edge() == ZzFluentUI::ZzSidePaneEdge::Left) {
                leftPane = pane;
            } else if (pane->edge() == ZzFluentUI::ZzSidePaneEdge::Right) {
                rightPane = pane;
            }
        }
        QVERIFY(leftPane != nullptr);
        QVERIFY(rightPane != nullptr);
        const auto activityBars =
            window->findChildren<ZzFluentUI::ZzActivityBar *>();
        QCOMPARE(activityBars.size(), 2);
        ZzFluentUI::ZzActivityBar *leftActivityBar = nullptr;
        ZzFluentUI::ZzActivityBar *rightActivityBar = nullptr;
        for (auto *bar : activityBars) {
            if (bar->edge() == ZzFluentUI::ZzSidePaneEdge::Left) {
                leftActivityBar = bar;
            } else if (bar->edge() == ZzFluentUI::ZzSidePaneEdge::Right) {
                rightActivityBar = bar;
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
                for (int row = 0; row < view->model()->rowCount(); ++row) {
                    const QModelIndex index = view->model()->index(row, 0);
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
                    const auto descriptor = index.data(Qt::DecorationRole)
                                                .value<ZzFluentUI::ZzIconDescriptor>();
                    if (index.data().toString() == QStringLiteral("设置")) {
                        QCOMPARE(
                            descriptor.source,
                            ZzFluentUI::ZzIconSource::FontGlyph);
                        QCOMPARE(
                            descriptor.fontIcon,
                            ZzFluentUI::ZzFontIcon::Gear);
                    } else if (index.data().toString()
                               == QStringLiteral("组件")) {
                        QCOMPARE(
                            descriptor.source,
                            ZzFluentUI::ZzIconSource::FontGlyph);
                        QCOMPARE(
                            descriptor.fontIcon,
                            ZzFluentUI::ZzFontIcon::PuzzlePiece);
                    } else {
                        QCOMPARE(
                            descriptor.source,
                            ZzFluentUI::ZzIconSource::SvgResource);
                    }
                }
            }
        }
        QVERIFY(leftActivityBar != nullptr);
        QVERIFY(rightActivityBar != nullptr);
        auto *leftPrimaryView = zzPrimaryActivityView(leftActivityBar);
        auto *leftSecondaryView = zzSecondaryActivityView(leftActivityBar);
        auto *rightPrimaryView = zzPrimaryActivityView(rightActivityBar);
        auto *rightSecondaryView = zzSecondaryActivityView(rightActivityBar);
        QVERIFY(leftPrimaryView != nullptr);
        QVERIFY(leftSecondaryView != nullptr);
        QVERIFY(rightPrimaryView != nullptr);
        QVERIFY(rightSecondaryView != nullptr);
        QCOMPARE(zzActivityTitles(leftPrimaryView),
            QStringList({QStringLiteral("会话"), QStringLiteral("文件"),
                QStringLiteral("组件")}));
        QCOMPARE(zzActivityTitles(leftSecondaryView),
            QStringList({QStringLiteral("设置")}));
        QCOMPARE(zzActivityTitles(rightPrimaryView),
            QStringList({QStringLiteral("属性"), QStringLiteral("任务")}));
        QVERIFY(zzActivityTitles(rightSecondaryView).isEmpty());

        auto *navigationModel = window->navigationModel();
        QVERIFY(navigationModel != nullptr);
        QCOMPARE(navigationModel->rowCount(), 11);
        QVERIFY(!navigationModel->indexForRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("settings"))));
        const auto aboutIndex = navigationModel->indexForRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("about")));
        QVERIFY(aboutIndex);
        QCOMPARE(aboutIndex.value().data(static_cast<int>(
                     ZzPureTools::ZzNavigationRole::Placement))
                     .value<ZzFluentUI::ZzNavigationPlacement>(),
            ZzFluentUI::ZzNavigationPlacement::Footer);
        QVERIFY(!window->navigationController()->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("settings"))));

        const auto rootGroup = splitWorkspace->groupIds().constFirst();
        auto *rootTabs = splitWorkspace->tabWidget(rootGroup);
        QVERIFY(rootTabs != nullptr);
        QVERIFY(rootTabs->findChildren<
            ZzFluentUI::ZzNavigationPane *>().isEmpty());
        const int pageHostIndex = rootTabs->indexOf(window->pageHost());
        QVERIFY(pageHostIndex >= 0);
        QVERIFY(rootTabs->isTabPinned(pageHostIndex));
        QVERIFY(!rootTabs->isTabCloseEnabled(pageHostIndex));
        QCOMPARE(window->pageHost()->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("home")));
        QVERIFY(window->navigationController()->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("controls"))));
        QCOMPARE(window->pageHost()->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("controls")));
        QVERIFY(window->navigationController()->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("about"))));
        QCOMPARE(window->pageHost()->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("about")));
        QCOMPARE(window->findChild<QWidget *>(
                     QStringLiteral("zzExampleSessionPanel")), nullptr);
        QCOMPARE(window->findChild<QWidget *>(
                     QStringLiteral("zzExampleSftpPanel")), nullptr);
        QCOMPARE(window->findChild<QWidget *>(
                     QStringLiteral("zzExamplePropertiesPanel")), nullptr);
        QCOMPARE(window->findChild<QWidget *>(
                     QStringLiteral("zzExampleTasksPanel")), nullptr);
        QVERIFY(!leftPane->isCollapsed());
        ZZ_COMPARE_EVENTUALLY(leftPane->currentWidget(), window->navigationPane());
        ZZ_COMPARE_EVENTUALLY(leftPane->visibleWidgets(),
            QList<QWidget *>({window->navigationPane()}));
        QVERIFY(rightPane->isCollapsed());
        QVERIFY(rightPane->visibleWidgets().isEmpty());

        window->show();
        window->activateWindow();
        QCoreApplication::processEvents();
        ZZ_VERIFY_EVENTUALLY(window->isActiveWindow());
        QTest::mouseClick(
            leftPrimaryView->viewport(), Qt::LeftButton, Qt::NoModifier,
            leftPrimaryView->visualRect(
                leftPrimaryView->model()->index(0, 0)).center());
        ZZ_VERIFY_EVENTUALLY(window->findChild<QWidget *>(
            QStringLiteral("zzExampleSessionPanel")) != nullptr);
        QWidget *const sessionsPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleSessionPanel"));
        QVERIFY(sessionsPanel != nullptr);
        if (sessionsPanel == nullptr) {
            return;
        }
        QVERIFY(!leftPane->isCollapsed());
        QCOMPARE(leftPane->currentWidget(), sessionsPanel);
        QCOMPARE(window->findChildren<QWidget *>(
                     QStringLiteral("zzExampleSessionPanel")).size(), 1);
        QTest::mouseClick(
            leftPrimaryView->viewport(), Qt::LeftButton, Qt::NoModifier,
            leftPrimaryView->visualRect(
                leftPrimaryView->model()->index(1, 0)).center());
        ZZ_VERIFY_EVENTUALLY(window->findChild<QWidget *>(
            QStringLiteral("zzExampleSftpPanel")) != nullptr);
        QWidget *const filesPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleSftpPanel"));
        QVERIFY(filesPanel != nullptr);
        if (filesPanel == nullptr) {
            return;
        }
        QCOMPARE(leftPane->currentWidget(), filesPanel);
        QVERIFY(!leftPane->isCollapsed());
        QCOMPARE(window->findChildren<QWidget *>(
                     QStringLiteral("zzExampleSftpPanel")).size(), 1);
        QCOMPARE(leftPane->visibleWidgets(),
            QList<QWidget *>({filesPanel}));

        QTest::mouseClick(
            leftPrimaryView->viewport(), Qt::LeftButton, Qt::NoModifier,
            leftPrimaryView->visualRect(
                leftPrimaryView->model()->index(2, 0)).center());
        ZZ_COMPARE_EVENTUALLY(leftPane->currentWidget(), window->navigationPane());
        ZZ_COMPARE_EVENTUALLY(leftPane->visibleWidgets(),
            QList<QWidget *>({window->navigationPane()}));

        QTest::mouseClick(
            rightPrimaryView->viewport(), Qt::LeftButton, Qt::NoModifier,
            rightPrimaryView->visualRect(
                rightPrimaryView->model()->index(0, 0)).center());
        ZZ_VERIFY_EVENTUALLY(window->findChild<QWidget *>(
            QStringLiteral("zzExamplePropertiesPanel")) != nullptr);
        QWidget *const propertiesPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExamplePropertiesPanel"));
        QVERIFY(propertiesPanel != nullptr);
        if (propertiesPanel == nullptr) {
            return;
        }
        QVERIFY(!rightPane->isCollapsed());
        QCOMPARE(rightPane->currentWidget(), propertiesPanel);
        QCOMPARE(window->findChildren<QWidget *>(
                     QStringLiteral("zzExamplePropertiesPanel")).size(), 1);

        QTest::mouseClick(
            rightPrimaryView->viewport(), Qt::LeftButton, Qt::NoModifier,
            rightPrimaryView->visualRect(
                rightPrimaryView->model()->index(1, 0)).center());
        ZZ_VERIFY_EVENTUALLY(window->findChild<QWidget *>(
            QStringLiteral("zzExampleTasksPanel")) != nullptr);
        QWidget *const tasksPanel = window->findChild<QWidget *>(
            QStringLiteral("zzExampleTasksPanel"));
        QVERIFY(tasksPanel != nullptr);
        if (tasksPanel == nullptr) {
            return;
        }
        QCOMPARE(rightPane->currentWidget(), tasksPanel);
        QCOMPARE(window->findChildren<QWidget *>(
                     QStringLiteral("zzExampleTasksPanel")).size(), 1);
        QCOMPARE(rightPane->visibleWidgets(),
            QList<QWidget *>({tasksPanel}));

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

    void settingsActionCreatesOneWindowModalChildPerMainWindow()
    {
        auto *window = createAdditionalWindow();
        QAction *const action = settingsAction(window);
        QVERIFY(action != nullptr);

        action->trigger();
        QCoreApplication::processEvents();

        QMainWindow *const settings = settingsWindow(window);
        QVERIFY(settings != nullptr);
        QCOMPARE(settings->parentWidget(), window);
        QCOMPARE(settings->windowModality(), Qt::WindowModal);
        QVERIFY(settings->windowFlags().testFlag(Qt::Window));
        QVERIFY(!settings->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(settings->testAttribute(Qt::WA_DeleteOnClose));
        QVERIFY(settings->isVisible());

        closeSettings(settings);
        closeApplicationWindow(window);
    }

    void repeatedSettingsActivationRaisesExistingWindow()
    {
        auto *window = createAdditionalWindow();
        QAction *const action = settingsAction(window);
        QVERIFY(action != nullptr);
        action->trigger();
        QCoreApplication::processEvents();
        QMainWindow *const first = settingsWindow(window);
        QVERIFY(first != nullptr);
        if (first == nullptr) {
            return;
        }

        window->raise();
        window->activateWindow();
        action->trigger();

        QCOMPARE(settingsWindow(window), first);
        QCOMPARE(window->findChildren<QMainWindow *>(
                     QStringLiteral("zzExampleSettingsWindow"),
                     Qt::FindDirectChildrenOnly).size(), 1);
        QVERIFY(first->isVisible());
        ZZ_VERIFY_EVENTUALLY(first->isActiveWindow());

        closeSettings(first);
        closeApplicationWindow(window);
    }

    void closingSettingsAllowsRecreation()
    {
        auto *window = createAdditionalWindow();
        QAction *const action = settingsAction(window);
        QVERIFY(action != nullptr);
        action->trigger();
        QCoreApplication::processEvents();
        QPointer<QMainWindow> first(settingsWindow(window));
        QVERIFY(!first.isNull());

        first->close();
        ZZ_VERIFY_EVENTUALLY(first.isNull());
        QCOMPARE(settingsWindow(window), nullptr);

        action->trigger();
        QCoreApplication::processEvents();
        QMainWindow *const recreated = settingsWindow(window);
        QVERIFY(recreated != nullptr);
        if (recreated == nullptr) {
            return;
        }
        QVERIFY(recreated->isVisible());

        closeSettings(recreated);
        closeApplicationWindow(window);
    }

    void settingsWindowsAreIsolatedAcrossTwoMainWindows()
    {
        auto *firstWindow = createAdditionalWindow();
        auto *secondWindow = createAdditionalWindow();
        QAction *const firstAction = settingsAction(firstWindow);
        QAction *const secondAction = settingsAction(secondWindow);
        QVERIFY(firstAction != nullptr);
        QVERIFY(secondAction != nullptr);
        QVERIFY(firstAction != secondAction);
        auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        auto *secondShell =
            ZzExample::ZzExampleWindowShell::attachedTo(*secondWindow);
        QVERIFY(secondShell != nullptr);
        auto mismatched = ZzExample::ZzExampleSettingsWindow::create(
            firstWindow, context_, application, secondShell);
        QVERIFY(!mismatched);

        firstAction->trigger();
        secondAction->trigger();
        QCoreApplication::processEvents();
        QMainWindow *const firstSettings = settingsWindow(firstWindow);
        QMainWindow *const secondSettings = settingsWindow(secondWindow);
        QVERIFY(firstSettings != nullptr);
        QVERIFY(secondSettings != nullptr);
        QVERIFY(firstSettings != secondSettings);
        QCOMPARE(firstSettings->parentWidget(), firstWindow);
        QCOMPARE(secondSettings->parentWidget(), secondWindow);

        closeSettings(firstSettings);
        closeSettings(secondSettings);
        closeApplicationWindow(secondWindow, baselineWindowCount_ + 1);
        closeApplicationWindow(firstWindow);
    }

    void commandPaletteAndActivityUseTheSameSettingsAction()
    {
        auto *window = createAdditionalWindow();
        QAction *const action = settingsAction(window);
        QVERIFY(action != nullptr);
        QSignalSpy triggered(action, &QAction::triggered);

        auto *leftBar = window->findChild<ZzFluentUI::ZzActivityBar *>();
        for (auto *bar : window->findChildren<ZzFluentUI::ZzActivityBar *>()) {
            if (bar->edge() == ZzFluentUI::ZzSidePaneEdge::Left) {
                leftBar = bar;
                break;
            }
        }
        QVERIFY(leftBar != nullptr);
        auto *activityView = zzSecondaryActivityView(leftBar);
        QVERIFY(activityView != nullptr);
        window->show();
        window->raise();
        window->activateWindow();
        QCoreApplication::processEvents();
        ZZ_VERIFY_EVENTUALLY(window->isActiveWindow());
        QModelIndex settingsIndex;
        for (int row = 0; row < activityView->model()->rowCount(); ++row) {
            const QModelIndex candidate = activityView->model()->index(row, 0);
            if (candidate.data().toString() == QStringLiteral("设置")) {
                settingsIndex = candidate;
                break;
            }
        }
        QVERIFY(settingsIndex.isValid());
        QTest::mouseClick(
            activityView->viewport(), Qt::LeftButton, Qt::NoModifier,
            activityView->visualRect(settingsIndex).center());
        ZZ_COMPARE_EVENTUALLY(triggered.count(), 1);

        auto *palette = window->findChild<ZzFluentUI::ZzCommandPalette *>();
        QVERIFY(palette != nullptr);
        QCOMPARE(palette->model()->rowCount(), 7);
        palette->setQuery(QStringLiteral("打开设置"));
        palette->open();
        QCOMPARE(palette->resultCount(), 1);
        QVERIFY(palette->activateCurrent());
        QCOMPARE(triggered.count(), 2);

        closeSettings(settingsWindow(window));
        closeApplicationWindow(window);
    }

    void closingMainWindowClosesOnlyItsSettingsWindow()
    {
        auto *firstWindow = createAdditionalWindow();
        auto *secondWindow = createAdditionalWindow();
        QAction *const firstAction = settingsAction(firstWindow);
        QAction *const secondAction = settingsAction(secondWindow);
        QVERIFY(firstAction != nullptr);
        QVERIFY(secondAction != nullptr);
        firstAction->trigger();
        secondAction->trigger();
        QCoreApplication::processEvents();
        QPointer<ZzPureTools::ZzApplicationWindow> firstWindowGuard(firstWindow);
        QPointer<QMainWindow> firstSettings(settingsWindow(firstWindow));
        QPointer<QMainWindow> secondSettings(settingsWindow(secondWindow));
        QVERIFY(!firstSettings.isNull());
        QVERIFY(!secondSettings.isNull());

        firstWindow->close();
        ZZ_VERIFY_EVENTUALLY(firstWindowGuard.isNull());
        ZZ_VERIFY_EVENTUALLY(firstSettings.isNull());
        QVERIFY(!secondSettings.isNull());
        QVERIFY(secondSettings->isVisible());

        closeSettings(secondSettings.data());
        closeApplicationWindow(secondWindow);
    }

private:
    [[nodiscard]] ZzPureTools::ZzApplicationWindow *createAdditionalWindow()
    {
        auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        if (application == nullptr) {
            return nullptr;
        }
        auto result = application->createWindow();
        return result ? std::move(result).value() : nullptr;
    }

    [[nodiscard]] static QAction *settingsAction(
        ZzPureTools::ZzApplicationWindow *window)
    {
        return window == nullptr
            ? nullptr
            : window->findChild<QAction *>(
                  QStringLiteral("zzExampleSettingsAction"));
    }

    [[nodiscard]] static QMainWindow *settingsWindow(
        ZzPureTools::ZzApplicationWindow *window)
    {
        return window == nullptr
            ? nullptr
            : window->findChild<QMainWindow *>(
                  QStringLiteral("zzExampleSettingsWindow"),
                  Qt::FindDirectChildrenOnly);
    }

    static void closeSettings(QMainWindow *settings)
    {
        QVERIFY(settings != nullptr);
        QPointer<QMainWindow> guard(settings);
        settings->close();
        ZZ_VERIFY_EVENTUALLY(guard.isNull());
    }

    void closeApplicationWindow(
        ZzPureTools::ZzApplicationWindow *window,
        qsizetype expectedWindowCount = -1)
    {
        QVERIFY(window != nullptr);
        auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        window->close();
        const qsizetype expected = expectedWindowCount < 0
            ? baselineWindowCount_ : expectedWindowCount;
        ZZ_COMPARE_EVENTUALLY(application->windowCount(), expected);
    }

    std::shared_ptr<ZzExample::ZzExampleApplicationContext> context_;
    ZzPureTools::ZzApplicationWindow *initialWindow_ = nullptr;
    qsizetype baselineWindowCount_ = 0;
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
