#pragma once

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzSegoeIconFont.h>

#include <QColor>
#include <QDateTime>
#include <QListView>
#include <QPalette>
#include <memory>

class QHideEvent;
class QShowEvent;
class QChildEvent;

namespace ZzFluentUI {

class ZzTimelineEventPrivate;
class ZzTimelinePrivate;
class ZzTimelineDelegate;

/** @brief 时间轴中的可观察事件，由所属时间轴接管生命周期。 */
class ZZ_FLUENT_UI_EXPORT ZzTimelineEvent final : public QObject {
    Q_OBJECT
public:
    enum ZzTimelineStatus { Normal, Completed, Current, Pending, Warning, Error };
    Q_ENUM(ZzTimelineStatus)
    enum ZzTimelinePlacement { Automatic, LeftSide, RightSide };
    Q_ENUM(ZzTimelinePlacement)

    /** @brief 事件发生时间；无效时间不绘制时间文本。 */
    Q_PROPERTY(QDateTime timestamp READ timestamp WRITE setTimestamp NOTIFY timestampChanged)
    /** @brief 自定义时间文字，非空时覆盖格式化时间。 */
    Q_PROPERTY(QString timeText READ timeText WRITE setTimeText NOTIFY timeTextChanged)
    /** @brief 事件标题。 */
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    /** @brief 事件描述。 */
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    /** @brief 节点状态，决定默认颜色和图形。 */
    Q_PROPERTY(ZzTimelineStatus status READ status WRITE setStatus NOTIFY statusChanged)
    /** @brief 自定义节点颜色；无效颜色使用状态颜色。 */
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    /** @brief 原始 Segoe Fluent Icons 字体字符。 */
    Q_PROPERTY(QString icon READ icon WRITE setIcon NOTIFY iconChanged)
    /** @brief 交错布局中该事件的内容侧。 */
    Q_PROPERTY(ZzTimelinePlacement placement READ placement WRITE setPlacement NOTIFY placementChanged)

    explicit ZzTimelineEvent(QObject* parent = nullptr);
    ZzTimelineEvent(const QDateTime& timestamp, const QString& title, const QString& description = {},
        ZzTimelineStatus status = Normal, QObject* parent = nullptr);
    ~ZzTimelineEvent() override;

    [[nodiscard]] QDateTime timestamp() const;
    void setTimestamp(QDateTime value);
    Q_SIGNAL void timestampChanged(QDateTime value);
    [[nodiscard]] QString timeText() const;
    void setTimeText(QString value);
    Q_SIGNAL void timeTextChanged(QString value);
    [[nodiscard]] QString title() const;
    void setTitle(QString value);
    Q_SIGNAL void titleChanged(QString value);
    [[nodiscard]] QString description() const;
    void setDescription(QString value);
    Q_SIGNAL void descriptionChanged(QString value);
    [[nodiscard]] ZzTimelineStatus status() const;
    void setStatus(ZzTimelineStatus value);
    Q_SIGNAL void statusChanged(ZzTimelineStatus value);
    [[nodiscard]] QColor color() const;
    void setColor(QColor value);
    Q_SIGNAL void colorChanged(QColor value);
    [[nodiscard]] QString icon() const;
    void setIcon(QString value);
    void setIcon(ZzSegoeIcon icon);
    Q_SIGNAL void iconChanged(QString value);
    [[nodiscard]] ZzTimelinePlacement placement() const;
    void setPlacement(ZzTimelinePlacement value);
    Q_SIGNAL void placementChanged(ZzTimelinePlacement value);
    /** @brief 任一事件属性实际变化后发出。 */
    Q_SIGNAL void itemChanged();

private:
    std::unique_ptr<ZzTimelineEventPrivate> d_ptr;
};

/** @brief 可反序、交错和横纵布局的 Fluent 时间轴视图。仅在 GUI 线程使用。 */
class ZZ_FLUENT_UI_EXPORT ZzTimeline final : public QListView {
    Q_OBJECT
public:
    enum ZzTimelineLayoutMode { ContentOnRight, ContentOnLeft, Alternating, AlternatingReverse };
    Q_ENUM(ZzTimelineLayoutMode)

    /** @brief 时间轴方向。 */
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    /** @brief 内容相对时间轴的位置。 */
    Q_PROPERTY(ZzTimelineLayoutMode layoutMode READ layoutMode WRITE setLayoutMode NOTIFY layoutModeChanged)
    /** @brief 仅反转显示顺序，不改变 events() 的插入顺序。 */
    Q_PROPERTY(bool reverse READ isReverse WRITE setReverse NOTIFY reverseChanged)
    /** @brief 是否显示时间。 */
    Q_PROPERTY(bool timestampVisible READ isTimestampVisible WRITE setTimestampVisible NOTIFY
            timestampVisibleChanged)
    /** @brief 是否显示描述。 */
    Q_PROPERTY(bool descriptionVisible READ isDescriptionVisible WRITE setDescriptionVisible NOTIFY
            descriptionVisibleChanged)
    /** @brief 时间格式；默认 yyyy-MM-dd HH:mm。 */
    Q_PROPERTY(
        QString timestampFormat READ timestampFormat WRITE setTimestampFormat NOTIFY timestampFormatChanged)
    /** @brief 时间列宽度，范围 32 至 400。 */
    Q_PROPERTY(int timestampWidth READ timestampWidth WRITE setTimestampWidth NOTIFY timestampWidthChanged)
    /** @brief 节点直径，范围 6 至 64。 */
    Q_PROPERTY(int nodeSize READ nodeSize WRITE setNodeSize NOTIFY nodeSizeChanged)
    /** @brief 连接线宽度，范围 0.5 至 24。 */
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
    /** @brief 纵向相邻事件间距，范围 0 至 160。 */
    Q_PROPERTY(int itemSpacing READ itemSpacing WRITE setItemSpacing NOTIFY itemSpacingChanged)
    /** @brief 横向单事件宽度，范围 120 至 640。 */
    Q_PROPERTY(int horizontalItemWidth READ horizontalItemWidth WRITE setHorizontalItemWidth NOTIFY
            horizontalItemWidthChanged)
    /** @brief 视口内容内边距，范围 0 至 160。 */
    Q_PROPERTY(int contentPadding READ contentPadding WRITE setContentPadding NOTIFY contentPaddingChanged)
    /** @brief 连接线颜色；无效颜色按调色板计算。 */
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged)
    /** @brief 标题字号，0 表示使用控件字体。 */
    Q_PROPERTY(int titleFontPixelSize READ titleFontPixelSize WRITE setTitleFontPixelSize NOTIFY
            titleFontPixelSizeChanged)
    /** @brief 描述字号，0 表示自动字号。 */
    Q_PROPERTY(int descriptionFontPixelSize READ descriptionFontPixelSize WRITE setDescriptionFontPixelSize
            NOTIFY descriptionFontPixelSizeChanged)
    /** @brief 时间字号，0 表示自动字号。 */
    Q_PROPERTY(int timestampFontPixelSize READ timestampFontPixelSize WRITE setTimestampFontPixelSize NOTIFY
            timestampFontPixelSizeChanged)
    /** @brief 是否允许 Current 节点呼吸动效；仍遵从减少动效样式。 */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY
            animationEnabledChanged)
    /** @brief 呼吸动效单周期毫秒数，范围 200 至 10000。 */
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY
            animationDurationChanged)

    explicit ZzTimeline(QWidget* parent = nullptr);
    ~ZzTimeline() override;

#define ZZ_TIMELINE_PROPERTY(Type, Name, Getter, Setter)                                                     \
    [[nodiscard]] Type Getter() const;                                                                       \
    void Setter(Type value);                                                                                 \
    Q_SIGNAL void Name##Changed(Type value);
    ZZ_TIMELINE_PROPERTY(Qt::Orientation, orientation, orientation, setOrientation)
    ZZ_TIMELINE_PROPERTY(ZzTimelineLayoutMode, layoutMode, layoutMode, setLayoutMode)
    ZZ_TIMELINE_PROPERTY(bool, reverse, isReverse, setReverse)
    ZZ_TIMELINE_PROPERTY(bool, timestampVisible, isTimestampVisible, setTimestampVisible)
    ZZ_TIMELINE_PROPERTY(bool, descriptionVisible, isDescriptionVisible, setDescriptionVisible)
    ZZ_TIMELINE_PROPERTY(QString, timestampFormat, timestampFormat, setTimestampFormat)
    ZZ_TIMELINE_PROPERTY(int, timestampWidth, timestampWidth, setTimestampWidth)
    ZZ_TIMELINE_PROPERTY(int, nodeSize, nodeSize, setNodeSize)
    ZZ_TIMELINE_PROPERTY(qreal, lineWidth, lineWidth, setLineWidth)
    ZZ_TIMELINE_PROPERTY(int, itemSpacing, itemSpacing, setItemSpacing)
    ZZ_TIMELINE_PROPERTY(int, horizontalItemWidth, horizontalItemWidth, setHorizontalItemWidth)
    ZZ_TIMELINE_PROPERTY(int, contentPadding, contentPadding, setContentPadding)
    ZZ_TIMELINE_PROPERTY(QColor, lineColor, lineColor, setLineColor)
    ZZ_TIMELINE_PROPERTY(int, titleFontPixelSize, titleFontPixelSize, setTitleFontPixelSize)
    ZZ_TIMELINE_PROPERTY(int, descriptionFontPixelSize, descriptionFontPixelSize, setDescriptionFontPixelSize)
    ZZ_TIMELINE_PROPERTY(int, timestampFontPixelSize, timestampFontPixelSize, setTimestampFontPixelSize)
    ZZ_TIMELINE_PROPERTY(bool, animationEnabled, isAnimationEnabled, setAnimationEnabled)
    ZZ_TIMELINE_PROPERTY(int, animationDuration, animationDuration, setAnimationDuration)
#undef ZZ_TIMELINE_PROPERTY

    /** @brief 按插入顺序返回受本时间轴管理的事件。 */
    [[nodiscard]] QList<ZzTimelineEvent*> events() const;
    /** @brief 按显示顺序取事件；越界返回空。 */
    [[nodiscard]] ZzTimelineEvent* eventAt(int visualIndex) const;
    /** @brief 创建并接管事件；回调已删除或转移事件时返回空。 */
    ZzTimelineEvent* addEvent(const QDateTime& timestamp, const QString& title,
        const QString& description = {}, ZzTimelineEvent::ZzTimelineStatus status = ZzTimelineEvent::Normal);
    /** @brief 接管已有事件；跨时间轴转移会解除旧所属关系。 */
    void addEvent(ZzTimelineEvent* event);
    /** @brief 移出事件并解除父对象；回调重新接管时返回空。 */
    ZzTimelineEvent* takeEvent(ZzTimelineEvent* event);
    /** @brief 从当前时间轴移除并延迟销毁未被回调重新接管的事件。 */
    void removeEvent(ZzTimelineEvent* event);
    /** @brief 移除全部事件并延迟销毁未被重新接管的事件。 */
    void clearEvents();
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    Q_SIGNAL void eventsChanged();
    Q_SIGNAL void eventClicked(ZzTimelineEvent* event);
    Q_SIGNAL void eventActivated(ZzTimelineEvent* event);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void childEvent(QChildEvent* event) override;

private:
    friend class ZzTimelineDelegate;
    [[nodiscard]] QString formattedTimestamp(const ZzTimelineEvent* event) const;
    [[nodiscard]] QColor resolvedEventColor(const ZzTimelineEvent* event, QPalette::ColorGroup group) const;
    [[nodiscard]] qreal pulseProgress() const;
    void applyViewOrientation();
    void connectEvent(ZzTimelineEvent* event);
    void markExternalDetach(ZzTimelineEvent* event);
    void refreshItemLayout();
    void updateAnimationState();
    using QListView::setFlow;
    using QListView::setItemDelegate;
    using QListView::setModel;
    using QListView::setWrapping;
    std::unique_ptr<ZzTimelinePrivate> d_ptr;
};

} // namespace ZzFluentUI
