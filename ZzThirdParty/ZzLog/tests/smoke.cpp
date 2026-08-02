#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "ZzLog/ZzLog.h"

int main() {
    namespace log = zz::log;

    const log::Config defaults;
    assert(defaults.console.enabled);
    assert(defaults.console.level == log::Level::info);
    assert(!defaults.file.enabled);

    log::Config invalid_file;
    invalid_file.console.enabled = false;
    invalid_file.file.enabled = true;
    auto result = log::initialize(invalid_file);
    assert(!result);
    assert(result.error == log::InitError::invalid_configuration);
    assert(!log::is_initialized());

    log::Config silent;
    silent.console.enabled = false;
    result = log::initialize(silent);
    assert(result);
    assert(log::is_initialized());
    assert(!log::should_log(log::Level::critical));
    log::shutdown();

    const auto file_path = std::filesystem::current_path() / "zzlog-smoke.log";
    std::error_code error;
    std::filesystem::remove(file_path, error);

    log::Config file_config;
    file_config.console.enabled = false;
    file_config.file.enabled = true;
    file_config.file.path = file_path;
    file_config.file.level = log::Level::debug;
    file_config.file.pattern = "%v";

    result = log::initialize(file_config);
    assert(result);
    assert(result.file_path == file_path.lexically_normal());
    assert(log::active_file_path() == result.file_path);
    assert(log::should_log(log::Level::debug));

    log::debug("value={}", 42);
    log::log_text(log::Level::info, "message without formatting");

    constexpr int thread_count = 4;
    constexpr int messages_per_thread = 100;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < thread_count; ++thread) {
        threads.emplace_back([thread] {
            for (int message = 0; message < messages_per_thread; ++message) {
                log::debug("thread={} message={}", thread, message);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    assert(log::set_file_level(log::Level::warn));
    assert(!log::should_log(log::Level::info));
    log::error_text("runtime {error} text");
    log::shutdown();
    log::shutdown();

    std::ifstream input(file_path);
    const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    assert(contents.find("value=42") != std::string::npos);
    assert(contents.find("message without formatting") != std::string::npos);
    assert(contents.find("runtime {error} text") != std::string::npos);
    const auto line_count = static_cast<std::size_t>(std::count(contents.begin(), contents.end(), '\n'));
    assert(line_count == 3 + thread_count * messages_per_thread);

    std::filesystem::remove(file_path, error);
    return 0;
}
