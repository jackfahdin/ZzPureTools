#pragma once

#include <cstdint>

#include <QtCore/QFlags>

namespace ZzWindowKit {

/**
 * @brief 描述当前窗口后端保守声明的平台能力。
 */
enum class ZzWindowCapability : std::uint32_t
{
    /** @brief 不声明任何原生能力。 */
    None = 0,
    /** @brief 可请求显示原生系统菜单。 */
    SystemMenu = 1U << 0U,
    /** @brief 可请求模糊背景。 */
    Blur = 1U << 1U,
    /** @brief 可请求 Acrylic 背景。 */
    Acrylic = 1U << 2U,
    /** @brief 可请求 Mica 背景。 */
    Mica = 1U << 3U,
    /** @brief 可请求 Mica Alt 背景。 */
    MicaAlt = 1U << 4U,
    /** @brief 可使用原生系统按钮区域。 */
    NativeSystemButtons = 1U << 5U,
    /** @brief 可参与 Windows Snap Layout。 */
    SnapLayout = 1U << 6U
};

/** @brief 可组合的窗口能力集合。 */
Q_DECLARE_FLAGS(ZzWindowCapabilities, ZzWindowCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(ZzWindowCapabilities)

} // namespace ZzWindowKit
