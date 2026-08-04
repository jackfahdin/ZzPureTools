#pragma once

#include <cstdint>

namespace ZzWindowKit {

/**
 * @brief 描述窗口原生材质使用的颜色模式。
 */
enum class ZzWindowColorScheme : std::uint8_t
{
    /** @brief 跟随当前系统颜色模式。 */
    System,
    /** @brief 使用浅色模式。 */
    Light,
    /** @brief 使用深色模式。 */
    Dark
};

} // namespace ZzWindowKit
