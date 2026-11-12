#pragma once

#include <type_traits>

#include <QtCore/QPoint>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>

// New Qt SDKs use floating-point drag positions; older supported SDKs only
// accept QPoint. Detect the constructor so development Qt kits work as well.
template<typename Event>
auto zzTestDragPosition(QPoint position)
{
    if constexpr (std::is_constructible_v<Event, QPointF, Qt::DropActions,
                      const QMimeData *, Qt::MouseButtons, Qt::KeyboardModifiers>) {
        return QPointF(position);
    } else {
        return position;
    }
}
