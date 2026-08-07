#include "ZzExampleWindowShellPrivate.h"

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtGui/QAction>
#include <QtGui/QIcon>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QToolButton>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>

#include <ZzLog/ZzLog.h>

#include "ZzExampleActivityModel.h"
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

/** @brief 将路由表中的 UTF-8 常量转换为 Qt 字符串。 */
[[nodiscard]] QString zzFromUtf8(std::string_view text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

/** @brief 创建统一图标、工具提示和可访问名称的工具栏命令。 */
[[nodiscard]] QAction *zzAddCommand(
    QToolBar *toolBar,
    QStyle::StandardPixmap icon,
    const QString &text)
{
    auto *action = toolBar->addAction(
        toolBar->style()->standardIcon(icon), text);
    action->setToolTip(text);
    action->setStatusTip(text);
    return action;
}

} // namespace

ZzExampleWindowShellPrivate::ZzExampleWindowShellPrivate(
    ZzExampleWindowShell *shell,
    ZzPureTools::ZzApplicationWindow *applicationWindow,
    std::shared_ptr<ZzExampleApplicationContext> applicationContext,
    ZzPureTools::ZzPureApplication *pureApplication,
    bool enableCloseGuard)
    : q_ptr(shell)
    , window(applicationWindow)
    , context(std::move(applicationContext))
    , application(pureApplication)
    , closeGuardEnabled(enableCloseGuard)
{
}

ZzCore::ZzResult<void> ZzExampleWindowShellPrivate::initialize()
{
    if (q_ptr == nullptr || window == nullptr || !context
        || application == nullptr) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("window shell requires window, context and application")));
    }
    navigation = window->navigationController();
    auto *theme = application->themeController();
    if (navigation == nullptr || theme == nullptr
        || window->titleBar() == nullptr) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("window shell requires initialized navigation and theme")));
    }

    window->setMinimumSize(860, 560);
    window->resize(1280, 800);
    if (closeGuardEnabled) {
        window->installEventFilter(q_ptr);
    }

    auto *commandBar = new QToolBar(
        QCoreApplication::translate("ZzPureToolsExample", "窗口命令"), window);
    commandBar->setObjectName(QStringLiteral("zzExampleCommandBar"));
    commandBar->setMovable(false);
    commandBar->setFloatable(false);
    commandBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    window->addToolBar(Qt::TopToolBarArea, commandBar);

    backAction = zzAddCommand(
        commandBar, QStyle::SP_ArrowBack, QCoreApplication::translate("ZzPureToolsExample", "返回"));
    backAction->setObjectName(QStringLiteral("zzExampleBackAction"));
    forwardAction = zzAddCommand(
        commandBar, QStyle::SP_ArrowForward, QCoreApplication::translate("ZzPureToolsExample", "前进"));
    forwardAction->setObjectName(QStringLiteral("zzExampleForwardAction"));
    commandBar->addSeparator();

    searchEdit = new QLineEdit(commandBar);
    searchEdit->setObjectName(QStringLiteral("zzExamplePageSearch"));
    searchEdit->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "页面搜索"));
    searchEdit->setPlaceholderText(QCoreApplication::translate("ZzPureToolsExample", "搜索页面"));
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setMinimumWidth(180);
    searchEdit->setMaximumWidth(320);
    commandBar->addWidget(searchEdit);
    commandBar->addSeparator();

    auto *themeAction = zzAddCommand(
        commandBar, QStyle::SP_DesktopIcon, QCoreApplication::translate("ZzPureToolsExample", "切换主题"));
    themeAction->setObjectName(QStringLiteral("zzExampleThemeAction"));
    auto *newWindowAction = zzAddCommand(
        commandBar, QStyle::SP_FileIcon, QCoreApplication::translate("ZzPureToolsExample", "新建窗口"));
    newWindowAction->setObjectName(QStringLiteral("zzExampleNewWindowAction"));

    auto *windowMenuButton = new QToolButton(commandBar);
    windowMenuButton->setObjectName(
        QStringLiteral("zzExampleWindowMenuButton"));
    windowMenuButton->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "窗口菜单"));
    windowMenuButton->setToolTip(QCoreApplication::translate("ZzPureToolsExample", "窗口菜单"));
    windowMenuButton->setIcon(
        commandBar->style()->standardIcon(QStyle::SP_TitleBarMenuButton));
    windowMenuButton->setPopupMode(QToolButton::InstantPopup);
    auto *windowMenu = new QMenu(windowMenuButton);
    auto *minimizeAction = windowMenu->addAction(
        commandBar->style()->standardIcon(QStyle::SP_TitleBarMinButton),
        QCoreApplication::translate("ZzPureToolsExample", "最小化"));
    auto *maximizeAction = windowMenu->addAction(
        commandBar->style()->standardIcon(QStyle::SP_TitleBarMaxButton),
        QCoreApplication::translate("ZzPureToolsExample", "最大化或还原"));
    auto *closeAction = windowMenu->addAction(
        commandBar->style()->standardIcon(QStyle::SP_TitleBarCloseButton),
        QCoreApplication::translate("ZzPureToolsExample", "关闭"));
    windowMenuButton->setMenu(windowMenu);
    commandBar->addWidget(windowMenuButton);

    activityDock = new QDockWidget(
        QCoreApplication::translate("ZzPureToolsExample", "活动与更新"), window);
    activityDock->setObjectName(QStringLiteral("zzExampleActivityDock"));
    activityDock->setAllowedAreas(
        Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    auto *activityTabs = new QTabWidget(activityDock);
    activityTabs->setObjectName(QStringLiteral("zzExampleActivityTabs"));
    activityState = new QListView(activityTabs);
    activityState->setObjectName(QStringLiteral("zzExampleActivityList"));
    activityState->setAccessibleName(QCoreApplication::translate("ZzPureToolsExample", "应用活动"));
    activityState->setModel(&context->activityModel());
    activityState->setItemDelegate(
        new ZzFluentUI::ZzFluentItemDelegate(activityState));
    activityState->setUniformItemSizes(true);
    activityState->setSelectionMode(QAbstractItemView::SingleSelection);
    QScrollBar *const activityScrollBar =
        activityState->verticalScrollBar();
    QObject::connect(
        activityScrollBar,
        &QScrollBar::valueChanged,
        q_ptr,
        [this, activityScrollBar](int value) {
            if (!activityTailScrollPending) {
                activityFollowsTail = value >= activityScrollBar->maximum();
            }
        });
    QObject::connect(
        &context->activityModel(),
        &QAbstractItemModel::rowsInserted,
        q_ptr,
        [this](const QModelIndex &parent, int, int) {
            if (parent.isValid() || !activityFollowsTail
                || activityTailScrollPending) {
                return;
            }
            activityTailScrollPending = true;
            QTimer::singleShot(0, q_ptr, [this] {
                if (activityState != nullptr && activityFollowsTail) {
                    activityState->scrollToBottom();
                }
                activityTailScrollPending = false;
            });
        });
    auto *updateState = new QLabel(
        QCoreApplication::translate("ZzPureToolsExample", "组件状态已就绪"), activityTabs);
    updateState->setAlignment(Qt::AlignCenter);
    activityTabs->addTab(activityState, QCoreApplication::translate("ZzPureToolsExample", "活动"));
    activityTabs->addTab(updateState, QCoreApplication::translate("ZzPureToolsExample", "更新"));
    activityDock->setWidget(activityTabs);
    window->addDockWidget(Qt::RightDockWidgetArea, activityDock);
    QObject::connect(
        activityDock,
        &QDockWidget::visibilityChanged,
        q_ptr,
        &ZzExampleWindowShell::activityDockVisibilityChanged);

    statusBar = new QStatusBar(window);
    statusBar->setObjectName(QStringLiteral("zzExampleStatusBar"));
    routeLabel = new QLabel(statusBar);
    routeLabel->setObjectName(QStringLiteral("zzExampleRouteStatus"));
    auto *taskLabel = new QLabel(QCoreApplication::translate("ZzPureToolsExample", "任务：就绪"), statusBar);
    taskLabel->setObjectName(QStringLiteral("zzExampleTaskStatus"));
    auto *platformLabel = new QLabel(context->platformName(), statusBar);
    platformLabel->setObjectName(QStringLiteral("zzExamplePlatformStatus"));
    statusBar->addWidget(routeLabel, 1);
    statusBar->addPermanentWidget(taskLabel);
    statusBar->addPermanentWidget(platformLabel);
    window->setStatusBar(statusBar);
    recordActivity(QCoreApplication::translate("ZzPureToolsExample", "窗口已完成装配"));

    QObject::connect(
        backAction, &QAction::triggered, q_ptr, [this] {
            auto result = navigation->goBack();
            if (!result) {
                reportFailure(result.error());
            }
        });
    QObject::connect(
        forwardAction, &QAction::triggered, q_ptr, [this] {
            auto result = navigation->goForward();
            if (!result) {
                reportFailure(result.error());
            }
        });
    QObject::connect(
        navigation,
        &ZzPureTools::ZzNavigationController::historyStateChanged,
        q_ptr,
        [this](bool canGoBack, bool canGoForward) {
            syncHistoryActions(canGoBack, canGoForward);
        });
    QObject::connect(
        navigation,
        &ZzPureTools::ZzNavigationController::currentRouteChanged,
        q_ptr,
        [this](const ZzPureTools::ZzRouteId &routeId) {
            routeLabel->setText(
                QCoreApplication::translate("ZzPureToolsExample", "路由：%1").arg(routeId.value()));
            recordActivity(
                QCoreApplication::translate("ZzPureToolsExample", "已导航到 %1").arg(routeId.value()));
        });
    QObject::connect(
        searchEdit, &QLineEdit::returnPressed, q_ptr,
        [this] { navigateFromSearch(); });
    QObject::connect(
        themeAction, &QAction::triggered, q_ptr,
        [this] { cycleTheme(); });
    QObject::connect(
        newWindowAction, &QAction::triggered, q_ptr, [this] {
            auto result = application->createWindow();
            if (!result) {
                reportFailure(result.error());
            }
        });
    QObject::connect(
        minimizeAction, &QAction::triggered, window,
        &QWidget::showMinimized);
    QObject::connect(
        maximizeAction, &QAction::triggered, q_ptr, [this] {
            if (window->isMaximized()) {
                window->showNormal();
            } else {
                window->showMaximized();
            }
        });
    QObject::connect(
        closeAction, &QAction::triggered, window, &QWidget::close);

    syncHistoryActions(
        navigation->canGoBack(), navigation->canGoForward());
    routeLabel->setText(QCoreApplication::translate("ZzPureToolsExample", "路由：准备中"));

    QTimer::singleShot(0, q_ptr, [this] {
        const QString title = QStringLiteral("ZzPureToolsExample");
        window->setWindowTitle(title);
        window->titleBar()->setTitle(title);
    });
    return ZzCore::ZzResult<void>::success();
}

void ZzExampleWindowShellPrivate::navigateFromSearch()
{
    const QString query = searchEdit->text().trimmed();
    if (query.isEmpty()) {
        return;
    }

    for (const auto &route : ZzExampleRouteCatalog::routes()) {
        const QString routeId = zzFromUtf8(route.routeId);
        const QString title = zzFromUtf8(route.title);
        if (!routeId.contains(query, Qt::CaseInsensitive)
            && !title.contains(query, Qt::CaseInsensitive)) {
            continue;
        }
        auto result = navigation->navigate(
            ZzPureTools::ZzRouteId(routeId));
        if (!result) {
            reportFailure(result.error());
        } else {
            searchEdit->clear();
        }
        return;
    }
    statusBar->showMessage(QCoreApplication::translate("ZzPureToolsExample", "未找到匹配页面"), 2500);
}

void ZzExampleWindowShellPrivate::cycleTheme()
{
    auto *theme = application->themeController();
    switch (theme->mode()) {
    case ZzFluentUI::ZzThemeMode::System:
        theme->setMode(ZzFluentUI::ZzThemeMode::Light);
        break;
    case ZzFluentUI::ZzThemeMode::Light:
        theme->setMode(ZzFluentUI::ZzThemeMode::Dark);
        break;
    case ZzFluentUI::ZzThemeMode::Dark:
        theme->setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        break;
    case ZzFluentUI::ZzThemeMode::HighContrast:
        theme->setMode(ZzFluentUI::ZzThemeMode::System);
        break;
    }
    statusBar->showMessage(QCoreApplication::translate("ZzPureToolsExample", "主题已切换"), 1800);
}

void ZzExampleWindowShellPrivate::reportFailure(
    const ZzCore::ZzError &error)
{
    statusBar->showMessage(QCoreApplication::translate("ZzPureToolsExample", "操作失败"), 3000);
    qWarning().noquote()
        << "ZzPureToolsExample operation failed:"
        << error.technicalMessage()
        << error.context();
}

void ZzExampleWindowShellPrivate::recordActivity(const QString &text)
{
    const QString normalized = text.simplified();
    if (normalized.isEmpty()) {
        return;
    }
    const QString display = QStringLiteral("%1  %2")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
             normalized);
    context->activityModel().append(display);
    ZzLog::writeText(
        ZzLog::ZzLogLevel::Info,
        normalized.toUtf8().toStdString());
}

void ZzExampleWindowShellPrivate::syncHistoryActions(
    bool canGoBack,
    bool canGoForward) noexcept
{
    backAction->setEnabled(canGoBack);
    forwardAction->setEnabled(canGoForward);
}

bool ZzExampleWindowShellPrivate::isActivityDockVisible() const noexcept
{
    return activityDock != nullptr && !activityDock->isHidden();
}

void ZzExampleWindowShellPrivate::setActivityDockVisible(bool visible)
{
    if (activityDock != nullptr) {
        activityDock->setVisible(visible);
    }
}

bool ZzExampleWindowShellPrivate::filterWindowEvent(
    QObject *watched,
    QEvent *event)
{
    if (watched != window || event == nullptr
        || event->type() != QEvent::Close) {
        return false;
    }
    if (closeGuardActive) {
        event->ignore();
        return true;
    }

    closeGuardActive = true;
    auto *dialog = new QMessageBox(
        QMessageBox::Question,
        QCoreApplication::translate("ZzPureToolsExample", "关闭窗口"),
        QCoreApplication::translate("ZzPureToolsExample", "请选择当前窗口的关闭方式。"),
        QMessageBox::NoButton,
        window);
    QAbstractButton *cancelButton = dialog->addButton(
        QCoreApplication::translate("ZzPureToolsExample", "取消"), QMessageBox::RejectRole);
    QAbstractButton *minimizeButton = dialog->addButton(
        QCoreApplication::translate("ZzPureToolsExample", "最小化"), QMessageBox::ActionRole);
    QAbstractButton *closeButton = dialog->addButton(
        QCoreApplication::translate("ZzPureToolsExample", "关闭"), QMessageBox::AcceptRole);
    dialog->setDefaultButton(
        qobject_cast<QPushButton *>(cancelButton));
    dialog->exec();
    QAbstractButton *clicked = dialog->clickedButton();
    const bool closeRequested = clicked == closeButton;
    const bool minimizeRequested = clicked == minimizeButton;
    delete dialog;
    closeGuardActive = false;

    if (closeRequested) {
        recordActivity(QCoreApplication::translate("ZzPureToolsExample", "窗口关闭已确认"));
        return false;
    }
    event->ignore();
    if (minimizeRequested) {
        window->showMinimized();
        recordActivity(QCoreApplication::translate("ZzPureToolsExample", "窗口关闭已转换为最小化"));
    } else {
        recordActivity(QCoreApplication::translate("ZzPureToolsExample", "窗口关闭已取消"));
    }
    return true;
}

} // namespace ZzExample
