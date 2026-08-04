#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <ZzLog/ZzLog.h>

#define ZZ_TEST_CHECK(condition)                                              \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(                                                     \
                stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                  \
        }                                                                     \
    } while (false)

int main()
{
    namespace log = ZzLog;

    const log::ZzLogConfig defaults;
    ZZ_TEST_CHECK(defaults.console.enabled);
    ZZ_TEST_CHECK(defaults.console.level == log::ZzLogLevel::Info);
    ZZ_TEST_CHECK(defaults.file.overflowPolicy
                  == log::ZzLogOverflowPolicy::OverrunOldest);
    ZZ_TEST_CHECK(!defaults.file.enabled);

    log::ZzLogConfig invalidFile;
    invalidFile.console.enabled = false;
    invalidFile.file.enabled = true;
    auto result = log::initialize(invalidFile);
    ZZ_TEST_CHECK(!result);
    ZZ_TEST_CHECK(result.error == log::ZzLogInitError::InvalidConfiguration);
    ZZ_TEST_CHECK(!log::isInitialized());

    log::ZzLogConfig silent;
    silent.console.enabled = false;
    result = log::initialize(silent);
    ZZ_TEST_CHECK(result);
    ZZ_TEST_CHECK(log::isInitialized());
    ZZ_TEST_CHECK(!log::shouldLog(log::ZzLogLevel::Critical));
    log::shutdown();

    const auto filePath =
        std::filesystem::current_path() / "zzlog-smoke.log";
    std::error_code error;
    std::filesystem::remove(filePath, error);

    log::ZzLogConfig fileConfig;
    fileConfig.console.enabled = false;
    fileConfig.file.enabled = true;
    fileConfig.file.path = filePath;
    fileConfig.file.level = log::ZzLogLevel::Debug;
    fileConfig.file.pattern = "%v";

    result = log::initialize(fileConfig);
    ZZ_TEST_CHECK(result);
    ZZ_TEST_CHECK(result.filePath == filePath.lexically_normal());
    ZZ_TEST_CHECK(log::activeFilePath() == result.filePath);
    ZZ_TEST_CHECK(log::shouldLog(log::ZzLogLevel::Debug));

    ZZ_LOG_DEBUG("value={}", 42);
    log::writeText(log::ZzLogLevel::Info, "message without formatting");

    constexpr int threadCount = 4;
    constexpr int messagesPerThread = 100;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < threadCount; ++thread) {
        threads.emplace_back([thread] {
            for (int message = 0; message < messagesPerThread; ++message) {
                ZZ_LOG_DEBUG("thread={} message={}", thread, message);
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    ZZ_TEST_CHECK(log::flushAndWait(std::chrono::seconds(5)));
    ZZ_TEST_CHECK(log::droppedMessageCount() == 0);
    {
        std::ifstream flushedInput(filePath);
        const std::string flushedContents{
            std::istreambuf_iterator<char>(flushedInput),
            std::istreambuf_iterator<char>()};
        ZZ_TEST_CHECK(
            flushedContents.find("message without formatting")
            != std::string::npos);
    }

    ZZ_TEST_CHECK(log::setFileLevel(log::ZzLogLevel::Warning));
    ZZ_TEST_CHECK(!log::shouldLog(log::ZzLogLevel::Info));
    log::writeText(log::ZzLogLevel::Error, "runtime {error} text");
    log::shutdown();
    log::shutdown();
    ZZ_TEST_CHECK(log::droppedMessageCount() == 0);
    ZZ_TEST_CHECK(log::flushAndWait(std::chrono::milliseconds(10)));

    std::ifstream input(filePath);
    const std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    ZZ_TEST_CHECK(contents.find("value=42") != std::string::npos);
    ZZ_TEST_CHECK(
        contents.find("message without formatting") != std::string::npos);
    ZZ_TEST_CHECK(contents.find("runtime {error} text") != std::string::npos);
    const auto lineCount = static_cast<std::size_t>(
        std::count(contents.begin(), contents.end(), '\n'));
    ZZ_TEST_CHECK(lineCount == 3 + threadCount * messagesPerThread);

    std::filesystem::remove(filePath, error);
    return 0;
}
