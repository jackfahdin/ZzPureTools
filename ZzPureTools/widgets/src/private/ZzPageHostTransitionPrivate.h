#pragma once

#include <functional>
#include <memory>
#include <ZzPureTools/ZzPageHost.h>

namespace ZzPureTools {

/** @brief 内部导出的主题、焦点与快照适配器；不拥有业务页面。 */
class ZZ_PURE_TOOLS_EXPORT ZzPageHostTransitionPrivate final
{
    Q_DISABLE_COPY_MOVE(ZzPageHostTransitionPrivate)
public:
    /** @brief 绑定页面宿主并创建唯一播放器。 */
    explicit ZzPageHostTransitionPrivate(ZzPageHost *owner);
    /** @brief 先停止播放并断开恢复回调。 */
    ~ZzPageHostTransitionPrivate();
    /** @brief 在业务生命周期提交前捕获两端页面。 */
    void prepare(QWidget *source, QWidget *target, const ZzRouteId &route,
        ZzPageHost::ZzTransitionDirection direction);
    /** @brief 业务路由提交后播放，捕获失效时直接恢复目标。 */
    void commit();
    /** @brief 取消图像并恢复真实页和仍归本区域的焦点。 */
    void settle();
    /** @brief 仅失效快照；用于 QWidget 析构信号内的安全清理。 */
    void invalidate();
    /** @brief 查询实际播放状态。 */
    bool running() const noexcept;
    /** @brief 判断路由开关是否影响当前段。 */
    bool involves(const ZzRouteId &route) const;
    /** @brief 中断环境变化，消费播放期间的键盘事件。 */
    bool event(QEvent *event);
    /** @brief 内部基准捕获区间观察桥。 */
    static void observeCapture(ZzPageHost *host, std::function<void(bool)> callback);
    /** @brief 内部资源断言桥，不属于 SDK 公共 API。 */
    static qsizetype snapshotBytes(const ZzPageHost *host);
private:
    class ZzPrivate;
    std::unique_ptr<ZzPrivate> d;
};
} // namespace ZzPureTools
