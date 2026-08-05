#include <cstdio>
#include <cstdlib>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QIODevice>
#include <QtCore/QList>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QVariantAnimation>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzToggleSwitch.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {

constexpr int zzWarmupIterations = 10;
constexpr int zzMeasuredIterations = 100;
constexpr int zzAnimationTimeoutMilliseconds = 1000;
constexpr qsizetype zzMinimumPaintMarkers = 8;

/** @brief 记录目标控件每次 Paint 的单调纳秒时刻。 */
class ZzPaintTimeline final : public QObject
{
public:
    /** @brief 启动跨轮次单调时钟。 */
    ZzPaintTimeline()
    {
        timer_.start();
    }

    /** @brief 清空当前轮次标记但保持时钟连续。 */
    void reset()
    {
        markers_.clear();
    }

    /** @brief 返回当前轮次只读 Paint 时刻。 */
    [[nodiscard]] const QList<qint64> &markers() const noexcept
    {
        return markers_;
    }

protected:
    /** @brief 观察 Paint 且不改变控件事件处理。 */
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (event != nullptr && event->type() == QEvent::Paint) {
            markers_.append(timer_.nsecsElapsed());
        }
        return false;
    }

private:
    QElapsedTimer timer_;
    QList<qint64> markers_;
};

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出动画基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 等待生产动画结束并验证该轮确实形成连续绘制。 */
bool zzRunToggle(
    ZzFluentUI::ZzToggleSwitch *toggle,
    QVariantAnimation *animation,
    ZzPaintTimeline *timeline,
    QString *failure)
{
    timeline->reset();
    QEventLoop animationLoop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setTimerType(Qt::PreciseTimer);
    QObject::connect(
        animation,
        &QVariantAnimation::finished,
        &animationLoop,
        &QEventLoop::quit);
    QObject::connect(
        &timeoutTimer,
        &QTimer::timeout,
        &animationLoop,
        &QEventLoop::quit);

    QTest::mouseClick(
        toggle, Qt::LeftButton, Qt::NoModifier, toggle->rect().center());
    if (animation->state() != QAbstractAnimation::Running) {
        *failure = QStringLiteral("toggle did not start its production animation");
        return false;
    }
    timeoutTimer.start(zzAnimationTimeoutMilliseconds);
    animationLoop.exec();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    if (animation->state() != QAbstractAnimation::Stopped) {
        *failure = QStringLiteral("toggle animation exceeded 1000 ms timeout");
        return false;
    }
    if (timeline->markers().size() < zzMinimumPaintMarkers) {
        *failure = QStringLiteral("toggle animation produced only %1 paint markers")
                       .arg(timeline->markers().size());
        return false;
    }
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
    host.setObjectName(QStringLiteral("ZzAnimationBenchmarkWindow"));
    host.setFixedSize(240, 100);
    auto *layout = new QVBoxLayout(&host);
    auto *toggle = new ZzFluentUI::ZzToggleSwitch(
        QStringLiteral("Animation"), &host);
    toggle->setFixedSize(toggle->sizeHint());
    layout->addWidget(toggle, 0, Qt::AlignCenter);

    ZzPaintTimeline timeline;
    toggle->installEventFilter(&timeline);
    host.show();
    host.raise();
    host.activateWindow();
    toggle->setFocus(Qt::OtherFocusReason);
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    const QList<QVariantAnimation *> animations =
        toggle->findChildren<QVariantAnimation *>();
    if (animations.size() != 1) {
        return zzFail(QStringLiteral(
            "toggle must own exactly one QVariantAnimation, found %1")
                          .arg(animations.size()));
    }
    QVariantAnimation *const animation = animations.constFirst();

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("animation"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, host.screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    QString failure;
    for (int warmup = 0; warmup < zzWarmupIterations; ++warmup) {
        if (!zzRunToggle(toggle, animation, &timeline, &failure)) {
            return zzFail(failure);
        }
    }

    const qsizetype initialObjectCount =
        toggle->findChildren<QObject *>().size();
    constexpr double nanosecondsPerMillisecond = 1'000'000.0;
    for (int iteration = 0; iteration < zzMeasuredIterations; ++iteration) {
        if (!zzRunToggle(toggle, animation, &timeline, &failure)) {
            return zzFail(failure);
        }
        const QList<qint64> &markers = timeline.markers();
        for (qsizetype index = 1; index < markers.size(); ++index) {
            reporter.addSample({
                QStringLiteral("frame-time"),
                QStringLiteral("ms"),
                static_cast<double>(markers.at(index) - markers.at(index - 1))
                    / nanosecondsPerMillisecond});
        }
    }

    const qsizetype finalObjectCount =
        toggle->findChildren<QObject *>().size();
    if (finalObjectCount != initialObjectCount) {
        return zzFail(QStringLiteral(
            "toggle descendant count changed from %1 to %2")
                          .arg(initialObjectCount)
                          .arg(finalObjectCount));
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
