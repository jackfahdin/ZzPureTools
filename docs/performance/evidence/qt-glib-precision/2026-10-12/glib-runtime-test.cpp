#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QSocketNotifier>
#include <QtCore/QTimer>
#include <QtCore/private/qeventdispatcher_glib_p.h>
#include <chrono>
#include <cstdio>
#include <thread>
#include <unistd.h>
#include <glib.h>

using namespace std::chrono_literals;

static int pollCalls = 0;
static gint countedPoll(GPollFD *fds, guint count, gint timeout)
{
    ++pollCalls;
    return g_poll(fds, count, timeout);
}

int main(int argc, char **argv)
{
    // QCoreApplication's internal constructor calls are not ELF-interposable.
    // Explicitly create the linked diagnostic dispatcher for this core-only test.
    QCoreApplication::setEventDispatcher(new QEventDispatcherGlib(g_main_context_default()));
    QCoreApplication app(argc, argv);
    int failures = 0;
    auto check = [&](bool ok, const char *name) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
        if (!ok) ++failures;
    };
    check(app.eventDispatcher()->inherits("QEventDispatcherGlib"), "uses GLib dispatcher");

    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
    watchdog.start(1500);
    int ticks = 0;
    bool nestedCompleted = false;
    bool posted = false;
    bool socketReady = false;
    auto maybeQuit = [&] { if (ticks >= 5 && posted && socketReady) loop.quit(); };
    QTimer repeating;
    repeating.setTimerType(Qt::PreciseTimer);
    repeating.setInterval(3ms);
    QObject::connect(&repeating, &QTimer::timeout, &loop, [&] {
        ++ticks;
        if (ticks == 1) {
            QEventLoop nested;
            QTimer::singleShot(10ms, Qt::PreciseTimer, &nested, &QEventLoop::quit);
            nested.exec();
            check(ticks == 1, "nested loop does not reenter the active repeating timer");
            nestedCompleted = true;
        }
        if (ticks == 5) repeating.stop();
        maybeQuit();
    });
    repeating.start();
    bool cancelledFired = false;
    QTimer cancelled;
    QObject::connect(&cancelled, &QTimer::timeout, &loop, [&] { cancelledFired = true; });
    cancelled.start(1);
    cancelled.stop();
    bool deletedFired = false;
    auto *deleted = new QTimer;
    QObject::connect(deleted, &QTimer::timeout, &loop, [&] { deletedFired = true; });
    deleted->start(1);
    delete deleted;

    int pipes[2];
    if (::pipe(pipes) != 0) return 2;
    QSocketNotifier notifier(pipes[0], QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &loop, [&] {
        char byte;
        socketReady = ::read(pipes[0], &byte, 1) == 1 && byte == 'x';
        notifier.setEnabled(false);
        maybeQuit();
    });
    bool wroteByte = false; // Read only after joining the worker.
    std::thread worker([&] {
        std::this_thread::sleep_for(20ms);
        QMetaObject::invokeMethod(&loop, [&] { posted = true; maybeQuit(); }, Qt::QueuedConnection);
        const char byte = 'x';
        wroteByte = ::write(pipes[1], &byte, 1) == 1;
    });
    loop.exec();
    worker.join();
    watchdog.stop();
    notifier.setEnabled(false);
    ::close(pipes[0]);
    ::close(pipes[1]);
    check(!timedOut && ticks == 5 && nestedCompleted, "repeating timer and nested loop complete");
    check(posted, "cross-thread posted event wakes the loop");
    check(wroteByte && socketReady, "socket notifier remains functional");
    check(!cancelledFired && !deletedFired, "stopped and deleted timers never fire");

    QElapsedTimer elapsed;
    int blocks = 0;
    const auto connection = QObject::connect(app.eventDispatcher(),
        &QAbstractEventDispatcher::aboutToBlock, &loop, [&] { ++blocks; });
    // Count the actual inner GLib iterations, not only Qt's outer block signal.
    // A custom poll retains millisecond waiting but detects self-wakeup spinning.
    g_main_context_set_poll_func(g_main_context_default(), countedPoll);
    elapsed.start();
    QTimer::singleShot(30ms, Qt::PreciseTimer, &loop, &QEventLoop::quit);
    loop.exec();
    check(elapsed.durationElapsed() >= 30ms, "precise timer does not fire early");
    check(blocks < 20, "idle loop waits without spinning on stale deadlines");
    check(pollCalls > 0 && pollCalls < 20, "GLib poll does not spin inside Qt processEvents");
    std::printf("pollCalls=%d\n", pollCalls);
    g_main_context_set_poll_func(g_main_context_default(), nullptr);
    QObject::disconnect(connection);
    return failures == 0 ? 0 : 1;
}
