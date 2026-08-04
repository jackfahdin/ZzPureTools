#include <chrono>
#include <cstdio>
#include <filesystem>
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

    const auto path =
        std::filesystem::current_path() / "zzlog-overflow.log";
    std::error_code error;
    std::filesystem::remove(path, error);

    log::ZzLogConfig config;
    config.console.enabled = false;
    config.file.enabled = true;
    config.file.path = path;
    config.file.level = log::ZzLogLevel::Info;
    config.file.pattern = "%v";
    config.file.async = true;
    config.file.queueSize = 1;
    config.file.overflowPolicy = log::ZzLogOverflowPolicy::DiscardNew;

    const auto initialized = log::initialize(config);
    ZZ_TEST_CHECK(initialized);

    constexpr int threadCount = 8;
    constexpr int messagesPerThread = 4096;
    const std::string payload(4096, 'x');
    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int thread = 0; thread < threadCount; ++thread) {
        threads.emplace_back([thread, &payload] {
            for (int message = 0; message < messagesPerThread; ++message) {
                log::write(
                    log::ZzLogLevel::Info,
                    std::source_location::current(),
                    "thread={} message={} {}",
                    thread,
                    message,
                    payload);
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    const auto droppedBeforeFlush = log::droppedMessageCount();
    ZZ_TEST_CHECK(droppedBeforeFlush > 0);
    ZZ_TEST_CHECK(
        droppedBeforeFlush
        <= static_cast<std::uint64_t>(threadCount * messagesPerThread));
    ZZ_TEST_CHECK(log::flushAndWait(std::chrono::seconds(10)));
    ZZ_TEST_CHECK(log::droppedMessageCount() == droppedBeforeFlush);

    log::shutdown();
    std::filesystem::remove(path, error);
    return 0;
}
