#pragma once

#include <memory>
#include <QWidget>
#include <QColor>
#include <QVector>
#include <QByteArray>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {
class ZzAudioSpectrumPrivate;
/**
 * @brief Hann FFT 音频频谱；不打开音频设备，不解码文件。
 * 除 pushPcm16Le / pushSamples 外所有 API 仅限 GUI 线程。推送支持普通
 * 工作线程（非硬实时），调用方须在析构前停止并等待生产线程。
 * 配置属性统一发送 configurationChanged，外观属性统一发送 appearanceChanged。
 * levels 是归一化目标；displayedLevels 和 peakLevels 为呈现状态。
 * 所有 qreal 配置忽略非有限值；有效值钳制至对应范围。
 * 分析参数变更清空旧输入与显示；centerFrequencies 返回 Nyquist
 * 范围内的实际频带中心。隐藏、禁用时暂停分析；不重播过期输入。
 */
class ZZ_FLUENT_UI_EXPORT ZzAudioSpectrum final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzAudioSpectrum)
    Q_PROPERTY(ZzInputMode inputMode READ inputMode WRITE setInputMode NOTIFY configurationChanged)
    Q_PROPERTY(ZzChannelMix channelMix READ channelMix WRITE setChannelMix NOTIFY configurationChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY configurationChanged)
    Q_PROPERTY(int channelCount READ channelCount WRITE setChannelCount NOTIFY configurationChanged)
    Q_PROPERTY(int fftSize READ fftSize WRITE setFftSize NOTIFY configurationChanged)
    Q_PROPERTY(int barCount READ barCount WRITE setBarCount NOTIFY configurationChanged)
    Q_PROPERTY(qreal minimumFrequency READ minimumFrequency WRITE setMinimumFrequency NOTIFY configurationChanged)
    Q_PROPERTY(qreal maximumFrequency READ maximumFrequency WRITE setMaximumFrequency NOTIFY configurationChanged)
    Q_PROPERTY(qreal minimumDecibels READ minimumDecibels WRITE setMinimumDecibels NOTIFY configurationChanged)
    Q_PROPERTY(qreal maximumDecibels READ maximumDecibels WRITE setMaximumDecibels NOTIFY configurationChanged)
    Q_PROPERTY(qreal spectralTilt READ spectralTilt WRITE setSpectralTilt NOTIFY configurationChanged)
    Q_PROPERTY(int refreshInterval READ refreshInterval WRITE setRefreshInterval NOTIFY configurationChanged)
    Q_PROPERTY(bool animationEnabled READ animationEnabled WRITE setAnimationEnabled NOTIFY configurationChanged)
    Q_PROPERTY(int inputTimeout READ inputTimeout WRITE setInputTimeout NOTIFY configurationChanged)
    Q_PROPERTY(qreal gravity READ gravity WRITE setGravity NOTIFY configurationChanged)
    Q_PROPERTY(int peakHoldDuration READ peakHoldDuration WRITE setPeakHoldDuration NOTIFY configurationChanged)
    Q_PROPERTY(qreal barGap READ barGap WRITE setBarGap NOTIFY appearanceChanged)
    Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY appearanceChanged)
    Q_PROPERTY(bool reflectionEnabled READ reflectionEnabled WRITE setReflectionEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(qreal reflectionOpacity READ reflectionOpacity WRITE setReflectionOpacity NOTIFY appearanceChanged)
    Q_PROPERTY(bool peakEnabled READ peakEnabled WRITE setPeakEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(qreal peakHeight READ peakHeight WRITE setPeakHeight NOTIFY appearanceChanged)
    Q_PROPERTY(bool rainbowEnabled READ rainbowEnabled WRITE setRainbowEnabled NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY appearanceChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QVector<qreal> levels READ levels NOTIFY levelsChanged)
    Q_PROPERTY(QVector<qreal> displayedLevels READ displayedLevels NOTIFY displayedLevelsChanged)
    Q_PROPERTY(QVector<qreal> peakLevels READ peakLevels NOTIFY peakLevelsChanged)
    Q_PROPERTY(QVector<qreal> centerFrequencies READ centerFrequencies NOTIFY configurationChanged)
public:
    enum ZzInputMode : int { Pcm, ExternalBands };
    Q_ENUM(ZzInputMode)
    enum ZzChannelMix : int { PowerAverage, WaveformAverage };
    Q_ENUM(ZzChannelMix)
    using InputMode = ZzInputMode;
    using ChannelMix = ZzChannelMix;
    explicit ZzAudioSpectrum(QWidget *parent = nullptr);
    ~ZzAudioSpectrum() override;
    /** @brief 输入模式；默认 Pcm，切换清空输入与显示。 */
    [[nodiscard]] InputMode inputMode() const;
    void setInputMode(InputMode value);
    /** @brief 声道合成；默认 PowerAverage 保留反相信号能量。 */
    [[nodiscard]] ChannelMix channelMix() const;
    void setChannelMix(ChannelMix value);
    /** @brief 采样率，默认 44100 Hz；[8000,192000]。 */
    [[nodiscard]] int sampleRate() const;
    void setSampleRate(int value);
    /** @brief 交错声道数，默认 1；[1,8]。 */
    [[nodiscard]] int channelCount() const;
    void setChannelCount(int value);
    /** @brief FFT 窗长，默认 2048；仅接受 1024、2048、4096、8192。 */
    [[nodiscard]] int fftSize() const;
    void setFftSize(int value);
    /** @brief 逻辑频带数，默认 64；[8,256]，窄屏仅合并绘制。 */
    [[nodiscard]] int barCount() const;
    void setBarCount(int value);
    /** @brief 最低频率，默认 60 Hz；[1,maximumFrequency-1]。 */
    [[nodiscard]] qreal minimumFrequency() const;
    void setMinimumFrequency(qreal value);
    /** @brief 最高频率，默认 16000 Hz；[minimumFrequency+1,sampleRate/2]。 */
    [[nodiscard]] qreal maximumFrequency() const;
    void setMaximumFrequency(qreal value);
    /** @brief 归一化下界，默认 -54 dB；[-160,maximumDecibels-0.1]。 */
    [[nodiscard]] qreal minimumDecibels() const;
    void setMinimumDecibels(qreal value);
    /** @brief 归一化上界，默认 -3 dB；[minimumDecibels+0.1,24]。 */
    [[nodiscard]] qreal maximumDecibels() const;
    void setMaximumDecibels(qreal value);
    /** @brief 视觉倾斜，默认 3 dB/倍频程；[-6,9]。 */
    [[nodiscard]] qreal spectralTilt() const;
    void setSpectralTilt(qreal value);
    /** @brief 刷新间隔，默认 16 ms；[1,1000]。 */
    [[nodiscard]] int refreshInterval() const;
    void setRefreshInterval(int value);
    /** @brief 是否插值和衰减，默认启用；减少动效时直接呈现。 */
    [[nodiscard]] bool animationEnabled() const;
    void setAnimationEnabled(bool value);
    /** @brief 输入超时，默认 120 ms；[0,60000]，0 禁用超时。 */
    [[nodiscard]] int inputTimeout() const;
    void setInputTimeout(int value);
    /** @brief 柱高下落加速度，默认 4.2/s²；[0.01,100]。 */
    [[nodiscard]] qreal gravity() const;
    void setGravity(qreal value);
    /** @brief 峰值实际保持时间，默认 200 ms；[0,60000]。 */
    [[nodiscard]] int peakHoldDuration() const;
    void setPeakHoldDuration(int value);
    /** @brief 柱间距，默认 2 逻辑像素；[0,64]。 */
    [[nodiscard]] qreal barGap() const;
    void setBarGap(qreal value);
    /** @brief 圆角半径，默认 3 逻辑像素；[0,64]。 */
    [[nodiscard]] qreal cornerRadius() const;
    void setCornerRadius(qreal value);
    /** @brief 垂直镜像倒影，默认启用。 */
    [[nodiscard]] bool reflectionEnabled() const;
    void setReflectionEnabled(bool value);
    /** @brief 倒影起始不透明度，默认 0.58；[0,1]。 */
    [[nodiscard]] qreal reflectionOpacity() const;
    void setReflectionOpacity(qreal value);
    /** @brief 峰值标记，默认启用。 */
    [[nodiscard]] bool peakEnabled() const;
    void setPeakEnabled(bool value);
    /** @brief 峰值高度，默认 2 逻辑像素；[0,64]。 */
    [[nodiscard]] qreal peakHeight() const;
    void setPeakHeight(qreal value);
    /** @brief 横向彩虹，默认启用；高对比度始终采用主题。 */
    [[nodiscard]] bool rainbowEnabled() const;
    void setRainbowEnabled(bool value);
    /** @brief 单色柱颜色，无效值恢复主题；高对比度忽略自定义。 */
    [[nodiscard]] QColor accentColor() const;
    void setAccentColor(QColor value);
    /** @brief 背景颜色，无效值恢复主题；高对比度忽略自定义。 */
    [[nodiscard]] QColor backgroundColor() const;
    void setBackgroundColor(QColor value);
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QVector<qreal> levels() const;
    [[nodiscard]] QVector<qreal> displayedLevels() const;
    [[nodiscard]] QVector<qreal> peakLevels() const;
    [[nodiscard]] QVector<qreal> centerFrequencies() const;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    /** @brief 有符号 16 位小端交错 PCM；支持跨调用半采样、半帧拼接。 */
    void pushPcm16Le(const QByteArray &bytes);
    /** @brief 交错 [-1,1] 浮点 PCM；非有限值按静音处理。 */
    void pushSamples(const QVector<float> &samples);
    /**
     * @brief 仅 ExternalBands 模式接收，长度等于 barCount；拒绝非有限值，钳制至 [0,1]。
     * 静态呈现的首帧立即显示；定时器运行期间后续输入只更新目标，
     * 每个 refreshInterval 呈现最新帧，减少动效遵循相同限频规则。
     */
    void setBandLevels(const QVector<qreal> &levels);
    /** @brief 清空缓冲、目标、显示与峰值，取消待显示的旧输入。 */
    void clear();
Q_SIGNALS:
    void configurationChanged();
    void appearanceChanged();
    void runningChanged(bool running);
    void levelsChanged(const QVector<qreal> &levels);
    void displayedLevelsChanged(const QVector<qreal> &levels);
    void peakLevelsChanged(const QVector<qreal> &levels);
protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    friend class ZzAudioSpectrumPrivate;
    std::unique_ptr<ZzAudioSpectrumPrivate> d_ptr;
};
} // namespace ZzFluentUI
