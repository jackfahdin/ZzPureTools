#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <QtCore/QtGlobal>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzModuleId.h>

namespace ZzPureTools {

/** @brief 描述模块运行时内部生命周期状态。 */
enum class ZzApplicationRuntimeState : std::uint8_t
{
    Built,
    Starting,
    Running,
    StopRequested,
    Stopped
};

/** @brief 实现模块启动、失败回滚以及逆序停止状态机。 */
class ZzApplicationRuntimePrivate final
{
public:
    /** @brief 接管已经按拓扑顺序排列的模块。 */
    explicit ZzApplicationRuntimePrivate(
        std::vector<std::unique_ptr<ZzApplicationModule>> modules);

    /** @brief 保存与模块拓扑顺序一一对应的稳定标识。 */
    void setModuleIds(std::vector<ZzModuleId> moduleIds);

    /** @brief 按拓扑顺序启动全部模块。 */
    [[nodiscard]] ZzCore::ZzResult<void> start();

    /** @brief 逆序请求已启动模块停止。 */
    void requestStop() noexcept;

    /** @brief 逆序完成已启动模块停止。 */
    void stop() noexcept;

    /** @brief 查询是否处于正常运行状态。 */
    [[nodiscard]] bool isRunning() const noexcept;

    /** @brief 返回持有的模块数量。 */
    [[nodiscard]] qsizetype moduleCount() const noexcept;

private:
    /** @brief 回滚此前成功启动的模块并进入停止状态。 */
    void rollbackStartedModules() noexcept;

    std::vector<std::unique_ptr<ZzApplicationModule>> modules_;
    std::vector<ZzModuleId> moduleIds_;
    std::size_t startedCount_ = 0;
    ZzApplicationRuntimeState state_ = ZzApplicationRuntimeState::Built;
};

} // namespace ZzPureTools
