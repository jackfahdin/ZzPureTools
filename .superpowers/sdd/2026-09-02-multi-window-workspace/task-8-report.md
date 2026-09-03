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

## 第 1 轮审查修复

### RED

针对审查报告中的 2 项 Critical 与 5 项 Important，新增回归场景首先确认原实现失败：

```text
aboutToCloseRejectsReentrantCloseAndUnregister
committedTransferMayDestroySourceWorkspaceSafely
nonMonotonicOriginsRestoreExactOrder
thirdPageFailureRestoresExactCurrentOrder
reclaimDoesNotSuppressUnrelatedTransferOrClose
rejectedInternalCloseRollsBackAndCannotBypassDeny
closePolicyChangeInvalidatesDelegateRequest
failedReclaimAuditContainsNoCommitSuccess
```

八项测试均在修复前按预期失败，覆盖同步关闭信号重入、迁移参与者销毁、非单调来源索引、第三页失败回滚、跨窗口状态串扰、关闭拒绝、策略变更令牌和审计误报。

### 实现

- 将关闭/批准/事件过滤和回收事务主体移入 Private，并按窗口使用
  `Idle`、`DelegatePending`、`DelegateApproved`、`Reclaiming`、
  `DispatchingClose`、`NotifyingClose`、`CloseAccepted` 状态机；关闭策略改变时
  失效旧批准请求，内部 close 令牌绑定当前窗口且拒绝后回滚。
- 为 Fluent 工作区增加仅供协调器使用的静默单页迁移和整体组顺序恢复原语；
  回收阶段屏蔽工作区/标签容器信号，保留同步迁移所需的 TabBar 内部更新。
- 使用 `QPointer` 跨同步信号边界重新解析源/目标对象；内部迁移仅以精确的
  `{sourceWindowId,targetWindowId,page}` 上下文抑制来源记录，不影响无关窗口。
- 为来源记录分配单调序列号，按目标组构建完整当前顺序与 pinned 快照，成功
  或失败时整体恢复；批量事务完成前暂不写入 `page.reclaim` success 审计。

### GREEN

```text
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceWindowCoordinatorTest \
           ZzMultiWindowIsolationTest \
           ZzWorkspaceCrossTransferTest --parallel 2
  PASS

ctest --preset linux-gcc-debug \
  -R '^puretools\.(workspace-window-coordinator|multi-window)|^fluent\.workspace-cross-transfer' \
  --output-on-failure
  53/53 passed
```

整套 `ctest --preset linux-gcc-debug --output-on-failure` 已运行完成：`104/194`
通过，`90` 项失败或未运行。其中大部分是当前配置未构建的无关可执行文件；
`fluent.tab-controls`、`fluent.split-workspace` 和
`puretools.workspace-public-api` 在进入测试前因动态库搜索路径缺失退出；其余已执行
但与本任务无关的残余失败为既存 `architecture.complete-audit`（
`ZzWorkspaceCrossTransferTransactionPrivate.cpp` 中未使用 `Zz` 前缀的类型）和
`platform.binary-dependencies`（目标未构建）。Task 8 聚焦测试在整套运行中同样
全部通过。

### 自审结论

逐项复核了关闭状态转移、对象销毁后的 `QPointer` 重解析、完整顺序快照及
rollback、内部迁移精确抑制、关闭拒绝清理、Delegate 代次失效和延迟 success
审计；`git diff --check` 通过。未发现新的 Critical 或 Important 问题。

## 第 2 轮审查修复

### RED

针对复审报告中的 1 项 Critical 与 2 项 Important，先新增五项回归测试并在
生产修改前运行。原实现均按预期失败：

```text
reclaimDoesNotExposeSourceTabBarSignals:
  !exposed returned FALSE
reclaimDoesNotExposeTargetTabBarSignals:
  !exposed returned FALSE
reclaimCommitSignalCannotReflowPage:
  !exposed returned FALSE
windowAboutToClosePrecedesAcceptedClose:
  visibleAtNotification returned FALSE
rejectedSystemCloseRollsBackAndClearsState:
  source indexOf(page) was -1, expected 0
```

### 实现

- 静默迁移同时阻断来源与目标 `ZzTabBar` 的公开信号，并通过 `QPointer` 保护
  信号阻断器；标签容器增加仅供 `ZzSplitWorkspace` 使用的静默状态，避免插入时
  pinned 归一化暴露中间状态。
- 静默顺序恢复不再调用会公开 `tabMoved` 的 `QTabBar::moveTab()`，改用
  `QTabWidget` 基类移除/插入并恢复完整标签元数据，最后显式同步当前标签和
  stacked widget。
- 回收成功不再合成公开 `tabTransferCommitted`，因此外部槽不能在关闭提交后
  销毁目标或把页面迁回已接受关闭的源窗口；普通用户迁移的公开提交信号保持不变。
- `ZzApplicationWindow::closeEvent()` 在所有事件过滤器通过并由基类最终接受后，
  同步调用协调器的私有接受回调。协调器只在此处执行回收和发出
  `windowAboutToClose`，成功后窗口才发出 `closeAccepted`；外部过滤器拒绝时不会
  启动回收，也不会留下 `CloseAccepted` 状态。
- 回调捕获协调器 `QPointer`，避免窗口析构晚于协调器或窗口析构清理记录时访问
  已销毁对象。审计失败用两个真实来源组构造第二步迁移失效，继续验证只记录
  rollback、不记录 commit。

### GREEN

```text
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceWindowCoordinatorTest \
           ZzMultiWindowIsolationTest \
           ZzWorkspaceCrossTransferTest --parallel 2
  PASS

ctest --preset linux-gcc-debug \
  -R '^puretools\.workspace-window-coordinator\.(reclaimDoesNotSuppressUnrelatedTransferOrClose|failedReclaimAuditContainsNoCommitSuccess)$' \
  --output-on-failure
  2/2 passed

ctest --preset linux-gcc-debug \
  -R '^puretools\.(workspace-window-coordinator|multi-window)|^fluent\.workspace-cross-transfer' \
  --output-on-failure
  58/58 passed
```

### 自审结论

逐项复核静默阶段的来源/目标 TabBar 信号、页面当前项同步、失败回滚、关闭策略
状态迁移、外部 Close 过滤器顺序及 `windowAboutToClose`/`closeAccepted` 时序。
`ZzApplicationWindow` 的改动仅保留最终关闭接受回调及协调器友元，没有扩展公开
API；未发现新的 Critical 或 Important 问题。

## 第 3 轮审查修复

### RED

针对复审剩余的 1 项 Critical，先在 `f138abd` 行为上增加并运行回归场景：

```text
aboutToCloseRejectsDestroyedTargetAndPreservesPage:
  closeWindow 返回成功，通知槽删除 target 后 source 页面已不在原组
aboutToCloseBlocksPublicTransferOutOfSource:
  通知发生时 source 页面已迁出，sourceSnapshotIntact 为 false
aboutToCloseBlocksPublicTransferOutOfSource（布局键 mutation）:
  冻结期间 setPageLayoutKey 仍返回成功
```

### 实现

- 将 `windowAboutToClose` 移到全部来源快照和目标预检完成之后、任何静默迁移
  之前；通知返回前页面仍由关闭来源窗口拥有。
- 通知期间以 `QPointer` 和 `QScopeGuard` 冻结来源 workspace 及全部预检目标
  workspace。跨工作区迁移、组内迁移、分组、布局恢复和页面布局键更新均由同一
  `transactionDepth` 栅栏拒绝。
- FluentUI 仅通过协调器 friend 可见的私有 begin/end 原语维护事务深度，
  PureTools 不包含 FluentUI 私有头。
- 通知后重新核验关闭窗口、Shell、来源 workspace、来源组顺序、每页原位置和
  稳定身份，以及全部目标 workspace、目标组和原页面顺序。任何参与者或快照
  变化都在尚未迁移时恢复关闭状态并拒绝关闭，不进入 rollback。
- 更新 `windowAboutToClose` Doxygen，明确页面已锁定、窗口即将回收并关闭，
  接收期间禁止公开迁移。

### GREEN

```text
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceWindowCoordinatorTest \
           ZzMultiWindowIsolationTest \
           ZzWorkspaceCrossTransferTest --parallel 2
  PASS

ctest --preset linux-gcc-debug \
  -R '^puretools\.(workspace-window-coordinator|multi-window)|^fluent\.workspace-cross-transfer' \
  --output-on-failure
  60/60 passed
```

架构定向检查中 `architecture.public-headers` 与 `architecture.boundaries` 通过；
`architecture.complete-audit` 仍只报告既存的三个类型前缀问题：
`ZzWorkspaceCrossTransferTransactionPrivate.cpp` 中的 `DepthGuard`、`TabsSnapshot`
和 `WorkspaceSnapshot`，本轮未修改该文件。

### 自审结论

通知槽删除唯一 target 时，目标 QPointer 失效并由通知后审计拒绝关闭，source 页
仍保持原组、原索引和身份；通知槽尝试公开迁移或布局键更新均被事务深度拒绝，
审计通过后正常静默回收。解冻 guard 只解引用存活 QPointer，参与 workspace 在
通知槽析构时不会产生悬空访问。
