#pragma once

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <QColor>
#include <QProgressBar>
#include <memory>

namespace ZzFluentUI {

class ZzLiquidGaugePrivate;

/**
 * @brief 双层水波进度，保留 QProgressBar 的量程、格式和 reset 语义。
 *
 * 所有尺寸为逻辑像素。隐藏、禁用、关闭动画、零振幅或 reduced-motion
 * 时暂停并保留波形相位；退化量程绘制空液面。必须在 GUI 线程使用。
 */
class ZZ_FLUENT_UI_EXPORT ZzLiquidGauge final : public QProgressBar
{
    Q_OBJECT
public:
    enum ZzLiquidShape { CircleShape, RectShape, PinShape, TriangleShape };
    Q_ENUM(ZzLiquidShape)

    /** @brief 外轮廓，默认 CircleShape；非法枚举值被忽略。 */
    Q_PROPERTY(ZzLiquidShape shape READ shape WRITE setShape NOTIFY shapeChanged)
    [[nodiscard]] ZzLiquidShape shape() const;
    void setShape(ZzLiquidShape value);
    Q_SIGNAL void shapeChanged(ZzLiquidShape value);

    /** @brief 波峰高度 [0,100]，默认 6；非有限输入被忽略。 */
    Q_PROPERTY(qreal waveAmplitude READ waveAmplitude WRITE setWaveAmplitude NOTIFY waveAmplitudeChanged)
    [[nodiscard]] qreal waveAmplitude() const;
    void setWaveAmplitude(qreal value);
    Q_SIGNAL void waveAmplitudeChanged(qreal value);

    /** @brief 横向完整波形数 [1,20]，默认 3。 */
    Q_PROPERTY(int waveCount READ waveCount WRITE setWaveCount NOTIFY waveCountChanged)
    [[nodiscard]] int waveCount() const;
    void setWaveCount(int value);
    Q_SIGNAL void waveCountChanged(int value);

    /** @brief 一个动画周期的毫秒数 [100,60000]，默认 2400。 */
    Q_PROPERTY(int waveAnimationDuration READ waveAnimationDuration WRITE setWaveAnimationDuration
                   NOTIFY waveAnimationDurationChanged)
    [[nodiscard]] int waveAnimationDuration() const;
    void setWaveAnimationDuration(int value);
    Q_SIGNAL void waveAnimationDurationChanged(int value);

    /** @brief 是否允许水波移动，默认 true。 */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    [[nodiscard]] bool isAnimationEnabled() const;
    void setAnimationEnabled(bool value);
    Q_SIGNAL void animationEnabledChanged(bool value);

    /** @brief 当前是否实际播放动画；受可见性、启用状态及主题限制。 */
    Q_PROPERTY(bool running READ isRunning)
    [[nodiscard]] bool isRunning() const;

    /** @brief 后层波相对透明度 [0,1]，默认 0.45；非有限输入被忽略。 */
    Q_PROPERTY(qreal secondaryWaveOpacity READ secondaryWaveOpacity WRITE setSecondaryWaveOpacity
                   NOTIFY secondaryWaveOpacityChanged)
    [[nodiscard]] qreal secondaryWaveOpacity() const;
    void setSecondaryWaveOpacity(qreal value);
    Q_SIGNAL void secondaryWaveOpacityChanged(qreal value);

    /** @brief 轮廓线宽 [0,100]，默认 2；0 不绘制，非有限输入被忽略。 */
    Q_PROPERTY(qreal outlineWidth READ outlineWidth WRITE setOutlineWidth NOTIFY outlineWidthChanged)
    [[nodiscard]] qreal outlineWidth() const;
    void setOutlineWidth(qreal value);
    Q_SIGNAL void outlineWidthChanged(qreal value);

    /** @brief 轮廓至液体的间距 [0,100]，默认 3；非有限输入被忽略。 */
    Q_PROPERTY(qreal outlineDistance READ outlineDistance WRITE setOutlineDistance NOTIFY outlineDistanceChanged)
    [[nodiscard]] qreal outlineDistance() const;
    void setOutlineDistance(qreal value);
    Q_SIGNAL void outlineDistanceChanged(qreal value);

    /** @brief 主波颜色，无效颜色（默认）跟随主题强调色。 */
    Q_PROPERTY(QColor waveColor READ waveColor WRITE setWaveColor NOTIFY waveColorChanged)
    [[nodiscard]] QColor waveColor() const;
    void setWaveColor(QColor value);
    Q_SIGNAL void waveColorChanged(QColor value);

    /** @brief 未填充区域颜色，无效颜色（默认）使用 QPalette::Base。 */
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    [[nodiscard]] QColor backgroundColor() const;
    void setBackgroundColor(QColor value);
    Q_SIGNAL void backgroundColorChanged(QColor value);

    /** @brief 轮廓颜色，无效颜色（默认）跟随主波颜色。 */
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor NOTIFY outlineColorChanged)
    [[nodiscard]] QColor outlineColor() const;
    void setOutlineColor(QColor value);
    Q_SIGNAL void outlineColorChanged(QColor value);

    /** @brief 液面上方文字颜色，无效颜色（默认）使用 QPalette::Text。 */
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    [[nodiscard]] QColor textColor() const;
    void setTextColor(QColor value);
    Q_SIGNAL void textColorChanged(QColor value);

    /** @brief 被波浪覆盖的文字颜色，无效颜色（默认）使用 QPalette::HighlightedText。 */
    Q_PROPERTY(QColor submergedTextColor READ submergedTextColor WRITE setSubmergedTextColor
                   NOTIFY submergedTextColorChanged)
    [[nodiscard]] QColor submergedTextColor() const;
    void setSubmergedTextColor(QColor value);
    Q_SIGNAL void submergedTextColorChanged(QColor value);

    /** @brief 中心文字像素大小 [0,200]，默认 0 表示随控件尺寸计算。 */
    Q_PROPERTY(int contentFontPixelSize READ contentFontPixelSize WRITE setContentFontPixelSize
                   NOTIFY contentFontPixelSizeChanged)
    [[nodiscard]] int contentFontPixelSize() const;
    void setContentFontPixelSize(int value);
    Q_SIGNAL void contentFontPixelSizeChanged(int value);

    explicit ZzLiquidGauge(QWidget *parent = nullptr);
    ~ZzLiquidGauge() override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    std::unique_ptr<ZzLiquidGaugePrivate> d_ptr;
};

} // namespace ZzFluentUI
