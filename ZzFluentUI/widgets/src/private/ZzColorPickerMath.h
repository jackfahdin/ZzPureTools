#pragma once

#include <QtGui/QColor>

namespace ZzFluentUI {
/** @brief 在 Qt 6 float HSV 边界显式转换有界逻辑分量。 */
inline QColor zzColorFromHsv(qreal hue, qreal saturation, qreal value, qreal alpha = 1)
{
    return QColor::fromHsvF(static_cast<float>(hue), static_cast<float>(saturation),
                           static_cast<float>(value), static_cast<float>(alpha));
}
} // namespace ZzFluentUI
