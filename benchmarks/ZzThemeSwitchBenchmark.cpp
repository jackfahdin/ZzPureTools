#include <cstdio>
#include <cstdlib>
#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QIODevice>
#include <QtCore/QList>
#include <QtCore/QTextStream>
#include <QtGui/QImage>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include <QtTest/QSignalSpy>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeMode.h>

#include "ZzBenchmarkMetadata.h"
#include "ZzPerformanceReporter.h"

namespace {

constexpr int zzColumns = 25;
constexpr int zzRows = 20;
constexpr int zzControlExtent = 36;
constexpr int zzGridSpacing = 4;
constexpr int zzWarmupIterations = 10;
constexpr int zzMeasuredIterations = 100;

/** @brief 返回命令行中 --report 后的输出路径。 */
QString zzReportPath(const QStringList &arguments)
{
    const qsizetype option = arguments.indexOf(QStringLiteral("--report"));
    return option >= 0 && option + 1 < arguments.size()
        ? arguments.at(option + 1).trimmed() : QString{};
}

/** @brief 输出主题基准失败原因并返回失败码。 */
int zzFail(const QString &message)
{
    QTextStream stream(stderr, QIODevice::WriteOnly);
    stream << message << '\n';
    stream.flush();
    return EXIT_FAILURE;
}

/** @brief 验证预渲染图像既非透明空白也不只有单一颜色。 */
bool zzHasMeaningfulPixels(const QImage &image)
{
    bool hasVisiblePixel = false;
    QRgb firstColor = 0;
    bool hasFirstColor = false;
    bool hasDifferentColor = false;
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(
            image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb color = line[x];
            hasVisiblePixel = hasVisiblePixel || qAlpha(color) != 0;
            if (!hasFirstColor) {
                firstColor = color;
                hasFirstColor = true;
            } else if (color != firstColor) {
                hasDifferentColor = true;
            }
            if (hasVisiblePixel && hasDifferentColor) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    const QString reportPath = zzReportPath(application.arguments());
    if (reportPath.isEmpty()) {
        return zzFail(QStringLiteral("missing --report output path"));
    }

    ZzFluentUI::ZzThemeController controller;
    auto style = std::make_unique<ZzFluentUI::ZzFluentStyle>(&controller);
    QWidget window;
    window.setObjectName(QStringLiteral("ZzThemeSwitchBenchmarkWindow"));
    window.setStyle(style.get());
    window.setFixedSize(1000, 800);
    auto *layout = new QGridLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(zzGridSpacing);
    layout->setVerticalSpacing(zzGridSpacing);

    QList<QWidget *> controls;
    controls.reserve(
        static_cast<qsizetype>(zzColumns) * zzRows);
    for (int row = 0; row < zzRows; ++row) {
        for (int column = 0; column < zzColumns; ++column) {
            auto *control = new QPushButton(
                QString::number((row * zzColumns) + column), &window);
            control->setMinimumSize(0, 0);
            control->setFixedSize(zzControlExtent, zzControlExtent);
            layout->addWidget(control, row, column);
            controls.append(control);
        }
    }

    window.ensurePolished();
    for (QWidget *control : controls) {
        control->ensurePolished();
    }
    window.show();
    layout->activate();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    for (QWidget *control : controls) {
        const QRect geometryInWindow(
            control->mapTo(&window, QPoint(0, 0)), control->size());
        if (!control->isVisibleTo(&window)
            || !window.rect().contains(geometryInWindow)) {
            return zzFail(QStringLiteral(
                "control is not fully visible in the fixed benchmark grid"));
        }
    }

    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    window.render(&image);
    if (!zzHasMeaningfulPixels(image)) {
        return zzFail(QStringLiteral(
            "theme benchmark pre-render was blank or single-colored"));
    }

    ZzBenchmarks::ZzPerformanceReporter reporter;
    reporter.setScenario(QStringLiteral("theme-switch"));
    reporter.setWarmupIterations(zzWarmupIterations);
    const auto metadata = ZzBenchmarks::ZzBenchmarkMetadata::populate(
        reporter, window.screen());
    if (!metadata) {
        return zzFail(metadata.error().technicalMessage());
    }

    constexpr int totalIterations = zzWarmupIterations
        + zzMeasuredIterations;
    for (int iteration = 0; iteration < totalIterations; ++iteration) {
        QSignalSpy changed(
            &controller, &ZzFluentUI::ZzThemeController::snapshotChanged);
        if (!changed.isValid()) {
            return zzFail(QStringLiteral(
                "failed to observe theme snapshot changes"));
        }

        QElapsedTimer timer;
        timer.start();
        controller.setMode((iteration % 2) == 0
                ? ZzFluentUI::ZzThemeMode::Dark
                : ZzFluentUI::ZzThemeMode::Light);
        if (changed.count() == 0 && !changed.wait(1000)) {
            return zzFail(QStringLiteral(
                "theme controller emitted no snapshot change"));
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        window.render(&image);
        const qint64 elapsedNanoseconds = timer.nsecsElapsed();
        if (iteration >= zzWarmupIterations) {
            constexpr double nanosecondsPerMillisecond = 1'000'000.0;
            reporter.addSample({
                QStringLiteral("latency"),
                QStringLiteral("ms"),
                static_cast<double>(elapsedNanoseconds)
                    / nanosecondsPerMillisecond});
        }
    }

    const auto writeResult = reporter.write(reportPath);
    return writeResult ? EXIT_SUCCESS
                       : zzFail(writeResult.error().technicalMessage());
}
