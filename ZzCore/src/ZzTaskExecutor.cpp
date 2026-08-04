#include <ZzCore/ZzTaskExecutor.h>

#include <exception>

#include <QtCore/QThread>
#include <QtCore/QtGlobal>

#include "private/ZzTaskExecutorPrivate.h"

namespace ZzCore {

ZzTaskExecutor::ZzTaskExecutor(int threadCount, QObject *parent)
    : QObject(parent)
    , d_ptr(std::make_unique<ZzTaskExecutorPrivate>(threadCount))
{
}

ZzTaskExecutor::~ZzTaskExecutor()
{
    if (!d_ptr->isOwnerThread() || d_ptr->isWorkerThread()) {
        Q_ASSERT_X(
            false,
            "ZzTaskExecutor::~ZzTaskExecutor",
            "executor must be destroyed from its owner thread");
        std::terminate();
    }
    static_cast<void>(
        d_ptr->shutdown(QDeadlineTimer(QDeadlineTimer::Forever)));
}

int ZzTaskExecutor::threadCount() const noexcept
{
    return d_ptr->threadCount();
}

bool ZzTaskExecutor::isAcceptingTasks() const noexcept
{
    return d_ptr->isAcceptingTasks();
}

bool ZzTaskExecutor::shutdown(QDeadlineTimer deadline)
{
    if (!d_ptr->isOwnerThread() || d_ptr->isWorkerThread()) {
        Q_ASSERT_X(
            false,
            "ZzTaskExecutor::shutdown",
            "shutdown must be called from the owner thread");
        std::terminate();
    }
    return d_ptr->shutdown(deadline);
}

bool ZzTaskExecutor::enqueue(
    const std::shared_ptr<Internal::ZzTaskControl> &control,
    QRunnable *runnable)
{
    return d_ptr->enqueue(control, runnable);
}

void ZzTaskExecutor::finishTask(std::uint64_t taskId) noexcept
{
    d_ptr->finishTask(taskId);
}

} // namespace ZzCore
