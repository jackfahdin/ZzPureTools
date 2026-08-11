#pragma once

#include <memory>

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>

#include <ZzFluentUI/ZzItemDensity.h>

#include "ZzSelectionIndicatorTransition.h"

class QItemSelectionModel;
class QModelIndex;
class QPainter;
class QStyleOptionViewItem;
class QTreeView;

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

    /**
     * @brief 监听当前树形选择模型，并在选择变化后刷新完整视口。
     * @param treeView 非空的当前绘制树形视图。
     */
    void observeTreeSelection(QTreeView *treeView) const;

    ZzFluentItemDelegate *const q_ptr;
    ZzItemDensity density = ZzItemDensity::Standard;
    mutable QPointer<QTreeView> observedTreeView;
    mutable QPointer<QItemSelectionModel> observedSelectionModel;
    mutable QMetaObject::Connection selectionChangedConnection;
    mutable std::unique_ptr<ZzSelectionIndicatorTransition> treeTransition;

private:
    /** @brief 首次用于 Tree 时创建唯一且长期持有的动画状态。 */
    void ensureTreeTransition() const;

    /** @brief 返回树形选择中拥有强调条的有效索引。 */
    [[nodiscard]] QModelIndex selectedTreeIndex() const;

    /** @brief 按当前主题动效策略切换树形目标索引。 */
    void transitionTreeSelection() const;

    /** @brief 只刷新过渡涉及的两行。 */
    void repaintTreeTransitionRows() const;

    /** @brief 返回当前树形主题的选中动画时长。 */
    [[nodiscard]] int treeTransitionDuration() const;
};

} // namespace ZzFluentUI
