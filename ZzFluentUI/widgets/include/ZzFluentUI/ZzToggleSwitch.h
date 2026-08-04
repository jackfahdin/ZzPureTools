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
