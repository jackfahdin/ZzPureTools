#include "../widgets/src/private/ZzSpectrumAnalyzer.h"
#include "../widgets/src/private/ZzSpectrumInput.h"

#include <QtTest/QTest>
#include <cmath>
#include <numbers>
#include <thread>

using namespace ZzFluentUI;

namespace {
QVector<float> signal(int frames, int channels, int bin, qreal amplitude, bool antiphase = false)
{
    QVector<float> result(frames * channels);
    for (int frame = 0; frame < frames; ++frame) {
        const auto value = static_cast<float>(amplitude * std::sin(
            2 * std::numbers::pi * bin * frame / frames));
        for (int channel = 0; channel < channels; ++channel)
            result[frame * channels + channel] = antiphase && channel % 2 ? -value : value;
    }
    return result;
}
}

/** @brief 以已知信号验证 FFT 数值，而非仅检查图形是否变化。 */
class ZzSpectrumAnalyzerTest final : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void hugeSplitInputMatchesOrderedStream()
    {
        for (int channels : {1, 2, 3, 8}) {
            ZzSpectrumInput streamed, burst;
            streamed.configure(1024, channels);
            burst.configure(1024, channels);
            QByteArray bytes(40001 * channels * 2 + 3, Qt::Uninitialized);
            for (qsizetype i = 0; i < bytes.size(); ++i)
                bytes[i] = static_cast<char>((i * 19 + i / 5) % 256);
            for (qsizetype i = 0; i < bytes.size(); i += 77)
                streamed.pushPcm(bytes.mid(i, 77));
            burst.pushPcm(bytes.first(3));
            burst.pushPcm(bytes.mid(3));
            QCOMPARE(burst.takeLatest().samples, streamed.takeLatest().samples);
            const QByteArray tail = QByteArray::fromHex("ffffffffffffff");
            streamed.pushPcm(tail);
            burst.pushPcm(tail);
            QCOMPARE(burst.takeLatest().samples, streamed.takeLatest().samples);

            QVector<float> floats(40001 * channels + 3);
            for (qsizetype i = 0; i < floats.size(); ++i)
                floats[i] = static_cast<float>(i % 201 - 100) / 100.0f;
            for (qsizetype i = 0; i < floats.size(); i += 77)
                streamed.pushFloats(floats.mid(i, 77));
            burst.pushFloats(floats.first(3));
            burst.pushFloats(floats.mid(3));
            QCOMPARE(burst.takeLatest().samples, streamed.takeLatest().samples);
        }
    }

    void workerInputAndClearNeverRestoreOlderSamples()
    {
        ZzSpectrumInput input;
        input.configure(1024, 1);
        std::jthread producer([&input] {
            for (int i = 0; i < 400; ++i) input.pushFloats(QVector<float>(64, 0.5f));
        });
        for (int i = 0; i < 100; ++i) {
            const auto snapshot = input.takeLatest();
            QVERIFY(snapshot.samples.size() <= 1024);
        }
        producer.join();
        input.acknowledgeWake();
        const auto before = input.takeLatest();
        QCOMPARE(before.samples, QVector<float>(1024, 0.5f));
        input.clear();
        const auto after = input.takeLatest();
        QVERIFY(after.serial > before.serial);
        QVERIFY(after.samples.isEmpty());
        QVERIFY(input.pushFloats(QVector<float>(1024, -0.5f)));
        QCOMPARE(input.takeLatest().samples, QVector<float>(1024, -0.5f));
    }

    void pcmSplitsAndOverflowPreserveFrames()
    {
        ZzSpectrumInput input;
        input.configure(1024, 2);
        const QByteArray frame = QByteArray::fromHex("004000c0");
        const QByteArray pcm = frame.repeated(1024);
        QVERIFY(input.pushPcm(pcm.first(1)));
        QVERIFY(!input.pushPcm(pcm.mid(1, 2)));
        QVERIFY(!input.pushPcm(pcm.mid(3)));
        auto snapshot = input.takeLatest();
        QCOMPARE(snapshot.samples.size(), 2048);
        QCOMPARE(snapshot.samples[0], 0.5f);
        QCOMPARE(snapshot.samples[1], -0.5f);
        input.acknowledgeWake();
        QVERIFY(input.pushPcm(frame.repeated(30000)));
        QCOMPARE(input.bufferedFrames(), 8192);
        snapshot = input.takeLatest();
        QCOMPARE(snapshot.samples.size(), 2048);
        QCOMPARE(snapshot.samples.last(), -0.5f);
        input.clear();
        QCOMPARE(input.bufferedFrames(), 0);
        QVERIFY(input.takeLatest().samples.isEmpty());
    }

    void formatChangesAndInvalidSamplesAreBounded()
    {
        ZzSpectrumInput input;
        input.configure(1024, 2);
        QVERIFY(input.pushPcm(QByteArray::fromHex("004000")));
        input.configure(1024, 1);
        QVector<float> samples(1024, 2.0f);
        samples[0] = std::numeric_limits<float>::quiet_NaN();
        samples[1] = -std::numeric_limits<float>::infinity();
        input.pushFloats(samples);
        const auto snapshot = input.takeLatest();
        QCOMPARE(snapshot.samples.size(), 1024);
        QCOMPARE(snapshot.samples[0], 0.0f);
        QCOMPARE(snapshot.samples[1], 0.0f);
        QCOMPARE(snapshot.samples[2], 1.0f);
        input.configure(1024, 1, false);
        QVERIFY(!input.pushFloats(samples));
        QVERIFY(input.takeLatest().samples.isEmpty());
    }
    void wakeAcknowledgmentTracksQueuedEventsOnly()
    {
        ZzSpectrumInput input;
        input.configure(1024, 1);
        const QVector<float> frame(1024, 0.5f);
        QVERIFY(input.pushFloats(frame));
        (void)input.takeLatest();
        input.clear();
        QVERIFY(!input.pushFloats(frame));
        input.configure(1024, 1);
        QVERIFY(!input.pushFloats(frame));
        input.acknowledgeWake();
        input.clear();
        QVERIFY(input.pushFloats(frame));
        // Even without a visible consumer, any burst has one outstanding wake.
        for (int i = 0; i < 100; ++i) QVERIFY(!input.pushFloats(frame));
        input.acknowledgeWake();
        input.configure(1024, 1);
        QVERIFY(input.pushFloats(frame));
        (void)input.takeLatest();
        QVERIFY(!input.pushFloats(frame));
    }
    void inputGapExpiresPartialFramesAndRespectsDisabledTimeout()
    {
        ZzSpectrumInput input;
        input.configure(1024, 2, true, 25);
        input.pushPcm(QByteArray::fromHex("ff"));
        QTest::qSleep(35);
        input.pushPcm(QByteArray(4096, char(0)));
        QCOMPARE(input.takeLatest().samples, QVector<float>(2048, 0));
        input.clear();
        input.pushFloats({1});
        QTest::qSleep(35);
        input.pushFloats(QVector<float>(2048, 0));
        QCOMPARE(input.takeLatest().samples, QVector<float>(2048, 0));
        input.configure(1024, 1, true, 0);
        input.pushFloats(QVector<float>(1024, 0.5f));
        QTest::qSleep(35);
        input.pushFloats({0});
        QCOMPARE(input.takeLatest().samples.first(), 0.5f);
        input.configure(1024, 8, true, 25);
        for (int i = 0; i < 8; ++i) {
            input.pushFloats({1});
            QTest::qSleep(10);
        }
        input.pushFloats(QVector<float>(8192 - 8, 0));
        const auto continuous = input.takeLatest().samples;
        QCOMPARE(continuous.size(), 8192);
        for (int i = 0; i < 8; ++i) QCOMPARE(continuous[i], 1.0f);
    }

    void correctsWindowGainAtEveryFftSize()
    {
        for (const int size : {1024, 2048, 4096, 8192}) {
            ZzSpectrumSettings settings;
            settings.fftSize = size;
            settings.tilt = 0;
            ZzSpectrumAnalyzer analyzer;
            analyzer.configure(settings);
            const auto levels = analyzer.analyze(signal(size, 1, 32, 0.5));
            QCOMPARE(levels.size(), 64);
            const auto magnitudes = analyzer.magnitudes();
            QCOMPARE(magnitudes.size(), size / 2 + 1);
            QVERIFY(std::abs(magnitudes[32] - 0.5) < 0.001);
            QVERIFY(magnitudes[100] < 0.0001);
            const auto lower = analyzer.analyze(signal(size, 1, 32, 0.25));
            Q_UNUSED(lower);
            QVERIFY(std::abs(analyzer.magnitudes()[32] - 0.25) < 0.001);
        }
    }

    void retainsOppositePhaseChannels()
    {
        ZzSpectrumSettings settings;
        settings.channels = 2;
        ZzSpectrumAnalyzer analyzer;
        analyzer.configure(settings);
        const auto pcm = signal(2048, 2, 32, 0.5, true);
        const auto power = analyzer.analyze(pcm);
        QVERIFY(*std::max_element(power.cbegin(), power.cend()) > 0.5);
        QVERIFY(std::abs(analyzer.magnitudes()[32] - 0.5) < 0.001);
        settings.powerAverage = false;
        analyzer.configure(settings);
        const auto average = analyzer.analyze(pcm);
        QCOMPARE(*std::max_element(average.cbegin(), average.cend()), 0.0);
    }

    void resolvesTwoTonesAndInterpolatesNarrowBands()
    {
        ZzSpectrumSettings settings;
        settings.tilt = 0;
        settings.bars = 256;
        ZzSpectrumAnalyzer analyzer;
        analyzer.configure(settings);
        auto samples = signal(2048, 1, 5, 0.4);
        const auto upper = signal(2048, 1, 187, 0.2);
        for (int i = 0; i < 2048; ++i) samples[i] += upper[i];
        const auto levels = analyzer.analyze(samples);
        QVERIFY(std::abs(analyzer.magnitudes()[5] - 0.4) < 0.001);
        QVERIFY(std::abs(analyzer.magnitudes()[187] - 0.2) < 0.001);
        const auto centers = analyzer.centerFrequencies();
        for (int bin : {5, 187}) {
            const qreal frequency = qreal(bin) * settings.sampleRate / settings.fftSize;
            const auto nearest = std::min_element(centers.cbegin(), centers.cend(),
                [frequency](qreal a, qreal b) { return std::abs(a - frequency) < std::abs(b - frequency); });
            const auto index = std::distance(centers.cbegin(), nearest);
            QVERIFY(levels[index] > 0.5);
        }
        for (qreal value : levels) QVERIFY(std::isfinite(value) && value >= 0 && value <= 1);
    }

    void handlesSilenceDcNyquistAndFrequencyLimits()
    {
        ZzSpectrumSettings settings;
        settings.sampleRate = 8000;
        settings.minimumFrequency = 3500;
        settings.maximumFrequency = 16000;
        ZzSpectrumAnalyzer analyzer;
        analyzer.configure(settings);
        const auto centers = analyzer.centerFrequencies();
        QCOMPARE(centers.size(), 64);
        QVERIFY(centers.first() >= 3500);
        QVERIFY(centers.last() < 4000);
        const auto silence = analyzer.analyze(QVector<float>(2048, 0));
        QCOMPARE(silence, QVector<qreal>(64, 0));
        const auto dc = analyzer.analyze(QVector<float>(2048, 0.5f));
        Q_UNUSED(dc);
        QVERIFY(std::abs(analyzer.magnitudes().first() - 0.5) < 0.001);
        QVector<float> nyquist(2048);
        for (int i = 0; i < 2048; ++i) nyquist[i] = i % 2 ? -0.5f : 0.5f;
        const auto high = analyzer.analyze(nyquist);
        Q_UNUSED(high);
        QVERIFY(std::abs(analyzer.magnitudes().last() - 0.5) < 0.001);
    }

    void analysisBenchmark_data()
    {
        QTest::addColumn<int>("size");
        QTest::addColumn<int>("channels");
        QTest::addColumn<int>("bars");
        QTest::newRow("default-stereo") << 2048 << 2 << 64;
        QTest::newRow("maximum-eight-channel") << 8192 << 8 << 256;
    }
    void analysisBenchmark()
    {
        QFETCH(int, size);
        QFETCH(int, channels);
        QFETCH(int, bars);
        ZzSpectrumSettings settings;
        settings.fftSize = size;
        settings.channels = channels;
        settings.bars = bars;
        ZzSpectrumAnalyzer analyzer;
        analyzer.configure(settings);
        const auto pcm = signal(size, channels, 32, 0.5);
        QVector<qreal> output;
        QBENCHMARK { output = analyzer.analyze(pcm); }
        QCOMPARE(output.size(), bars);
    }
};

QTEST_GUILESS_MAIN(ZzSpectrumAnalyzerTest)
#include "ZzSpectrumAnalyzerTest.moc"
