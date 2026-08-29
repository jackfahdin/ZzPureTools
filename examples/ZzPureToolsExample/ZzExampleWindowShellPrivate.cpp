#include "ZzExampleWindowShellPrivate.h"

#include <array>
#include <utility>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QTime>
#include <QtGui/QAction>
#include <QtGui/QKeySequence>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzCommandPalette.h>
#include <ZzFluentUI/ZzBottomPane.h>
#include <ZzFluentUI/ZzCommandBar.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzTabWidget.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzNavigationController.h>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzPureTools/ZzRouteId.h>
#include <ZzPureTools/ZzWorkspaceActivityId.h>
#include <ZzPureTools/ZzWorkspacePanelId.h>
#include <ZzPureTools/ZzWorkspaceShell.h>
#include <ZzPureTools/ZzWorkspaceTitleMode.h>
#include <ZzLog/ZzLog.h>

#include "ZzExampleActivityModel.h"
#include "ZzExampleApplicationContext.h"
#include "ZzExampleRouteCatalog.h"
#include "ZzExampleSettingsWindow.h"
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

    auto created = ZzPureTools::ZzWorkspaceShell::create(
        window, window->titleBar());
    if (!created) {
        return ZzCore::ZzResult<void>::failure(created.error());
    }
    workspace = std::move(created).value();
    workspace->setApplicationTitle(QStringLiteral("ZzPureToolsExample"));
    workspace->setTitleMode(
        ZzPureTools::ZzWorkspaceTitleMode::CurrentTabAndApplication);

    sessions = std::make_unique<ZzExampleSessionModel>(q_ptr);
    workspace->commandPalette()->setModel(sessions->commandModel());

    backAction = new QAction(q_ptr);
    backAction->setObjectName(QStringLiteral("zzExampleBackAction"));
    backAction->setShortcut(QKeySequence::Back);
    forwardAction = new QAction(q_ptr);
    forwardAction->setObjectName(QStringLiteral("zzExampleForwardAction"));
    forwardAction->setShortcut(QKeySequence::Forward);
    settingsAction = new QAction(q_ptr);
    settingsAction->setObjectName(QStringLiteral("zzExampleSettingsAction"));
    settingsAction->setText(QCoreApplication::translate(
        "ZzPureToolsExample", "设置"));
    auto *themeAction = new QAction(q_ptr);
    themeAction->setObjectName(QStringLiteral("zzExampleThemeAction"));
    themeAction->setText(QCoreApplication::translate(
        "ZzPureToolsExampleTitleBar", "切换主题"));
    auto *newWindowAction = new QAction(q_ptr);
    newWindowAction->setObjectName(QStringLiteral("zzExampleNewWindowAction"));
    newWindowAction->setText(QCoreApplication::translate(
        "ZzPureToolsExampleTitleBar", "新建窗口"));
    auto *openPaletteAction = new QAction(q_ptr);
    openPaletteAction->setText(QCoreApplication::translate(
        "ZzPureToolsExample", "命令面板"));
    openPaletteAction->setShortcut(
        QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    auto *newTerminalAction = new QAction(q_ptr);
    newTerminalAction->setText(QCoreApplication::translate(
        "ZzPureToolsExample", "新建终端"));
    newTerminalAction->setShortcut(QKeySequence::AddTab);
    auto *closeTerminalAction = new QAction(q_ptr);
    closeTerminalAction->setText(QCoreApplication::translate(
        "ZzPureToolsExampleTitleBar", "关闭终端"));
    closeTerminalAction->setShortcut(QKeySequence::Close);
    backAction->setText(QCoreApplication::translate(
        "ZzPureToolsExampleTitleBar", "后退"));
    forwardAction->setText(QCoreApplication::translate(
        "ZzPureToolsExampleTitleBar", "前进"));
    window->addActions({backAction, forwardAction, settingsAction, themeAction,
        newWindowAction, openPaletteAction, newTerminalAction,
        closeTerminalAction});

    auto *const titleBar = window->titleBar();
    auto *const theme = application->themeController();
    if (titleBar == nullptr || theme == nullptr) {
        return zzInvalidState(
            QStringLiteral("example window title bar is unavailable"));
    }
    QMenuBar *const titleMenuBar = titleBar->menuBar();
    if (titleMenuBar == nullptr) {
        return zzInvalidState(
            QStringLiteral("example title bar menu is unavailable"));
    }
    QMenu *const fileMenu = titleMenuBar->addMenu(
        QCoreApplication::translate("ZzPureToolsExampleTitleBar", "文件"));
    QMenu *const navigationMenu = titleMenuBar->addMenu(
        QCoreApplication::translate("ZzPureToolsExampleTitleBar", "导航"));
    QMenu *const viewMenu = titleMenuBar->addMenu(
        QCoreApplication::translate("ZzPureToolsExampleTitleBar", "视图"));
    if (fileMenu == nullptr || navigationMenu == nullptr || viewMenu == nullptr) {
        return zzInvalidState(
            QStringLiteral("example title bar menu creation failed"));
    }
    fileMenu->addAction(newTerminalAction);
    fileMenu->addAction(closeTerminalAction);
    fileMenu->addSeparator();
    fileMenu->addAction(newWindowAction);
    navigationMenu->addAction(backAction);
    navigationMenu->addAction(forwardAction);
    navigationMenu->addSeparator();
    navigationMenu->addAction(openPaletteAction);
    viewMenu->addAction(settingsAction);
    viewMenu->addAction(themeAction);
    titleBar->setThemeMode(theme->mode());
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::themeModeRequested,
        q_ptr,
        [this](ZzFluentUI::ZzThemeMode mode) {
            if (application != nullptr
                && application->themeController() != nullptr) {
                application->themeController()->setMode(mode);
            }
        });
    QObject::connect(
        theme,
        &ZzFluentUI::ZzThemeController::snapshotChanged,
        q_ptr,
        [titleBar, theme] {
            if (titleBar != nullptr && theme != nullptr) {
                titleBar->setThemeMode(theme->mode());
            }
        });
    searchEdit = workspace->commandPalette()->searchEdit();
    searchEdit->setObjectName(QStringLiteral("zzExamplePageSearch"));
    searchEdit->setAccessibleName(QCoreApplication::translate(
        "ZzPureToolsExample", "命令搜索"));

    auto side = workspace->registerSidePanelFactory(
        zzPanelId("sessions"),
        QCoreApplication::translate("ZzPureToolsExample", "会话"),
        ZzFluentUI::ZzIconDescriptor::fromSvgResource(
            QStringLiteral(
                ":/ZzPureToolsExample/workspace-icons/Sessions.svg")),
        ZzFluentUI::ZzActivityArea::LeftPrimary,
        [sessionModel = sessions.get()] {
            return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                ZzExampleWorkspaceContent::createSessionPanel(sessionModel));
        });
    if (!side) {
        return side;
    }

    auto files = workspace->registerSidePanelFactory(
        zzPanelId("files"),
        QCoreApplication::translate("ZzPureToolsExample", "文件"),
        ZzFluentUI::ZzIconDescriptor::fromSvgResource(
            QStringLiteral(
                ":/ZzPureToolsExample/workspace-icons/Files.svg")),
        ZzFluentUI::ZzActivityArea::LeftPrimary, [] {
            return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                ZzExampleWorkspaceContent::createSftpPanel());
        });
    if (!files) {
        return files;
    }

    auto integrated = workspace->integrateApplicationNavigation(
        zzPanelId("components"),
        QCoreApplication::translate("ZzPureToolsExample", "组件"),
        ZzFluentUI::ZzIconDescriptor::fromFontIcon(
            ZzFluentUI::ZzFontIcon::PuzzlePiece),
        ZzFluentUI::ZzActivityArea::LeftPrimary,
        QCoreApplication::translate("ZzPureToolsExample", "组件示例"));
    if (!integrated) {
        return integrated;
    }
    window->setCentralWidget(workspace->workspaceWidget());

    auto settings = workspace->registerFixedActivityAction(
        ZzPureTools::ZzWorkspaceActivityId(QStringLiteral("settings")),
        QCoreApplication::translate("ZzPureToolsExample", "设置"),
        ZzFluentUI::ZzIconDescriptor::fromFontIcon(
            ZzFluentUI::ZzFontIcon::Gear),
        ZzFluentUI::ZzActivityArea::LeftSecondary,
        settingsAction);
    if (!settings) {
        return settings;
    }

    auto properties = workspace->registerSidePanelFactory(
        zzPanelId("properties"),
        QCoreApplication::translate("ZzPureToolsExample", "属性"),
        ZzFluentUI::ZzIconDescriptor::fromSvgResource(
            QStringLiteral(
                ":/ZzPureToolsExample/workspace-icons/Properties.svg")),
        ZzFluentUI::ZzActivityArea::RightPrimary, [] {
            return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                ZzExampleWorkspaceContent::createPropertiesPanel());
        });
    if (!properties) {
        return properties;
    }

    auto tasks = workspace->registerSidePanelFactory(
        zzPanelId("tasks"),
        QCoreApplication::translate("ZzPureToolsExample", "任务"),
        ZzFluentUI::ZzIconDescriptor::fromSvgResource(
            QStringLiteral(
                ":/ZzPureToolsExample/workspace-icons/Tasks.svg")),
        ZzFluentUI::ZzActivityArea::RightPrimary, [] {
            return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
                ZzExampleWorkspaceContent::createTasksPanel());
        });
    if (!tasks) {
        return tasks;
    }

    struct ZzBottomRegistration final
    {
        ZzPureTools::ZzWorkspacePanelId id;
        QString title;
        std::unique_ptr<QWidget> content;
    };
    auto terminalPanel = std::make_unique<QWidget>();
    terminalPanel->setObjectName(QStringLiteral("zzExampleTerminalPanel"));
    auto *terminalLayout = new QVBoxLayout(terminalPanel.get());
    terminalLayout->setContentsMargins(6, 4, 6, 4);
    terminalLayout->addWidget(
        ZzExampleWorkspaceContent::createTerminalPage(
            QStringLiteral("dev-local"))
            .release());

    auto outputPanel = std::make_unique<QWidget>();
    outputPanel->setObjectName(QStringLiteral("zzExampleOutputPanel"));
    auto *outputLayout = new QVBoxLayout(outputPanel.get());
    outputLayout->setContentsMargins(6, 4, 6, 4);
    auto *outputCommandBar = new ZzFluentUI::ZzCommandBar(outputPanel.get());
    outputCommandBar->setObjectName(QStringLiteral("zzExampleOutputCommandBar"));
    outputCommandBar->addPrimaryAction(newTerminalAction);
    outputCommandBar->addSecondaryAction(openPaletteAction);
    outputLayout->addWidget(outputCommandBar);
    outputLayout->addWidget(
        ZzExampleWorkspaceContent::createOutputPanel(
            &context->activityModel())
            .release());

    std::array<ZzBottomRegistration, 3> bottomPanels{{
        {zzPanelId("terminal"),
         QCoreApplication::translate("ZzPureToolsExample", "终端"),
         std::move(terminalPanel)},
        {zzPanelId("problems"),
         QCoreApplication::translate("ZzPureToolsExample", "问题"),
         ZzExampleWorkspaceContent::createProblemsPanel()},
        {zzPanelId("activity-log"),
         QCoreApplication::translate("ZzPureToolsExample", "输出"),
         std::move(outputPanel)},
    }};
    for (ZzBottomRegistration &panel : bottomPanels) {
        auto result = workspace->registerBottomPanel(
            panel.id, panel.title, {}, panel.content.get());
        if (!result) {
            return result;
        }
        [[maybe_unused]] QWidget *const adoptedBottomPanel =
            panel.content.release();
    }

    const int navigationIndex =
        workspace->tabWidget()->indexOf(window->pageHost());
    createTerminalTab();
    workspace->tabWidget()->setCurrentIndex(navigationIndex);

    QObject::connect(
        workspace->bottomPane(), &ZzFluentUI::ZzBottomPane::collapsedChanged,
        q_ptr, [this](bool collapsed) {
            Q_EMIT q_ptr->activityDockVisibilityChanged(!collapsed);
        });

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
    QObject::connect(settingsAction, &QAction::triggered,
        q_ptr, [this] {
            if (settingsWindow.isNull()) {
                auto settingsCreated = ZzExampleSettingsWindow::create(
                    window, context, application, q_ptr);
                if (!settingsCreated) {
                    reportFailure(settingsCreated.error());
                    return;
                }
                ZzExampleSettingsWindow *const identity =
                    settingsCreated.value();
                settingsWindow = identity;
                QObject::connect(
                    identity, &QObject::destroyed, q_ptr,
                    [this, identity] {
                        if (settingsWindow.data() == identity) {
                            settingsWindow.clear();
                        }
                    });
            }
            settingsWindow->show();
            settingsWindow->raise();
            settingsWindow->activateWindow();
        });
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
        if (auto result = workspace->showPanel(zzPanelId("files")); !result) {
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
    case ZzExampleCommandId::ShowSettings:
        settingsAction->trigger();
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
    return workspace != nullptr && workspace->bottomPane() != nullptr
        && !workspace->bottomPane()->isCollapsed();
}

void ZzExampleWindowShellPrivate::setActivityDockVisible(bool visible)
{
    if (workspace == nullptr) {
        return;
    }
    auto result = workspace->showPanel(zzPanelId("activity-log"), visible);
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
