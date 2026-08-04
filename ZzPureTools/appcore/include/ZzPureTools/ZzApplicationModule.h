#pragma once

#include <ZzCore/ZzResult.h>
#include <ZzPureTools/ZzAppCoreExport.h>
#include <ZzPureTools/ZzModuleDescriptor.h>

namespace ZzPureTools {

/**
 * @brief 定义一个由 composition root 显式构造的应用模块。
 *
 * 所有方法均在应用主线程调用。requestStop() 只发出协作停止请求，
 * stop() 必须 noexcept 并完成最终资源回收。
 */
class ZZ_APP_CORE_EXPORT ZzApplicationModule
{
public:
    /** @brief 允许通过接口安全销毁具体模块。 */
    virtual ~ZzApplicationModule() = default;

    /** @brief 返回拥有值的模块身份、版本与直接依赖描述。 */
    [[nodiscard]] virtual ZzModuleDescriptor descriptor() const = 0;

    /**
     * @brief 在依赖全部启动后启动当前模块。
     * @return 成功或包含稳定错误信息的失败结果。
     */
    [[nodiscard]] virtual ZzCore::ZzResult<void> start() = 0;

    /** @brief 发出幂等的协作停止请求，不阻塞等待最终回收。 */
    virtual void requestStop() noexcept = 0;

    /** @brief 完成幂等、不可抛异常的最终资源停止。 */
    virtual void stop() noexcept = 0;
};

} // namespace ZzPureTools
