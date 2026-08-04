#pragma once

#include <memory>

#include <ZzCore/ZzCoreExport.h>
#include <ZzCore/ZzQtLogBridgeConfig.h>
#include <ZzCore/ZzResult.h>

namespace ZzCore {

class ZzQtLogBridgePrivate;

/**
 * @brief 显式管理进程唯一的 Qt 消息处理器并转发到 ZzLog。
 *
 * install() 和 uninstall() 可与日志写入并发，但生命周期方法彼此不得并发调用。
 * 对象析构会自动卸载自身；安装期间外部代码不得替换 Qt 全局消息处理器。处理器不
 * 发出 QObject 信号，也不调用 Qt 日志 API，适合接收任意线程产生的 Qt 消息。
 */
class ZZ_CORE_EXPORT ZzQtLogBridge final
{
public:
    /** @brief 创建尚未安装的日志桥。 */
    ZzQtLogBridge();

    /** @brief 卸载仍处于安装状态的日志桥并恢复旧处理器。 */
    ~ZzQtLogBridge();

    ZzQtLogBridge(const ZzQtLogBridge &) = delete;
    ZzQtLogBridge &operator=(const ZzQtLogBridge &) = delete;
    ZzQtLogBridge(ZzQtLogBridge &&) = delete;
    ZzQtLogBridge &operator=(ZzQtLogBridge &&) = delete;

    /**
     * @brief 安装进程级 Qt 消息处理器。
     * @param config 旧处理器链式调用配置。
     * @return 安装成功时返回成功；已有活动实例时返回 InvalidState。
     *
     * 调用方必须保证本函数不与 install()、uninstall() 或对象析构并发。
     */
    [[nodiscard]] ZzResult<void> install(
        const ZzQtLogBridgeConfig &config = {});

    /**
     * @brief 恢复安装前的处理器并等待全部在途桥接调用退出。
     * @return 卸载成功时返回成功；当前对象未安装时返回 InvalidState。
     *
     * 调用方必须保证本函数不与 install()、另一次 uninstall() 或析构并发，且不得
     * 从 Qt 消息处理器调用链中调用本函数。
     */
    [[nodiscard]] ZzResult<void> uninstall();

    /**
     * @brief 查询当前对象是否为活动日志桥。
     * @return 当前对象已安装且尚未开始卸载时返回 true。
     */
    [[nodiscard]] bool isInstalled() const noexcept;

private:
    std::unique_ptr<ZzQtLogBridgePrivate> d_ptr;
};

} // namespace ZzCore
