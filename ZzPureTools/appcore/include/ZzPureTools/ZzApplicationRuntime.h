#pragma once

#include <memory>
#include <vector>

#include <QtCore/QtGlobal>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzAppCoreExport.h>
#include <ZzPureTools/ZzApplicationModule.h>

namespace ZzPureTools {

class ZzApplicationRuntimePrivate;
class ZzModuleGraphBuilderPrivate;

/**
 * @brief 按依赖顺序启动并按相反顺序停止一组应用模块。
 *
 * 对象拥有全部模块。启动失败只回滚此前成功启动的模块；析构和移动赋值会
 * 自动完成仍在运行模块的协作停止和最终停止。
 */
class ZZ_APP_CORE_EXPORT ZzApplicationRuntime final
{
public:
    /** @brief 停止仍在运行的模块并销毁运行时。 */
    ~ZzApplicationRuntime();

    /** @brief 禁止复制拥有模块的运行时。 */
    ZzApplicationRuntime(const ZzApplicationRuntime &) = delete;

    /** @brief 禁止复制赋值拥有模块的运行时。 */
    ZzApplicationRuntime &operator=(const ZzApplicationRuntime &) = delete;

    /**
     * @brief 转移运行时和模块所有权。
     * @param other 被移动的运行时。
     */
    ZzApplicationRuntime(ZzApplicationRuntime &&other) noexcept;

    /**
     * @brief 停止当前模块后转移另一个运行时的所有权。
     * @param other 被移动的运行时。
     * @return 当前运行时引用。
     */
    ZzApplicationRuntime &operator=(
        ZzApplicationRuntime &&other) noexcept;

    /**
     * @brief 按拓扑顺序启动全部模块。
     * @return 全部启动成功，或首个启动失败及其模块上下文。
     */
    [[nodiscard]] ZzCore::ZzResult<void> start();

    /**
     * @brief 按逆拓扑顺序向已启动模块发出幂等协作停止请求。
     */
    void requestStop() noexcept;

    /** @brief 按逆拓扑顺序幂等完成最终停止。 */
    void stop() noexcept;

    /**
     * @brief 查询运行时是否处于正常运行状态。
     * @return 全部模块已启动且尚未请求停止时返回 true。
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief 返回运行时拥有的模块数量。
     * @return 模块数量；移动后的对象返回 0。
     */
    [[nodiscard]] qsizetype moduleCount() const noexcept;

private:
    friend class ZzModuleGraphBuilderPrivate;

    explicit ZzApplicationRuntime(
        std::vector<std::unique_ptr<ZzApplicationModule>> modules);

    std::unique_ptr<ZzApplicationRuntimePrivate> d_ptr;
};

} // namespace ZzPureTools
