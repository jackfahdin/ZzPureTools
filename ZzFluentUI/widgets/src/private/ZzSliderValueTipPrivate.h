#pragma once

#include <QtCore/QBasicTimer>
#include <QtCore/QList>
#include <QtCore/QPointer>
#include <ZzFluentUI/ZzSliderValueTipMode.h>

class QEvent;
class QLabel;
class QSlider;
class QWidget;

namespace ZzFluentUI {

class ZzSliderValueTip;

/** @brief 管理滑块提示的输入观察、单次计时、跟随定位及延迟创建。 */
class ZzSliderValueTipPrivate final
{
public:
    /** @brief 绑定非拥有滑块，连接原生调节信号。 */
    ZzSliderValueTipPrivate(ZzSliderValueTip *q, QSlider *slider);
    /** @brief 撤销临时观察与提示窗口，保持滑块所有权。 */
    ~ZzSliderValueTipPrivate();
    /** @brief 显示提示；continuous 表示鼠标持续操作，不启动隐藏计时。 */
    void show(bool continuous);
    /** @brief 刷新已显示提示的数值、主题与屏幕内位置。 */
    void refresh();
    /** @brief 隐藏提示，停止计时并撤销祖先观察。 */
    void hide();
    /** @brief 只观察调节与环境事件，不转发或合成原生输入。 */
    void handleEvent(QObject *watched, QEvent *event);
    /** @brief 更新提示外观和滑柄锚点，百分比使用 64 位中间值。 */
    void updateTip();

    ZzSliderValueTip *const q_ptr;
    QPointer<QSlider> slider;
    QPointer<QLabel> tip;
    QList<QPointer<QWidget>> ancestors;
    QBasicTimer hideTimer;
    ZzSliderValueTipMode mode = ZzSliderValueTipMode::Percentage;
};

} // namespace ZzFluentUI
