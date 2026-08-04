#include <QtTest/QTest>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <thread>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QObject>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>

#include <ZzCore/ZzTaskExecutor.h>

class ZzTaskExecutorTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void returnsValue()
    {
        ZzCore::ZzTaskExecutor executor(2);
        auto handle = executor.submit<int>([](std::stop_token) {
            return ZzCore::ZzResult<int>::success(42);
        });
        handle.future().waitForFinished();
        QCOMPARE(handle.future().result().value(), 42);
    }

    void convertsException()
    {
        ZzCore::ZzTaskExecutor executor(1);
        auto handle = executor.submit<int>([](std::stop_token)
            -> ZzCore::ZzResult<int> {
            throw std::runtime_error("failure");
        });
        handle.future().waitForFinished();
        QCOMPARE(
            handle.future().result().error().code(),
            ZzCore::ZzErrorCode::Unknown);
    }

    void cooperativelyCancels()
    {
        ZzCore::ZzTaskExecutor executor(1);
        auto handle = executor.submit<int>([](std::stop_token token) {
            while (!token.stop_requested()) {
                QThread::yieldCurrentThread();
            }
            return ZzCore::ZzResult<int>::failure(
                ZzCore::ZzError(
                    ZzCore::ZzErrorCode::Cancelled,
                    QStringLiteral("cancelled")));
        });
        handle.requestCancel();
        handle.future().waitForFinished();
        QCOMPARE(
            handle.future().result().error().code(),
            ZzCore::ZzErrorCode::Cancelled);
    }

    void rejectsSubmissionAfterShutdown()
    {
        ZzCore::ZzTaskExecutor executor(1);
        QVERIFY(executor.shutdown(QDeadlineTimer(1000)));
        auto handle = executor.submit<int>([](std::stop_token) {
            return ZzCore::ZzResult<int>::success(1);
        });
        handle.future().waitForFinished();
        QCOMPARE(
            handle.future().result().error().code(),
            ZzCore::ZzErrorCode::InvalidState);
    }

    void shutdownCancelsRunningAndQueuedTasks()
    {
        ZzCore::ZzTaskExecutor executor(1);
        QSemaphore runningTaskStarted;

        auto running = executor.submit<int>([&runningTaskStarted](
            std::stop_token token) {
            runningTaskStarted.release();
            while (!token.stop_requested()) {
                QThread::yieldCurrentThread();
            }
            return ZzCore::ZzResult<int>::failure(
                ZzCore::ZzError(
                    ZzCore::ZzErrorCode::Cancelled,
                    QStringLiteral("running task cancelled")));
        });

        QVERIFY(runningTaskStarted.tryAcquire(1, 1000));
        auto queued = executor.submit<int>([](std::stop_token) {
            return ZzCore::ZzResult<int>::success(99);
        });

        QVERIFY(executor.shutdown(QDeadlineTimer(2000)));
        running.future().waitForFinished();
        queued.future().waitForFinished();
        QCOMPARE(
            running.future().result().error().code(),
            ZzCore::ZzErrorCode::Cancelled);
        QCOMPARE(
            queued.future().result().error().code(),
            ZzCore::ZzErrorCode::Cancelled);
    }

    void returnsMoveOnlyValue()
    {
        ZzCore::ZzTaskExecutor executor(1);
        auto handle = executor.submit<std::unique_ptr<int>>(
            [](std::stop_token) {
                return ZzCore::ZzResult<std::unique_ptr<int>>::success(
                    std::make_unique<int>(7));
            });

        auto future = handle.future();
        future.waitForFinished();
        auto result = future.takeResult();
        auto value = std::move(result).value();
        QVERIFY(value != nullptr);
        QCOMPARE(*value, 7);
    }

    void lateCancelDoesNotOverwriteFinished()
    {
        ZzCore::ZzTaskExecutor executor(1);
        auto handle = executor.submit<int>([](std::stop_token) {
            return ZzCore::ZzResult<int>::success(1);
        });
        handle.future().waitForFinished();
        QCOMPARE(handle.status(), ZzCore::ZzTaskStatus::Finished);
        handle.requestCancel();
        QCOMPARE(handle.status(), ZzCore::ZzTaskStatus::Finished);
    }

    void destroyedContextDropsContinuation()
    {
        ZzCore::ZzTaskExecutor executor(1);
        QSemaphore started;
        QSemaphore allowCompletion;
        std::atomic<int> callbackCount{0};
        auto handle = executor.submit<int>([&](std::stop_token) {
            started.release();
            allowCompletion.acquire();
            return ZzCore::ZzResult<int>::success(1);
        });
        QVERIFY(started.tryAcquire(1, 1000));

        auto context = std::make_unique<QObject>();
        auto continuation = handle.future().then(
            context.get(),
            [&callbackCount](const ZzCore::ZzResult<int> &) {
                callbackCount.fetch_add(1, std::memory_order_relaxed);
            });
        context.reset();
        allowCompletion.release();
        handle.future().waitForFinished();
        QCoreApplication::processEvents();

        Q_UNUSED(continuation);
        QCOMPARE(callbackCount.load(std::memory_order_relaxed), 0);
    }

    void shutdownTimeoutKeepsTaskOwned()
    {
        ZzCore::ZzTaskExecutor executor(1);
        QSemaphore started;
        QSemaphore allowCompletion;
        auto handle = executor.submit<int>([&](std::stop_token token) {
            started.release();
            allowCompletion.acquire();
            if (token.stop_requested()) {
                return ZzCore::ZzResult<int>::failure(
                    ZzCore::ZzError(
                        ZzCore::ZzErrorCode::Cancelled,
                        QStringLiteral("cancelled after blocking stage")));
            }
            return ZzCore::ZzResult<int>::success(1);
        });
        QVERIFY(started.tryAcquire(1, 1000));

        QVERIFY(!executor.shutdown(QDeadlineTimer(10)));
        std::jthread releaser([&allowCompletion] {
            allowCompletion.release();
        });
        QVERIFY(executor.shutdown(QDeadlineTimer(2000)));
        handle.future().waitForFinished();
        QCOMPARE(
            handle.future().result().error().code(),
            ZzCore::ZzErrorCode::Cancelled);
    }
};

QTEST_GUILESS_MAIN(ZzTaskExecutorTest)

#include "ZzTaskExecutorTest.moc"
