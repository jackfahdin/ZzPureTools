#pragma once

#include <functional>
#include <QtGui/QColor>
#include <QtWidgets/QWidget>
#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

/** @brief 以固定数量色阶提供当前色的明暗快捷选择。 */
class ZzColorShadeStrip final : public QWidget
{
public:
    /** @brief 创建支持鼠标和键盘的色阶条。 */
    explicit ZzColorShadeStrip(QWidget *parent);
    /** @brief 刷新当前颜色派生的七个色阶。 */
    void setColor(QColor color, qreal hue, qreal saturation);
    /** @brief 返回稳定色阶高度。 */
    [[nodiscard]] QSize sizeHint() const override;
    std::function<void(QColor)> selected;
protected:
    /** @brief 绘制色阶和主题焦点边框。 */
    void paintEvent(QPaintEvent *) override;
    /** @brief 点击色阶后提交对应 RGBA 值。 */
    void mousePressEvent(QMouseEvent *event) override;
    /** @brief 方向键浏览色阶、空格或 Enter 提交。 */
    void keyPressEvent(QKeyEvent *event) override;
private:
    ZzWidgetTheme theme_;
    QList<QColor> colors_;
    int focused_ = 3;
};
} // namespace ZzFluentUI
