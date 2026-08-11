#pragma once

#include <QtCore/QRectF>

#include "ZzWidgetTheme.h"

class QPainter;
class QVariantAnimation;

namespace ZzFluentUI {

class ZzPivot;

/** @brief 持有 Pivot 固定动画、主题回退和当前指示条几何。 */
class ZzPivotPrivate final
{
public:
    /** @brief 创建固定动画并观察 QTabBar 当前项变化。 */
    explicit ZzPivotPrivate(ZzPivot *q);

    /** @brief 绘制全部可见项和当前指示条。 */
    void paint(QPainter *painter);

    /** @brief 从当前可见矩形连续过渡到目标项。 */
    void startIndicatorTransition(int index);

    /** @brief 停止动画并同步到当前项终态。 */
    void settleIndicator();

    /** @brief 重建非 Fluent 回退快照并同步终态。 */
    void refreshTheme();

    ZzPivot *const q_ptr;
    ZzWidgetTheme theme;
    QVariantAnimation *indicatorAnimation = nullptr;
    QRectF currentIndicatorRect;

private:
    /** @brief 返回指定项基于文字宽度的终态指示条矩形。 */
    [[nodiscard]] QRectF targetIndicatorRect(int index) const;

    /** @brief 返回当前主题策略调整后的 Normal 动画时长。 */
    [[nodiscard]] int transitionDuration() const;

    /** @brief 只刷新新旧指示条覆盖的局部区域。 */
    void updateIndicatorRegion(const QRectF &previous);
};

} // namespace ZzFluentUI
