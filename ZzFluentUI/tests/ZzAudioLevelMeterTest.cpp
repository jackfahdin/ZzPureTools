#include <ZzTestEventLoop.h>
#include <ZzFluentUI/ZzAudioLevelMeter.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

#include <QImage>
#include <QPointer>
#include <QProxyStyle>
#include <QSignalSpy>
#include <QTest>

#include <limits>
#include <thread>

using ZzFluentUI::ZzAudioLevelMeter;

class MeterMotionStyle final : public QProxyStyle
{
public:
    bool animate = true;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr, QStyleHintReturn *data = nullptr) const override
    {
        return hint == SH_Widget_Animate ? int(animate)
                                        : QProxyStyle::styleHint(hint, option, widget, data);
    }
};

class ZzAudioLevelMeterTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void convertsLinearAndBoundsInputs()
    {
        ZzAudioLevelMeter meter;
        meter.setAnimationEnabled(false);
        meter.setLinearLevels({1, 0.5, 0, -0.5});
        QCOMPARE(meter.channelCount(), 4);
        QCOMPARE(meter.level(0), 0.0);
        QVERIFY(qAbs(meter.level(1) + 6.020599913) < 0.000001);
        QCOMPARE(meter.level(2), -60.0);
        QCOMPARE(meter.level(3), meter.level(1));
        meter.setLevels({std::numeric_limits<qreal>::quiet_NaN(),
                         std::numeric_limits<qreal>::infinity(), -1000, 1000});
        QCOMPARE(meter.levels(), QVector<qreal>({-60, -60, -60, 0}));
        QCOMPARE(meter.displayedLevels(), meter.levels());
        QCOMPARE(meter.peakLevels(), meter.levels());
        meter.setLevels(QVector<qreal>(10000, -8));
        QCOMPARE(meter.channelCount(), 8);
        QCOMPARE(meter.levels().size(), 8);
        QCOMPARE(meter.level(-1), -60.0);
        meter.setLevels({});
        QCOMPARE(meter.levels(), QVector<qreal>(8, -60));
    }

    void rangeAndSettingsAreSanitized()
    {
        ZzAudioLevelMeter meter;
        meter.setAnimationEnabled(false);
        meter.setStereoLevels(-55, -1);
        meter.setRange(-30, -15);
        QCOMPARE(meter.warningDecibels(), -15.0);
        QCOMPARE(meter.clipDecibels(), -15.0);
        QCOMPARE(meter.levels(), QVector<qreal>({-30, -15}));
        QCOMPARE(meter.peakLevels(), meter.levels());
        meter.setRange(5, -10);
        QCOMPARE(meter.minimumDecibels(), -30.0);
        meter.setSegmentCount(10000);
        QCOMPARE(meter.segmentCount(), 120);
        meter.setScaleTickCount(-5);
        QCOMPARE(meter.scaleTickCount(), 2);
        meter.setScaleInterval(0);
        QCOMPARE(meter.scaleInterval(), 1.0);
        meter.setScalePosition(static_cast<ZzAudioLevelMeter::ZzMeterScalePosition>(99));
        QCOMPARE(meter.scalePosition(), ZzAudioLevelMeter::RightScale);
        meter.setCustomScaleValues({0, -10, -10, -100, std::numeric_limits<qreal>::infinity()});
        QCOMPARE(meter.customScaleValues(), QVector<qreal>({0, -10, -100}));
    }

    void decaysAfterActualHoldAndTimesOut()
    {
        MeterMotionStyle style;
        ZzAudioLevelMeter meter;
        meter.setStyle(&style);
        meter.setInputTimeout(0);
        meter.setDecayRate(200);
        meter.setPeakDecayRate(100);
        meter.setPeakHoldDuration(220);
        meter.show();
        meter.setLevel(-2);
        meter.setLevel(-40);
        QCOMPARE(meter.displayedLevel(0), -2.0);
        QTest::qWait(90);
        QVERIFY(meter.displayedLevel(0) < -10);
        QCOMPARE(meter.peakLevel(0), -2.0);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(meter.peakLevel(0) < -3, 600);
        meter.resetPeaks();
        QCOMPARE(meter.peakLevel(0), meter.displayedLevel(0));
        meter.setInputTimeout(45);
        meter.setLevel(-5);
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(meter.level(0), -60.0, 300);
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(meter.displayedLevel(0), -60.0, 600);
        meter.clear();
        QVERIFY(!meter.isRunning());
        QCOMPARE(meter.peakLevel(0), -60.0);
    }

    void stalledGuiUsesRealElapsedTime()
    {
        MeterMotionStyle style;
        ZzAudioLevelMeter meter;
        meter.setStyle(&style);
        meter.setInputTimeout(0);
        meter.setDecayRate(100);
        meter.setPeakHoldDuration(0);
        meter.setPeakDecayRate(50);
        meter.show();
        meter.setLevel(0);
        meter.setLevel(-60);
        // 阻塞事件循环模拟 GUI 忙碌；恢复后不能只按一个固定动画帧衰减。
        QTest::qSleep(240);
        QApplication::processEvents();
        QVERIFY(meter.displayedLevel(0) <= -20);
        QVERIFY(meter.peakLevel(0) <= -10);
        QVERIFY(meter.peakLevel(0) > meter.displayedLevel(0));
        meter.clear();
        meter.setDecayRate(0);
        meter.setPeakDecayRate(0);
        meter.setLevel(-3);
        meter.setLevel(-50);
        QVERIFY(!meter.isRunning());
        QCOMPARE(meter.displayedLevel(0), -3.0);
    }

    void freshInputDoesNotInheritStalledFrameTime()
    {
        MeterMotionStyle style;
        ZzAudioLevelMeter meter;
        meter.setStyle(&style);
        meter.setInputTimeout(0);
        meter.setDecayRate(100);
        meter.setPeakHoldDuration(200);
        meter.setPeakDecayRate(100);
        meter.show();
        meter.setStereoLevels(0, 0);
        meter.setStereoLevels(-60, -60);
        QTest::qSleep(300);
        // 新峰值刚刚到达；前面的阻塞时间不能被算进它的保持时间。
        meter.setStereoLevels(0, -60);
        meter.setStereoLevels(-60, -60);
        QApplication::processEvents();
        QVERIFY(meter.displayedLevel(0) > -10);
        QCOMPARE(meter.peakLevel(0), 0.0);
    }

    void stopsWhenHiddenDisabledOrReduced()
    {
        MeterMotionStyle style;
        ZzAudioLevelMeter meter;
        meter.setStyle(&style);
        meter.show();
        meter.setLevel(-3);
        QVERIFY(meter.isRunning());
        meter.hide();
        QVERIFY(!meter.isRunning());
        meter.show();
        meter.setEnabled(false);
        QVERIFY(!meter.isRunning());
        meter.setEnabled(true);
        style.animate = false;
        QEvent event(QEvent::StyleChange);
        QApplication::sendEvent(&meter, &event);
        QVERIFY(!meter.isRunning());
        meter.setLevel(-30);
        QCOMPARE(meter.displayedLevel(0), -30.0);
        QCOMPARE(meter.peakLevel(0), -30.0);
        QTest::qWait(180);
        QCOMPARE(meter.level(0), -30.0);
        QVERIFY(!meter.isRunning());
    }

    void workerInputsAreQueued()
    {
        ZzAudioLevelMeter meter;
        meter.setAnimationEnabled(false);
        QSignalSpy changes(&meter, &ZzAudioLevelMeter::levelsChanged);
        std::thread worker([&meter] {
            meter.setLevels(QVector<qreal>(100000, -9));
            meter.setLinearLevel(0.5);
        });
        worker.join();
        QCOMPARE(changes.count(), 0);
        ZZ_COMPARE_EVENTUALLY(meter.channelCount(), 1);
        QVERIFY(qAbs(meter.level(0) + 6.020599913) < 0.000001);
    }

    void synchronousDeletionAndReentryAreSafe()
    {
        for (int signal = 0; signal < 5; ++signal) {
            QPointer<ZzAudioLevelMeter> meter = new ZzAudioLevelMeter;
            if (signal == 0) {
                connect(meter, &ZzAudioLevelMeter::channelCountChanged, meter, [meter] { delete meter; });
                meter->setLevel(-3);
            } else if (signal == 1) {
                connect(meter, &ZzAudioLevelMeter::levelsChanged, meter, [meter] { delete meter; });
                meter->setStereoLevels(-3, -4);
            } else if (signal == 2) {
                connect(meter, &ZzAudioLevelMeter::minimumDecibelsChanged, meter, [meter] { delete meter; });
                meter->setRange(-20, 4);
            } else if (signal == 3) {
                connect(meter, &ZzAudioLevelMeter::peakLevelsChanged, meter, [meter] { delete meter; });
                meter->resetPeaks();
            } else {
                MeterMotionStyle style;
                meter->setStyle(&style);
                meter->show();
                connect(meter, &ZzAudioLevelMeter::runningChanged, meter, [meter] { delete meter; });
                meter->setStereoLevels(-3, -4);
            }
            QVERIFY(meter.isNull());
        }
        ZzAudioLevelMeter meter;
        connect(&meter, &ZzAudioLevelMeter::channelCountChanged, &meter,
                [&meter] { meter.setChannelCount(8); });
        meter.setLevel(-5);
        QCOMPARE(meter.channelCount(), 8);
        QCOMPARE(meter.levels().size(), 8);
    }

    void highContrastClipRemainsVisibleAndExplicitColorWins()
    {
        const QPalette original = QApplication::palette();
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzAudioLevelMeter meter;
        meter.setStyle(&style);
        QPalette palette = meter.palette();
        palette.setColor(QPalette::Window, Qt::black);
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::HighlightedText, Qt::black);
        meter.setPalette(palette);
        meter.setAnimationEnabled(false);
        meter.setPeakHoldEnabled(false);
        meter.setChannelLabelsVisible(false);
        meter.setScalePosition(ZzAudioLevelMeter::NoScale);
        meter.setColorMode(ZzAudioLevelMeter::ThresholdColors);
        meter.setSegmentCount(2);
        meter.setLevel(0);
        meter.resize(100, 200);
        QImage image(meter.size(), QImage::Format_ARGB32_Premultiplied);
        meter.render(&image);
        QVERIFY(image.pixelColor(50, 30).lightness() > 128);
        meter.setClipColor(Qt::magenta);
        meter.render(&image);
        QCOMPARE(image.pixelColor(50, 30), QColor(Qt::magenta));
        QApplication::setPalette(original);
    }

    void rendersSmallAndMultichannel()
    {
        ZzAudioLevelMeter meter;
        meter.setAnimationEnabled(false);
        meter.setScalePosition(ZzAudioLevelMeter::CenterScale);
        meter.setColorMode(ZzAudioLevelMeter::GradientColors);
        meter.setScaleMode(ZzAudioLevelMeter::FixedTickCount);
        meter.setScaleTickCount(64);
        meter.setLevels({-5, -8, -10, -30, -1, -6, -12, -20});
        for (QSize size : {QSize(1, 1), QSize(24, 20), QSize(420, 220)}) {
            meter.resize(size);
            QImage image(size, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            meter.render(&image);
            QVERIFY(image.pixelColor(0, 0).alpha() > 0);
        }
    }
};

QTEST_MAIN(ZzAudioLevelMeterTest)
#include "ZzAudioLevelMeterTest.moc"
