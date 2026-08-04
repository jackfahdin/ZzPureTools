#pragma once

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzModuleDescriptor.h>

/** @brief 演示不依赖展示层的最小应用模块生命周期。 */
class ZzDemoModule final : public ZzPureTools::ZzApplicationModule
{
public:
    /** @brief 返回模块稳定标识、版本和空依赖集合。 */
    [[nodiscard]] ZzPureTools::ZzModuleDescriptor descriptor()
        const override;

    /** @brief 记录模块进入已启动状态。 */
    [[nodiscard]] ZzCore::ZzResult<void> start() override;

    /** @brief 幂等记录协作停止请求。 */
    void requestStop() noexcept override;

    /** @brief 幂等完成模块状态清理。 */
    void stop() noexcept override;

private:
    bool started_ = false;
    bool stopRequested_ = false;
};
