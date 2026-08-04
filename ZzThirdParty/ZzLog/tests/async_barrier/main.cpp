#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <spdlog/details/log_msg.h>
#include <spdlog/formatter.h>
#include <spdlog/sinks/async_sink.h>
#include <spdlog/sinks/sink.h>

#define ZZ_TEST_CHECK(condition)                                              \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(                                                     \
                stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
            return __LINE__;                                                  \
        }                                                                     \
    } while (false)

namespace {

namespace backend = zzlog_spdlog;

class ZzBlockingSink final : public backend::sinks::sink
{
public:
    void log(const backend::details::log_msg &) override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++logCount_;
        logEntered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this] { return !blockLog_; });
    }

    void flush() override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++flushCount_;
        flushEntered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this] { return !blockFlush_; });
    }

    void set_pattern(const std::string &) override
    {
    }

    void set_formatter(std::unique_ptr<backend::formatter>) override
    {
    }

    void blockLog()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        blockLog_ = true;
        logEntered_ = false;
    }

    void waitForLogEntry()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return logEntered_; });
    }

    void releaseLog()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        blockLog_ = false;
        condition_.notify_all();
    }

    void blockFlush()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        blockFlush_ = true;
        flushEntered_ = false;
    }

    void waitForFlushEntry()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return flushEntered_; });
    }

    void releaseFlush()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        blockFlush_ = false;
        condition_.notify_all();
    }

    [[nodiscard]] std::size_t flushCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return flushCount_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool blockLog_{false};
    bool logEntered_{false};
    bool blockFlush_{false};
    bool flushEntered_{false};
    std::size_t logCount_{0};
    std::size_t flushCount_{0};
};

} // namespace

int main()
{
    using namespace std::chrono_literals;

    {
        auto backendSink = std::make_shared<ZzBlockingSink>();
        backend::sinks::async_sink::config config;
        config.queue_size = 4;
        config.sinks.push_back(backendSink);
        backend::sinks::async_sink asyncSink(std::move(config));

        backendSink->blockFlush();
        asyncSink.flush();
        backendSink->waitForFlushEntry();
        ZZ_TEST_CHECK(!asyncSink.flush_and_wait(20ms));
        backendSink->releaseFlush();
        ZZ_TEST_CHECK(asyncSink.flush_and_wait(5s));
        ZZ_TEST_CHECK(backendSink->flushCount() == 3);
        ZZ_TEST_CHECK(!asyncSink.flush_and_wait(-1ms));
    }

    {
        auto backendSink = std::make_shared<ZzBlockingSink>();
        backend::sinks::async_sink::config config;
        config.queue_size = 1;
        config.policy =
            backend::sinks::async_sink::overflow_policy::discard_new;
        config.sinks.push_back(backendSink);
        backend::sinks::async_sink asyncSink(std::move(config));

        const backend::details::log_msg message(
            "barrier-test",
            backend::level::info,
            "payload");
        backendSink->blockLog();
        asyncSink.log(message);
        backendSink->waitForLogEntry();
        asyncSink.log(message);
        const auto discardedBeforeBarrier = asyncSink.get_discard_counter();

        std::mutex startMutex;
        std::condition_variable startCondition;
        bool started = false;
        bool barrierCompleted = false;
        std::thread barrierThread([&] {
            {
                std::lock_guard<std::mutex> lock(startMutex);
                started = true;
                startCondition.notify_one();
            }
            barrierCompleted = asyncSink.flush_and_wait(5s);
        });
        {
            std::unique_lock<std::mutex> lock(startMutex);
            startCondition.wait(lock, [&started] { return started; });
        }

        backendSink->releaseLog();
        barrierThread.join();
        ZZ_TEST_CHECK(barrierCompleted);
        ZZ_TEST_CHECK(
            asyncSink.get_discard_counter() == discardedBeforeBarrier);
        ZZ_TEST_CHECK(backendSink->flushCount() == 1);
    }

    return 0;
}
