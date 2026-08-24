# Activity move 单次物理重排实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将 Activity move 从按完整目标顺序执行 `n` 次线性面板移动，收敛为只对 `movedId` 执行至多一次物理重排，同时保留完整同步审计与 rollback。

**架构：** 接管阶段只允许 `movedId` 跨 SidePane；排序阶段从实际 stack 和固定 target 构造一次性 ID 列表，先验证移除 moved ID 后的相对顺序完全一致，再只移动 moved content 到目标索引。每个 panel 的身份/owner/boundary 审计和最终 projection/activity audit 保持不变。

**技术栈：** Qt 6.8+ Widgets/Test、C++20、`QStringList`、`QHash`、`QPointer`、`QElapsedTimer`、CMake/CMakePresets、GCC 15。

---

## 设计与执行边界

设计规格：

`docs/superpowers/specs/2026-08-24-activity-move-single-reorder-design.md`

本计划是
`docs/superpowers/plans/2026-08-24-workspace-shell-order-planner-linearization.md`
任务 2 的执行补充，不创建第二个最终实现提交。实现与 planner 线性化仍共同提交为
`性能：线性化工作区布局规划器`。

当前事实基线：

```text
128 forward placeSide: 11.91 ms, 128 movePanel calls, 127 panelMoved signals
512 forward placeSide: 183.8 ms, 512 movePanel calls, 511 panelMoved signals
placeSide growth: 15.4x
all other transaction phases: approximately 3.4x to 4.0x
```

全局约束：

- 不修改公开 WorkspaceShell、ActivityBar 或 PanelStack API/ABI/信号声明；
- 一次 Activity move 只允许 `movedId` 改变 side/area/local row；
- `movePanel()` 至多调用一次，成功信号的 content 必须是 moved content；
- 每个 panel 的 record、content identity、owner、pane/stack ancestry 审计不得删除；
- mutation observer、rollback、最终 projection/activity projection 审计不得放宽；
- 不保留已证伪的逐项 PanelStack 本地镜像实验或临时诊断计时；
- 不新增永久缓存、batch reorder API、timer、线程、协程或 `processEvents()`；
- Activity move 128/512 中位数增长继续固定 `< 10x`；
- 不触碰主工作树、`temp_image/`、CI、GitHub CLI，不 push；
- 最终实现提交正文使用中文真实换行，详细记录单移动合同、RED/GREEN 和回归证据。

## 文件结构

- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`：单 signal RED、跨侧/no-op、同步污染和性能门禁。
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`：单移动校验与应用。

原任务 2 的另外两个 planner 文件继续按原简报完成，本补充不改变其算法合同。

---

### 任务 1：只重排用户请求的 Activity panel

**文件：**

- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp:2313-2465,3070-3160`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp:430-1380`

- [ ] **步骤 1：增加单 moved content 信号 RED**

在 `ZzWorkspaceShellTest` 增加
`activityMoveReordersOnlyRequestedPanel()`：

1. 在 LeftPrimary 依次注册 6 个 content，保存全部 raw pointer；
2. 为 stack 设置固定 sizes `{101, 202, 303, 404, 505, 606}`；
3. 连接 `ZzPanelStack::panelMoved`，按发生顺序记录 `{content, targetIndex}`；
4. 通过 left ActivityBar 把 model row 0 移到 LeftPrimary row 5；
5. 断言只收到一次 signal，content 是原 row 0，target index 为 5；
6. 断言 PanelStack、visibleWidgets、model title/Area、sizes 分别成为
   `{1,2,3,4,5,0}` 和 `{202,303,404,505,606,101}`；
7. 清空信号记录，把当前 row 5 移回 row 0，断言同样只收到 moved content 的一次 signal，
   所有顺序和 sizes 恢复；
8. 清空记录，对 row 0 发出同 area/same row no-op，断言零次 signal，完整状态不变；
9. 断言 current、active、owner/ancestry 与 `saveLayout()` 仍合法。

当前逐项重放实现应在第 5 步失败：前向移动会收到 5 次其他 content 的 `panelMoved`。

- [ ] **步骤 2：增加跨侧非尾部单移动覆盖**

在同一测试或独立
`activityMoveAcrossSidesReordersOnlyRequestedPanel()` 中：

1. LeftPrimary 注册 moved，RightPrimary 注册 `right-one`、`right-two`；
2. 分别监听左右 stack 的 `panelMoved`；
3. 请求 moved 到 RightPrimary row 1；
4. 断言左 stack 不发 `panelMoved`，右 stack 只发一次，content 为 moved，target index 为 1；
5. 断言左侧移除 moved，右侧顺序为 `{right-one, moved, right-two}`；
6. 断言 Activity model area、左右 current/active、visible、sizes、owner 和 `saveLayout()` 正确。

跨侧 `take/add` 的 parent change 不计作 `panelMoved`；若追加位置恰好是目标尾部，允许零次
`panelMoved`。

- [ ] **步骤 3：调整同步污染测试以适配唯一信号**

修改 `activityMoveStopsWhenAnEarlierPanelIsReparented()`：

- 仍注册 first/second/third，并把 first 请求到末尾；
- 在第一次正向 `panelMoved` 回调中，先记录 signal content 是 first、target index 是 2，
  再把非 moved 的 second reparent 到 `thirdParty`；
- 移除“等待第二次 signal”的旧触发条件；
- 事务返回后断言首个事件是 `{first, 2}`，后续若 rollback 发出第二个事件则也只能是 first
  回到 snapshot index 0；禁止任何 second/third signal；同时断言 second 仍由 thirdParty 持有、
  目标 model order 未提交、Shell 没有强取 thirdParty content。

该用例证明优化只减少正向物理 mutation，不会漏掉 moved-content 同步信号中对其他 panel 的
污染；失败 rollback 的反向 moved-content signal 仍按既有合同允许。

- [ ] **步骤 4：构建并运行新增行为测试确认 RED**

运行：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceShellTest --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  activityMoveReordersOnlyRequestedPanel \
  activityMoveAcrossSidesReordersOnlyRequestedPanel \
  activityMoveStopsWhenAnEarlierPanelIsReparented
```

预期：首项到末尾因 5 次 signal 而 RED；跨侧用例可因额外/错误 content signal RED；同步污染
用例不得因 fixture 或悬空 owner 失败。记录实际失败断言和退出码，不修改断言绕过 RED。

- [ ] **步骤 5：增加单移动值 helper**

在 `ZzWorkspaceActivityMoveTransactionPrivate.cpp` 匿名 namespace 增加带简体中文 Doxygen 的
helper：

```cpp
[[nodiscard]] QStringList zzWithoutId(
    const QStringList &ids,
    const QString &removedId);

[[nodiscard]] bool zzHasSingleMovedIdOrder(
    const QStringList &actual,
    const QStringList &target,
    const QString &movedId);
```

`zzWithoutId()` 按输入顺序输出所有不等于 movedId 的 ID。`zzHasSingleMovedIdOrder()` 必须验证：

- actual/target 数量相同；
- movedId 在 actual/target 各恰好一次，或在该 side 两边都不存在；
- 移除 movedId 后列表完全相等；
- 不接受空 ID、重复 ID 或未知 ID。

helper 不读取 QObject，不用 QHash/QSet 迭代输出。每侧只调用固定次数，整体线性。

- [ ] **步骤 6：限制接管阶段只允许 movedId 跨 SidePane**

保留 `placeSide` 的第一轮 record 扫描和所有 boundary audit。发现 `current != destination` 时：

- 若 `id != movedId_`，标记 incomplete；strict 模式立即失败，rollback 模式跳过该污染 ID；
- 只有 movedId 可以执行既有 `takeWidget()` / `addWidget()`；
- 每次 parent mutation 继续通过 `ZzMutationObserver::allowParentChange(movedContent)`；
- 同步返回后继续更新 moved content 的固定 frame identity，并调用 `zzBoundaryMatches()`；
- 第三方 parent、pane/stack 销毁或其他 content parent change 继续使 observer 无效。

不得为非 moved ID “顺便恢复”位置或 parent；该状态不是合法单 Activity move 投影。

- [ ] **步骤 7：用一次 movedId 重排替换完整目标逐项重放**

删除当前对 `side.order` 每个 index 调用 `movePanel()` 的第二轮循环，替换为每侧固定流程：

1. 调用一次 `stack->panels()`，通过 audit 的 widget->ID 映射得到 `actualOrder`；
2. 用 `zzHasSingleMovedIdOrder(actualOrder, side.order, movedId_)` 验证非 moved 相对顺序；
3. 对 `side.order` 的每个 ID 继续执行 record、content identity、owner 和
   `zzBoundaryMatches()`，不得只审计 moved ID；
4. 当前 side 不含 movedId 时要求 `actualOrder == side.order`，不调用 movePanel；
5. 当前 side 含 movedId 时，单次查找 actual index 和 target index；
6. 索引不同时只调用一次
   `destinationGuard->panelStack()->movePanel(movedContent, targetIndex)`，observer 只允许该
   moved content 的一次 panel move；
7. 同步返回后验证 observer、moved boundary，并重新获取完整 panels，要求 ID 顺序严格等于
   `side.order`；
8. 索引相同时不调用 movePanel，但仍做完整最终顺序/边界验证。

正向 apply 与 `applyProjection(snapshot, snapshotOrder, false)` rollback 共用该流程。

- [ ] **步骤 8：运行行为、同步污染和 rollback GREEN**

运行：

```bash
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceShellTest --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  activityMoveReordersOnlyRequestedPanel \
  activityMoveAcrossSidesReordersOnlyRequestedPanel \
  movesActivityPanelsWithoutLosingStackState \
  activityMoveStopsWhenAnEarlierPanelIsReparented \
  rollsBackActivityMoveWhenTargetIsDestroyedSynchronously \
  rollsBackActivityMoveWhenModelResetOverwritesPaneState \
  activityMoveRejectsPaneDestroyedByPanelMovedSignal \
  activityMoveRollbackRejectsPaneDestroyedByPanelMovedSignal
```

预期：全部通过；成功 move 至多一个 moved content signal，污染/销毁仍阻止错误 target 提交，
rollback 恢复或按既有 cleanup 合同结束。

- [ ] **步骤 9：复跑 128/512 完整事务性能 GREEN**

运行：

```bash
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  activityMoveAuditScalesBelowQuadraticGrowth
```

记录 128/512 五个 sample 的中位数和倍率，必须满足 `large < small * 10`。不得把分层诊断
计时、qInfo、临时阈值或固定毫秒兜底留在源码。

- [ ] **步骤 10：执行变异和完整任务 2 回归**

真实变异：临时把 moved target index 加一并约束到末尾，运行
`activityMoveReordersOnlyRequestedPanel` 和
`activityMoveAcrossSidesReordersOnlyRequestedPanel`，至少一个必须因 signal target 或完整顺序
失败；立即恢复、重建并复跑 GREEN。变异不得提交。

随后继续执行原任务 2 简报的完整验证：

```bash
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  activityMoveAuditScalesBelowQuadraticGrowth \
  movesActivityPanelsWithoutLosingStackState \
  restoresSavedSidePanelOrderAcrossRegistrationOrders
ctest --preset linux-gcc-debug \
  -R '^(puretools.workspace-layout-state-private|puretools.workspace-shell)$' \
  --output-on-failure
```

还要完成 planner 稳定锚点变异和 `contains/indexOf/find_if` 剩余扫描分类。任何长测试必须等待
最终退出码，不得把超时写成通过。

- [ ] **步骤 11：更新报告并形成原任务 2 提交**

把本设计的 RED、分层诊断、单移动 GREEN 信号次数、性能中位数/倍率、污染/rollback、变异和
完整回归追加到：

`.superpowers/sdd/2026-08-24-workspace-shell-order-planner-linearization/task-2-report.md`

只暂存原任务 2 四个文件：

```bash
git add \
  ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp \
  ZzPureTools/tests/ZzWorkspaceShellTest.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
git diff --cached --check
git diff --cached --name-only
git commit \
  -m "性能：线性化工作区布局规划器" \
  -m "为布局恢复建立局部集合与索引，以稳定锚点合并遗漏项；Activity model target 使用位置表，物理事务只对 movedId 执行至多一次 movePanel，同时保留逐面板边界和最终完整投影审计。" \
  -m "记录 planner 512/4096 与 Activity move 128/512 的真实 RED/GREEN、单 moved content 信号、同步污染和 rollback 回归、两项变异、扫描分类及定向 CTest 结果。"
```

staged 范围必须只包含上述四个文件。设计/计划和 ActivityBar 前置优化已经是独立提交，禁止
重复暂存、amend 或 squash；不要 push。
