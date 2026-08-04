#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识使用平台字体族构造的排版角色。 */
enum class ZzTypographyToken : std::uint16_t
{
    Caption,
    Body,
    BodyStrong,
    Subtitle,
    Title,
    Count
};

} // namespace ZzFluentUI
