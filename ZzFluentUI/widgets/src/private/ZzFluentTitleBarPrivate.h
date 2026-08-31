#pragma once

#include <cstdint>

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtGui/QIcon>

class QAction;
class QActionEvent;
class QActionGroup;
class QLabel;
class QMenu;
class QMenuBar;
class QToolButton;
class QWidget;

namespace ZzFluentUI {

class ZzFluentTitleBar;
enum class ZzThemeMode : std::uint8_t;
enum class ZzTitleBarThemeInteractionMode : std::uint8_t;
enum class ZzTitleBarMenuDisplayMode : std::uint8_t;

/** @brief 管理标题栏纯视觉子控件、翻译和状态，不执行窗口命令。 */
class ZzFluentTitleBarPrivate final
{
public:
    /** @brief 创建标题、图标和三个意图按钮。 */
    explicit ZzFluentTitleBarPrivate(ZzFluentTitleBar *q);

    /** @brief 恢复宿主在本组件介入前的最小宽度约束。 */
    ~ZzFluentTitleBarPrivate();

    /** @brief 刷新图标、tooltip、accessible name 和标题。 */
    void refreshPresentation();

    /** @brief 刷新标题文本和全窗口中心安全几何。 */
    void refreshTitle();

    /** @brief 按策略与可用宽度更新菜单形态和全部子控件几何。 */
    void updateLayout();

    /** @brief 计算完整菜单、标题和窗口按钮所需的最小逻辑宽度。 */
    [[nodiscard]] int minimumExpandedWidth() const noexcept;

    /** @brief 按开关状态同步宿主顶层窗口最小宽度并保留外部值。 */
    void syncWindowMinimumWidth(int requiredWidth);

    /** @brief 恢复并清除已记录的宿主最小宽度。 */
    void restoreWindowMinimumWidth() noexcept;

    /** @brief 使用菜单栏现有 QAction 重建折叠菜单投影。 */
    void rebuildCompactMenu();

    /**
     * @brief 增量同步一个菜单栏 QAction 事件。
     * @param event ActionAdded 或 ActionRemoved 事件。
     */
    void handleMenuActionEvent(QActionEvent *event);

    /** @brief 将确认主题状态回写到菜单动作。 */
    void refreshThemeActions();

    ZzFluentTitleBar *const q_ptr;
    QLabel *iconLabel = nullptr;
    QLabel *titleLabel = nullptr;
    QMenuBar *menuBar = nullptr;
    QToolButton *compactMenuButton = nullptr;
    QMenu *compactMenu = nullptr;
    QToolButton *themeButton = nullptr;
    QMenu *themeMenu = nullptr;
    QActionGroup *themeActionGroup = nullptr;
    QAction *systemThemeAction = nullptr;
    QAction *lightThemeAction = nullptr;
    QAction *darkThemeAction = nullptr;
    QAction *highContrastThemeAction = nullptr;
    QToolButton *alwaysOnTopButton = nullptr;
    QToolButton *minimizeButton = nullptr;
    QToolButton *maximizeButton = nullptr;
    QToolButton *closeButton = nullptr;
    QString title;
    QIcon windowIcon;
    ZzTitleBarMenuDisplayMode menuDisplayMode;
    ZzThemeMode themeMode;
    ZzTitleBarThemeInteractionMode themeInteractionMode;
    bool maximized = false;
    bool systemButtonsVisible = true;
    bool alwaysOnTop = false;
    bool menuCollapseEnabled = true;
    bool adaptiveExpanded = true;
    QPointer<QWidget> minimumWidthHost;
    int originalHostMinimumWidth = 0;
    int enforcedHostMinimumWidth = 0;
};

} // namespace ZzFluentUI
