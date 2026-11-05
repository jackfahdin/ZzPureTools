#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QCheckBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QHideEvent;

namespace ZzFluentUI {

class ZzToggleSwitchPrivate;

/**
 * @brief 保留 QCheckBox 语义的 Fluent 开关。
 *
 * 控件必须在 GUI 线程创建和调用；键盘、焦点和无障碍检查状态
 * 由 QCheckBox 提供，动画只影响呈现。
 * 旋钮拖动在释放时提交；失焦、隐藏、禁用等中断只取消手势，
 * 不发出 released/clicked。正常释放保留按钮组通知与互斥规则。
 */
class ZZ_FLUENT_UI_EXPORT ZzToggleSwitch final : public QCheckBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzToggleSwitch)

public:
    /**
     * @brief 创建无文本的未选中开关。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzToggleSwitch(QWidget *parent = nullptr);

    /**
     * @brief 创建带展示文本的未选中开关。
     * @param text 可本地化的展示文本。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzToggleSwitch(
        const QString &text,
        QWidget *parent = nullptr);

    /** @brief 销毁私有状态，动画由 QObject parent 自动释放。 */
    ~ZzToggleSwitch() override;

    /**
     * @brief 返回轨道、文本和 contents margins 所需逻辑尺寸。
     * @return 不随 viewport 缩放的设备无关尺寸。
     */
    [[nodiscard]] QSize sizeHint() const override;

protected:
    /** @brief 丢失鼠标抓取或窗口失活时取消未提交的拖动。 */
    bool event(QEvent *event) override;
    /** @brief 旋钮左键按下开始候选手势，其他区域沿用复选框点击。 */
    void mousePressEvent(QMouseEvent *event) override;
    /** @brief 越过阈值后直接更新旋钮呈现，不提前改变 checked。 */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** @brief 拖动释放按中点提交一次状态并发出标准按钮信号。 */
    void mouseReleaseEvent(QMouseEvent *event) override;
    /** @brief 拖动指定终态，普通点击保留 QCheckBox 的状态循环。 */
    void nextCheckState() override;

    /**
     * @brief 判断控件完整视觉区域是否命中，而非使用平台复选框小区域。
     * @param position 控件局部坐标中的指针位置。
     * @return 位置位于控件矩形内时返回 true。
     */
    [[nodiscard]] bool hitButton(const QPoint &position) const override;

    /** @brief 绘制轨道、旋钮、文本和统一焦点环。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 同步影响呈现或几何的 Qt 状态变化。 */
    void changeEvent(QEvent *event) override;

    /** @brief 隐藏前停止动画并同步 checked 终态。 */
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzToggleSwitchPrivate> d_ptr;
};

} // namespace ZzFluentUI
