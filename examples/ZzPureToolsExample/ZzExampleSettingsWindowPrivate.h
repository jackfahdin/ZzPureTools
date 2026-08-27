#pragma once

#include <memory>

#include <QtCore/QPointer>

#include <ZzCore/ZzResult.h>

namespace ZzFluentUI {
class ZzFluentTitleBar;
class ZzThemeController;
}

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzWindowKit {
class ZzWindowAgent;
}

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleSettingsWindow;
class ZzExampleSystemPage;
class ZzExampleSystemPresenter;
class ZzExampleSystemViewModel;
class ZzExampleWindowShell;

/** @brief 装配设置窗口 View、Presenter、主题观察和无边框 chrome。 */
class ZzExampleSettingsWindowPrivate final
{
public:
    /** @brief 绑定尚未初始化的公开设置窗口。 */
    explicit ZzExampleSettingsWindowPrivate(
        ZzExampleSettingsWindow *window);

    /** @brief 先释放观察 Qt 子树的 Presenter、模型与 WindowKit 代理。 */
    ~ZzExampleSettingsWindowPrivate();

    /**
     * @brief 按 View、Presenter、WindowKit 和信号的固定顺序完成装配。
     * @return 全部完成时成功，输入或 WindowKit 错误原样返回。
     */
    [[nodiscard]] ZzCore::ZzResult<void> initialize(
        std::shared_ptr<ZzExampleApplicationContext> context,
        ZzPureTools::ZzPureApplication *application,
        ZzExampleWindowShell *shell);

    /** @brief 刷新设置窗口及标题栏的本地化标题。 */
    void refreshTranslations();

    /** @brief 同步标题栏最大化或还原视觉状态。 */
    void syncWindowState();

    /** @brief 同步标题栏已生效的应用主题模式。 */
    void syncTheme();

    ZzExampleSettingsWindow *const q_ptr;
    QPointer<ZzPureTools::ZzApplicationWindow> parentWindow;
    QPointer<ZzFluentUI::ZzThemeController> theme;
    ZzFluentUI::ZzFluentTitleBar *titleBar = nullptr;
    QPointer<ZzExampleSystemPage> page;
    std::unique_ptr<ZzExampleSystemViewModel> viewModel;
    std::unique_ptr<ZzExampleSystemPresenter> presenter;
    std::unique_ptr<ZzWindowKit::ZzWindowAgent> agent;
    bool initialized = false;
};

} // namespace ZzExample
