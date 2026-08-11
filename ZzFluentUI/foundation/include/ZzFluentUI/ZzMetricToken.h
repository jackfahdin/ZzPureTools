#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识与设备无关的逻辑像素尺寸。 */
enum class ZzMetricToken : std::uint16_t
{
    CornerRadiusSmall,
    CornerRadiusMedium,
    StrokeThin,
    FocusStrokeWidth,
    ControlHeight,
    HorizontalPadding,
    VerticalPadding,
    IconSmall,
    IconMedium,
    OverlayPadding,
    DialogMinWidth,
    DialogMaxWidth,
    BadgeMinDiameter,
    TeachingTipTargetGap,
    TeachingTipMaxWidth,
    SelectionIndicatorThickness,
    SelectionIndicatorExtent,
    DrawerDefaultWidth,
    Count
};

} // namespace ZzFluentUI
