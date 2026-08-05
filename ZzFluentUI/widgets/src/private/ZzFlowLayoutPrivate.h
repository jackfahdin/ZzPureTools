#pragma once

#include <cstdint>
#include <vector>

#include <QtCore/QList>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtWidgets/QSizePolicy>

class QLayoutItem;

namespace ZzFluentUI {

class ZzFlowLayout;

/** @brief 保存一行内单个 item 的逻辑位置和规范化尺寸。 */
struct ZzFlowItemPlacement final
{
    QLayoutItem *item = nullptr;
    QSize size;
    int logicalX = 0;
    QSizePolicy::ControlTypes controlTypes;
};

/** @brief 持有流式布局 item、缓存并执行线性几何计算。 */
class ZzFlowLayoutPrivate final
{
public:
    /** @brief 绑定公开布局，不取得 QObject 所有权。 */
    explicit ZzFlowLayoutPrivate(ZzFlowLayout *q) noexcept;

    /** @brief 删除布局仍拥有的全部 layout item。 */
    ~ZzFlowLayoutPrivate();

    /** @brief 计算高度或同步应用给定矩形中的 item 几何。 */
    [[nodiscard]] int doLayout(const QRect &rect, bool applyGeometry) const;

    /** @brief 清除所有依赖 item、间距或 style 的派生缓存。 */
    void invalidateCaches() noexcept;

    /** @brief 计算并缓存 minimumSize 与 sizeHint。 */
    void ensureSizeHints() const;

    /** @brief 返回 style metric，负值表示需要按相邻控件解析。 */
    [[nodiscard]] int styleSpacing(Qt::Orientation orientation) const;

    /** @brief 返回两个相邻 item 之间的非负轴向间距。 */
    [[nodiscard]] int adjacentSpacing(
        Qt::Orientation orientation,
        int styleMetric,
        QSizePolicy::ControlTypes first,
        QSizePolicy::ControlTypes second) const;

    /** @brief 按可用行宽规范化 item 的首选尺寸。 */
    [[nodiscard]] QSize itemSize(
        QLayoutItem *item,
        int availableWidth) const;

    ZzFlowLayout *const q_ptr;
    QList<QLayoutItem *> items;
    int horizontalSpacing = -1;
    int verticalSpacing = -1;
    std::uint64_t generation = 1;

    mutable std::vector<ZzFlowItemPlacement> lineScratch;
    mutable int cachedWidth = -1;
    mutable int cachedHeight = 0;
    mutable std::uint64_t cachedHeightGeneration = 0;
    mutable QSize cachedMinimumSize;
    mutable QSize cachedSizeHint;
    mutable std::uint64_t cachedSizeGeneration = 0;
    mutable QRect appliedGeometry;
    mutable std::uint64_t appliedGeometryGeneration = 0;
};

} // namespace ZzFluentUI
