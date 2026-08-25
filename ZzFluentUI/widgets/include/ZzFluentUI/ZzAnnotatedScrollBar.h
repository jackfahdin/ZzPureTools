#pragma once

#include <memory>

#include <QtCore/QModelIndex>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzScrollBar.h>

class QAbstractItemModel;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QStyleOptionSlider;

namespace ZzFluentUI {

class ZzAnnotatedScrollBarPrivate;

/** @brief 模型中描述滚动条标记的角色。 */
enum class ZzScrollMarkerRole : int
{
    Position = Qt::UserRole + 1,
    Kind,
    Priority,
    Color
};

/** @brief 标记的主题语义类别；Custom 使用模型提供的颜色。 */
enum class ZzScrollMarkerKind : int
{
    Information,
    Success,
    Warning,
    Error,
    Custom
};

/**
 * @brief 在保留原生滚动行为的同时，从扁平模型绘制语义标记。
 *
 * 模型不被拥有。每个顶层行的第零列可通过 ZzScrollMarkerRole 提供
 * 归一化位置、类别、优先级和 Custom 颜色。无效位置会被忽略而不会改写模型。
 */
class ZZ_FLUENT_UI_EXPORT ZzAnnotatedScrollBar final : public ZzScrollBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzAnnotatedScrollBar)

public:
    /** @brief 创建垂直模型标记滚动条。 */
    explicit ZzAnnotatedScrollBar(QWidget *parent = nullptr);

    /** @brief 创建指定方向的模型标记滚动条。 */
    explicit ZzAnnotatedScrollBar(
        Qt::Orientation orientation,
        QWidget *parent = nullptr);

    /** @brief 销毁非拥有模型连接和标记缓存。 */
    ~ZzAnnotatedScrollBar() override;

    /** @brief 返回当前非拥有标记模型；模型销毁后返回 nullptr。 */
    [[nodiscard]] QAbstractItemModel *markerModel() const noexcept;

    /** @brief 设置非拥有扁平标记模型；等值调用不发信号。 */
    void setMarkerModel(QAbstractItemModel *model);

    /** @brief 返回是否允许鼠标点击标记直接定位。 */
    [[nodiscard]] bool markersInteractive() const noexcept;

    /** @brief 启用或关闭标记点击；关闭时完整委托 QScrollBar。 */
    void setMarkersInteractive(bool interactive);

    /**
     * @brief 返回命中点对应的源模型索引。
     * @return 未命中、模型销毁或缓存为空时返回无效索引。
     */
    [[nodiscard]] QModelIndex markerAt(const QPoint &point) const;

Q_SIGNALS:
    /** @brief 在非拥有标记模型变更或销毁后发出。 */
    void markerModelChanged(QAbstractItemModel *model);

    /** @brief 点击命中标记并更新范围值后发出源索引。 */
    void markerActivated(const QModelIndex &index);

protected:
    /** @brief 先绘制原生滚动条，再绘制已缓存的像素桶。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 命中交互标记时定位并通知，未命中时委托基类。 */
    void mousePressEvent(QMouseEvent *event) override;

    /** @brief 用于从缓存源索引提供标记 tooltip。 */
    bool event(QEvent *event) override;

    /** @brief 尺寸改变后重建有上界的像素桶。 */
    void resizeEvent(QResizeEvent *event) override;

    /** @brief 方向、style 和 palette 改变后刷新颜色或像素桶。 */
    void changeEvent(QEvent *event) override;

    /** @brief 运行时方向切换后同步标记像素桶主轴。 */
    void sliderChange(SliderChange change) override;

private:
    friend class ZzAnnotatedScrollBarPrivate;

    /** @brief 向私有缓存提供与原生滚动条一致的 style option。 */
    void initMarkerStyleOption(QStyleOptionSlider *option) const;

    std::unique_ptr<ZzAnnotatedScrollBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
