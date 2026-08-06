#pragma once

#include <memory>

#include <ZzCore/ZzResult.h>

class QAction;
class QLabel;
class QLineEdit;
class QStatusBar;

namespace ZzCore {
class ZzError;
}

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzNavigationController;
class ZzPureApplication;
}

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleWindowShell;

/** @brief 实现单窗口壳层控件创建与展示意图转发。 */
class ZzExampleWindowShellPrivate final
{
public:
    /** @brief 保存由外层生命周期保证有效的窗口与应用观察值。 */
    ZzExampleWindowShellPrivate(
        ZzExampleWindowShell *shell,
        ZzPureTools::ZzApplicationWindow *applicationWindow,
        std::shared_ptr<ZzExampleApplicationContext> applicationContext,
        ZzPureTools::ZzPureApplication *pureApplication);

    /** @brief 创建控件并连接窗口级导航、主题和多窗口意图。 */
    [[nodiscard]] ZzCore::ZzResult<void> initialize();

    /** @brief 按搜索文本激活首个匹配路由。 */
    void navigateFromSearch();

    /** @brief 循环切换 System、Light、Dark 与 HighContrast。 */
    void cycleTheme();

    /** @brief 在状态栏报告技术失败并写入 Qt 日志。 */
    void reportFailure(const ZzCore::ZzError &error);

    /** @brief 同步返回与前进命令的启用状态。 */
    void syncHistoryActions(bool canGoBack, bool canGoForward) noexcept;

    ZzExampleWindowShell *q_ptr = nullptr;
    ZzPureTools::ZzApplicationWindow *window = nullptr;
    std::shared_ptr<ZzExampleApplicationContext> context;
    ZzPureTools::ZzPureApplication *application = nullptr;
    ZzPureTools::ZzNavigationController *navigation = nullptr;
    QAction *backAction = nullptr;
    QAction *forwardAction = nullptr;
    QLineEdit *searchEdit = nullptr;
    QStatusBar *statusBar = nullptr;
    QLabel *routeLabel = nullptr;
};

} // namespace ZzExample
