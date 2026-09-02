# 工作区公共 API 契约实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框跟踪进度；每个任务完成验证后立即创建中文 commit。

**目标：** 用独立的公开头消费者测试冻结工作区组件的所有权、状态同步和布局接口，确保后续内部重构不改变可复用 API。

**架构：** 测试只通过 `Zz::PureTools`、`Zz::FluentUI` 和 Qt 公共 API 创建宿主窗口，验证 `ZzWorkspaceShell` 对 Activity Bar、Side Pane、Bottom Pane、Tab Widget、Command Palette、Dock Panel 的观察接口和生命周期合同。生产代码不新增行为；文档记录调用方可依赖的线程、所有权和错误返回规则。

**技术栈：** Qt 6.8+ Widgets/Test、C++20、CMakePresets、现有 ZzResult 与 offscreen 平台。

---

### 任务 1：新增工作区公共 API 契约测试

**文件：**
- 创建：`ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp`
- 修改：`ZzPureTools/tests/CMakeLists.txt`

- [ ] **步骤 1：编写测试源并确认目标尚不存在**

测试覆盖：

```cpp
void exposesStableWorkspaceSurfaces();
void registersAndReturnsOwnedPanels();
void keepsFactoryLazyAndRetryable();
void roundTripsLayoutAndTitleState();
```

每个测试都从公开头创建 `QMainWindow` 与 `ZzWorkspaceShell`，验证：

- `workspaceWidget()`、左右 Activity Bar/Side Pane、`splitWorkspace()`、`bottomPane()`、`commandPalette()` 非空且父级稳定；
- Side、Bottom、Dock 注册只接管无父 QWidget，`takePanel()` 归还同一指针且父对象为空；
- Side Panel Factory 在首次显示前不调用，失败后可重试，成功后只调用一次；
- 标题策略和 `setAlwaysOnTop()` 不触发宿主隐藏/显示；
- `saveLayout()` 产生非空状态，`restoreLayout()` 可恢复侧栏显隐、标签和标题模式；
- 所有失败路径返回 `ZzErrorCode`，不会改变已有对象树。

先运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspacePublicApiTest --parallel 2
```

预期：目标尚未注册，构建失败。

- [ ] **步骤 2：注册测试目标**

在 `ZzPureTools/tests/CMakeLists.txt` 添加 `ZzWorkspacePublicApiTest`，链接 `Qt6::Test`、`Qt6::Widgets`、`Zz::PureTools` 和 `Zz::FluentUI`，设置 `AUTOMOC`、项目警告、sanitizer，并注册 `puretools.workspace-public-api`，环境为 `QT_QPA_PLATFORM=offscreen`。

- [ ] **步骤 3：运行测试确认通过**

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --target ZzWorkspacePublicApiTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools.workspace-public-api$' --output-on-failure
```

预期：全部契约用例通过，且无新增警告。

- [ ] **步骤 4：提交**

```bash
git add ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp ZzPureTools/tests/CMakeLists.txt
git diff --cached --check
git commit -m "测试：冻结工作区公共 API 契约" -m "新增只消费公开头的工作区生命周期、惰性面板、标题状态和布局往返验证，防止后续内部重构改变所有权与错误返回行为。"
```

### 任务 2：补充工作区 API 使用说明

**文件：**
- 创建：`docs/development/WORKSPACE_API_ZH.md`
- 修改：`README.md`

- [ ] **步骤 1：编写调用方说明**

文档明确列出：

- `ZzWorkspaceShell::create()` 的顶层 `QMainWindow` 与 GUI 线程前提；
- Side/Bottom/Dock 面板的无父 QWidget 所有权转移和 `takePanel()` 归还规则；
- Factory 的惰性调用与失败重试语义；
- Activity 模型为非拥有观察值，Shell 只发意图；
- 标题、置顶、布局保存恢复的职责边界；
- shared/static 安装包的最小消费示例和验证命令。

- [ ] **步骤 2：在 README 增加链接**

在工作区组件表格下增加“公共 API 与生命周期”链接，指向该文档。

- [ ] **步骤 3：检查文档与提交**

```bash
git diff --check
rg -n "ZzWorkspaceShell|takePanel|registerSidePanelFactory|GUI 线程" docs/development/WORKSPACE_API_ZH.md
git add docs/development/WORKSPACE_API_ZH.md README.md
git commit -m "文档：补充工作区公共 API 使用约定" -m "记录线程前提、所有权边界、惰性工厂、标题状态和安装消费方式，便于外部应用直接集成。"
```

### 任务 3：Linux 验证与跨平台静态检查

**文件：**
- 不修改生产代码；更新本计划的验证记录。

- [ ] **步骤 1：运行定向、安装消费和公共头测试**

```bash
cmake --build --preset linux-gcc-debug --parallel 2
ctest --preset linux-gcc-debug -R '^(puretools.workspace-public-api|install.consumer|architecture.public-headers)$' --output-on-failure
```

- [ ] **步骤 2：运行静态边界检查**

```bash
cmake --build --preset linux-gcc-debug --target ZzClangTidy --parallel 2
ctest --preset linux-gcc-debug -R '^(architecture.complete-audit|platform.preset-matrix)$' --output-on-failure
```

记录 Linux 实际结果；Windows MSVC/MinGW 和 macOS 只记录源码公共 API 静态检查，不写成已运行通过。

- [ ] **步骤 3：提交验证记录**

在本文末尾追加命令、日期、Qt/GCC 版本和通过数量后提交：

```bash
git add docs/superpowers/plans/2026-09-02-workspace-public-api-contract.md
git commit -m "文档：记录工作区 API 契约验证证据" -m "保存 Linux 定向测试、安装消费、公共头和静态边界检查结果，并明确 Windows/macOS 的验证边界。"
```

## 完成标准

- `ZzWorkspacePublicApiTest` 只包含公开头，所有契约用例通过；
- 文档明确所有权、线程和状态职责，README 可导航到文档；
- Linux 定向测试、安装消费、公共头和静态边界检查有新鲜输出；
- `temp_image/` 未被读取、修改、暂存或提交。
