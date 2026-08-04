#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;

namespace ZzFluentUI {

class ZzFluentTitleBarPrivate;

/**
 * @brief 只负责标题栏视觉、状态和窗口意图的 QWidget。
 *
 * 控件不调用平台 API、窗口命令或无边框适配层；宿主负责执行意图。
 */
class ZZ_FLUENT_UI_EXPORT ZzFluentTitleBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFluentTitleBar)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

public:
    /**
     * @brief 创建带图标、标题和三个系统按钮的纯视觉标题栏。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzFluentTitleBar(QWidget *parent = nullptr);

    /** @brief 销毁私有状态，所有 chrome 子控件由 QObject parent 释放。 */
    ~ZzFluentTitleBar() override;

    /** @brief 返回当前展示标题。 */
    [[nodiscard]] QString title() const;

    /**
     * @brief 更新展示标题并在实际变化时发信号。
     * @param title 可本地化的窗口标题。
     */
    void setTitle(QString title);

    /**
     * @brief 更新纯视觉窗口图标。
     * @param icon 隐式共享的 Qt 图标值。
     */
    void setWindowIcon(const QIcon &icon);

    /**
     * @brief 更新最大化按钮的图标和可访问名称。
     * @param maximized 为 true 时展示还原意图。
     */
    void setMaximized(bool maximized);

    /**
     * @brief 统一显示或隐藏三个系统按钮。
     * @param visible 外层平台策略决定的可见性。
     */
    void setSystemButtonsVisible(bool visible);

    /** @brief 返回非拥有的窗口图标子控件。 */
    [[nodiscard]] QWidget *windowIconWidget() const noexcept;

    /** @brief 返回非拥有的最小化按钮。 */
    [[nodiscard]] QWidget *minimizeButton() const noexcept;

    /** @brief 返回非拥有的最大化/还原按钮。 */
    [[nodiscard]] QWidget *maximizeButton() const noexcept;

    /** @brief 返回非拥有的关闭按钮。 */
    [[nodiscard]] QWidget *closeButton() const noexcept;

    /**
     * @brief 返回需由无边框适配层排除拖动的交互子控件。
     * @return 三个非拥有按钮指针的按值列表。
     */
    [[nodiscard]] QList<QWidget *> interactiveWidgets() const;

Q_SIGNALS:
    /** @brief 展示标题实际变化后发出。 */
    void titleChanged(const QString &title);

    /** @brief 用户请求最小化窗口；控件不执行窗口命令。 */
    void minimizeRequested();

    /** @brief 用户请求最大化或还原窗口；控件不执行窗口命令。 */
    void maximizeRestoreRequested();

    /** @brief 用户请求关闭窗口；控件不会关闭或删除宿主。 */
    void closeRequested();

protected:
    /** @brief 语言、样式、调色板或 DPR 变化时刷新 chrome 展示。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzFluentTitleBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
