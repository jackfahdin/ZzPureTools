#pragma once

#include <memory>
#include <QColor>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {
class ZzAudioLevelMeterPrivate;

/**
 * @brief 支持 1～8 声道的只读分段音频电平表。
 * 输入为 dBFS 或线性峰值幅度。五个电平输入方法可以从普通工作线程调用，
 * 跨线程输入最多复制八个声道并排队到 GUI 线程；不适合硬实时音频回调。
 * 其他方法只供 GUI 线程使用。隐藏或禁用时暂停动画；减少动态效果和
 * animationEnabled=false 时直接显示最新输入，并暂停输入超时。
 */
class ZZ_FLUENT_UI_EXPORT ZzAudioLevelMeter final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzAudioLevelMeter)
public:
    /** @brief 刻度位置；CenterScale 在非双声道时等同 RightScale。 */
    enum ZzMeterScalePosition : int { NoScale, LeftScale, RightScale, CenterScale };
    Q_ENUM(ZzMeterScalePosition)
    /** @brief 刻度生成方式。 */
    enum ZzMeterScaleMode : int { IntervalScale, FixedTickCount, CustomScale };
    Q_ENUM(ZzMeterScaleMode)
    /** @brief 激活分段配色。 */
    enum ZzMeterColorMode : int { SingleColor, ThresholdColors, GradientColors };
    Q_ENUM(ZzMeterColorMode)

    Q_PROPERTY(int channelCount READ channelCount WRITE setChannelCount NOTIFY channelCountChanged)
    Q_PROPERTY(qreal minimumDecibels READ minimumDecibels WRITE setMinimumDecibels NOTIFY minimumDecibelsChanged)
    Q_PROPERTY(qreal maximumDecibels READ maximumDecibels WRITE setMaximumDecibels NOTIFY maximumDecibelsChanged)
    Q_PROPERTY(qreal warningDecibels READ warningDecibels WRITE setWarningDecibels NOTIFY warningDecibelsChanged)
    Q_PROPERTY(qreal clipDecibels READ clipDecibels WRITE setClipDecibels NOTIFY clipDecibelsChanged)
    Q_PROPERTY(int segmentCount READ segmentCount WRITE setSegmentCount NOTIFY segmentCountChanged)
    Q_PROPERTY(qreal segmentSpacing READ segmentSpacing WRITE setSegmentSpacing NOTIFY segmentSpacingChanged)
    Q_PROPERTY(qreal segmentRadius READ segmentRadius WRITE setSegmentRadius NOTIFY segmentRadiusChanged)
    Q_PROPERTY(qreal channelSpacing READ channelSpacing WRITE setChannelSpacing NOTIFY channelSpacingChanged)
    Q_PROPERTY(ZzMeterScalePosition scalePosition READ scalePosition WRITE setScalePosition NOTIFY scalePositionChanged)
    Q_PROPERTY(ZzMeterScaleMode scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged)
    Q_PROPERTY(qreal scaleInterval READ scaleInterval WRITE setScaleInterval NOTIFY scaleIntervalChanged)
    Q_PROPERTY(int scaleTickCount READ scaleTickCount WRITE setScaleTickCount NOTIFY scaleTickCountChanged)
    Q_PROPERTY(QString scaleUnit READ scaleUnit WRITE setScaleUnit NOTIFY scaleUnitChanged)
    Q_PROPERTY(bool scaleUnitVisible READ isScaleUnitVisible WRITE setScaleUnitVisible NOTIFY scaleUnitVisibleChanged)
    Q_PROPERTY(int scalePrecision READ scalePrecision WRITE setScalePrecision NOTIFY scalePrecisionChanged)
    Q_PROPERTY(bool scaleTickMarksVisible READ areScaleTickMarksVisible WRITE setScaleTickMarksVisible NOTIFY scaleTickMarksVisibleChanged)
    Q_PROPERTY(qreal scaleTickLength READ scaleTickLength WRITE setScaleTickLength NOTIFY scaleTickLengthChanged)
    Q_PROPERTY(bool channelLabelsVisible READ areChannelLabelsVisible WRITE setChannelLabelsVisible NOTIFY channelLabelsVisibleChanged)
    Q_PROPERTY(bool peakHoldEnabled READ isPeakHoldEnabled WRITE setPeakHoldEnabled NOTIFY peakHoldEnabledChanged)
    Q_PROPERTY(int peakHoldDuration READ peakHoldDuration WRITE setPeakHoldDuration NOTIFY peakHoldDurationChanged)
    Q_PROPERTY(qreal decayRate READ decayRate WRITE setDecayRate NOTIFY decayRateChanged)
    Q_PROPERTY(qreal peakDecayRate READ peakDecayRate WRITE setPeakDecayRate NOTIFY peakDecayRateChanged)
    Q_PROPERTY(int inputTimeout READ inputTimeout WRITE setInputTimeout NOTIFY inputTimeoutChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(ZzMeterColorMode colorMode READ colorMode WRITE setColorMode NOTIFY colorModeChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QColor activeColor READ activeColor WRITE setActiveColor NOTIFY activeColorChanged)
    Q_PROPERTY(QColor inactiveColor READ inactiveColor WRITE setInactiveColor NOTIFY inactiveColorChanged)
    Q_PROPERTY(QColor warningColor READ warningColor WRITE setWarningColor NOTIFY warningColorChanged)
    Q_PROPERTY(QColor clipColor READ clipColor WRITE setClipColor NOTIFY clipColorChanged)
    Q_PROPERTY(QColor peakColor READ peakColor WRITE setPeakColor NOTIFY peakColorChanged)
    Q_PROPERTY(QColor scaleColor READ scaleColor WRITE setScaleColor NOTIFY scaleColorChanged)
    Q_PROPERTY(QVector<qreal> customScaleValues READ customScaleValues WRITE setCustomScaleValues NOTIFY customScaleValuesChanged)
    Q_PROPERTY(QStringList channelLabels READ channelLabels WRITE setChannelLabels NOTIFY channelLabelsChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

    /** @brief 创建默认双声道、-60～0 dB 量程的电平表。 */
    explicit ZzAudioLevelMeter(QWidget *parent = nullptr);
    /** @brief 停止计时器并释放呈现状态。 */
    ~ZzAudioLevelMeter() override;

    /** @brief 声道数，限制为 1～8。 默认值：2。 */
    [[nodiscard]] int channelCount() const;
    /** @brief 设置声道数，限制为 1～8。 */
    void setChannelCount(int value);
    /** @brief 显示量程下限，最小 -160 dB。 默认值：-60.0。 */
    [[nodiscard]] qreal minimumDecibels() const;
    /** @brief 设置显示量程下限，最小 -160 dB。 */
    void setMinimumDecibels(qreal value);
    /** @brief 显示量程上限，最大 24 dB。 默认值：0.0。 */
    [[nodiscard]] qreal maximumDecibels() const;
    /** @brief 设置显示量程上限，最大 24 dB。 */
    void setMaximumDecibels(qreal value);
    /** @brief 警告颜色阈值，限制于下限与过载阈值之间。 默认值：-12.0。 */
    [[nodiscard]] qreal warningDecibels() const;
    /** @brief 设置警告颜色阈值，限制于下限与过载阈值之间。 */
    void setWarningDecibels(qreal value);
    /** @brief 过载颜色阈值，限制于警告阈值与上限之间。 默认值：-3.0。 */
    [[nodiscard]] qreal clipDecibels() const;
    /** @brief 设置过载颜色阈值，限制于警告阈值与上限之间。 */
    void setClipDecibels(qreal value);
    /** @brief 期望分段数量，限制为 2～120。 默认值：30。 */
    [[nodiscard]] int segmentCount() const;
    /** @brief 设置期望分段数量，限制为 2～120。 */
    void setSegmentCount(int value);
    /** @brief 分段间距，限制为 0～20 逻辑像素。 默认值：3.0。 */
    [[nodiscard]] qreal segmentSpacing() const;
    /** @brief 设置分段间距，限制为 0～20 逻辑像素。 */
    void setSegmentSpacing(qreal value);
    /** @brief 分段圆角，限制为 0～20 逻辑像素。 默认值：2.0。 */
    [[nodiscard]] qreal segmentRadius() const;
    /** @brief 设置分段圆角，限制为 0～20 逻辑像素。 */
    void setSegmentRadius(qreal value);
    /** @brief 声道间距，限制为 0～40 逻辑像素。 默认值：8.0。 */
    [[nodiscard]] qreal channelSpacing() const;
    /** @brief 设置声道间距，限制为 0～40 逻辑像素。 */
    void setChannelSpacing(qreal value);
    /** @brief 刻度位置，中心刻度仅用于双声道。 默认值：RightScale。 */
    [[nodiscard]] ZzMeterScalePosition scalePosition() const;
    /** @brief 设置刻度位置，中心刻度仅用于双声道。 */
    void setScalePosition(ZzMeterScalePosition value);
    /** @brief 刻度生成方式。 默认值：IntervalScale。 */
    [[nodiscard]] ZzMeterScaleMode scaleMode() const;
    /** @brief 设置刻度生成方式。 */
    void setScaleMode(ZzMeterScaleMode value);
    /** @brief 刻度间隔，限制为 1～60 dB。 默认值：10.0。 */
    [[nodiscard]] qreal scaleInterval() const;
    /** @brief 设置刻度间隔，限制为 1～60 dB。 */
    void setScaleInterval(qreal value);
    /** @brief 固定刻度数，限制为 2～64。 默认值：7。 */
    [[nodiscard]] int scaleTickCount() const;
    /** @brief 设置固定刻度数，限制为 2～64。 */
    void setScaleTickCount(int value);
    /** @brief 刻度单位文本。 默认值：QStringLiteral(dB)。 */
    [[nodiscard]] QString scaleUnit() const;
    /** @brief 设置刻度单位文本。 */
    void setScaleUnit(QString value);
    /** @brief 是否在每个刻度后显示单位。 默认值：false。 */
    [[nodiscard]] bool isScaleUnitVisible() const;
    /** @brief 设置是否在每个刻度后显示单位。 */
    void setScaleUnitVisible(bool value);
    /** @brief 刻度小数位数，限制为 0～3。 默认值：0。 */
    [[nodiscard]] int scalePrecision() const;
    /** @brief 设置刻度小数位数，限制为 0～3。 */
    void setScalePrecision(int value);
    /** @brief 是否显示短刻度线。 默认值：false。 */
    [[nodiscard]] bool areScaleTickMarksVisible() const;
    /** @brief 设置是否显示短刻度线。 */
    void setScaleTickMarksVisible(bool value);
    /** @brief 短刻度线长，限制为 1～20 逻辑像素。 默认值：4.0。 */
    [[nodiscard]] qreal scaleTickLength() const;
    /** @brief 设置短刻度线长，限制为 1～20 逻辑像素。 */
    void setScaleTickLength(qreal value);
    /** @brief 是否显示底部声道标签。 默认值：true。 */
    [[nodiscard]] bool areChannelLabelsVisible() const;
    /** @brief 设置是否显示底部声道标签。 */
    void setChannelLabelsVisible(bool value);
    /** @brief 是否显示并保持峰值标记。 默认值：true。 */
    [[nodiscard]] bool isPeakHoldEnabled() const;
    /** @brief 设置是否显示并保持峰值标记。 */
    void setPeakHoldEnabled(bool value);
    /** @brief 峰值保持时间，限制为 0～10000 毫秒。 默认值：1000。 */
    [[nodiscard]] int peakHoldDuration() const;
    /** @brief 设置峰值保持时间，限制为 0～10000 毫秒。 */
    void setPeakHoldDuration(int value);
    /** @brief 主电平衰减速度，限制为 0～1000 dB/s；0 表示保持。 默认值：36.0。 */
    [[nodiscard]] qreal decayRate() const;
    /** @brief 设置主电平衰减速度，限制为 0～1000 dB/s；0 表示保持。 */
    void setDecayRate(qreal value);
    /** @brief 峰值衰减速度，限制为 0～1000 dB/s；0 表示保持。 默认值：18.0。 */
    [[nodiscard]] qreal peakDecayRate() const;
    /** @brief 设置峰值衰减速度，限制为 0～1000 dB/s；0 表示保持。 */
    void setPeakDecayRate(qreal value);
    /** @brief 输入超时，限制为 0～10000 毫秒；0 表示关闭。 默认值：120。 */
    [[nodiscard]] int inputTimeout() const;
    /** @brief 设置输入超时，限制为 0～10000 毫秒；0 表示关闭。 */
    void setInputTimeout(int value);
    /** @brief 是否启用衰减动画；关闭后电平与峰值直接跟随输入。 默认值：true。 */
    [[nodiscard]] bool isAnimationEnabled() const;
    /** @brief 设置是否启用衰减动画；关闭后电平与峰值直接跟随输入。 */
    void setAnimationEnabled(bool value);
    /** @brief 激活分段的配色方式。 默认值：SingleColor。 */
    [[nodiscard]] ZzMeterColorMode colorMode() const;
    /** @brief 设置激活分段的配色方式。 */
    void setColorMode(ZzMeterColorMode value);
    /** @brief 背景颜色；无效颜色使用主题默认值。 默认值：QColor()。 */
    [[nodiscard]] QColor backgroundColor() const;
    /** @brief 设置背景颜色；无效颜色使用主题默认值。 */
    void setBackgroundColor(QColor value);
    /** @brief 激活颜色；无效颜色使用主题默认值。 默认值：QColor()。 */
    [[nodiscard]] QColor activeColor() const;
    /** @brief 设置激活颜色；无效颜色使用主题默认值。 */
    void setActiveColor(QColor value);
    /** @brief 未激活颜色；无效颜色使用主题默认值。 默认值：QColor()。 */
    [[nodiscard]] QColor inactiveColor() const;
    /** @brief 设置未激活颜色；无效颜色使用主题默认值。 */
    void setInactiveColor(QColor value);
    /** @brief 警告颜色；无效颜色使用主题默认值。 默认值：QColor()。 */
    [[nodiscard]] QColor warningColor() const;
    /** @brief 设置警告颜色；无效颜色使用主题默认值。 */
    void setWarningColor(QColor value);
    /** @brief 过载颜色；无效颜色使用主题默认值。 默认值：QColor()。 */
    [[nodiscard]] QColor clipColor() const;
    /** @brief 设置过载颜色；无效颜色使用主题默认值。 */
    void setClipColor(QColor value);
    /** @brief 峰值颜色；无效颜色使用主题默认值。 默认值：QColor()。 */
    [[nodiscard]] QColor peakColor() const;
    /** @brief 设置峰值颜色；无效颜色使用主题默认值。 */
    void setPeakColor(QColor value);
    /** @brief 文字颜色；无效颜色使用主题默认值。 默认值：QColor()。 */
    [[nodiscard]] QColor scaleColor() const;
    /** @brief 设置文字颜色；无效颜色使用主题默认值。 */
    void setScaleColor(QColor value);

    /** @brief 返回用户设置的声道标签。 */
    [[nodiscard]] QStringList channelLabels() const;
    /** @brief 设置声道标签；缺失项使用 L/R 或从 1 开始的编号。 */
    void setChannelLabels(const QStringList &labels);
    /** @brief 返回已排序去重的自定义刻度。 */
    [[nodiscard]] QVector<qreal> customScaleValues() const;
    /** @brief 过滤非有限值后降序去重；量程外的值保留以便以后恢复。 */
    void setCustomScaleValues(const QVector<qreal> &values);
    /** @brief 返回最近输入；超时后归量程下限。 */
    [[nodiscard]] QVector<qreal> levels() const;
    /** @brief 返回衰减后的当前显示电平。 */
    [[nodiscard]] QVector<qreal> displayedLevels() const;
    /** @brief 返回当前峰值电平。 */
    [[nodiscard]] QVector<qreal> peakLevels() const;
    /** @brief 返回指定声道输入；无效索引返回量程下限。 */
    [[nodiscard]] qreal level(int channel) const;
    /** @brief 返回指定声道显示电平；无效索引返回量程下限。 */
    [[nodiscard]] qreal displayedLevel(int channel) const;
    /** @brief 返回指定声道峰值；无效索引返回量程下限。 */
    [[nodiscard]] qreal peakLevel(int channel) const;
    /** @brief 返回动画计时器运行状态，不代表音频播放状态。 */
    [[nodiscard]] bool isRunning() const;
    /** @brief 返回适合垂直电平表的建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;
    /** @brief 返回随声道数量变化的最小建议尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** @brief 设置 -160～24 dB 量程，至少相差 1 dB；无效逆序输入不生效。 */
    void setRange(qreal minimumDecibels, qreal maximumDecibels);
    /** @brief 提交单声道 dBFS；自动切换为一个声道。 */
    void setLevel(qreal decibels);
    /** @brief 提交左右声道 dBFS；自动切换为两个声道。 */
    void setStereoLevels(qreal leftDecibels, qreal rightDecibels);
    /** @brief 提交至多八声道；NaN/inf 归下限，空数组等同 clear()。 */
    void setLevels(const QVector<qreal> &decibels);
    /** @brief 按 20*log10(abs(amplitude)) 提交单声道峰值幅度。 */
    void setLinearLevel(qreal amplitude);
    /** @brief 提交至多八声道线性幅度；零和非有限值归量程下限。 */
    void setLinearLevels(const QVector<qreal> &amplitudes);
    /** @brief 将峰值重置到显示值，并重新开始保持时间。 */
    void resetPeaks();
    /** @brief 清空输入、显示和峰值，并停止动画。 */
    void clear();

Q_SIGNALS:
    /** @brief channelCount 的有效值发生变化。 */
    void channelCountChanged(int value);
    /** @brief minimumDecibels 的有效值发生变化。 */
    void minimumDecibelsChanged(qreal value);
    /** @brief maximumDecibels 的有效值发生变化。 */
    void maximumDecibelsChanged(qreal value);
    /** @brief warningDecibels 的有效值发生变化。 */
    void warningDecibelsChanged(qreal value);
    /** @brief clipDecibels 的有效值发生变化。 */
    void clipDecibelsChanged(qreal value);
    /** @brief segmentCount 的有效值发生变化。 */
    void segmentCountChanged(int value);
    /** @brief segmentSpacing 的有效值发生变化。 */
    void segmentSpacingChanged(qreal value);
    /** @brief segmentRadius 的有效值发生变化。 */
    void segmentRadiusChanged(qreal value);
    /** @brief channelSpacing 的有效值发生变化。 */
    void channelSpacingChanged(qreal value);
    /** @brief scalePosition 的有效值发生变化。 */
    void scalePositionChanged(ZzMeterScalePosition value);
    /** @brief scaleMode 的有效值发生变化。 */
    void scaleModeChanged(ZzMeterScaleMode value);
    /** @brief scaleInterval 的有效值发生变化。 */
    void scaleIntervalChanged(qreal value);
    /** @brief scaleTickCount 的有效值发生变化。 */
    void scaleTickCountChanged(int value);
    /** @brief scaleUnit 的有效值发生变化。 */
    void scaleUnitChanged(QString value);
    /** @brief scaleUnitVisible 的有效值发生变化。 */
    void scaleUnitVisibleChanged(bool value);
    /** @brief scalePrecision 的有效值发生变化。 */
    void scalePrecisionChanged(int value);
    /** @brief scaleTickMarksVisible 的有效值发生变化。 */
    void scaleTickMarksVisibleChanged(bool value);
    /** @brief scaleTickLength 的有效值发生变化。 */
    void scaleTickLengthChanged(qreal value);
    /** @brief channelLabelsVisible 的有效值发生变化。 */
    void channelLabelsVisibleChanged(bool value);
    /** @brief peakHoldEnabled 的有效值发生变化。 */
    void peakHoldEnabledChanged(bool value);
    /** @brief peakHoldDuration 的有效值发生变化。 */
    void peakHoldDurationChanged(int value);
    /** @brief decayRate 的有效值发生变化。 */
    void decayRateChanged(qreal value);
    /** @brief peakDecayRate 的有效值发生变化。 */
    void peakDecayRateChanged(qreal value);
    /** @brief inputTimeout 的有效值发生变化。 */
    void inputTimeoutChanged(int value);
    /** @brief animationEnabled 的有效值发生变化。 */
    void animationEnabledChanged(bool value);
    /** @brief colorMode 的有效值发生变化。 */
    void colorModeChanged(ZzMeterColorMode value);
    /** @brief backgroundColor 的有效值发生变化。 */
    void backgroundColorChanged(QColor value);
    /** @brief activeColor 的有效值发生变化。 */
    void activeColorChanged(QColor value);
    /** @brief inactiveColor 的有效值发生变化。 */
    void inactiveColorChanged(QColor value);
    /** @brief warningColor 的有效值发生变化。 */
    void warningColorChanged(QColor value);
    /** @brief clipColor 的有效值发生变化。 */
    void clipColorChanged(QColor value);
    /** @brief peakColor 的有效值发生变化。 */
    void peakColorChanged(QColor value);
    /** @brief scaleColor 的有效值发生变化。 */
    void scaleColorChanged(QColor value);
    /** @brief 用户声道标签发生变化。 */
    void channelLabelsChanged(const QStringList &labels);
    /** @brief 自定义刻度发生变化。 */
    void customScaleValuesChanged(const QVector<qreal> &values);
    /** @brief 输入电平变化，包括超时、清空和量程钳制。 */
    void levelsChanged(const QVector<qreal> &levels);
    /** @brief 峰值变化。 */
    void peakLevelsChanged(const QVector<qreal> &levels);
    /** @brief 动画计时器开始或停止。 */
    void runningChanged(bool running);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class ZzAudioLevelMeterPrivate;
    std::unique_ptr<ZzAudioLevelMeterPrivate> d_ptr;
};
} // namespace ZzFluentUI
