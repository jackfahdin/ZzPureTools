#pragma once

#include <QtCore/QObject>
#include <QtCore/QRectF>
#include <QtCore/QPointer>

class QTabBar;
class QVariantAnimation;

namespace ZzFluentUI {

class ZzFluentStyle;

/** @brief 为原生标签栏保留一个可连续重定向的指示条滑动动画。 */
class ZzTabIndicatorAnimation final : public QObject
{
public:
    ZzTabIndicatorAnimation(QTabBar *bar, ZzFluentStyle *style);
    /** @brief 返回主题及几何变化后仍位于标签栏内的当前指示条。 */
    [[nodiscard]] QRectF rect();
    /** @brief 立即结束过渡，用于隐藏、主题和标签布局改变。 */
    void settle();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] QRectF targetRect() const;
    void transitionToCurrent();
    QPointer<QTabBar> bar_;
    ZzFluentStyle *style_;
    QVariantAnimation *animation_;
    QRectF current_;
};

} // namespace ZzFluentUI
