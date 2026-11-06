#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>
#include <ZzFluentUI/ZzAnimatedStackedWidget.h>

namespace ZzFluentUI {

/** @brief 持有最多两张有预算上限的快照，只负责绘制与区域输入隔离。 */
class ZzStackTransitionOverlay final : public QWidget
{
public:
    using Mode = ZzAnimatedStackedWidget::ZzTransitionMode;
    /** @brief 创建无焦点的私有覆盖层。 */
    explicit ZzStackTransitionOverlay(QWidget *parent);
    /** @brief 验证两张 ARGB32 图像不超过 64 MiB，并返回物理尺寸。 */
    static QSize boundedPixelSize(QSize logical, qreal dpr);
    /** @brief 接管已通过预算检查的图像。 */
    void setFrames(QImage outgoing, QImage incoming, Mode mode, bool reverse);
    /** @brief 更新归一化绘制进度。 */
    void setProgress(qreal progress);
    /** @brief 释放全部图像存储。 */
    void clear();
    /** @brief 返回当前持有的图像字节数，用于资源预算核查。 */
    qsizetype retainedBytes() const noexcept { return outgoing_.sizeInBytes() + incoming_.sizeInBytes(); }
protected:
    /** @brief 仅合成两张固定快照，不重新渲染页面。 */
    void paintEvent(QPaintEvent *event) override;
    /** @brief 阻止覆盖区域输入穿透到真实页面。 */
    bool event(QEvent *event) override;
private:
    QImage outgoing_;
    QImage incoming_;
    Mode mode_ = Mode::None;
    qreal progress_ = 0;
    bool reverse_ = false;
};

} // namespace ZzFluentUI
