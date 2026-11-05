#pragma once

#include <memory>
#include <QVector>

namespace ZzFluentUI {

/** @brief 仅包含数值分析参数，不依赖窗口或音频设备。 */
struct ZzSpectrumSettings final
{
    int sampleRate = 44100;
    int channels = 1;
    int fftSize = 2048;
    int bars = 64;
    qreal minimumFrequency = 60;
    qreal maximumFrequency = 16000;
    qreal minimumDecibels = -54;
    qreal maximumDecibels = -3;
    qreal tilt = 3;
    bool powerAverage = true;
};

/** @brief Hann 加窗实数 FFT；所有调用由同一个线程串行执行。 */
class ZzSpectrumAnalyzer final
{
public:
    ZzSpectrumAnalyzer();
    ~ZzSpectrumAnalyzer();
    ZzSpectrumAnalyzer(const ZzSpectrumAnalyzer &) = delete;
    ZzSpectrumAnalyzer &operator=(const ZzSpectrumAnalyzer &) = delete;
    void configure(const ZzSpectrumSettings &settings);
    [[nodiscard]] QVector<qreal> analyze(const QVector<float> &interleaved);
    [[nodiscard]] QVector<qreal> centerFrequencies() const;
    /** @brief 最近一次 FFT 的窗增益校正幅度，未应用视觉倾斜。 */
    [[nodiscard]] QVector<qreal> magnitudes() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace ZzFluentUI
