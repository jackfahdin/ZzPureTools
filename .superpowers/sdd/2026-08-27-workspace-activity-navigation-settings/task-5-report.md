# 任务 5 报告：事务集成 ApplicationWindow 导航表面

## TDD 证据

### RED：公开集成 API 尚不存在

```text
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceNavigationIntegrationTest --parallel 2

ZzWorkspaceNavigationIntegrationTest.cpp 编译失败：
ZzWorkspaceShell 不存在 integrateApplicationNavigation()。
```

该失败证明新增测试真实依赖任务要求的公开入口，而非既有行为。

### RED：成功注册后的晚期失败未完整回滚

测试在 `tabPinnedChanged` 同步改变 route，使事务在 Side Panel 和中央 tab 已成功
注册后失败：

```text
QWARN: QBoxLayout::insert: index 1 out of range (max: 0)
FAIL: duplicatePanelIdRollsBackBodyModelCurrentAndOwnership
'createdWindow' returned FALSE
```

根因是 body 子项按不稳定顺序回插，且临时 Side Panel 移除后未恢复调用前的
SidePane、ActivityBar 和 Shell 逻辑投影。

### RED：固定 tab 信号同步销毁 Shell

测试在 `tabPinnedChanged` 中同步销毁 Shell：

```text
Received signal 11 (SIGSEGV)
QTabWidget::indexOf(this=0x0, w=0x0)
ZzWorkspaceNavigationIntegrationTransactionPrivate.cpp:311
```

根因是 `setTabPinned()` 发信号后直接通过已清空的 `tabsGuard` 计算关闭状态索引。

### GREEN：针对性回归

```text
QT_QPA_PLATFORM=offscreen \
  ./build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceNavigationIntegrationTest \
  duplicatePanelIdRollsBackBodyModelCurrentAndOwnership \
  synchronousDestructionDuringTransferRollsBackOrFailsClosed -v1

Totals: 4 passed, 0 failed, 0 skipped
```

## 实现摘要

- 为 `ZzWorkspaceShell` 增加严格签名的 `integrateApplicationNavigation()` 入口和中文 Doxygen。
- `ZzApplicationWindowPrivate` 保存原 body 的 guarded/raw 身份，仅向导航集成事务开放 friend 边界。
- 新增 `NavigationIntegration` 事务种类，并只允许该事务内部受控调用 Side Panel 注册/取回。
- 事务审计 host、body/layout、NavigationPane、PageHost、model/controller、route、工作区、tab、SidePane、PanelStack 与 ActivityBar 身份。
- NavigationPane 迁入 SidePane，PageHost 迁入固定且不可关闭的中央 tab；成功后销毁空 body。
- 失败按反序移除 tab 和 panel，按原索引顺序恢复 body 子项，并恢复两侧 current、visible、sizes、collapsed、hidden、Activity current/active、Shell current/expanded、tab current 与 route。
- 所有会发同步信号的关键调用后立即复核 `QPointer`；body 删除位于显式提交边界之后，提交后只做 guarded 完成或失败关闭。
- `takePanel()` 在 SidePane 取回与 Activity model 删除信号后于函数内部复核 Shell/model 生命周期；测试同时覆盖 `rowsAboutToBeRemoved` 中同步销毁 Shell。
- 只有完整回滚成功才解除 `NavigationIntegration` 互斥；受干扰的半回滚保持失败关闭。
- 固定 tab 的 pin/close 状态由 Shell 生命周期绑定的连接持续重申，并拒绝第二次集成。

## 文件范围

修改 9 个既有任务文件：

```text
ZzPureTools/CMakeLists.txt
ZzPureTools/tests/CMakeLists.txt
ZzPureTools/widgets/include/ZzPureTools/ZzApplicationWindow.h
ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h
ZzPureTools/widgets/src/ZzWorkspaceShell.cpp
ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp
ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.h
ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp
ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h
```

新增 3 个任务文件：

```text
ZzPureTools/tests/ZzWorkspaceNavigationIntegrationTest.cpp
ZzPureTools/widgets/src/private/ZzWorkspaceNavigationIntegrationTransactionPrivate.cpp
ZzPureTools/widgets/src/private/ZzWorkspaceNavigationIntegrationTransactionPrivate.h
```

## 验证

```text
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceNavigationIntegrationTest ZzApplicationBuilderTest \
  ZzNavigationControllerTest ZzWorkspaceShellTest --parallel 2

结果：成功，4 个目标均已构建。
```

```text
ctest --test-dir build/linux-gcc-debug \
  -R '^puretools\.(workspace-navigation-integration|navigation|workspace-shell)$|^puretools\.application-builder\.' \
  --output-on-failure

结果：18/18 passed，0 failed。
```

```text
git diff --check

结果：通过，无空白错误。
```

最终独立复审确认先前的 3 项问题均已解决，未发现新的 Critical 或 Important 问题。

## 遗留关注

无已知功能或测试缺口。事务无法在已跨越 body 销毁提交边界后承诺回滚；该路径按设计保留
单次集成标记并失败关闭，避免在身份已破坏时重试搬运。
