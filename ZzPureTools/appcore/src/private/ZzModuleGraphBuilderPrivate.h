#pragma once

#include <memory>
#include <vector>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzApplicationModule.h>

namespace ZzPureTools {

class ZzApplicationRuntime;

/** @brief 实现模块描述校验和稳定线性复杂度拓扑排序。 */
class ZzModuleGraphBuilderPrivate final
{
public:
    /** @brief 增加一个由构建器独占的模块。 */
    [[nodiscard]] ZzCore::ZzResult<void> addModule(
        std::unique_ptr<ZzApplicationModule> module);

    /** @brief 冻结构建器并生成按依赖排序的运行时。 */
    [[nodiscard]] ZzCore::ZzResult<
        std::unique_ptr<ZzApplicationRuntime>> build();

    /** @brief 查询构建器是否已执行构建。 */
    [[nodiscard]] bool isFrozen() const noexcept;

private:
    std::vector<std::unique_ptr<ZzApplicationModule>> modules_;
    bool frozen_ = false;
};

} // namespace ZzPureTools
