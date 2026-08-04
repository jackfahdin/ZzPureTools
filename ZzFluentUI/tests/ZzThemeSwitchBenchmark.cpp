#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

#include <QtCore/QCoreApplication>
#include <QtCore/QtGlobal>
#include <QtTest/QTest>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

/**
 * @brief 测量五百个可见控件的应用级主题切换耗时。
 */
class ZzThemeSwitchBenchmark final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void switchesFiveHundredVisibleControls()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = std::make_unique<ZzFluentUI::ZzFluentStyle>(
            &controller);
        QWidget window;
        window.setStyle(style.get());
        auto *layout = new QGridLayout(&window);
        for (int index = 0; index < 500; ++index) {
            layout->addWidget(
                new QPushButton(QString::number(index)),
                index / 25,
                index % 25);
        }
        window.show();
        QCoreApplication::processEvents();

        std::vector<double> samples;
        samples.reserve(100);
        for (int iteration = 0; iteration < 110; ++iteration) {
            const auto start = std::chrono::steady_clock::now();
            controller.setMode(iteration % 2 == 0
                    ? ZzFluentUI::ZzThemeMode::Dark
                    : ZzFluentUI::ZzThemeMode::Light);
            QCoreApplication::processEvents();
            const auto elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start)
                                     .count();
            if (iteration >= 10) {
                samples.push_back(elapsed);
            }
        }

        QCOMPARE(samples.size(), std::size_t{100});
        std::sort(samples.begin(), samples.end());
        const double p50 = samples[samples.size() / 2];
        const std::size_t p95Rank =
            ((samples.size() * std::size_t{95}) + std::size_t{99})
            / std::size_t{100};
        const double p95 = samples[p95Rank - 1];
        const double maximum = samples.back();
        qInfo(
            "theme-switch-ms warmup=10 samples=100 p50=%.3f p95=%.3f max=%.3f",
            p50,
            p95,
            maximum);
        if (qEnvironmentVariableIntValue(
                "ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(
                p95 <= 50.0,
                "500-control theme switch P95 exceeded 50 ms");
        }
    }
};

QTEST_MAIN(ZzThemeSwitchBenchmark)

#include "ZzThemeSwitchBenchmark.moc"
