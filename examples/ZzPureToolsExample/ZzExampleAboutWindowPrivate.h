#pragma once

#include <memory>

#include <QtCore/QPointer>

#include <ZzCore/ZzResult.h>

namespace ZzFluentUI {
class ZzFluentTitleBar;
}

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzWindowKit {
class ZzWindowAgent;
}

namespace ZzExample {

class ZzExampleAboutWindow;
class ZzExampleApplicationContext;
class ZzExampleSystemPage;
class ZzExampleSystemPresenter;
class ZzExampleSystemViewModel;
class ZzExampleWindowShell;

/** @brief 装配关于窗口 View、Presenter 与只保留关闭命令的无边框 chrome。 */
class ZzExampleAboutWindowPrivate final
{
public:
    /** @brief 绑定尚未初始化的公开关于窗口。 */
    explicit ZzExampleAboutWindowPrivate(ZzExampleAboutWindow *window);

    /** @brief 先释放观察 Qt 子树的 Presenter、模型与 WindowKit 代理。 */
    ~ZzExampleAboutWindowPrivate();

    /**
     * @brief 按 View、Presenter 和 WindowKit 的固定顺序完成装配。
     * @return 全部完成时成功，输入或 WindowKit 错误原样返回。
     */
    [[nodiscard]] ZzCore::ZzResult<void> initialize(
        std::shared_ptr<ZzExampleApplicationContext> context,
        ZzPureTools::ZzPureApplication *application,
        ZzExampleWindowShell *shell);

    /** @brief 刷新关于窗口及标题栏的本地化标题。 */
    void refreshTranslations();

    ZzExampleAboutWindow *const q_ptr;
    QPointer<ZzPureTools::ZzApplicationWindow> parentWindow;
    ZzFluentUI::ZzFluentTitleBar *titleBar = nullptr;
    QPointer<ZzExampleSystemPage> page;
    std::unique_ptr<ZzExampleSystemViewModel> viewModel;
    std::unique_ptr<ZzExampleSystemPresenter> presenter;
    std::unique_ptr<ZzWindowKit::ZzWindowAgent> agent;
    bool initialized = false;
};

} // namespace ZzExample
