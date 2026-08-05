#pragma once

#include <memory>

#include <QtWidgets/QProgressBar>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QHideEvent;
class QPaintEvent;
class QShowEvent;

namespace ZzFluentUI {

class ZzProgressRingPrivate;

/**
 * @brief 使用 QProgressBar 范围和值语义绘制 Fluent 圆环进度。
 *
 * minimum 与 maximum 同为 0 时进入 Qt 标准不确定状态。控件必须在
 * GUI 线程创建和调用；动画只影响呈现，不改变值或业务状态。
 */
class ZZ_FLUENT_UI_EXPORT ZzProgressRing final : public QProgressBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzProgressRing)
    Q_PROPERTY(
        int ringWidth
        READ ringWidth
        WRITE setRingWidth
        NOTIFY ringWidthChanged)

public:
    /**
     * @brief 创建范围为 0 到 100、值为 0 的环形进度控件。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzProgressRing(QWidget *parent = nullptr);

    /** @brief 停止持久动画并销毁私有呈现状态。 */
    ~ZzProgressRing() override;

    /** @brief 返回设备无关逻辑像素表示的圆环线宽。 */
    [[nodiscard]] int ringWidth() const noexcept;

    /**
     * @brief 设置圆环线宽。
     * @param width 逻辑像素；收敛到 1 至 64。
     */
    void setRingWidth(int width);

    /** @brief 返回稳定的默认正方形建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

    /** @brief 返回能够呈现圆环轮廓的最小正方形尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /**
     * @brief 转发 Qt 范围设置并立即同步不确定动画状态。
     * @param minimum 最小值。
     * @param maximum 最大值；小于 minimum 时遵循 QProgressBar 收敛规则。
     */
    void setRange(int minimum, int maximum);

    /**
     * @brief 转发 Qt 最小值设置并立即同步不确定动画状态。
     * @param minimum 新最小值。
     */
    void setMinimum(int minimum);

    /**
     * @brief 转发 Qt 最大值设置并立即同步不确定动画状态。
     * @param maximum 新最大值。
     */
    void setMaximum(int maximum);

Q_SIGNALS:
    /** @brief 有效圆环线宽实际变化后发出。 */
    void ringWidthChanged(int width);

protected:
    /** @brief 使用 palette、范围和值绘制圆环和可选文本。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在样式、palette、启用状态或字体变化时同步呈现。 */
    void changeEvent(QEvent *event) override;

    /** @brief 可见后按当前范围和 style 动效偏好启动至多一条动画。 */
    void showEvent(QShowEvent *event) override;

    /** @brief 隐藏前停止动画，避免后台唤醒。 */
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzProgressRingPrivate> d_ptr;
};

} // namespace ZzFluentUI
