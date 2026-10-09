#pragma once

namespace ZzFluentUI {
class ZzTabBar;
/** @brief 配置选择器专用的固定三分段 Fluent 导航。 */
class ZzColorPickerFluent final
{
public:
    ZzColorPickerFluent() = delete;
    /** @brief 复用 SegmentedWinUI3 装配与原始 14px 字体图标。 */
    static void configureTabs(ZzTabBar *tabs);
};
} // namespace ZzFluentUI
