#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QList>

#include "ZzSelectionIndicatorTransition.h"

class QAbstractItemView;
class QItemSelectionModel;

namespace ZzFluentUI {

class ZzFluentStyle;

/**
 * @brief 为单个视图观察选择并持有一个长期动画，不改变选择模型。
 *
 * 由样式 QObject 拥有；视图销毁时同步回收。仅单一行或单元格之间过渡，
 * 多选直接进入静态绘制状态。索引按行归一化，独立于表格视觉列次序。
 */
class ZzItemSelectionAnimation final : public QObject
{
public:
    ZzItemSelectionAnimation(QAbstractItemView *view, ZzFluentStyle *style);
    /** @brief 绘制前迁移被替换的 selectionModel，并应用 reduced motion。 */
    void synchronize();
    /** @brief 返回当前 index 的长轴比例及旧条绘制责任。 */
    [[nodiscard]] qreal scaleFor(const QModelIndex &index, bool selected) const;
    [[nodiscard]] bool forcesIndicator(const QModelIndex &index) const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] QModelIndex key(const QModelIndex &index) const;
    [[nodiscard]] QModelIndex singleSelectedIndex() const;
    void selectionChanged();
    void settle();
    void repaintRows();
    QPointer<QAbstractItemView> view_;
    ZzFluentStyle *style_;
    QPointer<QItemSelectionModel> selection_;
    QList<QMetaObject::Connection> connections_;
    bool multiple_ = false;
    ZzSelectionIndicatorTransition transition_;
};

} // namespace ZzFluentUI
