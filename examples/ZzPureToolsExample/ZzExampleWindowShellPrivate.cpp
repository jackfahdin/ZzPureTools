#include "ZzExampleWindowShellPrivate.h"

#include <utility>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QTime>
#include <QtGui/QAction>
#include <QtGui/QKeySequence>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzDockPanel.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzFontIcon.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>
#include <ZzLog/ZzLog.h>

#include "ZzExampleActivityModel.h"
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleSessionModel.h"
#include "ZzExampleWindowShell.h"
#include "ZzExampleWorkspaceContent.h"

namespace ZzExample {

namespace {

[[nodiscard]] QString zzFromUtf8(std::string_view text)
{
    return QString::fromUtf8(
        text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] ZzPureTools::ZzWorkspacePanelId zzPanelId(
    const char *value)
{
    return ZzPureTools::ZzWorkspacePanelId(QString::fromLatin1(value));
}

[[nodiscard]] ZzCore::ZzResult<void> zzInvalidState(QString message)
{
    return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
        ZzCore::ZzErrorCode::InvalidState, std::move(message)));
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

ZzExampleWindowShellPrivate::~ZzExampleWindowShellPrivate() = default;

ZzCore::ZzResult<void> ZzExampleWindowShellPrivate::initialize()
{
    if (q_ptr == nullptr || window == nullptr || !context
        || application == nullptr) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("window shell requires window, context and application")));
    }
    navigation = window->navigationController();
    if (navigation == nullptr || application->themeController() == nullptr
        || window->titleBar() == nullptr) {
        return zzInvalidState(
            QStringLiteral("window shell requires initialized navigation and theme"));
    }

    window->setMinimumSize(860, 560);
    window->resize(1280, 800);
    if (closeGuardEnabled) {
        window->installEventFilter(q_ptr);
    }

    QWidget *const navigationContent = window->takeCentralWidget();
    if (navigationContent == nullptr) {
        return zzInvalidState(
            QStringLiteral("window shell requires existing navigation content"));
    }
    auto created = ZzPureTools::ZzWorkspaceShell::create(
        window, window->titleBar());
    if (!created) {
        window->setCentralWidget(navigationContent);
        return ZzCore::ZzResult<void>::failure(created.error());
    }
    workspace = std::move(created).value();
    window->setCentralWidget(workspace->workspaceWidget());
    workspace->setApplicationTitle(QStringLiteral("ZzPureToolsExample"));
    workspace->setTitleMode(
        ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);

    sessions = std::make_unique<ZzExampleSessionModel>(q_ptr);
    workspace->commandPalette()->setModel(sessions->commandModel());
    searchEdit = workspace->commandPalette()->searchEdit();
    searchEdit->setObjectName(QStringLiteral("zzExamplePageSearch"));
    searchEdit->setAccessibleName(QCoreApplication::translate(
        "ZzPureToolsExample", "命令搜索"));

    auto sessionPanel = ZzExampleWorkspaceContent::createSessionPanel(
        sessions.get());
    auto side = workspace->registerSidePanel(
        zzPanelId("sessions"),
        QCoreApplication::translate("ZzPureToolsExample", "会话"),
        ZzFluentUI::ZzIconDescriptor::fromFontIcon(
            ZzFluentUI::ZzFontIcon::Server),
        ZzFluentUI::ZzActivityArea::LeftPrimary,
        sessionPanel.get());
    if (!side) {
        return side;
    }
    sessionPanel.release();

    const int navigationIndex = workspace->tabWidget()->addTab(
        navigationContent,
        QCoreApplication::translate("ZzPureToolsExample", "组件示例"));
    workspace->tabWidget()->setTabPinned(navigationIndex, true);
    workspace->tabWidget()->setTabCloseEnabled(navigationIndex, false);
    createTerminalTab();
    workspace->tabWidget()->setCurrentIndex(navigationIndex);

    struct ZzDockRegistration final
    {
        ZzPureTools::ZzWorkspacePanelId id;
        QString title;
        Qt::DockWidgetArea area;
        std::unique_ptr<QWidget> content;
    };
    std::vector<ZzDockRegistration> docks;
    docks.emplace_back(ZzDockRegistration{
        zzPanelId("sftp"), QStringLiteral("SFTP"),
        Qt::LeftDockWidgetArea,
        ZzExampleWorkspaceContent::createSftpPanel()});
    docks.emplace_back(ZzDockRegistration{
        zzPanelId("activity-log"),
        QCoreApplication::translate("ZzPureToolsExample", "日志"),
        Qt::BottomDockWidgetArea,
        ZzExampleWorkspaceContent::createActivityLogPanel(
            &context->activityModel())});
    docks.emplace_back(ZzDockRegistration{
        zzPanelId("properties"),
        QCoreApplication::translate("ZzPureToolsExample", "属性"),
        Qt::RightDockWidgetArea,
        ZzExampleWorkspaceContent::createPropertiesPanel()});
    docks.emplace_back(ZzDockRegistration{
        zzPanelId("tasks"),
        QCoreApplication::translate("ZzPureToolsExample", "任务"),
        Qt::BottomDockWidgetArea,
        ZzExampleWorkspaceContent::createTasksPanel()});
    for (ZzDockRegistration &dock : docks) {
        auto result = workspace->registerDockPanel(
            dock.id, dock.title, {}, dock.area, dock.content.get());
        if (!result) {
            return result;
        }
        dock.content.release();
    }
    activityDock = window->findChild<ZzFluentUI::ZzDockPanel *>(
        QStringLiteral("zzWorkspaceDock:activity-log"));
    if (activityDock == nullptr) {
        return zzInvalidState(
            QStringLiteral("workspace activity log dock was not created"));
    }
    QObject::connect(
        activityDock, &ZzFluentUI::ZzDockPanel::visibilityChanged,
        q_ptr, &ZzExampleWindowShell::activityDockVisibilityChanged);

    backAction = new QAction(q_ptr);
    backAction->setObjectName(QStringLiteral("zzExampleBackAction"));
    backAction->setShortcut(QKeySequence::Back);
    forwardAction = new QAction(q_ptr);
    forwardAction->setObjectName(QStringLiteral("zzExampleForwardAction"));
    forwardAction->setShortcut(QKeySequence::Forward);
    auto *themeAction = new QAction(q_ptr);
    themeAction->setObjectName(QStringLiteral("zzExampleThemeAction"));
    auto *newWindowAction = new QAction(q_ptr);
    newWindowAction->setObjectName(QStringLiteral("zzExampleNewWindowAction"));
    auto *openPaletteAction = new QAction(q_ptr);
    openPaletteAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    auto *newTerminalAction = new QAction(q_ptr);
    newTerminalAction->setShortcut(QKeySequence::AddTab);
    auto *closeTerminalAction = new QAction(q_ptr);
    closeTerminalAction->setShortcut(QKeySequence::Close);
    window->addActions({backAction, forwardAction, themeAction,
        newWindowAction, openPaletteAction, newTerminalAction,
        closeTerminalAction});

    statusBar = new QStatusBar(window);
    statusBar->setObjectName(QStringLiteral("zzExampleStatusBar"));
    routeLabel = new QLabel(statusBar);
    routeLabel->setObjectName(QStringLiteral("zzExampleRouteStatus"));
    statusBar->addWidget(routeLabel, 1);
    statusBar->addPermanentWidget(new QLabel(
        QCoreApplication::translate("ZzPureToolsExample", "任务：就绪"),
        statusBar));
    statusBar->addPermanentWidget(
        new QLabel(context->platformName(), statusBar));
    window->setStatusBar(statusBar);

    QObject::connect(backAction, &QAction::triggered, q_ptr, [this] {
        auto result = navigation->goBack();
        if (!result) {
            reportFailure(result.error());
        }
    });
    QObject::connect(forwardAction, &QAction::triggered, q_ptr, [this] {
        auto result = navigation->goForward();
        if (!result) {
            reportFailure(result.error());
        }
    });
    QObject::connect(navigation,
        &ZzPureTools::ZzNavigationController::historyStateChanged,
        q_ptr, [this](bool back, bool forward) {
            syncHistoryActions(back, forward);
        });
    QObject::connect(navigation,
        &ZzPureTools::ZzNavigationController::currentRouteChanged,
        q_ptr, [this](const ZzPureTools::ZzRouteId &routeId) {
            routeLabel->setText(QCoreApplication::translate(
                "ZzPureToolsExample", "路由：%1").arg(routeId.value()));
            recordActivity(QCoreApplication::translate(
                "ZzPureToolsExample", "已导航到 %1")
                               .arg(routeId.value()));
        });
    QObject::connect(searchEdit, &QLineEdit::returnPressed,
        q_ptr, [this] { navigateFromSearch(); });
    QObject::connect(themeAction, &QAction::triggered,
        q_ptr, [this] { cycleTheme(); });
    QObject::connect(newWindowAction, &QAction::triggered,
        q_ptr, [this] {
            auto result = application->createWindow();
            if (!result) {
                reportFailure(result.error());
            }
        });
    QObject::connect(openPaletteAction, &QAction::triggered,
        workspace->commandPalette(),
        &ZzFluentUI::ZzCommandPalette::open);
    QObject::connect(newTerminalAction, &QAction::triggered,
        q_ptr, [this] { createTerminalTab(); });
    QObject::connect(closeTerminalAction, &QAction::triggered,
        q_ptr, [this] { closeCurrentTerminal(); });
    QObject::connect(workspace->commandPalette(),
        &ZzFluentUI::ZzCommandPalette::commandActivated,
        q_ptr, [this](const QModelIndex &index) {
            dispatchWorkspaceCommand(sessions->commandId(index));
        });
    QObject::connect(workspace->tabWidget(),
        &QTabWidget::tabCloseRequested, q_ptr, [this](int index) {
            if (workspace->tabWidget()->isTabCloseEnabled(index)) {
                QWidget *const page = workspace->tabWidget()->widget(index);
                workspace->tabWidget()->removeTab(index);
                delete page;
            }
        });
    QObject::connect(workspace->tabWidget(),
        &ZzFluentUI::ZzTabWidget::newTabRequested,
        q_ptr, [this] { createTerminalTab(); });

    syncHistoryActions(
        navigation->canGoBack(), navigation->canGoForward());
    routeLabel->setText(QCoreApplication::translate(
        "ZzPureToolsExample", "路由：准备中"));
    recordActivity(QCoreApplication::translate(
        "ZzPureToolsExample", "窗口已完成装配"));
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
        auto result = navigation->navigate(ZzPureTools::ZzRouteId(routeId));
        if (!result) {
            reportFailure(result.error());
        } else {
            searchEdit->clear();
        }
        return;
    }
    statusBar->showMessage(QCoreApplication::translate(
        "ZzPureToolsExample", "未找到匹配页面"), 2500);
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
    statusBar->showMessage(QCoreApplication::translate(
        "ZzPureToolsExample", "主题已切换"), 1800);
}

void ZzExampleWindowShellPrivate::dispatchWorkspaceCommand(
    ZzExampleCommandId command)
{
    switch (command) {
    case ZzExampleCommandId::NewTerminal:
        createTerminalTab();
        break;
    case ZzExampleCommandId::CloseTerminal:
        closeCurrentTerminal();
        break;
    case ZzExampleCommandId::ShowSftp:
        if (auto result = workspace->showPanel(zzPanelId("sftp")); !result) {
            reportFailure(result.error());
        }
        break;
    case ZzExampleCommandId::ShowActivityLog:
        if (auto result = workspace->showPanel(zzPanelId("activity-log"));
            !result) {
            reportFailure(result.error());
        }
        break;
    case ZzExampleCommandId::ShowProperties:
        if (auto result = workspace->showPanel(zzPanelId("properties"));
            !result) {
            reportFailure(result.error());
        }
        break;
    case ZzExampleCommandId::ShowTasks:
        if (auto result = workspace->showPanel(zzPanelId("tasks")); !result) {
            reportFailure(result.error());
        }
        break;
    }
}

void ZzExampleWindowShellPrivate::createTerminalTab()
{
    if (workspace == nullptr || workspace->tabWidget() == nullptr) {
        return;
    }
    ++terminalSequence;
    const QString title = QCoreApplication::translate(
        "ZzPureToolsExample", "终端 %1").arg(terminalSequence);
    auto terminal = ZzExampleWorkspaceContent::createTerminalPage(title);
    const int index = workspace->tabWidget()->addTab(
        terminal.release(), title);
    workspace->tabWidget()->setCurrentIndex(index);
    recordActivity(QCoreApplication::translate(
        "ZzPureToolsExample", "已创建 %1").arg(title));
}

void ZzExampleWindowShellPrivate::closeCurrentTerminal()
{
    if (workspace == nullptr || workspace->tabWidget() == nullptr) {
        return;
    }
    const int index = workspace->tabWidget()->currentIndex();
    if (!workspace->tabWidget()->isTabCloseEnabled(index)) {
        return;
    }
    QWidget *const page = workspace->tabWidget()->widget(index);
    workspace->tabWidget()->removeTab(index);
    delete page;
    recordActivity(QCoreApplication::translate(
        "ZzPureToolsExample", "已关闭当前终端"));
}

void ZzExampleWindowShellPrivate::reportFailure(const ZzCore::ZzError &error)
{
    if (statusBar != nullptr) {
        statusBar->showMessage(QCoreApplication::translate(
            "ZzPureToolsExample", "操作失败"), 3000);
    }
    qWarning().noquote() << "ZzPureToolsExample operation failed:"
                         << error.technicalMessage() << error.context();
}

void ZzExampleWindowShellPrivate::recordActivity(const QString &text)
{
    const QString normalized = text.simplified();
    if (normalized.isEmpty()) {
        return;
    }
    context->activityModel().append(QStringLiteral("%1  %2").arg(
        QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
        normalized));
    ZzLog::writeText(
        ZzLog::ZzLogLevel::Info, normalized.toUtf8().toStdString());
}

void ZzExampleWindowShellPrivate::syncHistoryActions(
    bool canGoBack, bool canGoForward) noexcept
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
    if (workspace == nullptr) {
        return;
    }
    auto result = workspace->showPanel(
        zzPanelId("activity-log"), visible);
    if (!result) {
        reportFailure(result.error());
    }
}

bool ZzExampleWindowShellPrivate::filterWindowEvent(
    QObject *watched, QEvent *event)
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
        QCoreApplication::translate(
            "ZzPureToolsExample", "请选择当前窗口的关闭方式。"),
        QMessageBox::NoButton, window);
    QAbstractButton *cancelButton = dialog->addButton(
        QCoreApplication::translate("ZzPureToolsExample", "取消"),
        QMessageBox::RejectRole);
    QAbstractButton *minimizeButton = dialog->addButton(
        QCoreApplication::translate("ZzPureToolsExample", "最小化"),
        QMessageBox::ActionRole);
    QAbstractButton *closeButton = dialog->addButton(
        QCoreApplication::translate("ZzPureToolsExample", "关闭"),
        QMessageBox::AcceptRole);
    dialog->setDefaultButton(qobject_cast<QPushButton *>(cancelButton));
    dialog->exec();
    QAbstractButton *clicked = dialog->clickedButton();
    const bool closeRequested = clicked == closeButton;
    const bool minimizeRequested = clicked == minimizeButton;
    delete dialog;
    closeGuardActive = false;

    if (closeRequested) {
        recordActivity(QCoreApplication::translate(
            "ZzPureToolsExample", "窗口关闭已确认"));
        return false;
    }
    event->ignore();
    if (minimizeRequested) {
        window->showMinimized();
        recordActivity(QCoreApplication::translate(
            "ZzPureToolsExample", "窗口关闭已转换为最小化"));
    } else {
        recordActivity(QCoreApplication::translate(
            "ZzPureToolsExample", "窗口关闭已取消"));
    }
    return true;
}

} // namespace ZzExample
