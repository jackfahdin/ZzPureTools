#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

// A Qt-only control, intentionally without any framework headers or libraries.
class Control final : public QWidget
{
public:
    QElapsedTimer clock;
    qint64 lastPaint = -1;
    bool measuring = false;
    QJsonArray paintIntervals;
    QJsonArray paintDurations;
    double value = 0;
    Control() { clock.start(); setFixedSize(280, 280); }
    void paintEvent(QPaintEvent *) override
    {
        const qint64 started = clock.nsecsElapsed();
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        painter.fillRect(QRectF(0, 0, value * 2, 40), Qt::blue);
        painter.end();
        if (measuring) {
            if (lastPaint >= 0) paintIntervals.append(double(started - lastPaint) / 1e6);
            paintDurations.append(double(clock.nsecsElapsed() - started) / 1e6);
        }
        lastPaint = started;
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc != 2) return 2;
    Control widget;
    QVariantAnimation animation;
    animation.setDuration(200);
    animation.setEasingCurve(QEasingCurve::OutCubic);
    QJsonArray ticks;
    qint64 previousTick = -1;
    QObject::connect(&animation, &QVariantAnimation::valueChanged, &widget,
        [&](const QVariant &value) {
            const qint64 now = widget.clock.nsecsElapsed();
            if (widget.measuring && previousTick >= 0) {
                ticks.append(double(now - previousTick) / 1e6);
            }
            previousTick = now;
            widget.value = value.toDouble();
            widget.update();
        });
    widget.show();
    app.processEvents();
    for (int round = 0; round < 110; ++round) {
        widget.measuring = false;
        animation.setStartValue((round % 2) == 0 ? 0.0 : 100.0);
        animation.setEndValue((round % 2) == 0 ? 100.0 : 0.0);
        widget.lastPaint = -1;
        previousTick = -1;
        widget.measuring = round >= 10;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&animation, &QVariantAnimation::finished, &loop, &QEventLoop::quit);
        timeout.start(1000);
        animation.start();
        loop.exec();
        app.processEvents();
        if (animation.state() != QAbstractAnimation::Stopped) return 3;
    }
    widget.measuring = false;
    QJsonArray preciseTimerIntervals;
    QElapsedTimer timerClock;
    QTimer timer;
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(16);
    qint64 previous = -1;
    int count = 0;
    QEventLoop timerLoop;
    QObject::connect(&timer, &QTimer::timeout, &timerLoop, [&] {
        const qint64 now = timerClock.nsecsElapsed();
        if (count >= 10) preciseTimerIntervals.append(double(now - previous) / 1e6);
        previous = now;
        if (++count == 510) timerLoop.quit();
    });
    timerClock.start();
    timer.start();
    timerLoop.exec();
    const QJsonObject result {
        {"purpose", "Qt-only diagnostic control; not a performance reference"},
        {"qt", qVersion()},
        {"backend", app.eventDispatcher()->metaObject()->className()},
        {"warmupIterations", 10}, {"measuredIterations", 100},
        {"animationDurationMs", 200}, {"unit", "ms"},
        {"animationIntervals", ticks}, {"paintIntervals", widget.paintIntervals},
        {"paintDurations", widget.paintDurations},
        {"precise16msTimerIntervals", preciseTimerIntervals}
    };
    QFile output(QString::fromLocal8Bit(argv[1]));
    if (!output.open(QIODevice::WriteOnly)) return 4;
    const auto bytes = QJsonDocument(result).toJson();
    return output.write(bytes) == bytes.size() ? 0 : 5;
}
