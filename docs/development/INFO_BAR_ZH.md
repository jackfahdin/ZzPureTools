# 信息栏

`ZzInfoBar` 和 `ZzInfoBarHost` 迁移自 FluentUIStyle 的 `ExInfoBar`、`ExInfoBarHost`。Qt Widgets 没有原生 InfoBar 类；此实现通过 QWidget、标签、按钮、布局、QPainter 和动画组合完成，仅依赖 Qt 公共 API。

示例入口为“自定义控件 → 信息栏(InfoBar)”，位于时间轴之后。旧 `ZzMessageBar` 及其 `closeRequested` 仅表示关闭意图的契约保留。

## 页面内信息栏

```cpp
#include <ZzFluentUI/ZzInfoBar.h>

auto *bar = new ZzFluentUI::ZzInfoBar(parent);
bar->setSeverity(ZzFluentUI::ZzInfoBar::Success);
bar->setTitle(tr("保存成功"));
bar->setMessage(tr("所有更改均已保存。"));
layout->addWidget(bar);
bar->setOpen(true);
```

信息、成功、警告、错误四种级别沿用参考配色。状态图标与关闭图标直接按参考 QPainter 几何绘制，没有另换 SVG 或字体资源。宽度不足时，标题、消息和操作区域自动改为纵向排列；支持从右向左布局。

默认初始关闭。`setOpen(true)` 展开，`dismiss()` 收起，内建关闭按钮自动收起。`openChanged` 表示目标状态变化，`opened` 和 `closed` 表示过渡完成。默认动画 167 ms，支持关闭动画及系统减少动效。

设置 `actionButtonText` 可展示内建操作按钮，点击后发出 `actionTriggered`。`setActionWidget` 接管自定义操作控件，并销毁被替换的旧控件；`takeActionWidget` 解除父对象关系，将所有权交回调用方。

## 窗口通知

```cpp
#include <ZzFluentUI/ZzInfoBarHost.h>

auto *host = new ZzFluentUI::ZzInfoBarHost(window, page);
host->showInfoBar(ZzFluentUI::ZzInfoBar::Informational,
    tr("更新"), tr("新版本已经可以下载。"),
    ZzFluentUI::ZzInfoBarHost::TopRight, 4500);
```

通知是目标窗口内的子控件。支持左上、顶部、右上、左下、底部、右下六个位置，各自按顺序排队，空间不足时等待。显示后开始计时，鼠标悬停或目标隐藏时暂停。超时为 0 时保持显示，负数使用 Host 默认值 4500 ms。

默认窗口边距 24、通知间距 8、最大宽度 360（最小允许值 160）。`dismissAll()` 关闭活动及等待通知，带位置参数时只关闭该位置。Host 接管的信息栏关闭后会销毁，长期保存返回值应使用 `QPointer`。

示例以当前窗口为目标、当前页面为所有者，关闭页面时清理通知，不替换应用的默认 Host。页面包含四级内嵌示例、重新打开、六位置弹出、连续十条、批量关闭，以及内容、行为和通知参数编辑。

## 验证

自动验证入口：`fluent.info-bar`、`fluent.info-bar-host`、`fluent.message-bar`、示例集成/英文/工作区烟测、公共头与架构检查。截图覆盖浅色、深色、高对比主题和 100%、125%、150%、200% 缩放。

架构脚本目前仍会报告迁移组件已有的嵌套枚举命名、辅助实现文件与视觉常量问题；本次保留参考 API 的 `Severity`、`Position`，以及示例现有的共享 Showcase 页面组织方式，因此不能将全仓库架构审计标记为通过。

真实参考对比使用相同字体、文本和 palette，在 `build/info-bar-reference` 输出浅深两组并排截图。当前验收环境为 Linux GCC / Qt 6.11.1；Windows/MSVC 需要在对应环境构建验证。
