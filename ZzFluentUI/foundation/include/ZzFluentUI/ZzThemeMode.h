#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 指定应用级主题来源。 */
enum class ZzThemeMode : std::uint8_t
{
    System,
    Light,
    Dark,
    HighContrast
};

} // namespace ZzFluentUI
