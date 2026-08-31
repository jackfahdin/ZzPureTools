#pragma once

#include <memory>

#include <QtWidgets/QMainWindow>

#include <ZzCore/ZzResult.h>

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzExample {

class ZzExampleAboutWindowPrivate;
class ZzExampleApplicationContext;
class ZzExampleWindowShell;

/** @brief 承载当前主窗口独占关于页面的窗口模态 View。 */
class ZzExampleAboutWindow final : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzExampleAboutWindow)

public:
    /**
     * @brief 创建并完整初始化当前主窗口的关于窗口。
     * @param parentWindow 非空主窗口，同时作为 Qt 父对象和模态宿主。
     * @param context 非空共享应用上下文。
     * @param application 非空主题与窗口应用宿主。
     * @param shell 非空当前主窗口壳层。
     * @return 由 parentWindow 拥有的窗口观察指针，或初始化错误。
     */
    [[nodiscard]] static ZzCore::ZzResult<ZzExampleAboutWindow *> create(
        ZzPureTools::ZzApplicationWindow *parentWindow,
        std::shared_ptr<ZzExampleApplicationContext> context,
        ZzPureTools::ZzPureApplication *application,
        ZzExampleWindowShell *shell);

    /** @brief 按 Presenter、模型和 WindowKit 代理顺序释放私有状态。 */
    ~ZzExampleAboutWindow() override;

protected:
    /** @brief 同步语言变化，其余变化交给 QMainWindow。 */
    void changeEvent(QEvent *event) override;

private:
    /** @brief 创建尚未初始化且由指定主窗口拥有的模态窗口。 */
    explicit ZzExampleAboutWindow(
        ZzPureTools::ZzApplicationWindow *parentWindow);

    /** @brief 完成关于 View、Presenter 与 WindowKit chrome 装配。 */
    [[nodiscard]] ZzCore::ZzResult<void> initialize(
        std::shared_ptr<ZzExampleApplicationContext> context,
        ZzPureTools::ZzPureApplication *application,
        ZzExampleWindowShell *shell);

    std::unique_ptr<ZzExampleAboutWindowPrivate> d_ptr;
};

} // namespace ZzExample
