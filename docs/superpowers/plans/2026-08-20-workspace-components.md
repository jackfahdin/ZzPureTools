# 通用工作区组件实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度；每个逻辑任务完成后立即创建中文 commit。

**目标：** 在 `ZzFluentUI` 和 `ZzPureTools` 中交付可独立安装的标题栏、活动侧栏、侧面板、资源浏览、标签工作区、命令面板、停靠面板和工作区协调器，并用 `ZzPureToolsExample` 组成 SSH 风格但不依赖 SSH 业务的完整场景。

**架构：** Fluent 控件只依赖 Qt 公共 Widgets/Model-View 协议和 `ZzFluentFoundation`；`ZzWorkspaceShell` 位于 `ZzPureTools`，协调 Fluent 控件、Qt 原生 `QMainWindow/QDockWidget` 和现有 `ZzWindowAgent`，但不替宿主设置 central widget。Example 仅提供本地展示模型、`QAction`、内容页面和业务意图处理。

**技术栈：** Qt 6.8+（Linux 主验证使用 Qt 6.11.1）、C++20、CMakeLists.txt 与 CMakePresets.json、Qt Test、Model/View、QMainWindow/QDockWidget、现有 ZzCore::ZzResult、ZzWindowKit、ZzFluentStyle、ZzPerformanceReporter。

---

## 文件清单与职责

### 公共枚举和值类型

- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzTitleBarMenuDisplayMode.h`：标题栏 Expanded/Compact/Adaptive 枚举及中文 Doxygen。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzActivityArea.h`：左/右主区和次区枚举。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzActivityItemRole.h`：Activity 模型 Area、Badge 角色定义。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzSidePaneEdge.h`：Side Pane 左右边缘枚举。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzCommandItemRole.h`：命令关键词、快捷键、分组、优先级角色。
- 创建 `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspacePanelId.h` 与 `ZzPureTools/widgets/src/ZzWorkspacePanelId.cpp`：非空、trim 后稳定的面板 ID、比较、`qHash` 和 QVariant 元类型。
- 创建 `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceTitleMode.h`：Application、CurrentTab、CurrentTabAndApplication、Custom 枚举。

### Fluent 控件

- 修改 `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentTitleBar.h`、`ZzFluentUI/widgets/src/ZzFluentTitleBar.cpp`、`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.h`、`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp`：非 native 自适应菜单、全窗口中心标题、主题/置顶意图、无障碍状态和 `hitTestVisibleWidgets()`。
- 创建 `ZzFluentUI/widgets/include/ZzFluentUI/ZzActivityBar.h`、`ZzFluentUI/widgets/src/ZzActivityBar.cpp`、`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.h`、`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp`：两个固定 `QListView` 投影主/次 Activity、键盘交互、badge 和进程内拖拽意图。
- 创建 `ZzFluentUI/widgets/include/ZzFluentUI/ZzSidePane.h`、`ZzFluentUI/widgets/src/ZzSidePane.cpp`、`ZzFluentUI/widgets/src/private/ZzSidePanePrivate.h`、`ZzFluentUI/widgets/src/private/ZzSidePanePrivate.cpp`：标题、`QStackedWidget` 页面所有权、折叠、宽度钳制和拖拽把手。
- 创建 `ZzFluentUI/widgets/include/ZzFluentUI/ZzExplorerPane.h`、`ZzFluentUI/widgets/src/ZzExplorerPane.cpp`、`ZzFluentUI/widgets/src/private/ZzExplorerPanePrivate.h`、`ZzFluentUI/widgets/src/private/ZzExplorerPanePrivate.cpp`：工具栏、搜索框、递归过滤 proxy、源索引映射、60 ms 持久定时器和模型生命周期收敛。
- 创建 `ZzFluentUI/widgets/include/ZzFluentUI/ZzCommandPalette.h`、`ZzFluentUI/widgets/src/ZzCommandPalette.cpp`、`ZzFluentUI/widgets/src/private/ZzCommandPalettePrivate.h`、`ZzFluentUI/widgets/src/private/ZzCommandPalettePrivate.cpp`：覆盖式搜索、缓存排序、键盘导航、焦点恢复和激活意图。
- 创建 `ZzFluentUI/widgets/include/ZzFluentUI/ZzDockPanel.h`、`ZzFluentUI/widgets/src/ZzDockPanel.cpp`、`ZzFluentUI/widgets/src/private/ZzDockPanelPrivate.h`、`ZzFluentUI/widgets/src/private/ZzDockPanelPrivate.cpp`：`QDockWidget` 的 Fluent 标题栏、图标、内容转移和原生浮动/停靠协议。
- 修改 `ZzFluentUI/widgets/include/ZzFluentUI/ZzTabWidget.h`、`ZzFluentUI/widgets/src/ZzTabWidget.cpp`、`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.h`、`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.cpp`、`ZzFluentUI/widgets/include/ZzFluentUI/ZzTabBar.h`、`ZzFluentUI/widgets/src/ZzTabBar.cpp`、`ZzFluentUI/widgets/src/private/ZzTabBarPrivate.h`、`ZzFluentUI/widgets/src/private/ZzTabBarPrivate.cpp`：固定/脏/注意/可关闭状态、新建意图、批量关闭、标题同步和事务元数据转移。
- 修改 `ZzFluentUI/CMakeLists.txt`：注册新增源文件、MOC 头、基础枚举头和安装导出清单。

### PureTools 工作区协调

- 创建 `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`、`ZzPureTools/widgets/src/ZzWorkspaceShell.cpp`、`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`、`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`：工厂校验、workspace root、Activity/Side/Tab/Command/Dock 装配、面板 ID 注册、标题策略、置顶回写和布局编解码回滚。
- 修改 `ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp`：将标题栏 `hitTestVisibleWidgets()` 写入 WindowKit Chrome 配置，并保留现有页面路由装配顺序。
- 修改 `ZzPureTools/CMakeLists.txt`：加入工作区源文件、公共导出头和 `Zz::FluentUI` 的私有依赖检查。
- 修改 `ZzWindowKit/include/ZzWindowKit/ZzWindowChromeConfiguration.h`、`ZzWindowKit/src/private/ZzWindowAgentPrivate.cpp`、`ZzWindowKit/tests/ZzWindowAgentTest.cpp`：验证新增命中测试控件集合，禁止重复、空指针、跨线程和非标题栏后代。

### 测试、截图、安装和架构门禁

- 修改 `ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`：菜单迟滞、中心标题不重叠、动态 Action、RTL、主题/置顶意图和命中控件。
- 创建 `ZzFluentUI/tests/ZzActivityBarTest.cpp`、`ZzFluentUI/tests/ZzSidePaneTest.cpp`、`ZzFluentUI/tests/ZzExplorerPaneTest.cpp`、`ZzFluentUI/tests/ZzCommandPaletteTest.cpp`、`ZzFluentUI/tests/ZzDockPanelTest.cpp`：分别覆盖模型投影、所有权、100k 节点过滤、命令排序、键盘/焦点、拖拽拒伪造、原生 Dock 合同和对象数量稳定。
- 修改 `ZzFluentUI/tests/ZzTabControlsTest.cpp`：覆盖固定/脏/注意状态、批量关闭、标题入口、跨容器转移和失败回滚。
- 修改 `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`：增加 titlebar、Activity/Side Pane、Command Palette、Dock/Tab 场景，生成 Light/Dark/HighContrast 与 DPR 100/125/150/200 基线。
- 创建 `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`：覆盖工厂输入、ID 冲突、所有权 take、标题模式、置顶、注册顺序、布局校验/未知 ID/回滚和宿主提前销毁。
- 修改 `ZzPureTools/tests/CMakeLists.txt`、`ZzFluentUI/tests/CMakeLists.txt`、`ZzWindowKit/tests/CMakeLists.txt`：接入测试目标、CTest 标签、offscreen 环境和严格警告/ sanitizer。
- 修改 `tests/InstallConsumer/Gui/main.cpp`、`tests/InstallConsumer/CMakeLists.txt`、`tests/PublicHeaderConsumer/CMakeLists.txt`：安装后的 shared/static 包实例化所有新增公共头和最小工作区。
- 修改 `tests/Architecture/ZzArchitectureAudit.cmake`、`tests/Architecture/CMakeLists.txt`：禁止工作区组件依赖 SSH/网络/设置/Qt Private、stylesheet、裸色和每行 QWidget，并允许唯一的 `ZzApplicationWindowPrivate.cpp` 组合入口。

### Example 与性能

- 创建 `examples/ZzPureToolsExample/ZzExampleSessionModel.h`、`examples/ZzPureToolsExample/ZzExampleSessionModel.cpp`：平面会话展示模型，提供固定本地 SSH 风格数据，不包含连接或终端逻辑。
- 创建 `examples/ZzPureToolsExample/ZzExampleWorkspaceContent.h`、`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.cpp`：生成终端、SFTP、日志、属性和任务占位页面，并将页面标题交给 `ZzTabWidget`。
- 修改 `examples/ZzPureToolsExample/ZzExampleWindowShell.h`、`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.h`、`examples/ZzPureToolsExample/ZzExampleWindowShell.cpp`、`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`：删除 Activity/Dock/命令/标题同步内部算法，改为调用 `ZzWorkspaceShell` 公共接口；保留导航、关闭守卫、主题 presenter 和活动日志策略。
- 修改 `examples/ZzPureToolsExample/main.cpp`、`examples/ZzPureToolsExample/CMakeLists.txt`；创建 `examples/ZzPureToolsExample/tests/CMakeLists.txt`：注册公开工作区模型、内容页面、命令 QAction，增加 smoke 验收单击、Tab、命令、Dock、标题和布局 round-trip。
- 创建 `benchmarks/ZzWorkspaceComponentsBenchmark.cpp`：按固定输入记录菜单切换、Activity、100k Explorer 查询、200 Tab、10k 命令、64 Side Panel/32 Dock 布局和 workspace render 的 P50/P95/最大值、QObject 数、样式缓存和 RSS，初始报告使用 `observe`。
- 修改 `CMakeLists.txt`、`benchmarks/CMakeLists.txt`、`tests/Platform/ZzPerformanceThresholdContract.cmake`、`tests/Platform/PresetMatrixContract.cmake`：接入 `benchmark.workspace-components`，不提高既有正式阈值，只加入结构性对象泄漏失败条件。
- 修改 `docs/development/BUILDING_ZH.md`、`docs/development/PLATFORM_SUPPORT_ZH.md`、`docs/performance/PERFORMANCE_BASELINE_ZH.md`、`docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`：记录 Linux 实际结果以及 Windows MSVC、Windows MinGW、macOS 的源码/静态检查状态，未执行的平台明确写“未执行”。

---

## 实施任务

### 任务 1：标题栏增强与 WindowKit 命中测试

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentTitleBar.h`、`ZzFluentUI/widgets/src/ZzFluentTitleBar.cpp`、`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.h`、`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp`、`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzWindowKit/include/ZzWindowKit/ZzWindowChromeConfiguration.h`、`ZzWindowKit/src/private/ZzWindowAgentPrivate.cpp`、`ZzWindowKit/tests/ZzWindowAgentTest.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp`
- 测试：`ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`

- [ ] **步骤 1：编写失败测试。** 在标题栏测试中创建 1200 px 和 520 px 宽度窗口，断言 Adaptive 模式分别显示横向 `QMenuBar` 和折叠菜单；加入 ActionAdded/Removed 后菜单仍使用原 QAction；设置长标题，断言标题矩形位于窗口中心安全区域内；发出主题/置顶请求后只收到信号；RTL 下左右交互区镜像。向 WindowAgent 测试加入菜单、主题和置顶控件的命中集合校验。
- [ ] **步骤 2：运行并确认失败。** 运行 `cmake --build --preset linux-gcc-debug --target ZzFluentTitleBarTest ZzWindowAgentTest --parallel 2 && ctest --preset linux-gcc-debug -R 'fluent.title-bar|window-agent' --output-on-failure`；预期编译失败，原因是新枚举、菜单 API、请求信号和 `hitTestVisibleWidgets()` 尚不存在。
- [ ] **步骤 3：实现最少代码。** 增加非 native `QMenuBar` 和稳定折叠菜单按钮；只在 ActionAdded、ActionRemoved、LanguageChange、尺寸变化时重建顶层菜单投影；按整个标题栏中心计算标题安全矩形并用 `QFontMetrics::elidedText()` 截断；增加主题/置顶状态和请求信号、tooltip、accessibleName、checked；WindowKit 配置验证新集合并在 `ZzApplicationWindowPrivate` 接线。
- [ ] **步骤 4：验证通过。** 运行同一构建和 CTest 命令，再执行 `ctest --preset linux-gcc-debug -R 'fluent.title-bar|window-agent' --output-on-failure`；预期所有断言通过且重复 resize 不增加菜单 QObject。
- [ ] **步骤 5：Commit。**
  ```bash
  git add ZzFluentUI ZzWindowKit ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp
  git commit -m "组件：增强自适应标题栏与窗口命中测试"
  ```

### 任务 2：Activity Bar 与 Side Pane

**文件：**
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzActivityArea.h`、`ZzFluentUI/foundation/include/ZzFluentUI/ZzActivityItemRole.h`、`ZzFluentUI/foundation/include/ZzFluentUI/ZzSidePaneEdge.h`
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzActivityBar.h`、`ZzFluentUI/widgets/src/ZzActivityBar.cpp`、`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.h`、`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp`
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSidePane.h`、`ZzFluentUI/widgets/src/ZzSidePane.cpp`、`ZzFluentUI/widgets/src/private/ZzSidePanePrivate.h`、`ZzFluentUI/widgets/src/private/ZzSidePanePrivate.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 测试：`ZzFluentUI/tests/ZzActivityBarTest.cpp`、`ZzFluentUI/tests/ZzSidePaneTest.cpp`

- [ ] **步骤 1：编写失败测试。** 用自定义 `QAbstractListModel` 提供四个 Area、badge、禁用行和可拖动行；断言左右 Bar 只显示本侧主/次投影，点击当前项发出折叠、点击其他项发出激活，键盘 Home/End/Up/Down/Enter/Space 顺序稳定，伪造 MIME 被拒绝，模型销毁后不访问索引。为 SidePane 编写页面接管、take、折叠恢复宽度、最小/最大钳制、左右把手和页面销毁测试。
- [ ] **步骤 2：运行并确认失败。** 运行 `cmake --build --preset linux-gcc-debug --target ZzActivityBarTest ZzSidePaneTest --parallel 2 && ctest --preset linux-gcc-debug -R 'activity-bar|side-pane' --output-on-failure`；预期因目标头和实现缺失而失败。
- [ ] **步骤 3：实现最少代码。** Activity 使用两个固定 `QListView`、一个复用 delegate 和源索引映射，不创建每项 QWidget；拖拽 MIME 使用随机令牌且只发出 move 意图，Shell 之外不修改模型。SidePane 使用固定标题区、`QStackedWidget` 和 4 px 把手，所有权只经 add/take 变更，宽度拖动为 O(1)。
- [ ] **步骤 4：验证通过。** 运行定向 CTest，并用 `QT_QPA_PLATFORM=offscreen` 运行 `ZzActivityBarTest` 与 `ZzSidePaneTest`；记录 500 行模型下重复激活前后 `QObject` 数相等。
- [ ] **步骤 5：Commit。**
  ```bash
  git add ZzFluentUI
  git commit -m "组件：新增活动栏与侧面板"
  ```

### 任务 3：Tab 工作区状态与关闭意图

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzTabWidget.h`、`ZzFluentUI/widgets/src/ZzTabWidget.cpp`、`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.h`、`ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.cpp`、`ZzFluentUI/widgets/include/ZzFluentUI/ZzTabBar.h`、`ZzFluentUI/widgets/src/ZzTabBar.cpp`、`ZzFluentUI/widgets/src/private/ZzTabBarPrivate.h`、`ZzFluentUI/widgets/src/private/ZzTabBarPrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzTabControlsTest.cpp`、`ZzFluentUI/CMakeLists.txt`

- [ ] **步骤 1：编写失败测试。** 测试页面固定顺序、modified/attention/closeEnabled 的重复 setter 不重复发信号，新建按钮，关闭其他/右侧跳过 pinned 和不可关闭页，`setPageTitle()` 同步 tabText/windowTitle，页面跨容器转移和拖出后元数据保持，失败时源页面和状态不变。
- [ ] **步骤 2：运行并确认失败。** 运行 `cmake --build --preset linux-gcc-debug --target ZzTabControlsTest --parallel 2 && ctest --preset linux-gcc-debug -R fluent.tabs --output-on-failure`；预期新状态 API 和信号缺失导致失败。
- [ ] **步骤 3：实现最少代码。** 在私有 `QHash<QWidget *, Metadata>` 保存状态，页面 destroyed 时清理；扩展 TabBar 的稳定新建按钮和上下文菜单；批量关闭只发页面指针列表，不删除页面；把 transfer snapshot 扩展为固定/脏/注意/可关闭元数据，所有验证完成后再提交 Qt 插入/移除。
- [ ] **步骤 4：验证通过。** 运行定向测试，并在 200 页、1000 次切换/状态更新后检查定时器、动画和 QObject 数没有增长。
- [ ] **步骤 5：Commit。**
  ```bash
  git add ZzFluentUI/widgets ZzFluentUI/tests/ZzTabControlsTest.cpp ZzFluentUI/CMakeLists.txt
  git commit -m "组件：完善标签工作区状态与关闭意图"
  ```

### 任务 4：Explorer Pane 与 Command Palette

**文件：**
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzCommandItemRole.h`
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzExplorerPane.h`、`ZzFluentUI/widgets/src/ZzExplorerPane.cpp`、`ZzFluentUI/widgets/src/private/ZzExplorerPanePrivate.h`、`ZzFluentUI/widgets/src/private/ZzExplorerPanePrivate.cpp`
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzCommandPalette.h`、`ZzFluentUI/widgets/src/ZzCommandPalette.cpp`、`ZzFluentUI/widgets/src/private/ZzCommandPalettePrivate.h`、`ZzFluentUI/widgets/src/private/ZzCommandPalettePrivate.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 测试：`ZzFluentUI/tests/ZzExplorerPaneTest.cpp`、`ZzFluentUI/tests/ZzCommandPaletteTest.cpp`

- [ ] **步骤 1：编写失败测试。** Explorer 使用递归模型测试精确/前缀/包含匹配、源索引映射、60 ms debounce 合并输入、rowsInserted/Removed/dataChanged/reset 缓存失效和模型销毁。Command 使用 10k 行测试完全匹配、名称前缀、token 前缀、包含、关键词、Priority 与源行稳定排序，禁用项不可激活，Escape/外部点击恢复焦点。
- [ ] **步骤 2：运行并确认失败。** 运行 `cmake --build --preset linux-gcc-debug --target ZzExplorerPaneTest ZzCommandPaletteTest --parallel 2 && ctest --preset linux-gcc-debug -R 'explorer|command-palette' --output-on-failure`；预期目标类尚不存在而失败。
- [ ] **步骤 3：实现最少代码。** Explorer 使用一个持久 `QTimer`、递归 proxy 和源索引映射，空查询直接透传；Command 缓存规范化文本，查询长度限制 512，排序不创建 QObject、item widget、正则或编辑距离对象；覆盖层只作为 workspace 子控件，保存并恢复焦点。
- [ ] **步骤 4：验证通过。** 运行定向测试和 100k 节点/10k 命令数据测试；通过 `QApplication::allWidgets()` 和对象计数断言查询前后没有每行 QWidget、定时器或动画增长。
- [ ] **步骤 5：Commit。**
  ```bash
  git add ZzFluentUI
  git commit -m "组件：新增资源浏览与命令面板"
  ```

### 任务 5：DockPanel 与 WorkspaceShell

**文件：**
- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspacePanelId.h`、`ZzPureTools/widgets/src/ZzWorkspacePanelId.cpp`、`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceTitleMode.h`
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzDockPanel.h`、`ZzFluentUI/widgets/src/ZzDockPanel.cpp`、`ZzFluentUI/widgets/src/private/ZzDockPanelPrivate.h`、`ZzFluentUI/widgets/src/private/ZzDockPanelPrivate.cpp`
- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`、`ZzPureTools/widgets/src/ZzWorkspaceShell.cpp`、`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`、`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`、`ZzPureTools/CMakeLists.txt`
- 测试：`ZzFluentUI/tests/ZzDockPanelTest.cpp`、`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [ ] **步骤 1：编写失败测试。** Dock 测试覆盖 features、allowedAreas、toggleViewAction、浮动/重新停靠、标题栏无障碍和 `takeContentWidget()`。Shell 测试覆盖 host/titleBar 线程和祖先校验、重复 ID、空 content、Side/Dock 所有权、badge、四种标题模式、置顶可见性保持、布局 magic/version/长度/digest 校验、未知 ID 忽略、恢复失败回滚和 host 先销毁。
- [ ] **步骤 2：运行并确认失败。** 运行 `cmake --build --preset linux-gcc-debug --target ZzDockPanelTest ZzWorkspaceShellTest --parallel 2 && ctest --preset linux-gcc-debug -R 'dock-panel|workspace-shell' --output-on-failure`；预期新公共类型和工厂不存在而失败。
- [ ] **步骤 3：实现最少代码。** `ZzDockPanel` 只包 Qt Dock 标题视觉和内容转移，不创建第二个 WindowAgent；Shell 工厂先完成全部参数和线程验证，再转移 content 所有权；以 `zzWorkspaceDock:<id>` 注册 objectName；布局 payload 明确序列化 Qt state、Side 状态、当前 ID、顺序和标题模式，最大输入 1 MiB，失败时恢复 Qt/Shell 快照。
- [ ] **步骤 4：验证通过。** 运行定向测试，再执行 shared/static Debug 构建的 Shell 集成测试；用浮动 Dock 实际调用 `setFloating()/addDockWidget()`，检查没有业务窗口 API 或平台私有头。
- [ ] **步骤 5：Commit。**
  ```bash
  git add ZzFluentUI ZzPureTools ZzWindowKit
  git commit -m "组件：实现停靠面板与工作区协调器"
  ```

### 任务 6：Example 公开接口串联

**文件：**
- 创建：`examples/ZzPureToolsExample/ZzExampleSessionModel.h`、`examples/ZzPureToolsExample/ZzExampleSessionModel.cpp`、`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.h`、`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShell.h`、`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.h`、`examples/ZzPureToolsExample/ZzExampleWindowShell.cpp`、`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`、`examples/ZzPureToolsExample/main.cpp`、`examples/ZzPureToolsExample/CMakeLists.txt`
- 创建：`examples/ZzPureToolsExample/tests/CMakeLists.txt`
- 测试：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`

- [ ] **步骤 1：编写失败测试。** Smoke 测试从公开 `ZzWorkspaceShell::create()` 注册会话 Side Panel、终端 Tab、SFTP/日志/属性/任务 Dock 和命令模型；用鼠标单击一次激活 Activity、用命令面板键盘激活、创建/关闭 Tab、浮动 Dock、切换当前 Tab 标题并完成布局 round-trip。
- [ ] **步骤 2：运行并确认失败。** 运行 `cmake --build --preset linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest --parallel 2 && ctest --preset linux-gcc-debug -R example.workspace-smoke --output-on-failure`；预期 Example 仍依赖旧的 QDockWidget/QToolBar/搜索和标题同步算法，测试无法通过。
- [ ] **步骤 3：实现最少代码。** 会话模型只提供本地固定数据；内容工厂只创建 QWidget；WindowShell 负责导航/主题/关闭守卫和连接 QAction 意图，所有 Activity、Tab、Command、Dock、标题和布局行为改为 Shell/Fluent 公共接口；保留活动日志“位于尾部时跟随、手动上翻时暂停”的视图策略。
- [ ] **步骤 4：验证通过。** 运行 smoke、现有 Example ActivityModel 测试和 `QT_QPA_PLATFORM=offscreen` 下的启动检查；使用 `rg` 断言 Example 不直接创建 `QDockWidget`、`QListView` Activity、`QToolBar` 工作区命令或调用 `setWindowTitle` 同步当前 Tab。
- [ ] **步骤 5：Commit。**
  ```bash
  git add examples/ZzPureToolsExample
  git commit -m "示例：使用公开工作区组件串联 SSH 风格场景"
  ```

### 任务 7：性能、截图、安装消费与平台审计

**文件：**
- 创建：`benchmarks/ZzWorkspaceComponentsBenchmark.cpp`
- 修改：`CMakeLists.txt`、`benchmarks/CMakeLists.txt`、`ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`、`tests/InstallConsumer/CMakeLists.txt`、`tests/InstallConsumer/Gui/main.cpp`、`tests/PublicHeaderConsumer/CMakeLists.txt`、`tests/Architecture/ZzArchitectureAudit.cmake`、`tests/Architecture/CMakeLists.txt`、`tests/Platform/ZzPerformanceThresholdContract.cmake`、`tests/Platform/PresetMatrixContract.cmake`
- 修改：`docs/development/BUILDING_ZH.md`、`docs/development/PLATFORM_SUPPORT_ZH.md`、`docs/performance/PERFORMANCE_BASELINE_ZH.md`、`docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`

- [ ] **步骤 1：编写失败测试和基准。** 增加 `benchmark.workspace-components` 的八个场景和结构性断言：重复操作 QObject 数不增长、可见列表没有每行 QWidget、布局失败可回滚、1000 次状态切换不增加 timer/animation；截图测试加入 Light/Dark/HighContrast、DPR 100/125/150/200。
- [ ] **步骤 2：运行并确认失败。** 运行 `cmake --preset linux-gcc-benchmarks && cmake --build --preset linux-gcc-benchmarks --parallel 2 && ctest --preset linux-gcc-benchmarks -R 'benchmark.workspace-components|fluent.screenshot' --output-on-failure`；预期新 benchmark、截图场景和安装头尚未接线而失败。
- [ ] **步骤 3：实现最少接线。** benchmark 通过现有 `ZzPerformanceReporter` 输出 observe JSON，不改变正式阈值；截图按既有 baseline 目录与 DPR 注册；安装/PublicHeader Consumer 编译全部新增头；ArchitectureAudit 检查依赖、命名空间、Doxygen、PIMPL、Qt Private、stylesheet 和业务泄漏。
- [ ] **步骤 4：验证通过。** 在 Linux 上依次运行 Debug、Release、Static、Clang-Tidy、ASan/UBSan、截图、benchmark、安装消费和架构门禁：
  ```bash
  cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-gcc-debug --parallel 2
  ctest --preset linux-gcc-debug --output-on-failure
  cmake --build --preset linux-gcc-debug --target ZzClangTidy
  cmake --preset linux-gcc-benchmarks
  cmake --build --preset linux-gcc-benchmarks --parallel 2
  ctest --preset linux-gcc-benchmarks -R benchmark.workspace-components --output-on-failure
  ```
  连续三轮保存 benchmark JSON，报告 P50/P95/最大值和同一机器指纹噪声带；Windows MSVC、Windows MinGW、macOS 只运行可用的源码/公共头/静态检查，未执行原生构建的平台在文档中明确记录。
- [ ] **步骤 5：Commit。**
  ```bash
  git add benchmarks CMakeLists.txt ZzFluentUI/tests tests docs
  git commit -m "质量：补齐工作区组件性能与跨平台验收"
  ```

---

## 规格覆盖度自检

- 标题栏自适应菜单、中心标题、主题/置顶意图和 WindowKit 命中测试：任务 1。
- Activity 左右主次分组、badge、键盘、拖拽意图和 SidePane 所有权/宽度：任务 2。
- Tab pinned/modified/attention/closeEnabled、新建、批量关闭、标题同步和转移事务：任务 3。
- Explorer 递归过滤、固定 debounce、源索引映射和 Command Palette 排序/焦点/对象预算：任务 4。
- Dock 原生浮动/停靠、WorkspaceShell 注册/标题/置顶/版本化布局回滚：任务 5。
- Example 的 SSH 风格假数据、终端/SFTP/日志/属性/任务内容和公开接口 smoke：任务 6。
- 性能 observe、截图、多 DPI、安装消费、架构审计和 Linux/Windows/macOS 状态：任务 7。
- 线程、错误、所有权、四文件 PIMPL、中文 Doxygen、C++20、Qt 公共 API 和不提升正式阈值：任务 1 至任务 7 的实现与门禁共同覆盖。

## 计划自检结果

- 计划中的每个实现步骤都给出了具体文件、命令、预期结果和可审查的完成条件，没有空泛步骤。
- 所有任务均包含失败测试、失败验证、最小实现、通过验证和中文 commit。
- `ZzActivityArea`、`ZzCommandItemRole`、`ZzWorkspacePanelId`、`ZzWorkspaceTitleMode` 等类型先在文件清单中定义，再在后续任务中复用，名称保持一致。
- 计划没有把 SSH、SFTP、网络、设置或业务模型依赖引入 FluentUI；Example 只作为公开 API 的验收消费者。
