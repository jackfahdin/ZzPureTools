#pragma once

#include <memory>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtGui/QIcon>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzTitleBarMenuDisplayMode.h>

class QEvent;
class QMenuBar;
class QObject;
class QResizeEvent;

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
    Q_PROPERTY(
        ZzFluentUI::ZzTitleBarMenuDisplayMode menuDisplayMode
        READ menuDisplayMode
        WRITE setMenuDisplayMode
        NOTIFY menuDisplayModeChanged)
    Q_PROPERTY(
        bool menuCollapseEnabled
        READ isMenuCollapseEnabled
        WRITE setMenuCollapseEnabled
        NOTIFY menuCollapseEnabledChanged)
    Q_PROPERTY(
        ZzFluentUI::ZzThemeMode themeMode
        READ themeMode
        WRITE setThemeMode
        NOTIFY themeModeChanged)
    Q_PROPERTY(
        bool alwaysOnTop
        READ isAlwaysOnTop
        WRITE setAlwaysOnTop
        NOTIFY alwaysOnTopChanged)

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

    /**
     * @brief 返回由标题栏拥有的非原生菜单栏。
     * @return 非拥有观察指针；调用方可使用标准 QMenuBar API 添加菜单。
     */
    [[nodiscard]] QMenuBar *menuBar() const noexcept;

    /** @brief 返回当前请求的菜单展示策略。 */
    [[nodiscard]] ZzTitleBarMenuDisplayMode menuDisplayMode() const noexcept;

    /**
     * @brief 设置菜单展示策略，并立即重新计算稳定子控件的可见性。
     * @param mode 新展示策略。
     */
    void setMenuDisplayMode(ZzTitleBarMenuDisplayMode mode);

    /** @brief 返回是否允许自适应菜单折叠。 */
    [[nodiscard]] bool isMenuCollapseEnabled() const noexcept;

    /**
     * @brief 设置是否允许自适应菜单折叠。
     *
     * 关闭时强制显示完整菜单，并将宿主顶层窗口的最小宽度提升到
     * 完整菜单所需值；重新开启时恢复宿主此前确认的最小宽度。
     * @param enabled 为 true 时允许 Adaptive 模式按宽度折叠。
     */
    void setMenuCollapseEnabled(bool enabled);

    /**
     * @brief 返回当前菜单、标题和窗口按钮完整展示所需的最小宽度。
     * @return 逻辑像素宽度，不包含自适应迟滞余量。
     */
    [[nodiscard]] int minimumExpandedWidth() const noexcept;

    /** @brief 返回应用最后确认的主题模式。 */
    [[nodiscard]] ZzThemeMode themeMode() const noexcept;

    /**
     * @brief 同步应用已经确认的主题模式，不执行主题切换。
     * @param mode 已生效的主题模式。
     */
    void setThemeMode(ZzThemeMode mode);

    /** @brief 返回宿主最后确认的置顶状态。 */
    [[nodiscard]] bool isAlwaysOnTop() const noexcept;

    /**
     * @brief 同步宿主已经确认的置顶状态，不修改窗口标志。
     * @param alwaysOnTop 已生效的置顶状态。
     */
    void setAlwaysOnTop(bool alwaysOnTop);

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

    /**
     * @brief 返回需要从无边框拖动命中区排除的稳定非系统控件。
     * @return 菜单栏、折叠菜单、主题和置顶控件的非拥有指针列表。
     *
     * 列表同时包含当前隐藏的菜单形态，确保 Adaptive 模式切换无需重新配置
     * WindowKit。系统按钮只通过专用 getter 配置，不会出现在此列表中。
     */
    [[nodiscard]] QList<QWidget *> hitTestVisibleWidgets() const;

Q_SIGNALS:
    /** @brief 展示标题实际变化后发出。 */
    void titleChanged(const QString &title);

    /** @brief 菜单展示策略实际变化后发出。 */
    void menuDisplayModeChanged(ZzTitleBarMenuDisplayMode mode);

    /** @brief 菜单自动折叠开关实际变化后发出。 */
    void menuCollapseEnabledChanged(bool enabled);

    /** @brief 应用确认的主题状态实际变化后发出。 */
    void themeModeChanged(ZzThemeMode mode);

    /** @brief 用户请求主题模式；标题栏不会直接修改应用主题。 */
    void themeModeRequested(ZzThemeMode mode);

    /** @brief 宿主确认的置顶状态实际变化后发出。 */
    void alwaysOnTopChanged(bool alwaysOnTop);

    /** @brief 用户请求修改置顶状态；标题栏不会直接修改窗口标志。 */
    void alwaysOnTopRequested(bool alwaysOnTop);

    /** @brief 用户请求最小化窗口；控件不执行窗口命令。 */
    void minimizeRequested();

    /** @brief 用户请求最大化或还原窗口；控件不执行窗口命令。 */
    void maximizeRestoreRequested();

    /** @brief 用户请求关闭窗口；控件不会关闭或删除宿主。 */
    void closeRequested();

protected:
    /** @brief 语言、样式、调色板或 DPR 变化时刷新 chrome 展示。 */
    void changeEvent(QEvent *event) override;

    /** @brief 观察菜单 Action 增删并同步折叠菜单的同一 QAction 实例。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /** @brief 窗口宽度变化时重新计算菜单形态和全窗口居中标题。 */
    void resizeEvent(QResizeEvent *event) override;

    /** @brief 宿主关系变化后重新绑定最小宽度约束。 */
    bool event(QEvent *event) override;

private:
    std::unique_ptr<ZzFluentTitleBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
