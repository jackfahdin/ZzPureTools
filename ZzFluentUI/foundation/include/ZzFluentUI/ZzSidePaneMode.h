#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 定义侧边面板一次显示单页或同时堆叠多页的布局模式。 */
enum class ZzSidePaneMode : std::uint8_t
{
    Single,
    Stacked
};

} // namespace ZzFluentUI
