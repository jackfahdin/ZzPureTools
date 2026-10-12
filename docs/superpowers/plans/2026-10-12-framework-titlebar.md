# 框架标题栏统一实现计划

**目标：** 框架默认提供 Example 的单击浅深切换与置顶行为，Example 直接使用；删除主题菜单 API，不保留兼容层。

**架构：** `ZzFluentTitleBar` 保持纯视觉组件，只发送切换意图。`ZzApplicationWindow` 执行主题与窗口命令，并通过 `themeToggled` 通知发起窗口的业务层保存设置。QWindowKit 的 chrome 注册保持不变。

**技术栈：** Qt Widgets、QtTest、QWindowKit、CMake。

## 1. 行为回归

- [x] 在 `ZzFluentUI/tests/ZzFluentTitleBarTest.cpp` 添加默认无菜单、点击只发意图的测试。
- [x] 在 `ZzPureTools/tests/ZzMultiWindowIsolationTest.cpp` 添加无工作区的主题切换和窗口置顶隔离测试，并注册到 CTest。
- [x] 编译并运行，确认三项分别因菜单仍存在、主题未切换、置顶未生效而失败。

## 2. 统一默认实现

- [x] 从 `ZzFluentTitleBar.h/.cpp` 和私有实现删除菜单属性、菜单动作及 `ZzTitleBarThemeInteractionMode.h`；点击始终发送 `themeToggleRequested()`。
- [x] `ZzApplicationWindow` 增加 `toggleTheme()`，按 `resolvedMode() == Dark ? Light : Dark` 切换后发送 `themeToggled(mode)`；基础窗口绑定置顶命令并同步确认状态。
- [x] `ZzWorkspaceShellPrivate.cpp` 跳过框架窗口已经绑定的标题栏，普通 `QMainWindow` 或独立标题栏仍由工作区绑定置顶意图。
- [x] Example 删除标题栏模式配置、重复主题同步和切换算法；菜单调用 `window->toggleTheme()`，`themeToggled` 负责写入 `appearance/themeMode` 和显示结果。
- [x] 删除 Settings 隐藏主题按钮的旧菜单信号连接，保留设置页主题选择器。
- [x] 更新旧菜单测试，覆盖共享主题同步、发起窗口通知隔离、确认状态及设置持久化。

## 3. 验证与交付

- [x] 构建 `ZzFluentTitleBarTest`、`ZzMultiWindowIsolationTest`、`ZzWorkspaceShellTest`、`ZzExampleWorkspaceSmokeTest`、`ZzPureToolsExample`。
- [x] 执行标题栏、多窗口、工作区、Example 冒烟及相关截图测试；检查删除公开头文件后的契约。
- [x] 独立只读审查未发现阻塞问题，更新 `docs/development/CONTROL_ACCENT_ZH.md` 接入说明。

验证环境：Linux / GCC 15.2 / Qt 6.11.1，`linux-gcc-benchmarks` Release + LTO。

- 完整构建通过；最终补充测试后的定向构建通过。
- `ctest --preset linux-gcc-benchmarks -R 'fluent.title-bar|puretools.multi-window.|puretools.workspace-shell|example.workspace-smoke|screenshot' --output-on-failure`：28/28 通过，包含 12 项截图；未更新截图基线。
- 最终补测视图菜单及收敛工作区绑定条件后，上述非截图测试复跑 16/16 通过。
- `architecture.public-headers`、`architecture.boundaries`、`architecture.complete-audit`：3/3 通过。
- 额外执行的 `architecture.documentation-audit` 失败：检查仍要求 README 包含旧句「尚无同一提交完整成功的矩阵」，而 HEAD 的 README 已更新为历史 CI 与当前状态分开说明。README 与该检查脚本本轮均未修改，此问题独立于标题栏改动。
- Windows 原生桌面交互未在本机验证。

QWindowKit 确认：`ZzQWindowKitBackend::configureChrome` 调用 `setTitleBar` 和 `setSystemButton(WindowIcon/Minimize/Maximize/Close)`。按钮及 SVG 来自 FluentUI，点击由框架窗口连接到 Qt 窗口命令。保留低层 `ZzWindowKitDemo` 的独立演示，不给它引入 FluentUI 依赖。

不改性能门限、历史性能基线和 `docs/research/`。本轮不自动提交或推送。
