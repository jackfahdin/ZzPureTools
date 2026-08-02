#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "fmt/base.h"

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

namespace zz::log {

enum class Level : std::uint8_t { trace = 0, debug = 1, info = 2, warn = 3, error = 4, critical = 5, off = 6 };

enum class OverflowPolicy : std::uint8_t { block, overrun_oldest, discard_new };

enum class InitError : std::uint8_t { none, already_initialized, invalid_configuration, filesystem_error, backend_error };

struct ConsoleOptions {
    bool enabled = true;
    Level level = Level::info;
    std::string pattern = "[%T] [%^%l%$] %v";
};

struct FileOptions {
    bool enabled = false;
    std::filesystem::path path;
    Level level = Level::info;
    std::string pattern = "[%Y-%m-%d %T.%e] [%l] [%t] %v";
    bool async = true;
    std::size_t queue_size = 8192;
    OverflowPolicy overflow_policy = OverflowPolicy::block;
    std::size_t max_file_size = 10 * 1024 * 1024;
    std::size_t max_files = 5;
    bool rotate_on_open = false;
};

using ErrorHandler = std::function<void(std::string_view)>;

struct Config {
    std::string logger_name = "ZzLog";
    ConsoleOptions console;
    FileOptions file;
    Level flush_on = Level::error;
    ErrorHandler error_handler;
};

struct InitResult {
    InitError error = InitError::none;
    std::string message;
    std::filesystem::path file_path;

    [[nodiscard]] explicit operator bool() const noexcept { return error == InitError::none; }
};

// initialize() and shutdown() are lifecycle operations. Call them while no other
// thread is logging. Normal log calls and level changes are thread-safe.
[[nodiscard]] ZZLOG_API InitResult initialize(Config config = {});
ZZLOG_API void shutdown() noexcept;
ZZLOG_API void flush() noexcept;

[[nodiscard]] ZZLOG_API bool is_initialized() noexcept;
[[nodiscard]] ZZLOG_API bool should_log(Level level) noexcept;
[[nodiscard]] ZZLOG_API std::filesystem::path active_file_path();

[[nodiscard]] ZZLOG_API bool set_console_level(Level level) noexcept;
[[nodiscard]] ZZLOG_API bool set_file_level(Level level) noexcept;

ZZLOG_API void log_text(Level level, std::string_view message) noexcept;

namespace detail {

ZZLOG_API void log_format(Level level, fmt::string_view format, fmt::format_args args) noexcept;

template <typename... Args>
void log(Level level, fmt::format_string<Args...> format, Args&&... args) noexcept {
    auto format_args = fmt::make_format_args(args...);
    log_format(level, format.get(), format_args);
}

}  // namespace detail

template <typename... Args>
void write(Level level, fmt::format_string<Args...> format, Args&&... args) noexcept {
    if (should_log(level)) {
        detail::log(level, format, std::forward<Args>(args)...);
    }
}

template <typename... Args>
void trace(fmt::format_string<Args...> format, Args&&... args) noexcept {
    write(Level::trace, format, std::forward<Args>(args)...);
}

template <typename... Args>
void debug(fmt::format_string<Args...> format, Args&&... args) noexcept {
    write(Level::debug, format, std::forward<Args>(args)...);
}

template <typename... Args>
void info(fmt::format_string<Args...> format, Args&&... args) noexcept {
    write(Level::info, format, std::forward<Args>(args)...);
}

template <typename... Args>
void warn(fmt::format_string<Args...> format, Args&&... args) noexcept {
    write(Level::warn, format, std::forward<Args>(args)...);
}

template <typename... Args>
void error(fmt::format_string<Args...> format, Args&&... args) noexcept {
    write(Level::error, format, std::forward<Args>(args)...);
}

template <typename... Args>
void critical(fmt::format_string<Args...> format, Args&&... args) noexcept {
    write(Level::critical, format, std::forward<Args>(args)...);
}

inline void trace_text(std::string_view message) noexcept { log_text(Level::trace, message); }
inline void debug_text(std::string_view message) noexcept { log_text(Level::debug, message); }
inline void info_text(std::string_view message) noexcept { log_text(Level::info, message); }
inline void warn_text(std::string_view message) noexcept { log_text(Level::warn, message); }
inline void error_text(std::string_view message) noexcept { log_text(Level::error, message); }
inline void critical_text(std::string_view message) noexcept { log_text(Level::critical, message); }

}  // namespace zz::log
