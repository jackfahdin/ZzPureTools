#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include "ZzWidgetTheme.h"

class QToolButton;
class QVariantAnimation;
class QVBoxLayout;
class QWidget;

namespace ZzFluentUI {

class ZzExpander;

/** @brief 持有 Expander 固定子对象、内容所有权和单动画状态。 */
class ZzExpanderPrivate final
{
public:
    /** @brief 创建固定 header、内容宿主和动画对象。 */
    explicit ZzExpanderPrivate(ZzExpander *q);

    /** @brief 断开内容销毁观察，避免 public 基类析构期晚回调。 */
    ~ZzExpanderPrivate();

    /** @brief 按当前主题、语言、方向和状态刷新 header 展示。 */
    void refreshPresentation();

    /** @brief 重建非 Fluent 回退快照并刷新展示。 */
    void refreshTheme();

    /** @brief 接管新内容并删除被替换的旧内容。 */
    void setContentWidget(QWidget *widget);

    /** @brief 取回内容并解除 parent。 */
    [[nodiscard]] QWidget *takeContentWidget();

    /** @brief 从当前可见高度切换到逻辑展开状态。 */
    void startTransition();

    /** @brief 内容 sizeHint 变化时更新运行中的展开终点。 */
    void retargetExpandedHeight();

    ZzExpander *const q_ptr;
    ZzWidgetTheme theme;
    QToolButton *headerButton = nullptr;
    QWidget *contentHost = nullptr;
    QVBoxLayout *contentLayout = nullptr;
    QVariantAnimation *heightAnimation = nullptr;
    QPointer<QWidget> contentWidget;
    QMetaObject::Connection contentDestroyedConnection;
    QString headerText;
    bool expanded = false;

private:
    /** @brief 返回包含主题 padding 的内容目标高度。 */
    [[nodiscard]] int expandedContentHeight() const;

    /** @brief 返回当前主题策略调整后的 Normal 动画时长。 */
    [[nodiscard]] int transitionDuration() const;

    /** @brief 同步进入当前逻辑状态的稳定几何。 */
    void settleTransition();

    /** @brief 折叠前把内容子树焦点移回 header。 */
    void restoreHeaderFocusIfNeeded();
};

} // namespace ZzFluentUI
