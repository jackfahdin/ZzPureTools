#include "ZzExampleRouteCatalog.h"

#include <array>

#include <QtCore/QCoreApplication>

namespace ZzExample {

namespace {

using ZzLifetime = ZzPureTools::ZzPageLifetimePolicy;
using ZzPlacement = ZzFluentUI::ZzNavigationPlacement;

constexpr std::array<ZzExampleRouteDescriptor, 11> zzRoutes{{
    {"home", QT_TRANSLATE_NOOP("ZzPureToolsExample", "首页"),
     QT_TRANSLATE_NOOP("ZzPureToolsExample", "工作区"),
     ZzLifetime::Persistent,
     ZzPlacement::Primary},
    {"controls", QT_TRANSLATE_NOOP("ZzPureToolsExample", "基础控件"),
     QT_TRANSLATE_NOOP("ZzPureToolsExample", "控件"),
     ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"cards", QT_TRANSLATE_NOOP("ZzPureToolsExample", "卡片与媒体"), "",
     ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"list-view", QT_TRANSLATE_NOOP("ZzPureToolsExample", "列表视图"),
     QT_TRANSLATE_NOOP("ZzPureToolsExample", "数据视图"),
     ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"table-view", QT_TRANSLATE_NOOP("ZzPureToolsExample", "表格视图"), "",
     ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"tree-view", QT_TRANSLATE_NOOP("ZzPureToolsExample", "树形视图"), "",
     ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"navigation", QT_TRANSLATE_NOOP("ZzPureToolsExample", "导航与历史"),
     QT_TRANSLATE_NOOP("ZzPureToolsExample", "交互"),
     ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"feedback", QT_TRANSLATE_NOOP(
                     "ZzPureToolsExample", "反馈与弹出层"),
     "", ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"icons", QT_TRANSLATE_NOOP("ZzPureToolsExample", "图标"),
     QT_TRANSLATE_NOOP("ZzPureToolsExample", "资源"),
     ZzLifetime::Recreatable,
     ZzPlacement::Primary},
    {"platform", QT_TRANSLATE_NOOP("ZzPureToolsExample", "窗口与平台"),
     QT_TRANSLATE_NOOP("ZzPureToolsExample", "系统"),
     ZzLifetime::Persistent,
     ZzPlacement::Primary},
    {"about", QT_TRANSLATE_NOOP("ZzPureToolsExample", "关于"), "",
     ZzLifetime::Persistent,
     ZzPlacement::Footer},
}};

} // namespace

std::span<const ZzExampleRouteDescriptor>
ZzExampleRouteCatalog::routes() noexcept
{
    return zzRoutes;
}

} // namespace ZzExample
