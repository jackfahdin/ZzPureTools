#pragma once

#include <QtCore/QString>

class QElapsedTimer;

namespace ZzBenchmarks {
class ZzPerformanceReporter;
}

namespace ZzPureTools {
class ZzApplicationWindow;
class ZzPureApplication;
}

namespace ZzExample {

/** @brief 标识综合示例支持的互斥性能测量场景。 */
enum class ZzExamplePerformanceScenario
{
    Disabled,
    StartupProbe,
    Navigation,
    ThemeSwitch,
    LargeModel,
    Idle,
    Invalid
};

/** @brief 实现综合示例性能场景的确定性测量与报告写入。 */
class ZzExamplePerformanceControllerPrivate final
{
public:
    /** @brief 保存应用与进程入口计时器观察值并解析性能场景。 */
    ZzExamplePerformanceControllerPrivate(
        ZzPureTools::ZzPureApplication *application,
        const QElapsedTimer *processTimer);

    /** @brief 返回是否请求了性能场景。 */
    [[nodiscard]] bool isEnabled() const noexcept;

    /** @brief 对首个完成装配的窗口安装或调度当前性能场景。 */
    void windowAttached(ZzPureTools::ZzApplicationWindow &window);

private:
    /** @brief 从唯一环境变量解析互斥性能场景。 */
    [[nodiscard]] static ZzExamplePerformanceScenario readScenario();

    /** @brief 在首次完整绘制后向父 benchmark 输出启动标记。 */
    void installStartupProbe(ZzPureTools::ZzApplicationWindow &window);

    /** @brief 测量真实页面创建、切换、布局和绘制延迟。 */
    void measureNavigation(ZzPureTools::ZzApplicationWindow &window);

    /** @brief 测量完整窗口在 Light/Dark 间切换和绘制的延迟。 */
    void measureThemeSwitch(ZzPureTools::ZzApplicationWindow &window);

    /** @brief 将列表页替换为十万行惰性模型并测量逐页滚动。 */
    void measureLargeModel(ZzPureTools::ZzApplicationWindow &window);

    /** @brief 在无轮询事件循环中测量完整应用空闲 CPU 与 RSS。 */
    void measureIdle(ZzPureTools::ZzApplicationWindow &window);

    /**
     * @brief 初始化当前场景的 reporter 和统一环境指纹。
     * @param reporter 待写入的空 reporter。
     * @param scenario 稳定报告场景名。
     * @param warmupIterations 未计入正式样本的预热次数。
     * @param window 提供实际屏幕身份的综合窗口。
     * @return 初始化是否成功；失败时已安排应用失败退出。
     */
    [[nodiscard]] bool initializeReporter(
        ZzBenchmarks::ZzPerformanceReporter *reporter,
        const QString &scenario,
        qsizetype warmupIterations,
        ZzPureTools::ZzApplicationWindow &window) const;

    /** @brief 原子写入报告并以对应状态结束应用。 */
    void writeReportAndExit(
        const ZzBenchmarks::ZzPerformanceReporter &reporter) const;

    /** @brief 输出稳定技术原因并以失败状态结束事件循环。 */
    void fail(const QString &reason) const;

    ZzPureTools::ZzPureApplication *application = nullptr;
    const QElapsedTimer *processTimer = nullptr;
    ZzExamplePerformanceScenario scenario =
        ZzExamplePerformanceScenario::Disabled;
    QString reportPath;
    bool scheduled = false;
};

} // namespace ZzExample
