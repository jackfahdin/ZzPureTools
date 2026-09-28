#pragma once

#include <cstdint>

namespace ZzPureTools {

/** @brief 创建工作区时选择中央页面容器；创建后不改变页面所有权结构。 */
enum class ZzWorkspaceCenterMode : std::uint8_t
{
    /** @brief 默认无标签堆叠页面；应用使用导航或自己的控件切换页面。 */
    Stacked,
    /** @brief 显式启用内置多标签分屏工作区及标签迁移能力。 */
    Tabbed
};

} // namespace ZzPureTools
