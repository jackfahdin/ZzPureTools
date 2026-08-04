#pragma once

#include <memory>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzAppCoreExport.h>
#include <ZzPureTools/ZzApplicationModule.h>

namespace ZzPureTools {

class ZzApplicationRuntime;
class ZzModuleGraphBuilderPrivate;

/**
 * @brief 收集应用模块并一次性构建经过依赖排序的运行时。
 *
 * build() 无论成功或失败都会冻结构建器。构建器只负责模块生命周期编排，
 * 不提供服务定位或运行期模块查询。
 */
class ZZ_APP_CORE_EXPORT ZzModuleGraphBuilder final
{
public:
    /** @brief 创建尚未冻结的空模块图构建器。 */
    ZzModuleGraphBuilder();

    /** @brief 销毁构建器和尚未转移给运行时的模块。 */
    ~ZzModuleGraphBuilder();

    /** @brief 禁止复制拥有模块的构建器。 */
    ZzModuleGraphBuilder(const ZzModuleGraphBuilder &) = delete;

    /** @brief 禁止复制赋值拥有模块的构建器。 */
    ZzModuleGraphBuilder &operator=(const ZzModuleGraphBuilder &) = delete;

    /**
     * @brief 转移构建器及其尚未构建的模块。
     * @param other 被移动的构建器。
     */
    ZzModuleGraphBuilder(ZzModuleGraphBuilder &&other) noexcept;

    /**
     * @brief 转移赋值构建器及其尚未构建的模块。
     * @param other 被移动的构建器。
     * @return 当前构建器引用。
     */
    ZzModuleGraphBuilder &operator=(
        ZzModuleGraphBuilder &&other) noexcept;

    /**
     * @brief 按注册顺序增加一个独占模块。
     * @param module 非空模块；构建器接管其唯一所有权。
     * @return 增加成功，或参数、冻结状态错误。
     */
    [[nodiscard]] ZzCore::ZzResult<void> addModule(
        std::unique_ptr<ZzApplicationModule> module);

    /**
     * @brief 校验模块描述并按稳定拓扑顺序构建运行时。
     * @return 独占运行时，或重复、缺失依赖、成环及异常错误。
     *
     * 该操作只能调用一次，返回后构建器永久冻结。
     */
    [[nodiscard]] ZzCore::ZzResult<
        std::unique_ptr<ZzApplicationRuntime>> build();

    /**
     * @brief 查询构建器是否已执行过 build() 或已被移动。
     * @return 不再接受模块和构建请求时返回 true。
     */
    [[nodiscard]] bool isFrozen() const noexcept;

private:
    std::unique_ptr<ZzModuleGraphBuilderPrivate> d_ptr;
};

} // namespace ZzPureTools
