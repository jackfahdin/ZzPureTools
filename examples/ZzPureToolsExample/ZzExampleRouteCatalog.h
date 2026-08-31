#pragma once

#include <span>
#include <string_view>

#include <ZzFluentUI/ZzNavigationPlacement.h>

#include <ZzPureTools/ZzPageLifetimePolicy.h>

namespace ZzExample {

/** @brief 描述正式示例的一条稳定路由及导航元数据。 */
struct ZzExampleRouteDescriptor final
{
    /** @brief 不本地化的稳定路由标识。 */
    std::string_view routeId;

    /** @brief 使用 UTF-8 保存的默认中文页面标题。 */
    std::string_view title;

    /** @brief 仅在分区首项设置的默认中文分区标题。 */
    std::string_view section;

    /** @brief 页面离开活动状态后的实例保留策略。 */
    ZzPureTools::ZzPageLifetimePolicy lifetime;

    /** @brief 页面在主导航或固定页脚中的位置。 */
    ZzFluentUI::ZzNavigationPlacement placement;
};

/** @brief 提供全进程只读且无动态分配的正式示例路由表。 */
class ZzExampleRouteCatalog final
{
public:
    /** @brief 禁止实例化只读静态路由目录。 */
    ZzExampleRouteCatalog() = delete;

    /** @brief 返回按导航展示顺序排列的十条路由。 */
    [[nodiscard]] static std::span<const ZzExampleRouteDescriptor>
    routes() noexcept;
};

} // namespace ZzExample
