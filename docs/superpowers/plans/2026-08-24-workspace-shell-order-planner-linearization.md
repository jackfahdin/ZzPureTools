# WorkspaceShell 主次顺序与规划器线性化实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 让任意合法 Side panel 注册顺序立即形成 Primary 在前、Secondary 在后的规范物理顺序，并将 workspace 纯值规划器从二次扫描收敛为平均线性时间。

**架构：** Side 注册在任何 QObject mutation 前固定目标 PanelStack 顺序，分阶段提交并在每个同步 Qt 信号边界审计完整身份和物理顺序；Activity model 的全局注册顺序保持不变。布局规划器为每次调用建立局部 `QHash/QSet` 索引，并用稳定锚点桶一次合并遗漏项；有序输入容器仍是输出顺序的唯一来源。

**技术栈：** Qt 6.8+ Widgets/Test、C++20、CMake/CMakePresets、GCC 15、Clang 20、`QPointer`、`QHash`、`QSet`、`QElapsedTimer`、ZzCore `ZzResult`。

---

## 设计与基线

设计规格：

`docs/superpowers/specs/2026-08-24-workspace-shell-order-planner-linearization-design.md`

执行分支和基线：

```text
worktree: .worktrees/workspace-shell-transaction-redesign
branch: feature/workspace-shell-transaction-redesign
design commit: 91040df
transaction implementation baseline: 68a5b0e
```

全局约束：

- 不修改 `ZzWorkspaceShell` 公开头、ABI、schema 1/2 或 Qt Dock state 版本；
- Activity model 全局 row 继续按成功注册的发生顺序排列；
- 每侧 PanelStack 固定为 `Primary rows + Secondary rows`；
- 同 area 注册顺序和 move 后的 area-local 顺序保持稳定；
- 同步回调第三方接管 content 后不得强制 reparent；
- planner 输出不得来自 `QHash/QSet` 迭代顺序；
- 不增加永久缓存、timer、animation、协程或 `processEvents()`；
- 不触碰主工作树原型和 `temp_image/`；
- 不调用 GitHub CLI、不处理 CI、不 push；
- 每项修改立即形成独立中文提交，标题简述，正文详细记录合同、实现和验证证据。

本计划不静默修复 rollback-failed 路径把 Secondary 强制映射为 Primary 的另一个审查发现；该
问题继续留在任务 9R 账本，等待独立设计和 TDD 批次。

## 文件结构

**任务 1 修改：**

- `ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`：计算 Side 注册固定目标、分阶段执行 PanelStack 接管与重排、审计同步信号后的完整身份和顺序。
- `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`：覆盖 Secondary-first、交错区域、全局 model 兼容、立即保存、round trip 和第三方同步接管。

**任务 2 修改：**

- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp`：建立局部规划索引、线性过滤/归一化和遗漏项锚点合并。
- `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`：使用一次性 model 位置索引生成 moved row 的稳定目标顺序。
- `ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp`：增加稳定锚点语义和 512/4096 增长门禁。
- `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`：收紧 Activity move 从线性到二次增长的区分门禁。

**任务 3 创建或修改：**

- 创建：`.superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/task-9r-order-linearization-report.md`：保存实际 RED/GREEN、构建、测试、sanitizer 和静态检查证据。
- 修改：`.superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/progress.md`：关闭两个 Important，保留未解决边界和环境失败。

---

### 任务 1：规范任意 Side 注册顺序

**文件：**

- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp:1880-2080`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp:433-528`

- [ ] **步骤 1：加入 Secondary-first 与交错注册失败测试**

在 `ZzWorkspaceShellTest` 增加
`secondaryFirstRegistrationUsesCanonicalSideOrderAndRoundTrips()`。按以下固定发生顺序注册：

```text
left-secondary-one
right-secondary-one
left-primary-one
right-primary-one
left-primary-two
left-secondary-two
```

每个 content 使用 `std::unique_ptr<QWidget>` 创建，注册成功后调用既有
`zzReleaseAfterAdoption()`。保存六个原始 `QWidget *`，并断言：

```cpp
QCOMPARE(leftPane->panelStack()->panels(),
    QList<QWidget *>({leftPrimaryOneRaw, leftPrimaryTwoRaw,
        leftSecondaryOneRaw, leftSecondaryTwoRaw}));
QCOMPARE(rightPane->panelStack()->panels(),
    QList<QWidget *>({rightPrimaryOneRaw, rightSecondaryOneRaw}));
```

从左右 ActivityBar 取得共享 model，按全局 row 逐项比较标题，顺序必须仍为上述六项注册发生
顺序；同时逐 row 比较 `ZzActivityItemRole::Area`。

为左右 stack 设置固定尺寸：

```cpp
QVERIFY(leftPane->panelStack()->setPanelSizes({111, 222, 333, 444}));
QVERIFY(rightPane->panelStack()->setPanelSizes({555, 666}));
```

立即调用 `saveLayout()` 并断言成功。创建第二个 `ZzShellFixture`，以 Primary-first 的不同顺序
注册同 ID 的六个新 content，恢复保存数据后断言：

- 两侧物理顺序仍为 Primary + Secondary；
- sizes 分别为 `{111, 222, 333, 444}` 和 `{555, 666}`；
- current、visibleWidgets、Activity current/active 与来源一致；
- 每个目标 content 仍由对应 SidePane/PanelStack 拥有；
- 再次 `saveLayout()` 成功且 envelope 可再次 restore。

- [ ] **步骤 2：加入 `panelMoved` 同步第三方接管和嵌套注册测试**

增加 `sideRegistrationDoesNotReclaimThirdPartyContentAfterPanelMove()`：

1. 先成功注册一个 `LeftSecondary`，确保随后注册 Primary 必须调用 `movePanel()`；
2. 创建无 parent 的 Primary content 和一个栈上 `QWidget thirdPartyOwner`；
3. 连接左侧 `ZzPanelStack::panelMoved`；当 moved content 等于 Primary 时，调用
   `leftPane->takeWidget(primaryRaw)`，校验返回同一指针，再执行
   `primaryRaw->setParent(&thirdPartyOwner)`；
4. 调用 `registerSidePanel()` 注册 Primary；
5. 断言回调只进入一次、注册返回 `InvalidState`、content parent 仍是 `thirdPartyOwner`；
6. 断言 `takePanel(primaryId)` 返回 `NotFound`，Activity model 和 PanelStack 只剩原 Secondary；
7. 断言 `saveLayout()` 对剩余合法状态成功。

测试结束前将 Primary content 从 `thirdPartyOwner` 解 parent，交回原 `unique_ptr` 清理，避免
父对象和智能指针同时持有生命周期造成测试自身歧义。

再增加 `sideRegistrationRollsBackOnlyOuterAfterReentrantRegistration()`：

1. 先注册一个 `LeftSecondary`；
2. 注册外层 `LeftPrimary`，在其 `panelMoved` 回调中通过 Shell API 注册另一个
   `LeftSecondary`，用布尔 guard 防止递归进入；
3. 断言嵌套注册成功、外层注册因固定 stack/model 目标被污染而返回 `InvalidState`；
4. 断言外层 ID 为 `NotFound`，原 Secondary 和嵌套 Secondary 的 content/owner/model row 均保留；
5. 断言剩余 PanelStack 同 area 顺序稳定，`saveLayout()` 成功。

- [ ] **步骤 3：运行聚焦用例确认 RED**

运行：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceShellTest --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  secondaryFirstRegistrationUsesCanonicalSideOrderAndRoundTrips \
  sideRegistrationDoesNotReclaimThirdPartyContentAfterPanelMove \
  sideRegistrationRollsBackOnlyOuterAfterReentrantRegistration
```

预期：第一条因左/右 PanelStack 仍保持注册发生顺序或 `saveLayout()` 返回 `InvalidState` 而
失败；第二条可因当前注册路径尚未产生目标 `panelMoved` 或未完成目标审计而失败。不得用修改
断言绕过 RED。在当前执行会话保留实际失败断言和退出码，任务 3 再一次性写入报告；本步骤
不创建未提交的报告草稿，也不提交失败代码。

- [ ] **步骤 4：在 mutation 前构造固定物理顺序**

在 `ZzWorkspaceShellPrivate.cpp` 匿名 namespace 增加只依赖值状态的 helper，并添加简体中文
Doxygen：

```cpp
[[nodiscard]] bool zzIsPrimaryArea(
    ZzFluentUI::ZzActivityArea area) noexcept;

[[nodiscard]] int zzSideRegistrationTargetIndex(
    const QVector<ZzWorkspaceShellPrivate::ZzPanelRecord> &panels,
    ZzFluentUI::ZzActivityArea area) noexcept;
```

`zzSideRegistrationTargetIndex()` 只扫描 mutation 前 registry：计入 kind 为 Side、content 身份
有效且未处于 removal 的记录；Primary 返回本侧现有 Primary 数量，Secondary 返回本侧现有
全部 Side 数量。必须计入已经预留的 `registrationInProgress` 记录，以便嵌套注册形成自己的
固定目标。不要依赖枚举整数排序。

在 `registerSidePanel()` 通过所有入口校验后、追加 registry record 前捕获：

```cpp
const QVector<ZzSideLayoutEntry> rowsBefore = activityRows();
const int targetStackIndex =
    zzSideRegistrationTargetIndex(panels, area);
const QPointer<QMainWindow> hostGuard(host);
const QPointer<QWidget> rootGuard(workspaceRoot);
const QPointer<ZzFluentUI::ZzSidePane> paneGuard(pane);
const QPointer<ZzFluentUI::ZzPanelStack> stackGuard(pane->panelStack());
const QPointer<QWidget> contentGuard(content);
```

将 mutation 前 `stackGuard->panels()` 转成 `QList<QPointer<QWidget>> appendOrder`，在尾部加入
content；复制后在 `targetStackIndex` 插入 content 得到 `canonicalOrder`。两个列表都必须只含
mutation 前身份和新 content，不得在信号返回后重建。

- [ ] **步骤 5：实现分阶段身份与完整顺序审计**

在 `registerSidePanel()` 内定义局部 audit lambda。它接收当前阶段的固定期望顺序，并验证：

```text
host/root/pane/stack/content QPointer 与 Shell 当前字段相同
stablePanelIndex(expectedRecord) >= 0
content identity、registration generation 和 registrationInProgress 未变
stack->panels() 与期望 QPointer 列表逐项同身份且数量相同
pane 和 stack ancestry 都包含 content
content 未由第三方 parent 接管
```

固定提交顺序为：

1. 追加 registry record 并捕获 `const ZzPanelRecord expectedRecord`；
2. `paneGuard->addWidget()` 后审计 `appendOrder`；
3. `paneGuard->setCurrentWidget()` 后再次审计 `appendOrder`；
4. 当 `targetStackIndex` 不是追加索引时调用
   `stackGuard->movePanel(contentGuard, targetStackIndex)`；
5. `movePanel()` 返回后审计 `canonicalOrder`；
6. Activity model 继续使用 `append()`，随后比较完整 `activityRows()` 等于
   `rowsBefore + newPlacement`，证明全局 model 顺序没有被重排；
7. 更新两侧 current、owning bar active、pane collapsed 和 edge visibility；
8. 每个同步 setter 返回后重新审计 record、subsystem identity 和 `canonicalOrder`；
9. 最后清除 `registrationInProgress`，再次确认稳定后返回成功。

任一步失败统一调用 `rollbackPanelRegistration(id, contentIdentity)`。若 `contentGuard->parent()`
已经是第三方对象，rollback 只能清 registry/model/connection，不调用 pane API 强取。保留当前
错误码 `InvalidState` 和现有技术消息风格。

若同步回调成功注册了另一个合法 ID，完整 stack/model 期望会检测到额外身份，使外层注册失败
并只回滚自己的 generation；不得删除或回滚同步回调成功拥有的另一个 panel。

- [ ] **步骤 6：运行聚焦和完整 WorkspaceShell 测试确认 GREEN**

运行：

```bash
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceShellTest --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  secondaryFirstRegistrationUsesCanonicalSideOrderAndRoundTrips \
  sideRegistrationDoesNotReclaimThirdPartyContentAfterPanelMove \
  sideRegistrationRollsBackOnlyOuterAfterReentrantRegistration \
  preservesRegistrationOrderAndUpdatesBadges \
  reservesSideIdDuringSynchronousRegistrationSignals \
  movesActivityPanelsWithoutLosingStackState
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest
ctest --preset linux-gcc-debug \
  -R '^puretools.workspace-shell$' --output-on-failure
```

预期：聚焦用例全部通过，完整 `ZzWorkspaceShellTest` 为 0 failed，定向 CTest 为 1/1。特别
确认既有 `preservesRegistrationOrderAndUpdatesBadges` 仍保持全局 model 注册顺序。

- [ ] **步骤 7：提交任务 1**

运行：

```bash
git add \
  ZzPureTools/tests/ZzWorkspaceShellTest.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp
git diff --cached --check
git commit \
  -m "修复：统一侧栏主次面板注册顺序" \
  -m "将每侧 PanelStack 固定为 Primary 行在前、Secondary 行在后，并保持同一区域注册顺序；Activity model 的全局注册顺序保持兼容。\n\n注册前固定目标索引和完整身份顺序，在 add、current、move 与 Activity 同步信号返回后审计 host、pane、stack、content、generation、owner 和 model。Secondary-first 注册现在可立即保存并 round trip；第三方同步接管时返回 InvalidState，只清理自身记录而不强取对象。记录聚焦与完整 WorkspaceShell 测试结果。"
```

提交后运行 `git status --short`，工作树必须干净。

---

### 任务 2：线性化纯值规划器和 Activity 目标排序

**文件：**

- 修改：`ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp:1-640`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp:5800-5840`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp:1-610`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp:454-508`

- [ ] **步骤 1：增加小规模锚点语义测试**

在 `ZzWorkspaceLayoutStatePrivateTest` 增加
`alternatingOmissionsKeepStableAnchorsAndSizes()`。snapshot 左侧固定为：

```text
order:   a, b, c, d, e, f, g
visible: a, b, c, d, e, f, g
sizes:   101, 102, 103, 104, 105, 106, 107
activity.leftPrimary: a, b, c, d, e, f, g
```

request projection 左侧只描述重排后的 `f, b, d`，对应 requested sizes
`{606, 202, 404}`；其余项省略，右侧为空。断言现有逐项插入语义的固定结果：

```text
order/visible: e, f, g, a, b, c, d
sizes:         105, 606, 107, 101, 202, 103, 404
```

snapshot current 固定为 `a`，`request.leftCurrent` 固定为 `f`；最终 current 必须是 `f`，active
set 必须包含全部七项。`contents.panelId` 和 `activity.leftPrimary` 都必须等于上述最终 order，
确保线性化不改变投影语义。该小测试是行为金丝雀，不以耗时作为通过条件。

- [ ] **步骤 2：增加 512/4096 最差路径性能 RED**

在测试匿名 namespace 增加：

```cpp
using ZzLayoutState =
    ZzPureTools::ZzWorkspaceLayoutStatePrivate;

struct ZzPlannerFixture final
{
    ZzLayoutState::ZzWorkspaceSnapshot snapshot;
    ZzLayoutState::ZzLayoutRequest request;
    QStringList expectedOrder;
    QList<int> expectedSizes;
};

struct ZzPlannerMeasurement final
{
    qint64 medianNanoseconds = 0;
    quint64 checksum = 0;
};

[[nodiscard]] ZzPlannerFixture zzAlternatingPlannerFixture(int count);
[[nodiscard]] std::optional<ZzPlannerMeasurement> zzMeasurePlanner(
    const ZzPlannerFixture &fixture,
    int repetitions);
```

新增 include：

```cpp
#include <algorithm>
#include <optional>
#include <QtCore/QElapsedTimer>
```

`zzAlternatingPlannerFixture()` 生成零填充、稳定且唯一的 ID：

```cpp
const QString id = QStringLiteral("side-%1")
    .arg(index, 4, 10, QLatin1Char('0'));
```

所有 ID 都进入 snapshot identity、left order/visible、sizes 和 leftPrimary。request projection
只保留偶数 ID；偶数 ID 使用 requested size `2000 + index`，奇数 ID 省略并应恢复 snapshot
size `100 + index`。expected order 仍是完整 snapshot order，expected sizes 按奇偶来源构造。

增加 `restorePlannerScalesBelowQuadraticGrowth()`：

1. 构造 512 和 4096 两组 fixture；
2. 分别预热一次；
3. 每组采集 7 个 sample，每个 sample 重复规划 3 次；
4. helper 内 target 不存在或 order size 不符时立即返回 `std::nullopt`，不得在非 void helper
   内使用 `QVERIFY`；成功时将首尾 sizes 加入 checksum；
5. 对 sample 排序并取中位数；
6. 测试函数断言 measurement 存在、median/checksum 为正；
7. 测量结束后在计时区外各规划一次，完整比较两组 target 的 order、visible、sizes、contents 和
   activity；
8. 断言 `largeMedian < smallMedian * 20`，失败消息打印两个 ns 值和倍率。

不要把 fixture 构造、QString 格式化或 expected 比较放入计时区。不要增加固定 `+250 ms` 兜底。

- [ ] **步骤 3：收紧 Activity move 增长门禁**

将 `activityMoveAuditAvoidsCubicGrowth()` 重命名为
`activityMoveAuditScalesBelowQuadraticGrowth()`：

- panel count 从 192/384 改为 128/512；
- 每组先执行一次不计时 move 预热；
- 采集 5 个 move sample 并取中位数；
- 每次在首尾之间往返，避免后续 sample 对同一 no-op 位置计时；
- 每次验证 moved title、Area 和 PanelStack 目标 index；
- 最终断言 `largeMedian < smallMedian * 10`。

该用例计时区只包含 `moveRequested` 同步事务，不包含 fixture 注册。

- [ ] **步骤 4：运行性能与语义用例确认 RED**

运行：

```bash
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest \
  --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest \
  alternatingOmissionsKeepStableAnchorsAndSizes \
  restorePlannerScalesBelowQuadraticGrowth
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
  activityMoveAuditScalesBelowQuadraticGrowth
```

预期：小规模锚点语义可以通过；512/4096 增长门禁因当前重复 `QStringList::contains/indexOf`
接近二次增长而失败。Activity 门禁若当前已低于 10 倍可以通过，它用于锁定后续不回退；任务 2
的强制 RED 证据来自 planner 最差路径。

若 planner 性能用例未 RED，先核对计时区确实调用带 request projection 的
`buildRestoreTarget()`，且奇数项确实进入遗漏恢复。只允许增加每个 sample 的 repetitions 或
修正 fixture，不允许将倍率上限改大。

- [ ] **步骤 5：建立稳定 membership 与 identity 索引**

在 `ZzWorkspaceLayoutStatePrivate.cpp` 增加 `QHash`、`QSet` include，并在匿名 namespace 定义
私有局部类型：

```cpp
struct ZzSnapshotIndex final
{
    QHash<QString, const ZzLayoutState::ZzPanelIdentity *> identitiesById;
    QSet<QString> registeredSideIds;
    QHash<QString, ZzFluentUI::ZzActivityArea> activityAreas;
};

struct ZzSideIndex final
{
    QSet<QString> orderIds;
    QSet<QString> visibleIds;
    QHash<QString, int> sizesById;
};
```

为两个类型提供带简体中文 Doxygen 的 builder。builder 先按输入 size `reserve()`，再单次扫描；
重复 ID 保留第一次出现的确定性语义。`registeredSideIds` 的兼容规则必须与当前实现一致：

- identity 明确为 Side 时注册；
- Side order 中没有任何 identity 的 legacy ID 也注册；
- identity 明确为 Bottom/Dock 时，即使错误出现在 Side order 也不得注册为 Side。

把 claimed、unique、visible 和 Split group membership 的 `QStringList` 改为 `QSet<QString>`；
输出列表仍按输入列表扫描写入。

- [ ] **步骤 6：实现线性稳定锚点合并**

用以下私有值类型替换 `zzSnapshotInsertionIndex()` 和逐遗漏项 insert：

```cpp
struct ZzStableMergeResult final
{
    QStringList order;
    QList<int> sizes;
};

[[nodiscard]] ZzStableMergeResult zzMergeOmittedBySnapshotAnchors(
    const QStringList &requestedOrder,
    const QList<int> &requestedSizes,
    const QStringList &snapshotOrder,
    const QList<int> &snapshotSizes,
    const QSet<QString> &omittedIds);
```

实现固定为：

1. 建立 `requested ID -> target position`；
2. 对 snapshot 做反向扫描，记录每个位置最近的后继 target position；
3. 对 snapshot 做正向扫描，维护最近前驱 target position；
4. 遗漏 ID 优先进入后继 position 的 before bucket；没有后继才进入前驱 position 的 after
   bucket；两者都没有则进入 unanchored tail；
5. 按 requested position 输出 before、requested ID、after，最后输出 unanchored tail；
6. requested ID 使用 requested size，omitted ID 使用 snapshot size；缺失或非正尺寸归一为 1。

before/after bucket 使用按 position 索引的 `QVector<QStringList>` 与 `QVector<QList<int>>`，不得
用 `QHash` 迭代输出。空 sizes 输入时仍生成与 order 对齐的正尺寸，保持现有 normalize 合同。

在 `zzReconcileSerializedSideProjection()` 中：

- 先以 registered/claimed set 过滤左右 requested order；
- 再分别收集左右 snapshot omitted set；
- 对左右 order/visible 各调用一次稳定 merge；
- Activity 四个 area 分别过滤 requested rows，并对对应 snapshot area 一次稳定 merge；
- 删除逐 ID 的 `zzRestoreOmittedActivityRow()`。

- [ ] **步骤 7：线性化其余 normalize 和 runtime identity 路径**

逐项替换以下重复扫描：

- `zzUniqueNonEmpty()`：`QSet` 去重并保持第一次出现顺序；
- `zzNormalizeSide()`：一次构造 order set，一次同步重建 visible/sizes；
- `zzNormalizeBottom()`：order set 过滤 visible，并用 set 判断 current fallback；
- `zzCollectSplitGroupOrder()`：递归时传入 seen set，叶子只插入一次；
- `zzNormalizeSplit()`：tree group set 与 requested group set 单次过滤/补全；
- `zzRestoreRuntimeIdentities()`：snapshot dock map 和 identity map 按 ID 查询；
- current/active 判断复用已构造 Side set；
- `zzRegisteredSideIds()`、`zzFilterRequestedSideOrder()` 和
  `zzFilterActivityRows()` 的 claimed 参数全部改为 `QSet<QString> *`。

单次 `buildActivityMoveTarget()` 中对一个 moved ID 执行的两侧 remove 和 size 查询本身是
`O(n)`，可以保留；不得为了消除单次线性扫描引入长期 cache。

- [ ] **步骤 8：线性化 Activity move model anchor 查询**

在 `zzBuildTargetModelOrder()` 中：

1. 从 snapshot model order 单次生成移除 moved ID 后的 `order`；
2. 建立 `QHash<QString, qsizetype> orderPositions`；
3. 在目标 area list 中定位 moved ID；
4. 向后或向前扫描目标 area list时，以 hash 查询 anchor 是否存在；
5. 在固定 insertion index 插入 moved ID。

不得在循环内调用 `order.indexOf()`。保留 target area 的 before/after 语义和全局 model order
中未移动 row 的相对顺序。

- [ ] **步骤 9：运行 GREEN、语义回归和变异检查**

运行：

```bash
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest \
  --parallel 2
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

预期：private state test 全部通过，三个 WorkspaceShell 聚焦用例通过，定向 CTest 2/2。

执行一次真实变异：把 `zzMergeOmittedBySnapshotAnchors()` 的 before bucket 输出临时改到 anchor
之后，运行 `alternatingOmissionsKeepStableAnchorsAndSizes`，必须失败；立即恢复实现并复跑全绿。
变异不得提交。

使用 `rg` 审查目标文件：

```bash
rg -n 'contains\(|indexOf\(|find_if' \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
```

逐条分类剩余命中。允许单次 moved ID 查询、QHash/QSet membership 和 QWidget/PanelStack 身份
查询；禁止任何循环体内对随 panel/group 数量增长的 `QStringList::contains/indexOf`。

- [ ] **步骤 10：提交任务 2**

运行：

```bash
git add \
  ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp \
  ZzPureTools/tests/ZzWorkspaceShellTest.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
git diff --cached --check
git commit \
  -m "性能：线性化工作区布局规划器" \
  -m "为 Side、Bottom、Split、Activity 与 runtime identity 建立单次局部索引，并以稳定前后锚点桶合并序列化中遗漏的面板；输出仍只由输入有序列表决定，不依赖哈希迭代顺序。\n\n移除按遗漏项重复 contains、indexOf 和 find_if 的二次扫描，Activity move 使用一次性 model 位置表。新增锚点语义测试和 512/4096 最差路径增长门禁，收紧 Activity move 线性增长回归，并记录 RED、GREEN、变异与定向 CTest 结果。"
```

提交后确认状态中没有生产或测试文件残留修改。

---

### 任务 3：执行最终门禁并关闭审查发现

**文件：**

- 创建：`.superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/task-9r-order-linearization-report.md`
- 修改：`.superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/progress.md`

- [ ] **步骤 1：核对提交边界和工作树**

运行：

```bash
git status --short --branch
git log --oneline 91040df..HEAD
git diff --check 91040df..HEAD
git diff --stat 91040df..HEAD
```

预期：只有任务 1、任务 2 两个代码提交；工作树干净；diff 只触及计划列出的生产和测试文件。

- [ ] **步骤 2：运行 Linux GCC Debug 完整目标验证**

运行：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
cmake --preset linux-gcc-debug \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest
ctest --preset linux-gcc-debug \
  -R '^(puretools.workspace-layout-state-private|puretools.workspace-layout-codec-private|puretools.workspace-shell|architecture.boundaries|architecture.complete-audit|architecture.fluent-visual-token-contract)$' \
  --output-on-failure
```

记录 target 构建数量、两个 Qt Test 的 passed/failed 数和定向 CTest 总数。任何新增失败必须先
定位并修复，不能进入报告的“既有失败”列表。

- [ ] **步骤 3：运行 Release、static 和 sanitizer 定向验证**

依次运行：

```bash
cmake --preset linux-gcc-release \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-release \
  --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
ctest --preset linux-gcc-release \
  -R '^(puretools.workspace-layout-state-private|puretools.workspace-shell)$' \
  --output-on-failure

cmake --preset linux-static-release \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-static-release \
  --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
ctest --preset linux-static-release \
  -R '^(puretools.workspace-layout-state-private|puretools.workspace-shell)$' \
  --output-on-failure

export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
cmake --preset linux-clang-asan \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-clang-asan \
  --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
ctest --preset linux-clang-asan \
  -R '^(puretools.workspace-layout-state-private|puretools.workspace-shell)$' \
  --output-on-failure
```

预期三套各 2/2 通过，ASan/UBSan 无报告。若 Clang 仍在基线测试的
`request.projection.emplace()` 处失败，报告精确文件、行号和编译器诊断，保持为既有前序
阻塞；不得把失败写成 sanitizer 通过，也不得在本任务混入未设计的兼容修复。

- [ ] **步骤 4：运行 clang-tidy 和跨平台静态检查**

先尝试：

```bash
CC=/usr/bin/clang-20 CXX=/usr/bin/clang++-20 \
  cmake --preset linux-clang-tidy-release \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build --preset linux-clang-tidy-release \
  --target ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceShellTest --parallel 2
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy
```

再运行源码静态检查：

```bash
! rg -n 'namespace [A-Za-z_][A-Za-z0-9_]*::' \
  ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
! rg -n 'Qt[A-Za-z]+/private|Qt[A-Za-z]+Private|setStyleSheet|styleSheet\(' \
  ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
! rg -n '__attribute__|__builtin_|#pragma GCC|#pragma clang' \
  ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
```

以上扫描作为 Windows MSVC/Qt SDK MinGW 与 macOS AppleClang 的静态可移植性证据，只能记录
“未发现平台专用扩展”，不能记录三平台运行通过。

- [ ] **步骤 5：复跑已知全量 CTest 并分类，不混修范围外失败**

运行：

```bash
ctest --preset linux-gcc-debug --output-on-failure
```

将结果分为：

- 本批修改相关测试；
- 既有 workspace screenshot 基线；
- 既有 `OrderedPage` 架构命名；
- Example 硬编码 `/home/zz/.qttest/.../logs` 只读路径；
- 新出现的其他失败。

前三类既有问题只如实记录；任何新失败都必须回到对应代码任务修复并创建新的中文修复提交，
然后从步骤 1 重新验证。

- [ ] **步骤 6：执行独立代码审查**

使用 `superpowers:requesting-code-review` 审查精确 diff `91040df..HEAD`。审查重点固定为：

```text
Secondary-first 是否在注册成功后立即可保存
Activity model 全局注册顺序是否保持
完整 stack/model 固定目标是否抵抗同步重入
第三方 owner 是否从未被强取
锚点桶是否逐输入顺序确定性输出
是否仍有随 panel/group 数量形成的嵌套全列表扫描
性能测试是否真的区分 8x 线性和 64x 二次增长
是否修改公开 ABI、schema、Qt Private API 或跨平台边界
```

Critical/Important 必须先修复并独立中文提交，再做一次定向复审；不得在报告中静默 parked。
Minor 必须说明是否影响本批完成合同。

- [ ] **步骤 7：写入真实验证报告和账本**

创建 `task-9r-order-linearization-report.md`，固定包含：

```markdown
# Task 9R order and planner linearization report

## Commits
## Secondary-first RED and GREEN
## Planner 512/4096 RED and GREEN
## Semantic mutation evidence
## GCC Debug/Release/static
## ASan/UBSan
## Clang and clang-tidy
## Full CTest classification
## Windows/MinGW/macOS static portability
## Independent review
## Remaining parked findings
```

每节写真实命令、退出码、passed/failed 数和关键倍率；没有执行的平台明确写“未运行”。在
`progress.md` 追加两个 Important 的关闭提交和审查结论，同时继续保留：

- codec error-code mapping Minor；
- 不支持的 ParentChange 销毁边界；
- rollback-failed Secondary area 风险；
- 仍未解决的环境或前序 clang/full CTest 失败。

- [ ] **步骤 8：提交任务 3 文档证据**

运行：

```bash
git add \
  .superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/progress.md \
  .superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/task-9r-order-linearization-report.md
git diff --cached --check
git commit \
  -m "文档：记录工作区顺序与性能验收结果" \
  -m "记录 Secondary-first 注册和 512/4096 规划增长门禁的 RED/GREEN、语义变异、GCC shared/static、sanitizer、clang-tidy、全量 CTest 分类及跨平台静态检查证据。\n\n关闭侧栏主次顺序冲突与规划器二次复杂度两个 Important；继续保留 rollback-failed Secondary area、ParentChange 不支持边界及既有环境失败，不将未运行平台表述为通过。"
```

最后运行：

```bash
git status --short --branch
git log --oneline 91040df..HEAD
git diff --check 91040df..HEAD
```

预期：隔离工作树干净，提交链依次为顺序修复、性能修复、验证文档；不包含主工作树原型、
`temp_image/`、构建产物或测试日志。

## 执行完成判定

只有以下各项同时成立才能宣称本计划完成：

1. 左右两侧所有 Secondary-first/交错注册测试通过，成功注册后立即 `saveLayout()`；
2. PanelStack 为 Primary + Secondary，同 area 稳定，Activity model 全局注册顺序不变；
3. `panelMoved` 同步第三方接管返回 `InvalidState` 且 owner 未被强取；
4. 锚点小样例的 order、sizes、Activity 和 contents 完全符合既有语义；
5. 512/4096 planner 增长从 RED 转为低于 20 倍，Activity 128/512 低于 10 倍；
6. 目标源码不再存在对 panel/group 数量构成二次增长的嵌套列表扫描；
7. GCC Debug/Release/static 和 sanitizer 的实际状态有命令证据；
8. 独立审查没有未解决的本批 Critical/Important；
9. 三个实施提交均使用中文标题和详细中文正文；
10. 工作树干净，未触碰或提交范围外文件。
