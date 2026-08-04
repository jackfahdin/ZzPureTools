#pragma once

#include <array>
#include <cstddef>

#include <QtCore/QCache>
#include <QtGui/QBrush>
#include <QtGui/QPixmap>

#include <ZzFluentUI/ZzIconCacheKey.h>

namespace ZzFluentUI {

class ZzThemeSnapshot;

/** @brief 保存单个交互状态的无分配画刷槽。 */
struct ZzStyleVisual final
{
    QBrush fill;
    QBrush stroke;
};

/** @brief 保存固定视觉槽和以字节预算管理的 LRU 图标缓存。 */
class ZzStyleCache final
{
public:
    /** @brief 创建指定最大图标字节预算的缓存。 */
    explicit ZzStyleCache(int maximumIconBytes);

    /** @brief 从完整主题快照一次重建四个视觉槽。 */
    void rebuildVisuals(const ZzThemeSnapshot &snapshot);

    /** @brief O(1) 返回视觉槽；越界时断言并回退到普通状态。 */
    [[nodiscard]] const ZzStyleVisual &visual(
        std::size_t stateIndex) const noexcept;

    /** @brief 返回非拥有缓存指针；后续缓存写入可能使其失效。 */
    [[nodiscard]] const QPixmap *icon(
        const ZzIconCacheKey &key) const noexcept;

    /** @brief 按物理像素字节成本插入图标，超预算时忽略。 */
    void insertIcon(const ZzIconCacheKey &key, QPixmap pixmap);

    /** @brief 清空全部图标，不修改固定视觉槽。 */
    void clearIcons() noexcept;

    /** @brief 返回当前图标缓存总字节成本。 */
    [[nodiscard]] int iconBytes() const noexcept;

private:
    std::array<ZzStyleVisual, 4> visuals_;
    QCache<ZzIconCacheKey, QPixmap> icons_;
};

} // namespace ZzFluentUI
