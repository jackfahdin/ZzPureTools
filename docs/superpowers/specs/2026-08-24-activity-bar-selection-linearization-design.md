# ActivityBar 选择同步线性化设计

## 1. 文档目的

本文补充 `WorkspaceShell` 主次顺序与规划器线性化设计中 Activity move 的性能实现边界。
当前纯值 `buildActivityMoveTarget()` 在 128/512 项输入下增长为 `3.43x`，但包含
`ZzActivityBar` 同步的完整事务仍增长约 `13.9x`，未满足已确认的 `< 10x` 门禁。

分层测量和源码追踪表明，主导复杂度不在 target model order，也不在实际
`ZzPanelStack::movePanel()` 次数，而在模型重置后 ActivityBar 重建 active source indexes：

- `ZzActivityProjectionModel::mapFromSource()` 对有序 `sourceRows_` 使用线性 `indexOf()`；
- `ZzActivityBarPrivate::setActiveSourceIndexes()` 对不断增长的结果使用线性 `contains()`。

当 active indexes 与面板数一同增长时，两处重复扫描形成源码结构上的 `O(n^2)`。
本设计在不改变公开行为、不削弱事务审计且不增加长期缓存失效协议的前提下收敛该路径。

## 2. 已确认决策

- 保持 `ZzActivityBar` 公开头、公开方法、信号、ABI 和模型角色不变。
- `sourceRows_` 继续作为投影模型的唯一行映射存储，不增加永久 source-row 哈希缓存。
- `sourceRows_` 由源模型从小到大扫描生成，固定保持严格递增；反向映射使用二分查找。
- active source indexes 使用单次调用内的局部集合去重，输出仍保持输入首次出现顺序。
- 不遍历集合生成公开顺序，不改变无效索引、错误模型、错误列或当前 edge 之外 area 的过滤语义。
- 不删除或放宽 Activity move 的 mutation observer、同步边界审计、rollback 或最终完整投影审计。
- 已证伪的 PanelStack 本地顺序镜像实验不进入最终提交。

## 3. 范围

### 3.1 包含

- `ZzActivityProjectionModel::mapFromSource()` 的有序二分查找；
- `ZzActivityBarPrivate::setActiveSourceIndexes()` 的局部哈希去重；
- ActivityBar 重复输入、首次出现顺序、模型重置和 area 过滤回归；
- WorkspaceShell Activity move 128/512 完整同步事务增长门禁；
- Linux GCC 15 运行验证和任务 3 的跨平台静态检查。

### 3.2 不包含

- 不新增 `ZzActivityBar` 公开 API；
- 不把 source-row 位置表保存为长期成员；
- 不修改 Activity model reset 协议或引入增量 rowsMoved API；
- 不修改 `ZzPanelStack`；
- 不改变 active indexes 的可观察顺序或重复项折叠语义；
- 不使用 timer、后台线程、协程、`processEvents()` 或固定时间兜底；
- 不放宽 128/512 输入增长 `< 10x` 的验收阈值。

## 4. 组件设计

### 4.1 投影行反向映射

`ZzActivityProjectionModel::refresh()` 仍按源模型 row 从 `0` 到 `rowCount - 1` 扫描，
只把属于当前 area 的 row 追加到 `sourceRows_`。因此 `sourceRows_` 天然严格递增。

`mapFromSource()` 在完成既有 model、parent、column 和 row 合法性检查后，使用
`std::lower_bound` 查找源 row。只有 iterator 未到末尾且值完全相等时才创建投影 index；
不存在时返回无效 `QModelIndex`。单次查询从 `O(n)` 变为 `O(log n)`，额外空间为 `O(1)`。

该实现不缓存依赖源模型生命周期的额外位置表，模型 reset、insert、remove、move、layout 和
area data change 仍只需执行既有 `refresh()`，不存在新的失效分支。

### 4.2 Active indexes 去重

`setActiveSourceIndexes()` 在 multi-active 模式下创建调用期局部 `QSet<int>`，并按调用方
传入顺序扫描 indexes：

1. 先复用 `acceptsSourceIndex()` 过滤无效、错误模型、错误列和当前 edge 之外的索引；
2. 以已经验证属于同一 source model、column 0 的 source row 作为去重键；
3. 第一次出现时把 row 加入集合，并把对应 `QPersistentModelIndex` 追加到结果；
4. 重复出现时跳过。

集合只用于 membership 查询，绝不用于输出遍历，因此 `activeSourceIndexes()` 和
`activeSourceIndexesChanged` 继续保持输入首次出现顺序。局部集合按输入数量 `reserve()`，
调用结束即释放，不形成长期缓存。

### 4.3 事务边界

`ZzWorkspaceActivityMoveTransactionPrivate` 继续负责完整事务：捕获快照、构建 target、应用
Pane/PanelStack/Activity 状态、同步审计、失败回滚和最终投影核对。本设计只降低 ActivityBar
接受 source indexes 时的查询复杂度，不绕过任何事务步骤。

任务 2 已实现的 `zzBuildTargetModelOrder()` 位置索引继续保留。此前用于验证假设的
PanelStack 本地镜像跳过逻辑应恢复，因为它没有降低完整事务倍率，并引入了跨侧临时状态的
额外推理负担。

## 5. 复杂度

设源模型有 `n` 行，本次 active indexes 有 `m` 项：

- 四个投影模型在 model reset 后各自单次扫描源模型，合计仍为 `O(n)`；
- 每个 active index 的 area 接受检查执行有限次二分查找，为 `O(log n)`；
- active 去重的平均 membership 查询为 `O(1)`，有序输出扫描为 `O(m)`；
- 整体为 `O(n + m log n)`，局部额外空间为 `O(m)`。

该复杂度不存在源码结构上的二次全列表扫描。128 到 512 项放大 4 倍时，完整 Activity move
事务的实测中位数增长必须小于 10 倍。

## 6. 错误与兼容性

- 单选模式继续只保留 current source index；
- multi-active 模式继续过滤不可接受索引并稳定折叠重复项；
- source model reset 后 persistent indexes 的清理和重建行为保持不变；
- `currentSourceIndexChanged`、`activeSourceIndexesChanged` 的触发条件保持不变；
- Primary/Secondary 投影和左右 edge 过滤结果必须与优化前完全一致；
- 任何同步回调污染仍由 Workspace transaction 返回失败并按既有合同回滚。

## 7. TDD 与验收

### 7.1 语义测试

在既有 `ZzActivityBarTest` 增加或收紧真实模型测试，至少覆盖：

- multi-active 输入包含重复 source indexes 时，公开结果只保留第一次出现且顺序稳定；
- 无效索引、其他模型索引、非零列和当前 edge 之外 area 仍被过滤；
- model reset 后重新设置 active indexes，Primary/Secondary 映射与公开 source indexes 正确；
- current 与 active 信号不因实现改为二分/QSet 而产生额外通知。

### 7.2 性能与集成门禁

- 保留任务 2 的 `activityMoveAuditScalesBelowQuadraticGrowth()`；
- 128/512 两组预热后各采集 5 个 sample，中位数增长必须 `< 10x`；
- 每次往返 move 都继续断言 title、Area、PanelStack index、current 和 active 投影；
- 不把 fixture 注册、字符串生成或预期比较放入计时区；
- 不增加固定毫秒兜底，不降低输入规模，不提高倍率阈值。

至少运行：

```bash
cmake --build --preset linux-gcc-debug \
  --target ZzActivityBarTest ZzWorkspaceShellTest --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzFluentUI/tests/ZzActivityBarTest
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  activityMoveAuditScalesBelowQuadraticGrowth \
  movesActivityPanelsWithoutLosingStackState \
  restoresSavedSidePanelOrderAcrossRegistrationOrders
```

任务 2 还必须完成原计划要求的 planner 语义、512/4096 性能、变异、定向 CTest 和剩余扫描
分类。任务 3 统一执行 Linux 多配置、sanitizer、clang-tidy 及 Windows/MSVC、MinGW、macOS
静态可移植性检查。

## 8. 文件与提交边界

任务 2 在原四个文件基础上允许增加：

- `ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp`；
- `ZzFluentUI/tests/ZzActivityBarTest.cpp`。

最终仍形成一个任务 2 实现提交，详细记录 planner 与 ActivityBar 两处复杂度合同、真实
RED/GREEN 倍率、变异和回归证据，不提交诊断埋点或已证伪实验。
