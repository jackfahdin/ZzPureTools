#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QPoint>
#include <QtCore/QString>

#include <ZzFluentUI/ZzTeachingTipPlacement.h>

#include "ZzWidgetTheme.h"

class QHBoxLayout;
class QLabel;
class QParallelAnimationGroup;
class QPropertyAnimation;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace ZzFluentUI {

class ZzIconButton;
class ZzPushButton;
class ZzTeachingTip;

/** @brief 保存教学提示布局、定位、目标监听和固定动画对象。 */
class ZzTeachingTipPrivate final : public QObject
{
    friend class ZzTeachingTip;

public:
    /** @brief 创建固定子控件树、两条属性动画和一次性信号连接。 */
    explicit ZzTeachingTipPrivate(ZzTeachingTip *q);

    /** @brief 停止动画并注销全部目标与应用事件过滤器。 */
    ~ZzTeachingTipPrivate() override;

    /** @brief 同步文本、按钮、字体、尺寸、图标和无障碍内容。 */
    void refreshPresentation();

    /** @brief 刷新非 Fluent 样式回退主题并重新定位可见提示。 */
    void refreshTheme();

    /** @brief 接管新内容并按契约删除被替换内容。 */
    void setContentWidget(QWidget *widget);

    /** @brief 解除当前内容 parent 并交回调用者。 */
    [[nodiscard]] QWidget *takeContentWidget();

    /** @brief 替换非拥有目标并更新几何和生命周期监听。 */
    void setTargetWidget(QWidget *target);

    /** @brief 校验目标、定位并启动或直接完成显示过渡。 */
    void showForTarget();

    /** @brief 启动或直接完成一次幂等关闭过渡。 */
    void dismiss();

    /** @brief 根据首选顺序和屏幕可用区域计算最终方向与位置。 */
    [[nodiscard]] bool reposition();

    /** @brief 根据可见性和策略注册应用级轻量关闭过滤器。 */
    void syncApplicationEventFilter();

    /** @brief 隐藏时注销应用过滤器并完成一次 dismissed 信号。 */
    void handleHidden();

    /** @brief 监听目标生命周期和应用级外部鼠标点击。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

    ZzTeachingTip *const q_ptr;
    ZzWidgetTheme theme;
    QWidget *headerHost = nullptr;
    QHBoxLayout *headerLayout = nullptr;
    QLabel *titleLabel = nullptr;
    ZzIconButton *closeButton = nullptr;
    QLabel *textLabel = nullptr;
    QWidget *contentHost = nullptr;
    QVBoxLayout *contentLayout = nullptr;
    ZzPushButton *actionButton = nullptr;
    QPointer<QWidget> contentWidget;
    QMetaObject::Connection contentDestroyedConnection;
    QPointer<QWidget> targetWidget;
    QPointer<QWidget> targetWindow;
    QMetaObject::Connection targetDestroyedConnection;
    QParallelAnimationGroup *animationGroup = nullptr;
    QPropertyAnimation *opacityAnimation = nullptr;
    QPropertyAnimation *positionAnimation = nullptr;
    QMetaObject::Connection animationFinishedConnection;
    QString title;
    QString text;
    QString actionText;
    QString generatedAccessibleName;
    ZzTeachingTipPlacement preferredPlacement = ZzTeachingTipPlacement::Auto;
    ZzTeachingTipPlacement effectivePlacement = ZzTeachingTipPlacement::Auto;
    QPoint finalPosition;
    qreal arrowCenter = 0.0;
    bool lightDismissEnabled = true;
    bool actionEnabled = true;
    bool actionVisible = false;
    bool closeButtonVisible = true;
    bool applicationFilterInstalled = false;
    bool dismissing = false;
    bool dismissSignalPending = false;

private:
    /** @brief 更新目标所属顶层窗口的屏幕与可见性监听。 */
    void refreshTargetWindowFilter();

    /** @brief 返回当前动效策略处理后的 Fast 时长。 */
    [[nodiscard]] int animationDuration() const noexcept;

    /** @brief 返回朝向目标的四逻辑像素偏移。 */
    [[nodiscard]] QPoint targetDisplacement() const noexcept;

    /** @brief 配置并启动固定动画对象。 */
    void startAnimation(
        qreal startOpacity,
        qreal endOpacity,
        const QPoint &startPosition,
        const QPoint &endPosition,
        int duration);

    /** @brief 处理固定动画组完成事件。 */
    void finishAnimation();

    /** @brief 更新实际方向并只在变化时发信号。 */
    void setEffectivePlacement(ZzTeachingTipPlacement placement);
};

} // namespace ZzFluentUI
