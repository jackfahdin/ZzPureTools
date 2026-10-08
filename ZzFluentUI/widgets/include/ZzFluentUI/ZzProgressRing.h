#pragma once

#include <memory>

#include <QtWidgets/QProgressBar>
#include <QtGui/QColor>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QHideEvent;
class QPaintEvent;
class QShowEvent;
class QResizeEvent;

namespace ZzFluentUI {

class ZzProgressRingPrivate;

/**
 * @brief 使用 QProgressBar 范围和值语义绘制 Fluent 圆环进度。
 *
 * minimum 与 maximum 同为 0 时进入 Qt 标准不确定状态。控件必须在
 * GUI 线程创建和调用；动画只影响呈现，不改变值或业务状态。
 * 默认数值字号随尺寸调整；显式 setFont() 后使用指定字体，
 * setFont(QFont()) 恢复自动字号。环宽与旋转速度可分别配置。
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
    Q_PROPERTY(int indeterminateDuration READ indeterminateDuration
        WRITE setIndeterminateDuration NOTIFY indeterminateDurationChanged)
    Q_PROPERTY(qreal thickness READ thickness WRITE setThickness NOTIFY thicknessChanged)
    Q_PROPERTY(QColor ringColor READ ringColor WRITE setRingColor NOTIFY ringColorChanged)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY trackColorChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QFont titleFont READ titleFont WRITE setTitleFont NOTIFY titleFontChanged)
    Q_PROPERTY(QFont valueFont READ valueFont WRITE setValueFont NOTIFY valueFontChanged)
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor NOTIFY titleColorChanged)
    Q_PROPERTY(QColor valueColor READ valueColor WRITE setValueColor NOTIFY valueColorChanged)
    Q_PROPERTY(int textSpacing READ textSpacing WRITE setTextSpacing NOTIFY textSpacingChanged)
    Q_PROPERTY(QWidget* centerWidget READ centerWidget WRITE setCenterWidget NOTIFY centerWidgetChanged)
    Q_PROPERTY(bool textVisible READ isTextVisible WRITE setTextVisible)

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

    /** @brief 逻辑像素厚度，支持小数；收敛至 [1,64]，忽略非有限值。 */
    [[nodiscard]] qreal thickness() const noexcept;
    void setThickness(qreal thickness);
    /** @brief 无效颜色恢复 palette 的 Accent / Mid。 */
    [[nodiscard]] QColor ringColor() const;
    void setRingColor(QColor color);
    [[nodiscard]] QColor trackColor() const;
    void setTrackColor(QColor color);
    [[nodiscard]] QString title() const;
    void setTitle(QString title);
    /** @brief 默认 QFont() 使用尺寸适配字体；显式 setFont() 仍有效。 */
    [[nodiscard]] QFont titleFont() const;
    void setTitleFont(QFont font);
    [[nodiscard]] QFont valueFont() const;
    void setValueFont(QFont font);
    /** @brief 无效颜色恢复 palette 文本色，标题默认降低不透明度。 */
    [[nodiscard]] QColor titleColor() const;
    void setTitleColor(QColor color);
    [[nodiscard]] QColor valueColor() const;
    void setValueColor(QColor color);
    [[nodiscard]] int textSpacing() const noexcept;
    void setTextSpacing(int spacing);
    [[nodiscard]] QWidget *centerWidget() const noexcept;
    /** @brief 接管 widget 并删除旧中心；拒绝自身、祖先及旧中心后代。 */
    void setCenterWidget(QWidget *widget);
    /** @brief 隐藏、解除父对象并返回中心控件；调用方接管所有权。 */
    [[nodiscard]] QWidget *takeCenterWidget();
    /** @brief 同时同步内置文字和中心控件的可见性。 */
    void setTextVisible(bool visible);

    /** @brief 返回忙碌圆弧旋转一周的毫秒数，默认 800。 */
    [[nodiscard]] int indeterminateDuration() const noexcept;

    /**
     * @brief 设置忙碌圆弧旋转周期，运行中切换保留当前角度。
     * @param milliseconds 周期毫秒数，收敛到 200 至 60000。
     * @note 复用同一动画；隐藏、禁用、减少动效或确定进度时不启动动画。
     */
    void setIndeterminateDuration(int milliseconds);

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

    /** @brief 有效旋转周期实际改变后发出，单位毫秒。 */
    void indeterminateDurationChanged(int milliseconds);
    void thicknessChanged(qreal thickness);
    void ringColorChanged(QColor color);
    void trackColorChanged(QColor color);
    void titleChanged(QString title);
    void titleFontChanged(QFont font);
    void valueFontChanged(QFont font);
    void titleColorChanged(QColor color);
    void valueColorChanged(QColor color);
    void textSpacingChanged(int spacing);
    void centerWidgetChanged(QWidget *widget);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    /** @brief 使用 palette、范围和值绘制圆环和可选文本。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在样式、palette、启用状态或字体变化时同步呈现。 */
    void changeEvent(QEvent *event) override;

    /** @brief 可见后按当前范围和 style 动效偏好启动至多一条动画。 */
    void showEvent(QShowEvent *event) override;

    /** @brief 隐藏前停止动画，避免后台唤醒。 */
    void hideEvent(QHideEvent *event) override;

private:
    [[nodiscard]] QRectF centerContentRect() const;
    void updateCenterWidgetGeometry();
    void releaseCenterWidget();
    std::unique_ptr<ZzProgressRingPrivate> d_ptr;
};

} // namespace ZzFluentUI
