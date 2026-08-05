#pragma once

#include <QtCore/qglobal.h>

class QVariantAnimation;

namespace ZzFluentUI {

class ZzProgressRing;

/** @brief 持有环形进度的单一持久动画和纯呈现状态。 */
class ZzProgressRingPrivate final
{
public:
    /** @brief 绑定公开控件并创建一次线性循环动画。 */
    explicit ZzProgressRingPrivate(ZzProgressRing *q);

    /** @brief 停止动画并断开所有捕获私有状态的回调。 */
    ~ZzProgressRingPrivate();

    /** @brief 根据可见性、启用状态、范围与 style 偏好同步动画。 */
    void syncAnimation();

    /** @brief 停止动画并把相位复位到确定的静态起点。 */
    void stopAnimation() noexcept;

    /** @brief 返回 minimum/maximum 是否表达 Qt 不确定进度。 */
    [[nodiscard]] bool isIndeterminate() const noexcept;

    ZzProgressRing *const q_ptr;
    QVariantAnimation *const animation;
    qreal phase = 0.0;
    int ringWidth = 4;
};

} // namespace ZzFluentUI
