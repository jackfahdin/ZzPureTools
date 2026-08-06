#pragma once

#include <memory>

#include <QtCore/QObject>

#include "ZzExampleSystemPageKind.h"

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleSystemPage;
class ZzExampleSystemPresenterPrivate;
class ZzExampleSystemViewModel;
class ZzExampleWindowShell;

/** @brief 协调系统页与设置、主题、日志、窗口及平台能力端口。 */
class ZzExampleSystemPresenter final : public QObject
{
public:
    /**
     * @brief 使用 composition root 注入的非 UI 服务和窗口端口装配页面。
     * @param kind 页面种类。
     * @param view 非空页面 View。
     * @param viewModel 非空展示模型。
     * @param context 非空共享应用上下文。
     * @param application 非空应用级主题宿主。
     * @param window 非空当前窗口。
     * @param shell 非空当前窗口壳层。
     */
    ZzExampleSystemPresenter(
        ZzExampleSystemPageKind kind,
        ZzExampleSystemPage *view,
        ZzExampleSystemViewModel *viewModel,
        std::shared_ptr<ZzExampleApplicationContext> context,
        ZzPureTools::ZzPureApplication *application,
        ZzPureTools::ZzApplicationWindow *window,
        ZzExampleWindowShell *shell);

    /** @brief 断开设置与窗口观察并释放共享上下文。 */
    ~ZzExampleSystemPresenter() override;

    /** @brief 禁止复制持有连接和共享上下文的 Presenter。 */
    ZzExampleSystemPresenter(
        const ZzExampleSystemPresenter &) = delete;

    /** @brief 禁止复制赋值持有连接和共享上下文的 Presenter。 */
    ZzExampleSystemPresenter &operator=(
        const ZzExampleSystemPresenter &) = delete;

    /** @brief 禁止移动已经注册为 QObject 接收者的 Presenter。 */
    ZzExampleSystemPresenter(ZzExampleSystemPresenter &&) = delete;

    /** @brief 禁止移动赋值已经注册为 QObject 接收者的 Presenter。 */
    ZzExampleSystemPresenter &operator=(
        ZzExampleSystemPresenter &&) = delete;

private:
    std::unique_ptr<ZzExampleSystemPresenterPrivate> d_ptr;
};

} // namespace ZzExample
