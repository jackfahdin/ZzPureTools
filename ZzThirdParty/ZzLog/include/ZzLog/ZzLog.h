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

/**
 * @brief 日志等级。
 */
enum class ZzLogLevel : std::uint8_t
{
    /** @brief 最细粒度的跟踪信息。 */
    Trace = 0,
    /** @brief 调试信息。 */
    Debug = 1,
    /** @brief 常规运行信息。 */
    Info = 2,
    /** @brief 不影响当前流程的警告。 */
    Warning = 3,
    /** @brief 可恢复的错误。 */
    Error = 4,
    /** @brief 导致功能不可继续的严重错误。 */
    Critical = 5,
    /** @brief 关闭日志输出。 */
    Off = 6
};

/**
 * @brief 异步队列满载时的数据消息处理策略。
 */
enum class ZzLogOverflowPolicy : std::uint8_t
{
    /** @brief 阻塞生产线程，直到队列出现空位。 */
    Block,
    /** @brief 覆盖队列中最早的数据消息。 */
    OverrunOldest,
    /** @brief 丢弃当前准备写入的新数据消息。 */
    DiscardNew
};

/**
 * @brief 日志系统初始化错误。
 */
enum class ZzLogInitError : std::uint8_t
{
    /** @brief 初始化成功。 */
    None,
    /** @brief 日志系统已经初始化。 */
    AlreadyInitialized,
    /** @brief 配置字段不满足约束。 */
    InvalidConfiguration,
    /** @brief 日志路径解析或文件系统操作失败。 */
    FilesystemError,
    /** @brief 日志后端创建失败。 */
    BackendError
};

/**
 * @brief 控制台日志配置。
 */
struct ZzLogConsoleOptions final
{
    /** @brief 是否启用控制台日志。 */
    bool enabled = true;

    /** @brief 控制台接收的最低日志等级。 */
    ZzLogLevel level = ZzLogLevel::Info;

    /** @brief 控制台输出格式；使用 ZzLog 后端支持的 pattern 语法。 */
    std::string pattern = "[%T] [%^%l%$] %v";
};

/**
 * @brief 文件日志配置。
 */
struct ZzLogFileOptions final
{
    /** @brief 是否启用文件日志。 */
    bool enabled = false;

    /** @brief 日志文件路径；启用文件日志时不能为空。 */
    std::filesystem::path path;

    /** @brief 文件接收的最低日志等级。 */
    ZzLogLevel level = ZzLogLevel::Info;

    /** @brief 文件输出格式；使用 ZzLog 后端支持的 pattern 语法。 */
    std::string pattern = "[%Y-%m-%d %T.%e] [%l] [%t] %v";

    /** @brief 是否在专用工作线程中写入文件。 */
    bool async = true;

    /** @brief 异步队列容量，合法范围为 1 到 250000。 */
    std::size_t queueSize = 8192;

    /** @brief 异步队列满载时的数据消息处理策略。 */
    ZzLogOverflowPolicy overflowPolicy = ZzLogOverflowPolicy::OverrunOldest;

    /** @brief 单个轮转文件的最大字节数，必须大于 0。 */
    std::size_t maxFileSize = std::size_t{10} * 1024 * 1024;

    /** @brief 保留的轮转文件数量，合法范围为 0 到 200000。 */
    std::size_t maxFiles = 5;

    /** @brief 是否在打开现有文件时立即执行一次轮转。 */
    bool rotateOnOpen = false;
};

/**
 * @brief 内部错误回调。
 *
 * 回调参数仅在调用期间有效。回调不得抛出异常；即使抛出，ZzLog 也会吞掉异常并
 * 回退到标准错误输出。
 */
using ZzLogErrorHandler = std::function<void(std::string_view)>;

/**
 * @brief 日志系统配置。
 */
struct ZzLogConfig final
{
    /** @brief 后端日志器名称，不能为空。 */
    std::string loggerName = "ZzLog";

    /** @brief 控制台日志配置。 */
    ZzLogConsoleOptions console;

    /** @brief 文件日志配置。 */
    ZzLogFileOptions file;

    /** @brief 达到该等级时请求后端刷新；Off 表示关闭按等级刷新。 */
    ZzLogLevel flushOn = ZzLogLevel::Error;

    /** @brief 可选的内部错误回调。 */
    ZzLogErrorHandler errorHandler;
};

/**
 * @brief 日志系统初始化结果。
 */
struct ZzLogInitResult final
{
    /** @brief 错误码；None 表示成功。 */
    ZzLogInitError error = ZzLogInitError::None;

    /** @brief 供诊断使用的错误详情；成功时为空。 */
    std::string message;

    /** @brief 后端实际使用的规范化绝对文件路径；未启用文件日志时为空。 */
    std::filesystem::path filePath;

    /**
     * @brief 判断初始化是否成功。
     * @return 成功返回 true，否则返回 false。
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ZzLogInitError::None;
    }
};

/**
 * @brief 初始化进程内唯一的日志运行时。
 * @param config 日志配置，按值传入以便运行时持有独立副本。
 * @return 初始化结果以及实际文件路径。
 *
 * 只能由应用生命周期线程调用。调用期间，其他线程不得进入任何 ZzLog API。
 */
[[nodiscard]] ZZLOG_API ZzLogInitResult initialize(ZzLogConfig config = {});

/**
 * @brief 关闭日志运行时并尽力完成已接收消息的刷新。
 *
 * 本函数幂等且不抛异常。只能由应用生命周期线程调用；调用期间，其他线程不得进入
 * 任何 ZzLog API。
 */
ZZLOG_API void shutdown() noexcept;

/**
 * @brief 请求所有已配置 sink 刷新并立即返回。
 *
 * 异步文件 sink 的实际刷新在工作线程中完成；需要完成语义时使用 flushAndWait()。
 * 本函数可与普通写入、等级修改和观测 API 并发调用。
 */
ZZLOG_API void flush() noexcept;

/**
 * @brief 在指定时限内等待同步刷新和异步后端刷新完成。
 * @param timeout 最大等待时间；负值立即失败，零值只执行非阻塞检查。
 * @return 调用前已接收消息完成后端刷新时返回 true，超时或后端失败时返回 false。
 *
 * 等待期间不持有生命周期互斥，可与普通写入、等级修改和观测 API 并发调用。
 */
[[nodiscard]] ZZLOG_API bool flushAndWait(
    std::chrono::milliseconds timeout) noexcept;

/**
 * @brief 查询日志运行时是否已初始化。
 * @return 已初始化返回 true，否则返回 false。
 */
[[nodiscard]] ZZLOG_API bool isInitialized() noexcept;

/**
 * @brief 查询指定等级当前是否会被任一输出接收。
 * @param level 待查询的日志等级。
 * @return 会被接收返回 true，否则返回 false。
 */
[[nodiscard]] ZZLOG_API bool shouldLog(ZzLogLevel level) noexcept;

/**
 * @brief 获取后端实际使用的日志文件路径。
 * @return 规范化绝对路径；未启用文件日志或未初始化时返回空路径。
 */
[[nodiscard]] ZZLOG_API std::filesystem::path activeFilePath();

/**
 * @brief 获取当前运行时累计丢弃的数据消息数。
 * @return overrun 与 discard 计数之和；无异步文件 sink 时返回 0。
 *
 * flush 和 terminate 控制消息永远不计入该数值。重新初始化会建立新的计数周期。
 */
[[nodiscard]] ZZLOG_API std::uint64_t droppedMessageCount() noexcept;

/**
 * @brief 修改控制台最低日志等级。
 * @param level 新的最低等级。
 * @return 控制台 sink 存在且修改成功时返回 true，否则返回 false。
 */
[[nodiscard]] ZZLOG_API bool setConsoleLevel(ZzLogLevel level) noexcept;

/**
 * @brief 修改文件最低日志等级。
 * @param level 新的最低等级。
 * @return 文件 sink 存在且修改成功时返回 true，否则返回 false。
 */
[[nodiscard]] ZZLOG_API bool setFileLevel(ZzLogLevel level) noexcept;

/**
 * @brief 写入已经格式化或来自运行时的原始文本。
 * @param level 日志等级。
 * @param message 文本视图；内容在函数返回前被后端复制或消费。
 * @param location 调用来源，默认捕获直接调用 writeText() 的位置。
 *
 * 本函数线程安全且不抛异常。文本中的花括号不会被再次解释。
 */
ZZLOG_API void writeText(
    ZzLogLevel level,
    std::string_view message,
    std::source_location location = std::source_location::current()) noexcept;

/**
 * @brief 使用 C++20 编译期检查的格式串写入日志。
 * @tparam ZzArgs 格式参数类型。
 * @param level 日志等级。
 * @param location 调用来源，通常传入 std::source_location::current()。
 * @param format C++20 格式串。
 * @param args 与格式占位符对应的参数。
 *
 * 当该等级被过滤时不会执行格式化。格式化或后端异常不会越过本函数。
 */
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

/** @brief 记录 Trace 等级的类型安全格式化日志。 */
#define ZZ_LOG_TRACE(format, ...)                                            \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Trace, std::source_location::current(),         \
        format __VA_OPT__(,) __VA_ARGS__)
/** @brief 记录 Debug 等级的类型安全格式化日志。 */
#define ZZ_LOG_DEBUG(format, ...)                                            \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Debug, std::source_location::current(),         \
        format __VA_OPT__(,) __VA_ARGS__)
/** @brief 记录 Info 等级的类型安全格式化日志。 */
#define ZZ_LOG_INFO(format, ...)                                             \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Info, std::source_location::current(),          \
        format __VA_OPT__(,) __VA_ARGS__)
/** @brief 记录 Warning 等级的类型安全格式化日志。 */
#define ZZ_LOG_WARNING(format, ...)                                          \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Warning, std::source_location::current(),       \
        format __VA_OPT__(,) __VA_ARGS__)
/** @brief 记录 Error 等级的类型安全格式化日志。 */
#define ZZ_LOG_ERROR(format, ...)                                            \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Error, std::source_location::current(),         \
        format __VA_OPT__(,) __VA_ARGS__)
/** @brief 记录 Critical 等级的类型安全格式化日志。 */
#define ZZ_LOG_CRITICAL(format, ...)                                         \
    ::ZzLog::write(                                                          \
        ::ZzLog::ZzLogLevel::Critical, std::source_location::current(),      \
        format __VA_OPT__(,) __VA_ARGS__)
