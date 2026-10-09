#include "../ZzExampleAudioLevels.h"
#include <QAudioBuffer>
#include <QTest>
#include <array>
#include <limits>

class ZzExampleAudioLevelsTest : public QObject
{
    Q_OBJECT
private:
    template <typename T, size_t N>
    QAudioBuffer buffer(const std::array<T, N> &samples, QAudioFormat::SampleFormat kind, int channels)
    {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(channels);
        format.setSampleFormat(kind);
        return QAudioBuffer(
            QByteArray(reinterpret_cast<const char *>(samples.data()), sizeof(samples)), format);
    }
private Q_SLOTS:
    // 抓住左右声道混合、使用RMS替代峰值或20log10写错的转换。
    void stereoPeaksStayIndependent()
    {
        const auto levels = ZzExample::ZzExampleAudioLevels::peaks(
            buffer(std::array<float, 6> { 0.5f, 0.0f, -0.25f, -1.0f, 0.0f, 0.25f }, QAudioFormat::Float, 2));
        QCOMPARE(levels.size(), 2);
        QVERIFY(qAbs(levels[0] - (-6.020599913)) < 0.00001);
        QCOMPARE(levels[1], 0.0);
    }
    void integerFormatsAndSilence()
    {
        const auto i16 = ZzExample::ZzExampleAudioLevels::peaks(
            buffer(std::array<qint16, 2> { -32768, 16384 }, QAudioFormat::Int16, 2));
        QCOMPARE(i16[0], 0.0);
        QVERIFY(qAbs(i16[1] + 6.020599913) < 0.00001);
        const auto i32 = ZzExample::ZzExampleAudioLevels::peaks(
            buffer(std::array<qint32, 2> { std::numeric_limits<qint32>::min(), 0 }, QAudioFormat::Int32, 2));
        QCOMPARE(i32[0], 0.0);
        QCOMPARE(i32[1], -160.0);
        const auto u8 = ZzExample::ZzExampleAudioLevels::peaks(
            buffer(std::array<quint8, 4> { 128, 128, 0, 192 }, QAudioFormat::UInt8, 2));
        QCOMPARE(u8[0], 0.0);
        QVERIFY(qAbs(u8[1] + 6.020599913) < 0.00001);
    }
    void invalidAndNonFiniteInput()
    {
        QVERIFY(ZzExample::ZzExampleAudioLevels::peaks(QAudioBuffer()).isEmpty());
        const auto levels = ZzExample::ZzExampleAudioLevels::peaks(
            buffer(std::array<float, 4> { std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN(), 0.0f, -0.5f },
                QAudioFormat::Float, 2));
        QCOMPARE(levels[0], -160.0);
        QVERIFY(qAbs(levels[1] + 6.020599913) < 0.00001);
    }
    void multichannelKeepsFirstEight()
    {
        const auto levels = ZzExample::ZzExampleAudioLevels::peaks(
            buffer(std::array<float, 10> { 1, 0.5f, 0, 0, 0, 0, 0, 0.25f, 1, 1 }, QAudioFormat::Float, 10));
        QCOMPARE(levels.size(), 8);
        QCOMPARE(levels[0], 0.0);
        QVERIFY(qAbs(levels[7] + 12.041199827) < 0.00001);
    }
};
QTEST_GUILESS_MAIN(ZzExampleAudioLevelsTest)
#include "ZzExampleAudioLevelsTest.moc"
