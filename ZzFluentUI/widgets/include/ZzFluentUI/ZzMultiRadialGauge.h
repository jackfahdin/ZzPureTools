#pragma once

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <memory>

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QWidget>

class QPaintEvent;

namespace ZzFluentUI {

class ZzMultiRadialGaugeItemPrivate;

/** @brief 可观察的数据项，由所属仪表盘管理生命周期。 */
class ZZ_FLUENT_UI_EXPORT ZzMultiRadialGaugeItem final : public QObject {
    Q_OBJECT

  public:
    /** @brief 数据项名称，对应 ECharts gauge.data.name。 */
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QString label() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabel(QString value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelChanged(QString value);

    /** @brief 数据项数值，对应 ECharts gauge.data.value。 */
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal value() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValue(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueChanged(qreal value);

    /** @brief 进度弧、指针和数值徽标使用的颜色。 */
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor color() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void colorChanged(QColor value);

    /** @brief 是否绘制当前数据项。 */
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void visibleChanged(bool value);

    /** @brief 名称相对仪表半径的中心偏移，例如 (-0.4, 0.8) 对应 ECharts 的 [-40%, 80%]。 */
    Q_PROPERTY(QPointF titleOffset READ titleOffset WRITE setTitleOffset NOTIFY titleOffsetChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QPointF titleOffset() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTitleOffset(QPointF value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void titleOffsetChanged(QPointF value);

    /** @brief 数值相对仪表半径的中心偏移，例如 (-0.4, 0.95) 对应 ECharts 的 [-40%, 95%]。 */
    Q_PROPERTY(QPointF detailOffset READ detailOffset WRITE setDetailOffset NOTIFY detailOffsetChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QPointF detailOffset() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setDetailOffset(QPointF value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void detailOffsetChanged(QPointF value);

    explicit ZzMultiRadialGaugeItem(QObject *parent = nullptr);
    /** @brief 销毁私有数据。 */
    ~ZzMultiRadialGaugeItem() override;
    ZzMultiRadialGaugeItem(const QString &label, qreal value, const QColor &color, QObject *parent = nullptr);

    Q_SIGNAL void itemChanged();

  private:
    friend class ZzMultiRadialGauge;
    std::unique_ptr<ZzMultiRadialGaugeItemPrivate> d_ptr;
};

class ZzMultiRadialGaugePrivate;

/** @brief 支持主题调色板与有限值动画的径向仪表盘。数值属性拒绝 NaN/Inf，并收敛绘图尺寸与刻度预算。 */
class ZZ_FLUENT_UI_EXPORT ZzMultiRadialGauge final : public QWidget {
    Q_OBJECT

  public:
    enum NeedleStyle { NoNeedle, LineNeedle, TriangleNeedle };
    Q_ENUM(NeedleStyle)

    /** @brief 所有数据项共用的最小值。 */
    Q_PROPERTY(qreal minimum READ minimum WRITE setMinimum NOTIFY minimumChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal minimum() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMinimum(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void minimumChanged(qreal value);

    /** @brief 所有数据项共用的最大值。 */
    Q_PROPERTY(qreal maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal maximum() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMaximum(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void maximumChanged(qreal value);

    /** @brief 刻度环起始角度，正上方为 0°，顺时针为正。 */
    Q_PROPERTY(qreal minimumAngle READ minimumAngle WRITE setMinimumAngle NOTIFY minimumAngleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal minimumAngle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMinimumAngle(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void minimumAngleChanged(qreal value);

    /** @brief 刻度环结束角度，正上方为 0°，顺时针为正。 */
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

    /** @brief 每两个相邻主刻度之间的次刻度数量。 */
    Q_PROPERTY(int minorTickCount READ minorTickCount WRITE setMinorTickCount NOTIFY minorTickCountChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int minorTickCount() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMinorTickCount(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void minorTickCountChanged(int value);

    /** @brief 是否绘制 Track。 */
    Q_PROPERTY(bool trackVisible READ isTrackVisible WRITE setTrackVisible NOTIFY trackVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isTrackVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTrackVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void trackVisibleChanged(bool value);

    /** @brief Track 宽度。 */
    Q_PROPERTY(qreal trackWidth READ trackWidth WRITE setTrackWidth NOTIFY trackWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal trackWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTrackWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void trackWidthChanged(qreal value);

    /** @brief Track 颜色，无效颜色表示使用 QPalette::Mid。 */
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY trackColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor trackColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTrackColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void trackColorChanged(QColor value);

    /** @brief Track 的端点样式。 */
    Q_PROPERTY(
        Qt::PenCapStyle trackCapStyle READ trackCapStyle WRITE setTrackCapStyle NOTIFY trackCapStyleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] Qt::PenCapStyle trackCapStyle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTrackCapStyle(Qt::PenCapStyle value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void trackCapStyleChanged(Qt::PenCapStyle value);

    /** @brief 是否绘制每个数据项的进度弧。 */
    Q_PROPERTY(
        bool progressVisible READ isProgressVisible WRITE setProgressVisible NOTIFY progressVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isProgressVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressVisibleChanged(bool value);

    /** @brief 多条进度弧是否重叠；关闭后按数据项顺序绘制为同心弧。 */
    Q_PROPERTY(
        bool progressOverlap READ isProgressOverlap WRITE setProgressOverlap NOTIFY progressOverlapChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isProgressOverlap() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressOverlap(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressOverlapChanged(bool value);

    /** @brief 每条进度弧的宽度。 */
    Q_PROPERTY(qreal progressWidth READ progressWidth WRITE setProgressWidth NOTIFY progressWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal progressWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressWidthChanged(qreal value);

    /** @brief 不重叠时，相邻进度弧边缘之间的距离。 */
    Q_PROPERTY(
        qreal progressSpacing READ progressSpacing WRITE setProgressSpacing NOTIFY progressSpacingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal progressSpacing() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressSpacing(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressSpacingChanged(qreal value);

    /** @brief 进度弧端点样式。 */
    Q_PROPERTY(Qt::PenCapStyle progressCapStyle READ progressCapStyle WRITE setProgressCapStyle NOTIFY
                   progressCapStyleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] Qt::PenCapStyle progressCapStyle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setProgressCapStyle(Qt::PenCapStyle value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void progressCapStyleChanged(Qt::PenCapStyle value);

    /** @brief 刻度环与控件外边缘之间的距离。 */
    Q_PROPERTY(qreal scalePadding READ scalePadding WRITE setScalePadding NOTIFY scalePaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal scalePadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setScalePadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void scalePaddingChanged(qreal value);

    /** @brief 次刻度长度。 */
    Q_PROPERTY(qreal tickLength READ tickLength WRITE setTickLength NOTIFY tickLengthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal tickLength() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickLength(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickLengthChanged(qreal value);

    /** @brief 次刻度宽度。 */
    Q_PROPERTY(qreal tickWidth READ tickWidth WRITE setTickWidth NOTIFY tickWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal tickWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickWidthChanged(qreal value);

    /** @brief 主刻度长度。 */
    Q_PROPERTY(
        qreal majorTickLength READ majorTickLength WRITE setMajorTickLength NOTIFY majorTickLengthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal majorTickLength() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMajorTickLength(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void majorTickLengthChanged(qreal value);

    /** @brief 主刻度宽度。 */
    Q_PROPERTY(qreal majorTickWidth READ majorTickWidth WRITE setMajorTickWidth NOTIFY majorTickWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal majorTickWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMajorTickWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void majorTickWidthChanged(qreal value);

    /** @brief 刻度与刻度环内边缘之间的距离。 */
    Q_PROPERTY(qreal tickPadding READ tickPadding WRITE setTickPadding NOTIFY tickPaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal tickPadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickPadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickPaddingChanged(qreal value);

    /** @brief 刻线颜色，无效颜色表示使用调色板文本色。 */
    Q_PROPERTY(QColor tickColor READ tickColor WRITE setTickColor NOTIFY tickColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor tickColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTickColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void tickColorChanged(QColor value);

    /** @brief 是否显示主刻度数值标签。 */
    Q_PROPERTY(bool labelsVisible READ areLabelsVisible WRITE setLabelsVisible NOTIFY labelsVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool areLabelsVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelsVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelsVisibleChanged(bool value);

    /** @brief 刻度数值标签与刻线之间的距离。 */
    Q_PROPERTY(qreal labelPadding READ labelPadding WRITE setLabelPadding NOTIFY labelPaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal labelPadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelPadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelPaddingChanged(qreal value);

    /** @brief 刻度数值标签字号。 */
    Q_PROPERTY(int labelFontPixelSize READ labelFontPixelSize WRITE setLabelFontPixelSize NOTIFY
                   labelFontPixelSizeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int labelFontPixelSize() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelFontPixelSize(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelFontPixelSizeChanged(int value);

    /** @brief 刻度数值标签颜色，无效颜色表示使用调色板文本色。 */
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor labelColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelColorChanged(QColor value);

    /** @brief 指针样式，所有数据项共用。 */
    Q_PROPERTY(NeedleStyle needleStyle READ needleStyle WRITE setNeedleStyle NOTIFY needleStyleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] NeedleStyle needleStyle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleStyle(NeedleStyle value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleStyleChanged(NeedleStyle value);

    /** @brief 指针宽度。 */
    Q_PROPERTY(qreal needleWidth READ needleWidth WRITE setNeedleWidth NOTIFY needleWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal needleWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleWidthChanged(qreal value);

    /** @brief 指针长度相对于刻度环半径的比例。 */
    Q_PROPERTY(qreal needleLength READ needleLength WRITE setNeedleLength NOTIFY needleLengthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal needleLength() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleLength(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleLengthChanged(qreal value);

    /** @brief 指针和轴心相对仪表半径的中心偏移。 */
    Q_PROPERTY(QPointF needleOffset READ needleOffset WRITE setNeedleOffset NOTIFY needleOffsetChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QPointF needleOffset() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setNeedleOffset(QPointF value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void needleOffsetChanged(QPointF value);

    /** @brief 是否绘制公共轴心。 */
    Q_PROPERTY(bool hubVisible READ isHubVisible WRITE setHubVisible NOTIFY hubVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isHubVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setHubVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void hubVisibleChanged(bool value);

    /** @brief 公共轴心半径。 */
    Q_PROPERTY(qreal hubRadius READ hubRadius WRITE setHubRadius NOTIFY hubRadiusChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal hubRadius() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setHubRadius(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void hubRadiusChanged(qreal value);

    /** @brief 公共轴心颜色，无效颜色表示使用调色板强调色。 */
    Q_PROPERTY(QColor hubColor READ hubColor WRITE setHubColor NOTIFY hubColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor hubColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setHubColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void hubColorChanged(QColor value);

    /** @brief 是否显示每个数据项的名称。 */
    Q_PROPERTY(bool titleVisible READ isTitleVisible WRITE setTitleVisible NOTIFY titleVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isTitleVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTitleVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void titleVisibleChanged(bool value);

    /** @brief 是否显示每个数据项的数值详情。 */
    Q_PROPERTY(bool detailVisible READ isDetailVisible WRITE setDetailVisible NOTIFY detailVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isDetailVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setDetailVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void detailVisibleChanged(bool value);

    /** @brief 数值是否使用数据项颜色作为圆角徽标背景。 */
    Q_PROPERTY(bool detailBadgeVisible READ isDetailBadgeVisible WRITE setDetailBadgeVisible NOTIFY
                   detailBadgeVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isDetailBadgeVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setDetailBadgeVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void detailBadgeVisibleChanged(bool value);

    /** @brief 数据项名称字号。 */
    Q_PROPERTY(int titleFontPixelSize READ titleFontPixelSize WRITE setTitleFontPixelSize NOTIFY
                   titleFontPixelSizeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int titleFontPixelSize() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTitleFontPixelSize(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void titleFontPixelSizeChanged(int value);

    /** @brief 数据项数值字号。 */
    Q_PROPERTY(int detailFontPixelSize READ detailFontPixelSize WRITE setDetailFontPixelSize NOTIFY
                   detailFontPixelSizeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int detailFontPixelSize() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setDetailFontPixelSize(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void detailFontPixelSizeChanged(int value);

    /** @brief 数据项名称颜色，无效颜色表示使用调色板文本色。 */
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor NOTIFY titleColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor titleColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTitleColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void titleColorChanged(QColor value);

    /** @brief 徽标内数值颜色，无效颜色表示自动选择。 */
    Q_PROPERTY(
        QColor detailTextColor READ detailTextColor WRITE setDetailTextColor NOTIFY detailTextColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor detailTextColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setDetailTextColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void detailTextColorChanged(QColor value);

    /** @brief 数值徽标的水平内边距。 */
    Q_PROPERTY(qreal detailBadgePadding READ detailBadgePadding WRITE setDetailBadgePadding NOTIFY
                   detailBadgePaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal detailBadgePadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setDetailBadgePadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void detailBadgePaddingChanged(qreal value);

    /** @brief 追加在每个数值后的文本。 */
    Q_PROPERTY(QString valueSuffix READ valueSuffix WRITE setValueSuffix NOTIFY valueSuffixChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QString valueSuffix() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueSuffix(QString value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueSuffixChanged(QString value);

    /** @brief 数值详情的小数位数。 */
    Q_PROPERTY(int valueDecimals READ valueDecimals WRITE setValueDecimals NOTIFY valueDecimalsChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int valueDecimals() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueDecimals(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueDecimalsChanged(int value);

    /** @brief 数据项数值变化动画时长，单位为毫秒；0 表示关闭动画。 */
    Q_PROPERTY(int valueAnimationDuration READ valueAnimationDuration WRITE setValueAnimationDuration NOTIFY
                   valueAnimationDurationChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int valueAnimationDuration() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueAnimationDuration(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueAnimationDurationChanged(int value);

    /** @brief 创建仪表盘，parent 管理控件生命周期。 */
    explicit ZzMultiRadialGauge(QWidget *parent = nullptr);
    ~ZzMultiRadialGauge() override;

    /** @brief 原子设置有限量程，通知重入时丢弃过期通知。 */
    void setRange(qreal minimum, qreal maximum);
    /** @brief 返回当前数据项的非拥有指针快照。 */
    [[nodiscard]] QList<ZzMultiRadialGaugeItem *> items() const;
    /** @brief 创建并接管数据项；添加通知中被销毁时返回 nullptr。 */
    ZzMultiRadialGaugeItem *addItem(const QString &label, qreal value, const QColor &color = QColor());
    /** @brief 接管数据项，支持跨控件转移；忽略重复和待删除项。 */
    void addItem(ZzMultiRadialGaugeItem *item);
    /** @brief 移除本控件拥有的项，并通过 deleteLater 释放。 */
    void removeItem(ZzMultiRadialGaugeItem *item);
    /** @brief 清空集合并延迟释放全部拥有项；空集合不重复通知。 */
    void clearItems();

    /** @brief 返回默认展示尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;
    /** @brief 返回建议最小尺寸，小于该值仍安全绘制。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

    /** @brief 集合或数据变化后通知；外部 reparent 的移出通知排队到父子关系更新完成后。 */
    Q_SIGNAL void itemsChanged();

  protected:
    /** @brief 环境变化时同步有限动画。 */
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

  private:
    [[nodiscard]] qreal sweepAngle() const;
    [[nodiscard]] qreal valueFraction(qreal value) const;
    [[nodiscard]] qreal displayedValue(const ZzMultiRadialGaugeItem *item) const;
    void connectItem(ZzMultiRadialGaugeItem *item);
    void startValueAnimation();
    void synchronizeDisplayedValues();

  private:
    std::unique_ptr<ZzMultiRadialGaugePrivate> d_ptr;
};

} // namespace ZzFluentUI
