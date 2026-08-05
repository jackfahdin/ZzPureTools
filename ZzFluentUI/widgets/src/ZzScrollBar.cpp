#include <ZzFluentUI/ZzScrollBar.h>

#include <QtCore/QEvent>
#include <QtGui/QEnterEvent>
#include <QtGui/QHideEvent>

#include "private/ZzScrollBarPrivate.h"

namespace ZzFluentUI {

ZzScrollBar::ZzScrollBar(QWidget *parent)
    : QScrollBar(parent)
    , d_ptr(std::make_unique<ZzScrollBarPrivate>(this))
{
    setMouseTracking(true);
}

ZzScrollBar::ZzScrollBar(
    Qt::Orientation orientation,
    QWidget *parent)
    : QScrollBar(orientation, parent)
    , d_ptr(std::make_unique<ZzScrollBarPrivate>(this))
{
    setMouseTracking(true);
}

ZzScrollBar::~ZzScrollBar() = default;

void ZzScrollBar::enterEvent(QEnterEvent *event)
{
    QScrollBar::enterEvent(event);
    d_ptr->setExpanded(true);
}

void ZzScrollBar::leaveEvent(QEvent *event)
{
    QScrollBar::leaveEvent(event);
    d_ptr->setExpanded(false);
}

void ZzScrollBar::hideEvent(QHideEvent *event)
{
    d_ptr->finishImmediately(false);
    QScrollBar::hideEvent(event);
}

void ZzScrollBar::changeEvent(QEvent *event)
{
    QScrollBar::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    switch (event->type()) {
    case QEvent::EnabledChange:
    case QEvent::PaletteChange:
        d_ptr->finishImmediately(isEnabled() && underMouse());
        break;
    case QEvent::StyleChange:
        d_ptr->finishImmediately(isEnabled() && underMouse());
        updateGeometry();
        break;
    default:
        break;
    }
}

} // namespace ZzFluentUI
