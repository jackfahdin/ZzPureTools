#include "ZzTaskExecutorPrivate.h"

#include <exception>
#include <utility>
#include <vector>

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

ZzTaskExecutorPrivate::~ZzTaskExecutorPrivate()
{
    if (isWorkerThread()) {
        Q_ASSERT_X(
            false,
            "ZzTaskExecutorPrivate::~ZzTaskExecutorPrivate",
            "executor must not be destroyed from its worker thread");
        std::terminate();
    }
    static_cast<void>(shutdown(QDeadlineTimer(QDeadlineTimer::Forever)));
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

bool ZzTaskExecutorPrivate::shutdown(QDeadlineTimer deadline)
{
    std::vector<std::shared_ptr<Internal::ZzTaskControl>> snapshot;
    {
        std::lock_guard<std::mutex> lock(tasksMutex);
        acceptingTasks = false;
        snapshot.reserve(tasks.size());
        for (const auto &[taskId, control] : tasks) {
            static_cast<void>(taskId);
            snapshot.push_back(control);
        }
    }

    for (const auto &control : snapshot) {
        static_cast<void>(control->requestCancellation());
    }

    return threadPool.waitForDone(deadline);
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
