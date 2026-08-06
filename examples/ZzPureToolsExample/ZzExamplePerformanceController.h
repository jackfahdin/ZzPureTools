#pragma once

#include <memory>

class QElapsedTimer;

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzExample {

class ZzExamplePerformanceControllerPrivate;

/**
 * @brief 在 benchmark 构建中调度完整综合示例的性能场景。
 *
 * 控制器只负责测量展示层工作流并输出统一 reporter JSON，不进入普通发布构建，
 * 也不改变业务模型或正式交互路径。
 */
class ZzExamplePerformanceController final
{
public:
    /**
     * @brief 创建读取性能场景环境变量的控制器。
     * @param application 被测应用宿主。
     * @param processTimer 从 main() 入口启动且覆盖完整启动阶段的计时器。
     */
    ZzExamplePerformanceController(
        ZzPureTools::ZzPureApplication &application,
        const QElapsedTimer &processTimer);

    /** @brief 释放尚未触发的性能调度状态。 */
    ~ZzExamplePerformanceController();

    /** @brief 禁止复制持有单次测量状态的控制器。 */
    ZzExamplePerformanceController(
        const ZzExamplePerformanceController &) = delete;

    /** @brief 禁止复制赋值持有单次测量状态的控制器。 */
    ZzExamplePerformanceController &operator=(
        const ZzExamplePerformanceController &) = delete;

    /** @brief 禁止移动已被窗口装配回调观察的控制器。 */
    ZzExamplePerformanceController(
        ZzExamplePerformanceController &&) = delete;

    /** @brief 禁止移动赋值已被窗口装配回调观察的控制器。 */
    ZzExamplePerformanceController &operator=(
        ZzExamplePerformanceController &&) = delete;

    /** @brief 返回是否请求了性能场景。 */
    [[nodiscard]] bool isEnabled() const noexcept;

    /**
     * @brief 在首窗完成壳层装配后安装或调度一次性能场景。
     * @param window 已完成综合示例装配但可能尚未首次绘制的窗口。
     */
    void windowAttached(ZzPureTools::ZzApplicationWindow &window);

private:
    std::unique_ptr<ZzExamplePerformanceControllerPrivate> d_ptr;
};

} // namespace ZzExample
