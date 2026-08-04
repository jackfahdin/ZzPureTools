#pragma once

#include <cstdint>

namespace ZzWindowKit {

/**
 * @brief 描述窗口背景材质请求。
 */
enum class ZzWindowBackdrop : std::uint8_t
{
    /** @brief 关闭已知背景材质。 */
    None,
    /** @brief 请求平台模糊背景。 */
    Blur,
    /** @brief 请求 Windows Acrylic 背景。 */
    Acrylic,
    /** @brief 请求 Windows Mica 背景。 */
    Mica,
    /** @brief 请求 Windows Mica Alt 背景。 */
    MicaAlt,
    /** @brief 请求当前平台推荐的背景材质。 */
    Automatic
};

} // namespace ZzWindowKit
