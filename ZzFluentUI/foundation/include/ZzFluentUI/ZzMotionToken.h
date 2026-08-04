#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识非业务状态动画的标准时长。 */
enum class ZzMotionToken : std::uint16_t
{
    Fast,
    Normal,
    Slow,
    PageTransition,
    Count
};

} // namespace ZzFluentUI
