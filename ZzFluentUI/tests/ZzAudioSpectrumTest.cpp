#include <ZzTestEventLoop.h>
#include <ZzFluentUI/ZzAudioSpectrum.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <QImage>
#include <QHideEvent>
#include <QShowEvent>
#include <QPainter>
#include <QPointer>
#include <QProxyStyle>
#include <QSignalSpy>
#include <QTest>
#include <cmath>
#include <limits>
#include <thread>

using ZzFluentUI::ZzAudioSpectrum;
class SpectrumMotionStyle final : public QProxyStyle
{
public:
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr, QStyleHintReturn *data = nullptr) const override
    {
        return hint == SH_Widget_Animate ? 1 : QProxyStyle::styleHint(hint, option, widget, data);
    }
};
namespace {
QVector<float> tone(int size = 2048)
{
    QVector<float> result(size);
    for (int i = 0; i < size; ++i)
        result[i] = float(0.8 * std::sin(2 * 3.141592653589793 * 1000 * i / 44100));
    return result;
}
qreal maximum(const QVector<qreal> &values)
{
    return values.isEmpty() ? 0 : *std::max_element(values.begin(), values.end());
}
QImage renderSpectrum(ZzAudioSpectrum &s, qreal dpr = 1)
{
    QImage result(s.size() * dpr, QImage::Format_ARGB32_Premultiplied);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    s.render(&painter);
    return result;
}
}
class ZzAudioSpectrumTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsAndBounds()
    {
        ZzAudioSpectrum s;
        QCOMPARE(s.levels().size(), 64);
        QCOMPARE(s.centerFrequencies().size(), 64);
        s.setSampleRate(1); QCOMPARE(s.sampleRate(), 8000);
        QVERIFY(s.centerFrequencies().last() < 4000);
        s.setChannelCount(20); QCOMPARE(s.channelCount(), 8);
        s.setBarCount(10000); QCOMPARE(s.barCount(), 256);
        s.setBarCount(1); QCOMPARE(s.barCount(), 8);
        s.setFftSize(123); QCOMPARE(s.fftSize(), 2048);
        s.setSpectralTilt(100); QCOMPARE(s.spectralTilt(), 9.0);
        s.setSpectralTilt(-100); QCOMPARE(s.spectralTilt(), -6.0);
        s.setMinimumFrequency(std::numeric_limits<qreal>::quiet_NaN());
        QVERIFY(std::isfinite(s.minimumFrequency()));
        s.setReflectionOpacity(9); QCOMPARE(s.reflectionOpacity(), 1.0);
    }
    void externalIsExplicitAndValidated()
    {
        ZzAudioSpectrum s;
        s.setAnimationEnabled(false);
        const QVector<qreal> full(64, 0.8);
        s.setBandLevels(full); QCOMPARE(maximum(s.levels()), 0.0);
        s.setInputMode(ZzAudioSpectrum::ExternalBands);
        s.setBandLevels(full); QCOMPARE(s.levels(), full);
        QCOMPARE(s.displayedLevels(), full);
        s.setBandLevels({1}); QCOMPARE(s.levels(), full);
        auto invalid = full; invalid[0] = std::numeric_limits<qreal>::infinity();
        s.setBandLevels(invalid); QCOMPARE(s.levels(), full);
        s.clear(); QCOMPARE(maximum(s.levels()), 0.0);
        QCOMPARE(maximum(s.displayedLevels()), 0.0); QVERIFY(!s.isRunning());
    }
    void pcmProducerAndClearQueued()
    {
        ZzAudioSpectrum s; s.setAnimationEnabled(false); s.show();
        std::thread producer([&] { s.pushSamples(tone()); }); producer.join();
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 500);
        s.pushSamples(tone()); s.clear();
        QApplication::processEvents();
        QCOMPARE(maximum(s.levels()), 0.0);
        s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 500);
        s.pushSamples(tone()); s.clear(); s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 500);
    }
    void deliveredWakeDoesNotStarveReset_data()
    {
        QTest::addColumn<bool>("formatReset");
        QTest::newRow("clear") << false;
        QTest::newRow("format-reset") << true;
    }
    void deliveredWakeDoesNotStarveReset()
    {
        QFETCH(bool, formatReset);
        ZzAudioSpectrum s; s.setAnimationEnabled(false);
        s.setRefreshInterval(60); s.setInputTimeout(500); s.show();
        s.pushSamples(tone());
        QCoreApplication::sendPostedEvents(&s, QEvent::MetaCall);
        QVERIFY(s.isRunning()); QCOMPARE(maximum(s.levels()), 0.0);
        if (formatReset) s.setFftSize(1024);
        else s.clear();
        QVERIFY(!s.isRunning());
        s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
    }
    void timeoutAndHiddenResume()
    {
        ZzAudioSpectrum s; s.setAnimationEnabled(false); s.setInputTimeout(50);
        s.pushSamples(tone()); QTest::qSleep(80); s.show();
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(!s.isRunning(), 300);
        QCOMPARE(maximum(s.levels()), 0.0);
        s.setInputTimeout(0);
        s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
        s.setInputTimeout(50);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(!s.isRunning(), 300);
        QCOMPARE(maximum(s.displayedLevels()), 0.0);
        s.setInputTimeout(0);
        s.hide(); s.pushSamples(tone()); QApplication::processEvents();
        QVERIFY(!s.isRunning()); s.show();
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
        s.setEnabled(false); QVERIFY(!s.isRunning());
        s.pushSamples(tone()); s.setEnabled(true);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
    }
    void timedOutPcmCannotReplayOldWindow_data()
    {
        QTest::addColumn<bool>("hidden");
        QTest::addColumn<int>("resumeInterval");
        QTest::newRow("visible") << false << 10;
        QTest::newRow("hidden") << true << 10;
        QTest::newRow("visible-delayed-tick") << false << 60;
        QTest::newRow("hidden-delayed-tick") << true << 60;
    }
    void timedOutPcmCannotReplayOldWindow()
    {
        QFETCH(bool, hidden);
        QFETCH(int, resumeInterval);
        ZzAudioSpectrum s; s.setAnimationEnabled(false);
        s.setRefreshInterval(10); s.setInputTimeout(0); s.show();
        s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
        if (hidden) s.hide();
        s.setInputTimeout(40);
        QTest::qSleep(70); QApplication::processEvents();
        s.pushSamples({0});
        // The stale window must be discarded by the new input, not by a later
        // timeout while the test waits for the presentation timer.
        s.setInputTimeout(0);
        s.setRefreshInterval(resumeInterval);
        if (hidden) s.show();
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()), 0.0, 300);
        QCOMPARE(maximum(s.displayedLevels()), 0.0);
        s.pushSamples(QVector<float>(2047, 0));
        QTest::qWait(20); QCOMPARE(maximum(s.levels()), 0.0);
        s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
    }
    void signalsAllowDeletionAndReentry()
    {
        auto *s = new ZzAudioSpectrum;
        s->setAnimationEnabled(false); s->setInputMode(ZzAudioSpectrum::ExternalBands);
        QPointer<ZzAudioSpectrum> guard(s);
        connect(s, &ZzAudioSpectrum::levelsChanged, s, [s] { delete s; });
        s->setBandLevels(QVector<qreal>(64, 1)); QVERIFY(!guard);
        ZzAudioSpectrum reentrant;
        reentrant.setAnimationEnabled(false);
        reentrant.setInputMode(ZzAudioSpectrum::ExternalBands);
        connect(&reentrant, &ZzAudioSpectrum::levelsChanged, &reentrant,
                [&reentrant] { if (maximum(reentrant.levels()) > 0) reentrant.clear(); });
        reentrant.setBandLevels(QVector<qreal>(64, 1));
        QCOMPARE(maximum(reentrant.displayedLevels()), 0.0);
        QCOMPARE(maximum(reentrant.peakLevels()), 0.0);
    }
    void peakHoldStartsWhenDirectPeakArrives()
    {
        SpectrumMotionStyle style;
        ZzAudioSpectrum s; s.setStyle(&style);
        s.setInputMode(ZzAudioSpectrum::ExternalBands); s.setInputTimeout(0);
        s.setAnimationEnabled(false); s.show();
        s.setBandLevels(QVector<qreal>(64, 1));
        // Observe the hold phase without requiring the OS to schedule us inside
        // a 250 ms window. A missing arrival timestamp still fails this check.
        s.setAnimationEnabled(true); s.setPeakHoldDuration(10000);
        s.setBandLevels(QVector<qreal>(64, 0));
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.displayedLevels()) < 1, 300);
        QCOMPARE(maximum(s.peakLevels()), 1.0);
        s.setPeakHoldDuration(0);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.peakLevels()) < 1, 300);
        QVERIFY(maximum(s.peakLevels()) > maximum(s.displayedLevels()));
    }
    void reducedMotionRetainsTimeout()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(true);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzAudioSpectrum s; s.setStyle(&style);
        s.setInputMode(ZzAudioSpectrum::ExternalBands);
        s.setInputTimeout(40); s.show();
        s.setBandLevels(QVector<qreal>(64, 0.75));
        QCOMPARE(s.displayedLevels(), s.levels());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(!s.isRunning(), 250);
        QCOMPARE(maximum(s.displayedLevels()), 0.0);
    }
    void pcmBytesAndNewestWindowOnly()
    {
        ZzAudioSpectrum s; s.setAnimationEnabled(false); s.setInputTimeout(0); s.show();
        const auto samples = tone();
        QByteArray bytes;
        for (float sample : samples) {
            const int value = qRound(sample * 32767);
            bytes.append(char(value & 255)); bytes.append(char((value >> 8) & 255));
        }
        s.pushPcm16Le(bytes.first(3)); s.pushPcm16Le(bytes.sliced(3));
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
        s.hide();
        s.pushSamples(tone(2048 * 10));
        s.pushSamples(QVector<float>(2048, 0));
        s.show();
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()), 0.0, 300);
        s.setInputMode(ZzAudioSpectrum::ExternalBands);
        s.pushSamples(tone());
        s.setBandLevels(QVector<qreal>(64, 0.3));
        QApplication::processEvents(); QCOMPARE(maximum(s.levels()), 0.3);
        s.setInputMode(ZzAudioSpectrum::Pcm);
        s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
    }
    void renderReflectionAndDpi()
    {
        ZzAudioSpectrum s; s.resize(244, 164); s.setBarCount(8);
        s.setAnimationEnabled(false); s.setInputMode(ZzAudioSpectrum::ExternalBands);
        s.setRainbowEnabled(false); s.setAccentColor(Qt::red); s.setBackgroundColor(Qt::black);
        s.setCornerRadius(0); s.setPeakEnabled(false); s.setInputTimeout(0);
        s.setBandLevels({0.3, 1, 0, 0, 0, 0, 0, 0});
        const auto image = renderSpectrum(s);
        // Baseline is 104.4; the short first bar ends at y=74 and its
        // true reflected counterpart ends at y=137, not at the bottom.
        QVERIFY(image.pixelColor(17, 90).red() > 220);
        QVERIFY(image.pixelColor(17, 112).red() > image.pixelColor(17, 130).red());
        QCOMPARE(image.pixelColor(17, 145), QColor(Qt::black));
        QVERIFY(image.pixelColor(47, 145).red() > 0);
        s.setReflectionEnabled(false);
        const auto flat = renderSpectrum(s);
        QVERIFY(flat.pixelColor(17, 145).red() > 220);
        const auto dpi = renderSpectrum(s, 2);
        QVERIFY(dpi.pixelColor(34, 290).red() > 220);
        s.setReflectionEnabled(true); s.setAccentColor(QColor(255, 0, 0, 0));
        const auto transparent = renderSpectrum(s);
        QCOMPARE(transparent.pixelColor(47, 112), QColor(Qt::black));
        s.resize(5, 7); s.setBarCount(256);
        s.setBandLevels(QVector<qreal>(256, 1)); renderSpectrum(s, 2);
        QCOMPARE(s.barCount(), 256); QCOMPARE(s.levels().size(), 256);
    }
    void highContrastOverridesRainbowAndCustomColors()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(ZzFluentUI::ZzThemeMode::HighContrast);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzAudioSpectrum s; s.setStyle(&style); s.resize(244, 164);
        s.setBarCount(8); s.setInputMode(ZzAudioSpectrum::ExternalBands);
        s.setAnimationEnabled(false); s.setReflectionEnabled(false); s.setInputTimeout(0);
        s.setAccentColor(Qt::red); s.setBackgroundColor(Qt::green);
        s.setBandLevels(QVector<qreal>(8, 1));
        const auto image = renderSpectrum(s);
        QCOMPARE(image.pixelColor(17, 80), style.themeSnapshot()->color(ZzFluentUI::ZzColorToken::TextPrimary));
        QCOMPARE(image.pixelColor(1, 1), style.themeSnapshot()->color(ZzFluentUI::ZzColorToken::Surface));
        QCOMPARE(image.pixelColor(17, 80), image.pixelColor(227, 80));
    }
    void configurationSignalsAllowDeletion()
    {
        for (int signal = 0; signal < 3; ++signal) {
            auto *s = new ZzAudioSpectrum;
            QPointer<ZzAudioSpectrum> guard(s);
            if (signal == 0) {
                connect(s, &ZzAudioSpectrum::configurationChanged, s, [s] { delete s; });
                s->setBarCount(8);
            } else if (signal == 1) {
                connect(s, &ZzAudioSpectrum::appearanceChanged, s, [s] { delete s; });
                s->setBarGap(4);
            } else {
                s->show();
                connect(s, &ZzAudioSpectrum::runningChanged, s, [s] { delete s; });
                s->clear();
            }
            QVERIFY(!guard);
        }
    }
    void presentationSignalsAllowDeletion()
    {
        for (int signal = 0; signal < 2; ++signal) {
            auto *s = new ZzAudioSpectrum;
            s->setAnimationEnabled(false); s->setInputMode(ZzAudioSpectrum::ExternalBands);
            QPointer<ZzAudioSpectrum> guard(s);
            if (signal == 0)
                connect(s, &ZzAudioSpectrum::displayedLevelsChanged, s, [s] { delete s; });
            else
                connect(s, &ZzAudioSpectrum::peakLevelsChanged, s, [s] { delete s; });
            s->setBandLevels(QVector<qreal>(64, 0.8));
            QVERIFY(!guard);
        }
        auto *queued = new ZzAudioSpectrum;
        queued->pushSamples(tone()); delete queued;
        QApplication::processEvents();
        auto *ticking = new ZzAudioSpectrum;
        ticking->setAnimationEnabled(false); ticking->show();
        QPointer<ZzAudioSpectrum> guard(ticking);
        connect(ticking, &ZzAudioSpectrum::levelsChanged, ticking, [ticking] { delete ticking; });
        ticking->pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(!guard, 300);
    }
    void settersBoundAndSuppressUnchangedSignals()
    {
        ZzAudioSpectrum s;
        QSignalSpy configuration(&s, &ZzAudioSpectrum::configurationChanged);
        QSignalSpy appearance(&s, &ZzAudioSpectrum::appearanceChanged);
        s.setSampleRate(s.sampleRate()); s.setBarGap(s.barGap());
        QCOMPARE(configuration.count(), 0); QCOMPARE(appearance.count(), 0);
        s.setChannelMix(ZzAudioSpectrum::WaveformAverage);
        QCOMPARE(s.channelMix(), ZzAudioSpectrum::WaveformAverage);
        s.setChannelMix(static_cast<ZzAudioSpectrum::ChannelMix>(100));
        QCOMPARE(configuration.count(), 1);
        s.setInputMode(static_cast<ZzAudioSpectrum::InputMode>(100));
        QCOMPARE(s.inputMode(), ZzAudioSpectrum::Pcm);
        s.setMinimumDecibels(-1000); QCOMPARE(s.minimumDecibels(), -160.0);
        s.setMaximumDecibels(1000); QCOMPARE(s.maximumDecibels(), 24.0);
        s.setMinimumFrequency(0); QCOMPARE(s.minimumFrequency(), 1.0);
        s.setMaximumFrequency(1e9); QCOMPARE(s.maximumFrequency(), s.sampleRate() / 2.0);
        s.setRefreshInterval(0); QCOMPARE(s.refreshInterval(), 1);
        s.setInputTimeout(-1); QCOMPARE(s.inputTimeout(), 0);
        s.setGravity(-2); QCOMPARE(s.gravity(), 0.01);
        s.setPeakHoldDuration(-1); QCOMPARE(s.peakHoldDuration(), 0);
        s.setBarGap(-2); QCOMPARE(s.barGap(), 0.0);
        s.setCornerRadius(1e9); QCOMPARE(s.cornerRadius(), 64.0);
        s.setPeakHeight(-1); QCOMPARE(s.peakHeight(), 0.0);
        s.setInputMode(ZzAudioSpectrum::ExternalBands); s.setAnimationEnabled(false);
        s.setBandLevels(QVector<qreal>(64, 5)); QCOMPARE(maximum(s.levels()), 1.0);
        s.setBandLevels(QVector<qreal>(64, -5)); QCOMPARE(maximum(s.levels()), 0.0);
    }
    void pcmRefreshIsRateLimited()
    {
        ZzAudioSpectrum s; s.setAnimationEnabled(false); s.setInputTimeout(1000);
        s.setRefreshInterval(100); s.show();
        QSignalSpy levels(&s, &ZzAudioSpectrum::levelsChanged);
        for (int i = 0; i < 30; ++i) s.pushSamples(tone());
        QTest::qWait(20); QCOMPARE(maximum(s.levels()), 0.0);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 400);
        QCOMPARE(levels.count(), 1);
        QTest::qWait(120); QCOMPARE(levels.count(), 1);
        s.clear(); QVERIFY(!s.isRunning());
    }
    void hideEventStopsEvenWhenVisibleFlagRemainsSet()
    {
        ZzAudioSpectrum s; s.setAnimationEnabled(false); s.show();
        s.pushSamples(tone());
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(maximum(s.levels()) > 0.5, 300);
        // A spontaneous window-manager hide can leave QWidget::isVisible true.
        QHideEvent hidden; QApplication::sendEvent(&s, &hidden);
        QVERIFY(s.isVisible()); QVERIFY(!s.isRunning());
        s.pushSamples(tone()); QApplication::processEvents();
        QVERIFY(!s.isRunning());
        QShowEvent shown; QApplication::sendEvent(&s, &shown);
        QVERIFY(s.isRunning());
    }
    void directExternalBurstUsesLatestFrame_data()
    {
        QTest::addColumn<bool>("reducedMotion");
        QTest::newRow("animation-disabled") << false;
        QTest::newRow("reduced-motion") << true;
    }
    void directExternalBurstUsesLatestFrame()
    {
        QFETCH(bool, reducedMotion);
        ZzFluentUI::ZzThemeController controller;
        controller.setReducedMotion(reducedMotion);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzAudioSpectrum s; s.setStyle(&style);
        s.setAnimationEnabled(reducedMotion);
        s.setInputMode(ZzAudioSpectrum::ExternalBands);
        s.setRefreshInterval(100); s.setInputTimeout(350); s.show();
        QSignalSpy displayed(&s, &ZzAudioSpectrum::displayedLevelsChanged);
        QSignalSpy peaks(&s, &ZzAudioSpectrum::peakLevelsChanged);
        s.setBandLevels(QVector<qreal>(64, 0.2));
        QCOMPARE(maximum(s.displayedLevels()), 0.2);
        QCOMPARE(displayed.count(), 1);
        for (int i = 3; i <= 9; ++i) s.setBandLevels(QVector<qreal>(64, qreal(i) / 10));
        QCOMPARE(maximum(s.levels()), 0.9);
        QCOMPARE(maximum(s.displayedLevels()), 0.2);
        QCOMPARE(maximum(s.peakLevels()), 0.2);
        QCOMPARE(displayed.count(), 1); QCOMPARE(peaks.count(), 1);
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(maximum(s.displayedLevels()), 0.9, 250);
        QCOMPARE(displayed.count(), 2); QCOMPARE(peaks.count(), 2);
        s.setBandLevels(QVector<qreal>(64, 0.4));
        QCOMPARE(maximum(s.displayedLevels()), 0.9);
        ZZ_COMPARE_EVENTUALLY_WITH_TIMEOUT(maximum(s.displayedLevels()), 0.4, 250);
        QCOMPARE(displayed.count(), 3);
        ZZ_VERIFY_EVENTUALLY_WITH_TIMEOUT(!s.isRunning(), 600);
        QCOMPARE(maximum(s.displayedLevels()), 0.0);
        s.setBandLevels(QVector<qreal>(64, 0.7));
        QCOMPARE(maximum(s.displayedLevels()), 0.7);
    }
    void directRenderBenchmark_data()
    {
        QTest::addColumn<int>("bars");
        QTest::newRow("64-bars") << 64;
        QTest::newRow("256-bars") << 256;
    }
    void liveThemeSwitchAndRainbowColors()
    {
        ZzFluentUI::ZzThemeController controller;
        controller.setMode(ZzFluentUI::ZzThemeMode::Light);
        ZzFluentUI::ZzFluentStyle style(&controller);
        ZzAudioSpectrum s; s.setStyle(&style); s.resize(244, 164);
        s.setBarCount(8); s.setAnimationEnabled(false);
        s.setInputMode(ZzAudioSpectrum::ExternalBands); s.setInputTimeout(0);
        s.setReflectionEnabled(false); s.show();
        s.setBandLevels(QVector<qreal>(8, 0.8));
        const auto light = renderSpectrum(s);
        QVERIFY(light.pixelColor(17, 80) != light.pixelColor(227, 80));
        QCOMPARE(light.pixelColor(1, 1), style.themeSnapshot()->color(ZzFluentUI::ZzColorToken::Surface));
        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        const auto dark = renderSpectrum(s);
        QVERIFY(dark.pixelColor(1, 1) != light.pixelColor(1, 1));
        QCOMPARE(dark.pixelColor(1, 1), style.themeSnapshot()->color(ZzFluentUI::ZzColorToken::Surface));
        s.setRainbowEnabled(false);
        const auto mono = renderSpectrum(s);
        QCOMPARE(mono.pixelColor(17, 80), mono.pixelColor(227, 80));
        QCOMPARE(mono.pixelColor(17, 80), style.themeSnapshot()->color(ZzFluentUI::ZzColorToken::Accent));
        controller.setMode(ZzFluentUI::ZzThemeMode::Light);
        QCOMPARE(renderSpectrum(s).pixelColor(1, 1), light.pixelColor(1, 1));
    }
    void rendersHeldPeakAboveDecayedBars()
    {
        SpectrumMotionStyle style;
        ZzAudioSpectrum s; s.setStyle(&style); s.resize(244, 164);
        s.setBarCount(8); s.setInputMode(ZzAudioSpectrum::ExternalBands);
        s.setAnimationEnabled(false); s.setInputTimeout(0); s.setReflectionEnabled(false);
        s.setRainbowEnabled(false); s.setAccentColor(Qt::red); s.setBackgroundColor(Qt::black);
        s.setPeakHeight(4); s.setPeakHoldDuration(1000); s.setGravity(100); s.show();
        s.setBandLevels(QVector<qreal>(8, 0.8)); s.setAnimationEnabled(true);
        s.setBandLevels(QVector<qreal>(8, 0));
        QTest::qSleep(110); QApplication::processEvents();
        QVERIFY(maximum(s.displayedLevels()) < 0.5);
        QCOMPARE(maximum(s.peakLevels()), 0.8);
        const auto peaks = renderSpectrum(s);
        QCOMPARE(peaks.pixelColor(17, 32), QColor(Qt::red));
        QCOMPARE(peaks.pixelColor(17, 80), QColor(Qt::black));
        s.setPeakEnabled(false);
        QCOMPARE(renderSpectrum(s).pixelColor(17, 32), QColor(Qt::black));
    }
    void directRenderBenchmark()
    {
        if (!qEnvironmentVariableIsSet("ZZ_SPECTRUM_BENCHMARK"))
            QSKIP("Set ZZ_SPECTRUM_BENCHMARK=1 to measure direct setBandLevels + render.");
        QFETCH(int, bars);
        ZzAudioSpectrum s; s.resize(800, 320);
        s.setInputMode(ZzAudioSpectrum::ExternalBands); s.setBarCount(bars);
        s.setAnimationEnabled(false); s.setInputTimeout(0);
        QVector<qreal> first(bars), second(bars);
        for (int i = 0; i < bars; ++i) {
            first[i] = 0.1 + 0.9 * qreal(i + 1) / bars;
            second[i] = 1.1 - first[i];
        }
        QImage image(s.size(), QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&image);
        s.setBandLevels(first); s.render(&painter);
        bool alternate = false;
        // The hidden widget intentionally exercises the synchronous direct
        // update and 800x320 image paint, without scheduling or timer jitter.
        QBENCHMARK {
            alternate = !alternate;
            s.setBandLevels(alternate ? second : first);
            s.render(&painter);
        }
        QCOMPARE(s.displayedLevels(), alternate ? second : first);
    }
};
QTEST_MAIN(ZzAudioSpectrumTest)
#include "ZzAudioSpectrumTest.moc"
