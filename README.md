# ZzPureToolsPro

ZzPureToolsPro 是面向 Qt 6.8+ 与 C++20 的高性能跨平台 Qt Widgets
组件库和应用框架。项目采用前后端分离架构，当前版本为 `0.1.0`，处于第一期
基础能力完成后的开发阶段。

目标平台为 Linux、Windows 和 macOS。Linux 是当前主要实现与自动验证平台；
GitHub 托管 CI 已产生三个平台的真实诊断结果，但尚无同一提交完整成功的矩阵。
Windows、macOS 以及 Linux 桌面会话的真机交互状态以
[平台支持与验收状态](docs/development/PLATFORM_SUPPORT_ZH.md)为准。

## 组件

| 组件 | CMake 目标 | 职责 |
|---|---|---|
| `ZzCore` | `Zz::Core` | 错误与结果、应用路径、设置、可取消任务和 Qt 日志桥接 |
| `ZzWindowKit` | `Zz::WindowKit` | 对 QWindowKit 的私有适配与无边框窗口状态管理 |
| `ZzFluentUI` | `Zz::FluentFoundation`、`Zz::FluentUI` | Fluent 主题令牌、样式、绘制原语和 Widgets 基础控件 |
| `ZzPureTools` | `Zz::AppCore`、`Zz::PureTools` | 模块生命周期、路由、页面、导航、多窗口和应用装配 |

`ZzFluentUI` 当前公开 37 个可组合组件：

- 基础与布局：`ZzPushButton`、`ZzSplitButton`、`ZzIconButton`、
  `ZzToggleSwitch`、`ZzProgressRing`、`ZzSpinBox`、`ZzDoubleSpinBox`、
  `ZzScrollBar`、`ZzScrollArea`、`ZzFlowLayout`。
- 输入与选择：`ZzPasswordBox`、`ZzKeyBinder`、`ZzColorPicker`、
  `ZzRatingControl`、`ZzSuggestBox`、
  `ZzMultiSelectComboBox`、`ZzRoller`、`ZzRollerPicker`、`ZzCalendar`、
  `ZzCalendarPicker`。
- 导航与内容：`ZzBreadcrumbBar`、`ZzNavigationView`、`ZzNavigationPane`、
  `ZzTabBar`、`ZzTabWidget`、`ZzPivot`、`ZzCarouselView`、`ZzExpander`、
  `ZzDrawer`、`ZzFluentItemDelegate`。
- 反馈与表面：`ZzMessageBar`、`ZzInfoBadge`、`ZzContentDialog`、
  `ZzTeachingTip`、`ZzActionCard`、`ZzImageCard`、`ZzFluentTitleBar`。

标准 Qt Widgets 由应用级 `ZzFluentStyle` 统一提供 Fluent 外观和尺寸契约，
包括 `QCheckBox`、`QRadioButton`、`QSlider`、`QLineEdit`、
`QPlainTextEdit`、`QComboBox`、`QProgressBar`、`QLCDNumber`、
`QListView`、`QTableView`、`QTreeView`、`QMenuBar`、`QToolBar` 和
`QStatusBar`。这些控件继续保留 Qt 的模型、选择、键盘、弹出菜单、无障碍和
RTL 语义，因此不会为了“组件名一一对应”重复创建同义的 `Zz` 包装类。
标准控件的广度合同由 `ZzFluentStandardControlsTest` 和固定尺寸截图场景维护，
画廊中的 `Standard surfaces` 分区只使用本地 Qt model 和固定演示数据。

QWindowKit 类型不会暴露到 Zz 公共 API；UI 组件不直接访问领域模型、数据库、
网络客户端或业务服务。完整依赖规则见
[架构设计](docs/superpowers/specs/2026-08-02-zzpuretoolspro-architecture-design.md)。

## 构建要求

- CMake 3.23 或更高版本。
- Ninja。
- Qt 6.8 或更高版本，包含 Core、Gui、Widgets、Svg、Concurrent 和 Test 模块。
- Linux GCC 13.1+ 或 Clang 17+；Windows MSVC 2022 x64 或 Qt 官方 MinGW-w64；
  macOS Apple Clang 15+。

项目只考虑 Qt 6，不兼容 Qt 5。共享库是默认构建方式，静态库、LTO、
clang-tidy、ASan 和 UBSan 均有独立 CMake Preset。

## Linux 快速开始

先让环境变量指向本机实际工具链和 Qt SDK：

```bash
export QT_ROOT=/path/to/Qt/6.x/gcc_64
export GCC_13=/path/to/gcc
export GXX_13=/path/to/g++

cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug --output-on-failure
cmake --install build/linux-gcc-debug --prefix install/linux-gcc-debug
```

若要构建可交互示例，在配置时启用示例并构建指定目标：

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug \
  --target ZzWindowKitDemo ZzFluentControlsGallery \
  ZzPureToolsDemo ZzPureToolsExample
```

完整 Linux、Windows、MinGW 和 macOS 命令、环境变量及发布配置见
[构建手册](docs/development/BUILDING_ZH.md)。`CMakePresets.json` 保存共享矩阵；
开发者的本机路径应通过环境变量或未跟踪的 `CMakeUserPresets.json` 提供，示例见
[`CMakeUserPresets.json.example`](CMakeUserPresets.json.example)。

## 示例

- `ZzWindowKitDemo`：无边框窗口适配、标题栏命中区域和窗口控制。
- `ZzFluentFoundationDemo`：主题快照与基础令牌。
- `ZzFluentControlsGallery`：浅色、深色、高对比度和基础控件交互。
- `ZzPureToolsDemo`：模块、页面注册、导航和多窗口应用流程。
- `ZzPureToolsExample`：十二个集成页面、窗口壳层、双向导航、主题、Dock、
  多窗口和关闭守卫的完整桌面应用入口。

示例只演示组件契约，不承载业务逻辑。基础装配可从
[`examples/ZzPureToolsDemo/main.cpp`](examples/ZzPureToolsDemo/main.cpp)开始阅读；
完整集成入口位于
[`examples/ZzPureToolsExample/main.cpp`](examples/ZzPureToolsExample/main.cpp)。

## 质量与平台状态

常规 CTest 覆盖单元测试、公共头独立编译、架构边界、安装消费、包重定位、
二进制依赖和发布合规契约。本机 Linux runner 还覆盖 GCC/Clang、
shared/static、LTO、clang-tidy、ASan+UBSan、五示例冒烟和十二项性能回归；
Windows 与 macOS runner 会在 shared/static 组合中编译全部示例。

`.github/workflows/ci.yml` 已定义 Ubuntu、Windows MSVC、Windows Qt MinGW、
macOS arm64 与 macOS x86_64 托管矩阵，但托管 CI 不替代真实窗口系统和设备上的
人工验收。首次上传后的处理方式见
[GitHub Actions 手册](docs/development/GITHUB_ACTIONS_ZH.md)。

## 开发资料

- [中文编码规范](docs/development/CODING_STANDARD_ZH.md)
- [图标资源所有权与维护记录](docs/development/ICON_ASSETS_ZH.md)
- [性能基线与测量规则](docs/performance/PERFORMANCE_BASELINE_ZH.md)
- [Windows 人工验收清单](docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md)
- [macOS 人工验收清单](docs/release/MANUAL_MACOS_CHECKLIST_ZH.md)
- [Linux 人工验收清单](docs/release/MANUAL_LINUX_CHECKLIST_ZH.md)
- [第三方软件通知](docs/third-party/THIRD_PARTY_NOTICES.md)
- [发布阻断项](docs/third-party/RELEASE_BLOCKERS_ZH.md)

## 许可证

ZzPureToolsPro 采用 [MIT License](LICENSE)，版权所有者为 Jackfahdin。
第三方组件继续适用各自许可证；来源、版本、构建期工具边界和安装通知以
[第三方软件通知](docs/third-party/THIRD_PARTY_NOTICES.md)为准。
