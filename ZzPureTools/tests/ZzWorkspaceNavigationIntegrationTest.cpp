#include <functional>
#include <memory>
#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtGui/QAction>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <ZzTestEventLoop.h>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzActivityItemRole.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceActivityId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>

namespace {

using ZzWindowSetup = std::function<ZzCore::ZzResult<void>(
    ZzPureTools::ZzApplicationWindow &)>;

class ZzVisibilityEventFilter final : public QObject
{
public:
    int hideCount = 0;
    int showCount = 0;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event != nullptr && event->type() == QEvent::Hide) {
            ++hideCount;
        } else if (event != nullptr && event->type() == QEvent::Show) {
            ++showCount;
        }
        return QObject::eventFilter(watched, event);
    }
};

[[nodiscard]] ZzCore::ZzResult<void> zzFailure(QString message)
{
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidState, std::move(message)));
}

[[nodiscard]] ZzPureTools::ZzPureApplication &zzApplication()
{
    auto *const application =
        qobject_cast<ZzPureTools::ZzPureApplication *>(qApp);
    Q_ASSERT(application != nullptr);
    return *application;
}

[[nodiscard]] ZzPureTools::ZzWorkspacePanelId zzPanelId(
    const char *value)
{
    return ZzPureTools::ZzWorkspacePanelId(QString::fromLatin1(value));
}

[[nodiscard]] ZzPureTools::ZzWorkspaceActivityId zzActivityId(
    const char *value)
{
    return ZzPureTools::ZzWorkspaceActivityId(QString::fromLatin1(value));
}

[[nodiscard]] ZzFluentUI::ZzIconDescriptor zzIcon()
{
    return ZzFluentUI::ZzIconDescriptor::fromFontIcon(
        ZzFluentUI::ZzFontIcon::PuzzlePiece);
}

[[nodiscard]] ZzPureTools::ZzPageRegistration zzHomePage()
{
    ZzPureTools::ZzPageRegistration page;
    page.routeId = ZzPureTools::ZzRouteId(QStringLiteral("home"));
    page.lifetime = ZzPureTools::ZzPageLifetimePolicy::WhileActive;
    page.factory = [](QWidget *parent)
        -> ZzCore::ZzResult<std::unique_ptr<ZzPureTools::ZzPageInstance>> {
        return ZzPureTools::ZzPageInstance::create(
            parent,
            new QWidget(parent),
            std::make_unique<QObject>(),
            std::make_unique<QObject>());
    };
    return page;
}

void zzConfigureApplication(
    ZzPureTools::ZzApplicationBuilder &builder,
    ZzWindowSetup *currentSetup)
{
    QVERIFY(builder.addPage(zzHomePage()));
    QVERIFY(builder.addNavigationNode({
        ZzPureTools::ZzRouteId(QStringLiteral("home")),
        QStringLiteral("ZzWorkspaceNavigationIntegrationTest"),
        QStringLiteral("Home"),
        {}}));
    QVERIFY(builder.setInitialRoute(
        ZzPureTools::ZzRouteId(QStringLiteral("home"))));
    QVERIFY(builder.setWindowSetupCallback(
        [currentSetup](ZzPureTools::ZzApplicationWindow &window) {
            return currentSetup != nullptr && *currentSetup
                ? (*currentSetup)(window)
                : ZzCore::ZzResult<void>::success();
        }));
}

} // namespace

class ZzWorkspaceNavigationIntegrationTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        currentSetup_ = [](ZzPureTools::ZzApplicationWindow &) {
            return ZzCore::ZzResult<void>::success();
        };
        ZzPureTools::ZzApplicationBuilder builder;
        zzConfigureApplication(builder, &currentSetup_);
        QVERIFY(builder.build(zzApplication()));
        baselineWindowCount_ = zzApplication().windowCount();
        QCOMPARE(baselineWindowCount_, 1);
    }

    void cleanup()
    {
        currentSetup_ = [](ZzPureTools::ZzApplicationWindow &) {
            return ZzCore::ZzResult<void>::success();
        };
    }

    void cleanupTestCase()
    {
        zzApplication().beginShutdown();
    }

    void integratesNavigationPaneAndPageHostWithoutChangingRoute()
    {
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
        QPointer<QWidget> originalBody;
        QPointer<QWidget> workspace;
        ZzPureTools::ZzNavigationModel *modelBefore = nullptr;
        ZzPureTools::ZzNavigationController *controllerBefore = nullptr;
        QPointer<ZzFluentUI::ZzNavigationPane> navigationBefore;
        QPointer<ZzPureTools::ZzPageHost> pageHostBefore;
        ZzPureTools::ZzRouteId routeBefore;
        ZzPureTools::ZzRouteId routeAfter;
        bool surfaceAuditPassed = false;

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            originalBody = window.centralWidget();
            modelBefore = window.navigationModel();
            controllerBefore = window.navigationController();
            navigationBefore = window.navigationPane();
            pageHostBefore = window.pageHost();
            routeBefore = controllerBefore->currentRoute();
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            workspace = shell->workspaceWidget();
            auto integrated = shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
            if (!integrated) {
                return integrated;
            }
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            routeAfter = controllerBefore->currentRoute();
            auto *const leftPane = shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left);
            auto *const tabs = shell->tabWidget();
            const int pageIndex = tabs != nullptr
                ? tabs->indexOf(pageHostBefore) : -1;
            surfaceAuditPassed = originalBody.isNull()
                && window.centralWidget() == nullptr
                && leftPane != nullptr
                && leftPane->isAncestorOf(navigationBefore)
                && tabs != nullptr && pageIndex >= 0
                && tabs->isTabPinned(pageIndex)
                && !tabs->isTabCloseEnabled(pageIndex)
                && shell->splitWorkspace()->isAncestorOf(pageHostBefore)
                && window.navigationModel() == modelBefore
                && window.navigationController() == controllerBefore
                && window.navigationPane() == navigationBefore
                && window.pageHost() == pageHostBefore
                && routeAfter == routeBefore;
            if (!surfaceAuditPassed) {
                return zzFailure(QStringLiteral(
                    "integrated navigation surface identity audit failed"));
            }
            window.setCentralWidget(workspace);
            return ZzCore::ZzResult<void>::success();
        };

        auto createdWindow = zzApplication().createWindow();

        QVERIFY(createdWindow);
        auto *const window = createdWindow.value();
        QVERIFY(surfaceAuditPassed);
        QCOMPARE(window->navigationModel(), modelBefore);
        QCOMPARE(window->navigationController(), controllerBefore);
        QCOMPARE(window->navigationPane(), navigationBefore);
        QCOMPARE(window->pageHost(), pageHostBefore);
        QCOMPARE(
            controllerBefore->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("home")));
        QVERIFY(!routeBefore.isValid());
        QCOMPARE(routeAfter, routeBefore);
        QCOMPARE(window->centralWidget(), workspace.data());
        QCOMPARE(workspace->parentWidget(), window);
        ZzVisibilityEventFilter visibilityAudit;
        window->installEventFilter(&visibilityAudit);
        window->show();
        QCoreApplication::processEvents();
        const int hideCountBefore = visibilityAudit.hideCount;
        const int showCountBefore = visibilityAudit.showCount;
        QVERIFY(shell->setAlwaysOnTop(true));
        QCoreApplication::processEvents();
        QCOMPARE(visibilityAudit.hideCount, hideCountBefore);
        QCOMPARE(visibilityAudit.showCount, showCountBefore);
        QVERIFY(window->isVisible());
        QVERIFY(shell->setAlwaysOnTop(false));
        QCoreApplication::processEvents();
        QCOMPARE(visibilityAudit.hideCount, hideCountBefore);
        QCOMPARE(visibilityAudit.showCount, showCountBefore);
        window->removeEventFilter(&visibilityAudit);
        shell.reset();
        QVERIFY(workspace != nullptr);
        QCOMPARE(window->centralWidget(), workspace.data());
        QCOMPARE(workspace->parentWidget(), window);
        QCOMPARE(window->navigationPane(), navigationBefore.data());
        QCOMPARE(window->pageHost(), pageHostBefore.data());
        QCOMPARE(window->navigationModel(), modelBefore);
        QCOMPARE(window->navigationController(), controllerBefore);
        modelBefore->refreshTranslations();
        QCOMPARE(
            controllerBefore->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("home")));
        QVERIFY(controllerBefore->navigate(
            ZzPureTools::ZzRouteId(QStringLiteral("home"))));
        closeWindow(window);
        QVERIFY(workspace.isNull());
        QVERIFY(navigationBefore.isNull());
        QVERIFY(pageHostBefore.isNull());
    }

    void preservesIntegratedSurfacesBeforeWorkspaceMount()
    {
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
        QPointer<QWidget> workspace;
        QPointer<ZzFluentUI::ZzNavigationPane> navigation;
        QPointer<ZzPureTools::ZzPageHost> pageHost;
        ZzPureTools::ZzNavigationModel *model = nullptr;
        ZzPureTools::ZzNavigationController *controller = nullptr;
        ZzPureTools::ZzRouteId routeBefore;
        bool survivedShellDestruction = false;

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            model = window.navigationModel();
            controller = window.navigationController();
            routeBefore = controller->currentRoute();
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            workspace = shell->workspaceWidget();
            auto integrated = shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
            if (!integrated) {
                return integrated;
            }
            navigation = window.navigationPane();
            pageHost = window.pageHost();
            const bool ownedBeforeDestruction = window.centralWidget() == nullptr
                && workspace != nullptr && workspace->parentWidget() == &window
                && navigation != nullptr && pageHost != nullptr;

            shell.reset();

            survivedShellDestruction = ownedBeforeDestruction
                && workspace != nullptr && workspace->parentWidget() == &window
                && navigation != nullptr && pageHost != nullptr
                && window.navigationPane() == navigation.data()
                && window.pageHost() == pageHost.data()
                && window.navigationModel() == model
                && window.navigationController() == controller
                && controller->currentRoute() == routeBefore;
            if (!survivedShellDestruction) {
                return zzFailure(QStringLiteral(
                    "unmounted integrated surfaces did not survive Shell destruction"));
            }
            model->refreshTranslations();
            if (!controller->navigate(
                    ZzPureTools::ZzRouteId(QStringLiteral("home")))) {
                return zzFailure(QStringLiteral(
                    "unmounted integrated navigation stopped working"));
            }
            return ZzCore::ZzResult<void>::success();
        };

        auto createdWindow = zzApplication().createWindow();

        QVERIFY(createdWindow);
        auto *const window = createdWindow.value();
        QVERIFY(survivedShellDestruction);
        QCOMPARE(window->centralWidget(), nullptr);
        QCOMPARE(workspace->parentWidget(), window);
        QCOMPARE(window->navigationPane(), navigation.data());
        QCOMPARE(window->pageHost(), pageHost.data());
        QCOMPARE(window->navigationModel(), model);
        QCOMPARE(window->navigationController(), controller);
        QCOMPARE(
            controller->currentRoute(),
            ZzPureTools::ZzRouteId(QStringLiteral("home")));
        closeWindow(window);
        QVERIFY(workspace.isNull());
        QVERIFY(navigation.isNull());
        QVERIFY(pageHost.isNull());
    }

    void pinsPageHostAndRejectsSecondIntegration()
    {
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
        ZzCore::ZzErrorCode secondCode = ZzCore::ZzErrorCode::Unknown;
        int tabCountBeforeSecond = -1;
        int panelCountBeforeSecond = -1;
        bool secondRejectedWithoutMutation = false;

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            auto integrated = shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
            if (!integrated) {
                return integrated;
            }
            auto *const tabs = shell->tabWidget();
            auto *const stack = shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->panelStack();
            tabCountBeforeSecond = tabs->count();
            panelCountBeforeSecond = stack->panelCount();
            const auto route = window.navigationController()->currentRoute();
            auto second = shell->integrateApplicationNavigation(
                zzPanelId("again"), QStringLiteral("Again"), zzIcon(),
                ZzFluentUI::ZzActivityArea::RightPrimary,
                QStringLiteral("Again"));
            if (!second) {
                secondCode = second.error().code();
            }
            secondRejectedWithoutMutation = !second
                && tabs->count() == tabCountBeforeSecond
                && stack->panelCount() == panelCountBeforeSecond
                && window.navigationController()->currentRoute() == route;
            window.setCentralWidget(shell->workspaceWidget());
            return secondRejectedWithoutMutation
                ? ZzCore::ZzResult<void>::success()
                : zzFailure(QStringLiteral("second integration mutated state"));
        };

        auto createdWindow = zzApplication().createWindow();

        QVERIFY(createdWindow);
        auto *const window = createdWindow.value();
        QVERIFY(secondRejectedWithoutMutation);
        QCOMPARE(secondCode, ZzCore::ZzErrorCode::InvalidState);
        auto *const tabs = shell->tabWidget();
        const int index = tabs->indexOf(window->pageHost());
        QVERIFY(index >= 0);
        tabs->setTabPinned(index, false);
        QVERIFY(tabs->isTabPinned(tabs->indexOf(window->pageHost())));
        tabs->setTabCloseEnabled(tabs->indexOf(window->pageHost()), true);
        QVERIFY(!tabs->isTabCloseEnabled(tabs->indexOf(window->pageHost())));
        closeWindow(window);
        shell.reset();
    }

    void rejectsPlainMainWindowWithoutMutation()
    {
        QMainWindow host;
        auto *const body = new QWidget(&host);
        host.setCentralWidget(body);
        auto created = ZzPureTools::ZzWorkspaceShell::create(&host);
        QVERIFY(created);
        auto shell = std::move(created).value();
        auto *const tabs = shell->tabWidget();
        auto *const stack = shell->sidePane(
            ZzFluentUI::ZzSidePaneEdge::Left)->panelStack();
        const int tabCount = tabs->count();
        const int panelCount = stack->panelCount();
        const int activityCount = shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount();

        const auto auditInvalidHost = [&](
                                          const ZzPureTools::ZzWorkspacePanelId &panelId,
                                          const QString &panelTitle,
                                          ZzFluentUI::ZzActivityArea area,
                                          const QString &tabTitle) {
            auto integrated = shell->integrateApplicationNavigation(
                panelId, panelTitle, zzIcon(), area, tabTitle);
            return !integrated
                && integrated.error().code() == ZzCore::ZzErrorCode::InvalidState
                && host.centralWidget() == body
                && body->parentWidget() == &host
                && tabs->count() == tabCount
                && stack->panelCount() == panelCount
                && shell->activityBar(
                    ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount()
                    == activityCount;
        };

        QVERIFY(auditInvalidHost(
            zzPanelId("components"), QStringLiteral("Components"),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            QStringLiteral("Component examples")));
        QVERIFY(auditInvalidHost(
            zzPanelId(""), QStringLiteral("Components"),
            ZzFluentUI::ZzActivityArea::LeftPrimary,
            QStringLiteral("Component examples")));
        QVERIFY(auditInvalidHost(
            zzPanelId("components"), QString(),
            ZzFluentUI::ZzActivityArea::RightSecondary, QString()));
        QCOMPARE(host.centralWidget(), body);
        QCOMPARE(body->parentWidget(), &host);
        QCOMPARE(tabs->count(), tabCount);
        QCOMPARE(stack->panelCount(), panelCount);
        QCOMPARE(shell->activityBar(
            ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(),
            activityCount);
    }

    void rejectsReplacedBodyOrForeignNavigationParent()
    {
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
        bool replacedBodyRejected = false;
        bool foreignParentRejected = false;

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            QWidget *const body = window.takeCentralWidget();
            auto *const replacement = new QWidget;
            window.setCentralWidget(replacement);
            auto *const tabs = shell->tabWidget();
            auto *const stack = shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left)->panelStack();
            const int tabCount = tabs->count();
            const int panelCount = stack->panelCount();
            auto replaced = shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
            replacedBodyRejected = !replaced
                && window.centralWidget() == replacement
                && window.navigationPane()->parentWidget() == body
                && window.pageHost()->parentWidget() == body
                && tabs->count() == tabCount
                && stack->panelCount() == panelCount;
            QWidget *const returnedReplacement = window.takeCentralWidget();
            window.setCentralWidget(body);
            delete returnedReplacement;

            auto *const bodyLayout = qobject_cast<QHBoxLayout *>(body->layout());
            auto *const navigation = window.navigationPane();
            const int navigationIndex = bodyLayout->indexOf(navigation);
            bodyLayout->removeWidget(navigation);
            auto *const foreignParent = new QWidget(&window);
            navigation->setParent(foreignParent);
            const auto route = window.navigationController()->currentRoute();
            auto foreign = shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
            foreignParentRejected = !foreign
                && window.centralWidget() == body
                && navigation->parentWidget() == foreignParent
                && window.pageHost()->parentWidget() == body
                && tabs->count() == tabCount
                && stack->panelCount() == panelCount
                && window.navigationController()->currentRoute() == route;
            navigation->setParent(body);
            bodyLayout->insertWidget(navigationIndex, navigation);
            delete foreignParent;
            return replacedBodyRejected && foreignParentRejected
                ? ZzCore::ZzResult<void>::success()
                : zzFailure(QStringLiteral("invalid body mutation was not atomic"));
        };

        auto createdWindow = zzApplication().createWindow();

        QVERIFY(createdWindow);
        QVERIFY(replacedBodyRejected);
        QVERIFY(foreignParentRejected);
        closeWindow(createdWindow.value());
        shell.reset();
    }

    void duplicatePanelIdRollsBackBodyModelCurrentAndOwnership()
    {
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
        QPointer<QWidget> occupied;
        QAction fixedAction(QStringLiteral("Settings"));
        QPointer<QAction> fixedActionGuard(&fixedAction);
        QSignalSpy fixedTriggered(&fixedAction, &QAction::triggered);
        bool rollbackAuditPassed = false;
        bool lateRollbackAuditPassed = false;

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            occupied = new QWidget;
            auto occupiedRegistered = shell->registerSidePanel(
                zzPanelId("components"), QStringLiteral("Occupied"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                occupied);
            if (!occupiedRegistered) {
                return occupiedRegistered;
            }
            auto fixedRegistered = shell->registerFixedActivityAction(
                zzActivityId("settings"), QStringLiteral("Settings"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftSecondary,
                &fixedAction);
            if (!fixedRegistered) {
                return fixedRegistered;
            }
            QWidget *const body = window.centralWidget();
            QWidget *const navigationParent = window.navigationPane()->parentWidget();
            QWidget *const pageParent = window.pageHost()->parentWidget();
            auto *const tabs = shell->tabWidget();
            auto *const leftPane = shell->sidePane(
                ZzFluentUI::ZzSidePaneEdge::Left);
            auto *const stack = leftPane->panelStack();
            auto *const model = shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left)->model();
            auto *const activityBar = shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left);
            QWidget *const current = leftPane->currentWidget();
            const QList<QWidget *> visible = leftPane->visibleWidgets();
            const QList<int> sizes = stack->panelSizes();
            const bool collapsed = leftPane->isCollapsed();
            const QModelIndex activityCurrent =
                activityBar->currentSourceIndex();
            const QList<QModelIndex> activityActive =
                activityBar->activeSourceIndexes();
            const int tabCount = tabs->count();
            const int panelCount = stack->panelCount();
            const int rowCount = model->rowCount();
            QStringList rowTitles;
            QList<ZzFluentUI::ZzActivityArea> rowAreas;
            QList<Qt::ItemFlags> rowFlags;
            for (int row = 0; row < rowCount; ++row) {
                const QModelIndex index = model->index(row, 0);
                rowTitles.append(index.data(Qt::DisplayRole).toString());
                rowAreas.append(index.data(static_cast<int>(
                    ZzFluentUI::ZzActivityItemRole::Area))
                    .value<ZzFluentUI::ZzActivityArea>());
                rowFlags.append(index.flags());
            }
            const auto route = window.navigationController()->currentRoute();

            auto integrated = shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));

            rollbackAuditPassed = !integrated
                && window.centralWidget() == body
                && window.navigationPane()->parentWidget() == navigationParent
                && window.pageHost()->parentWidget() == pageParent
                && tabs->count() == tabCount
                && stack->panelCount() == panelCount
                && model->rowCount() == rowCount
                && leftPane->currentWidget() == current
                && leftPane->isAncestorOf(occupied)
                && window.navigationController()->currentRoute() == route;

            bool routeChangedDuringPin = false;
            QObject::connect(
                tabs, &ZzFluentUI::ZzTabWidget::tabPinnedChanged,
                &window, [&](int, bool pinned) {
                    if (!pinned || routeChangedDuringPin) {
                        return;
                    }
                    routeChangedDuringPin = static_cast<bool>(
                        window.navigationController()->navigate(
                            ZzPureTools::ZzRouteId(QStringLiteral("home"))));
                });
            auto lateFailure = shell->integrateApplicationNavigation(
                zzPanelId("navigation"), QStringLiteral("Navigation"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
            lateRollbackAuditPassed = routeChangedDuringPin && !lateFailure
                && window.centralWidget() == body
                && window.navigationPane()->parentWidget() == navigationParent
                && window.pageHost()->parentWidget() == pageParent
                && tabs->count() == tabCount
                && stack->panelCount() == panelCount
                && model->rowCount() == rowCount
                && leftPane->currentWidget() == current
                && leftPane->visibleWidgets() == visible
                && stack->panelSizes() == sizes
                && leftPane->isCollapsed() == collapsed
                && activityBar->currentSourceIndex() == activityCurrent
                && activityBar->activeSourceIndexes() == activityActive
                && fixedActionGuard == &fixedAction
                && leftPane->isAncestorOf(occupied)
                && window.navigationController()->currentRoute() == route;
            QStringList rowTitlesAfter;
            QList<ZzFluentUI::ZzActivityArea> rowAreasAfter;
            QList<Qt::ItemFlags> rowFlagsAfter;
            for (int row = 0; row < model->rowCount(); ++row) {
                const QModelIndex index = model->index(row, 0);
                rowTitlesAfter.append(index.data(Qt::DisplayRole).toString());
                rowAreasAfter.append(index.data(static_cast<int>(
                    ZzFluentUI::ZzActivityItemRole::Area))
                    .value<ZzFluentUI::ZzActivityArea>());
                rowFlagsAfter.append(index.flags());
            }
            lateRollbackAuditPassed = lateRollbackAuditPassed
                && rowTitlesAfter == rowTitles
                && rowAreasAfter == rowAreas
                && rowFlagsAfter == rowFlags;
            const QModelIndex fixedIndex = model->index(1, 0);
            Q_EMIT activityBar->activationRequested(fixedIndex);
            lateRollbackAuditPassed = lateRollbackAuditPassed
                && fixedActionGuard == &fixedAction
                && fixedTriggered.count() == 1;
            return rollbackAuditPassed && lateRollbackAuditPassed
                ? ZzCore::ZzResult<void>::success()
                : zzFailure(QStringLiteral("duplicate panel rollback failed"));
        };

        auto createdWindow = zzApplication().createWindow();

        QVERIFY(createdWindow);
        QVERIFY(rollbackAuditPassed);
        QVERIFY(lateRollbackAuditPassed);
        QVERIFY(occupied != nullptr);
        closeWindow(createdWindow.value());
        shell.reset();
        QVERIFY(occupied.isNull());
    }

    void synchronousDestructionDuringTransferRollsBackOrFailsClosed()
    {
        std::unique_ptr<ZzPureTools::ZzWorkspaceShell> shell;
        QPointer<QWidget> workspace;
        bool pinDestructionEntered = false;
        bool rollbackDestructionEntered = false;
        bool bodyPollutionEntered = false;
        bool postCommitMutationsSucceeded = false;

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            workspace = shell->workspaceWidget();
            QObject::connect(
                shell->tabWidget(),
                &ZzFluentUI::ZzTabWidget::tabPinnedChanged,
                &window, [&](int, bool pinned) {
                    if (!pinned) {
                        return;
                    }
                    pinDestructionEntered = true;
                    shell.reset();
                });
            return shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
        };

        auto createdWindow = zzApplication().createWindow();

        QVERIFY(pinDestructionEntered);
        QVERIFY(!createdWindow);
        QVERIFY(shell == nullptr);
        QVERIFY(workspace.isNull());
        QCOMPARE(zzApplication().windowCount(), baselineWindowCount_);

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            workspace = shell->workspaceWidget();
            auto *const tabs = shell->tabWidget();
            bool routeChanged = false;
            QObject::connect(
                tabs, &ZzFluentUI::ZzTabWidget::tabPinnedChanged,
                &window, [&](int, bool pinned) {
                    if (!pinned || routeChanged) {
                        return;
                    }
                    routeChanged = static_cast<bool>(
                        window.navigationController()->navigate(
                            ZzPureTools::ZzRouteId(QStringLiteral("home"))));
                });
            QObject::connect(
                shell->activityBar(ZzFluentUI::ZzSidePaneEdge::Left)->model(),
                &QAbstractItemModel::rowsAboutToBeRemoved,
                &window, [&](const QModelIndex &, int, int) {
                    rollbackDestructionEntered = true;
                    shell.reset();
                });
            return shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
        };

        createdWindow = zzApplication().createWindow();

        QVERIFY(rollbackDestructionEntered);
        QVERIFY(!createdWindow);
        QVERIFY(shell == nullptr);
        QVERIFY(workspace.isNull());
        QCOMPARE(zzApplication().windowCount(), baselineWindowCount_);

        currentSetup_ = [&](ZzPureTools::ZzApplicationWindow &window) {
            auto created = ZzPureTools::ZzWorkspaceShell::create(
                &window, window.titleBar());
            if (!created) {
                return ZzCore::ZzResult<void>::failure(created.error());
            }
            shell = std::move(created).value();
            workspace = shell->workspaceWidget();
            const QString applicationTitleBefore = shell->applicationTitle();
            const QString customTitleBefore = shell->customTitle();
            const auto titleModeBefore = shell->titleMode();
            const auto differentTitleMode = titleModeBefore
                    == ZzPureTools::ZzWorkspaceTitleMode::Application
                ? ZzPureTools::ZzWorkspaceTitleMode::Custom
                : ZzPureTools::ZzWorkspaceTitleMode::Application;
            const bool alwaysOnTopBefore = shell->isAlwaysOnTop();
            QObject::connect(
                window.centralWidget(), &QObject::destroyed,
                &window, [&] {
                    bodyPollutionEntered = true;
                    static_cast<void>(window.navigationController()->navigate(
                        ZzPureTools::ZzRouteId(QStringLiteral("home"))));
                });
            auto integrated = shell->integrateApplicationNavigation(
                zzPanelId("components"), QStringLiteral("Components"),
                zzIcon(), ZzFluentUI::ZzActivityArea::LeftPrimary,
                QStringLiteral("Component examples"));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            auto *const registeredContent = new QWidget;
            auto registration = shell->registerSidePanel(
                zzPanelId("registered"), QStringLiteral("Registered"), zzIcon(),
                ZzFluentUI::ZzActivityArea::RightPrimary, registeredContent);
            if (!registration) {
                delete registeredContent;
            }
            auto activation = shell->showPanel(
                zzPanelId("components"), false);
            const auto savedLayout = shell->saveLayout();
            const bool layoutRoundTrip = savedLayout
                && shell->restoreLayout(savedLayout.value());
            shell->setApplicationTitle(
                applicationTitleBefore + QStringLiteral(" poisoned"));
            shell->setCustomTitle(
                customTitleBefore + QStringLiteral(" poisoned"));
            shell->setTitleMode(differentTitleMode);
            auto alwaysOnTop = shell->setAlwaysOnTop(!alwaysOnTopBefore);
            auto *const activityBar = shell->activityBar(
                ZzFluentUI::ZzSidePaneEdge::Left);
            const QModelIndex navigationIndex = activityBar->model()->index(0, 0);
            const auto areaBefore = navigationIndex.data(static_cast<int>(
                ZzFluentUI::ZzActivityItemRole::Area))
                .value<ZzFluentUI::ZzActivityArea>();
            Q_EMIT activityBar->moveRequested(
                navigationIndex, ZzFluentUI::ZzActivityArea::RightPrimary, 0);
            postCommitMutationsSucceeded = bodyPollutionEntered && integrated
                && registration && activation && layoutRoundTrip && alwaysOnTop
                && shell->applicationTitle()
                    == applicationTitleBefore + QStringLiteral(" poisoned")
                && shell->customTitle()
                    == customTitleBefore + QStringLiteral(" poisoned")
                && shell->titleMode() == differentTitleMode
                && shell->isAlwaysOnTop() == !alwaysOnTopBefore
                && navigationIndex.data(static_cast<int>(
                    ZzFluentUI::ZzActivityItemRole::Area))
                    .value<ZzFluentUI::ZzActivityArea>()
                    == ZzFluentUI::ZzActivityArea::RightPrimary
                && areaBefore == ZzFluentUI::ZzActivityArea::LeftPrimary;
            window.setCentralWidget(workspace);
            return postCommitMutationsSucceeded
                ? ZzCore::ZzResult<void>::success()
                : zzFailure(QStringLiteral(
                    "post-commit navigation integration left workspace unusable"));
        };

        createdWindow = zzApplication().createWindow();

        QVERIFY(createdWindow);
        QVERIFY(bodyPollutionEntered);
        QVERIFY(postCommitMutationsSucceeded);
        closeWindow(createdWindow.value());
        shell.reset();
    }

private:
    void closeWindow(ZzPureTools::ZzApplicationWindow *window)
    {
        QVERIFY(window != nullptr);
        window->close();
        ZZ_COMPARE_EVENTUALLY(zzApplication().windowCount(), baselineWindowCount_);
    }

    ZzWindowSetup currentSetup_;
    qsizetype baselineWindowCount_ = 0;
};

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return 1;
    }
    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzWorkspaceNavigationIntegrationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ZzWorkspaceNavigationIntegrationTest.moc"
