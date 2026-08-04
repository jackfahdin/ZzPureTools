#include "ZzLog/ZzLog.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "spdlog/logger.h"
#include "spdlog/sinks/async_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

#ifdef __ANDROID__
    #include "spdlog/sinks/android_sink.h"
#else
    #include "spdlog/sinks/stdout_color_sinks.h"
#endif

namespace ZzLog {
namespace {

namespace backend = zzlog_spdlog;

using ZzBackendLogger = backend::logger;
using ZzBackendSink = backend::sinks::sink;

struct ZzRuntimeState final
{
    std::mutex lifecycleMutex;
    std::atomic<ZzBackendLogger *> activeLogger{nullptr};
    std::shared_ptr<ZzBackendLogger> logger;
    std::shared_ptr<ZzBackendSink> consoleSink;
    std::shared_ptr<ZzBackendSink> fileSink;
    ZzLogConfig config;
    std::filesystem::path filePath;
};

ZzRuntimeState &runtime()
{
    static ZzRuntimeState state;
    return state;
}

backend::level toBackendLevel(ZzLogLevel level) noexcept
{
    switch (level) {
        case ZzLogLevel::Trace:
            return backend::level::trace;
        case ZzLogLevel::Debug:
            return backend::level::debug;
        case ZzLogLevel::Info:
            return backend::level::info;
        case ZzLogLevel::Warning:
            return backend::level::warn;
        case ZzLogLevel::Error:
            return backend::level::err;
        case ZzLogLevel::Critical:
            return backend::level::critical;
        case ZzLogLevel::Off:
            return backend::level::off;
    }
    return backend::level::off;
}

bool validLevel(ZzLogLevel level) noexcept
{
    return static_cast<unsigned>(level)
        <= static_cast<unsigned>(ZzLogLevel::Off);
}

bool validPolicy(ZzLogOverflowPolicy policy) noexcept
{
    return static_cast<unsigned>(policy)
        <= static_cast<unsigned>(ZzLogOverflowPolicy::DiscardNew);
}

backend::sinks::async_sink::overflow_policy toBackendPolicy(
    ZzLogOverflowPolicy policy) noexcept
{
    switch (policy) {
        case ZzLogOverflowPolicy::Block:
            return backend::sinks::async_sink::overflow_policy::block;
        case ZzLogOverflowPolicy::OverrunOldest:
            return backend::sinks::async_sink::overflow_policy::overrun_oldest;
        case ZzLogOverflowPolicy::DiscardNew:
            return backend::sinks::async_sink::overflow_policy::discard_new;
    }
    return backend::sinks::async_sink::overflow_policy::block;
}

backend::level effectiveLevel(const ZzLogConfig &config) noexcept
{
    auto result = backend::level::off;
    if (config.console.enabled) {
        result = std::min(result, toBackendLevel(config.console.level));
    }
    if (config.file.enabled) {
        result = std::min(result, toBackendLevel(config.file.level));
    }
    return result;
}

ZzLogInitResult failure(ZzLogInitError error, std::string message)
{
    ZzLogInitResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

void emitInternalError(std::string_view message) noexcept
{
    ZzLogErrorHandler handler;
    {
        auto &state = runtime();
        std::lock_guard<std::mutex> lock(state.lifecycleMutex);
        handler = state.config.errorHandler;
    }

    if (handler) {
        try {
            handler(message);
            return;
        } catch (...) {
        }
    }

    std::fprintf(
        stderr,
        "[ZzLog error] %.*s\n",
        static_cast<int>(message.size()),
        message.data());
}

const char *baseName(const char *path) noexcept
{
    const char *result = path;
    for (const char *cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            result = cursor + 1;
        }
    }
    return result;
}

} // namespace

ZzLogInitResult initialize(ZzLogConfig config)
{
    auto &state = runtime();
    std::lock_guard<std::mutex> lock(state.lifecycleMutex);

    if (state.logger) {
        return failure(
            ZzLogInitError::AlreadyInitialized,
            "ZzLog is already initialized");
    }
    if (config.loggerName.empty()) {
        return failure(
            ZzLogInitError::InvalidConfiguration,
            "loggerName must not be empty");
    }
    if (!validLevel(config.console.level)
        || !validLevel(config.file.level)
        || !validLevel(config.flushOn)) {
        return failure(
            ZzLogInitError::InvalidConfiguration,
            "configuration contains an invalid log level");
    }
    if (config.file.enabled) {
        if (config.file.path.empty()) {
            return failure(
                ZzLogInitError::InvalidConfiguration,
                "file.path must be provided when file logging is enabled");
        }
        if (config.file.maxFileSize == 0) {
            return failure(
                ZzLogInitError::InvalidConfiguration,
                "file.maxFileSize must be greater than zero");
        }
        if (config.file.maxFiles > 200000) {
            return failure(
                ZzLogInitError::InvalidConfiguration,
                "file.maxFiles must not exceed 200000");
        }
        if (!validPolicy(config.file.overflowPolicy)) {
            return failure(
                ZzLogInitError::InvalidConfiguration,
                "file.overflowPolicy is invalid");
        }
        if (config.file.async
            && (config.file.queueSize == 0
                || config.file.queueSize
                    > backend::sinks::async_sink::max_queue_size)) {
            return failure(
                ZzLogInitError::InvalidConfiguration,
                "file.queueSize is outside the supported range");
        }
    }

    std::filesystem::path resolvedFilePath;
    if (config.file.enabled) {
        std::error_code error;
        resolvedFilePath = std::filesystem::absolute(config.file.path, error)
                               .lexically_normal();
        if (error) {
            return failure(
                ZzLogInitError::FilesystemError,
                "failed to resolve file.path: " + error.message());
        }
    }

    try {
        std::vector<std::shared_ptr<ZzBackendSink>> sinks;
        std::shared_ptr<ZzBackendSink> consoleSink;
        std::shared_ptr<ZzBackendSink> fileSink;

        if (config.console.enabled) {
#ifdef __ANDROID__
            auto console = std::make_shared<backend::sinks::android_sink_mt>(
                config.loggerName);
#else
            auto console =
                std::make_shared<backend::sinks::stdout_color_sink_mt>();
#endif
            console->set_level(toBackendLevel(config.console.level));
            console->set_pattern(config.console.pattern);
            consoleSink = console;
            sinks.push_back(consoleSink);
        }

        if (config.file.enabled) {
            if (config.file.async) {
                auto rotating =
                    std::make_shared<backend::sinks::rotating_file_sink_st>(
                        resolvedFilePath,
                        config.file.maxFileSize,
                        config.file.maxFiles,
                        config.file.rotateOnOpen);
                rotating->set_pattern(config.file.pattern);

                backend::sinks::async_sink::config asyncConfig;
                asyncConfig.queue_size = config.file.queueSize;
                asyncConfig.policy = toBackendPolicy(config.file.overflowPolicy);
                asyncConfig.sinks.push_back(std::move(rotating));
                auto asyncFile =
                    std::make_shared<backend::sinks::async_sink>(
                        std::move(asyncConfig));
                asyncFile->set_level(toBackendLevel(config.file.level));
                fileSink = std::move(asyncFile);
            } else {
                auto rotating =
                    std::make_shared<backend::sinks::rotating_file_sink_mt>(
                        resolvedFilePath,
                        config.file.maxFileSize,
                        config.file.maxFiles,
                        config.file.rotateOnOpen);
                rotating->set_level(toBackendLevel(config.file.level));
                rotating->set_pattern(config.file.pattern);
                fileSink = std::move(rotating);
            }
            sinks.push_back(fileSink);
        }

        auto logger = std::make_shared<ZzBackendLogger>(
            config.loggerName,
            sinks.begin(),
            sinks.end());
        logger->set_level(effectiveLevel(config));
        logger->flush_on(toBackendLevel(config.flushOn));
        if (config.errorHandler) {
            logger->set_error_handler(
                [handler = config.errorHandler](const std::string &message) {
                    try {
                        handler(message);
                    } catch (...) {
                    }
                });
        }

        state.config = std::move(config);
        state.filePath = resolvedFilePath;
        state.consoleSink = std::move(consoleSink);
        state.fileSink = std::move(fileSink);
        state.logger = std::move(logger);
        state.activeLogger.store(state.logger.get(), std::memory_order_release);

        ZzLogInitResult result;
        result.filePath = resolvedFilePath;
        return result;
    } catch (const backend::spdlog_ex &ex) {
        return failure(ZzLogInitError::BackendError, ex.what());
    } catch (const std::filesystem::filesystem_error &ex) {
        return failure(ZzLogInitError::FilesystemError, ex.what());
    } catch (const std::exception &ex) {
        return failure(ZzLogInitError::BackendError, ex.what());
    } catch (...) {
        return failure(
            ZzLogInitError::BackendError,
            "unknown error while initializing ZzLog");
    }
}

void shutdown() noexcept
{
    auto &state = runtime();
    std::shared_ptr<ZzBackendLogger> logger;
    std::shared_ptr<ZzBackendSink> consoleSink;
    std::shared_ptr<ZzBackendSink> fileSink;
    {
        std::lock_guard<std::mutex> lock(state.lifecycleMutex);
        state.activeLogger.store(nullptr, std::memory_order_release);
        logger = std::move(state.logger);
        consoleSink = std::move(state.consoleSink);
        fileSink = std::move(state.fileSink);
        state.config = ZzLogConfig{};
        state.filePath.clear();
    }

    try {
        if (logger) {
            logger->flush();
        }
    } catch (const std::exception &ex) {
        std::fprintf(stderr, "[ZzLog error] shutdown flush failed: %s\n", ex.what());
    } catch (...) {
        std::fprintf(stderr, "[ZzLog error] shutdown flush failed\n");
    }
    logger.reset();
    fileSink.reset();
    consoleSink.reset();
}

void flush() noexcept
{
    auto *logger = runtime().activeLogger.load(std::memory_order_acquire);
    if (logger == nullptr) {
        return;
    }
    try {
        logger->flush();
    } catch (const std::exception &ex) {
        emitInternalError(ex.what());
    } catch (...) {
        emitInternalError("unknown backend error while flushing");
    }
}

bool isInitialized() noexcept
{
    return runtime().activeLogger.load(std::memory_order_acquire) != nullptr;
}

bool shouldLog(ZzLogLevel level) noexcept
{
    if (!validLevel(level) || level == ZzLogLevel::Off) {
        return false;
    }
    auto *logger = runtime().activeLogger.load(std::memory_order_acquire);
    return logger != nullptr
        && logger->should_log(toBackendLevel(level));
}

std::filesystem::path activeFilePath()
{
    auto &state = runtime();
    std::lock_guard<std::mutex> lock(state.lifecycleMutex);
    return state.filePath;
}

bool setConsoleLevel(ZzLogLevel level) noexcept
{
    if (!validLevel(level)) {
        return false;
    }
    try {
        auto &state = runtime();
        std::lock_guard<std::mutex> lock(state.lifecycleMutex);
        if (!state.logger || !state.consoleSink) {
            return false;
        }
        state.config.console.level = level;
        state.consoleSink->set_level(toBackendLevel(level));
        state.logger->set_level(effectiveLevel(state.config));
        return true;
    } catch (const std::exception &ex) {
        emitInternalError(ex.what());
    } catch (...) {
        emitInternalError("unknown backend error while setting console level");
    }
    return false;
}

bool setFileLevel(ZzLogLevel level) noexcept
{
    if (!validLevel(level)) {
        return false;
    }
    try {
        auto &state = runtime();
        std::lock_guard<std::mutex> lock(state.lifecycleMutex);
        if (!state.logger || !state.fileSink) {
            return false;
        }
        state.config.file.level = level;
        state.fileSink->set_level(toBackendLevel(level));
        state.logger->set_level(effectiveLevel(state.config));
        return true;
    } catch (const std::exception &ex) {
        emitInternalError(ex.what());
    } catch (...) {
        emitInternalError("unknown backend error while setting file level");
    }
    return false;
}

void writeText(
    ZzLogLevel level,
    std::string_view message,
    std::source_location location) noexcept
{
    if (!validLevel(level) || level == ZzLogLevel::Off) {
        return;
    }
    auto *logger = runtime().activeLogger.load(std::memory_order_acquire);
    if (logger == nullptr) {
        return;
    }
    try {
        logger->log(
            backend::source_loc{
                baseName(location.file_name()),
                location.line(),
                location.function_name()},
            toBackendLevel(level),
            backend::string_view_t(message.data(), message.size()));
    } catch (const std::exception &ex) {
        emitInternalError(ex.what());
    } catch (...) {
        emitInternalError("unknown backend error while writing a log message");
    }
}

} // namespace ZzLog
