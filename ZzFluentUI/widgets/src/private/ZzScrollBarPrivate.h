#pragma once

#include <QtCore/QtTypes>

class QVariantAnimation;

namespace ZzFluentUI {

class ZzScrollBar;

/** @brief 持有滚动条唯一悬停动画和只读绘制进度。 */
class ZzScrollBarPrivate final
{
public:
    /**
     * @brief 创建并绑定唯一持久动画。
     * @param q 非空、非拥有的公开滚动条。
     */
    explicit ZzScrollBarPrivate(ZzScrollBar *q);

    /** @brief 停止动画并断开捕获私有状态的回调。 */
    ~ZzScrollBarPrivate();

    /**
     * @brief 根据目标和 style 动效偏好切换展开进度。
     * @param expanded 是否展开滑块和轨道视觉。
     */
    void setExpanded(bool expanded);

    /**
     * @brief 停止动画并同步指定终态。
     * @param expanded 终态是否展开。
     */
    void finishImmediately(bool expanded) noexcept;

    ZzScrollBar *const q_ptr;
    QVariantAnimation *const animation;
    qreal expansion = 0.0;
};

} // namespace ZzFluentUI
