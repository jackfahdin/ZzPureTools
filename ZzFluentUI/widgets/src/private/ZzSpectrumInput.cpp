#include "ZzSpectrumInput.h"
#include <QMutexLocker>
#include <algorithm>
#include <cmath>

namespace ZzFluentUI {
void ZzSpectrumInput::resetLocked()
{
    write_ = frames_ = partialCount_ = 0;
    lowByte_ = -1;
    typeKnown_ = false;
    timestamp_ = {};
    lastPush_ = {};
    ++serial_;
}
void ZzSpectrumInput::configure(int fftSize, int channels, bool enabled, int inputTimeout)
{
    const QMutexLocker lock(&mutex_);
    fftSize_ = fftSize;
    channels_ = channels;
    enabled_ = enabled;
    inputTimeout_ = inputTimeout;
    ring_.resize(8 * fftSize * channels);
    resetLocked();
}
void ZzSpectrumInput::setInputTimeout(int milliseconds)
{
    const QMutexLocker lock(&mutex_);
    inputTimeout_ = milliseconds;
}
void ZzSpectrumInput::preparePushLocked()
{
    const auto now = Clock::now();
    if (inputTimeout_ > 0 && lastPush_ != Clock::time_point{}
        && std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPush_).count() >= inputTimeout_)
        resetLocked();
    lastPush_ = now;
}
void ZzSpectrumInput::clear()
{
    const QMutexLocker lock(&mutex_);
    resetLocked();
}
void ZzSpectrumInput::appendLocked(float sample)
{
    partial_[static_cast<std::size_t>(partialCount_++)] =
        std::isfinite(sample) ? std::clamp(sample, -1.0f, 1.0f) : 0.0f;
    if (partialCount_ != channels_) return;
    for (int c = 0; c < channels_; ++c) {
        ring_[write_++] = partial_[static_cast<std::size_t>(c)];
        if (write_ == ring_.size()) write_ = 0;
    }
    frames_ = std::min(frames_ + 1, fftSize_ * 8);
    partialCount_ = 0;
    ++serial_;
    timestamp_ = Clock::now();
}
bool ZzSpectrumInput::finishLocked()
{
    const bool wake = !wakePending_;
    wakePending_ = true;
    return wake;
}
bool ZzSpectrumInput::pushPcm(const QByteArray &bytes)
{
    const QMutexLocker lock(&mutex_);
    if (!enabled_ || ring_.isEmpty() || bytes.isEmpty()) return false;
    if (typeKnown_ && floats_) resetLocked();
    preparePushLocked();
    typeKnown_ = true;
    floats_ = false;
    qsizetype begin = 0;
    const qsizetype frameBytes = channels_ * 2;
    const qsizetype capacity = ring_.size() * 2;
    if (bytes.size() > capacity + frameBytes) {
        const qsizetype boundary = (frameBytes - (partialCount_ * 2 + (lowByte_ >= 0 ? 1 : 0))) % frameBytes;
        begin = boundary + ((bytes.size() - boundary - capacity) / frameBytes) * frameBytes;
        write_ = frames_ = partialCount_ = 0;
        lowByte_ = -1;
    }
    for (qsizetype i = begin; i < bytes.size(); ++i) {
        const int byte = static_cast<unsigned char>(bytes[i]);
        if (lowByte_ < 0) lowByte_ = byte;
        else {
            const int bits = lowByte_ | (byte << 8);
            appendLocked(static_cast<float>(bits >= 32768 ? bits - 65536 : bits) / 32768.0f);
            lowByte_ = -1;
        }
    }
    return finishLocked();
}
bool ZzSpectrumInput::pushFloats(const QVector<float> &samples)
{
    const QMutexLocker lock(&mutex_);
    if (!enabled_ || ring_.isEmpty() || samples.isEmpty()) return false;
    if (typeKnown_ && !floats_) resetLocked();
    preparePushLocked();
    typeKnown_ = true;
    floats_ = true;
    qsizetype begin = 0;
    if (samples.size() > ring_.size() + channels_) {
        const qsizetype boundary = (channels_ - partialCount_) % channels_;
        begin = boundary + ((samples.size() - boundary - ring_.size()) / channels_) * channels_;
        write_ = frames_ = partialCount_ = 0;
    }
    for (qsizetype i = begin; i < samples.size(); ++i) appendLocked(samples[i]);
    return finishLocked();
}
void ZzSpectrumInput::acknowledgeWake()
{
    const QMutexLocker lock(&mutex_);
    wakePending_ = false;
}
ZzSpectrumInput::ZzSnapshot ZzSpectrumInput::takeLatest()
{
    const QMutexLocker lock(&mutex_);
    ZzSnapshot result;
    result.serial = serial_;
    result.timestamp = timestamp_;
    if (frames_ < fftSize_) return result;
    const int count = fftSize_ * channels_;
    result.samples.resize(count);
    const int capacity = static_cast<int>(ring_.size());
    const int begin = (write_ - count + capacity) % capacity;
    for (int i = 0; i < count; ++i) result.samples[i] = ring_[(begin + i) % capacity];
    return result;
}
int ZzSpectrumInput::bufferedFrames() const
{
    const QMutexLocker lock(&mutex_);
    return frames_;
}
} // namespace ZzFluentUI
