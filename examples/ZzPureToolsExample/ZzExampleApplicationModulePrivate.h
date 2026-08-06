#pragma once

#include <memory>

#include <ZzCore/ZzQtLogBridge.h>
#include <ZzCore/ZzResult.h>

namespace ZzExample {

class ZzExampleApplicationContext;

/** @brief 实现示例应用模块的具体资源启停协议。 */
class ZzExampleApplicationModulePrivate final
{
public:
    /** @brief 保存非空共享上下文。 */
    explicit ZzExampleApplicationModulePrivate(
        std::shared_ptr<ZzExampleApplicationContext> applicationContext);

    /** @brief 初始化文件日志和 Qt 消息桥。 */
    [[nodiscard]] ZzCore::ZzResult<void> start();

    /** @brief 记录协作停止请求。 */
    void requestStop() noexcept;

    /** @brief 按稳定顺序幂等释放全部模块资源。 */
    void stop() noexcept;

    std::shared_ptr<ZzExampleApplicationContext> context;
    ZzCore::ZzQtLogBridge logBridge;
    bool started = false;
    bool stopRequested = false;
    bool ownsLogRuntime = false;
};

} // namespace ZzExample
