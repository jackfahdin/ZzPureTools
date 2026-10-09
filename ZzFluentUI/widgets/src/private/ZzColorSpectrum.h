#pragma once

#include <functional>
#include <QtGui/QImage>
#include <QtWidgets/QWidget>

#include "ZzWidgetTheme.h"

namespace ZzFluentUI {

/** @brief 私有、可键盘编辑的 Hue×Saturation 方形和圆形色谱。 */
class ZzColorSpectrum final : public QWidget
{
public:
    /** @brief 创建持有独立栅格缓存的色谱。 */
    explicit ZzColorSpectrum(QWidget *parent);
    /** @brief 更新选点和形状；明度不参与色谱内容。 */
    void setState(qreal hue, qreal saturation, bool ring);
    /** @brief 返回参考色谱的建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;
    std::function<void(qreal, qreal)> edited;

protected:
    /** @brief 按尺寸、DPR 和形状复用色谱缓存并绘制焦点。 */
    void paintEvent(QPaintEvent *) override;
    /** @brief 左键开始一次色谱拖动。 */
    void mousePressEvent(QMouseEvent *event) override;
    /** @brief 把拖动限制在有效色谱边界。 */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** @brief 方向键编辑 hue/saturation，遵循 RTL。 */
    void keyPressEvent(QKeyEvent *event) override;

private:
    /** @brief 将逻辑坐标映射到范围受限的 HSV 分量。 */
    void editAt(QPointF position);
    ZzWidgetTheme theme_;
    QImage cache_;
    bool ring_ = false;
    bool cachedRing_ = false;
    qreal hue_ = 0;
    qreal saturation_ = 1;
};

} // namespace ZzFluentUI
