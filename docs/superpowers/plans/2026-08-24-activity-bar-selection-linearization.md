# ActivityBar 选择同步线性化实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 消除 ActivityBar 投影反向映射与多激活去重的二次扫描，使 128/512 项 active source indexes 同步增长低于 10 倍，并解除 WorkspaceShell Activity move 的共享组件瓶颈。

**架构：** 投影模型继续只保存按源 row 递增的 `sourceRows_`，用二分查找完成反向映射，不增加长期位置缓存；多激活 setter 用调用期局部 `QSet<int>` 查询重复项，同时按输入顺序写入公开结果。组件优化独立提交并审查，随后由 WorkspaceShell 任务复跑完整事务门禁。

**技术栈：** Qt 6.8+ Widgets/Test、C++20、`std::lower_bound`、`QSet<int>`、`QElapsedTimer`、CMake/CMakePresets、GCC 15。

---

## 设计与执行边界

设计规格：

`docs/superpowers/specs/2026-08-24-activity-bar-selection-linearization-design.md`

执行环境：

```text
worktree: .worktrees/workspace-shell-transaction-redesign
branch: feature/workspace-shell-transaction-redesign
design commit: f0e3885
```

当前 worktree 同时含原任务 2 的未提交 planner/WorkspaceShell 修改。它们属于另一个实现者，
本计划不得读取后重写、暂存、恢复或提交。最终提交只能包含本计划列出的两个文件。

全局约束：

- 不修改 `ZzActivityBar` 公开头、公开 API、ABI、信号或模型角色；
- 不增加永久 source-row 哈希缓存或新的缓存失效协议；
- active indexes 的输出只由输入顺序决定，`QSet` 只用于 membership 查询；
- 无效索引、错误模型、非零列、child index 和当前 edge 之外 area 的过滤语义保持不变；
- 不删除或放宽 WorkspaceShell mutation observer、边界审计、rollback 或最终投影审计；
- 不保留已证伪的 PanelStack 本地镜像实验；
- 不使用 Qt Private API、stylesheet、链式 namespace、timer、协程或 `processEvents()`；
- 不处理 CI、GitHub CLI 或 push，不触碰主工作树和 `temp_image/`；
- 修改完成后立即形成独立中文提交，标题简述，正文用真实换行详细记录合同、实现和验证。

## 文件结构

- 修改：`ZzFluentUI/tests/ZzActivityBarTest.cpp`：增加稳定顺序、模型重置和 128/512 增长回归。
- 修改：`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp`：二分 source-row 反向映射和局部 active 去重。

---

### 任务 1：线性化 ActivityBar source index 接受路径

**文件：**

- 修改：`ZzFluentUI/tests/ZzActivityBarTest.cpp:1-735`
- 修改：`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp:240-680`

- [ ] **步骤 1：补齐测试测量辅助类型和 include**

在 `ZzActivityBarTest.cpp` 增加：

```cpp
#include <algorithm>

#include <QtCore/QElapsedTimer>
```

在匿名 namespace 增加只承载测量结果的值类型，使用简体中文 Doxygen：

```cpp
struct ZzActiveSelectionMeasurement final
{
    qint64 medianNanoseconds = 0;
    qsizetype checksum = 0;
};
```

增加 helper：

```cpp
[[nodiscard]] QList<QModelIndex> zzSourceIndexes(
    QAbstractItemModel *model);

[[nodiscard]] ZzActiveSelectionMeasurement zzMeasureActiveSelection(
    ZzFluentUI::ZzActivityBar *bar,
    const QList<QModelIndex> &indexes,
    int repetitions);
```

`zzSourceIndexes()` 只按 model row 从小到大建立 column 0 indexes，不做字符串构造。
`zzMeasureActiveSelection()` 固定采集 7 个 sample；每个 sample 启动 `QElapsedTimer` 后重复调用
`setActiveSourceIndexes(indexes)` 指定次数，把 `activeSourceIndexes().size()` 累加到 checksum；
对 7 个纳秒值排序并取中位数。helper 不使用 `QVERIFY/QCOMPARE`，空指针或非正 repetitions
返回零值。

- [ ] **步骤 2：增加 active 首次出现顺序与 reset 语义测试**

新增 `keepsFirstActiveOccurrenceAndRebuildsAfterReset()`：

1. 创建 6 行 2 列 `QStandardItemModel`，row 0/2/4 为 `LeftPrimary`，row 3/5 为
   `LeftSecondary`，row 1 为 `RightPrimary`；
2. 创建 left edge ActivityBar、设置 model、开启 multi-active；
3. 传入 `{row4, row0, row4, row1, child, column1, otherModelRow, row2, row0}`；
4. 断言公开 active indexes 严格为 `{row4, row0, row2}`，证明按首次出现顺序稳定去重且既有
   无效输入继续被过滤；
5. 再传相同输入并断言 `activeSourceIndexesChanged` 不增加；
6. 调用 `model.clear()`，断言 active 为空且清理通知只发生一次；
7. 重新建立同样 6 行/area，传入新 indexes，断言 Primary/Secondary 映射和有序 active 结果正确。

该测试在优化前允许通过；它锁定实现不得为性能改变公开语义。

- [ ] **步骤 3：增加 128/512 组件性能 RED**

新增 `activeSourceSelectionScalesBelowQuadraticGrowth()`：

1. 分别创建 128 行和 512 行、单列的 `QStandardItemModel`，所有 row area 均为
   `LeftPrimary`；
2. 为每个 model 创建独立 left ActivityBar，设置 model 并开启 multi-active；
3. 在计时区外构建完整 source indexes，并各预热一次；
4. 使用 `zzMeasureActiveSelection()`，每个 sample 重复 setter 3 次；
5. 断言两个 median 和 checksum 均为正；
6. 在计时区外完整比较公开 active indexes 与对应输入，确保测到的不是 no-op 错误路径；
7. 断言 `largeMedian < smallMedian * 10`，失败消息打印两组纳秒值与倍率。

不得把 model 构造、area data、index 列表构造或结果比较放入计时区；不得增加固定毫秒兜底、
缩小 128/512 输入或放宽 10 倍阈值。

- [ ] **步骤 4：运行组件语义和性能测试确认 RED**

运行：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
cmake --build --preset linux-gcc-debug \
  --target ZzActivityBarTest --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzFluentUI/tests/ZzActivityBarTest \
  keepsFirstActiveOccurrenceAndRebuildsAfterReset \
  activeSourceSelectionScalesBelowQuadraticGrowth
```

预期：语义用例通过；性能用例因 `mapFromSource()->indexOf()` 和
`setActiveSourceIndexes()->next.contains()` 接近二次增长而失败，倍率高于 10。记录实际两组
中位数、倍率、失败断言和退出码。若性能用例未 RED，只能增加每个 sample 的 repetitions，
不得更改规模或阈值。

- [ ] **步骤 5：用有序二分替换 sourceRows 线性查找**

在 `ZzActivityProjectionModel::mapFromSource()` 保留既有输入校验，随后实现：

```cpp
const auto position = std::lower_bound(
    sourceRows_.cbegin(), sourceRows_.cend(), index.row());
if (position == sourceRows_.cend() || *position != index.row()) {
    return {};
}
return this->index(
    static_cast<int>(position - sourceRows_.cbegin()), 0);
```

在 `refresh()` 扫描处增加简体中文短注释，说明源 row 按递增顺序追加是二分查找成立的不变量。
不得排序副本、不得增加 `QHash<int, int>` 成员，也不得遍历源模型以外的数据生成投影顺序。

- [ ] **步骤 6：用调用期局部集合稳定去重 active indexes**

在 multi-active 分支构造：

```cpp
QSet<int> seenRows;
seenRows.reserve(indexes.size());
```

继续先调用 `acceptsSourceIndex(index)`；接受后只在 `seenRows` 尚未包含 `index.row()` 时按输入
顺序 append `QPersistentModelIndex(index)`。由于 accepted index 已被限定为相同 source model、
无 parent、column 0，以 row 作为去重 identity 与现有 `QPersistentModelIndex` 等价。

`QSet` 只做 `contains/insert`，禁止遍历它产生 `next`；保留 `activeSourceIndexes == next` 的
无通知快路径、viewport update 和公开信号参数顺序。

- [ ] **步骤 7：运行 GREEN、完整组件回归和变异检查**

运行：

```bash
cmake --build --preset linux-gcc-debug \
  --target ZzActivityBarTest --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzFluentUI/tests/ZzActivityBarTest \
  keepsFirstActiveOccurrenceAndRebuildsAfterReset \
  activeSourceSelectionScalesBelowQuadraticGrowth \
  keepsOrderedValidMultipleActiveSourceIndexes \
  synchronizesSingleActiveIndexWithCurrentSourceIndex
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzFluentUI/tests/ZzActivityBarTest
ctest --preset linux-gcc-debug \
  -R '^fluent.activity-bar$' --output-on-failure
```

预期：聚焦、完整 QtTest 和 CTest `1/1` 全绿，性能倍率 `< 10x`。

执行一次真实变异：把二分命中条件临时改成只检查 iterator 未到末尾、忽略
`*position == index.row()`，运行 `keepsFirstActiveOccurrenceAndRebuildsAfterReset`，必须因当前
edge 之外或不存在的 source row 被错误接受而失败；立即恢复并复跑聚焦用例。变异不得提交。

最后运行：

```bash
rg -n 'indexOf\(|contains\(' \
  ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp
```

逐条记录剩余命中。允许非增长路径、单个键盘/drop 查找和 `QSet` membership；
`mapFromSource()` 禁止 `sourceRows_.indexOf()`，active indexes 构造循环内禁止
`next.contains()`。

- [ ] **步骤 8：只提交 ActivityBar 两个文件**

先确认 staged 范围，再提交：

```bash
git add \
  ZzFluentUI/tests/ZzActivityBarTest.cpp \
  ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp
git diff --cached --check
git diff --cached --name-only
git commit \
  -m "性能：线性化活动栏选择同步" \
  -m "利用投影源行天然递增的不变量，以二分查找替换反向映射的线性扫描；多激活输入使用调用期局部集合去重，同时保持首次出现顺序、过滤和信号合同。" \
  -m "新增重复输入、模型重置和 128/512 增长门禁，记录真实 RED/GREEN 中位数、倍率、变异、完整 ActivityBar QtTest 与定向 CTest 结果。"
```

`git diff --cached --name-only` 必须只打印上述两个文件。提交后允许原任务 2 的四个未提交文件
继续存在；不得把它们恢复、暂存或包含在本提交中。

---

## 后续集成

本任务通过独立规格/代码审查后，恢复原任务 2 实现者：

1. 移除其 `ZzWorkspaceActivityMoveTransactionPrivate.cpp` 中已证伪的 PanelStack 本地镜像实验；
2. 保留 `zzBuildTargetModelOrder()` 的线性位置索引和 no-op area 语义修复；
3. 复跑 `activityMoveAuditScalesBelowQuadraticGrowth()`，完整事务 128/512 中位数增长必须 `< 10x`；
4. 完成 planner GREEN、变异、扫描分类、定向 CTest 和原任务 2 独立提交/审查。
