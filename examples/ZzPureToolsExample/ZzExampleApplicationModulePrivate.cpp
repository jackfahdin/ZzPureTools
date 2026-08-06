#include "ZzExampleApplicationModulePrivate.h"

#include <QtCore/QCoreApplication>
#include <chrono>
#include <utility>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>

#include <ZzCore/ZzApplicationPaths.h>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzSettingsStore.h>
#include <ZzCore/ZzTaskExecutor.h>

#include <ZzLog/ZzLog.h>

#include "ZzExampleApplicationContext.h"
#include "ZzExampleActivityModel.h"

namespace ZzExample {

ZzExampleApplicationModulePrivate::ZzExampleApplicationModulePrivate(
    std::shared_ptr<ZzExampleApplicationContext> applicationContext)
    : context(std::move(applicationContext))
{
}

ZzCore::ZzResult<void> ZzExampleApplicationModulePrivate::start()
{
    if (!context) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("example application context must not be null")));
    }
    if (started || ownsLogRuntime) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState,
            QStringLiteral("example application module is already started")));
    }

    const QString logPath = QDir(context->paths().logDirectory())
        .filePath(QStringLiteral("ZzPureToolsExample.log"));
    ZzLog::ZzLogConfig config;
    config.loggerName = "ZzPureToolsExample";
    config.console.enabled = true;
    config.file.enabled = true;
    config.file.path = QFileInfo(logPath).filesystemAbsoluteFilePath();
    config.file.level = ZzLog::ZzLogLevel::Debug;
    config.file.async = true;

    const auto logResult = ZzLog::initialize(std::move(config));
    if (!logResult) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::Backend,
            QStringLiteral("failed to initialize ZzLog"),
            QString::fromUtf8(logResult.message)));
    }
    ownsLogRuntime = true;

    auto bridgeResult = logBridge.install();
    if (!bridgeResult) {
        ZzLog::shutdown();
        ownsLogRuntime = false;
        return bridgeResult;
    }

    ZZ_LOG_INFO("ZzPureToolsExample application module started");
    context->activityModel().append(QCoreApplication::translate("ZzPureToolsExample", "应用模块已启动"));
    started = true;
    stopRequested = false;
    return ZzCore::ZzResult<void>::success();
}

void ZzExampleApplicationModulePrivate::requestStop() noexcept
{
    if (!started || stopRequested) {
        return;
    }
    stopRequested = true;
}

void ZzExampleApplicationModulePrivate::stop() noexcept
{
    if (!started && !ownsLogRuntime) {
        return;
    }

    stopRequested = true;
    try {
        if (context) {
            const auto settingsResult = context->settingsStore().sync();
            if (!settingsResult) {
                ZzLog::writeText(
                    ZzLog::ZzLogLevel::Warning,
                    "failed to synchronize example settings");
            }
            if (!context->taskExecutor().shutdown(QDeadlineTimer(5000))) {
                ZzLog::writeText(
                    ZzLog::ZzLogLevel::Warning,
                    "example task executor did not stop before deadline");
            }
        }
        if (logBridge.isInstalled()) {
            static_cast<void>(logBridge.uninstall());
        }
        if (ownsLogRuntime) {
            static_cast<void>(
                ZzLog::flushAndWait(std::chrono::seconds(2)));
        }
    } catch (...) { // NOLINT(bugprone-empty-catch) stop() 必须 noexcept。
        // stop() 不允许异常越过应用运行时边界。
    }

    if (logBridge.isInstalled()) {
        try {
            static_cast<void>(logBridge.uninstall());
        } catch (...) { // NOLINT(bugprone-empty-catch) 继续关闭日志运行时。
            // 卸载失败仍必须继续关闭 ZzLog 运行时。
        }
    }
    if (ownsLogRuntime) {
        ZzLog::shutdown();
    }
    ownsLogRuntime = false;
    started = false;
}

} // namespace ZzExample
