#include "ZzQtLogBridgePrivate.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string_view>
#include <utility>

#include <QtCore/QByteArray>
#include <QtCore/QScopeGuard>
#include <QtCore/QString>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzLog/ZzLog.h>

namespace ZzCore {

namespace {

struct ZzQtLogBridgeGlobalState final
{
    std::mutex lifecycleMutex;
    std::condition_variable noInFlight;
    ZzQtLogBridgePrivate *active = nullptr;
    QtMessageHandler previousHandler = nullptr;
    std::size_t inFlight = 0;
    bool uninstalling = false;
};

[[nodiscard]] ZzQtLogBridgeGlobalState &zzGlobalState()
{
    static ZzQtLogBridgeGlobalState state;
    return state;
}

[[nodiscard]] ZzLog::ZzLogLevel zzMapLogLevel(QtMsgType type) noexcept
{
    switch (type) {
    case QtDebugMsg:
        return ZzLog::ZzLogLevel::Debug;
    case QtInfoMsg:
        return ZzLog::ZzLogLevel::Info;
    case QtWarningMsg:
        return ZzLog::ZzLogLevel::Warning;
    case QtCriticalMsg:
        return ZzLog::ZzLogLevel::Error;
    case QtFatalMsg:
        return ZzLog::ZzLogLevel::Critical;
    }
    return ZzLog::ZzLogLevel::Error;
}

[[nodiscard]] ZzResult<void> zzInvalidStateResult(QString message)
{
    return ZzResult<void>::failure(ZzError(
        ZzErrorCode::InvalidState,
        std::move(message)));
}

} // namespace

ZzResult<void> ZzQtLogBridgePrivate::install(
    const ZzQtLogBridgeConfig &newConfig)
{
    auto &globalState = zzGlobalState();
    std::lock_guard<std::mutex> lock(globalState.lifecycleMutex);
    if (installed || globalState.active != nullptr
        || globalState.uninstalling) {
        return zzInvalidStateResult(QStringLiteral(
            "a Qt log bridge is already installed or uninstalling"));
    }

    const auto previous = qInstallMessageHandler(
        ZzQtLogBridgePrivate::handleMessage);
    config = newConfig;
    globalState.previousHandler = previous;
    globalState.active = this;
    installed = true;
    return ZzResult<void>::success();
}

ZzResult<void> ZzQtLogBridgePrivate::uninstall()
{
    auto &globalState = zzGlobalState();
    std::unique_lock<std::mutex> lock(globalState.lifecycleMutex);
    if (!installed || globalState.active != this
        || globalState.uninstalling) {
        return zzInvalidStateResult(QStringLiteral(
            "this Qt log bridge is not installed"));
    }

    globalState.uninstalling = true;
    globalState.active = nullptr;
    static_cast<void>(qInstallMessageHandler(
        globalState.previousHandler));
    globalState.noInFlight.wait(lock, [&globalState] {
        return globalState.inFlight == 0;
    });
    installed = false;
    globalState.uninstalling = false;
    return ZzResult<void>::success();
}

bool ZzQtLogBridgePrivate::isInstalled() const noexcept
{
    auto &globalState = zzGlobalState();
    std::lock_guard<std::mutex> lock(globalState.lifecycleMutex);
    return installed && globalState.active == this
        && !globalState.uninstalling;
}

void ZzQtLogBridgePrivate::handleMessage(
    QtMsgType type,
    const QMessageLogContext &context,
    const QString &message)
{
    static thread_local bool handling = false;
    if (handling) {
        return;
    }

    handling = true;
    const auto reentrancyGuard = qScopeGuard([] {
        handling = false;
    });

    auto &globalState = zzGlobalState();
    ZzQtLogBridgePrivate *activeBridge = nullptr;
    QtMessageHandler previousHandler = nullptr;
    ZzQtLogBridgeConfig activeConfig;
    {
        std::lock_guard<std::mutex> lock(globalState.lifecycleMutex);
        activeBridge = globalState.active;
        previousHandler = globalState.previousHandler;
        if (activeBridge != nullptr) {
            activeConfig = activeBridge->config;
            ++globalState.inFlight;
        }
    }

    if (activeBridge == nullptr) {
        if (previousHandler != nullptr) {
            try {
                previousHandler(type, context, message);
            } catch (...) { // NOLINT(bugprone-empty-catch) Qt 回调不得逃逸异常。
            }
        }
        return;
    }

    const auto inFlightGuard = qScopeGuard([&globalState] {
        std::lock_guard<std::mutex> lock(globalState.lifecycleMutex);
        Q_ASSERT(globalState.inFlight > 0);
        --globalState.inFlight;
        if (globalState.inFlight == 0) {
            globalState.noInFlight.notify_all();
        }
    });

    try {
        const auto formattedMessage = qFormatLogMessage(
            type, context, message).toUtf8();
        ZzLog::writeText(
            zzMapLogLevel(type),
            std::string_view(
                formattedMessage.constData(),
                static_cast<std::size_t>(formattedMessage.size())));
    } catch (...) { // NOLINT(bugprone-empty-catch) 日志桥不得递归记录异常。
    }

    if (activeConfig.chainPreviousHandler && previousHandler != nullptr) {
        try {
            previousHandler(type, context, message);
        } catch (...) { // NOLINT(bugprone-empty-catch) Qt 回调不得逃逸异常。
        }
    }
}

} // namespace ZzCore
