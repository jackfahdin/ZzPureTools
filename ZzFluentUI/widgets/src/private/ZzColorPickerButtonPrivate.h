#pragma once

#include <QtCore/QObject>

class QWidget;
namespace ZzFluentUI {
class ZzColorPicker;
class ZzColorPickerButton;
class ZzScrollArea;

/** @brief 拥有固定弹层装配，处理锚点切换、屏幕定位与关闭后的焦点。 */
class ZzColorPickerButtonPrivate final : public QObject
{
public:
    /** @brief 创建固定 Fluent 选择器及弹层并连接即时颜色通知。 */
    explicit ZzColorPickerButtonPrivate(ZzColorPickerButton *q);
    /** @brief 切换弹层并按锚点、方向和可用屏幕定位。 */
    void togglePopup();
    /** @brief 更新按钮与弹层的无障碍名称。 */
    void refreshText();
    /** @brief 处理原生弹层生命周期、外部点击及 Escape。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

    ZzColorPickerButton *const q_ptr;
    QWidget *const popup;
    ZzScrollArea *const scrollArea;
    ZzColorPicker *const picker;
    bool suppressAnchorRelease = false;
};
} // namespace ZzFluentUI
