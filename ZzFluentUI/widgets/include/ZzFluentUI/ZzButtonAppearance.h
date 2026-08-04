#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 描述按钮的视觉强调级别。 */
enum class ZzButtonAppearance : std::uint8_t
{
    Standard,
    Accent,
    Subtle
};

} // namespace ZzFluentUI
