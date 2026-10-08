#include "ZzExampleAudioLevels.h"

#include <QAudioBuffer>
#include <algorithm>
#include <cmath>

namespace ZzExample {
namespace {
template <typename Sample, typename Normalize>
void zzReadPeaks(const QAudioBuffer &buffer, QVector<qreal> &peaks, Normalize normalize)
{
    const auto *samples = buffer.constData<Sample>();
    const qsizetype stride = buffer.format().channelCount();
    for (qsizetype frame = 0; frame < buffer.frameCount(); ++frame) {
        for (qsizetype channel = 0; channel < peaks.size(); ++channel) {
            const qreal amplitude = std::abs(normalize(samples[frame * stride + channel]));
            if (std::isfinite(amplitude))
                peaks[channel] = std::max(peaks[channel], amplitude);
        }
    }
}
} // namespace

QVector<qreal> zzAudioBufferLevels(const QAudioBuffer &buffer)
{
    if (!buffer.isValid() || buffer.frameCount() <= 0 || buffer.format().channelCount() <= 0)
        return {};
    QVector<qreal> levels(std::min(8, buffer.format().channelCount()), 0.0);
    switch (buffer.format().sampleFormat()) {
    case QAudioFormat::UInt8:
        zzReadPeaks<quint8>(buffer, levels, [](quint8 value) { return (qreal(value) - 128.0) / 128.0; });
        break;
    case QAudioFormat::Int16:
        zzReadPeaks<qint16>(buffer, levels, [](qint16 value) { return qreal(value) / 32768.0; });
        break;
    case QAudioFormat::Int32:
        zzReadPeaks<qint32>(buffer, levels, [](qint32 value) { return qreal(value) / 2147483648.0; });
        break;
    case QAudioFormat::Float:
        zzReadPeaks<float>(buffer, levels, [](float value) { return qreal(value); });
        break;
    default:
        return {};
    }
    for (auto &level : levels)
        level = level > 0 ? std::clamp(20.0 * std::log10(level), -160.0, 24.0) : -160.0;
    return levels;
}
} // namespace ZzExample
