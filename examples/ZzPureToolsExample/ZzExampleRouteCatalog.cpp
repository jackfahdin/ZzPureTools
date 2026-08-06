#include "ZzExampleRouteCatalog.h"

#include <array>

namespace ZzExample {

namespace {

using ZzLifetime = ZzPureTools::ZzPageLifetimePolicy;
using ZzPlacement = ZzFluentUI::ZzNavigationPlacement;

constexpr std::array<ZzExampleRouteDescriptor, 12> zzRoutes{{
    {"home", "首页", "工作区", ZzLifetime::Persistent,
     ZzPlacement::Primary},
    {"controls", "基础控件", "控件", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"cards", "卡片与媒体", "", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"list-view", "列表视图", "数据视图", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"table-view", "表格视图", "", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"tree-view", "树形视图", "", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"navigation", "导航与历史", "交互", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"feedback", "反馈与弹出层", "", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"icons", "图标", "资源", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"platform", "窗口与平台", "系统", ZzLifetime::Persistent,
     ZzPlacement::Primary},
    {"settings", "设置", "", ZzLifetime::Persistent,
     ZzPlacement::Footer},
    {"about", "关于", "", ZzLifetime::Persistent,
     ZzPlacement::Footer},
}};

} // namespace

std::span<const ZzExampleRouteDescriptor>
ZzExampleRouteCatalog::routes() noexcept
{
    return zzRoutes;
}

} // namespace ZzExample
