#include <ZzPureTools/ZzAuxiliaryWindowChrome.h>

#include <exception>
#include <QtCore/QThread>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzWindowKit/ZzWindowAgent.h>

#include "private/ZzAuxiliaryWindowChromePrivate.h"

namespace ZzPureTools {

ZzAuxiliaryWindowChrome::ZzAuxiliaryWindowChrome()
    : d_ptr(std::make_unique<ZzAuxiliaryWindowChromePrivate>())
{
}

ZzAuxiliaryWindowChrome::~ZzAuxiliaryWindowChrome()
{
    d_ptr->agent.reset();
    delete d_ptr->titleBar.data();
}

ZzCore::ZzResult<std::unique_ptr<ZzAuxiliaryWindowChrome>>
ZzAuxiliaryWindowChrome::create(
    QMainWindow *window, ZzFluentUI::ZzThemeController *theme)
{
    using Result = ZzCore::ZzResult<std::unique_ptr<ZzAuxiliaryWindowChrome>>;
    if (window == nullptr || theme == nullptr) {
        return Result::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("auxiliary chrome requires a window and theme controller")));
    }
    if (qApp == nullptr || QThread::currentThread() != qApp->thread()
        || window->thread() != qApp->thread() || theme->thread() != qApp->thread()) {
        return Result::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("auxiliary chrome must use the GUI thread")));
    }
    if (!window->isWindow() || window->isVisible() || window->menuWidget() != nullptr) {
        return Result::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("auxiliary chrome requires an empty hidden top-level window")));
    }
    try {
        auto chrome = std::unique_ptr<ZzAuxiliaryWindowChrome>(new ZzAuxiliaryWindowChrome);
        auto &state = *chrome->d_ptr;
        state.titleBar = new ZzFluentUI::ZzFluentTitleBar(window);
        auto *bar = state.titleBar.data();
        bar->setTitle(window->windowTitle());
        bar->setWindowIcon(window->windowIcon());
        bar->setThemeMode(theme->mode());
        bar->setCommandButtonsVisible(false);
        window->setMenuWidget(bar);
        state.agent = std::make_unique<ZzWindowKit::ZzWindowAgent>();
        const auto attached = state.agent->attach(window);
        if (!attached) {
            return Result::failure(attached.error());
        }
        const bool native = state.agent->capabilities().testFlag(
            ZzWindowKit::ZzWindowCapability::NativeSystemButtons);
        bar->setWindowButtonsVisible(false, false, !native);
        ZzWindowKit::ZzWindowChromeConfiguration configuration;
        configuration.titleBar = bar;
        configuration.windowIcon = bar->windowIconWidget();
        configuration.interactiveWidgets = bar->hitTestVisibleWidgets();
        if (!native) {
            configuration.closeButton = bar->closeButton();
        }
        const auto configured = state.agent->configureChrome(configuration);
        if (!configured) {
            return Result::failure(configured.error());
        }
        QObject::connect(window, &QObject::destroyed,
            chrome.get(), [state = chrome->d_ptr.get()] {
                // A hidden QWidget emits destroyed before native-surface teardown.
                // Release its hooks while their platform view is still alive.
                state->agent.reset();
            });
        QObject::connect(window, &QWidget::windowTitleChanged,
            bar, &ZzFluentUI::ZzFluentTitleBar::setTitle);
        QObject::connect(window, &QWidget::windowIconChanged,
            bar, &ZzFluentUI::ZzFluentTitleBar::setWindowIcon);
        QObject::connect(theme, &ZzFluentUI::ZzThemeController::snapshotChanged,
            bar, [bar, theme] { bar->setThemeMode(theme->mode()); });
        QObject::connect(bar, &ZzFluentUI::ZzFluentTitleBar::closeRequested,
            window, &QWidget::close);
        return Result::success(std::move(chrome));
    } catch (const std::exception &exception) {
        return Result::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to create auxiliary chrome"),
            QString::fromLocal8Bit(exception.what())));
    } catch (...) {
        return Result::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Unknown,
            QStringLiteral("failed to create auxiliary chrome")));
    }
}

ZzFluentUI::ZzFluentTitleBar *ZzAuxiliaryWindowChrome::titleBar() const noexcept
{
    return d_ptr->titleBar.data();
}

ZzWindowKit::ZzWindowAgent *ZzAuxiliaryWindowChrome::windowAgent() const noexcept
{
    return d_ptr->agent.get();
}

} // namespace ZzPureTools
