#include "ZzRollerPrivate.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include <QtGui/QFontMetrics>
#include <QtWidgets/QSizePolicy>

#include <ZzFluentUI/ZzRoller.h>

namespace ZzFluentUI {

ZzRollerPrivate::ZzRollerPrivate(ZzRoller *q)
    : q_ptr(q)
{
    QObject::connect(
        q_ptr,
        qOverload<int>(&QSpinBox::valueChanged),
        q_ptr,
        [this](int index) {
            Q_EMIT q_ptr->currentIndexChanged(index);
            notifyCurrentTextIfNeeded();
            q_ptr->update();
        });
}

bool ZzRollerPrivate::isValidIndex(int index) const noexcept
{
    return index >= 0 && index < items.size();
}

int ZzRollerPrivate::steppedIndex(int start, int steps) const noexcept
{
    const int count = static_cast<int>(items.size());
    if (count == 0 || !isValidIndex(start)) {
        return -1;
    }

    const std::int64_t candidate = static_cast<std::int64_t>(start)
        + static_cast<std::int64_t>(steps);
    if (!q_ptr->wrapping()) {
        return static_cast<int>(std::clamp<std::int64_t>(
            candidate,
            0,
            static_cast<std::int64_t>(count - 1)));
    }

    const std::int64_t modulus = count;
    return static_cast<int>(((candidate % modulus) + modulus) % modulus);
}

bool ZzRollerPrivate::applyUserStep(int steps)
{
    return applyUserIndex(steppedIndex(q_ptr->value(), steps));
}

bool ZzRollerPrivate::applyUserIndex(int index)
{
    if (!isValidIndex(index) || index == q_ptr->value()) {
        return false;
    }
    q_ptr->setValue(index);
    return true;
}

void ZzRollerPrivate::refreshTextWidth()
{
    const QFontMetrics metrics(q_ptr->font());
    int width = 0;
    for (const QString &item : std::as_const(items)) {
        width = std::max(width, metrics.horizontalAdvance(item));
    }
    if (longestTextWidth == width) {
        return;
    }
    longestTextWidth = width;
    q_ptr->updateGeometry();
}

void ZzRollerPrivate::notifyCurrentTextIfNeeded()
{
    const QString current = q_ptr->currentText();
    if (lastCurrentText == current) {
        return;
    }
    lastCurrentText = current;
    Q_EMIT q_ptr->currentTextChanged(current);
}

int ZzRollerPrivate::rowOffsetAt(int y) const noexcept
{
    if (y < 0 || y >= q_ptr->height() || itemHeight <= 0) {
        return std::numeric_limits<int>::max();
    }
    const int row = std::clamp(
        y / itemHeight,
        0,
        visibleItemCount - 1);
    return row - (visibleItemCount / 2);
}

} // namespace ZzFluentUI
