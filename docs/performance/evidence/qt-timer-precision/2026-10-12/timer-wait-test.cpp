#include <QtCore/private/qtimerinfo_unix_p.h>
#include <chrono>
#include <cstdio>

using namespace std::chrono_literals;
struct Timers : QTimerInfoList {
    ~Timers() { clearTimers(); }
};

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char *name) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
        if (!ok) ++failures;
    };
    Timers timers;
    check(!timers.timerWait().has_value(), "empty list has no deadline");

    auto *first = new QTimerInfo(Qt::TimerId{1}, 100ms, Qt::PreciseTimer, nullptr);
    first->timeout = std::chrono::steady_clock::now() + 1h + 123456ns;
    timers.timerInsert(first);
    auto remaining = timers.timerWait();
    check(remaining && *remaining == first->timeout - timers.currentTime,
          "future deadline retains exact remaining nanoseconds");

    first->timeout = std::chrono::steady_clock::now() - 1s;
    remaining = timers.timerWait();
    check(remaining && *remaining == 0ns, "expired deadline returns zero");

    auto *second = new QTimerInfo(Qt::TimerId{2}, 200ms, Qt::PreciseTimer, nullptr);
    second->timeout = std::chrono::steady_clock::now() + 2h + 654321ns;
    timers.timerInsert(second);
    QTimerInfo *activating = first;
    first->activateRef = &activating;
    remaining = timers.timerWait();
    check(remaining && *remaining == second->timeout - timers.currentTime,
          "active first timer is skipped without rounding the next deadline");
    second->activateRef = &activating;
    check(!timers.timerWait().has_value(), "only active timers have no waiting deadline");
    return failures == 0 ? 0 : 1;
}
