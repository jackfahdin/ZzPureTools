#include <ZzFluentUI/ZzFluentTitleBar.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QActionEvent>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolButton>

#include "private/ZzFluentTitleBarPrivate.h"

namespace ZzFluentUI {

namespace {

constexpr int zzTitleBarHeight = 32;

} // namespace

ZzFluentTitleBar::ZzFluentTitleBar(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzFluentTitleBarPrivate>(this))
{
    d_ptr->menuBar->installEventFilter(this);
    setFixedHeight(zzTitleBarHeight);
}

ZzFluentTitleBar::~ZzFluentTitleBar()
{
    d_ptr->menuBar->removeEventFilter(this);
}

QString ZzFluentTitleBar::title() const
{
    return d_ptr->title;
}

void ZzFluentTitleBar::setTitle(QString title)
{
    if (d_ptr->title == title) {
        return;
    }
    d_ptr->title = std::move(title);
    d_ptr->updateLayout();
    Q_EMIT titleChanged(d_ptr->title);
}

void ZzFluentTitleBar::setWindowIcon(const QIcon &icon)
{
    d_ptr->windowIcon = icon;
    d_ptr->refreshPresentation();
}

QMenuBar *ZzFluentTitleBar::menuBar() const noexcept
{
    return d_ptr->menuBar;
}

ZzTitleBarMenuDisplayMode
ZzFluentTitleBar::menuDisplayMode() const noexcept
{
    return d_ptr->menuDisplayMode;
}

void ZzFluentTitleBar::setMenuDisplayMode(
    ZzTitleBarMenuDisplayMode mode)
{
    if (d_ptr->menuDisplayMode == mode) {
        return;
    }
    d_ptr->menuDisplayMode = mode;
    d_ptr->updateLayout();
    Q_EMIT menuDisplayModeChanged(mode);
}

bool ZzFluentTitleBar::isMenuCollapseEnabled() const noexcept
{
    return d_ptr->menuCollapseEnabled;
}

void ZzFluentTitleBar::setMenuCollapseEnabled(bool enabled)
{
    if (d_ptr->menuCollapseEnabled == enabled) {
        return;
    }
    d_ptr->menuCollapseEnabled = enabled;
    d_ptr->updateLayout();
    Q_EMIT menuCollapseEnabledChanged(enabled);
}

int ZzFluentTitleBar::minimumExpandedWidth() const noexcept
{
    return d_ptr->minimumExpandedWidth();
}

ZzThemeMode ZzFluentTitleBar::themeMode() const noexcept
{
    return d_ptr->themeMode;
}

void ZzFluentTitleBar::setThemeMode(ZzThemeMode mode)
{
    if (d_ptr->themeMode == mode) {
        return;
    }
    d_ptr->themeMode = mode;
    d_ptr->refreshPresentation();
    Q_EMIT themeModeChanged(mode);
}

ZzTitleBarThemeInteractionMode
ZzFluentTitleBar::themeInteractionMode() const noexcept
{
    return d_ptr->themeInteractionMode;
}

void ZzFluentTitleBar::setThemeInteractionMode(
    ZzTitleBarThemeInteractionMode mode)
{
    if (d_ptr->themeInteractionMode == mode) {
        return;
    }
    d_ptr->themeInteractionMode = mode;
    d_ptr->refreshPresentation();
    Q_EMIT themeInteractionModeChanged(mode);
}

QMenu *ZzFluentTitleBar::themeMenu() const noexcept
{
    return d_ptr->themeMenu;
}

bool ZzFluentTitleBar::isAlwaysOnTop() const noexcept
{
    return d_ptr->alwaysOnTop;
}

void ZzFluentTitleBar::setAlwaysOnTop(bool alwaysOnTop)
{
    if (d_ptr->alwaysOnTop == alwaysOnTop) {
        return;
    }
    d_ptr->alwaysOnTop = alwaysOnTop;
    d_ptr->refreshPresentation();
    Q_EMIT alwaysOnTopChanged(alwaysOnTop);
}

void ZzFluentTitleBar::setMaximized(bool maximized)
{
    if (d_ptr->maximized == maximized) {
        return;
    }
    d_ptr->maximized = maximized;
    d_ptr->refreshPresentation();
}

void ZzFluentTitleBar::setSystemButtonsVisible(bool visible)
{
    setWindowButtonsVisible(visible, visible, visible);
}

void ZzFluentTitleBar::setWindowButtonsVisible(
    bool minimizeVisible,
    bool maximizeVisible,
    bool closeVisible)
{
    if (d_ptr->minimizeButtonVisible == minimizeVisible
        && d_ptr->maximizeButtonVisible == maximizeVisible
        && d_ptr->closeButtonVisible == closeVisible) {
        return;
    }
    d_ptr->minimizeButtonVisible = minimizeVisible;
    d_ptr->maximizeButtonVisible = maximizeVisible;
    d_ptr->closeButtonVisible = closeVisible;
    d_ptr->refreshPresentation();
}

void ZzFluentTitleBar::setCommandButtonsVisible(bool visible)
{
    if (d_ptr->commandButtonsVisible == visible) {
        return;
    }
    d_ptr->commandButtonsVisible = visible;
    d_ptr->refreshPresentation();
    Q_EMIT commandButtonsVisibleChanged(visible);
}

bool ZzFluentTitleBar::areCommandButtonsVisible() const noexcept
{
    return d_ptr->commandButtonsVisible;
}

QWidget *ZzFluentTitleBar::windowIconWidget() const noexcept
{
    return d_ptr->iconLabel;
}

QWidget *ZzFluentTitleBar::minimizeButton() const noexcept
{
    return d_ptr->minimizeButton;
}

QWidget *ZzFluentTitleBar::maximizeButton() const noexcept
{
    return d_ptr->maximizeButton;
}

QWidget *ZzFluentTitleBar::closeButton() const noexcept
{
    return d_ptr->closeButton;
}

QList<QWidget *> ZzFluentTitleBar::interactiveWidgets() const
{
    return {
        d_ptr->minimizeButton,
        d_ptr->maximizeButton,
        d_ptr->closeButton};
}

QList<QWidget *> ZzFluentTitleBar::hitTestVisibleWidgets() const
{
    return {
        d_ptr->menuBar,
        d_ptr->compactMenuButton,
        d_ptr->themeButton,
        d_ptr->alwaysOnTopButton};
}

void ZzFluentTitleBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event != nullptr
        && (event->type() == QEvent::LanguageChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::PaletteChange
            || event->type() == QEvent::DevicePixelRatioChange
            || event->type() == QEvent::LayoutDirectionChange)) {
        d_ptr->refreshPresentation();
        if (event->type() == QEvent::LanguageChange) {
            d_ptr->rebuildCompactMenu();
        }
    }
}

bool ZzFluentTitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d_ptr->menuBar && event != nullptr
        && (event->type() == QEvent::ActionAdded
            || event->type() == QEvent::ActionRemoved)) {
        d_ptr->handleMenuActionEvent(static_cast<QActionEvent *>(event));
    }
    return QWidget::eventFilter(watched, event);
}

void ZzFluentTitleBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    d_ptr->updateLayout();
}

bool ZzFluentTitleBar::event(QEvent *event)
{
    const bool handled = QWidget::event(event);
    if (event != nullptr && event->type() == QEvent::ParentChange) {
        d_ptr->updateLayout();
    }
    return handled;
}

} // namespace ZzFluentUI
