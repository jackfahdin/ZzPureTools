#include <QtTest/QTest>
#include <QtWidgets/QLineEdit>

#include "ZzStartupReadyWaiter.h"

class ZzStartupReadyWaiterTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void waitsForFocusAfterPaint()
    {
        QWidget window;
        QLineEdit editor(&window);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.activateWindow();
        editor.setFocus();
        QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == &editor; }));
        editor.clearFocus();
        QCOMPARE(QApplication::focusWidget(), nullptr);

        int completions = 0;
        bool ready = false;
        ZzStartupReadyWaiter waiter(&window, [&](bool result) {
            ++completions;
            ready = result;
        });
        waiter.start();
        QTest::qWait(20);
        QCOMPARE(completions, 0);
        editor.setFocus();
        QVERIFY(QTest::qWaitFor([&] { return completions == 1; }));
        QVERIFY(ready);
        waiter.start();
        window.update();
        QTest::qWait(20);
        QCOMPARE(completions, 1);
    }

    void unusableWindowTimesOut()
    {
        QWidget hiddenWindow;
        int completions = 0;
        bool ready = true;
        QElapsedTimer elapsed;
        qint64 completionMs = -1;
        ZzStartupReadyWaiter waiter(&hiddenWindow, [&](bool result) {
            ++completions;
            ready = result;
            completionMs = elapsed.elapsed();
        }, nullptr, 30);
        elapsed.start();
        waiter.start();
        QCOMPARE(completions, 0);
        QVERIFY(QTest::qWaitFor([&] { return completions == 1; }));
        QVERIFY(!ready);
        QVERIFY(completionMs >= 30);
    }

    void destroyingWindowFailsAndDestroyingWaiterCancels()
    {
        auto *window = new QWidget;
        int completions = 0;
        bool ready = true;
        ZzStartupReadyWaiter waiter(window, [&](bool result) {
            ++completions;
            ready = result;
        });
        waiter.start();
        delete window;
        QVERIFY(QTest::qWaitFor([&] { return completions == 1; }));
        QVERIFY(!ready);

        QWidget hiddenWindow;
        auto *cancelled = new ZzStartupReadyWaiter(&hiddenWindow,
            [&](bool) { ++completions; });
        cancelled->start();
        delete cancelled;
        QTest::qWait(20);
        QCOMPARE(completions, 1);
    }
};

QTEST_MAIN(ZzStartupReadyWaiterTest)
#include "ZzStartupReadyWaiterTest.moc"
