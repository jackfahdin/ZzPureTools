# 任务 8 报告

## 实现

- 冻结协调器 `closeWindow`、`approveDelegatedClose`、关闭前、委托关闭和
  孤儿页面信号。
- 关闭事件过滤器完整实现 `Allow`、`Deny`、`Delegate`：系统 Allow 关闭在
  当前关闭事件内同步回收，Delegate 只保留一个待批准令牌，跨线程批准不会
  消耗令牌，Deny 不能被批准接口绕过。
- 为登记窗口分配稳定 `QUuid`，并按页面维护 `{windowId, groupId, index}`
  来源栈。页面回到已知窗口时截断已返回路径；回收优先原窗口和原索引，原组
  消失时使用该窗口活动组，来源窗口消失时依次使用主窗口和首个登记存活窗口。
- 扩展 `ZzSplitWorkspace::tabTransferCommitted`，在 `sourceGroup` 后携带事务
  开始时的 `sourceIndex`。两个成功提交路径及全部连接方同步更新。
- 批量回收先完成目标、页面身份、布局键和容量预检；迁移阶段失败时按相反
  顺序回滚已完成页面，保留页面身份、布局键、标签元数据、原组、原索引和来源栈。
- 无回收目标时只发射一次 `orphanedPages`，页面按组和标签稳定顺序保留在原窗；
  空窗口不发孤儿信号并正常关闭；`beginShutdown()` 仅清理登记，不执行普通回收。
- 写入 `window.create`、`window.tear_off`、`page.transfer`、`page.reclaim`
  审计事件。正文只包含稳定 ID、阶段、耗时和结果；未初始化日志时先经
  `ZzLog::shouldLog(Info)` 返回，不创建 sink，也不伪造 `layout.restore`。
- `ZzPureTools` 对 `ZzLog::ZzLog` 使用 PRIVATE 直接链接，协调器测试单独链接
  `ZzLog::ZzLog`。

## 修改文件

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- `ZzFluentUI/widgets/src/private/ZzWorkspaceCrossTransferTransactionPrivate.cpp`
- `ZzFluentUI/tests/ZzWorkspaceCrossTransferTest.cpp`
- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCoordinator.h`
- `ZzPureTools/widgets/src/ZzWorkspaceWindowCoordinator.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- `ZzPureTools/tests/CMakeLists.txt`
- `ZzPureTools/CMakeLists.txt`

## TDD 证据

自然 RED：

```text
denyPolicyIgnoresSystemCloseEvent:
  !window->close() returned FALSE
delegatePolicyRequestsApprovalOnlyOnce:
  !window->close() returned FALSE
allowSystemCloseReclaimsBeforeAcceptedClose:
  first workspace indexOf(page) was -1, expected 0
closingChainedWindowsReclaimsInReverseOriginOrder:
  page returned to C instead of A
layoutKeyConflictIsRejectedBeforeAnyPageMoves:
  target commit count was 1, expected 0
delegatedApprovalRejectsForeignThreadWithoutConsumingToken:
  owner-thread approval failed after foreign-thread attempt
transferCommitCarriesStartingSourceIndex:
  actual source index 0, expected 1
reclaimRestoresOriginalSourceIndex:
  moving page restored to the wrong slot
actualOperationsWriteStableAuditEvents:
  log file did not contain window.create
```

补充 mutation RED：

```text
missingOriginGroupUsesTargetActiveGroup:
  closeWindow returned FALSE after active-group fallback was removed
destroyedOriginFallsBackToPrimaryWindow:
  primary indexOf(page) was -1, expected 0
destroyedOriginAndPrimaryFallBackToFirstWindow:
  fallback indexOf(page) was -1, expected 0
orphanedPagesPreserveStableOrderAndWindow:
  emitted list size was 0, expected 3
emptyWindowClosesWithoutOrphanSignal:
  orphaned signal count was 1, expected 0
shutdownDoesNotRunOrdinaryReclaim:
  reclaim commit count was 1, expected 0
secondPageFailureRollsBackIdentityMetadataAndOrder:
  current tab count was 1, expected 2
operationsDoNotInitializeLogging:
  !ZzLog::isInitialized() returned FALSE
```

GREEN：

```text
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceWindowCoordinatorTest \
           ZzMultiWindowIsolationTest \
           ZzWorkspaceCrossTransferTest --parallel 2
  PASS

ctest --preset linux-gcc-debug \
  -R '^puretools\.(workspace-window-coordinator|multi-window)' \
  --output-on-failure
  44/44 passed

ctest --preset linux-gcc-debug \
  -R '^fluent\.workspace-cross-transfer$' --output-on-failure
  1/1 passed
```

## 自审

- `reclaiming` 和 `handlingCloseEvent` 均由局部 RAII 恢复；所有会发信号的迁移
  之后重新按稳定窗口 ID 查找记录，不继续使用旧记录迭代器。
- Allow 系统关闭只为当前关闭事件消费一次 `closeBypass`；普通直接关闭仍沿用
  `ZzApplicationWindow::closeAccepted` 和应用 queued erase 生命周期。
- 批量预检发生在任何页面移动之前，失败回滚发生在 `reclaiming` 保护范围内，
  不会污染来源栈；来源栈只在全部页面迁移完成后提交截断。
- 无目标分支最多发射一次孤儿信号并立即返回；空页面集合不会进入该分支。
- 日志正文未包含标题、指针、索引或布局键；代码中没有 `layout.restore` 写入。
- 已恢复误跑整文件 `clang-format` 造成的格式 churn。最终差异限定为上述 10 个
  文件，未变代码保持 `HEAD` 原格式。

未发现阻塞提交的问题。
