#pragma once
#include <ZzFluentUI/ZzAudioLevelMeter.h>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>
#include <vector>

namespace ZzFluentUI {
/** @brief 电平表的计时、布局与绘制状态。 */
class ZzAudioLevelMeterPrivate final
{
public:
    explicit ZzAudioLevelMeterPrivate(ZzAudioLevelMeter *widget);
    using ZzMeterScalePosition = ZzAudioLevelMeter::ZzMeterScalePosition;
    using ZzMeterScaleMode = ZzAudioLevelMeter::ZzMeterScaleMode;
    using ZzMeterColorMode = ZzAudioLevelMeter::ZzMeterColorMode;
    int channelCount = 2;
    qreal minimumDecibels = -60.0;
    qreal maximumDecibels = 0.0;
    qreal warningDecibels = -12.0;
    qreal clipDecibels = -3.0;
    int segmentCount = 30;
    qreal segmentSpacing = 3.0;
    qreal segmentRadius = 2.0;
    qreal channelSpacing = 8.0;
    ZzMeterScalePosition scalePosition = ZzAudioLevelMeter::RightScale;
    ZzMeterScaleMode scaleMode = ZzAudioLevelMeter::IntervalScale;
    qreal scaleInterval = 10.0;
    int scaleTickCount = 7;
    QString scaleUnit = QStringLiteral("dB");
    bool scaleUnitVisible = false;
    int scalePrecision = 0;
    bool scaleTickMarksVisible = false;
    qreal scaleTickLength = 4.0;
    bool channelLabelsVisible = true;
    bool peakHoldEnabled = true;
    int peakHoldDuration = 1000;
    qreal decayRate = 36.0;
    qreal peakDecayRate = 18.0;
    int inputTimeout = 120;
    bool animationEnabled = true;
    ZzMeterColorMode colorMode = ZzAudioLevelMeter::SingleColor;
    QColor backgroundColor;
    QColor activeColor;
    QColor inactiveColor;
    QColor warningColor;
    QColor clipColor;
    QColor peakColor;
    QColor scaleColor;

    ZzAudioLevelMeter *const q;
    QTimer timer;
    QElapsedTimer frameElapsed;
    QElapsedTimer inputElapsed;
    QVector<qreal> levels, displayed, peaks;
    QVector<qreal> holdRemaining;
    QStringList channelLabels;
    QVector<qreal> customScaleValues;
    quint64 revision = 0;

    struct ZzMeterSnapshot {
        int channels;
        QVector<qreal> levels, peaks;
        bool running;
    };
    [[nodiscard]] ZzMeterSnapshot before() const;
    void publish(const ZzMeterSnapshot &old, std::vector<std::function<void()>> notifications = {},
                 bool forcePeaks = false);
    void resizeStorage();
    [[nodiscard]] bool motionEnabled() const;
    void syncTimer();
    void advance();
    void advanceValues();
    void apply(const QVector<qreal> &values);
    [[nodiscard]] qreal bound(qreal value) const;
    [[nodiscard]] qreal ratio(qreal value) const;
    [[nodiscard]] QVector<qreal> ticks() const;
    [[nodiscard]] QString tickLabel(qreal value) const;
    [[nodiscard]] QString channelLabel(int channel) const;
    void paint();
};
} // namespace ZzFluentUI
