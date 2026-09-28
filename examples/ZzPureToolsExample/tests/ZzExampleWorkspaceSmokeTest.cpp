#include <memory>
#include <string_view>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QStandardPaths>
#include <QtGui/QAction>
#include <QtGui/QGuiApplication>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtGui/QStandardItemModel>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QTableView>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzCore/ZzSettingsStore.h>

#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzColorPicker.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzCommandBar.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityArea.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
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
#include <ZzPureTools/ZzWorkspaceWindowHandle.h>
#include <ZzPureTools/ZzWorkspaceWindowConfiguration.h>
#include <ZzPureTools/ZzWorkspaceWindowCoordinator.h>
#include <ZzPureTools/ZzWorkspaceWindowCreateOptions.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleSettingsWindow.h"
#include "ZzExampleSystemPage.h"
#include "ZzExampleSystemPresenter.h"
#include "ZzExampleSystemViewModel.h"
#include "ZzExampleWindowShell.h"
#include "ZzExampleWorkspaceContent.h"
#include "ZzExampleFileModel.h"

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

[[nodiscard]] ZzPureTools::ZzApplicationWindow *zzOtherWindow(
    ZzPureTools::ZzPureApplication &application,
    ZzPureTools::ZzApplicationWindow *firstWindow)
{
    for (QWidget *widget : application.topLevelWidgets()) {
        auto *window = qobject_cast<ZzPureTools::ZzApplicationWindow *>(widget);
        if (window != nullptr && window != firstWindow) {
            return window;
        }
    }
    return nullptr;
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
        auto shellResult = ZzPureTools::ZzWorkspaceShell::create(&host, nullptr, ZzPureTools::ZzWorkspaceCenterMode::Tabbed);
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

    void systemSnapshotKeepsNameColumnUserResizable()
    {
        ZzExample::ZzExampleSystemViewModel model;
        ZzExample::ZzExampleSystemPage page(
            ZzExample::ZzExampleSystemPageKind::About,
            QStringLiteral("About"),
            &model,
            nullptr);
        auto *const snapshot = page.findChild<QTableView *>(
            QStringLiteral("zzExampleSystemSnapshot"));
        QVERIFY(snapshot != nullptr);
        if (snapshot == nullptr) {
            return;
        }
        QCOMPARE(
            snapshot->horizontalHeader()->sectionResizeMode(0),
            QHeaderView::Interactive);
        QCOMPARE(
            snapshot->horizontalHeader()->sectionResizeMode(1),
            QHeaderView::Stretch);
        QVERIFY(snapshot->horizontalHeader()->sectionSize(0) >= 180);
    }

    void sessionTreeFillsItsPanelWhenResized()
    {
        QStandardItemModel model;
        model.appendRow(new QStandardItem(QStringLiteral("Session")));
        auto panel = ZzExample::ZzExampleWorkspaceContent::createSessionPanel(&model);
        panel->resize(240, 320);
        panel->show();
        auto *tree = panel->findChild<QTreeView *>();
        QVERIFY(tree != nullptr);
        for (int width : {240, 420}) {
            panel->resize(width, 320);
            QCoreApplication::processEvents();
            QCOMPARE(tree->geometry(), panel->rect());
            QCOMPARE(tree->viewport()->width(), tree->width());
            const QModelIndex index = model.index(0, 0);
            QCOMPARE(tree->visualRect(index).right(), tree->viewport()->rect().right());
            QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                             tree->visualRect(index).center());
            QCOMPARE(tree->currentIndex(), index);
        }
    }

    void filePanelPreservesHierarchyAndDoesNotEditDemoData()
    {
        ZzExample::ZzExampleFileModel files;
        auto panel = ZzExample::ZzExampleWorkspaceContent::createSftpPanel(&files);
        auto *tree = qobject_cast<QTreeView *>(panel.get());
        QVERIFY(tree != nullptr);
        panel->resize(340, 320);
        panel->show();
        QCoreApplication::processEvents();
        auto *model = tree->model();
        QVERIFY(model != nullptr);
        QCOMPARE(model, &files);
        QCOMPARE(model->columnCount(), 2);
        const auto root = model->index(0, 0);
        QCOMPARE(model->rowCount(root), 2);
        QVERIFY(tree->isExpanded(root));
        QCOMPARE(tree->editTriggers(), QAbstractItemView::NoEditTriggers);
        QVERIFY(!model->flags(root).testFlag(Qt::ItemIsEditable));
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                         tree->visualRect(root).center());
        QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tree->visualRect(root).center());
        QVERIFY(!tree->isExpanded(root));
    }

    void filePanelAcceptsEmptyModels()
    {
        QStandardItemModel empty;
        QStandardItemModel singleColumn(1, 1);
        for (QAbstractItemModel *model : {static_cast<QAbstractItemModel *>(nullptr),
                                        static_cast<QAbstractItemModel *>(&empty),
                                        static_cast<QAbstractItemModel *>(&singleColumn)}) {
            auto panel = ZzExample::ZzExampleWorkspaceContent::createSftpPanel(model);
            auto *tree = qobject_cast<QTreeView *>(panel.get());
            QVERIFY(tree != nullptr);
            QCOMPARE(tree->model(), model);
        }
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
            [this, application, coordinator = application->workspaceWindowCoordinator(),
                primaryRegistered = false]
            (ZzPureTools::ZzApplicationWindow &window) mutable {
                initialWindow_ = initialWindow_ == nullptr
                    ? &window : initialWindow_;
                if (coordinator != nullptr) {
                    coordinator->setWindowFactory(
                        [application]
                        (const ZzPureTools::ZzWorkspaceWindowCreateOptions &) {
                            auto created = application->createWindow(
                                ZzPureTools::ZzApplicationWindowVisibility::Deferred);
                            if (!created) {
                                return ZzCore::ZzResult<
                                    ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                                    created.error());
                            }
                            auto *createdWindow = created.value();
                            auto *createdShell =
                                ZzExample::ZzExampleWindowShell::attachedTo(
                                *createdWindow);
                            if (createdShell == nullptr
                                || createdShell->workspaceShell() == nullptr) {
                                createdWindow->close();
                                return ZzCore::ZzResult<
                                    ZzPureTools::ZzWorkspaceWindowHandle>::failure(
                                    ZzCore::ZzError(
                                        ZzCore::ZzErrorCode::InvalidState,
                                        QStringLiteral("example window shell is unavailable")));
                            }
                            ZzPureTools::ZzWorkspaceWindowHandle handle;
                            handle.window = createdWindow;
                            handle.shell = createdShell->workspaceShell();
                            return ZzCore::ZzResult<
                                ZzPureTools::ZzWorkspaceWindowHandle>::success(
                                handle);
                        });
                }
                auto shellResult = ZzExample::ZzExampleWindowShell::attach(
                    window, context_, *application, false, centerMode_);
                if (!shellResult) {
                    return shellResult;
                }
                auto *shell = ZzExample::ZzExampleWindowShell::attachedTo(window);
                if (shell == nullptr || shell->workspaceShell() == nullptr) {
                    return ZzCore::ZzResult<void>::failure(
                        ZzCore::ZzError(
                            ZzCore::ZzErrorCode::InvalidState,
                            QStringLiteral("example window shell is unavailable")));
                }
                if (!primaryRegistered) {
                    primaryRegistered = true;
                    ZzPureTools::ZzWorkspaceWindowConfiguration configuration;
                    configuration.title = window.windowTitle();
                    configuration.icon = window.windowIcon();
                    configuration.titleMode =
                        shell->workspaceShell()->titleMode();
                    configuration.closePolicy =
                        ZzPureTools::ZzWindowClosePolicy::Allow;
                    configuration.alwaysOnTop = window.windowFlags().testFlag(
                        Qt::WindowStaysOnTopHint);
                    configuration.minimumSize = window.minimumSize();
                    configuration.maximumSize = window.maximumSize();
                    configuration.initialGeometry = window.geometry();
                    const auto registered = coordinator->registerWindow(
                        ZzPureTools::ZzWorkspaceWindowHandle{
                            &window, shell->workspaceShell()},
                        configuration,
                        true);
                    if (!registered) {
                        return registered;
                    }
                }
                return shellResult;
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
        auto *titleBar = window->titleBar();
        QVERIFY(titleBar != nullptr);
        QCOMPARE(window->menuWidget(), titleBar);
        QCOMPARE(titleBar->title(), window->windowTitle());
        auto *iconLabel = qobject_cast<QLabel *>(titleBar->windowIconWidget());
        QVERIFY(iconLabel != nullptr);
        QVERIFY(!iconLabel->pixmap().isNull());
        auto *titleMenuBar = titleBar->menuBar();
        QVERIFY(titleMenuBar != nullptr);
        for (auto *button : titleMenuBar->findChildren<QToolButton *>()) {
            QVERIFY(!button->isVisible());
        }
        QCOMPARE(titleMenuBar->actions().size(), 4);
        const QStringList expectedMenus{
            QStringLiteral("文件"),
            QStringLiteral("导航"),
            QStringLiteral("视图"),
            QStringLiteral("帮助")};
        QStringList actualMenus;
        for (QAction *const action : titleMenuBar->actions()) {
            actualMenus.append(action->text());
        }
        QCOMPARE(actualMenus, expectedMenus);
        for (QAction *const action : titleMenuBar->actions()) {
            QVERIFY(action->menu() != nullptr);
            if (action->menu() == nullptr) {
                continue;
            }
            QVERIFY(!action->menu()->actions().isEmpty());
        }
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
                    const QVariant descriptorValue =
                        index.data(Qt::DecorationRole);
                    const auto descriptor = descriptorValue
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
        QCOMPARE(navigationModel->rowCount(), 10);
        QVERIFY(!navigationModel->indexForRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("settings"))));
        QVERIFY(!navigationModel->indexForRoute(
            ZzPureTools::ZzRouteId(QStringLiteral("about"))));
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
        QVERIFY(!window->navigationController()->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("about"))));
        QCOMPARE(window->pageHost()->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("controls")));
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
        auto *const alwaysOnTopButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarAlwaysOnTopButton"));
        QVERIFY(alwaysOnTopButton != nullptr);
        if (alwaysOnTopButton == nullptr) {
            return;
        }
        QVERIFY(alwaysOnTopButton->isVisible());
        QTest::mouseClick(alwaysOnTopButton, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(titleBar->isAlwaysOnTop());
        QVERIFY(window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QTest::mouseClick(alwaysOnTopButton, Qt::LeftButton);
        ZZ_VERIFY_EVENTUALLY(!titleBar->isAlwaysOnTop());
        QVERIFY(!window->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
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

    void coordinatorTearOffRestoresTerminalPageToPrimaryWindow()
    {
        auto *application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        QVERIFY(context_ != nullptr);
        auto *coordinator = application->workspaceWindowCoordinator();
        QVERIFY(coordinator != nullptr);

        auto *window = initialWindow_;
        QVERIFY(window != nullptr);
        auto *shell = ZzExample::ZzExampleWindowShell::attachedTo(*window);
        QVERIFY(shell != nullptr);
        auto *workspaceShell = shell->workspaceShell();
        QVERIFY(workspaceShell != nullptr);

        const auto firstConfiguration = coordinator->configuration(window);
        QVERIFY(firstConfiguration);
        QVERIFY(!firstConfiguration.value().title.isEmpty());
        QCOMPARE(firstConfiguration.value().closePolicy,
            ZzPureTools::ZzWindowClosePolicy::Allow);

        auto *commandBar = window->findChild<ZzFluentUI::ZzCommandBar *>(
            QStringLiteral("zzExampleOutputCommandBar"));
        QVERIFY(commandBar != nullptr);
        QAction *const newTerminalAction = commandBar->primaryActions().constFirst();
        QVERIFY(newTerminalAction != nullptr);
        newTerminalAction->trigger();
        QCoreApplication::processEvents();

        auto *splitWorkspace = workspaceShell->splitWorkspace();
        QVERIFY(splitWorkspace != nullptr);
        const auto sourceGroup = splitWorkspace->activeGroupId();
        auto *sourceTabs = splitWorkspace->tabWidget(sourceGroup);
        QVERIFY(sourceTabs != nullptr);
        const int sourceIndex = sourceTabs->currentIndex();
        QVERIFY(sourceIndex >= 0);
        QWidget *const page = sourceTabs->widget(sourceIndex);
        QVERIFY(page != nullptr);
        const auto pageId = splitWorkspace->pageId(page);
        QVERIFY(pageId.isValid());
        QPointer<QWidget> pageGuard(page);

        ZzPureTools::ZzWorkspaceWindowCreateOptions options;
        options.configurationSource =
            ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow;
        options.sourceWindow = window;
        options.configuration.title = QStringLiteral("已撕出终端");
        options.configuration.closePolicy =
            ZzPureTools::ZzWindowClosePolicy::Delegate;

        const auto tearOffResult = coordinator->tearOff(
            splitWorkspace, sourceGroup, sourceIndex, options);
        if (!tearOffResult) {
            const auto &error = tearOffResult.error();
            QFAIL(qPrintable(QStringLiteral("tear-off failed: %1; %2")
                                 .arg(error.technicalMessage(), error.context())));
        }
        ZZ_VERIFY_EVENTUALLY(application->windowCount() == 2);

        auto *secondWindow = zzOtherWindow(*application, window);
        QVERIFY(secondWindow != nullptr);
        auto *secondShell = ZzExample::ZzExampleWindowShell::attachedTo(
            *secondWindow);
        QVERIFY(secondShell != nullptr);
        QVERIFY(secondShell != shell);
        QVERIFY(secondShell->workspaceShell() != workspaceShell);

        auto *secondWorkspaceShell = secondShell->workspaceShell();
        QVERIFY(secondWorkspaceShell != nullptr);
        auto *secondWorkspace = secondWorkspaceShell->splitWorkspace();
        QVERIFY(secondWorkspace != nullptr);
        auto *secondTabs =
            secondWorkspace->tabWidget(secondWorkspace->activeGroupId());
        QVERIFY(secondTabs != nullptr);
        QVERIFY(secondTabs->indexOf(page) >= 0);
        QCOMPARE(secondWorkspace->pageForId(pageId), page);

        const auto secondConfiguration = coordinator->configuration(
            secondWindow);
        QVERIFY(secondConfiguration);
        QCOMPARE(secondConfiguration.value().title, QStringLiteral("已撕出终端"));
        QCOMPARE(secondConfiguration.value().closePolicy,
            ZzPureTools::ZzWindowClosePolicy::Delegate);

        QVERIFY(coordinator->closeWindow(secondWindow));
        QCOMPARE(application->windowCount(), 2);
        QVERIFY(coordinator->approveDelegatedClose(secondWindow));
        ZZ_VERIFY_EVENTUALLY(application->windowCount() == 1);
        QCOMPARE(splitWorkspace->pageForId(pageId), page);
        QVERIFY(!pageGuard.isNull());

        QSignalSpy orphaned(
            coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::orphanedPages);
        QObject::connect(
            coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::orphanedPages,
            this,
            [splitWorkspace](const QList<QWidget *> &pages) {
                for (QWidget *const orphanedPage : pages) {
                    if (orphanedPage != nullptr) {
                        for (const auto &group : splitWorkspace->groupIds()) {
                            auto *const tabs = splitWorkspace->tabWidget(group);
                            const int index = tabs != nullptr
                                ? tabs->indexOf(orphanedPage) : -1;
                            if (index >= 0) {
                                tabs->removeTab(index);
                                break;
                            }
                        }
                        orphanedPage->setParent(nullptr);
                    }
                }
            });
        QVERIFY(coordinator->closeWindow(window));
        QCOMPARE(orphaned.count(), 1);
        const auto orphanedPages = orphaned.constFirst().constFirst()
                                       .value<QList<QWidget *>>();
        QVERIFY(orphanedPages.contains(page));
        ZZ_COMPARE_EVENTUALLY(application->windowCount(), 0);
        QVERIFY(!pageGuard.isNull());

        auto *replacementWindow = createAdditionalWindow();
        QVERIFY(replacementWindow != nullptr);
        auto *replacementShell =
            ZzExample::ZzExampleWindowShell::attachedTo(*replacementWindow);
        QVERIFY(replacementShell != nullptr);
        QVERIFY(replacementShell->workspaceShell() != nullptr);
        QVERIFY(coordinator->registerWindow(
            {replacementWindow, replacementShell->workspaceShell()},
            firstConfiguration.value(), true));
        initialWindow_ = replacementWindow;
    }

    void registeredSideTreesShareAppearanceAndFollowPaneWidth()
    {
        auto *window = createAdditionalWindow();
        auto *shell = ZzExample::ZzExampleWindowShell::attachedTo(*window)->workspaceShell();
        auto *pane = shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left);
        int rowHeight = -1;
        for (const auto &id : {"sessions", "files", "components"}) {
            QVERIFY(shell->showPanel(zzPanelId(id)));
            QWidget *content = pane->currentWidget();
            QVERIFY(content != nullptr);
            auto *tree = qobject_cast<QTreeView *>(content);
            if (tree == nullptr) {
                tree = content->findChild<QTreeView *>();
            }
            QVERIFY(tree != nullptr);
            QVERIFY(qobject_cast<ZzFluentUI::ZzFluentItemDelegate *>(tree->itemDelegate()) != nullptr);
            QCOMPARE(tree->frameShape(), QFrame::NoFrame);
            QCOMPARE(tree->viewport()->backgroundRole(), QPalette::Window);
            QCOMPARE(tree->selectionBehavior(), QAbstractItemView::SelectRows);
            for (int width : {240, 360}) {
                pane->setPaneWidth(width);
                QCoreApplication::processEvents();
                QCOMPARE(tree->width(), content->width());
                const int currentHeight = tree->visualRect(tree->model()->index(0, 0)).height();
                if (rowHeight < 0) {
                    rowHeight = currentHeight;
                }
                QCOMPARE(currentHeight, rowHeight);
                QCOMPARE(tree->viewport()->width(), tree->width());
            }
        }
        closeApplicationWindow(window);
    }

    void stackedWindowNavigatesWithoutOuterTabsAndKeepsPageEntrypoints()
    {
        const auto previousMode = centerMode_;
        centerMode_ = ZzPureTools::ZzWorkspaceCenterMode::Stacked;
        auto *window = createAdditionalWindow();
        centerMode_ = previousMode;
        QVERIFY(window != nullptr);
        auto *shell = ZzExample::ZzExampleWindowShell::attachedTo(*window)->workspaceShell();
        auto *stack = shell->stackWidget();
        QVERIFY(stack != nullptr);
        QVERIFY(shell->tabWidget() == nullptr);
        QVERIFY(shell->splitWorkspace() == nullptr);
        QCOMPARE(stack->count(), 1);
        QCOMPARE(stack->currentWidget(), window->pageHost());
        QVERIFY(stack->findChildren<QTabBar *>().isEmpty());

        auto *application = static_cast<ZzPureTools::ZzPureApplication *>(qApp);
        auto *coordinator = application->workspaceWindowCoordinator();
        QVERIFY(coordinator->registerWindow({window, shell}, {}));
        QSignalSpy pageChanges(coordinator,
            &ZzPureTools::ZzWorkspaceWindowCoordinator::activePageChanged);
        auto *commands = shell->commandPalette()->model();
        QVERIFY(commands != nullptr);
        Q_EMIT shell->commandPalette()->commandActivated(commands->index(0, 0));
        QCOMPARE(stack->count(), 2);
        QVERIFY(stack->currentWidget() != window->pageHost());
        QVERIFY(!pageChanges.isEmpty());
        const QPointer<QWidget> terminal(stack->currentWidget());
        auto *pagesMenu = window->findChild<QMenu *>(QStringLiteral("zzExampleOpenPagesMenu"));
        QVERIFY(pagesMenu != nullptr);
        Q_EMIT pagesMenu->aboutToShow();
        QCOMPARE(pagesMenu->actions().size(), 2);
        pagesMenu->actions().constFirst()->trigger();
        QCOMPARE(stack->currentWidget(), window->pageHost());
        pagesMenu->actions().constLast()->trigger();
        QCOMPARE(stack->currentWidget(), terminal.data());

        const auto currentRoute = window->navigationController()->currentRoute();
        auto currentNode = window->navigationModel()->indexForRoute(currentRoute);
        QVERIFY(currentNode);
        Q_EMIT window->navigationPane()->navigationRequested(currentNode.value());
        QCOMPARE(stack->currentWidget(), window->pageHost());
        QCOMPARE(window->navigationController()->currentRoute(), currentRoute);
        stack->setCurrentWidget(terminal);
        QVERIFY(window->navigationController()->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("controls"))));
        QCOMPARE(stack->currentWidget(), window->pageHost());
        stack->setCurrentWidget(terminal);
        Q_EMIT shell->commandPalette()->commandActivated(commands->index(1, 0));
        QVERIFY(terminal.isNull());
        QCOMPARE(stack->currentWidget(), window->pageHost());
        QCOMPARE(stack->count(), 1);

        ZzPureTools::ZzWorkspaceWindowCreateOptions options;
        options.sourceWindow = window;
        options.configurationSource = ZzPureTools::ZzWorkspaceConfigurationSource::SourceWindow;
        centerMode_ = ZzPureTools::ZzWorkspaceCenterMode::Stacked;
        auto second = coordinator->createWindow(options);
        centerMode_ = previousMode;
        QVERIFY(second);
        QVERIFY(second.value().shell->stackWidget() != stack);
        QVERIFY(coordinator->closeWindow(second.value().window));
        QVERIFY(coordinator->closeWindow(window));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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

        auto *const settingsTitleBar =
            settings->findChild<ZzFluentUI::ZzFluentTitleBar *>();
        QVERIFY(settingsTitleBar != nullptr);
        if (settingsTitleBar == nullptr) {
            return;
        }
        auto *const settingsPinButton = settingsTitleBar->findChild<
            QToolButton *>(QStringLiteral("zzTitleBarAlwaysOnTopButton"));
        auto *const settingsThemeButton = settingsTitleBar->findChild<
            QToolButton *>(QStringLiteral("zzTitleBarThemeButton"));
        auto *const settingsMinimizeButton = settingsTitleBar->findChild<
            QToolButton *>(QStringLiteral("zzTitleBarMinimizeButton"));
        auto *const settingsMaximizeButton = settingsTitleBar->findChild<
            QToolButton *>(QStringLiteral("zzTitleBarMaximizeButton"));
        auto *const settingsCloseButton = settingsTitleBar->findChild<
            QToolButton *>(QStringLiteral("zzTitleBarCloseButton"));
        QVERIFY(settingsPinButton != nullptr);
        QVERIFY(settingsThemeButton != nullptr);
        QVERIFY(settingsMinimizeButton != nullptr);
        QVERIFY(settingsMaximizeButton != nullptr);
        QVERIFY(settingsCloseButton != nullptr);
        if (settingsPinButton == nullptr) {
            return;
        }
        if (settingsThemeButton == nullptr) {
            return;
        }
        if (settingsMinimizeButton == nullptr
            || settingsMaximizeButton == nullptr
            || settingsCloseButton == nullptr) {
            return;
        }
        QVERIFY(settingsPinButton->isHidden());
        QVERIFY(settingsThemeButton->isHidden());
        QVERIFY(settingsMinimizeButton->isHidden());
        QVERIFY(settingsMaximizeButton->isHidden());
        // macOS 使用原生标题栏关闭按钮；其他平台由组件提供自绘关闭按钮。
        if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
            QVERIFY(settingsCloseButton->isVisible());
        }
        auto *const settingsThemeBox = settings->findChild<QComboBox *>();
        QVERIFY(settingsThemeBox != nullptr);
        if (settingsThemeBox == nullptr) {
            return;
        }
        QCOMPARE(settingsThemeBox->count(), 3);
        QCOMPARE(settingsThemeBox->itemData(0).toInt(), static_cast<int>(ZzFluentUI::ZzThemeMode::Light));
        QCOMPARE(settingsThemeBox->itemData(1).toInt(), static_cast<int>(ZzFluentUI::ZzThemeMode::Dark));
        QCOMPARE(settingsThemeBox->itemData(2).toInt(), static_cast<int>(ZzFluentUI::ZzThemeMode::System));
        settingsThemeBox->setCurrentIndex(
            settingsThemeBox->findData(static_cast<int>(ZzFluentUI::ZzThemeMode::Dark)));
        ZZ_VERIFY_EVENTUALLY(
            settingsTitleBar->themeMode() == ZzFluentUI::ZzThemeMode::Dark);
        ZZ_VERIFY_EVENTUALLY(
            window->titleBar()->themeMode() == ZzFluentUI::ZzThemeMode::Dark);

        auto *const themeButton = window->titleBar()->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarThemeButton"));
        QVERIFY(themeButton != nullptr);
        QVERIFY(themeButton->menu() == nullptr);
        themeButton->click();
        ZZ_VERIFY_EVENTUALLY(
            settingsThemeBox->currentData().toInt()
            == static_cast<int>(ZzFluentUI::ZzThemeMode::Light));
        settingsThemeBox->setCurrentIndex(2);
        QCOMPARE(window->titleBar()->themeMode(), ZzFluentUI::ZzThemeMode::System);
        auto *const application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        const auto beforeToggle = application->themeController()->resolvedMode();
        themeButton->click();
        QCOMPARE(application->themeController()->mode(),
                 beforeToggle == ZzFluentUI::ZzThemeMode::Dark
                     ? ZzFluentUI::ZzThemeMode::Light : ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(settingsThemeBox->currentData().toInt(),
                 static_cast<int>(application->themeController()->mode()));

        closeSettings(settings);
        closeApplicationWindow(window);
    }

    void titleBarThemeChangeIsPreservedWhenSettingsWindowOpens()
    {
        auto *const window = createAdditionalWindow();
        QVERIFY(window != nullptr);
        auto *const application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        QVERIFY(context_ != nullptr);

        const auto persistedLight = context_->settingsStore().write(
            QStringView(QStringLiteral("appearance/themeMode")),
            static_cast<int>(ZzFluentUI::ZzThemeMode::Light));
        QVERIFY(persistedLight);

        application->themeController()->setMode(ZzFluentUI::ZzThemeMode::Light);
        auto *const themeButton = window->titleBar()->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarThemeButton"));
        QVERIFY(themeButton != nullptr);
        QVERIFY(themeButton->menu() == nullptr);
        themeButton->click();
        ZZ_VERIFY_EVENTUALLY(
            window->titleBar()->themeMode()
            == ZzFluentUI::ZzThemeMode::Dark);
        const auto persistedTheme = context_->settingsStore().read(
            QStringView(QStringLiteral("appearance/themeMode")), -1);
        QVERIFY(persistedTheme);
        QCOMPARE(
            persistedTheme.value().toInt(),
            static_cast<int>(ZzFluentUI::ZzThemeMode::Dark));

        QAction *const settings = settingsAction(window);
        QVERIFY(settings != nullptr);
        settings->trigger();
        QCoreApplication::processEvents();
        auto *const settingsWindow = this->settingsWindow(window);
        QVERIFY(settingsWindow != nullptr);
        if (settingsWindow == nullptr) {
            closeApplicationWindow(window);
            return;
        }
        auto *const settingsThemeBox = settingsWindow->findChild<QComboBox *>();
        QVERIFY(settingsThemeBox != nullptr);
        if (settingsThemeBox == nullptr) {
            closeSettings(settingsWindow);
            closeApplicationWindow(window);
            return;
        }

        QCOMPARE(
            static_cast<int>(window->titleBar()->themeMode()),
            static_cast<int>(ZzFluentUI::ZzThemeMode::Dark));
        QCOMPARE(
            settingsThemeBox->currentData().toInt(),
            static_cast<int>(ZzFluentUI::ZzThemeMode::Dark));

        closeSettings(settingsWindow);
        closeApplicationWindow(window);
    }

    void accentSelectionUpdatesThemeAndPersistsWithoutFeedback()
    {
        auto *const window = createAdditionalWindow();
        auto *const application = qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
        QVERIFY(application != nullptr);
        auto *const theme = application->themeController();
        settingsAction(window)->trigger();
        auto *settings = settingsWindow(window);
        QVERIFY(settings != nullptr);
        auto *picker = settings->findChild<ZzFluentUI::ZzColorPicker *>();
        QVERIFY(picker != nullptr);
        QVERIFY(!picker->isAlphaEnabled());
        auto *const otherWindow = createAdditionalWindow();
        settingsAction(otherWindow)->trigger();
        auto *const otherSettings = settingsWindow(otherWindow);
        QVERIFY(otherSettings != nullptr);
        auto *const otherPicker = otherSettings->findChild<ZzFluentUI::ZzColorPicker *>();
        QVERIFY(otherPicker != nullptr);
        const QColor accent(QStringLiteral("#9c42bf"));
        picker->setCurrentColor(accent);
        QCOMPARE(theme->accentColor(), accent);
        QCOMPARE(otherPicker->currentColor(), accent);
        const auto saved = context_->settingsStore().read(
            QStringView(QStringLiteral("appearance/accentColor")), QString());
        QVERIFY(saved);
        QCOMPARE(saved.value().toString(), QStringLiteral("#9c42bf"));

        ZzFluentUI::ZzThemeController restoredTheme;
        ZzExample::ZzExampleSystemPresenter::restoreAppearanceSettings(*context_, restoredTheme);
        QCOMPARE(restoredTheme.accentColor(), accent);
        QCOMPARE(restoredTheme.mode(), theme->mode());

        // 外部主题通知只更新界面，不应触发保存或覆盖运行时主题。
        const QColor externalAccent(QStringLiteral("#217a46"));
        theme->setAccentColor(externalAccent);
        QCOMPARE(picker->currentColor(), externalAccent);
        QCOMPARE(otherPicker->currentColor(), externalAccent);
        const auto unchanged = context_->settingsStore().read(
            QStringView(QStringLiteral("appearance/accentColor")), QString());
        QVERIFY(unchanged);
        QCOMPARE(unchanged.value(), saved.value());
        closeSettings(settings);
        settingsAction(window)->trigger();
        settings = settingsWindow(window);
        QVERIFY(settings != nullptr);
        picker = settings->findChild<ZzFluentUI::ZzColorPicker *>();
        QVERIFY(picker != nullptr);
        QCOMPARE(picker->currentColor(), externalAccent);
        closeSettings(settings);
        closeSettings(otherSettings);
        closeApplicationWindow(otherWindow, baselineWindowCount_ + 1);
        closeApplicationWindow(window);
    }

    void restoresInvalidAppearanceSettingsWithSafeDefaults()
    {
        QVERIFY(context_->settingsStore().write(
            QStringView(QStringLiteral("appearance/themeMode")), 3));
        QVERIFY(context_->settingsStore().write(
            QStringView(QStringLiteral("appearance/accentColor")), QStringLiteral("invalid")));
        ZzFluentUI::ZzThemeController restoredTheme;
        const auto defaultAccent = restoredTheme.accentColor();
        ZzExample::ZzExampleSystemPresenter::restoreAppearanceSettings(*context_, restoredTheme);
        QCOMPARE(restoredTheme.mode(), ZzFluentUI::ZzThemeMode::System);
        QCOMPARE(restoredTheme.accentColor(), defaultAccent);
    }

    void aboutActionCreatesResizableWindowWithOneCloseCommand()
    {
        auto *window = createAdditionalWindow();
        QAction *const action = aboutAction(window);
        QVERIFY(action != nullptr);
        if (action == nullptr) {
            closeApplicationWindow(window);
            return;
        }

        action->trigger();
        QCoreApplication::processEvents();

        QMainWindow *const about = aboutWindow(window);
        QVERIFY(about != nullptr);
        if (about == nullptr) {
            closeApplicationWindow(window);
            return;
        }
        QCOMPARE(about->parentWidget(), window);
        QCOMPARE(about->windowModality(), Qt::WindowModal);
        QVERIFY(about->testAttribute(Qt::WA_DeleteOnClose));
        QVERIFY(about->isVisible());

        auto *const titleBar =
            about->findChild<ZzFluentUI::ZzFluentTitleBar *>();
        QVERIFY(titleBar != nullptr);
        auto *const snapshot = about->findChild<QTableView *>(
            QStringLiteral("zzExampleSystemSnapshot"));
        QVERIFY(snapshot != nullptr);
        if (titleBar == nullptr || snapshot == nullptr) {
            closeSettings(about);
            closeApplicationWindow(window);
            return;
        }
        auto *const iconLabel = qobject_cast<QLabel *>(
            titleBar->windowIconWidget());
        QVERIFY(iconLabel != nullptr);
        if (iconLabel == nullptr) {
            closeSettings(about);
            closeApplicationWindow(window);
            return;
        }
        QVERIFY(!iconLabel->pixmap().isNull());
        auto *const minimizeButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarMinimizeButton"));
        auto *const maximizeButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarMaximizeButton"));
        auto *const closeButton = titleBar->findChild<QToolButton *>(
            QStringLiteral("zzTitleBarCloseButton"));
        QVERIFY(minimizeButton != nullptr);
        QVERIFY(maximizeButton != nullptr);
        QVERIFY(closeButton != nullptr);
        if (minimizeButton == nullptr || maximizeButton == nullptr
            || closeButton == nullptr) {
            closeSettings(about);
            closeApplicationWindow(window);
            return;
        }
        QVERIFY(minimizeButton->isHidden());
        QVERIFY(maximizeButton->isHidden());
        if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
            QVERIFY(closeButton->isVisible());
        }
        QCOMPARE(
            snapshot->horizontalHeader()->sectionResizeMode(0),
            QHeaderView::Interactive);
        QCOMPARE(
            snapshot->horizontalHeader()->sectionResizeMode(1),
            QHeaderView::Stretch);
        QVERIFY(snapshot->horizontalHeader()->sectionSize(0) >= 180);

        action->trigger();
        QCOMPARE(aboutWindow(window), about);
        QCOMPARE(window->findChildren<QMainWindow *>(
                     QStringLiteral("zzExampleAboutWindow"),
                     Qt::FindDirectChildrenOnly).size(), 1);

        closeSettings(about);
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

    [[nodiscard]] static QAction *aboutAction(
        ZzPureTools::ZzApplicationWindow *window)
    {
        return window == nullptr
            ? nullptr
            : window->findChild<QAction *>(
                  QStringLiteral("zzExampleAboutAction"));
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

    [[nodiscard]] static QMainWindow *aboutWindow(
        ZzPureTools::ZzApplicationWindow *window)
    {
        return window == nullptr
            ? nullptr
            : window->findChild<QMainWindow *>(
                  QStringLiteral("zzExampleAboutWindow"),
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
    ZzPureTools::ZzWorkspaceCenterMode centerMode_ =
        ZzPureTools::ZzWorkspaceCenterMode::Tabbed;
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
    QPixmap testIcon(QSize(16, 16));
    testIcon.fill(Qt::green);
    QApplication::setWindowIcon(QIcon(testIcon));
    ZzExampleWorkspaceSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzExampleWorkspaceSmokeTest.moc"
