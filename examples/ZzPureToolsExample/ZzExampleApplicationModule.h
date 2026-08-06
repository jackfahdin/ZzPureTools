#pragma once

#include <memory>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzModuleDescriptor.h>

namespace ZzExample {

class ZzExampleApplicationContext;
class ZzExampleApplicationModulePrivate;

/** @brief 管理正式示例的日志、Qt 日志桥和共享服务停止顺序。 */
class ZzExampleApplicationModule final
    : public ZzPureTools::ZzApplicationModule
{
public:
    /** @brief 创建观察同一共享上下文的应用生命周期模块。 */
    explicit ZzExampleApplicationModule(
        std::shared_ptr<ZzExampleApplicationContext> context);

    /** @brief 幂等停止仍在运行的模块并释放私有状态。 */
    ~ZzExampleApplicationModule() override;

    /** @brief 禁止复制独占日志生命周期的模块。 */
    ZzExampleApplicationModule(const ZzExampleApplicationModule &) = delete;

    /** @brief 禁止复制赋值独占日志生命周期的模块。 */
    ZzExampleApplicationModule &operator=(
        const ZzExampleApplicationModule &) = delete;

    /** @brief 禁止移动已由应用运行时持有的模块。 */
    ZzExampleApplicationModule(ZzExampleApplicationModule &&) = delete;

    /** @brief 禁止移动赋值已由应用运行时持有的模块。 */
    ZzExampleApplicationModule &operator=(
        ZzExampleApplicationModule &&) = delete;

    /** @brief 返回模块稳定标识、版本和空依赖集合。 */
    [[nodiscard]] ZzPureTools::ZzModuleDescriptor descriptor()
        const override;

    /** @brief 初始化 ZzLog 并安装 Qt 全局日志桥。 */
    [[nodiscard]] ZzCore::ZzResult<void> start() override;

    /** @brief 幂等记录协作停止请求。 */
    void requestStop() noexcept override;

    /** @brief 同步设置、停止任务并关闭日志运行时。 */
    void stop() noexcept override;

private:
    std::unique_ptr<ZzExampleApplicationModulePrivate> d_ptr;
};

} // namespace ZzExample
