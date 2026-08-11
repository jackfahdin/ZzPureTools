#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <ZzFluentUI/ZzContentDialogButton.h>
#include <ZzFluentUI/ZzContentDialogResult.h>

#include "ZzWidgetTheme.h"

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace ZzFluentUI {

class ZzContentDialog;
class ZzPushButton;

/** @brief 保存内容对话框展示状态、子控件和实例级遮罩生命周期。 */
class ZzContentDialogPrivate final : public QObject
{
    friend class ZzContentDialog;

public:
    /** @brief 创建固定子控件树并绑定三个结果按钮。 */
    explicit ZzContentDialogPrivate(ZzContentDialog *q);

    /** @brief 清理仍存在的遮罩并解除宿主事件过滤。 */
    ~ZzContentDialogPrivate() override;

    /** @brief 同步标题、正文、按钮、字体、尺寸与无障碍文本。 */
    void refreshPresentation();

    /** @brief 仅在样式或调色板变化时重建回退主题。 */
    void refreshTheme();

    /** @brief 让对话框接管新内容，并按契约删除被替换内容。 */
    void setContentWidget(QWidget *widget);

    /** @brief 解除当前内容 parent 并把所有权交回调用者。 */
    [[nodiscard]] QWidget *takeContentWidget();

    /** @brief 返回当前默认按钮；不可触发时返回空。 */
    [[nodiscard]] ZzPushButton *activeDefaultButton() const noexcept;

    /** @brief 单击当前有效默认按钮；None、隐藏或禁用时不操作。 */
    void triggerDefaultButton();

    /** @brief 开始一次显示周期，保存焦点并按模态状态创建遮罩。 */
    void beginPresentation();

    /** @brief 结束显示周期，移除遮罩并恢复先前焦点。 */
    void endPresentation();

    /** @brief 更新公开结果并保证每个实际变化只发一次信号。 */
    void setDialogResult(ZzContentDialogResult result);

    /** @brief 绘制遮罩并跟随所属父窗口内容区尺寸。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

    ZzContentDialog *const q_ptr;
    ZzWidgetTheme theme;
    QLabel *titleLabel = nullptr;
    QLabel *textLabel = nullptr;
    QWidget *contentHost = nullptr;
    QVBoxLayout *contentLayout = nullptr;
    QWidget *buttonHost = nullptr;
    QHBoxLayout *buttonLayout = nullptr;
    ZzPushButton *primaryButton = nullptr;
    ZzPushButton *secondaryButton = nullptr;
    ZzPushButton *closeButton = nullptr;
    QPointer<QWidget> contentWidget;
    QMetaObject::Connection contentDestroyedConnection;
    QPointer<QWidget> overlay;
    QPointer<QWidget> overlayHost;
    QPointer<QWidget> previousFocus;
    QString title;
    QString text;
    QString primaryButtonText;
    QString secondaryButtonText;
    QString closeButtonText;
    QString generatedAccessibleName;
    bool primaryButtonVisible = false;
    bool primaryButtonEnabled = true;
    bool secondaryButtonVisible = false;
    bool secondaryButtonEnabled = true;
    bool closeButtonVisible = true;
    bool closeButtonEnabled = true;
    bool primaryButtonTextCustomized = false;
    bool secondaryButtonTextCustomized = false;
    bool closeButtonTextCustomized = false;
    ZzContentDialogButton defaultButton = ZzContentDialogButton::None;
    ZzContentDialogResult dialogResult = ZzContentDialogResult::None;

private:
    /** @brief 更新仍使用默认值的可翻译按钮文本。 */
    void refreshDefaultButtonTexts(bool notify);

    /** @brief 同步按钮文本、可见性、启用状态和默认标记。 */
    void refreshButtons();

    /** @brief 为当前模态显示创建仅覆盖所属窗口的主题遮罩。 */
    void ensureOverlay();

    /** @brief 立即删除实例遮罩并解除事件过滤。 */
    void removeOverlay();

    /** @brief 返回非拥有的所属顶层父窗口。 */
    [[nodiscard]] QWidget *modalHost() const noexcept;
};

} // namespace ZzFluentUI
