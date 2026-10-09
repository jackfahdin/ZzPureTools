#pragma once

#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtWidgets/QAbstractSlider>
#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

/** @brief 私有渐变滑条，复用 Qt 数值、键盘与无障碍 slider 契约。 */
class ZzColorGradientSlider final : public QAbstractSlider
{
public:
    /** @brief 创建指定方向的固定厚度渐变滑条。 */
    explicit ZzColorGradientSlider(Qt::Orientation orientation, QWidget *parent);
    /** @brief 更新渐变色标，仅依赖变化时失效缓存。 */
    void setColors(QList<QColor> colors);
    /** @brief 返回渐变滑条的建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

protected:
    /** @brief 绘制缓存渐变、透明棋盘和主题焦点手柄。 */
    void paintEvent(QPaintEvent *) override;
    /** @brief 左键直接定位并开始跟踪。 */
    void mousePressEvent(QMouseEvent *event) override;
    /** @brief 拖动受限到范围端点。 */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** @brief 释放 Qt sliderDown 状态。 */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    /** @brief 映射逻辑坐标到数值；水平方向遵循 RTL。 */
    void editAt(QPointF point);
    ZzWidgetTheme theme_;
    QList<QColor> colors_;
    QImage cache_;
};

} // namespace ZzFluentUI
