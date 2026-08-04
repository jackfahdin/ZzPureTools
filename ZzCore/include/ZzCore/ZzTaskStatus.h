#pragma once

#include <cstdint>

namespace ZzCore {

/**
 * @brief 后台任务的可观测生命周期状态。
 */
enum class ZzTaskStatus : std::uint8_t
{
    /** @brief 已提交但尚未开始执行。 */
    Pending,
    /** @brief callable 正在工作线程执行。 */
    Running,
    /** @brief future 已完成，不会再改变结果。 */
    Finished,
    /** @brief 已请求协作取消，任务可能仍在退出过程中。 */
    CancellationRequested
};

} // namespace ZzCore
