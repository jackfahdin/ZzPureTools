// Appended to the real Qt GLib dispatcher source by this diagnostic project.
#include <cstdio>

struct TimerSources {
    GTimerSource *normal = reinterpret_cast<GTimerSource *>(
        g_source_new(&timerSourceFuncs, sizeof(GTimerSource)));
    GIdleTimerSource *idle = reinterpret_cast<GIdleTimerSource *>(
        g_source_new(&idleTimerSourceFuncs, sizeof(GIdleTimerSource)));
    TimerSources()
    {
        new (&normal->timerList) QTimerInfoList;
        normal->processEventsFlags = QEventLoop::AllEvents;
        normal->runWithIdlePriority = false;
        idle->timerSource = normal;
    }
    ~TimerSources()
    {
        normal->timerList.clearTimers();
        normal->timerList.~QTimerInfoList();
        g_source_unref(&idle->source);
        g_source_unref(&normal->source);
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
        if (!ok) ++failures;
    };
    TimerSources fixture;
    auto *normal = fixture.normal;
    auto *idle = fixture.idle;
    gint timeout = 42;
    check(!timerSourcePrepare(&normal->source, &timeout) && timeout == -1
          && g_source_get_ready_time(&normal->source) == -1,
          "empty timer list does not schedule a wakeup");

    auto *timer = new QTimerInfo(Qt::TimerId{1}, 100ms, Qt::PreciseTimer, nullptr);
    timer->timeout = steady_clock::now() + 1h + 123456ns;
    normal->timerList.timerInsert(timer);
    const auto before = g_get_monotonic_time();
    const bool ready = timerSourcePrepare(&normal->source, &timeout);
    const auto after = g_get_monotonic_time();
    const auto exact = ceil<microseconds>(timer->timeout - normal->timerList.currentTime).count();
    const auto deadline = g_source_get_ready_time(&normal->source);
    check(!ready && timeout == -1 && deadline >= before + exact && deadline <= after + exact,
          "future timer publishes a microsecond deadline without millisecond rounding");
    bool stable = deadline > 0;
    for (int i = 0; i != 20; ++i) {
        timerSourcePrepare(&normal->source, &timeout);
        stable = stable && g_source_get_ready_time(&normal->source) == deadline;
    }
    check(stable, "unchanged Qt deadline keeps the same GLib ready time");
    auto *earlier = new QTimerInfo(Qt::TimerId{2}, 100ms, Qt::PreciseTimer, nullptr);
    earlier->timeout = timer->timeout - 30min;
    normal->timerList.timerInsert(earlier);
    timerSourcePrepare(&normal->source, &timeout);
    check(g_source_get_ready_time(&normal->source) > 0
          && g_source_get_ready_time(&normal->source) < deadline - 29 * 60 * G_USEC_PER_SEC,
          "inserting an earlier timer updates the armed deadline");
    normal->timerList.unregisterTimer(Qt::TimerId{2});
    timerSourcePrepare(&normal->source, &timeout);
    check(g_source_get_ready_time(&normal->source) > 0
          && g_source_get_ready_time(&normal->source) > deadline - G_USEC_PER_SEC,
          "removing the earliest timer restores the remaining deadline");

    // A stale ready time must not bypass Qt's priority or timer exclusion checks.
    g_source_set_ready_time(&normal->source, 0);
    normal->runWithIdlePriority = true;
    check(!timerSourcePrepare(&normal->source, &timeout) && timeout == -1
          && g_source_get_ready_time(&normal->source) == -1,
          "normal source clears its deadline when yielding to idle");
    check(!idleTimerSourcePrepare(&idle->source, &timeout)
          && g_source_get_ready_time(&idle->source) > 0
          && g_source_get_ready_time(&normal->source) == -1,
          "idle timer arms its own source only");
    normal->runWithIdlePriority = false;
    check(!idleTimerSourcePrepare(&idle->source, &timeout) && timeout == -1
          && g_source_get_ready_time(&idle->source) == -1,
          "idle source clears its deadline when yielding to normal");

    normal->processEventsFlags = QEventLoop::X11ExcludeTimers;
    g_source_set_ready_time(&normal->source, 0);
    timerSourcePrepare(&normal->source, &timeout);
    check(timeout == -1 && g_source_get_ready_time(&normal->source) == -1
          && !timerSourceCheck(&normal->source),
          "excluded timers clear their deadline and do not pass check");
    normal->processEventsFlags = QEventLoop::AllEvents;
    timerSourcePrepare(&normal->source, &timeout);
    check(g_source_get_ready_time(&normal->source) > g_get_monotonic_time(),
          "disarmed source can rearm the same unchanged future timer");

    timer->timeout = steady_clock::now() - 1s;
    check(timerSourcePrepare(&normal->source, &timeout)
          && g_source_get_ready_time(&normal->source) == 0,
          "expired timer is immediately ready");
    QTimerInfo *activating = timer;
    timer->activateRef = &activating;
    check(!timerSourcePrepare(&normal->source, &timeout)
          && g_source_get_ready_time(&normal->source) == -1,
          "active-only timer list clears a previous deadline");
    timer->activateRef = nullptr;
    normal->timerList.clearTimers();
    g_source_set_ready_time(&normal->source, 0);
    timerSourcePrepare(&normal->source, &timeout);
    check(timeout == -1 && g_source_get_ready_time(&normal->source) == -1,
          "removing the last timer disarms the source");

    // Dispatch clears the previous deadline even when no timer remains.
    g_source_set_ready_time(&normal->source, 0);
    timerSourceDispatch(&normal->source, nullptr, nullptr);
    check(g_source_get_ready_time(&normal->source) == -1,
          "normal dispatch disarms its old deadline");
    g_source_set_ready_time(&idle->source, 0);
    idleTimerSourceDispatch(&idle->source, nullptr, nullptr);
    check(g_source_get_ready_time(&idle->source) == -1,
          "idle dispatch disarms its old deadline");
    return failures == 0 ? 0 : 1;
}
