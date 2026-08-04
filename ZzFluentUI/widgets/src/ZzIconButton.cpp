#include <ZzFluentUI/ZzIconButton.h>

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

void ZzIconButton::setIconDescriptor(
    const ZzIconDescriptor &descriptor)
{
    d_ptr->descriptor = descriptor;
    d_ptr->hasDescriptor = true;
    d_ptr->refreshIcon();
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
