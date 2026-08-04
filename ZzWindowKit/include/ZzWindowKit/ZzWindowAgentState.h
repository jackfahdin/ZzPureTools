#pragma once

#include <cstdint>

namespace ZzWindowKit {

/**
 * @brief 描述无边框窗口代理的生命周期状态。
 */
enum class ZzWindowAgentState : std::uint8_t
{
    /** @brief 尚未绑定窗口。 */
    Detached,
    /** @brief 已绑定窗口但尚未配置标题栏。 */
    Attached,
    /** @brief 已绑定窗口并成功应用完整标题栏配置。 */
    Configured,
    /** @brief 已绑定的窗口已经销毁。 */
    Invalidated,
    /** @brief 后端绑定或标题栏配置失败。 */
    Failed
};

} // namespace ZzWindowKit
