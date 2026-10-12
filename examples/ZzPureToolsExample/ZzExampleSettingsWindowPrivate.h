#pragma once

#include <memory>

#include <QtCore/QPointer>

#include <ZzCore/ZzResult.h>

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzAuxiliaryWindowChrome;
class ZzPureApplication;
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

    ZzExampleSettingsWindow *const q_ptr;
    QPointer<ZzPureTools::ZzApplicationWindow> parentWindow;
    QPointer<ZzExampleSystemPage> page;
    std::unique_ptr<ZzExampleSystemViewModel> viewModel;
    std::unique_ptr<ZzExampleSystemPresenter> presenter;
    std::unique_ptr<ZzPureTools::ZzAuxiliaryWindowChrome> chrome;
    bool initialized = false;
};

} // namespace ZzExample
