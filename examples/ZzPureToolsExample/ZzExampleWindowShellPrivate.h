#pragma once

#include <memory>

#include <QtCore/QPointer>

#include <ZzCore/ZzResult.h>
#include <ZzPureTools/ZzWorkspaceCenterMode.h>

#include <ZzFluentUI/ZzThemeMode.h>

class QAction;
class QEvent;
class QLabel;
class QLineEdit;
class QObject;
class QStatusBar;
class QWidget;

namespace ZzCore {
class ZzError;
}

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzNavigationController;
class ZzPureApplication;
class ZzWorkspaceShell;
}

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleAboutWindow;
enum class ZzExampleCommandId : int;
class ZzExampleSessionModel;
class ZzExampleFileModel;
class ZzExampleSettingsWindow;
class ZzExampleWindowShell;

/** @brief 使用公开工作区组件实现单窗口展示与 QAction 意图转发。 */
class ZzExampleWindowShellPrivate final
{
public:
    /** @brief 保存由外层生命周期保证有效的窗口与应用观察值。 */
    ZzExampleWindowShellPrivate(
        ZzExampleWindowShell *shell,
        ZzPureTools::ZzApplicationWindow *applicationWindow,
        std::shared_ptr<ZzExampleApplicationContext> applicationContext,
        ZzPureTools::ZzPureApplication *pureApplication,
        bool enableCloseGuard,
        ZzPureTools::ZzWorkspaceCenterMode centerMode);

    /** @brief 在完整类型可见处释放公开 Shell 和本地会话模型。 */
    ~ZzExampleWindowShellPrivate();

    /** @brief 通过公开 Shell 创建 Activity、Tab、Command 和 Dock 工作区。 */
    [[nodiscard]] ZzCore::ZzResult<void> initialize();

    /** @brief 按搜索文本激活首个匹配路由。 */
    void navigateFromSearch();

    /** @brief 根据当前实际主题切换浅色或深色。 */
    void cycleTheme();

    /**
     * @brief 应用并持久化来自标题栏的主题模式请求。
     * @return 设置写入成功时返回 true，否则返回 false。
     */
    [[nodiscard]] bool applyThemeMode(ZzFluentUI::ZzThemeMode mode);

    /** @brief 执行命令模型选中的窗口级展示意图。 */
    void dispatchWorkspaceCommand(ZzExampleCommandId command);

    /** @brief 打开指定会话或新建终端页；已打开的指定会话直接复用。 */
    void createTerminalPage(const QString &sessionName = {});

    /** @brief 关闭当前允许关闭的终端展示标签。 */
    void closeCurrentTerminal();

    /** @brief 在状态栏报告技术失败并写入 Qt 日志。 */
    void reportFailure(const ZzCore::ZzError &error);

    /** @brief 同时追加共享活动模型并写入 ZzLog。 */
    void recordActivity(const QString &text);

    /** @brief 同步返回与前进命令的启用状态。 */
    void syncHistoryActions(bool canGoBack, bool canGoForward) noexcept;

    /** @brief 返回活动输出工具当前可见性。 */
    [[nodiscard]] bool isActivityDockVisible() const noexcept;

    /** @brief 通过公开 Shell 设置活动输出工具可见性。 */
    void setActivityDockVisible(bool visible);

    /** @brief 处理当前窗口的取消、最小化或确认关闭选择。 */
    [[nodiscard]] bool filterWindowEvent(QObject *watched, QEvent *event);

    ZzExampleWindowShell *q_ptr = nullptr;
    ZzPureTools::ZzApplicationWindow *window = nullptr;
    std::shared_ptr<ZzExampleApplicationContext> context;
    ZzPureTools::ZzPureApplication *application = nullptr;
    ZzPureTools::ZzNavigationController *navigation = nullptr;
    std::unique_ptr<ZzPureTools::ZzWorkspaceShell> workspace;
    std::unique_ptr<ZzExampleSessionModel> sessions;
    std::unique_ptr<ZzExampleFileModel> fileModel;
    QAction *backAction = nullptr;
    QAction *forwardAction = nullptr;
    QAction *settingsAction = nullptr;
    QAction *aboutAction = nullptr;
    QPointer<QWidget> settingsWindow;
    QPointer<QWidget> aboutWindow;
    QLineEdit *searchEdit = nullptr;
    QStatusBar *statusBar = nullptr;
    QLabel *routeLabel = nullptr;
    int terminalSequence = 0;
    ZzPureTools::ZzWorkspaceCenterMode centerMode;
    bool closeGuardEnabled = true;
    bool closeGuardActive = false;
};

} // namespace ZzExample
