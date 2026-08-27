#include "ZzWorkspaceNavigationIntegrationTransactionPrivate.h"

#include <utility>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QPointer>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzActivityBar.h>
#include <ZzFluentUI/ZzNavigationPane.h>
#include <ZzFluentUI/ZzPanelStack.h>
#include <ZzFluentUI/ZzSidePane.h>
#include <ZzFluentUI/ZzSplitWorkspace.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzNavigationModel.h>
#include <ZzPureTools/ZzPageHost.h>

#include "ZzApplicationWindowPrivate.h"
#include "ZzWorkspaceShellPrivate.h"

namespace ZzPureTools {

namespace {

template<typename ZzValue>
[[nodiscard]] ZzCore::ZzResult<ZzValue> zzIntegrationFailure(
    ZzCore::ZzErrorCode code,
    QString message,
    const ZzWorkspacePanelId &panelId = {})
{
    return ZzCore::ZzResult<ZzValue>::failure(ZzCore::ZzError(
        code, std::move(message), panelId.value()));
}

[[nodiscard]] bool zzIsSideArea(
    ZzFluentUI::ZzActivityArea area) noexcept
{
    switch (area) {
    case ZzFluentUI::ZzActivityArea::LeftPrimary:
    case ZzFluentUI::ZzActivityArea::LeftSecondary:
    case ZzFluentUI::ZzActivityArea::RightPrimary:
    case ZzFluentUI::ZzActivityArea::RightSecondary:
        return true;
    }
    return false;
}

[[nodiscard]] bool zzIsLeftArea(
    ZzFluentUI::ZzActivityArea area) noexcept
{
    return area == ZzFluentUI::ZzActivityArea::LeftPrimary
        || area == ZzFluentUI::ZzActivityArea::LeftSecondary;
}

[[nodiscard]] bool zzIsDirectLayoutChild(
    QHBoxLayout *layout,
    QWidget *widget,
    int index) noexcept
{
    return layout != nullptr && widget != nullptr && index >= 0
        && index < layout->count() && layout->itemAt(index) != nullptr
        && layout->itemAt(index)->widget() == widget;
}

struct ZzSideStateSnapshot final
{
    QPointer<ZzFluentUI::ZzSidePane> pane;
    QPointer<ZzFluentUI::ZzPanelStack> stack;
    QPointer<ZzFluentUI::ZzActivityBar> bar;
    QList<QPointer<QWidget>> panels;
    QList<QPointer<QWidget>> visible;
    QPointer<QWidget> current;
    QList<int> sizes;
    QPersistentModelIndex activityCurrent;
    QList<QPersistentModelIndex> activityActive;
    ZzWorkspacePanelId currentPanel;
    bool expanded = false;
    bool collapsed = false;
    bool paneHidden = false;
    bool barHidden = false;
};

[[nodiscard]] ZzSideStateSnapshot zzCaptureSideState(
    ZzFluentUI::ZzSidePane *pane,
    ZzFluentUI::ZzActivityBar *bar,
    const ZzWorkspacePanelId &currentPanel,
    bool expanded)
{
    ZzSideStateSnapshot snapshot;
    snapshot.pane = pane;
    snapshot.stack = pane != nullptr ? pane->panelStack() : nullptr;
    snapshot.bar = bar;
    snapshot.currentPanel = currentPanel;
    snapshot.expanded = expanded;
    if (snapshot.pane != nullptr && snapshot.stack != nullptr) {
        for (QWidget *const panel : snapshot.stack->panels()) {
            snapshot.panels.append(panel);
        }
        for (QWidget *const panel : snapshot.pane->visibleWidgets()) {
            snapshot.visible.append(panel);
        }
        snapshot.current = snapshot.pane->currentWidget();
        snapshot.sizes = snapshot.stack->panelSizes();
        snapshot.collapsed = snapshot.pane->isCollapsed();
        snapshot.paneHidden = snapshot.pane->isHidden();
    }
    if (snapshot.bar != nullptr) {
        snapshot.activityCurrent = snapshot.bar->currentSourceIndex();
        for (const QModelIndex &index : snapshot.bar->activeSourceIndexes()) {
            snapshot.activityActive.append(QPersistentModelIndex(index));
        }
        snapshot.barHidden = snapshot.bar->isHidden();
    }
    return snapshot;
}

} // namespace

ZzCore::ZzResult<void>
ZzWorkspaceNavigationIntegrationTransactionPrivate::execute(
    ZzWorkspaceShellPrivate &shell,
    const ZzWorkspacePanelId &panelId,
    const QString &panelTitle,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    const QString &centralTabTitle)
{
    auto *const window = qobject_cast<ZzApplicationWindow *>(shell.host.data());
    if (window == nullptr || shell.q_ptr == nullptr
        || shell.workspaceRoot == nullptr || shell.splitWorkspace == nullptr
        || shell.activityModel == nullptr) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace host is not an application window"),
            panelId);
    }
    if (shell.transactionKind != ZzWorkspaceShellPrivate::ZzTransactionKind::None
        || shell.applicationNavigationIntegrated) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Application navigation integration is unavailable"),
            panelId);
    }
    const QString normalizedPanelTitle = panelTitle.trimmed();
    const QString normalizedTabTitle = centralTabTitle.trimmed();
    if (!panelId.isValid() || normalizedPanelTitle.isEmpty()
        || normalizedTabTitle.isEmpty() || !zzIsSideArea(area)) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("Invalid application navigation integration"),
            panelId);
    }
    ZzApplicationWindowPrivate *const application = window->d_ptr.get();
    if (application == nullptr || application->body == nullptr
        || application->body.data() != application->bodyIdentity
        || window->centralWidget() != application->bodyIdentity
        || application->body->parentWidget() != window
        || application->navigationPane == nullptr
        || application->host == nullptr || application->model == nullptr
        || application->controller == nullptr
        || application->navigationPane->model() != application->model.get()) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Application navigation surface is inconsistent"),
            panelId);
    }

    auto *const bodyLayout =
        qobject_cast<QHBoxLayout *>(application->body->layout());
    const int navigationIndex = bodyLayout != nullptr
        ? bodyLayout->indexOf(application->navigationPane) : -1;
    const int pageIndex = bodyLayout != nullptr
        ? bodyLayout->indexOf(application->host) : -1;
    if (navigationIndex < 0 || pageIndex < 0
        || application->navigationPane->parentWidget()
            != application->bodyIdentity
        || application->host->parentWidget() != application->bodyIdentity
        || !zzIsDirectLayoutChild(
            bodyLayout, application->navigationPane, navigationIndex)
        || !zzIsDirectLayoutChild(bodyLayout, application->host, pageIndex)) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Application navigation parents are inconsistent"),
            panelId);
    }

    ZzFluentUI::ZzTabWidget *const tabs = shell.splitWorkspace->tabWidget(
        shell.splitWorkspace->activeGroupId());
    ZzFluentUI::ZzSidePane *const pane = zzIsLeftArea(area)
        ? shell.leftSidePane.data() : shell.rightSidePane.data();
    if (tabs == nullptr || pane == nullptr || pane->panelStack() == nullptr) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace navigation targets are unavailable"),
            panelId);
    }

    const QPointer<ZzWorkspaceShell> shellGuard(shell.q_ptr);
    const QPointer<ZzApplicationWindow> windowGuard(window);
    const QPointer<QWidget> rootGuard(shell.workspaceRoot);
    const QPointer<ZzFluentUI::ZzSplitWorkspace> splitGuard(
        shell.splitWorkspace);
    const QPointer<QAbstractItemModel> activityModelGuard(shell.activityModel);
    const QPointer<QWidget> bodyGuard(application->body);
    QWidget *const bodyIdentity = application->bodyIdentity;
    const QPointer<QHBoxLayout> layoutGuard(bodyLayout);
    const QPointer<ZzFluentUI::ZzNavigationPane> navigationGuard(
        application->navigationPane);
    ZzFluentUI::ZzNavigationPane *const navigationIdentity =
        application->navigationPane;
    const QPointer<ZzPageHost> pageGuard(application->host);
    ZzPageHost *const pageIdentity = application->host;
    ZzNavigationModel *const modelIdentity = application->model.get();
    ZzNavigationController *const controllerIdentity =
        application->controller.get();
    const QPointer<ZzFluentUI::ZzTabWidget> tabsGuard(tabs);
    ZzFluentUI::ZzTabWidget *const tabsIdentity = tabs;
    const QPointer<ZzFluentUI::ZzSidePane> paneGuard(pane);
    const ZzSideStateSnapshot leftState = zzCaptureSideState(
        shell.leftSidePane, shell.leftActivityBar,
        shell.leftCurrentPanel, shell.leftPaneExpanded);
    const ZzSideStateSnapshot rightState = zzCaptureSideState(
        shell.rightSidePane, shell.rightActivityBar,
        shell.rightCurrentPanel, shell.rightPaneExpanded);
    const ZzRouteId routeBefore = controllerIdentity->currentRoute();
    const int currentTabBefore = tabs->currentIndex();
    const int tabCountBefore = tabs->count();
    bool navigationRegistered = false;
    bool pageAdded = false;
    bool centralTaken = false;
    bool committed = false;

    shell.transactionKind =
        ZzWorkspaceShellPrivate::ZzTransactionKind::NavigationIntegration;
    const auto shellAlive = [&] { return shellGuard != nullptr; };
    const auto applicationAlive = [&] {
        return windowGuard != nullptr && windowGuard == window
            && window->d_ptr.get() == application;
    };
    const auto surfacesIntact = [&] {
        return shellAlive() && applicationAlive()
            && rootGuard != nullptr && rootGuard == shell.workspaceRoot
            && splitGuard != nullptr && splitGuard == shell.splitWorkspace
            && activityModelGuard != nullptr
            && activityModelGuard == shell.activityModel
            && tabsGuard != nullptr && tabsGuard == tabsIdentity
            && paneGuard != nullptr && paneGuard == pane
            && paneGuard == (zzIsLeftArea(area)
                    ? shell.leftSidePane : shell.rightSidePane)
            && leftState.pane != nullptr
            && leftState.pane == shell.leftSidePane
            && leftState.stack != nullptr
            && leftState.stack == leftState.pane->panelStack()
            && leftState.bar != nullptr
            && leftState.bar == shell.leftActivityBar
            && rightState.pane != nullptr
            && rightState.pane == shell.rightSidePane
            && rightState.stack != nullptr
            && rightState.stack == rightState.pane->panelStack()
            && rightState.bar != nullptr
            && rightState.bar == shell.rightActivityBar
            && navigationGuard != nullptr
            && navigationGuard == navigationIdentity
            && pageGuard != nullptr && pageGuard == pageIdentity
            && window->navigationModel() == modelIdentity
            && window->navigationController() == controllerIdentity
            && window->navigationPane() == navigationIdentity
            && window->pageHost() == pageIdentity
            && navigationIdentity->model() == modelIdentity
            && controllerIdentity->currentRoute() == routeBefore;
    };
    const auto identitiesIntact = [&] {
        return surfacesIntact() && bodyGuard != nullptr
            && bodyGuard == bodyIdentity && application->body == bodyGuard
            && layoutGuard != nullptr && bodyGuard->layout() == layoutGuard;
    };
    const auto restoreRoute = [&] {
        if (!applicationAlive() || pageGuard == nullptr
            || controllerIdentity->currentRoute() == routeBefore) {
            return applicationAlive() && pageGuard != nullptr;
        }
        if (routeBefore.isValid()) {
            static_cast<void>(controllerIdentity->navigate(routeBefore));
        } else {
            pageGuard->deactivateCurrent();
        }
        return applicationAlive() && pageGuard != nullptr
            && controllerIdentity->currentRoute() == routeBefore;
    };
    const auto restoreSideState = [&](const ZzSideStateSnapshot &snapshot,
                                      bool left) {
        if (!shellAlive() || snapshot.pane == nullptr
            || snapshot.stack == nullptr || snapshot.bar == nullptr
            || snapshot.pane != (left ? shell.leftSidePane : shell.rightSidePane)
            || snapshot.bar
                != (left ? shell.leftActivityBar : shell.rightActivityBar)
            || snapshot.stack != snapshot.pane->panelStack()) {
            return false;
        }
        const auto alive = [&] {
            return shellAlive() && snapshot.pane != nullptr
                && snapshot.stack != nullptr && snapshot.bar != nullptr
                && snapshot.pane
                    == (left ? shell.leftSidePane : shell.rightSidePane)
                && snapshot.bar
                    == (left ? shell.leftActivityBar : shell.rightActivityBar)
                && snapshot.stack == snapshot.pane->panelStack();
        };
        const QList<QWidget *> actualPanels = snapshot.stack->panels();
        if (actualPanels.size() != snapshot.panels.size()) {
            return false;
        }
        for (qsizetype index = 0; index < snapshot.panels.size(); ++index) {
            if (snapshot.panels.at(index) == nullptr
                || actualPanels.at(index) != snapshot.panels.at(index)) {
                return false;
            }
        }
        for (const QPointer<QWidget> &panel : snapshot.panels) {
            const bool wasVisible = snapshot.visible.contains(panel);
            if (snapshot.pane->visibleWidgets().contains(panel) != wasVisible
                && (!snapshot.pane->setWidgetVisible(panel, wasVisible)
                    || !alive())) {
                return false;
            }
        }
        if (snapshot.current != nullptr
            && snapshot.pane->currentWidget() != snapshot.current
            && (!snapshot.pane->setCurrentWidget(snapshot.current)
                || !alive())) {
            return false;
        }
        if (snapshot.pane->currentWidget() != snapshot.current) {
            return false;
        }
        if (snapshot.stack->panelSizes() != snapshot.sizes
            && (!snapshot.stack->setPanelSizes(snapshot.sizes) || !alive())) {
            return false;
        }
        if (snapshot.pane->isCollapsed() != snapshot.collapsed) {
            snapshot.pane->setCollapsed(snapshot.collapsed);
            if (!alive()) {
                return false;
            }
        }
        if (snapshot.pane->isHidden() != snapshot.paneHidden) {
            snapshot.pane->setVisible(!snapshot.paneHidden);
            if (!alive()) {
                return false;
            }
        }
        const QModelIndex activityCurrent(snapshot.activityCurrent);
        if (snapshot.bar->currentSourceIndex() != activityCurrent) {
            snapshot.bar->setCurrentSourceIndex(activityCurrent);
            if (!alive()) {
                return false;
            }
        }
        QList<QModelIndex> activityActive;
        activityActive.reserve(snapshot.activityActive.size());
        for (const QPersistentModelIndex &index : snapshot.activityActive) {
            if (!index.isValid()) {
                return false;
            }
            activityActive.append(index);
        }
        if (snapshot.bar->activeSourceIndexes() != activityActive) {
            snapshot.bar->setActiveSourceIndexes(activityActive);
            if (!alive()) {
                return false;
            }
        }
        if (snapshot.bar->isHidden() != snapshot.barHidden) {
            snapshot.bar->setVisible(!snapshot.barHidden);
            if (!alive()) {
                return false;
            }
        }
        if (left) {
            shell.leftCurrentPanel = snapshot.currentPanel;
            shell.leftPaneExpanded = snapshot.expanded;
        } else {
            shell.rightCurrentPanel = snapshot.currentPanel;
            shell.rightPaneExpanded = snapshot.expanded;
        }
        return alive();
    };
    const auto rollback = [&] {
        if (committed || !shellAlive() || !applicationAlive()) {
            return false;
        }
        QObject::disconnect(shell.navigationTabPinnedConnection);
        QObject::disconnect(shell.navigationTabCloseConnection);
        shell.navigationTabPinnedConnection = {};
        shell.navigationTabCloseConnection = {};
        if (pageAdded && tabsGuard != nullptr && pageGuard != nullptr) {
            const int currentPageIndex = tabsGuard->indexOf(pageGuard);
            if (currentPageIndex >= 0) {
                tabsGuard->removeTab(currentPageIndex);
                if (!shellAlive() || !applicationAlive()
                    || tabsGuard == nullptr || pageGuard == nullptr) {
                    return false;
                }
                if (pageGuard->parent() != nullptr) {
                    pageGuard->setParent(nullptr);
                    if (!shellAlive() || !applicationAlive()
                        || pageGuard == nullptr) {
                        return false;
                    }
                }
            }
        }
        if (navigationRegistered && navigationGuard != nullptr) {
            auto taken = shell.takePanel(panelId, true);
            if (!shellAlive() || !applicationAlive()
                || navigationGuard == nullptr) {
                return false;
            }
            if (taken && taken.value() == navigationGuard) {
                navigationRegistered = false;
            }
        }
        if (!restoreSideState(leftState, true)
            || !restoreSideState(rightState, false)) {
            return false;
        }
        if (bodyGuard != nullptr && layoutGuard != nullptr) {
            const auto restoreWidget = [&](int index,
                                           const QPointer<QWidget> &widget,
                                           int stretch) {
                if (!shellAlive() || !applicationAlive()
                    || bodyGuard == nullptr || layoutGuard == nullptr
                    || widget == nullptr) {
                    return false;
                }
                if (widget->parentWidget() == bodyGuard
                    && layoutGuard->indexOf(widget) == index) {
                    return true;
                }
                if (widget->parent() != nullptr) {
                    widget->setParent(nullptr);
                    if (!shellAlive() || !applicationAlive()
                        || bodyGuard == nullptr || layoutGuard == nullptr
                        || widget == nullptr) {
                        return false;
                    }
                }
                layoutGuard->insertWidget(index, widget, stretch);
                return shellAlive() && applicationAlive()
                    && bodyGuard != nullptr && layoutGuard != nullptr
                    && widget != nullptr && widget->parentWidget() == bodyGuard
                    && layoutGuard->indexOf(widget) == index;
            };
            const bool restored = navigationIndex < pageIndex
                ? restoreWidget(navigationIndex, navigationGuard, 0)
                    && restoreWidget(pageIndex, pageGuard, 1)
                : restoreWidget(pageIndex, pageGuard, 1)
                    && restoreWidget(navigationIndex, navigationGuard, 0);
            if (!restored) {
                return false;
            }
        }
        if (centralTaken && bodyGuard != nullptr
            && windowGuard->centralWidget() == nullptr) {
            windowGuard->setCentralWidget(bodyGuard);
            if (!shellAlive() || !applicationAlive()
                || bodyGuard == nullptr || windowGuard->centralWidget() != bodyGuard) {
                return false;
            }
            centralTaken = false;
        }
        if (tabsGuard != nullptr && tabsGuard->count() == tabCountBefore
            && currentTabBefore >= -1
            && currentTabBefore < tabsGuard->count()) {
            tabsGuard->setCurrentIndex(currentTabBefore);
            if (!shellAlive() || !applicationAlive()
                || tabsGuard == nullptr) {
                return false;
            }
        }
        return restoreRoute();
    };
    const auto fail = [&](QString message) {
        const bool rollbackComplete = rollback();
        if (rollbackComplete && shellAlive()) {
            shell.transactionKind =
                ZzWorkspaceShellPrivate::ZzTransactionKind::None;
        }
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            std::move(message),
            panelId);
    };

    bodyLayout->removeWidget(navigationGuard);
    navigationGuard->setParent(nullptr);
    if (!identitiesIntact() || navigationGuard->parent() != nullptr) {
        return fail(QStringLiteral("Navigation pane detachment was interrupted"));
    }

    auto registered = shell.registerSidePanel(
        panelId,
        normalizedPanelTitle,
        std::move(icon),
        area,
        navigationGuard,
        true);
    if (!shellAlive()) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace was destroyed during navigation transfer"),
            panelId);
    }
    if (!registered) {
        return fail(QStringLiteral("Navigation side panel registration failed"));
    }
    navigationRegistered = true;
    if (!identitiesIntact()
        || !paneGuard->isAncestorOf(navigationGuard)) {
        return fail(QStringLiteral("Navigation side panel identity changed"));
    }

    bodyLayout->removeWidget(pageGuard);
    pageGuard->setParent(nullptr);
    if (!identitiesIntact() || pageGuard->parent() != nullptr) {
        return fail(QStringLiteral("Page host detachment was interrupted"));
    }

    const int integratedTabIndex = tabsGuard->addTab(
        pageGuard, normalizedTabTitle);
    pageAdded = integratedTabIndex >= 0;
    if (!pageAdded || !identitiesIntact()
        || tabsGuard == nullptr || tabsGuard != tabsIdentity
        || tabsGuard->indexOf(pageGuard) != integratedTabIndex
        || !splitGuard->isAncestorOf(pageGuard)) {
        return fail(QStringLiteral("Page host tab insertion was interrupted"));
    }
    tabsGuard->setTabPinned(integratedTabIndex, true);
    if (!shellAlive()) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace was destroyed while fixing the page tab"),
            panelId);
    }
    if (!identitiesIntact() || tabsGuard == nullptr || pageGuard == nullptr) {
        return fail(QStringLiteral("Page host fixed tab state was interrupted"));
    }
    const int fixedTabIndex = tabsGuard->indexOf(pageGuard);
    if (fixedTabIndex < 0 || !tabsGuard->isTabPinned(fixedTabIndex)) {
        return fail(QStringLiteral("Page host fixed tab state was interrupted"));
    }
    tabsGuard->setTabCloseEnabled(fixedTabIndex, false);
    if (!shellAlive()) {
        return zzIntegrationFailure<void>(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("Workspace was destroyed while fixing the page tab"),
            panelId);
    }
    if (!identitiesIntact() || tabsGuard == nullptr || pageGuard == nullptr
        || tabsGuard->indexOf(pageGuard) != fixedTabIndex
        || !tabsGuard->isTabPinned(fixedTabIndex)
        || tabsGuard->isTabCloseEnabled(fixedTabIndex)) {
        return fail(QStringLiteral("Page host fixed tab state was interrupted"));
    }

    QWidget *const takenBody = windowGuard->takeCentralWidget();
    centralTaken = takenBody == bodyIdentity;
    if (!centralTaken || !identitiesIntact() || bodyGuard == nullptr
        || bodyGuard != bodyIdentity || windowGuard->centralWidget() != nullptr
        || layoutGuard == nullptr || layoutGuard->count() != 0) {
        return fail(QStringLiteral("Application body release was interrupted"));
    }
    shell.navigationTabPinnedConnection = QObject::connect(
        tabsGuard,
        &ZzFluentUI::ZzTabWidget::tabPinnedChanged,
        shell.q_ptr,
        [fixedPage = pageGuard, fixedTabs = tabsGuard](int, bool pinned) {
            if (pinned || fixedPage == nullptr || fixedTabs == nullptr) {
                return;
            }
            const int index = fixedTabs->indexOf(fixedPage);
            if (index >= 0) {
                fixedTabs->setTabPinned(index, true);
            }
        });
    shell.navigationTabCloseConnection = QObject::connect(
        tabsGuard,
        &ZzFluentUI::ZzTabWidget::tabCloseEnabledChanged,
        shell.q_ptr,
        [fixedPage = pageGuard, fixedTabs = tabsGuard](int, bool enabled) {
            if (!enabled || fixedPage == nullptr || fixedTabs == nullptr) {
                return;
            }
            const int index = fixedTabs->indexOf(fixedPage);
            if (index >= 0) {
                fixedTabs->setTabCloseEnabled(index, false);
            }
        });
    if (!shell.navigationTabPinnedConnection
        || !shell.navigationTabCloseConnection || !identitiesIntact()
        || bodyGuard == nullptr || windowGuard->centralWidget() != nullptr) {
        return fail(QStringLiteral("Page host fixed tab guard was interrupted"));
    }

    shell.applicationNavigationIntegrated = true;
    committed = true;
    shell.transactionKind = ZzWorkspaceShellPrivate::ZzTransactionKind::None;
    bodyGuard->deleteLater();
    return ZzCore::ZzResult<void>::success();
}

} // namespace ZzPureTools
