#pragma once

#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QObject>
#include <QtCore/QRunnable>
#include <QtCore/QScopeGuard>
#include <QtCore/QString>

#include <ZzCore/ZzCoreExport.h>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzStopToken.h>
#include <ZzCore/ZzTaskHandle.h>

namespace ZzCore {

class ZzTaskExecutorPrivate;

/**
 * @brief 使用独立 QThreadPool 调度可协作取消的后台任务。
 *
 * submit() 可由多个线程并发调用。shutdown() 和析构只能由构造对象的 owner 线程
 * 调用；析构会无限等待不协作的 callable，callable 不得销毁所属 executor。GUI
 * 完成回调应使用 QFuture::then(QObject *, ...) 绑定接收者生命周期。
 */
class ZZ_CORE_EXPORT ZzTaskExecutor final : public QObject
{
public:
    /**
     * @brief 创建独立任务执行器。
     * @param threadCount 最大线程数；小于 1 时使用至少为 1 的理想线程数。
     * @param parent 可选 QObject 所有者；所有者必须与 executor 位于同一线程。
     */
    explicit ZzTaskExecutor(
        int threadCount = 0,
        QObject *parent = nullptr);

    /**
     * @brief 请求取消并等待全部任务退出后销毁执行器。
     *
     * 必须在 owner 线程调用。未响应 ZzStopToken 的 callable 会延长析构时间。
     */
    ~ZzTaskExecutor() noexcept override;

    ZzTaskExecutor(const ZzTaskExecutor &) = delete;
    ZzTaskExecutor &operator=(const ZzTaskExecutor &) = delete;
    ZzTaskExecutor(ZzTaskExecutor &&) = delete;
    ZzTaskExecutor &operator=(ZzTaskExecutor &&) = delete;

    /**
     * @brief 提交接收 ZzStopToken 的后台任务。
     * @tparam ZzValue 成功值类型，可以是仅移动类型。
     * @tparam ZzCallable 可移动调用对象类型。
     * @param callable 返回 ZzResult<ZzValue> 的调用对象。
     * @return 共享任务状态的 handle；停止接收后返回已完成的 InvalidState 结果。
     *
     * callable 在 executor 的独立线程池执行。异常会转换为 Unknown；开始执行前已经
     * 取消的任务不会调用 callable。仅移动结果必须由唯一消费者调用 takeResult()。
     */
    template<typename ZzValue, typename ZzCallable>
    requires std::invocable<ZzCallable &, ZzStopToken>
        && std::same_as<
            std::invoke_result_t<ZzCallable &, ZzStopToken>,
            ZzResult<ZzValue>>
    [[nodiscard]] ZzTaskHandle<ZzValue> submit(ZzCallable &&callable)
    {
        auto state = std::make_shared<Internal::ZzTaskState<ZzValue>>();
        state->promise.start();

        using ZzStoredCallable = std::decay_t<ZzCallable>;
        auto *runnable = QRunnable::create(
            [this,
             state,
             task = ZzStoredCallable(std::forward<ZzCallable>(callable))]
            () mutable {
                auto registryGuard = qScopeGuard([this, state] {
                    finishTask(state->taskId);
                });

                auto result = [&]() -> ZzResult<ZzValue> {
                    auto expected = ZzTaskStatus::Pending;
                    if (!state->status.compare_exchange_strong(
                            expected,
                            ZzTaskStatus::Running,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        return ZzResult<ZzValue>::failure(ZzError(
                            ZzErrorCode::Cancelled,
                            QStringLiteral("task cancelled before execution")));
                    }

                    try {
                        return std::invoke(task, state->stopSource.token());
                    } catch (const std::exception &exception) {
                        return ZzResult<ZzValue>::failure(ZzError(
                            ZzErrorCode::Unknown,
                            QString::fromUtf8(exception.what())));
                    } catch (...) {
                        return ZzResult<ZzValue>::failure(ZzError(
                            ZzErrorCode::Unknown,
                            QStringLiteral("unknown task exception")));
                    }
                }();

                try {
                    static_cast<void>(
                        state->promise.addResult(std::move(result)));
                } catch (...) {
                    try {
                        static_cast<void>(state->promise.addResult(
                            ZzResult<ZzValue>::failure(ZzError(
                                ZzErrorCode::Unknown,
                                QStringLiteral(
                                    "failed to store task result")))));
                    } catch (...) { // NOLINT(bugprone-empty-catch) promise 已不可写。
                    }
                }
                state->status.store(
                    ZzTaskStatus::Finished, std::memory_order_release);
                state->promise.finish();
            });

        if (!enqueue(state, runnable)) {
            delete runnable;
            static_cast<void>(state->promise.addResult(
                ZzResult<ZzValue>::failure(ZzError(
                    ZzErrorCode::InvalidState,
                    QStringLiteral("task executor is shutting down")))));
            state->status.store(
                ZzTaskStatus::Finished, std::memory_order_release);
            state->promise.finish();
        }
        return ZzTaskHandle<ZzValue>(std::move(state));
    }

    /**
     * @brief 获取线程池最大线程数。
     * @return 构造时解析后的线程数，始终至少为 1。
     */
    [[nodiscard]] int threadCount() const noexcept;

    /**
     * @brief 查询执行器是否仍接收新任务。
     * @return 接收任务时返回 true，shutdown 开始后返回 false。
     */
    [[nodiscard]] bool isAcceptingTasks() const noexcept;

    /**
     * @brief 停止接收任务、请求取消并等待线程池完成。
     * @param deadline 包含等待截止时间的 Qt deadline。
     * @return 截止时间前全部任务完成时返回 true，超时或内部等待失败时返回 false。
     *
     * 必须在 owner 线程调用。本函数幂等；超时不会分离或遗失仍在运行的任务，可使用
     * 新 deadline 再次调用。
     */
    [[nodiscard]] bool shutdown(QDeadlineTimer deadline) noexcept;

private:
    [[nodiscard]] bool enqueue(
        const std::shared_ptr<Internal::ZzTaskControl> &control,
        QRunnable *runnable);
    void finishTask(std::uint64_t taskId) noexcept;

    std::unique_ptr<ZzTaskExecutorPrivate> d_ptr;
};

} // namespace ZzCore
