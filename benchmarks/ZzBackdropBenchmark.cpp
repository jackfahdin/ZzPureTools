#include <cstdio>
#include <cstdlib>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QIODevice>
#include <QtCore/QTextStream>
#include <QtGui/QImage>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"
#include "ZzSoftwareBackdrop.h"

namespace {

constexpr int zzWarmupIterations = 10;
constexpr int zzMeasuredIterations = 100;
constexpr int zzToggleIterations = 1000;
constexpr int zzWindowWidth = 960;
constexpr int zzWindowHeight = 540;

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出软件材质基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 让窗口和材质层消费当前轮次的可见性与绘制事件。 */
void zzProcessEvents()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
}

/** @brief 返回宿主当前子对象数量，用于检测长期切换泄漏。 */
qsizetype zzChildObjectCount(const QWidget &host)
{
    return host.findChildren<QObject *>().size();
}

/** @brief 在固定尺寸目标上绘制一次缓存材质。 */
bool zzRenderFrame(QWidget *layer, QImage *target)
{
    if (layer == nullptr || target == nullptr || target->isNull()) {
        return false;
    }
    target->fill(Qt::transparent);
    layer->render(target);
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    QWidget host;
    host.setObjectName(QStringLiteral("ZzBackdropBenchmarkHost"));
    host.setFixedSize(zzWindowWidth, zzWindowHeight);

    ZzWindowKit::ZzSoftwareBackdrop backdrop;
    if (!backdrop.attach(&host) || !backdrop.setEnabled(true)) {
        return zzFail(QStringLiteral(
            "failed to attach and enable the software backdrop"));
    }

    auto *layer = host.findChild<QWidget *>(
        QStringLiteral("zzSoftwareBackdropLayer"));
    if (layer == nullptr) {
        return zzFail(QStringLiteral("software backdrop layer is missing"));
    }
    const qsizetype baselineObjectCount = zzChildObjectCount(host);
    const auto devicePixelRatio = host.devicePixelRatioF();
    const QSize physicalSize = (QSizeF(host.size()) * devicePixelRatio)
        .toSize();
    QImage frameTarget(physicalSize, QImage::Format_ARGB32_Premultiplied);
    frameTarget.setDevicePixelRatio(devicePixelRatio);
    if (!zzRenderFrame(layer, &frameTarget)) {
        return zzFail(QStringLiteral("failed to render benchmark target"));
    }

    host.show();
    zzProcessEvents();
    if (host.screen() == nullptr) {
        return zzFail(QStringLiteral("benchmark host has no active screen"));
    }

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("backdrop"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, host.screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    // 预热固定尺寸绘制和缓存切换，避免把首次 Qt 初始化混入正式样本。
    for (int iteration = 0; iteration < zzWarmupIterations; ++iteration) {
        if (!backdrop.setEnabled(false) || !backdrop.setEnabled(true)
            || !zzRenderFrame(layer, &frameTarget)) {
            return zzFail(QStringLiteral("backdrop warmup failed"));
        }
        QEvent paletteChange(QEvent::PaletteChange);
        QCoreApplication::sendEvent(&host, &paletteChange);
    }

    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    for (int iteration = 0; iteration < zzMeasuredIterations; ++iteration) {
        if (!backdrop.setEnabled(false)) {
            return zzFail(QStringLiteral("failed to disable backdrop"));
        }
        QElapsedTimer enableTimer;
        enableTimer.start();
        if (!backdrop.setEnabled(true)) {
            return zzFail(QStringLiteral("failed to enable backdrop"));
        }
        reporter.addSample({
            QStringLiteral("enable-time"),
            QStringLiteral("ms"),
            static_cast<double>(enableTimer.nsecsElapsed())
                / nanosecondsPerMillisecond});

        QElapsedTimer frameTimer;
        frameTimer.start();
        if (!zzRenderFrame(layer, &frameTarget)) {
            return zzFail(QStringLiteral("fixed-size backdrop render failed"));
        }
        reporter.addSample({
            QStringLiteral("frame-time"),
            QStringLiteral("ms"),
            static_cast<double>(frameTimer.nsecsElapsed())
                / nanosecondsPerMillisecond});

        const auto rebuildBefore = backdrop.rebuildCount();
        QEvent paletteChange(QEvent::PaletteChange);
        QElapsedTimer rebuildTimer;
        rebuildTimer.start();
        QCoreApplication::sendEvent(&host, &paletteChange);
        if (backdrop.rebuildCount() != rebuildBefore + 1U) {
            return zzFail(QStringLiteral(
                "palette invalidation did not rebuild exactly once"));
        }
        reporter.addSample({
            QStringLiteral("rebuild-time"),
            QStringLiteral("ms"),
            static_cast<double>(rebuildTimer.nsecsElapsed())
                / nanosecondsPerMillisecond});

        for (int toggle = 0; toggle < zzToggleIterations; ++toggle) {
            if (!backdrop.setEnabled((toggle % 2) == 0)) {
                return zzFail(QStringLiteral("backdrop toggle failed"));
            }
        }
        if (zzChildObjectCount(host) != baselineObjectCount
            || host.findChild<QWidget *>(
                   QStringLiteral("zzSoftwareBackdropLayer")) != layer) {
            return zzFail(QStringLiteral(
                "backdrop toggles changed object count or layer identity"));
        }
        reporter.addSample({
            QStringLiteral("object-count"),
            QStringLiteral("objects"),
            static_cast<double>(zzChildObjectCount(host))});

        if (!backdrop.setEnabled(true)) {
            return zzFail(QStringLiteral(
                "failed to restore enabled state after toggle sample"));
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
