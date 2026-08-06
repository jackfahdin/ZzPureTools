#pragma once

#include <memory>

#include <QtCore/QString>

class QAbstractButton;
class QMessageBox;

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleSmokeController;

/** @brief 标识示例 smoke 的互斥自动化场景。 */
enum class ZzExampleSmokeScenario
{
    Disabled,
    Routes,
    MultiWindow,
    CloseCancel,
    CloseMinimize,
    CloseConfirm,
    Screenshot,
    Invalid
};

/** @brief 实现路由、多窗口和关闭守卫 smoke 的确定性调度。 */
class ZzExampleSmokeControllerPrivate final
{
public:
    /** @brief 保存应用与共享上下文观察值并解析 smoke 场景。 */
    ZzExampleSmokeControllerPrivate(
        bool enabled,
        ZzPureTools::ZzPureApplication *application,
        std::shared_ptr<ZzExampleApplicationContext> context);

    /** @brief 返回非 smoke 或关闭守卫场景是否需要真实守卫。 */
    [[nodiscard]] bool closeGuardEnabled() const noexcept;

    /** @brief 对首个完成装配的窗口调度当前 smoke 场景。 */
    void windowAttached(ZzPureTools::ZzApplicationWindow &window);

private:
    /** @brief 从单一环境变量解析互斥 smoke 场景。 */
    [[nodiscard]] static ZzExampleSmokeScenario readScenario(bool enabled);

    /** @brief 依次导航全部正式路由并在失败时终止 smoke。 */
    void scheduleRouteSmoke(ZzPureTools::ZzApplicationWindow &window);

    /** @brief 验证两窗口导航、代理、Dock、活动模型与关闭隔离。 */
    void scheduleMultiWindowSmoke(
        ZzPureTools::ZzApplicationWindow &window);

    /** @brief 自动选择关闭对话框角色并验证对应窗口结果。 */
    void scheduleCloseGuardSmoke(
        ZzPureTools::ZzApplicationWindow &window);

    /** @brief 固定字体、窗口和主题后生成或比较综合示例截图。 */
    void scheduleScreenshotSmoke(
        ZzPureTools::ZzApplicationWindow &window);

    /** @brief 在活动模态对话框中点击当前场景对应角色按钮。 */
    void chooseCloseDialogButton();

    /** @brief 输出稳定技术原因与可选上下文并以失败状态结束事件循环。 */
    void fail(const char *reason, const QString &details = {}) const;

    ZzPureTools::ZzPureApplication *application = nullptr;
    std::shared_ptr<ZzExampleApplicationContext> context;
    ZzExampleSmokeScenario scenario = ZzExampleSmokeScenario::Disabled;
    bool scheduled = false;
};

} // namespace ZzExample
