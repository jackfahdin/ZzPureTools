#include <QtTest/QTest>

#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

#include <QtCore/QByteArrayView>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSemaphore>
#include <QtCore/QTemporaryDir>

#include <ZzCore/ZzQtLogBridge.h>
#include <ZzLog/ZzLog.h>

namespace {

/**
 * @brief 保存阻塞旧日志处理器所需的确定性同步点。
 */
struct ZzBlockingHandlerState final
{
    QSemaphore previousEntered;
    QSemaphore allowPreviousReturn;
};

ZzBlockingHandlerState *zzBlockingHandlerState = nullptr;
std::atomic<int> *zzPreviousHandlerCount = nullptr;
std::atomic<int> *zzReentrantHandlerCount = nullptr;

void zzBlockingPreviousHandler(
    QtMsgType,
    const QMessageLogContext &,
    const QString &)
{
    auto *state = zzBlockingHandlerState;
    Q_ASSERT(state != nullptr);
    state->previousEntered.release();
    state->allowPreviousReturn.acquire();
}

void zzCountingPreviousHandler(
    QtMsgType,
    const QMessageLogContext &,
    const QString &)
{
    auto *count = zzPreviousHandlerCount;
    Q_ASSERT(count != nullptr);
    count->fetch_add(1, std::memory_order_relaxed);
}

void zzReentrantPreviousHandler(
    QtMsgType,
    const QMessageLogContext &,
    const QString &)
{
    auto *count = zzReentrantHandlerCount;
    Q_ASSERT(count != nullptr);
    count->fetch_add(1, std::memory_order_relaxed);
    qWarning("nested-warning-must-be-dropped");
}

[[nodiscard]] bool zzContainsMappedMessage(
    const QByteArray &contents,
    QByteArrayView level,
    QByteArrayView message)
{
    const auto lines = contents.split('\n');
    for (const auto &line : lines) {
        if (line.contains(level) && line.contains(message)) {
            return true;
        }
    }
    return false;
}

} // namespace

/**
 * @brief 验证 Qt 全局日志桥的安装、映射、恢复与并发卸载语义。
 */
class ZzQtLogBridgeTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cleanup()
    {
        ZzLog::shutdown();
    }

    void forwardsLevelsToFile()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const auto logFilePath = temporaryDirectory.filePath(
            QStringLiteral("qt-bridge.log"));

        ZzLog::ZzLogConfig logConfig;
        logConfig.console.enabled = false;
        logConfig.file.enabled = true;
        logConfig.file.path = QFileInfo(logFilePath)
                                  .filesystemAbsoluteFilePath();
        logConfig.file.level = ZzLog::ZzLogLevel::Debug;
        logConfig.file.pattern = "%l|%v";
        const auto initialized = ZzLog::initialize(std::move(logConfig));
        QVERIFY2(initialized, initialized.message.c_str());

        ZzCore::ZzQtLogBridge bridge;
        QVERIFY(bridge.install());
        QVERIFY(bridge.isInstalled());
        qDebug("bridge-debug");
        qInfo("bridge-info");
        qWarning("bridge-warning");
        qCritical("bridge-critical");
        QVERIFY(bridge.uninstall());
        QVERIFY(!bridge.isInstalled());
        QVERIFY(ZzLog::flushAndWait(std::chrono::seconds(5)));

        QFile logFile(logFilePath);
        QVERIFY(logFile.open(QIODevice::ReadOnly));
        const auto contents = logFile.readAll();
        QVERIFY(zzContainsMappedMessage(
            contents, QByteArrayView("debug|"), QByteArrayView("bridge-debug")));
        QVERIFY(zzContainsMappedMessage(
            contents, QByteArrayView("info|"), QByteArrayView("bridge-info")));
        QVERIFY(zzContainsMappedMessage(
            contents,
            QByteArrayView("warning|"),
            QByteArrayView("bridge-warning")));
        QVERIFY(zzContainsMappedMessage(
            contents,
            QByteArrayView("error|"),
            QByteArrayView("bridge-critical")));
    }

    void chainsAndRestoresPreviousHandler()
    {
        std::atomic<int> previousCalls{0};
        zzPreviousHandlerCount = &previousCalls;
        const auto originalHandler = qInstallMessageHandler(
            zzCountingPreviousHandler);

        ZzCore::ZzQtLogBridge bridge;
        QVERIFY(bridge.install({.chainPreviousHandler = true}));
        qWarning("chained-warning");
        QVERIFY(bridge.uninstall());
        QCOMPARE(previousCalls.load(std::memory_order_relaxed), 1);

        const auto restoredHandler = qInstallMessageHandler(originalHandler);
        zzPreviousHandlerCount = nullptr;
        QVERIFY(restoredHandler == zzCountingPreviousHandler);
    }

    void rejectsSecondInstance()
    {
        ZzCore::ZzQtLogBridge first;
        ZzCore::ZzQtLogBridge second;
        QVERIFY(first.install());

        const auto rejected = second.install();
        QVERIFY(!rejected);
        QCOMPARE(
            rejected.error().code(),
            ZzCore::ZzErrorCode::InvalidState);

        QVERIFY(first.uninstall());
        QVERIFY(second.install());
        QVERIFY(second.uninstall());
    }

    void destructorRestoresPreviousHandler()
    {
        std::atomic<int> previousCalls{0};
        zzPreviousHandlerCount = &previousCalls;
        const auto originalHandler = qInstallMessageHandler(
            zzCountingPreviousHandler);

        {
            ZzCore::ZzQtLogBridge bridge;
            QVERIFY(bridge.install({.chainPreviousHandler = true}));
            qWarning("destructor-restore-warning");
        }

        const auto restoredHandler = qInstallMessageHandler(originalHandler);
        zzPreviousHandlerCount = nullptr;
        QCOMPARE(previousCalls.load(std::memory_order_relaxed), 1);
        QVERIFY(restoredHandler == zzCountingPreviousHandler);
    }

    void dropsReentrantQtMessage()
    {
        std::atomic<int> previousCalls{0};
        zzReentrantHandlerCount = &previousCalls;
        const auto originalHandler = qInstallMessageHandler(
            zzReentrantPreviousHandler);

        ZzCore::ZzQtLogBridge bridge;
        QVERIFY(bridge.install({.chainPreviousHandler = true}));
        qWarning("outer-warning");
        QVERIFY(bridge.uninstall());

        const auto restoredHandler = qInstallMessageHandler(originalHandler);
        zzReentrantHandlerCount = nullptr;
        QCOMPARE(previousCalls.load(std::memory_order_relaxed), 1);
        QVERIFY(restoredHandler == zzReentrantPreviousHandler);
    }

    void concurrentUninstallWaitsForInFlightHandler()
    {
        for (int iteration = 0; iteration < 100; ++iteration) {
            ZzBlockingHandlerState handlerState;
            zzBlockingHandlerState = &handlerState;
            const auto originalHandler = qInstallMessageHandler(
                zzBlockingPreviousHandler);

            ZzCore::ZzQtLogBridge bridge;
            QVERIFY(bridge.install({.chainPreviousHandler = true}));

            std::jthread writer([] {
                qWarning("concurrent-uninstall-warning");
            });
            const bool previousEntered =
                handlerState.previousEntered.tryAcquire(1, 1000);

            QSemaphore uninstallStarted;
            QSemaphore uninstallReturned;
            std::atomic<bool> uninstallSucceeded{false};
            std::jthread uninstaller([&] {
                uninstallStarted.release();
                const auto result = bridge.uninstall();
                uninstallSucceeded.store(
                    static_cast<bool>(result), std::memory_order_release);
                uninstallReturned.release();
            });

            const bool uninstallerStarted = uninstallStarted.tryAcquire(1, 1000);
            const bool returnedBeforeRelease =
                uninstallReturned.tryAcquire(1, 50);
            handlerState.allowPreviousReturn.release();
            writer.join();
            uninstaller.join();
            const bool returnedAfterRelease = returnedBeforeRelease
                || uninstallReturned.tryAcquire(1, 1000);

            const auto restoredHandler = qInstallMessageHandler(
                originalHandler);
            zzBlockingHandlerState = nullptr;

            QVERIFY2(previousEntered, "旧 handler 未在时限内进入");
            QVERIFY2(uninstallerStarted, "卸载线程未在时限内启动");
            QVERIFY2(!returnedBeforeRelease, "卸载未等待在途 handler");
            QVERIFY2(returnedAfterRelease, "卸载线程未正常返回");
            QVERIFY(uninstallSucceeded.load(std::memory_order_acquire));
            QVERIFY(restoredHandler == zzBlockingPreviousHandler);
        }
    }
};

QTEST_GUILESS_MAIN(ZzQtLogBridgeTest)

#include "ZzQtLogBridgeTest.moc"
