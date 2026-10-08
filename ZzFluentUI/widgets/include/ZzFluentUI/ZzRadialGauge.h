#pragma once

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <memory>

#include <QColor>
#include <QDial>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace ZzFluentUI {

class ZzRadialGaugeRangePrivate;

/** @brief 可观察的数据项，由所属仪表盘管理生命周期。 */
class ZZ_FLUENT_UI_EXPORT ZzRadialGaugeRange final : public QObject {
    Q_OBJECT

  public:
    /** @brief 区间的起始数值。 */
    Q_PROPERTY(int fromValue READ fromValue WRITE setFromValue NOTIFY fromValueChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int fromValue() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setFromValue(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void fromValueChanged(int value);

    /** @brief 区间的结束数值。 */
    Q_PROPERTY(int toValue READ toValue WRITE setToValue NOTIFY toValueChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int toValue() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setToValue(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void toValueChanged(int value);

    /** @brief 区间在刻度环上的颜色。 */
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor color() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void colorChanged(QColor value);

    explicit ZzRadialGaugeRange(QObject *parent = nullptr);
    /** @brief 销毁私有数据。 */
    ~ZzRadialGaugeRange() override;
    ZzRadialGaugeRange(int fromValue, int toValue, const QColor &color, QObject *parent = nullptr);

    Q_SIGNAL void rangeChanged();

  private:
    std::unique_ptr<ZzRadialGaugeRangePrivate> d_ptr;
};

class ZzRadialGaugePrivate;

/** @brief 支持主题调色板与有限值动画的径向仪表盘。数值属性拒绝 NaN/Inf，并收敛绘图尺寸与刻度预算。 */
class ZZ_FLUENT_UI_EXPORT ZzRadialGauge final : public QDial {
    Q_OBJECT

  public:
    enum ScaleMode { TrackScale, ProgressScale, RangeScale };
    Q_ENUM(ScaleMode)

    enum NeedleStyle { NoNeedle, LineNeedle, TriangleNeedle };
    Q_ENUM(NeedleStyle)

    enum ValuePosition { CenterValue, BottomValue };
    Q_ENUM(ValuePosition)

    /** @brief 是否允许通过鼠标、键盘和滚轮修改数值。 */
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isInteractive() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setInteractive(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void interactiveChanged(bool value);

    /** @brief 数值变化动画时长，单位为毫秒；0 表示关闭动画。 */
    Q_PROPERTY(int valueAnimationDuration READ valueAnimationDuration WRITE setValueAnimationDuration NOTIFY
                   valueAnimationDurationChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int valueAnimationDuration() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueAnimationDuration(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueAnimationDurationChanged(int value);

    /** @brief 刻度环的绘制模式：纯 Track、数值进度或彩色区间。 */
    Q_PROPERTY(ScaleMode scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] ScaleMode scaleMode() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setScaleMode(ScaleMode value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void scaleModeChanged(ScaleMode value);

    /** @brief 刻度环的起始角度，正上方为 0°，顺时针为正。 */
    Q_PROPERTY(qreal minimumAngle READ minimumAngle WRITE setMinimumAngle NOTIFY minimumAngleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal minimumAngle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMinimumAngle(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void minimumAngleChanged(qreal value);

    /** @brief 刻度环的结束角度，正上方为 0°，顺时针为正。 */
    Q_PROPERTY(qreal maximumAngle READ maximumAngle WRITE setMaximumAngle NOTIFY maximumAngleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal maximumAngle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMaximumAngle(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void maximumAngleChanged(qreal value);

    /** @brief 整段圆弧上的主刻度数量，包含起点和终点。 */
    Q_PROPERTY(int majorTickCount READ majorTickCount WRITE setMajorTickCount NOTIFY majorTickCountChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int majorTickCount() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMajorTickCount(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void majorTickCountChanged(int value);

    /** @brief 每两个相邻主刻度之间绘制的次刻度数量。 */
    Q_PROPERTY(int minorTickCount READ minorTickCount WRITE setMinorTickCount NOTIFY minorTickCountChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int minorTickCount() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMinorTickCount(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void minorTickCountChanged(int value);

    /** @brief 刻度环的线宽，单位为逻辑像素。 */
    Q_PROPERTY(qreal scaleWidth READ scaleWidth WRITE setScaleWidth NOTIFY scaleWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal scaleWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setScaleWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void scaleWidthChanged(qreal value);

    /** @brief 刻度环与控件外边缘之间的间距 */
    Q_PROPERTY(qreal scalePadding READ scalePadding WRITE setScalePadding NOTIFY scalePaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal scalePadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setScalePadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void scalePaddingChanged(qreal value);

    /** @brief Track 圆弧的端点样式；Progress 模式默认使用圆头，其他模式默认使用平头。 */
    Q_PROPERTY(
        Qt::PenCapStyle trackCapStyle READ trackCapStyle WRITE setTrackCapStyle NOTIFY trackCapStyleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] Qt::PenCapStyle trackCapStyle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTrackCapStyle(Qt::PenCapStyle value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void trackCapStyleChanged(Qt::PenCapStyle value);

    /** @brief 进度环和彩色区间圆弧的端点样式；Progress 模式默认使用圆头，其他模式默认使用平头。 */
    Q_PROPERTY(
        Qt::PenCapStyle ringCapStyle READ ringCapStyle WRITE setRingCapStyle NOTIFY ringCapStyleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] Qt::PenCapStyle ringCapStyle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setRingCapStyle(Qt::PenCapStyle value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void ringCapStyleChanged(Qt::PenCapStyle value);

    /** @brief Progress 模式下，已扫过的圆弧是否沿扫描方向使用渐变色。 */
    Q_PROPERTY(bool progressGradientEnabled READ isProgressGradientEnabled WRITE setProgressGradientEnabled
                   NOTIFY progressGradientEnabledChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isProgressGradientEnabled() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressGradientEnabled(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressGradientEnabledChanged(bool value);

    /** @brief Progress 模式下，是否显示从起点到当前指针位置的渐变扇形。 */
    Q_PROPERTY(bool sweepAreaVisible READ isSweepAreaVisible WRITE setSweepAreaVisible NOTIFY
                   sweepAreaVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isSweepAreaVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setSweepAreaVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void sweepAreaVisibleChanged(bool value);

    /** @brief 指针扫过扇形的不透明度，取值范围为 [0.0, 1.0]。 */
    Q_PROPERTY(
        qreal sweepAreaOpacity READ sweepAreaOpacity WRITE setSweepAreaOpacity NOTIFY sweepAreaOpacityChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal sweepAreaOpacity() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setSweepAreaOpacity(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void sweepAreaOpacityChanged(qreal value);

    /** @brief 进度环及扫过扇形的渐变起点颜色，无效颜色表示使用较亮的调色板强调色。 */
    Q_PROPERTY(QColor progressGradientStartColor READ progressGradientStartColor WRITE
                   setProgressGradientStartColor NOTIFY progressGradientStartColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor progressGradientStartColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressGradientStartColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressGradientStartColorChanged(QColor value);

    /** @brief 进度环及扫过扇形的渐变终点颜色，无效颜色表示使用调色板强调色。 */
    Q_PROPERTY(QColor progressGradientEndColor READ progressGradientEndColor WRITE setProgressGradientEndColor
                   NOTIFY progressGradientEndColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor progressGradientEndColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressGradientEndColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressGradientEndColorChanged(QColor value);

    /** @brief 指针的线宽 */
    Q_PROPERTY(qreal needleWidth READ needleWidth WRITE setNeedleWidth NOTIFY needleWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal needleWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleWidthChanged(qreal value);

    /** @brief 指针的绘制样式：隐藏、线形或三角形。 */
    Q_PROPERTY(NeedleStyle needleStyle READ needleStyle WRITE setNeedleStyle NOTIFY needleStyleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] NeedleStyle needleStyle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleStyle(NeedleStyle value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleStyleChanged(NeedleStyle value);

    /** @brief 指针长度相对于刻度环半径的比例，取值范围为 [0.05, 1.0]。 */
    Q_PROPERTY(qreal needleLength READ needleLength WRITE setNeedleLength NOTIFY needleLengthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal needleLength() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleLength(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleLengthChanged(qreal value);

    /** @brief 次刻度的长度。 */
    Q_PROPERTY(qreal tickLength READ tickLength WRITE setTickLength NOTIFY tickLengthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal tickLength() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickLength(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickLengthChanged(qreal value);

    /** @brief 次刻度的线宽。 */
    Q_PROPERTY(qreal tickWidth READ tickWidth WRITE setTickWidth NOTIFY tickWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal tickWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickWidthChanged(qreal value);

    /** @brief 主刻度的长度。 */
    Q_PROPERTY(
        qreal majorTickLength READ majorTickLength WRITE setMajorTickLength NOTIFY majorTickLengthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal majorTickLength() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMajorTickLength(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void majorTickLengthChanged(qreal value);

    /** @brief 主刻度的线宽。 */
    Q_PROPERTY(qreal majorTickWidth READ majorTickWidth WRITE setMajorTickWidth NOTIFY majorTickWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal majorTickWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMajorTickWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void majorTickWidthChanged(qreal value);

    /** @brief 外圈刻度与刻度环之间的间距。 */
    Q_PROPERTY(qreal tickPadding READ tickPadding WRITE setTickPadding NOTIFY tickPaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal tickPadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickPadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickPaddingChanged(qreal value);

    /** @brief 是否绘制刻度对应的数值标签。 */
    Q_PROPERTY(bool labelsVisible READ areLabelsVisible WRITE setLabelsVisible NOTIFY labelsVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool areLabelsVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelsVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelsVisibleChanged(bool value);

    /** @brief 数值标签与刻度环内边缘之间的距离。 */
    Q_PROPERTY(qreal labelPadding READ labelPadding WRITE setLabelPadding NOTIFY labelPaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal labelPadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelPadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelPaddingChanged(qreal value);

    /** @brief 数值标签的字体像素大小。 */
    Q_PROPERTY(int labelFontPixelSize READ labelFontPixelSize WRITE setLabelFontPixelSize NOTIFY
                   labelFontPixelSizeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int labelFontPixelSize() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelFontPixelSize(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelFontPixelSizeChanged(int value);

    /** @brief 是否在指针中心绘制圆形轴心，线形和三角形指针均支持。 */
    Q_PROPERTY(bool hubVisible READ isHubVisible WRITE setHubVisible NOTIFY hubVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isHubVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setHubVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void hubVisibleChanged(bool value);

    /** @brief 指针轴心的半径。 */
    Q_PROPERTY(qreal hubRadius READ hubRadius WRITE setHubRadius NOTIFY hubRadiusChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal hubRadius() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setHubRadius(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void hubRadiusChanged(qreal value);

    /** @brief 是否在仪表盘底部显示当前数值。 */
    Q_PROPERTY(bool valueVisible READ isValueVisible WRITE setValueVisible NOTIFY valueVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isValueVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueVisibleChanged(bool value);

    /** @brief 当前数值显示在仪表盘中心或底部。 */
    Q_PROPERTY(
        ValuePosition valuePosition READ valuePosition WRITE setValuePosition NOTIFY valuePositionChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] ValuePosition valuePosition() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValuePosition(ValuePosition value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valuePositionChanged(ValuePosition value);

    /** @brief 显示在当前数值上方的标题。 */
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QString title() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTitle(QString value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void titleChanged(QString value);

    /** @brief 追加在当前数值后的单位文本。 */
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY unitChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QString unit() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setUnit(QString value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void unitChanged(QString value);

    /** @brief 当前数值的字体像素大小，0 表示根据控件尺寸自动计算。 */
    Q_PROPERTY(int valueFontPixelSize READ valueFontPixelSize WRITE setValueFontPixelSize NOTIFY
                   valueFontPixelSizeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int valueFontPixelSize() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueFontPixelSize(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueFontPixelSizeChanged(int value);

    /** @brief 指针颜色，无效颜色表示使用调色板强调色。 */
    Q_PROPERTY(QColor needleColor READ needleColor WRITE setNeedleColor NOTIFY needleColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor needleColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleColorChanged(QColor value);

    /** @brief 刻线颜色，无效颜色表示使用调色板文本色。 */
    Q_PROPERTY(QColor tickColor READ tickColor WRITE setTickColor NOTIFY tickColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor tickColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickColorChanged(QColor value);

    /** @brief 刻度数值标签颜色，无效颜色表示使用调色板文本色。 */
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor labelColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelColorChanged(QColor value);

    /** @brief 标题、当前数值和单位颜色，无效颜色表示使用调色板文本色。 */
    Q_PROPERTY(QColor valueColor READ valueColor WRITE setValueColor NOTIFY valueColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor valueColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueColorChanged(QColor value);

    /** @brief 创建仪表盘，parent 管理控件生命周期。 */
    explicit ZzRadialGauge(QWidget *parent = nullptr);
    ~ZzRadialGauge() override;

    Q_SLOT void setValue(int value);
    [[nodiscard]] bool isValueAnimating() const;

    /** @brief 返回默认展示尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;
    /** @brief 返回建议最小尺寸，小于该值仍安全绘制。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

    /** @brief 返回当前数据项的非拥有指针快照。 */
    [[nodiscard]] QList<ZzRadialGaugeRange *> ranges() const;
    ZzRadialGaugeRange *addRange(int fromValue, int toValue, const QColor &color);
    /** @brief 移除本控件拥有的项，并通过 deleteLater 释放。 */
    void removeRange(ZzRadialGaugeRange *range);
    /** @brief 清空集合并延迟释放全部拥有项；空集合不重复通知。 */
    void clearRanges();

    /** @brief 区间变化后通知；外部 reparent 的移出通知排队到父子关系更新完成后。 */
    Q_SIGNAL void rangesChanged();

  protected:
    /** @brief 基类指针更新数值或量程时取消旧的目标动画。 */
    void sliderChange(SliderChange change) override;
    /** @brief 环境变化时同步有限动画。 */
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

  private:
    [[nodiscard]] qreal sweepAngle() const;
    void settleValueAnimation();
    [[nodiscard]] qreal valueFraction(qreal value) const;
    [[nodiscard]] qreal positionFraction() const;
    [[nodiscard]] int positionFromPoint(const QPointF &point) const;
    void updatePositionFromPoint(const QPointF &point);

  private:
    std::unique_ptr<ZzRadialGaugePrivate> d_ptr;
};

} // namespace ZzFluentUI
