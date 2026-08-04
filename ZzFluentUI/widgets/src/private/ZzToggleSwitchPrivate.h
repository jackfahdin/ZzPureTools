#pragma once

#include <QtCore/QtTypes>

class QVariantAnimation;

namespace ZzFluentUI {

class ZzToggleSwitch;

/** @brief 持有可复用动画和当前只读绘制进度。 */
class ZzToggleSwitchPrivate final
{
public:
    /** @brief 创建并绑定唯一的持久动画对象。 */
    explicit ZzToggleSwitchPrivate(ZzToggleSwitch *q);

    /** @brief 按当前样式动效策略移动到 checked 终态。 */
    void moveTo(bool checked);

    /** @brief 停止动画并立即同步当前 checked 终态。 */
    void finishImmediately() noexcept;

    ZzToggleSwitch *const q_ptr;
    QVariantAnimation *animation = nullptr;
    qreal progress = 0.0;
};

} // namespace ZzFluentUI
