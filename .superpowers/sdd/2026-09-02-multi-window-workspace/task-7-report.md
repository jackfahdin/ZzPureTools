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
