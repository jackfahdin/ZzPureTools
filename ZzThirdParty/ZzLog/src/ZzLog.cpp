#include "ZzLog/ZzLog.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "fmt/format.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/async_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

#ifdef __ANDROID__
    #include "spdlog/sinks/android_sink.h"
#else
    #include "spdlog/sinks/stdout_color_sinks.h"
#endif

namespace zz::log {
namespace {

namespace backend = zzlog_spdlog;

using BackendLogger = backend::logger;
using BackendSink = backend::sinks::sink;

struct RuntimeState {
    std::mutex lifecycle_mutex;
    std::atomic<BackendLogger*> active_logger{nullptr};
    std::shared_ptr<BackendLogger> logger;
    std::shared_ptr<BackendSink> console_sink;
    std::shared_ptr<BackendSink> file_sink;
    Config config;
    std::filesystem::path file_path;
};

RuntimeState& runtime() {
    static RuntimeState state;
    return state;
}

backend::level to_backend_level(Level level) noexcept {
    switch (level) {
        case Level::trace:
            return backend::level::trace;
        case Level::debug:
            return backend::level::debug;
        case Level::info:
            return backend::level::info;
        case Level::warn:
            return backend::level::warn;
        case Level::error:
            return backend::level::err;
        case Level::critical:
            return backend::level::critical;
        case Level::off:
            return backend::level::off;
    }
    return backend::level::off;
}

bool valid_level(Level level) noexcept { return static_cast<unsigned>(level) <= static_cast<unsigned>(Level::off); }

bool valid_policy(OverflowPolicy policy) noexcept {
    return static_cast<unsigned>(policy) <= static_cast<unsigned>(OverflowPolicy::discard_new);
}

backend::sinks::async_sink::overflow_policy to_backend_policy(OverflowPolicy policy) {
    switch (policy) {
        case OverflowPolicy::block:
            return backend::sinks::async_sink::overflow_policy::block;
        case OverflowPolicy::overrun_oldest:
            return backend::sinks::async_sink::overflow_policy::overrun_oldest;
        case OverflowPolicy::discard_new:
            return backend::sinks::async_sink::overflow_policy::discard_new;
    }
    return backend::sinks::async_sink::overflow_policy::block;
}

backend::level effective_level(const Config& config) noexcept {
    auto result = backend::level::off;
    if (config.console.enabled) {
        result = std::min(result, to_backend_level(config.console.level));
    }
    if (config.file.enabled) {
        result = std::min(result, to_backend_level(config.file.level));
    }
    return result;
}

InitResult failure(InitError error, std::string message) {
    InitResult result;
    result.error = error;
    result.message = std::move(message);
    return result;
}

void emit_internal_error(std::string_view message) noexcept {
    ErrorHandler handler;
    {
        auto& state = runtime();
        std::lock_guard<std::mutex> lock(state.lifecycle_mutex);
        handler = state.config.error_handler;
    }

    if (handler) {
        try {
            handler(message);
            return;
        } catch (...) {
        }
    }

    std::fprintf(stderr, "[ZzLog error] %.*s\n", static_cast<int>(message.size()), message.data());
}

}  // namespace

InitResult initialize(Config config) {
    auto& state = runtime();
    std::lock_guard<std::mutex> lock(state.lifecycle_mutex);

    if (state.logger) {
        return failure(InitError::already_initialized, "ZzLog is already initialized");
    }
    if (config.logger_name.empty()) {
        return failure(InitError::invalid_configuration, "logger_name must not be empty");
    }
    if (!valid_level(config.console.level) || !valid_level(config.file.level) || !valid_level(config.flush_on)) {
        return failure(InitError::invalid_configuration, "configuration contains an invalid log level");
    }
    if (config.file.enabled) {
        if (config.file.path.empty()) {
            return failure(InitError::invalid_configuration, "file.path must be provided when file logging is enabled");
        }
        if (config.file.max_file_size == 0) {
            return failure(InitError::invalid_configuration, "file.max_file_size must be greater than zero");
        }
        if (config.file.max_files > 200000) {
            return failure(InitError::invalid_configuration, "file.max_files must not exceed 200000");
        }
        if (!valid_policy(config.file.overflow_policy)) {
            return failure(InitError::invalid_configuration, "file.overflow_policy is invalid");
        }
        if (config.file.async &&
            (config.file.queue_size == 0 || config.file.queue_size > backend::sinks::async_sink::max_queue_size)) {
            return failure(InitError::invalid_configuration, "file.queue_size is outside the supported range");
        }
    }

    std::filesystem::path resolved_file_path;
    if (config.file.enabled) {
        std::error_code error;
        resolved_file_path = std::filesystem::absolute(config.file.path, error).lexically_normal();
        if (error) {
            return failure(InitError::filesystem_error, "failed to resolve file.path: " + error.message());
        }
    }

    try {
        std::vector<std::shared_ptr<BackendSink>> sinks;
        std::shared_ptr<BackendSink> console_sink;
        std::shared_ptr<BackendSink> file_sink;

        if (config.console.enabled) {
#ifdef __ANDROID__
            auto console = std::make_shared<backend::sinks::android_sink_mt>(config.logger_name);
#else
            auto console = std::make_shared<backend::sinks::stdout_color_sink_mt>();
#endif
            console->set_level(to_backend_level(config.console.level));
            console->set_pattern(config.console.pattern);
            console_sink = console;
            sinks.push_back(console_sink);
        }

        if (config.file.enabled) {
            if (config.file.async) {
                auto rotating = std::make_shared<backend::sinks::rotating_file_sink_st>(
                    resolved_file_path, config.file.max_file_size, config.file.max_files, config.file.rotate_on_open);
                rotating->set_pattern(config.file.pattern);

                backend::sinks::async_sink::config async_config;
                async_config.queue_size = config.file.queue_size;
                async_config.policy = to_backend_policy(config.file.overflow_policy);
                async_config.sinks.push_back(std::move(rotating));
                auto async_file = std::make_shared<backend::sinks::async_sink>(std::move(async_config));
                async_file->set_level(to_backend_level(config.file.level));
                file_sink = std::move(async_file);
            } else {
                auto rotating = std::make_shared<backend::sinks::rotating_file_sink_mt>(
                    resolved_file_path, config.file.max_file_size, config.file.max_files, config.file.rotate_on_open);
                rotating->set_level(to_backend_level(config.file.level));
                rotating->set_pattern(config.file.pattern);
                file_sink = std::move(rotating);
            }
            sinks.push_back(file_sink);
        }

        auto logger = std::make_shared<BackendLogger>(config.logger_name, sinks.begin(), sinks.end());
        logger->set_level(effective_level(config));
        logger->flush_on(to_backend_level(config.flush_on));
        if (config.error_handler) {
            logger->set_error_handler([handler = config.error_handler](const std::string& message) { handler(message); });
        }

        state.config = std::move(config);
        state.file_path = resolved_file_path;
        state.console_sink = std::move(console_sink);
        state.file_sink = std::move(file_sink);
        state.logger = std::move(logger);
        state.active_logger.store(state.logger.get(), std::memory_order_release);

        InitResult result;
        result.file_path = resolved_file_path;
        return result;
    } catch (const backend::spdlog_ex& ex) {
        return failure(InitError::backend_error, ex.what());
    } catch (const std::filesystem::filesystem_error& ex) {
        return failure(InitError::filesystem_error, ex.what());
    } catch (const std::exception& ex) {
        return failure(InitError::backend_error, ex.what());
    } catch (...) {
        return failure(InitError::backend_error, "unknown error while initializing ZzLog");
    }
}

void shutdown() noexcept {
    auto& state = runtime();
    std::shared_ptr<BackendLogger> logger;
    std::shared_ptr<BackendSink> console_sink;
    std::shared_ptr<BackendSink> file_sink;
    {
        std::lock_guard<std::mutex> lock(state.lifecycle_mutex);
        state.active_logger.store(nullptr, std::memory_order_release);
        logger = std::move(state.logger);
        console_sink = std::move(state.console_sink);
        file_sink = std::move(state.file_sink);
        state.config = Config{};
        state.file_path.clear();
    }

    if (logger) {
        logger->flush();
    }
    logger.reset();
    file_sink.reset();
    console_sink.reset();
}

void flush() noexcept {
    auto* logger = runtime().active_logger.load(std::memory_order_acquire);
    if (logger != nullptr) {
        logger->flush();
    }
}

bool is_initialized() noexcept { return runtime().active_logger.load(std::memory_order_acquire) != nullptr; }

bool should_log(Level level) noexcept {
    if (!valid_level(level) || level == Level::off) {
        return false;
    }
    auto* logger = runtime().active_logger.load(std::memory_order_acquire);
    return logger != nullptr && logger->should_log(to_backend_level(level));
}

std::filesystem::path active_file_path() {
    auto& state = runtime();
    std::lock_guard<std::mutex> lock(state.lifecycle_mutex);
    return state.file_path;
}

bool set_console_level(Level level) noexcept {
    if (!valid_level(level)) {
        return false;
    }
    auto& state = runtime();
    std::lock_guard<std::mutex> lock(state.lifecycle_mutex);
    if (!state.logger || !state.console_sink) {
        return false;
    }
    state.config.console.level = level;
    state.console_sink->set_level(to_backend_level(level));
    state.logger->set_level(effective_level(state.config));
    return true;
}

bool set_file_level(Level level) noexcept {
    if (!valid_level(level)) {
        return false;
    }
    auto& state = runtime();
    std::lock_guard<std::mutex> lock(state.lifecycle_mutex);
    if (!state.logger || !state.file_sink) {
        return false;
    }
    state.config.file.level = level;
    state.file_sink->set_level(to_backend_level(level));
    state.logger->set_level(effective_level(state.config));
    return true;
}

void log_text(Level level, std::string_view message) noexcept {
    if (!valid_level(level) || level == Level::off) {
        return;
    }
    auto* logger = runtime().active_logger.load(std::memory_order_acquire);
    if (logger == nullptr) {
        return;
    }
    logger->log(to_backend_level(level), backend::string_view_t(message.data(), message.size()));
}

namespace detail {

void log_format(Level level, fmt::string_view format, fmt::format_args args) noexcept {
    try {
        fmt::basic_memory_buffer<char, 256> buffer;
        fmt::vformat_to(std::back_inserter(buffer), format, args);
        log_text(level, std::string_view(buffer.data(), buffer.size()));
    } catch (const std::exception& ex) {
        emit_internal_error(ex.what());
    } catch (...) {
        emit_internal_error("unknown formatting error");
    }
}

}  // namespace detail
}  // namespace zz::log
