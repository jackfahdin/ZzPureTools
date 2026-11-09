#pragma once

#include <QByteArray>
#include <QMutex>
#include <QVector>
#include <array>
#include <chrono>

namespace ZzFluentUI {

/** @brief 有界交错 PCM 环形缓冲；从普通工作线程推送，非硬实时接口。 */
class ZzSpectrumInput final
{
public:
    using Clock = std::chrono::steady_clock;
    using NowFunction = Clock::time_point (*)();
    explicit ZzSpectrumInput(NowFunction now = Clock::now) : now_(now) {}
    struct ZzSnapshot {
        QVector<float> samples;
        quint64 serial = 0;
        Clock::time_point timestamp{};
    };
    void configure(int fftSize, int channels, bool enabled = true, int inputTimeout = 120);
    void setInputTimeout(int milliseconds);
    void clear();
    /** @brief 返回是否需要发送唯一的 GUI 唤醒；数据在持锁期间复制。 */
    bool pushPcm(const QByteArray &bytes);
    bool pushFloats(const QVector<float> &samples);
    /** @brief 仅在唯一排队回调送达时确认；读取采样不消费该通知。 */
    void acknowledgeWake();
    [[nodiscard]] ZzSnapshot takeLatest();
    [[nodiscard]] int bufferedFrames() const;
private:
    const NowFunction now_;
    void resetLocked();
    void preparePushLocked();
    void appendLocked(float sample);
    bool finishLocked();
    mutable QMutex mutex_;
    QVector<float> ring_;
    std::array<float, 8> partial_{};
    int fftSize_ = 2048, channels_ = 1;
    int inputTimeout_ = 120;
    int write_ = 0, frames_ = 0, partialCount_ = 0;
    int lowByte_ = -1;
    bool floats_ = false, typeKnown_ = false, enabled_ = true, wakePending_ = false;
    quint64 serial_ = 0;
    Clock::time_point timestamp_{};
    Clock::time_point lastPush_{};
};
} // namespace ZzFluentUI
