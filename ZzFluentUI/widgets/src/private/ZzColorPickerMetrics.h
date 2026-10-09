#pragma once

#include <QtCore/qglobal.h>

namespace ZzFluentUI {
/** @brief 对齐参考颜色选择器的逻辑像素尺寸，缓存另乘 DPR。 */
inline constexpr int ZzColorPickerPadding = 12;
inline constexpr int ZzColorPickerSpacing = 10;
inline constexpr int ZzColorPickerTabsHeight = 36;
inline constexpr int ZzColorPickerPreviewHeight = 28;
inline constexpr int ZzColorPickerCompactPreviewHeight = 52;
inline constexpr int ZzColorPickerChannelWidth = 62;
inline constexpr int ZzColorPickerCompactChannelWidth = 84;
inline constexpr int ZzColorPickerSliderHitExtent = 24;
inline constexpr int ZzColorPickerSpectrumMinimum = 80;
inline constexpr qreal ZzColorPickerCornerRadius = 4;
} // namespace ZzFluentUI
