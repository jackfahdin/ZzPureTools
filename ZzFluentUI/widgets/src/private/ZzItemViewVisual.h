#pragma once

#include <QtCore/QRect>
#include <QtCore/QRectF>

class QModelIndex;
class QPainter;
class QStyleOptionViewItem;
class QWidget;

namespace ZzFluentUI {

class ZzFluentStyle;

/** @brief 指定 item 强调条使用逻辑方向或固定物理边。 */
enum class ZzItemIndicatorPlacement : unsigned char
{
    /** @brief 使用布局方向的 leading 边。 */
    LogicalLeading,
    /** @brief 固定使用物理左边，不随 RTL 翻转。 */
    PhysicalLeft,
    /** @brief 固定使用物理右边，不随 RTL 翻转。 */
    PhysicalRight
};

/** @brief 描述单个 item 选中视觉的绘制职责。 */
struct ZzItemViewVisualOptions final
{
    /** @brief 当前调用是否负责绘制交互背板。 */
    bool drawSurface = true;
    /** @brief 当前 item 是否拥有唯一强调条及其固定内容槽位。 */
    bool ownsIndicator = true;
    /** @brief 未选中的旧 item 是否仍需绘制收缩中的强调条。 */
    bool forceIndicator = false;
    /** @brief 是否绘制选中背景和强调条，但不影响模型选择语义。 */
    bool showSelection = true;
    /** @brief 强调条沿长轴绘制的比例，自动收敛到 0 至 1。 */
    qreal indicatorScale = 1.0;
    /** @brief 强调条的逻辑或固定物理边放置策略。 */
    ZzItemIndicatorPlacement indicatorPlacement =
        ZzItemIndicatorPlacement::LogicalLeading;
};

/** @brief 保存由同一几何契约计算的背板、强调条和内容区域。 */
struct ZzItemViewVisualLayout final
{
    QRectF surfaceRect;
    QRect indicatorRect;
    QRect contentRect;
};

/**
 * @brief 为导航、列表、表格与树形 item 提供统一选中视觉。
 *
 * 接口只执行当前可见 index 的常量时间几何计算和绘制，不缓存模型数据，
 * 也不分配逐 item 对象。
 */
class ZzItemViewVisual final
{
public:
    /**
     * @brief 判断当前 index 是否拥有所在行的唯一强调条。
     * @param widget 当前 item 所属视图。
     * @param index 当前模型索引。
     * @return 树形树列、表格行选择的首个可见列或普通 item 返回 true。
     */
    [[nodiscard]] static bool ownsIndicator(
        const QWidget *widget,
        const QModelIndex &index) noexcept;

    /**
     * @brief 按统一契约绘制背板与强调条并返回内容安全区域。
     * @param style 提供当前 Fluent 主题快照的样式。
     * @param option 当前 item 状态和矩形。
     * @param painter 非空绘制目标。
     * @param options 当前调用承担的绘制职责。
     * @return 与布局方向一致且不覆盖强调条的内容矩形。
     */
    [[nodiscard]] static ZzItemViewVisualLayout draw(
        const ZzFluentStyle &style,
        const QStyleOptionViewItem &option,
        QPainter *painter,
        ZzItemViewVisualOptions options = {});

private:
    ZzItemViewVisual() = delete;
};

} // namespace ZzFluentUI
