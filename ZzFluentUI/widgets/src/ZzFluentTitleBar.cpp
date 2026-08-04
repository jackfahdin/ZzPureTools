#include <ZzFluentUI/ZzFluentTitleBar.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>

#include "private/ZzFluentTitleBarPrivate.h"

namespace ZzFluentUI {

ZzFluentTitleBar::ZzFluentTitleBar(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzFluentTitleBarPrivate>(this))
{
    setFixedHeight(32);
}

ZzFluentTitleBar::~ZzFluentTitleBar() = default;

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
    d_ptr->refreshPresentation();
    Q_EMIT titleChanged(d_ptr->title);
}

void ZzFluentTitleBar::setWindowIcon(const QIcon &icon)
{
    d_ptr->windowIcon = icon;
    d_ptr->refreshPresentation();
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
    if (d_ptr->systemButtonsVisible == visible) {
        return;
    }
    d_ptr->systemButtonsVisible = visible;
    d_ptr->refreshPresentation();
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

void ZzFluentTitleBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event != nullptr
        && (event->type() == QEvent::LanguageChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::PaletteChange
            || event->type() == QEvent::DevicePixelRatioChange)) {
        d_ptr->refreshPresentation();
    }
}

} // namespace ZzFluentUI
