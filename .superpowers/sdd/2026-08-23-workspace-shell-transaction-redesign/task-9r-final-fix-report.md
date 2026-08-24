# Task 9R final fix report

## Scope

基线 `5123176`，仅修改事务私有实现与 `ZzWorkspaceShellTest`：

- `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp`
- `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

未修改公开 `ZzWorkspaceShell` 头、schema2 bytes、codec/state，也未触碰 `temp_image`。

## TDD evidence

先加入并运行 RED：

```text
activityMoveRejectsFinalSizesActivityStateOverride: currentSourceIndex polluted (actual row 0, expected row 1)
restoreRejectsTitleSinksOverwrittenWithinCommit: restore unexpectedly returned success
activityMoveSynchronizesEdgeVisibilityBothDirections: source edge visibility remained incorrect
```

生产修复后，三条回归均 GREEN。

## Fixes

- Activity move 最终审计现在比较 model global row order、四区 rows/area、左右 ActivityBar current/active（通过稳定 model row ID 映射）。
- 最终 sizes 应用后调用 `syncSideEdgeVisibility()`，并审计两侧 ActivityBar hidden/visible 状态。
- Layout transaction 审计 host effective title 与 title bar effective title；titleBar 为可选对象时允许为空，并将 titleBar QPointer 纳入 runtime guards，删除/替换会使事务失败。
- 标题比较按当前 mode、application/custom 独立值推导 effective title，保留 independent title/always-on-top 合同；schema1 title metadata 使用当前 snapshot 的非持久化文本输入。

## Verification

```text
cmake --build build/linux-gcc-debug --target ZzWorkspaceShellTest -j2  PASS
QT_QPA_PLATFORM=offscreen .../ZzWorkspaceShellTest <focused>  8 passed, 0 failed
QT_QPA_PLATFORM=offscreen .../ZzWorkspaceShellTest  143 passed, 0 failed
QT_QPA_PLATFORM=offscreen ctest --test-dir build/linux-gcc-debug -R puretools.workspace-shell --output-on-failure  1/1 passed
```

## Remaining findings / self-review

本波未改 `ZzWorkspaceLayoutStatePrivate.cpp` 与 planner 的多处 O(n^2) 查找（`zzUniqueNonEmpty`、`zzRegisteredSideIds`、`zzNormalizeTarget`、split/activity normalize、`buildRestoreTarget`/`buildActivityMoveTarget`），也未改 `buildActivityMoveTarget` 的 mixed side order 语义，以及 `registerSidePanel` 允许 Secondary 先于 Primary 导致的 `zzSideOrderMatchesActivity` 合同冲突。这些需要独立 planner/registration 设计与基准，当前波次不安全盲改，列为后续 Important/NEEDS_CONTEXT。

titleBar 是外部可选非拥有对象；测试覆盖 `create(host, nullptr)` 的 restore 合同。现有公开 fixture 的 titleBar 为栈对象，无法合法在同步回调中 delete；删除/替换场景由 QPointer guard 覆盖。
