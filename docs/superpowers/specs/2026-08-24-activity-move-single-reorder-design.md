# Activity move 单次物理重排设计

## 1. 文档目的

本文补充 WorkspaceShell Activity move 的物理应用合同。纯值规划器已经只改变一个
`movedId`，但当前 `applyProjection()` 仍按完整目标 `side.order` 逐位置调用
`ZzPanelStack::movePanel()`。首项移动到末尾时，这会依次移动其余 `n - 1` 个面板，形成
源码结构上的 `O(n^2)`，并发出与用户意图不符的 `n - 1` 次 `panelMoved`。

分层诊断的中位性质如下：

```text
128 项 forward placeSide: 11.91 ms, 128 次 movePanel, 127 次 panelMoved
512 项 forward placeSide: 183.8 ms, 512 次 movePanel, 511 次 panelMoved
增长: 15.4x
```

同一事务的 snapshot/planner、审计索引、side state、model reset、ActivityBar、sizes 和最终
审计均约为 3.4–4.0 倍增长。故本设计只修正 `placeSide` 的物理排序策略，不删除安全审计，
不修改 `ZzPanelStack` 公开接口，也不放宽 128/512 的 `< 10x` 门禁。

## 2. 已确认决策

- Activity move 事务每次只允许一个 `movedId` 改变 area 或 area-local row。
- 目标投影中除 `movedId` 外的所有 Side ID 必须保持原相对顺序。
- `applyProjection()` 只对 `movedId` 调用至多一次 `movePanel()`；已经位于目标索引时不调用。
- 目标 side 和源 side 在物理 mutation 前后都要核对完整身份顺序，不以单个索引检查代替。
- 每个 panel 的 record、content identity、owner、pane/stack ancestry 仍要审计。
- `ZzMutationObserver` 继续只允许本次 `movedId` 的预期 parent change 和 panel move；任何其他
  同步重排、接管、销毁或 model 污染使 strict apply 失败。
- rollback 使用同一单移动合同把 `movedId` 恢复到 snapshot，不建立第二套排序算法。
- 不新增 batch reorder API，不修改 `ZzPanelStack`，不保留已证伪的逐项本地镜像实验。

## 3. 范围

### 3.1 包含

- `ZzWorkspaceActivityMoveTransactionPrivate::applyProjection()` 中 Side 接管后的物理排序；
- 单 moved ID 投影合法性校验；
- 同侧前移/后移、跨侧、no-op 和 rollback 的完整顺序审计；
- `panelMoved` 信号只描述用户请求移动的 content；
- 128/512 Activity move 完整同步事务性能门禁。

### 3.2 不包含

- 不修改 Activity move 公开 API、`ZzActivityBar` 或 `ZzPanelStack` 公开 API；
- 不为任意全量布局恢复提供 batch reorder；
- 不改变 Activity model 全局稳定锚点语义；
- 不删除 mutation observer、边界审计、最终 projection/activity projection 审计或 rollback；
- 不增加永久缓存、timer、animation、线程、协程或 `processEvents()`；
- 不调整性能输入规模、sample 数量或 10 倍阈值。

## 4. 单移动不变量

设 mutation 前左右物理顺序分别为 `L0`、`R0`，目标为 `L1`、`R1`。合法 Activity move
必须满足：

1. `L0 + R0` 与 `L1 + R1` 的 ID 集合完全相等且无重复；
2. `movedId` 在 mutation 前后各出现一次；
3. 从四个列表中移除 `movedId` 后，每一侧保留下来的 ID 相对顺序不变；
4. 未跨侧时另一侧列表完全相等；跨侧时源侧只移除 `movedId`，目标侧只插入 `movedId`；
5. target projection 的 activity area、side membership 和 model order 对 `movedId` 一致。

该不变量由纯值 `buildActivityMoveTarget()` 建立，但 QObject mutation 前仍需基于固定 snapshot
和 target 再验证。验证失败时 strict apply 直接返回 false，不尝试把任意投影解释为单移动。

## 5. 应用流程

### 5.1 接管阶段

`placeSide()` 第一阶段继续按 target side 扫描并审计每个 record。只有 `movedId` 允许从另一
SidePane `takeWidget()` 后由目标 SidePane `addWidget()`；其他 ID 若不在预期 destination，
说明同步状态已经污染，strict apply 失败。

每个同步接管调用返回后继续验证：

- shell/pane/stack/content/record generation 身份稳定；
- content 的固定 frame owner、直接 parent 和 ancestry 合法；
- mutation observer 只消费预期的 moved content parent change；
- 非 moved panel 没有跨 pane。

### 5.2 排序阶段

接管完成后，对左右 PanelStack 各获取一次实际 `panels()`，映射为 ID，并执行：

1. 完整验证实际列表和目标列表的数量、唯一 ID 与非 moved 相对顺序；
2. 找到 `movedId` 所在目标 side 及 target index；
3. 若实际 moved index 已等于 target index，不调用 `movePanel()`；
4. 否则只调用一次 `movePanel(movedContent, targetIndex)`；
5. 同步信号返回后检查 observer、moved record/owner/boundary 和完整左右物理 ID 顺序；
6. 对所有非 moved ID 再执行 record、content identity 和 owner/ancestry 审计。

不得为了减少检查次数只比较 moved ID。性能优化减少的是 QObject mutation 次数，身份与完整
投影验证仍按线性扫描保留。

### 5.3 后续状态和 rollback

visibility、current、pane width/collapsed、Activity model rows、ActivityBar current/active、
sizes、edge visibility 和最终完整 projection 审计保持现有顺序。

strict target apply 失败后，既有 `applyProjection(snapshot, snapshotOrder, false)` 以同一个
`movedId` 执行反向单移动：跨侧时取回原 side，同侧时移回 snapshot index。rollback 完成后
仍由 `zzMovedRestored()` 和完整 snapshot audit 判定是否恢复；失败则进入既有 interrupted
cleanup，不增加新的静默恢复路径。

## 6. 信号语义

一次成功、非 no-op 的 Activity move 最多发出一次 `ZzPanelStack::panelMoved`，其 content
必须等于用户请求的 moved content，target index 必须等于目标 side 的最终物理索引。

- 同侧首项移到末尾：一次 moved content signal；
- 同侧末项移到首部：一次 moved content signal；
- 跨侧：`take/add` 的 parent change 按既有信号合同处理，目标 stack 只有在追加位置不是目标
  index 时才发出一次 moved content signal；
- 已在目标 area/row 的 no-op：零次 `panelMoved`；
- 同步回调额外移动其他 content：observer 失效，事务失败并 rollback。

该合同避免把因容器位移而改变 index 的相邻面板错误报告为用户主动移动的面板。

## 7. 复杂度

对于 `n` 个 Side panel：

- snapshot/target 单移动不变量检查为 `O(n)`；
- 一次实际 panels 捕获和 ID 映射为 `O(n)`；
- 查找 moved target index 为单次 `O(n)` 或局部 hash 查询；
- `ZzPanelStack::movePanel()` 至多调用一次，其内部线性查找和容器移动为 `O(n)`；
- 完整边界和投影审计保持 `O(n)`。

因此物理应用阶段为 `O(n)`，不再存在 `n` 次线性 `movePanel()`。局部额外空间为 `O(n)`，
不跨事务保存。

## 8. TDD 与验收

### 8.1 行为 RED

在 `ZzWorkspaceShellTest` 增加或收紧真实公开行为测试：

- 注册至少 6 个同侧面板，监听 `panelMoved`；
- 请求首项移到末尾，当前实现应发出 5 次其他 content 的 signal，形成 RED；
- 修复后只发出一次，signal content 等于请求 content，target index 为末尾；
- 再请求末项移到首部，同样只发出一次 moved content signal；
- 完整比较 Activity rows、PanelStack order、Area、current、active、visible、sizes 和 identity；
- 增加跨侧目标非尾部与 no-op，分别验证最多一次和零次 signal；
- 已有同步污染/rollback 用例继续通过。

测试不依赖私有 `ZzPanelStackPrivate`，只通过公开 Shell/ActivityBar intent 和公开
`ZzPanelStack::panelMoved` 观察行为。

### 8.2 性能 GREEN

保留 `activityMoveAuditScalesBelowQuadraticGrowth()`：

- 128/512 项，全部 `LeftPrimary`、visible/active；
- 预热后采集 5 个首尾往返 sample 并取中位数；
- 每次核对 moved title、Area、PanelStack target index、current 和 active；
- 完整事务 `largeMedian < smallMedian * 10`；
- 不把 fixture 注册放入计时区，不增加固定毫秒兜底。

还必须复跑：

- `movesActivityPanelsWithoutLosingStackState`；
- `restoresSavedSidePanelOrderAcrossRegistrationOrders`；
- 同步第三方接管、嵌套注册和失败 rollback 聚焦用例；
- 完整 `ZzWorkspaceShellTest` 或任务计划规定的分段全函数清单；
- 定向 WorkspaceShell CTest。

### 8.3 变异

把单移动 target index 临时偏移一位，行为测试或最终完整顺序审计必须失败；恢复实现后重新
构建并全绿。变异不得提交。

## 9. 文件与提交边界

本设计并入现有 Workspace planner 任务 2，允许修改：

- `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`；
- `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`。

不新增公开头，不修改 `ZzPanelStack`。实现与原 planner 线性化共同进入
`性能：线性化工作区布局规划器` 提交，正文必须记录本设计的根因数据、单 moved ID 合同、
实际信号次数、128/512 GREEN 倍率和回归结果。
