#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QThread>
#include <xcb/xproto.h>
#include <dlfcn.h>
#include <ZzFluentUI/ZzRadialGauge.h>
#include <vector>

struct Sample { qint64 ns; int iteration; const char *kind; int value; };
class Application final : public QApplication {
public:
    using QApplication::QApplication;
    QObject *gauge = nullptr;
    QObject *host = nullptr;
    int iteration = -1;
    QElapsedTimer clock;
    std::vector<Sample> samples;
    void mark(const char *kind, int value = 0) {
        if (iteration >= 0) samples.push_back({clock.nsecsElapsed(), iteration, kind, value});
    }
    bool notify(QObject *receiver, QEvent *event) override {
        const char *start = nullptr, *end = nullptr;
        if (iteration >= 0) {
            if (receiver == gauge && event->type() == QEvent::Paint) {
                start = "paint-start"; end = "paint-end";
            } else if (receiver == host && event->type() == QEvent::UpdateRequest) {
                start = "update-start"; end = "update-end";
            } else if (event->type() == QEvent::Timer) {
                start = "timer-start"; end = "timer-end";
            }
        }
        if (start) mark(start);
        const bool result = QApplication::notify(receiver, event);
        if (end) mark(end);
        return result;
    }
};
static Application *probe = nullptr;
extern "C" xcb_get_input_focus_reply_t *xcb_get_input_focus_reply(
    xcb_connection_t *connection, xcb_get_input_focus_cookie_t cookie,
    xcb_generic_error_t **error) {
    using Function = decltype(&xcb_get_input_focus_reply);
    static const auto real = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "xcb_get_input_focus_reply"));
    if (!real) std::abort();
    const bool record = probe && QThread::currentThread() == probe->thread();
    if (record) probe->mark("xcb-sync-start");
    auto *reply = real(connection, cookie, error);
    if (record) probe->mark("xcb-sync-end");
    return reply;
}
int main(int argc, char **argv) {
    Application app(argc, argv);
    probe = &app;
    if (argc != 2) return 2;
    app.samples.reserve(40000);
    app.clock.start();
    QWidget host;
    host.setFixedSize(280, 280);
    auto *layout = new QVBoxLayout(&host);
    auto *gauge = new ZzFluentUI::ZzRadialGauge(&host);
    gauge->setRange(0, 100);
    gauge->setValueAnimationDuration(200);
    gauge->setFixedSize(gauge->sizeHint());
    layout->addWidget(gauge, 0, Qt::AlignCenter);
    app.gauge = gauge; app.host = &host;
    host.show(); host.raise(); host.activateWindow();
    gauge->setFocus(Qt::OtherFocusReason);
    app.processEvents();
    const auto animations = gauge->findChildren<QVariantAnimation *>();
    if (animations.size() != 1) return 3;
    auto *animation = animations.first();
    QObject::connect(animation, &QVariantAnimation::valueChanged, &app,
        [&](const QVariant &) { app.mark("animation-value", animation->currentTime()); });
    QObject::connect(gauge, &QDial::valueChanged, &app,
        [&](int value) { app.mark("gauge-value", value); });
    QObject::connect(app.eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock,
        &app, [&] { app.mark("block"); });
    QObject::connect(app.eventDispatcher(), &QAbstractEventDispatcher::awake,
        &app, [&] { app.mark("awake"); });
    for (int i = -10; i < 100; ++i) {
        app.iteration = i;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(animation, &QVariantAnimation::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        app.mark("begin");
        const int target = i % 2 == 0 ? 100 : 0;
        gauge->setValue(target);
        timeout.start(1000);
        loop.exec();
        app.processEvents();
        if (animation->state() != QAbstractAnimation::Stopped || gauge->value() != target) return 4;
        app.mark("end");
    }
    app.iteration = -1;
    QFile output(QString::fromLocal8Bit(argv[1]));
    if (!output.open(QIODevice::WriteOnly)) return 5;
    QTextStream stream(&output);
    stream << "ns,iteration,event,value\n";
    for (const auto &s : app.samples)
        stream << s.ns << ',' << s.iteration << ',' << s.kind << ',' << s.value << '\n';
    probe = nullptr;
    return 0;
}
