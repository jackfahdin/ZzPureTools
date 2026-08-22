#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 定义滚动位置标记的主题语义。 */
enum class ZzScrollMarkerKind : std::uint8_t
{
    Information,
    Success,
    Warning,
    Error,
    Bookmark,
    SearchMatch,
    Custom
};

} // namespace ZzFluentUI
