#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QThreadPool>

#include <ZzCore/ZzTaskHandle.h>

class QThread;

namespace ZzCore {

class ZzTaskExecutorPrivate final
{
public:
    explicit ZzTaskExecutorPrivate(int requestedThreadCount);
    ~ZzTaskExecutorPrivate() noexcept;

    [[nodiscard]] bool enqueue(
        const std::shared_ptr<Internal::ZzTaskControl> &control,
        QRunnable *runnable);
    void finishTask(std::uint64_t taskId) noexcept;
    [[nodiscard]] bool shutdown(QDeadlineTimer deadline) noexcept;
    [[nodiscard]] int threadCount() const noexcept;
    [[nodiscard]] bool isAcceptingTasks() const noexcept;
    [[nodiscard]] bool isOwnerThread() const noexcept;
    [[nodiscard]] bool isWorkerThread() const noexcept;

    QThreadPool threadPool;
    mutable std::mutex tasksMutex;
    bool acceptingTasks = true;
    std::uint64_t nextTaskId = 1;
    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<Internal::ZzTaskControl>> tasks;
    QThread *ownerThread = nullptr;
};

} // namespace ZzCore
