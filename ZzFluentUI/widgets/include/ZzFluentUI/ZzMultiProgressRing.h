#pragma once

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <memory>

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QWidget>

class QPaintEvent;

namespace ZzFluentUI {

class ZzMultiProgressRingItemPrivate;

/** @brief 可观察的数据项，由所属仪表盘管理生命周期。 */
class ZZ_FLUENT_UI_EXPORT ZzMultiProgressRingItem final : public QObject {
    Q_OBJECT

  public:
    /** @brief 环对应的名称，显示在中央详情区域。 */
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QString label() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabel(QString value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelChanged(QString value);

    /** @brief 环的当前数值，由所属控件的 minimum 和 maximum 共同解释。 */
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal value() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValue(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueChanged(qreal value);

    /** @brief 环的前景颜色，无效颜色表示使用调色板强调色。 */
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor color() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void colorChanged(QColor value);

    explicit ZzMultiProgressRingItem(QObject *parent = nullptr);
    /** @brief 销毁私有数据。 */
    ~ZzMultiProgressRingItem() override;
    ZzMultiProgressRingItem(const QString &label, qreal value, const QColor &color,
                            QObject *parent = nullptr);

    Q_SIGNAL void itemChanged();

  private:
    friend class ZzMultiProgressRing;
    std::unique_ptr<ZzMultiProgressRingItemPrivate> d_ptr;
};

class ZzMultiProgressRingPrivate;

/** @brief 支持主题调色板与有限值动画的径向仪表盘。数值属性拒绝 NaN/Inf，并收敛绘图尺寸与刻度预算。 */
class ZZ_FLUENT_UI_EXPORT ZzMultiProgressRing final : public QWidget {
    Q_OBJECT

  public:
    /** @brief 所有环共用的最小值。 */
    Q_PROPERTY(qreal minimum READ minimum WRITE setMinimum NOTIFY minimumChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal minimum() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMinimum(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void minimumChanged(qreal value);

    /** @brief 所有环共用的最大值。 */
    Q_PROPERTY(qreal maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal maximum() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setMaximum(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void maximumChanged(qreal value);

    /** @brief 圆弧起始角度，正上方为 0°，顺时针为正。 */
    Q_PROPERTY(qreal startAngle READ startAngle WRITE setStartAngle NOTIFY startAngleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal startAngle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setStartAngle(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void startAngleChanged(qreal value);

    /** @brief 圆弧沿顺时针方向扫过的角度，取值范围为 [0°, 360°]。 */
    Q_PROPERTY(qreal sweepAngle READ sweepAngle WRITE setSweepAngle NOTIFY sweepAngleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal sweepAngle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setSweepAngle(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void sweepAngleChanged(qreal value);

    /** @brief 每条环的线宽，单位为逻辑像素。 */
    Q_PROPERTY(qreal ringWidth READ ringWidth WRITE setRingWidth NOTIFY ringWidthChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal ringWidth() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setRingWidth(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void ringWidthChanged(qreal value);

    /** @brief 相邻两条环边缘之间的距离，单位为逻辑像素。 */
    Q_PROPERTY(qreal ringSpacing READ ringSpacing WRITE setRingSpacing NOTIFY ringSpacingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal ringSpacing() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setRingSpacing(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void ringSpacingChanged(qreal value);

    /** @brief 最外层环与控件边缘之间的距离，单位为逻辑像素。 */
    Q_PROPERTY(qreal ringPadding READ ringPadding WRITE setRingPadding NOTIFY ringPaddingChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] qreal ringPadding() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setRingPadding(qreal value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void ringPaddingChanged(qreal value);

    /** @brief 进度环和 Track 的端点样式。 */
    Q_PROPERTY(Qt::PenCapStyle capStyle READ capStyle WRITE setCapStyle NOTIFY capStyleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] Qt::PenCapStyle capStyle() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setCapStyle(Qt::PenCapStyle value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void capStyleChanged(Qt::PenCapStyle value);

    /** @brief 是否在每条进度环后方绘制完整 Track。 */
    Q_PROPERTY(bool trackVisible READ isTrackVisible WRITE setTrackVisible NOTIFY trackVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isTrackVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTrackVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void trackVisibleChanged(bool value);

    /** @brief Track 颜色，无效颜色表示使用 QPalette::Mid。 */
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY trackColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor trackColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setTrackColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void trackColorChanged(QColor value);

    /** @brief 是否绘制中央的名称和数值详情。 */
    Q_PROPERTY(
        bool detailsVisible READ areDetailsVisible WRITE setDetailsVisible NOTIFY detailsVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool areDetailsVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setDetailsVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void detailsVisibleChanged(bool value);

    /** @brief 数值是否绘制带颜色边框的圆角徽标。 */
    Q_PROPERTY(bool valueBadgeVisible READ isValueBadgeVisible WRITE setValueBadgeVisible NOTIFY
                   valueBadgeVisibleChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] bool isValueBadgeVisible() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueBadgeVisible(bool value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueBadgeVisibleChanged(bool value);

    /** @brief 中央名称颜色，无效颜色表示使用 QPalette::Text。 */
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor NOTIFY labelColorChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QColor labelColor() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelColor(QColor value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelColorChanged(QColor value);

    /** @brief 追加在每个数值后的文本，例如百分号。 */
    Q_PROPERTY(QString valueSuffix READ valueSuffix WRITE setValueSuffix NOTIFY valueSuffixChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] QString valueSuffix() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueSuffix(QString value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueSuffixChanged(QString value);

    /** @brief 数值显示的小数位数，取值范围为 [0, 6]。 */
    Q_PROPERTY(int valueDecimals READ valueDecimals WRITE setValueDecimals NOTIFY valueDecimalsChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int valueDecimals() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueDecimals(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueDecimalsChanged(int value);

    /** @brief 中央名称的字体像素大小，0 表示根据控件尺寸自动计算。 */
    Q_PROPERTY(int labelFontPixelSize READ labelFontPixelSize WRITE setLabelFontPixelSize NOTIFY
                   labelFontPixelSizeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int labelFontPixelSize() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setLabelFontPixelSize(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void labelFontPixelSizeChanged(int value);

    /** @brief 中央数值的字体像素大小，0 表示根据控件尺寸自动计算。 */
    Q_PROPERTY(int valueFontPixelSize READ valueFontPixelSize WRITE setValueFontPixelSize NOTIFY
                   valueFontPixelSizeChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int valueFontPixelSize() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueFontPixelSize(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueFontPixelSizeChanged(int value);

    /** @brief 数值变化动画时长，单位为毫秒；0 表示关闭动画。 */
    Q_PROPERTY(int valueAnimationDuration READ valueAnimationDuration WRITE setValueAnimationDuration NOTIFY
                   valueAnimationDurationChanged)
    /** @brief 返回当前属性值。 */
    [[nodiscard]] int valueAnimationDuration() const;
    /** @brief 设置属性，实际值变化时发出对应通知。 */
    void setValueAnimationDuration(int value);
    /** @brief 有效属性值实际变化时发出。 */
    Q_SIGNAL void valueAnimationDurationChanged(int value);

    /** @brief 创建仪表盘，parent 管理控件生命周期。 */
    explicit ZzMultiProgressRing(QWidget *parent = nullptr);
    ~ZzMultiProgressRing() override;

    /** @brief 原子设置有限量程，通知重入时丢弃过期通知。 */
    void setRange(qreal minimum, qreal maximum);
    /** @brief 返回当前数据项的非拥有指针快照。 */
    [[nodiscard]] QList<ZzMultiProgressRingItem *> items() const;
    /** @brief 创建并接管数据项；添加通知中被销毁时返回 nullptr。 */
    ZzMultiProgressRingItem *addItem(const QString &label, qreal value, const QColor &color = QColor());
    /** @brief 接管数据项，支持跨控件转移；忽略重复和待删除项。 */
    void addItem(ZzMultiProgressRingItem *item);
    /** @brief 移除本控件拥有的项，并通过 deleteLater 释放。 */
    void removeItem(ZzMultiProgressRingItem *item);
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
    [[nodiscard]] qreal itemFraction(qreal value) const;
    [[nodiscard]] qreal displayedValue(const ZzMultiProgressRingItem *item) const;
    void connectItem(ZzMultiProgressRingItem *item);
    void startValueAnimation();
    void synchronizeDisplayedValues();

  private:
    std::unique_ptr<ZzMultiProgressRingPrivate> d_ptr;
};

} // namespace ZzFluentUI
