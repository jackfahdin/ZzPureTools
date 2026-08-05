#pragma once

#include <QtCore/QRect>
#include <QtCore/QString>

class QPainter;
class QStyleOptionButton;

namespace ZzFluentUI {

class ZzActionCard;

/** @brief 保存一次操作卡布局中不会跨帧持有的逻辑矩形。 */
struct ZzActionCardLayout final
{
    QRect iconRect;
    QRect titleRect;
    QRect descriptionRect;
    QRect indicatorRect;
};

/** @brief 持有操作卡展示状态并执行常数复杂度布局与绘制。 */
class ZzActionCardPrivate final
{
public:
    /** @brief 绑定公开对象，不取得 QObject 所有权。 */
    explicit ZzActionCardPrivate(ZzActionCard *q) noexcept;

    /** @brief 初始化只绘制 surface 的按钮 style option。 */
    void initStyleOption(QStyleOptionButton *option) const;

    /** @brief 计算支持 LTR/RTL 的图标、文字和指示器矩形。 */
    [[nodiscard]] ZzActionCardLayout layout() const;

    /** @brief 绘制 surface、图标、文字和尾部指示器。 */
    void paint(QPainter *painter) const;

    ZzActionCard *const q_ptr;
    QString description;
    bool trailingIndicatorVisible = true;
};

} // namespace ZzFluentUI
