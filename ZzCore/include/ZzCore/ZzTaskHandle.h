#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <utility>

#include <QtCore/QFuture>
#include <QtCore/QPromise>

#include <ZzCore/ZzResult.h>
#include <ZzCore/ZzTaskStatus.h>

namespace ZzCore {

namespace Internal {

struct ZzTaskControl
{
    virtual ~ZzTaskControl() = default;

    std::uint64_t taskId = 0;
    std::stop_source stopSource;
    std::atomic<ZzTaskStatus> status{ZzTaskStatus::Pending};

    bool requestCancellation() noexcept
    {
        auto current = status.load(std::memory_order_acquire);
        while (current != ZzTaskStatus::Finished
               && current != ZzTaskStatus::CancellationRequested) {
            if (status.compare_exchange_weak(
                    current,
                    ZzTaskStatus::CancellationRequested,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                static_cast<void>(stopSource.request_stop());
                return true;
            }
        }
        return false;
    }
};

template<typename ZzValue>
struct ZzTaskState final : ZzTaskControl
{
    QPromise<ZzResult<ZzValue>> promise;
};

} // namespace Internal

/**
 * @brief 持有后台任务结果、状态和协作取消入口。
 * @tparam ZzValue 任务成功值类型，可以是仅移动类型。
 *
 * Handle 可复制，所有副本共享同一任务状态。对于仅移动结果，只允许一个消费者在
 * future 完成后调用 QFuture::takeResult()；不得调用需要复制结果的 result()。
 */
template<typename ZzValue>
class ZzTaskHandle final
{
public:
    /**
     * @brief 获取共享 future。
     * @return 指向同一 QPromise 结果的 QFuture。
     */
    [[nodiscard]] QFuture<ZzResult<ZzValue>> future() const
    {
        return state_->promise.future();
    }

    /**
     * @brief 请求协作取消。
     *
     * 本函数不会强制终止线程。任务已经完成时调用不会覆盖 Finished 状态。
     */
    void requestCancel() noexcept
    {
        static_cast<void>(state_->requestCancellation());
    }

    /**
     * @brief 查询当前任务状态。
     * @return 原子读取的任务状态。
     */
    [[nodiscard]] ZzTaskStatus status() const noexcept
    {
        return state_->status.load(std::memory_order_acquire);
    }

private:
    friend class ZzTaskExecutor;

    explicit ZzTaskHandle(
        std::shared_ptr<Internal::ZzTaskState<ZzValue>> state)
        : state_(std::move(state))
    {
    }

    std::shared_ptr<Internal::ZzTaskState<ZzValue>> state_;
};

} // namespace ZzCore
