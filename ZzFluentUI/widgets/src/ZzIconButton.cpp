#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzControlAppearance.h>

#include <QtCore/QEvent>
#include <QtGui/QResizeEvent>

#include "private/ZzIconButtonPrivate.h"

namespace ZzFluentUI {

ZzIconButton::ZzIconButton(QWidget *parent)
    : QToolButton(parent)
    , d_ptr(std::make_unique<ZzIconButtonPrivate>(this))
{
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setFocusPolicy(Qt::StrongFocus);
}

ZzIconButton::~ZzIconButton() = default;

ZzButtonAppearance ZzIconButton::appearance() const
{
    return ZzControlAppearance::buttonAppearance(this);
}

void ZzIconButton::setAppearance(ZzButtonAppearance appearance)
{
    ZzControlAppearance::setButtonAppearance(this, appearance);
}

bool ZzIconButton::event(QEvent *event)
{
    const bool handled = QToolButton::event(event);
    if (d_ptr && event->type() == QEvent::DynamicPropertyChange
        && static_cast<const QDynamicPropertyChangeEvent *>(event)->propertyName() == "accent") {
        d_ptr->refreshIcon();
    }
    return handled;
}

void ZzIconButton::setIconDescriptor(
    const ZzIconDescriptor &descriptor)
{
    d_ptr->descriptor = descriptor;
    d_ptr->hasDescriptor = true;
    d_ptr->refreshIcon();
}

QColor ZzIconButton::iconColor() const noexcept
{
    return d_ptr->iconColor;
}

void ZzIconButton::setIconColor(QColor color)
{
    if (d_ptr->iconColor == color) {
        return;
    }
    d_ptr->iconColor = color;
    d_ptr->refreshIcon();
}

void ZzIconButton::resetIconColor()
{
    setIconColor({});
}

void ZzIconButton::changeEvent(QEvent *event)
{
    QToolButton::changeEvent(event);
    if (event != nullptr
        && (event->type() == QEvent::PaletteChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::EnabledChange
            || event->type() == QEvent::DevicePixelRatioChange
            || event->type() == QEvent::LayoutDirectionChange)) {
        d_ptr->refreshIcon();
    }
}

void ZzIconButton::resizeEvent(QResizeEvent *event)
{
    QToolButton::resizeEvent(event);
    d_ptr->refreshIcon();
}

} // namespace ZzFluentUI
