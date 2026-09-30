# TabBar 外观迁移

目标：将 FluentUIStyle 的九种 TabBar 外观和交互迁入库层，使用 Qt 6.8+ 公开 API。

## 设计

`ZzTabBarAppearance` 提供 Standard、Capsule、PivotGrow、PivotSlide、PivotStretch、Pill、SegmentedSlide、SegmentedFade、SegmentedWinUI3、Navigation。Standard 保持已有工作区外观。`ZzTabBar::setAppearance()` 与 `ZzControlAppearance::setTabBarAppearance()` 共用实现，后者也支持原生 QTabBar / QTabWidget 的标签栏。

分段外形与颜色通过类型化接口配置，颜色可分别指定浅色、深色；无效值恢复主题默认，高对比度优先系统调色板。新外观使用独立私有绘制和动画模块，不改变标签数据、页面所有权、关闭信号和工作区事务。

动画由选择信号驱动，快速切换从当前显示状态继续；布局、隐藏、主题变化和减少动态效果使动画就位。尺寸随字体、图标和标签按钮增长，Navigation 在纵向仍保持文字水平。滚动、RTL、隐藏标签均依据公开 tabRect 计算。

## 实现与验证计划

- [x] 添加运行时切换、Navigation 几何、纯图标、自定义颜色与动画中断测试，确认旧实现无法满足。
- [x] 增加公开枚举、颜色值类型及原生/封装标签栏接口；实现私有样式、动画和尺寸分派。
- [x] 示例提供九种可交互外观、纵向导航、纯图标和自定义分段颜色。
- [x] 同 Qt、字体渲染参考与当前浅深色对比，检查图片。
- [x] 执行 Tab、选择指示条、工作区、截图和示例回归；审查后补齐使用说明。

## 使用

应用先安装 `ZzFluentStyle`。封装控件和原生控件使用同一绘制实现：

```cpp
#include <ZzFluentUI/ZzTabBar.h>
#include <ZzFluentUI/ZzControlAppearance.h>

auto *bar = new ZzFluentUI::ZzTabBar(parent);
bar->setAppearance(ZzFluentUI::ZzTabBarAppearance::SegmentedSlide);
bar->setSegmentedRounded(true);
bar->addTab(QStringLiteral("概览"));
bar->addTab(QStringLiteral("设置"));

// QTabWidget 也可通过其公开 tabBar() 使用这些外观。
ZzFluentUI::ZzControlAppearance::setTabBarAppearance(
    nativeTabWidget->tabBar(), ZzFluentUI::ZzTabBarAppearance::PivotStretch);

// 纵向导航仍绘制水平文字；位置和伸展行为由调用方明确设置。
bar->setAppearance(ZzFluentUI::ZzTabBarAppearance::Navigation);
bar->setShape(QTabBar::RoundedWest);
bar->setExpanding(false);
```

`setSegmentedColors(light, dark)` 接受 `ZzTabBarColors`，字段为 `background`、`selected`、`hover`、`pressed`、`text`、`selectedText`。深色未指定的字段回退浅色，浅色也未指定则使用主题默认值。`setSegmentedColors({})` 清除全部覆盖。自定义选中色可自动计算黑/白前景；高对比度忽略覆盖以保持系统配色。

原生标签栏对应 `ZzControlAppearance::setTabBarColors()`、`setTabBarRounded()`。图标、提示、无障碍名称、关闭按钮、隐藏标签、滚动、方向键等沿用 QTabBar 公开 API。纯图标标签应设置 `setAccessibleTabName()` 和 `setTabToolTip()`。

示例沿用 FluentUIStyle 的原始 `Segoe Fluent Icons.ttf`，通过
`ZzSegoeIconFont::galleryIcon(ZzSegoeIcon::Home)` 保留原版 25 px 字形 / 30 px 画布
缩到 16 px 的比例；Pivot 单独采用 22/22 px。普通主题保持原版黑/白图标，
高对比度使用标签前景色。通用 `icon()` 仍提供跟随标签前景的图标，
选中、禁用及自定义分段配色均直接使用最终颜色，透明度只应用一次。
显式传入 `QColor` 的字体图标和调用方提供的普通 `QIcon` 保持原色。
资源来源和 SHA-256 见 [图标资源记录](ICON_ASSETS_ZH.md)。

独立 `ZzTabBar` 的新建按钮默认隐藏，由 `ZzTabWidget` 宿主接管并显示，避免覆盖第一个标签。新外观不自动修改 movable、tabsClosable、shape、expanding 或当前页面。示例入口为“基础控件 → 标签栏(TabBar)”。

## 实现取舍与验证

参考实现中的外观和配色迁入私有模块，动画使用公开 `QVariantAnimation`，无需 Qt 私有头文件或在绘制中保存字符串状态。PivotStretch 前 66% 在原位置伸长，后 34% 在目标位置回收并轻微回弹，快速切换以当前矩形重定向；SegmentedFade 保留各项当前透明度，避免连续点击跳回完整旧选中块。

示例页对照原始 `PageTab` 保留五组卡片和 13 个标签栏，包括三组自定义分段标签、
可关闭的 Pill、带彩色内容页的 Capsule，以及 220 ms OutCubic 纵向内容切换。
Pivot 使用 15 px 粗体；普通图文间距 4 px，WinUI3 专用间距 6 px，Navigation 前导 5 px。
关闭按钮使用同一字体的单色叉号。

针对 Qt 拖动标签的局部绘制矩形，使用 `QStyleOptionTab::tabIndex` 与坐标偏移。纵向 RTL 与 Qt 按钮布局保持一致，并为非方形 tabButton 预留正确跨轴空间；更换样式时回收事件观察和动画对象。

本地验证环境为 Linux / GCC 15 / Qt 6.11.1，未宣称 Windows 或 Qt 6.8 实机验证。浅色、深色、高对比度各保留 100%、125%、150%、200% 的截图基线。验证命令：

```sh
cmake --build build/linux-gcc-debug --target ZzTabControlsTest ZzFluentScreenshotTest ZzPureToolsExample
ctest --test-dir build/linux-gcc-debug --output-on-failure \
  -R '^(fluent\.(tab-controls|selection-indicator|workspace-transfer-registry-private|workspace-transfer-registry-lifetime|workspace-cross-transfer|split-workspace|screenshot-(100|125|150|200))|example\.(puretools-integration(-english)?|workspace-smoke))$' -j 4
```

参考项目只读。直接编译原始 `PageTab` 与当前示例源码、使用相同 Qt/DejaVu Sans 字体的浅深色对比保存在本地 `build/tab-bar-details/`，示例截图在 `build/tab-bar-preview/`。
