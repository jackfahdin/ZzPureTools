# 标题栏系统按钮与主题交互设计

**状态：** 已确认，进入实现阶段

## 目标

为 `ZzFluentTitleBar` 提供完整的应用级标题栏命令视觉层：窗口图标、菜单、标题、主题、置顶、最小化、最大化/还原和关闭。标题栏只维护视觉状态并发出用户意图，宿主应用负责执行窗口和主题变更。

主题按钮同时支持两种交互模式，客户端可以按使用场景选择：

1. `Menu`：点击按钮打开主题菜单，用户可选择跟随系统、浅色、深色或高对比度。
2. `Toggle`：点击按钮只请求在浅色与深色之间切换，适合希望快速切换的普通应用。

默认使用 `Menu`，保留系统和高对比度等完整能力；客户端可以将属性设置为 `Toggle`，无需替换标题栏或自行实现按钮。

## 架构与边界

`ZzFluentTitleBar` 是意图层，不直接调用 `QWindow`、平台 API、`ZzThemeController` 或无边框适配层。主题和置顶状态通过现有的“请求/确认”信号往返：用户操作发出请求，宿主执行成功后调用 `setThemeMode()` 或 `setAlwaysOnTop()` 回写确认状态。

新增公开枚举 `ZzTitleBarThemeInteractionMode`，并以 Qt 属性暴露：

```cpp
enum class ZzTitleBarThemeInteractionMode : std::uint8_t
{
    Menu,
    Toggle
};
```

`ZzFluentTitleBar` 新增：

```cpp
Q_PROPERTY(
    ZzFluentUI::ZzTitleBarThemeInteractionMode themeInteractionMode
    READ themeInteractionMode
    WRITE setThemeInteractionMode
    NOTIFY themeInteractionModeChanged)

[[nodiscard]] ZzTitleBarThemeInteractionMode themeInteractionMode() const noexcept;
void setThemeInteractionMode(ZzTitleBarThemeInteractionMode mode);
[[nodiscard]] QMenu *themeMenu() const noexcept;
```

并新增两个信号：

```cpp
void themeInteractionModeChanged(ZzTitleBarThemeInteractionMode mode);
void themeToggleRequested();
```

`themeToggleRequested()` 不携带目标模式。宿主依据主题控制器当前的 `resolvedMode()` 决定浅色或深色目标，避免标题栏错误解释 `System` 或高对比度状态。

## 主题菜单行为

菜单由标题栏创建并长期复用，不在每次点击时重新分配。菜单包含四个互斥 action，数据字段保存 `ZzThemeMode`：

| action | `ZzThemeMode` | 图标 |
| --- | --- | --- |
| 跟随系统 | `System` | `ComputerSystem.svg` |
| 浅色 | `Light` | `Moon.svg` |
| 深色 | `Dark` | `Sun.svg` |
| 高对比度 | `HighContrast` | `ComputerSystem.svg` |

选择 action 只发出已有的 `themeModeRequested(ZzThemeMode)`；确认状态变化后同步 action 勾选状态和按钮图标/可访问名称。切换到 `Toggle` 模式时菜单仍保留，可通过 `themeMenu()` 由客户端隐藏或扩展，但按钮点击改为发出 `themeToggleRequested()`。

## 图标与性能

标题栏不再绘制手工线条图标，统一使用 `ZzIconDescriptor::fromBundledSvg()` 描述 `ZzFluentUI/resources/icons/` 中的内嵌 SVG，并优先通过宿主 `ZzFluentStyle::iconPixmap()` 渲染。该路径复用 SVG 形状缓存、颜色缓存、DPI 量化和 RTL 支持；只在应用尚未安装 `ZzFluentStyle` 时使用同一 SVG 资源的轻量回退着色。

状态到图标的映射：置顶使用 `Pin.svg`/`PinFill.svg`，主题使用 `ComputerSystem.svg`、`Moon.svg`、`Sun.svg`，最小化使用 `Minimize.svg`，最大化/还原使用 `Maximize.svg`/`Restore.svg`，关闭使用 `Close.svg`。

## 其他窗口命令

最小化、最大化/还原和关闭继续只发出 `minimizeRequested()`、`maximizeRestoreRequested()`、`closeRequested()`。置顶按钮发出 `alwaysOnTopRequested(bool)`，标题栏只回写宿主确认的状态。按钮可见性、标题居中安全区、菜单自适应折叠和无边框命中区契约保持不变。

## 测试策略

在 `ZzFluentTitleBarTest` 中先增加失败测试，覆盖：默认和可切换的交互模式、模式变更信号、菜单四个主题 action、菜单模式请求、切换模式请求、主题/置顶/最大化图标状态、SVG 图标非空及调色板换色，以及原有系统按钮意图信号不变。通过测试后运行标题栏测试目标、Example smoke 和对应 CTest 集合。

