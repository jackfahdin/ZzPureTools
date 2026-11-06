#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QVariantAnimation>
#include <QtCore/QTextStream>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <ZzFluentUI/ZzAnimatedStackedWidget.h>
#include "../ZzFluentUI/widgets/src/private/ZzStackTransitionOverlay.h"
#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {

/** @brief 仅在基准中测量覆盖层 Paint 事件的同步 CPU 耗时。 */
class ZzStackBenchmarkApplication final : public QApplication
{
public:
    using QApplication::QApplication;
    QObject *overlay = nullptr;
    QList<double> paints;
    bool measuring = false;
    /** @brief 包围真实绘制事件，不改变产品绘制路径。 */
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

/** @brief 构造固定十二行表单和一个按钮的真实 QWidget 页面。 */
QWidget *zzCreatePage()
{
    auto *page = new QWidget;
    auto *layout = new QFormLayout(page);
    for (int i = 0; i < 12; ++i) layout->addRow(
        new QLabel(QStringLiteral("Field %1").arg(i)),
        new QLineEdit(QStringLiteral("Page transition sample %1").arg(i)));
    layout->addRow(new QPushButton(QStringLiteral("Apply")));
    return page;
}

} // namespace

int main(int argc, char *argv[])
{
    ZzStackBenchmarkApplication application(argc, argv);
    const auto arguments = application.arguments();
    const auto option = arguments.indexOf(QStringLiteral("--report"));
    if (option < 0 || option + 1 >= arguments.size()) return 1;
    ZzFluentUI::ZzAnimatedStackedWidget stack;
    stack.setFixedSize(1280, 720);
    stack.setDuration(200);
    stack.addWidget(zzCreatePage());
    stack.addWidget(zzCreatePage());
    stack.show();
    QCoreApplication::processEvents();
    auto *overlay = static_cast<ZzFluentUI::ZzStackTransitionOverlay *>(
        stack.findChild<QWidget *>(QStringLiteral("zzAnimatedStackOverlay")));
    auto *animation = stack.findChild<QVariantAnimation *>();
    if (!overlay || !animation) return 2;
    application.overlay = overlay;
    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("animated-stack"));
    reporter.setWarmupIterations(10);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(reporter, stack.screen());
    if (!metadata) {
        QTextStream(stderr) << metadata.error().technicalMessage() << '\n';
        return 3;
    }
    // 报告固定 schema 的 dpr 字段来自屏幕；确认与实际捕获的控件 DPR 一致。
    if (!qFuzzyCompare(stack.devicePixelRatioF(), stack.screen()->devicePixelRatio())) return 10;
    const auto objectCount = stack.findChildren<QObject *>().size();
    for (int iteration = 0; iteration < 110; ++iteration) {
        QElapsedTimer preparation;
        preparation.start();
        stack.setCurrentIndex(1 - stack.currentIndex());
        const double elapsed = static_cast<double>(preparation.nsecsElapsed()) / 1'000'000.0;
        if (!stack.isTransitionRunning()) return 4;
        animation->pause();
        const auto peak = overlay->retainedBytes();
        if (peak <= 0 || peak > 64 * 1024 * 1024) return 5;
        application.paints.clear();
        application.measuring = iteration >= 10;
        // 每轮覆盖多个进度，计时只包含覆盖层实际 Paint 分发。
        for (int frame = 1; frame <= 10; ++frame) {
            animation->setCurrentTime(frame * 18);
            overlay->repaint();
        }
        application.measuring = false;
        animation->setCurrentTime(animation->duration());
        if (overlay->retainedBytes() != 0 || stack.isTransitionRunning()) return 6;
        if (iteration >= 10) {
            if (application.paints.isEmpty()) return 7;
            reporter.addSample({QStringLiteral("snapshot_prepare_ms"), QStringLiteral("ms"), elapsed});
            for (double paint : application.paints)
                reporter.addSample({QStringLiteral("overlay_paint_ms"), QStringLiteral("ms"), paint});
            reporter.addSample({QStringLiteral("peak_snapshot_bytes"), QStringLiteral("bytes"), static_cast<double>(peak)});
            reporter.addSample({QStringLiteral("idle_snapshot_bytes"), QStringLiteral("bytes"), 0});
        }
    }
    if (stack.findChildren<QObject *>().size() != objectCount
        || stack.findChildren<QVariantAnimation *>().size() != 1) return 8;
    const auto result = reporter.write(arguments.at(option + 1));
    if (!result) QTextStream(stderr) << result.error().technicalMessage() << '\n';
    return result ? 0 : 9;
}
