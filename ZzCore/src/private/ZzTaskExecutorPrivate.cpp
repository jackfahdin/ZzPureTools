#include "ZzTaskExecutorPrivate.h"

#include <exception>
#include <utility>

#include <QtCore/QThread>
#include <QtCore/QtGlobal>

namespace ZzCore {

ZzTaskExecutorPrivate::ZzTaskExecutorPrivate(int requestedThreadCount)
    : ownerThread(QThread::currentThread())
{
    const auto resolvedThreadCount = requestedThreadCount > 0
        ? requestedThreadCount
        : qMax(1, QThread::idealThreadCount());
    threadPool.setMaxThreadCount(resolvedThreadCount);
}

ZzTaskExecutorPrivate::~ZzTaskExecutorPrivate() noexcept
{
    if (isWorkerThread()) {
        Q_ASSERT_X(
            false,
            "ZzTaskExecutorPrivate::~ZzTaskExecutorPrivate",
            "executor must not be destroyed from its worker thread");
        std::terminate();
    }
    if (!shutdown(QDeadlineTimer(QDeadlineTimer::Forever))) {
        std::terminate();
    }
}

bool ZzTaskExecutorPrivate::enqueue(
    const std::shared_ptr<Internal::ZzTaskControl> &control,
    QRunnable *runnable)
{
    std::lock_guard<std::mutex> lock(tasksMutex);
    if (!acceptingTasks) {
        return false;
    }

    while (nextTaskId == 0 || tasks.contains(nextTaskId)) {
        ++nextTaskId;
    }
    control->taskId = nextTaskId;
    ++nextTaskId;
    tasks.emplace(control->taskId, control);
    threadPool.start(runnable);
    return true;
}

void ZzTaskExecutorPrivate::finishTask(std::uint64_t taskId) noexcept
{
    std::lock_guard<std::mutex> lock(tasksMutex);
    tasks.erase(taskId);
}

bool ZzTaskExecutorPrivate::shutdown(QDeadlineTimer deadline) noexcept
{
    try {
        {
            std::lock_guard<std::mutex> lock(tasksMutex);
            acceptingTasks = false;
            for (const auto &[taskId, control] : tasks) {
                static_cast<void>(taskId);
                static_cast<void>(control->requestCancellation());
            }
        }

        return threadPool.waitForDone(deadline);
    } catch (...) {
        return false;
    }
}

int ZzTaskExecutorPrivate::threadCount() const noexcept
{
    return threadPool.maxThreadCount();
}

bool ZzTaskExecutorPrivate::isAcceptingTasks() const noexcept
{
    std::lock_guard<std::mutex> lock(tasksMutex);
    return acceptingTasks;
}

bool ZzTaskExecutorPrivate::isOwnerThread() const noexcept
{
    return QThread::currentThread() == ownerThread;
}

bool ZzTaskExecutorPrivate::isWorkerThread() const noexcept
{
    return threadPool.contains(QThread::currentThread());
}

} // namespace ZzCore
