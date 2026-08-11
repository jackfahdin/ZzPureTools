#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QModelIndex>
#include <QtCore/QPointer>

#include "ZzSelectionIndicatorTransition.h"

class QItemSelectionModel;

namespace ZzFluentUI {

class ZzNavigationView;
class ZzNavigationItemDelegate;

/** @brief 保存紧凑状态并统一校验模型激活意图。 */
class ZzNavigationViewPrivate final
{
public:
    /** @brief 绑定非空导航视图。 */
    explicit ZzNavigationViewPrivate(
        ZzNavigationView *publicObject) noexcept;

    /** @brief 仅对有效且启用的索引发出导航意图。 */
    void activateIndex(const QModelIndex &index);

    /** @brief 同步 private delegate 的固定行高和紧凑绘制。 */
    void setCompactPresentation(bool compact);

    /** @brief 迁移当前 selection model 连接并同步静态选中终态。 */
    void bindSelectionModel();

    /** @brief 返回指定索引当前指示条比例。 */
    [[nodiscard]] qreal indicatorScale(
        const QModelIndex &index,
        bool staticallySelected) const noexcept;

    /** @brief 返回旧索引是否仍需绘制收缩中的指示条。 */
    [[nodiscard]] bool forcesIndicator(
        const QModelIndex &index) const noexcept;

    /** @brief reduced motion 或样式变化时立即完成当前过渡。 */
    void finishTransition();

    ZzNavigationView *const q_ptr;
    ZzNavigationItemDelegate *delegate = nullptr;
    ZzSelectionIndicatorTransition transition;
    QPointer<QItemSelectionModel> observedSelectionModel;
    QMetaObject::Connection selectionChangedConnection;
    QMetaObject::Connection modelResetConnection;
    bool compact = false;

private:
    /** @brief 返回当前单选模型中的有效导航索引。 */
    [[nodiscard]] QModelIndex selectedIndex() const;

    /** @brief 使用当前主题动效策略切换目标索引。 */
    void transitionToSelection();

    /** @brief 只刷新过渡涉及的两个可见 item。 */
    void repaintTransitionIndexes() const;

    /** @brief 返回当前主题下的选中动画时长。 */
    [[nodiscard]] int transitionDuration() const;
};

} // namespace ZzFluentUI
