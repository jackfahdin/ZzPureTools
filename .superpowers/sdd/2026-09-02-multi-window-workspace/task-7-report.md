# 任务 7 报告：窗口创建、配置继承和撕出事务

## 实现内容

- 增加 `ZzWorkspaceWindowCoordinator` 的窗口工厂、配置应用、窗口创建和撕出入口。
- 按显式 Patch > 来源窗口快照 > 协调器默认值合并配置，并同步窗口标题、图标、尺寸、Shell 标题模式和置顶状态。
- 撕出强制使用 Deferred 暂存窗口，迁移同一页面到目标 Shell 首组，提交成功后显示、提升并按需激活。
- 为所有已登记工作区连接 `tabTearOffRequested`，几何按鼠标位置、推荐尺寸和当前屏幕可用区域收敛。
- 工厂、登记、配置或迁移失败时关闭暂存窗口并保持来源页面和协调器登记不变。

## 修改文件

- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCoordinator.h`
- `ZzPureTools/widgets/src/ZzWorkspaceWindowCoordinator.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- `ZzPureTools/tests/CMakeLists.txt`

## RED 证据

命令：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest --parallel 2
```

预期失败摘录：

```text
error: ‘class ZzPureTools::ZzWorkspaceWindowCoordinator’ has no member named ‘createWindow’
error: ‘class ZzPureTools::ZzWorkspaceWindowCoordinator’ has no member named ‘setWindowFactory’
```

该失败发生在生产实现补齐之前，说明测试确实捕获任务 7 缺失行为。

## GREEN 验证

构建与协调器场景：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzWorkspaceCrossTransferTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\\.workspace-window-coordinator\\.' --output-on-failure
```

结果：11/11 协调器场景通过，0 失败。简报中以基础名 `$` 结尾的正则无法匹配实际按场景注册的测试，因此使用场景前缀正则。

跨工作区迁移真实二进制：

```bash
LD_LIBRARY_PATH="$PWD/build/linux-gcc-debug/ZzPureTools:$PWD/build/linux-gcc-debug/ZzFluentUI:$PWD/build/linux-gcc-debug/ZzWindowKit:$PWD/build/linux-gcc-debug/ZzCore:$PWD/build/linux-gcc-debug/ZzThirdParty/ZzLog:/home/zz/Qt/6.11.1/gcc_64/lib" QT_QPA_PLATFORM=offscreen ./build/linux-gcc-debug/ZzFluentUI/tests/ZzWorkspaceCrossTransferTest -v1
```

结果：21/21 场景通过，0 失败。直接 ctest 二进制缺少 `libZzFluentUI.so.0` 搜索路径，按简报要求使用显式构建树库路径运行。

提交前检查：`git diff --check` 通过。

## 自审

- 配置合并在登记前完成，失败不会写入记录；应用失败时恢复旧配置表面。
- 撕出暂存窗口始终 Deferred，迁移提交前不可见；失败路径注销登记并关闭窗口。
- `QMetaObject::Connection` 随登记记录解除，窗口和 Shell 仍由应用/调用方生命周期管理。
- 测试使用真实 `ZzSplitWorkspace::transferTabToWorkspace()` 和真实窗口对象，覆盖信号连接、页面身份、来源独立性、工厂/迁移失败。

## 疑虑

- 工厂返回的 Shell 所有权仍由工厂调用方负责；协调器只保存 `QPointer`，与现有 Handle 非拥有契约一致。
- `ZzWindowClosePolicy` 目前仅作为配置快照保存，任务 8 的关闭策略未实现。

## 第 1/5 轮审查修复

基线：`60a1a6a`。

### 修复内容

- `tearOff()` 现在线程和关闭态检查后才读取任何登记记录或 QObject，非法调用统一返回 `InvalidState`。
- `applyConfiguration()` 在应用前保存实际窗口图标、最小/最大尺寸、几何，以及 Shell 的标题和置顶表面；失败时恢复这些真实值，不再依赖空 `initialGeometry` 哨兵。
- `createWindow()` 捕获工厂的标准和未知异常，转换为 `Unknown` 错误。协调器只对已成功返回的 Handle 承担关闭责任，无法获得的工厂内部局部对象仍由工厂自身异常安全契约负责。
- `fluent.workspace-cross-transfer` 通过 `ENVIRONMENT_MODIFICATION` 配置跨平台动态库路径：Linux 使用 `LD_LIBRARY_PATH`、macOS 使用 `DYLD_LIBRARY_PATH`、Windows 使用 `PATH`，路径均从构建目标生成表达式取得。

### RED 证据

新增测试后执行：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest --parallel 2
ctest --preset linux-gcc-debug -R '^(puretools\\.workspace-window-coordinator\\.factoryExceptionReturnsUnknownWithoutCoordinatorMutation|puretools\\.workspace-window-coordinator\\.tearOffRejectsForeignThreadAndShutdownBeforeStateAccess)$' --output-on-failure
```

结果：两项均按预期失败。工厂场景摘录为 `Caught unhandled exception` 和 `factory exception`；跨线程场景摘录为 `ASSERT: "zzIsShellThread(this)"`，调用栈定位到 `ZzWorkspaceShell::splitWorkspace()`。

### GREEN 验证

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest ZzWorkspaceCrossTransferTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\\.workspace-window-coordinator\\.' --output-on-failure
ctest --preset linux-gcc-debug -R '^(puretools\\.workspace-window-coordinator|fluent\\.workspace-cross-transfer)$' --output-on-failure
git diff --check
```

结果：协调器 13/13 通过；标准组合正则仍因基础名锚定规则只匹配 Fluent 场景，`fluent.workspace-cross-transfer` 1/1 通过，且无需手工 `LD_LIBRARY_PATH`。协调器完整场景使用前缀正则验证，避免该已确认的 CTest 注册名限制。

### 修改文件

- `ZzPureTools/widgets/src/ZzWorkspaceWindowCoordinator.cpp`
- `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- `ZzPureTools/tests/CMakeLists.txt`
- `ZzFluentUI/tests/CMakeLists.txt`
- 本报告

## 第 3/5 轮配置失败原子性修复

基线：`3cdc712`。本轮处理第 2 轮复审指出的 Shell 标题和标题模式并未在失败路径中实际写入、因而无法证明回滚的问题。

### 根因与方案

`ZzWorkspaceShell::setApplicationTitle()` 和 `setTitleMode()` 是无返回值操作；配置同步中唯一可报告失败的步骤是 `setAlwaysOnTop()`，其失败可能来自 Shell 事务状态或 WindowKit 后端。此前先写入窗口和 Shell 表面、再处理这一失败点，导致 Shell 标题/模式回滚需要不可稳定复现的故障时序。

现将 `setAlwaysOnTop()` 作为预检步骤放在所有其他表面写入之前。预检失败时，图标、尺寸、几何、应用标题和标题模式都尚未改动，配置快照也尚未写入；预检成功后剩余操作均为无错误返回的 Qt/Shell 表面同步，因此删除不可达的伪回滚分支。

### RED 证据

新命名场景：`applyConfigurationPreflightsAlwaysOnTopBeforeChangingRealSurfaces`。为证明它能捕获生产变异，临时将 `setAlwaysOnTop()` 错误后移到窗口和 Shell 写入之后：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\\.workspace-window-coordinator\\.applyConfigurationPreflightsAlwaysOnTopBeforeChangingRealSurfaces$' --output-on-failure
```

结果：按预期失败，摘录：

```text
Actual   (window->geometry())    : QRect(120,130 700x500)
Expected (before.initialGeometry): QRect(40,50 640x480)
```

该测试使用真实 `integrateApplicationNavigation()` 事务让 `setAlwaysOnTop()` 返回 `InvalidState`，并断言失败后真实窗口 geometry/minimumSize/maximumSize/icon、Shell applicationTitle/titleMode/alwaysOnTop 及协调器配置快照均保持调用前值。它捕获的是“将可失败置顶操作后移”这一生产变异，不断言 mock。

### GREEN 验证

恢复预检顺序后执行：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\\.workspace-window-coordinator\\.' --output-on-failure
git diff --check
```

结果：协调器 14/14 场景通过，0 失败；空白检查通过。

### 修改文件

- `ZzPureTools/widgets/src/ZzWorkspaceWindowCoordinator.cpp`
- `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- `ZzPureTools/tests/CMakeLists.txt`
- 本报告

## 第 2/5 轮回滚测试补齐

基线：`0cfbeca`。本轮只处理定向复审中缺失的 `applyConfiguration()` 失败后真实表面状态回滚测试。

### 测试设计

新场景 `applyConfigurationRollsBackRealSurfacesWhenShellTransactionRejectsAlwaysOnTop` 使用真实 `ZzWorkspaceShell::integrateApplicationNavigation()` 事务。该事务中，真实 `ZzWorkspaceShell::setAlwaysOnTop()` 会以 `InvalidState` 拒绝请求；通过真实 `QTabWidget::currentChanged` 回调调用 `applyConfiguration()`，使图标、最小/最大尺寸、几何、标题和标题模式已进入应用序列后再失败。

断言调用前真实 `window` 的 geometry/minimumSize/maximumSize/icon 及 `shell` 的 applicationTitle/titleMode/alwaysOnTop 全部恢复，协调器配置快照逐字段保持不变。该测试不对 mock 调用做断言。

### RED 证据

为验证测试能捕获目标缺陷，临时移除生产回滚中的 `window->setGeometry(geometryBefore)` 后执行：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\\.workspace-window-coordinator\\.applyConfigurationRollsBackRealSurfacesWhenShellTransactionRejectsAlwaysOnTop$' --output-on-failure
```

结果：按预期失败，摘录：

```text
Actual   (window->geometry())    : QRect(120,130 700x500)
Expected (before.initialGeometry): QRect(40,50 640x480)
```

这证明测试捕获的是部分字段已应用、`setAlwaysOnTop()` 失败后几何未恢复的实际回滚缺陷。

### GREEN 验证

恢复 `window->setGeometry(geometryBefore)` 后执行：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceWindowCoordinatorTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\\.workspace-window-coordinator\\.' --output-on-failure
git diff --check
```

结果：协调器 14/14 场景通过，0 失败；空白检查通过。

### 修改文件

- `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- `ZzPureTools/tests/CMakeLists.txt`
- 本报告
