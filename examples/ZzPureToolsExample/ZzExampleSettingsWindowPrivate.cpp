#include "ZzExampleSettingsWindowPrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzThemeController.h>

#include <ZzPureTools/ZzApplicationWindow.h>
#include <ZzPureTools/ZzPureApplication.h>

#include <ZzWindowKit/ZzWindowAgent.h>
#include <ZzWindowKit/ZzWindowCapability.h>
#include <ZzWindowKit/ZzWindowChromeConfiguration.h>

#include "ZzExampleApplicationContext.h"
#include "ZzExampleSettingsWindow.h"
#include "ZzExampleSystemPage.h"
#include "ZzExampleSystemPageKind.h"
#include "ZzExampleSystemPresenter.h"
#include "ZzExampleSystemViewModel.h"
#include "ZzExampleWindowShell.h"

namespace ZzExample {

namespace {

[[nodiscard]] ZzCore::ZzResult<void> zzSettingsWindowFailure(
    ZzCore::ZzErrorCode code,
    QString message)
{
    return ZzCore::ZzResult<void>::failure(
        ZzCore::ZzError(code, std::move(message)));
}

} // namespace

ZzExampleSettingsWindowPrivate::ZzExampleSettingsWindowPrivate(
    ZzExampleSettingsWindow *window)
    : q_ptr(window)
{
    Q_ASSERT(q_ptr != nullptr);
    if (q_ptr == nullptr) {
        std::terminate();
    }
}

ZzExampleSettingsWindowPrivate::~ZzExampleSettingsWindowPrivate()
{
    presenter.reset();
    viewModel.reset();
    agent.reset();
}

ZzCore::ZzResult<void> ZzExampleSettingsWindowPrivate::initialize(
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell)
{
    auto *const applicationWindow = qobject_cast<
        ZzPureTools::ZzApplicationWindow *>(q_ptr->parentWidget());
    if (initialized) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("example settings window is already initialized"));
    }
    if (applicationWindow == nullptr || !context || application == nullptr
        || shell == nullptr || application->themeController() == nullptr) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("example settings window requires parent, context, application and shell"));
    }
    if (ZzExampleWindowShell::attachedTo(*applicationWindow) != shell) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("example settings window shell must belong to its parent window"));
    }
    if (QThread::currentThread() != q_ptr->thread()
        || applicationWindow->thread() != q_ptr->thread()
        || application->thread() != q_ptr->thread()
        || shell->thread() != q_ptr->thread()) {
        return zzSettingsWindowFailure(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("example settings window inputs must use the GUI thread"));
    }

    parentWindow = applicationWindow;
    theme = application->themeController();
    titleBar = new ZzFluentUI::ZzFluentTitleBar(q_ptr);
    q_ptr->setMenuWidget(titleBar);
    viewModel = std::make_unique<ZzExampleSystemViewModel>();
    page = new ZzExampleSystemPage(
        ZzExampleSystemPageKind::Settings,
        QCoreApplication::translate("ZzPureToolsExample", "设置"),
        viewModel.get(),
        q_ptr);
    q_ptr->setCentralWidget(page);
    presenter = std::make_unique<ZzExampleSystemPresenter>(
        ZzExampleSystemPageKind::Settings,
        page,
        viewModel.get(),
        std::move(context),
        application,
        applicationWindow,
        shell);
    q_ptr->setMinimumSize(640, 520);
    q_ptr->resize(760, 620);

    agent = std::make_unique<ZzWindowKit::ZzWindowAgent>();
    auto attached = agent->attach(q_ptr);
    if (!attached) {
        return attached;
    }
    const bool nativeSystemButtons = agent->capabilities().testFlag(
        ZzWindowKit::ZzWindowCapability::NativeSystemButtons);
    titleBar->setWindowButtonsVisible(
        false,
        false,
        !nativeSystemButtons);
    titleBar->setCommandButtonsVisible(false);
    ZzWindowKit::ZzWindowChromeConfiguration chrome;
    chrome.titleBar = titleBar;
    chrome.windowIcon = titleBar->windowIconWidget();
    chrome.interactiveWidgets = titleBar->hitTestVisibleWidgets();
    if (!nativeSystemButtons) {
        chrome.minimizeButton = titleBar->minimizeButton();
        chrome.maximizeButton = titleBar->maximizeButton();
        chrome.closeButton = titleBar->closeButton();
    }
    auto configured = agent->configureChrome(chrome);
    if (!configured) {
        return configured;
    }

    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::minimizeRequested,
        q_ptr,
        &QWidget::showMinimized);
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::maximizeRestoreRequested,
        q_ptr,
        [this] {
            if (q_ptr->isMaximized()) {
                q_ptr->showNormal();
            } else {
                q_ptr->showMaximized();
            }
        });
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::closeRequested,
        q_ptr,
        &QWidget::close);
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::alwaysOnTopRequested,
        q_ptr,
        [this](bool requested) {
            if (agent == nullptr) {
                return;
            }
            const auto result = agent->setAlwaysOnTop(requested);
            if (!result) {
                return;
            }
            titleBar->setAlwaysOnTop(q_ptr->windowFlags().testFlag(
                Qt::WindowStaysOnTopHint));
        });
    QObject::connect(
        titleBar,
        &ZzFluentUI::ZzFluentTitleBar::themeModeRequested,
        theme,
        &ZzFluentUI::ZzThemeController::setMode);
    QObject::connect(
        theme,
        &ZzFluentUI::ZzThemeController::snapshotChanged,
        q_ptr,
        [this] { syncTheme(); });

    refreshTranslations();
    syncWindowState();
    syncTheme();
    initialized = true;
    return ZzCore::ZzResult<void>::success();
}

void ZzExampleSettingsWindowPrivate::refreshTranslations()
{
    const QString title = QCoreApplication::translate(
        "ZzPureToolsExample", "设置");
    q_ptr->setWindowTitle(title);
    if (titleBar != nullptr) {
        titleBar->setTitle(title);
    }
}

void ZzExampleSettingsWindowPrivate::syncWindowState()
{
    if (titleBar != nullptr) {
        titleBar->setMaximized(q_ptr->isMaximized());
    }
}

void ZzExampleSettingsWindowPrivate::syncTheme()
{
    if (titleBar != nullptr && theme != nullptr) {
        titleBar->setThemeMode(theme->mode());
    }
}

} // namespace ZzExample
