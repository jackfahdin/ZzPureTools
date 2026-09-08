#pragma once

#include <ZzFluentUI/ZzItemDensity.h>

class QModelIndex;
class QPainter;
class QStyleOptionViewItem;

namespace ZzFluentUI {

class ZzFluentItemDelegate;

/** @brief 保存密度并执行单 index、无模型缓存的局部绘制。 */
class ZzFluentItemDelegatePrivate final
{
public:
    /** @brief 绑定非空 public delegate。 */
    explicit ZzFluentItemDelegatePrivate(
        ZzFluentItemDelegate *publicObject) noexcept;

    /** @brief 初始化一个 option 副本并绘制当前 index。 */
    void paint(
        QPainter *painter,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const;

    ZzFluentItemDelegate *const q_ptr;
    ZzItemDensity density = ZzItemDensity::Standard;
};

} // namespace ZzFluentUI
