#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QRectF>

#include <ZzFluentUI/ZzDrawerEdge.h>

#include "ZzWidgetTheme.h"

class QVariantAnimation;
class QVBoxLayout;
class QWidget;

namespace ZzFluentUI {

class ZzDrawer;

/** @brief 持有 Drawer 固定面板、动画、事件过滤和内容所有权。 */
class ZzDrawerPrivate final : public QObject
{
public:
    /** @brief 创建固定面板、布局与单个进度动画。 */
    explicit ZzDrawerPrivate(ZzDrawer *q);

    /** @brief 停止动画并移除宿主和应用事件过滤器。 */
    ~ZzDrawerPrivate() override;

    /** @brief 刷新主题尺寸、无障碍名称、面板几何和输入 mask。 */
    void refreshPresentation();

    /** @brief 重建非 Fluent 回退主题并刷新展示。 */
    void refreshTheme();

    /** @brief 在 ParentChange 后迁移固定宿主事件过滤器。 */
    void updateHostBinding();

    /** @brief 接管新内容并删除被替换内容。 */
    void setContentWidget(QWidget *widget);

    /** @brief 取回内容并解除 parent。 */
    [[nodiscard]] QWidget *takeContentWidget();

    /** @brief 从当前进度打开并把焦点移入面板。 */
    void openDrawer();

    /** @brief 从当前进度关闭并在完成后恢复焦点。 */
    void closeDrawer();

    /** @brief 外部隐藏时同步逻辑状态和焦点。 */
    void handleExternalHide();

    /** @brief 返回当前逻辑坐标中的浮点面板矩形。 */
    [[nodiscard]] QRectF panelRect() const;

    /** @brief 监听宿主几何、层级和打开期间的全局键盘输入。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

    ZzDrawer *const q_ptr;
    ZzWidgetTheme theme;
    QWidget *panelHost = nullptr;
    QVBoxLayout *contentLayout = nullptr;
    QVariantAnimation *progressAnimation = nullptr;
    QPointer<QWidget> contentWidget;
    QMetaObject::Connection contentDestroyedConnection;
    QPointer<QWidget> observedHost;
    QPointer<QWidget> previousFocus;
    QString generatedAccessibleName;
    ZzDrawerEdge edge = ZzDrawerEdge::Left;
    qreal progress = 0.0;
    int widthHint = 0;
    bool modal = true;
    bool open = false;
    bool applicationFilterInstalled = false;
    bool hidingInternally = false;

private:
    /** @brief 返回受主题默认值和宿主宽度约束的实际面板宽度。 */
    [[nodiscard]] int panelWidth() const;

    /** @brief 返回当前主题策略调整后的 Normal 动画时长。 */
    [[nodiscard]] int transitionDuration() const;

    /** @brief 设置归一化进度并只刷新相关几何和绘制区域。 */
    void setProgress(qreal value);

    /** @brief 按剩余距离启动固定动画或直接同步终态。 */
    void startTransition(qreal target);

    /** @brief 完成当前逻辑终态，必要时隐藏并恢复焦点。 */
    void finishTransition();

    /** @brief 跟随宿主内容区并更新 panel geometry 与非模态 mask。 */
    void updateGeometryAndMask();

    /** @brief 按打开和模态状态安装或移除应用事件过滤器。 */
    void syncApplicationEventFilter();

    /** @brief 返回面板内按 Qt TabFocus 顺序排列的可聚焦控件。 */
    [[nodiscard]] QList<QWidget *> focusableWidgets() const;

    /** @brief 把焦点移入首个内容控件，无候选时聚焦 Drawer。 */
    void focusFirstContent();

    /** @brief 在面板内循环处理 Tab 或 Backtab。 */
    void cycleFocus(bool backwards);

    /** @brief 关闭完成后恢复打开前仍有效的可见焦点。 */
    void restorePreviousFocus();
};

} // namespace ZzFluentUI
