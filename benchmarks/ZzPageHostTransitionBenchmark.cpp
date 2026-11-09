#include <algorithm>
#include <cmath>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QTextStream>
#include <QtCore/QVariantAnimation>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <ZzPureTools/ZzPageHost.h>
#include <ZzPureTools/ZzPageInstance.h>
#include "../ZzPureTools/widgets/src/private/ZzPageHostTransitionPrivate.h"
#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {
/** @brief 仅基准记录真实覆盖层 Paint 分发 CPU 耗时。 */
class ZzHostBenchmarkApplication final : public QApplication
{
public:
    using QApplication::QApplication;
    QObject *overlay = nullptr;
    QList<double> paints;
    bool measuring = false;
    bool notify(QObject *receiver, QEvent *event) override
    {
        if (!measuring || receiver != overlay || event->type() != QEvent::Paint)
            return QApplication::notify(receiver, event);
        QElapsedTimer timer;
        timer.start();
        const bool result = QApplication::notify(receiver, event);
        paints.append(static_cast<double>(timer.nsecsElapsed()) / 1'000'000.0);
        return result;
    }
};
/** @brief 与独立容器基准相同的十二行表单。 */
ZzPureTools::ZzPageRegistration zzRegistration(const QString &route, bool transient, int &destroyed)
{
    ZzPureTools::ZzPageRegistration registration;
    registration.routeId = ZzPureTools::ZzRouteId(route);
    registration.lifetime = transient ? ZzPureTools::ZzPageLifetimePolicy::WhileActive
        : ZzPureTools::ZzPageLifetimePolicy::Persistent;
    registration.factory = [&destroyed](QWidget *parent) {
        auto *page = new QWidget(parent);
        auto *layout = new QFormLayout(page);
        for (int i = 0; i < 12; ++i) layout->addRow(new QLabel(QStringLiteral("Field %1").arg(i)),
            new QLineEdit(QStringLiteral("Page transition sample %1").arg(i)));
        layout->addRow(new QPushButton(QStringLiteral("Apply")));
        // 使用页面外的观察上下文，避免被展示对象之间的断连移除。
        QObject::connect(page, &QObject::destroyed, parent, [&destroyed] { ++destroyed; });
        return ZzPureTools::ZzPageInstance::create(parent, page, std::make_unique<QObject>(), std::make_unique<QObject>());
    };
    return registration;
}
/** @brief 使用 nearest-rank 分位数，不以平均值代替门限。 */
double zzP95(QList<double> samples)
{
    if (samples.isEmpty()) return 1e9;
    std::sort(samples.begin(), samples.end());
    const auto index = static_cast<qsizetype>(std::ceil(static_cast<double>(samples.size()) * 0.95)) - 1;
    return samples.at(index);
}
}

int main(int argc, char *argv[])
{
    ZzHostBenchmarkApplication application(argc, argv);
    const auto args = application.arguments();
    const auto reportIndex = args.indexOf(QStringLiteral("--report"));
    const auto policyIndex = args.indexOf(QStringLiteral("--policy"));
    if (reportIndex < 0 || reportIndex + 1 >= args.size() || policyIndex < 0 || policyIndex + 1 >= args.size()) return 1;
    const QString policy = args.at(policyIndex + 1);
    if (policy != QStringLiteral("persistent") && policy != QStringLiteral("while-active")) return 1;
    const bool transient = policy == QStringLiteral("while-active");
    int destroyed = 0;
    QElapsedTimer capture;
    double prepared = 0;
    int observations = 0;
    ZzPureTools::ZzPageHost host;
    host.setFixedSize(1280, 720);
    if (!host.setTransitionMode(ZzPureTools::ZzPageHost::ZzTransitionMode::Fade)
        || !host.setTransitionDuration(200)) return 2;
    const auto a = zzRegistration(QStringLiteral("a"), transient, destroyed);
    const auto b = zzRegistration(QStringLiteral("b"), transient, destroyed);
    if (!host.activate(a)) return 2;
    host.show();
    QCoreApplication::processEvents();
    auto *overlay = host.findChild<QWidget *>(QStringLiteral("zzAnimatedStackOverlay"));
    auto *animation = host.findChild<QVariantAnimation *>();
    if (!overlay || !animation) return 2;
    application.overlay = overlay;
    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("page-host-transition-%1").arg(policy));
    reporter.setWarmupIterations(10);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(reporter, host.screen());
    if (!metadata) { QTextStream(stderr) << metadata.error().technicalMessage() << '\n'; return 3; }
    if (!qFuzzyCompare(host.devicePixelRatioF(), host.screen()->devicePixelRatio())) return 3;
    ZzPureTools::ZzPageHostTransitionPrivate::observeCapture(&host, [&](bool entering) {
        if (entering) { ++observations; capture.start(); }
        else { --observations; prepared = static_cast<double>(capture.nsecsElapsed()) / 1'000'000.0; }
    });
    QList<double> preparations, paints;
    for (int iteration = 0; iteration < 110; ++iteration) {
        const auto &target = iteration % 2 == 0 ? b : a;
        QElapsedTimer total;
        total.start();
        if (!host.activate(target)) return 4;
        const double elapsed = static_cast<double>(total.nsecsElapsed()) / 1'000'000.0;
        if (!host.isTransitionRunning() || host.currentRoute() != target.routeId || observations != 0) return 4;
        if (destroyed != (transient ? iteration + 1 : 0)) {
            QTextStream(stderr) << "iteration " << iteration << ": destroyed views " << destroyed
                << ", expected " << (transient ? iteration + 1 : 0) << '\n';
            return 5;
        }
        animation->pause();
        const auto peak = ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host);
        if (peak <= 0 || peak > 64 * 1024 * 1024) {
            QTextStream(stderr) << "iteration " << iteration << ": snapshot bytes " << peak << '\n';
            return 5;
        }
        application.paints.clear();
        application.measuring = iteration >= 10;
        for (int frame = 1; frame <= 10; ++frame) {
            animation->setCurrentTime(frame * 18);
            overlay->repaint();
        }
        application.measuring = false;
        animation->setCurrentTime(200);
        if (host.isTransitionRunning() || ZzPureTools::ZzPageHostTransitionPrivate::snapshotBytes(&host)) return 6;
        if (iteration >= 10) {
            if (application.paints.isEmpty()) return 7;
            preparations.append(prepared);
            paints.append(application.paints);
            reporter.addSample({QStringLiteral("activate_total_ms"), QStringLiteral("ms"), elapsed});
            reporter.addSample({QStringLiteral("snapshot_prepare_ms"), QStringLiteral("ms"), prepared});
            for (const auto paint : application.paints) reporter.addSample({QStringLiteral("overlay_paint_ms"), QStringLiteral("ms"), paint});
            reporter.addSample({QStringLiteral("peak_snapshot_bytes"), QStringLiteral("bytes"), static_cast<double>(peak)});
            reporter.addSample({QStringLiteral("idle_snapshot_bytes"), QStringLiteral("bytes"), 0});
            reporter.addSample({QStringLiteral("destroyed_views"), QStringLiteral("count"), static_cast<double>(destroyed)});
        }
    }
    ZzPureTools::ZzPageHostTransitionPrivate::observeCapture(&host, {});
    if (host.findChildren<QVariantAnimation *>().size() != 1) return 8;
    const auto written = reporter.write(args.at(reportIndex + 1));
    if (!written) { QTextStream(stderr) << written.error().technicalMessage() << '\n'; return 9; }
    if (zzP95(preparations) > 16.7 || zzP95(paints) > 8) return 10;
    return 0;
}
