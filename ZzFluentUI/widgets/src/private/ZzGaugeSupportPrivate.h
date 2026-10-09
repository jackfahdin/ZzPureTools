#pragma once

#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {

/** @brief 高对比度主题下刻线和刻度文字保持完整的不透明度。 */
inline bool zzGaugeHighContrast(const QWidget *widget)
{
    const auto *style = qobject_cast<const ZzFluentStyle *>(widget->style());
    return style && style->themeSnapshot()->mode() == ZzThemeMode::HighContrast;
}

/** @brief 为不透明徽标选择对比度更高的黑色或白色文字。 */
inline QColor zzGaugeContrastingText(const QColor &background)
{
    const auto linear = [](qreal channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const qreal luminance = 0.2126 * linear(background.redF()) + 0.7152 * linear(background.greenF()) +
                            0.0722 * linear(background.blueF());
    return luminance > 0.179 ? QColorConstants::Black : QColorConstants::White;
}

/** @brief 有限动画遵守可见、启用状态以及样式的减少动态效果偏好。 */
inline bool zzGaugeMotionAllowed(const QWidget *widget)
{
    return widget->isVisible() && widget->isEnabled() &&
           widget->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, widget) != 0;
}

/** @brief 先缩放再相减，避免合法浮点量程在差值运算时溢出。 */
inline qreal zzGaugeFraction(qreal value, qreal minimum, qreal maximum)
{
    if (maximum <= minimum)
        return 0.0;
    const qreal scale = std::max({std::abs(minimum), std::abs(maximum), qreal(1.0)});
    return std::clamp((value / scale - minimum / scale) / (maximum / scale - minimum / scale), qreal(0.0),
                      qreal(1.0));
}

} // namespace ZzFluentUI
