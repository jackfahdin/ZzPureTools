#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32) && defined(ZZLOG_SHARED)
    #if defined(ZZLOG_BUILDING_LIBRARY)
        #define ZZLOG_API __declspec(dllexport)
    #else
        #define ZZLOG_API __declspec(dllimport)
    #endif
#elif defined(ZZLOG_SHARED) && (defined(__GNUC__) || defined(__clang__))
    #define ZZLOG_API __attribute__((visibility("default")))
#else
    #define ZZLOG_API
#endif

namespace ZzLog {

enum class ZzLogLevel : std::uint8_t
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

enum class ZzLogOverflowPolicy : std::uint8_t
{
    Block,
    OverrunOldest,
    DiscardNew
};

enum class ZzLogInitError : std::uint8_t
{
    None,
    AlreadyInitialized,
    InvalidConfiguration,
    FilesystemError,
    BackendError
};

struct ZzLogConsoleOptions final
{
    bool enabled = true;
    ZzLogLevel level = ZzLogLevel::Info;
    std::string pattern = "[%T] [%^%l%$] %v";
};

struct ZzLogFileOptions final
{
    bool enabled = false;
    std::filesystem::path path;
    ZzLogLevel level = ZzLogLevel::Info;
    std::string pattern = "[%Y-%m-%d %T.%e] [%l] [%t] %v";
    bool async = true;
    std::size_t queueSize = 8192;
    ZzLogOverflowPolicy overflowPolicy = ZzLogOverflowPolicy::OverrunOldest;
    std::size_t maxFileSize = 10 * 1024 * 1024;
    std::size_t maxFiles = 5;
    bool rotateOnOpen = false;
};

using ZzLogErrorHandler = std::function<void(std::string_view)>;

struct ZzLogConfig final
{
    std::string loggerName = "ZzLog";
    ZzLogConsoleOptions console;
    ZzLogFileOptions file;
    ZzLogLevel flushOn = ZzLogLevel::Error;
    ZzLogErrorHandler errorHandler;
};

struct ZzLogInitResult final
{
    ZzLogInitError error = ZzLogInitError::None;
    std::string message;
    std::filesystem::path filePath;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ZzLogInitError::None;
    }
};

[[nodiscard]] ZZLOG_API ZzLogInitResult initialize(ZzLogConfig config = {});
ZZLOG_API void shutdown() noexcept;
ZZLOG_API void flush() noexcept;

[[nodiscard]] ZZLOG_API bool flushAndWait(
    std::chrono::milliseconds timeout) noexcept;

[[nodiscard]] ZZLOG_API bool isInitialized() noexcept;
[[nodiscard]] ZZLOG_API bool shouldLog(ZzLogLevel level) noexcept;
[[nodiscard]] ZZLOG_API std::filesystem::path activeFilePath();
[[nodiscard]] ZZLOG_API std::uint64_t droppedMessageCount() noexcept;

[[nodiscard]] ZZLOG_API bool setConsoleLevel(ZzLogLevel level) noexcept;
[[nodiscard]] ZZLOG_API bool setFileLevel(ZzLogLevel level) noexcept;

ZZLOG_API void writeText(
    ZzLogLevel level,
    std::string_view message,
    std::source_location location = std::source_location::current()) noexcept;

template<typename... ZzArgs>
void write(
    ZzLogLevel level,
    std::source_location location,
    std::format_string<ZzArgs...> format,
    ZzArgs &&...args) noexcept
{
    if (!shouldLog(level)) {
        return;
    }

    try {
        writeText(
            level,
            std::format(format, std::forward<ZzArgs>(args)...),
            location);
    } catch (...) {
        writeText(
            ZzLogLevel::Error,
            "ZzLog formatting failed",
            location);
    }
}

} // namespace ZzLog

#define ZZ_LOG_TRACE(format, ...)                                            \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Trace, std::source_location::current(),         \
        format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_DEBUG(format, ...)                                            \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Debug, std::source_location::current(),         \
        format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_INFO(format, ...)                                             \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Info, std::source_location::current(),          \
        format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_WARNING(format, ...)                                          \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Warning, std::source_location::current(),       \
        format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_ERROR(format, ...)                                            \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Error, std::source_location::current(),         \
        format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_CRITICAL(format, ...)                                         \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Critical, std::source_location::current(),      \
        format __VA_OPT__(,) __VA_ARGS__)
