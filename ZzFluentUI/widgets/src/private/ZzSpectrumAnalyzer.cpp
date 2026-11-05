#include "ZzSpectrumAnalyzer.h"

#include <kiss_fftr.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ZzFluentUI {
struct ZzSpectrumAnalyzer::State
{
    ZzSpectrumSettings settings;
    kiss_fftr_cfg fft = nullptr;
    QVector<float> window, input;
    QVector<kiss_fft_cpx> output;
    QVector<qreal> magnitudes, centers, edges;
    qreal windowSum = 1;
    ~State() { if (fft) kiss_fftr_free(fft); }
};
ZzSpectrumAnalyzer::ZzSpectrumAnalyzer() : state_(std::make_unique<State>()) { configure({}); }
ZzSpectrumAnalyzer::~ZzSpectrumAnalyzer() = default;
void ZzSpectrumAnalyzer::configure(const ZzSpectrumSettings &settings)
{
    auto next = std::make_unique<State>();
    next->settings = settings;
    const int n = settings.fftSize;
    next->fft = kiss_fftr_alloc(n, 0, nullptr, nullptr);
    if (!next->fft) throw std::bad_alloc();
    next->window.resize(n);
    next->input.resize(n);
    next->output.resize(n / 2 + 1);
    next->magnitudes.fill(0, n / 2 + 1);
    next->windowSum = 0;
    for (int i = 0; i < n; ++i) {
        next->window[i] = static_cast<float>(0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / n));
        next->windowSum += next->window[i];
    }
    const qreal high = std::min(settings.maximumFrequency, settings.sampleRate / 2.0);
    const qreal low = std::clamp(settings.minimumFrequency, 1.0, high - 1.0);
    next->edges.resize(settings.bars + 1);
    next->centers.resize(settings.bars);
    for (int i = 0; i <= settings.bars; ++i) {
        next->edges[i] = low * std::pow(high / low, qreal(i) / settings.bars);
        if (i > 0) next->centers[i - 1] = std::sqrt(next->edges[i - 1] * next->edges[i]);
    }
    state_.swap(next);
}

QVector<qreal> ZzSpectrumAnalyzer::analyze(const QVector<float> &interleaved)
{
    auto &d = *state_;
    const auto &s = d.settings;
    const int n = s.fftSize;
    const int bins = n / 2 + 1;
    QVector<qreal> result(s.bars, 0);
    d.magnitudes.fill(0);
    if (interleaved.size() < n * s.channels) return result;
    const qsizetype start = interleaved.size() - n * s.channels;
    const auto sample = [&interleaved](qsizetype index) {
        const float value = interleaved[index];
        return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
    };
    const int transforms = s.powerAverage ? s.channels : 1;
    for (int channel = 0; channel < transforms; ++channel) {
        for (int i = 0; i < n; ++i) {
            const qsizetype index = start + i * s.channels;
            float value = 0;
            if (s.powerAverage) value = sample(index + channel);
            else {
                for (int c = 0; c < s.channels; ++c) value += sample(index + c);
                value /= static_cast<float>(s.channels);
            }
            d.input[i] = value * d.window[i];
        }
        kiss_fftr(d.fft, d.input.constData(), d.output.data());
        for (int i = 0; i < bins; ++i) {
            const qreal re = d.output[i].r;
            const qreal im = d.output[i].i;
            const qreal scale = (i == 0 || i == bins - 1 ? 1.0 : 2.0) / d.windowSum;
            d.magnitudes[i] += (re * re + im * im) * scale * scale / transforms;
        }
    }
    for (auto &value : d.magnitudes) value = std::sqrt(value);
    const auto interpolated = [&d, bins](qreal bin) {
        bin = std::clamp(bin, 0.0, qreal(bins - 1));
        const int left = static_cast<int>(bin);
        const int right = std::min(left + 1, bins - 1);
        const qreal t = bin - left;
        return d.magnitudes[left] * (1 - t) + d.magnitudes[right] * t;
    };
    for (int band = 0; band < s.bars; ++band) {
        const qreal low = d.edges[band] * n / s.sampleRate;
        const qreal high = d.edges[band + 1] * n / s.sampleRate;
        qreal amplitude = 0;
        if (high - low < 1.5) {
            amplitude = 0.25 * interpolated(low) + 0.5 * interpolated((low + high) / 2)
                + 0.25 * interpolated(high);
        } else {
            const int first = std::clamp(static_cast<int>(std::floor(low)), 1, bins - 1);
            const int last = std::clamp(static_cast<int>(std::ceil(high)), first + 1, bins);
            qreal power = 0, peak = 0;
            for (int bin = first; bin < last; ++bin) {
                power += d.magnitudes[bin] * d.magnitudes[bin];
                peak = std::max(peak, d.magnitudes[bin]);
            }
            amplitude = 0.3 * std::sqrt(power / (last - first)) + 0.7 * peak;
        }
        if (amplitude <= 0) continue;
        const qreal tilt = std::clamp(s.tilt * std::log2(d.centers[band] / 1000), -6.0, 9.0);
        const qreal db = 20 * std::log10(amplitude) + tilt;
        result[band] = std::clamp((db - s.minimumDecibels) /
            (s.maximumDecibels - s.minimumDecibels), 0.0, 1.0);
    }
    return result;
}
QVector<qreal> ZzSpectrumAnalyzer::centerFrequencies() const { return state_->centers; }
QVector<qreal> ZzSpectrumAnalyzer::magnitudes() const { return state_->magnitudes; }
} // namespace ZzFluentUI
