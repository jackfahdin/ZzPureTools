#pragma once

#include <QtCore/QPersistentModelIndex>

class QObject;
class QVariantAnimation;

namespace ZzFluentUI {

/**
 * @brief 使用单个长期动画对象管理选中指示条的两段式过渡。
 *
 * 状态只保存两个持久索引和常量数量的比例，不读取模型总行数，也不拥有模型。
 */
class ZzSelectionIndicatorTransition final
{
public:
    /**
     * @brief 创建由指定 QObject 管理生命周期的固定动画对象。
     * @param owner 非空 QObject 所有者。
     */
    explicit ZzSelectionIndicatorTransition(QObject *owner);

    /**
     * @brief 从当前可见比例连续过渡到新索引。
     * @param target 新选中索引；无效值表示清除选择。
     * @param durationMilliseconds 非正值直接进入终态。
     */
    void transitionTo(
        const QModelIndex &target,
        int durationMilliseconds);

    /** @brief 立即完成当前过渡并保留目标终态。 */
    void finish();

    /**
     * @brief 返回指定索引当前应绘制的指示条比例。
     * @param index 当前绘制索引。
     * @param staticallySelected Qt option 是否标记为选中。
     */
    [[nodiscard]] qreal scaleFor(
        const QModelIndex &index,
        bool staticallySelected) const noexcept;

    /** @brief 返回未选中的旧索引是否仍需绘制收缩中的指示条。 */
    [[nodiscard]] bool forcesIndicator(
        const QModelIndex &index) const noexcept;

    /** @brief 返回当前收缩中的旧索引。 */
    [[nodiscard]] QModelIndex outgoingIndex() const;

    /** @brief 返回当前增长或已稳定的新索引。 */
    [[nodiscard]] QModelIndex incomingIndex() const;

    /** @brief 返回生命周期固定的非空动画对象。 */
    [[nodiscard]] QVariantAnimation *animation() const noexcept;

private:
    /** @brief 按动画归一化值更新两个指示条比例。 */
    void updateScales(qreal progress) noexcept;

    /** @brief 返回索引在状态机中的当前可见比例，不读取静态选中态。 */
    [[nodiscard]] qreal trackedScale(
        const QModelIndex &index) const noexcept;

    QVariantAnimation *animation_ = nullptr;
    QPersistentModelIndex outgoingIndex_;
    QPersistentModelIndex incomingIndex_;
    qreal outgoingStartScale_ = 0.0;
    qreal incomingStartScale_ = 0.0;
    qreal outgoingScale_ = 0.0;
    qreal incomingScale_ = 0.0;
};

} // namespace ZzFluentUI
