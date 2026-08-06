#pragma once

#include <memory>

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleSmokeControllerPrivate;

/**
 * @brief 调度不进入正式交互路径的示例端到端 smoke 场景。
 *
 * 控制器只在显式 smoke 模式下操作窗口；正常桌面运行仅通过
 * closeGuardEnabled() 保留交互式关闭守卫。
 */
class ZzExampleSmokeController final
{
public:
    /**
     * @brief 创建读取 smoke 场景环境变量的控制器。
     * @param enabled 是否启用自动 smoke。
     * @param application 被测应用宿主。
     * @param context 跨窗口共享的应用上下文。
     */
    ZzExampleSmokeController(
        bool enabled,
        ZzPureTools::ZzPureApplication &application,
        std::shared_ptr<ZzExampleApplicationContext> context);

    /** @brief 释放尚未触发的 smoke 观察状态。 */
    ~ZzExampleSmokeController();

    /** @brief 禁止复制持有单次调度状态的控制器。 */
    ZzExampleSmokeController(
        const ZzExampleSmokeController &) = delete;

    /** @brief 禁止复制赋值持有单次调度状态的控制器。 */
    ZzExampleSmokeController &operator=(
        const ZzExampleSmokeController &) = delete;

    /** @brief 禁止移动已被窗口装配回调观察的控制器。 */
    ZzExampleSmokeController(ZzExampleSmokeController &&) = delete;

    /** @brief 禁止移动赋值已被窗口装配回调观察的控制器。 */
    ZzExampleSmokeController &operator=(
        ZzExampleSmokeController &&) = delete;

    /** @brief 返回当前场景是否应启用真实关闭守卫。 */
    [[nodiscard]] bool closeGuardEnabled() const noexcept;

    /**
     * @brief 在窗口完成壳层装配后调度一次对应 smoke 场景。
     * @param window 已完成示例壳层装配但可能尚未显示的窗口。
     */
    void windowAttached(ZzPureTools::ZzApplicationWindow &window);

private:
    std::unique_ptr<ZzExampleSmokeControllerPrivate> d_ptr;
};

} // namespace ZzExample
