#pragma once

#include <cstdint>

namespace ZzPureTools {

/** @brief 描述页面离开活动状态后的实例保留策略。 */
enum class ZzPageLifetimePolicy : std::uint8_t
{
    /** @brief 页面首次创建后保留到宿主销毁。 */
    Persistent,
    /** @brief 页面离开活动状态时立即销毁。 */
    WhileActive,
    /** @brief 页面离开后进入容量受限的可重建缓存。 */
    Recreatable
};

} // namespace ZzPureTools
