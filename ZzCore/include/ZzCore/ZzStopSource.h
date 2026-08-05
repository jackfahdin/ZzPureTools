#pragma once

#include <atomic>
#include <memory>

#include <ZzCore/ZzStopToken.h>

namespace ZzCore {

/**
 * @brief 拥有一个可由多个线程观察的协作取消状态。
 *
 * Source 可以低成本复制；所有副本和派生 Token 共享同一原子状态。停止请求只从
 * false 单向切换为 true，不提供重置操作。
 */
class ZzStopSource final
{
public:
    /** @brief 创建尚未请求取消的停止源。 */
    ZzStopSource()
        : state_(std::make_shared<std::atomic<bool>>(false))
    {
    }

    /**
     * @brief 原子地请求协作取消。
     * @return 本次调用首次发出请求时返回 true，已经请求过时返回 false。
     */
    [[nodiscard]] bool requestStop() noexcept
    {
        bool expected = false;
        return state_->compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    /**
     * @brief 查询是否已经请求取消。
     * @return 已请求取消时返回 true。
     */
    [[nodiscard]] bool stopRequested() const noexcept
    {
        return state_->load(std::memory_order_acquire);
    }

    /**
     * @brief 创建共享当前停止状态的只读 Token。
     * @return 可跨线程传递的取消状态视图。
     */
    [[nodiscard]] ZzStopToken token() const noexcept
    {
        return ZzStopToken(state_);
    }

private:
    std::shared_ptr<std::atomic<bool>> state_;
};

} // namespace ZzCore
