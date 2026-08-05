#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace ZzCore {

class ZzStopSource;

/**
 * @brief 提供跨平台协作取消状态的只读视图。
 *
 * Token 可以低成本复制并跨线程传递。默认构造的 Token 没有关联停止源，
 * 因此 stopPossible() 和 stopRequested() 均返回 false。
 */
class ZzStopToken final
{
public:
    /** @brief 创建没有关联停止源的 Token。 */
    ZzStopToken() noexcept = default;

    /**
     * @brief 查询关联停止源是否已经请求取消。
     * @return 已请求取消时返回 true，否则返回 false。
     */
    [[nodiscard]] bool stopRequested() const noexcept
    {
        return state_ != nullptr
            && state_->load(std::memory_order_acquire);
    }

    /**
     * @brief 查询当前 Token 是否关联有效停止源。
     * @return 可以接收停止请求时返回 true。
     */
    [[nodiscard]] bool stopPossible() const noexcept
    {
        return state_ != nullptr;
    }

private:
    friend class ZzStopSource;

    /** @brief 从停止源共享原子状态。 */
    explicit ZzStopToken(
        std::shared_ptr<std::atomic<bool>> state) noexcept
        : state_(std::move(state))
    {
    }

    std::shared_ptr<std::atomic<bool>> state_;
};

} // namespace ZzCore
