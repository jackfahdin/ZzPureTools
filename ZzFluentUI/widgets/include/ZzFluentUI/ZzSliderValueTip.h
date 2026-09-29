#pragma once

#include <memory>
#include <QtCore/QObject>
#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzSliderValueTipMode.h>

class QSlider;
class QTimerEvent;

namespace ZzFluentUI {

class ZzSliderValueTipPrivate;

/**
 * @brief 为原生 QSlider 附加调节提示，不替换其绘制、值或输入行为。
 * @note 仅在 GUI 线程使用；组件由滑块拥有，提示窗口延迟创建并复用。
 */
class ZZ_FLUENT_UI_EXPORT ZzSliderValueTip final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSliderValueTip)
    Q_PROPERTY(ZzFluentUI::ZzSliderValueTipMode mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    /**
     * @brief 安装或更新滑块的唯一提示组件。
     * @param slider 非拥有的目标滑块；空指针返回 nullptr。
     * @param mode 百分比、原始值或关闭；默认百分比。
     * @return 由 slider 拥有的组件；重复调用复用对象并更新模式。
     */
    [[nodiscard]] static ZzSliderValueTip *attach(QSlider *slider,
        ZzSliderValueTipMode mode = ZzSliderValueTipMode::Percentage);

    /** @brief 停止隐藏计时并销毁私有提示窗口，不销毁滑块。 */
    ~ZzSliderValueTip() override;

    /** @brief 返回当前调节提示模式。 */
    [[nodiscard]] ZzSliderValueTipMode mode() const noexcept;
    /** @brief 更新模式；无效枚举忽略，关闭模式立即隐藏提示。 */
    void setMode(ZzSliderValueTipMode mode);

Q_SIGNALS:
    /** @brief 模式实际变化后发出，新模式只影响展示。 */
    void modeChanged(ZzFluentUI::ZzSliderValueTipMode mode);

protected:
    /** @brief 观察原生输入和窗口环境变化，不消费滑块事件。 */
    bool eventFilter(QObject *watched, QEvent *event) override;
    /** @brief 处理键盘和滚轮调节后的单次隐藏计时。 */
    void timerEvent(QTimerEvent *event) override;

private:
    explicit ZzSliderValueTip(QSlider *slider);
    std::unique_ptr<ZzSliderValueTipPrivate> d_ptr;
};

} // namespace ZzFluentUI
