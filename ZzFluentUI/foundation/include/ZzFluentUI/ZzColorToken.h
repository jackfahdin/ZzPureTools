#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识可在绘制热路径中 O(1) 读取的 Fluent 颜色。 */
enum class ZzColorToken : std::uint16_t
{
    TextPrimary,
    TextSecondary,
    ControlFill,
    ControlFillHover,
    ControlFillPressed,
    ControlFillDisabled,
    ControlStroke,
    Accent,
    AccentText,
    FocusStroke,
    Surface,
    SurfaceSecondary,
    Error,
    Count
};

} // namespace ZzFluentUI
