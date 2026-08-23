# WorkspaceShell 事务引擎重构实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development
> 逐任务实现此计划。步骤使用复选框（`- [ ]`）跟踪进度；每个任务必须先 RED、再 GREEN、
> 独立中文提交并接受规格与质量审查。

**目标：** 用不可变快照和请求派生目标投影重构 WorkspaceShell 布局恢复与 Activity move，
消除同步 Qt 信号污染预期状态、悬空对象和不完整回滚，同时保持公开 API 与 schema 2 不变。

**架构：** 私有纯值规划器在任何 QObject mutation 前构造固定目标；codec 独立完成 schema
1/2 和 Split 子布局的有界解析；布局事务与 Activity move 事务只执行固定计划，并在每个
同步信号边界后核验 identity、generation、membership、ancestry 和完整终态投影。失败使用
原始快照经同一执行器反向恢复，禁止从 mutation 后的实况学习 expected/checkpoint。

**技术栈：** Qt 6.8+ Widgets/Test、C++20、CMake/CMakePresets、GCC 15、Clang 20、
QPointer/QScopeGuard、ZzCore::ZzResult、Qt QDataStream/SHA-256。

**设计规格：**
`docs/superpowers/specs/2026-08-23-workspace-shell-transaction-redesign-design.md`

---

## 全局约束

- `ZzWorkspaceShell` 公开头和 schema 2 字节合同不变；只新增 private 类和 private 测试。
- 公开 Qt signals 与直接组件 API 的同步重入允许发生；事务必须检测污染并回滚。
- Shell 的 register/take/show/badge/save/restore 在事务门开启时返回 `InvalidState`；Activity
  activate/move 不执行 mutation；置顶、应用标题和自定义标题保持独立。
- `setTitleMode()` 保持既有 `void` 签名；外部同步覆盖由固定 title projection 检测。
- QWidget 已由第三方接管时不得强取，只能报告 rollback failed 并清理失真记录。
- 不支持页面自定义 ParentChange/event filter 在换父期间删除无关 workspace 对象。
- 禁止 Qt Private API、stylesheet、链式 namespace、业务模型访问和平台专用未隔离代码。
- 所有新增自定义类型使用 `Zz` 前缀；复杂逻辑和接口使用简体中文 Doxygen。
- `temp_image/` 不读取、不修改、不暂存、不提交。
- 不调用 GitHub CLI、不处理 CI、不 push。
- 每任务最多五轮审查修复；第 5 轮仍有承重 Critical/Important 时熔断。

## 执行准备

实现前使用 `superpowers:using-git-worktrees`。主工作树保留未提交的任务 9 原型作为证据，
新实现从本计划提交后的干净 HEAD 开始，绝不复制原型中的生产事务代码。

1. 检查当前不是 linked worktree：

   ```bash
   git rev-parse --git-dir
   git rev-parse --git-common-dir
   git rev-parse --show-superproject-working-tree
   ```

2. 若 `.worktrees/` 尚未被忽略，向 `.gitignore` 增加一行 `.worktrees/`，只暂存该文件并提交：

   ```bash
   git add .gitignore
   git diff --cached --check
   git commit -m "构建：忽略本地开发工作树" \
     -m "将项目内 .worktrees 目录排除出版本控制，避免隔离开发目录被误暂存。"
   ```

3. 创建隔离分支和工作树：

   ```bash
   git worktree add \
     .worktrees/workspace-shell-transaction-redesign \
     -b feature/workspace-shell-transaction-redesign
   cd .worktrees/workspace-shell-transaction-redesign
   ```

4. 使用本机环境，不下载 Qt：

   ```bash
   export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
   export GCC_13=/usr/bin/gcc-15
   export GXX_13=/usr/bin/g++-15
   cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
   cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
   ctest --preset linux-gcc-debug -R '^puretools.workspace-shell$' --output-on-failure
   ```

   预期：任务 8 基线 WorkspaceShell 测试全绿。全量 CTest 的 4 个截图基线失败和
   `OrderedPage` 命名架构失败属于账本中的既有问题，不在本步骤混修。

5. 初始化新的 SDD 账本：

   ```bash
   /home/zz/.agents/skills/subagent-driven-development/scripts/sdd-workspace \
     docs/superpowers/plans/2026-08-23-workspace-shell-transaction-redesign.md
   ```

## 文件结构

**新增生产文件：**

- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`

**新增测试文件：**

- `ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp`
- `ZzPureTools/tests/ZzWorkspaceLayoutCodecPrivateTest.cpp`

**修改文件：**

- `ZzPureTools/CMakeLists.txt`
- `ZzPureTools/tests/CMakeLists.txt`
- `ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

`ZzWorkspaceLayoutStatePrivate` 与 codec 只依赖 QtCore/QtWidgets、ZzCore 和 FluentFoundation
的枚举，不依赖 `Zz::PureTools`。两个 private test 直接编译相应 private `.cpp`，避免导出
内部 ABI；事务类仅通过 WorkspaceShell 公共行为测试。

---

### 任务 1：建立不可变布局值模型与纯规划器

**文件：**

- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp`
- 创建：`ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/tests/CMakeLists.txt`

- [ ] **步骤 1：新增 pure planner 失败测试。**

  测试使用字符串 PanelId 和纯值 identity，不构造 Shell，不 mock QObject mutation。至少包含：

  ```cpp
  void sideFallbackUsesPaneCurrentInsteadOfActivityCurrent()
  {
      ZzWorkspaceLayoutStatePrivate::ZzWorkspaceSnapshot snapshot;
      snapshot.leftSide.order = {QStringLiteral("one"), QStringLiteral("two")};
      snapshot.leftSide.visible = {QStringLiteral("one"), QStringLiteral("two")};
      snapshot.leftSide.current = QStringLiteral("two");
      snapshot.activity.leftCurrent = QStringLiteral("one");

      ZzWorkspaceLayoutStatePrivate::ZzLayoutRequest request;
      request.leftCurrent = QStringLiteral("unknown");

      const auto target =
          ZzWorkspaceLayoutStatePrivate::buildRestoreTarget(snapshot, request);
      QVERIFY(target.has_value());
      QCOMPARE(target->leftSide.current, QStringLiteral("two"));
  }
  ```

  ```cpp
  void moveTargetDoesNotChangeWhenObservedStateChanges()
  {
      const auto snapshot = zzTwoSideSnapshot();
      const auto target = ZzWorkspaceLayoutStatePrivate::buildActivityMoveTarget(
          snapshot, QStringLiteral("terminal"),
          ZzFluentUI::ZzActivityArea::RightPrimary, 0);
      QVERIFY(target.has_value());

      auto observed = snapshot;
      observed.rightSide.visible.clear();
      QCOMPARE(target->rightSide.visible,
               QStringList({QStringLiteral("terminal")}));
  }
  ```

  另外覆盖空边缘强制折叠、unknown panel 保留快照状态、visible/sizes 对齐、Activity
  current/active 从 Side target 派生、invalid move 返回 `std::nullopt`。

- [ ] **步骤 2：运行目标确认 RED。**

  ```bash
  cmake --build --preset linux-gcc-debug \
    --target ZzWorkspaceLayoutStatePrivateTest --parallel 2
  ```

  预期：目标或 `ZzWorkspaceLayoutStatePrivate` 尚不存在，构建失败；失败原因必须是缺少目标
  API，不得是 include 路径或 CMake 拼写错误。

- [ ] **步骤 3：实现只含值的状态与规划器。**

  主类提供嵌套 `ZzPanelIdentity`、`ZzSideProjection`、`ZzBottomProjection`、
  `ZzDockProjection`、`ZzSplitProjection`、`ZzActivityProjection`、
  `ZzTitleProjection`、`ZzWorkspaceProjection`、`ZzWorkspaceSnapshot`、
  `ZzLayoutRequest` 和 `ZzActivityMoveRequest`。

  公开给私有调用方的最小静态接口固定为：

  ```cpp
  [[nodiscard]] static std::optional<ZzWorkspaceProjection>
  buildRestoreTarget(
      const ZzWorkspaceSnapshot &snapshot,
      const ZzLayoutRequest &request);

  [[nodiscard]] static std::optional<ZzWorkspaceProjection>
  buildActivityMoveTarget(
      const ZzWorkspaceSnapshot &snapshot,
      const QString &panelId,
      ZzFluentUI::ZzActivityArea targetArea,
      int targetRow);

  [[nodiscard]] static bool equals(
      const ZzWorkspaceProjection &left,
      const ZzWorkspaceProjection &right) noexcept;
  ```

  Planner 使用复制后的值状态模拟固定操作序列。所有 expected 在返回后按 `const` 对象保存；
  类中不提供从 observed 覆盖 target 的 setter。

- [ ] **步骤 4：配置 private test。**

  `ZzWorkspaceLayoutStatePrivateTest` 直接编译 state private `.cpp`，链接：

  ```cmake
  Qt6::Test
  Qt6::Core
  Qt6::Widgets
  Zz::FluentFoundation
  ```

  设置 `AUTOMOC ON`、项目 warnings、sanitizers 和 `QT_QPA_PLATFORM=offscreen`，CTest 名称为
  `puretools.workspace-layout-state-private`。

- [ ] **步骤 5：验证 GREEN 与变异敏感性。**

  ```bash
  cmake --build --preset linux-gcc-debug \
    --target ZzWorkspaceLayoutStatePrivateTest --parallel 2
  ctest --preset linux-gcc-debug \
    -R '^puretools.workspace-layout-state-private$' --output-on-failure
  ```

  预期：全部通过、无 warning。临时将 Side fallback 改为 Activity current 时第一条测试必须
  失败；恢复实现后再次全绿。

- [ ] **步骤 6：提交并审查。**

  ```bash
  git add \
    ZzPureTools/CMakeLists.txt \
    ZzPureTools/tests/CMakeLists.txt \
    ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp
  git diff --cached --check
  git commit -m "框架：建立工作区不可变布局投影" \
    -m "新增纯值快照、恢复目标和 Activity move 目标规划器。\n\n固定 Side current fallback、空边缘归一化和 Activity 派生规则，并以独立 private 测试证明目标不会学习 observed 状态。"
  ```

---

### 任务 2：提取有界 Workspace 与 Split codec

**文件：**

- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp`
- 创建：`ZzPureTools/tests/ZzWorkspaceLayoutCodecPrivateTest.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/tests/CMakeLists.txt`

- [ ] **步骤 1：编写 schema 和 Split codec 失败测试。**

  固定测试内的独立 v1/v2 encoder，覆盖：

  ```cpp
  void migratesSchemaOneIntoConcreteSchemaTwoRequest();
  void roundTripsSchemaTwoWithoutChangingBytesContract();
  void writerRejectsSplitStateThatReaderRejects();
  void canonicalSplitTargetRejectsLayoutChangedInjection();
  void boundsAllCountsBeforeAllocation_data();
  void boundsAllCountsBeforeAllocation();
  ```

  `boundsAllCountsBeforeAllocation_data()` 精确包含每侧 visible 32/33、side entries
  4096/4097、Split groups 64/65、depth 16/17、nodes 127/128、saved pages 4096/4097、
  ID/key 256/257、1 MiB 边界、截断、重复 ID/order、非法 enum、digest mutation。

- [ ] **步骤 2：运行 RED。**

  ```bash
  cmake --build --preset linux-gcc-debug \
    --target ZzWorkspaceLayoutCodecPrivateTest --parallel 2
  ```

  预期：codec 类和测试目标不存在。

- [ ] **步骤 3：实现 codec 最小接口。**

  ```cpp
  class ZzWorkspaceLayoutCodecPrivate final
  {
  public:
      [[nodiscard]] static ZzCore::ZzResult<
          ZzWorkspaceLayoutStatePrivate::ZzLayoutRequest>
      decode(const QByteArray &encoded);

      [[nodiscard]] static ZzCore::ZzResult<QByteArray>
      encodeVersionTwo(
          const ZzWorkspaceLayoutStatePrivate::ZzLayoutRequest &request);

      [[nodiscard]] static ZzCore::ZzResult<QByteArray>
      canonicalizeSplit(const QByteArray &encoded);
  };
  ```

  codec 必须先验证 count 再 reserve，Split parser 返回纯值树并由同一 writer 规范化。schema 1
  decode 直接构造完整 request：原始根组结构、current tab index 和默认折叠 Bottom，不把迁移
  延迟到 QWidget mutation 阶段。

- [ ] **步骤 4：配置独立 codec test。**

  测试目标直接编译 codec/state 两个 private `.cpp`，链接 Qt6::Test、Qt6::Core、
  Qt6::Widgets、Zz::Core 和 Zz::FluentFoundation，不链接 Zz::PureTools。

- [ ] **步骤 5：运行 codec GREEN 和旧格式证据。**

  ```bash
  cmake --build --preset linux-gcc-debug \
    --target ZzWorkspaceLayoutCodecPrivateTest --parallel 2
  ctest --preset linux-gcc-debug \
    -R '^puretools.workspace-layout-codec-private$' --output-on-failure
  ```

  预期：全部通过；writer-reader 对称，任何 reader 拒绝的 Split state 都不能被 writer 接受。

- [ ] **步骤 6：提交并审查。**

  ```bash
  git add \
    ZzPureTools/CMakeLists.txt \
    ZzPureTools/tests/CMakeLists.txt \
    ZzPureTools/tests/ZzWorkspaceLayoutCodecPrivateTest.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp
  git diff --cached --check
  git commit -m "框架：提取工作区有界布局编解码" \
    -m "独立实现 schema 1 迁移、schema 2 编解码和 Split 子布局规范化。\n\n统一 writer 与 reader 校验器，在分配前限制全部数量、深度、标识和总字节边界。"
  ```

---

### 任务 3：实现不可变 Activity move 事务

**文件：**

- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [ ] **步骤 1：从原型测试资产逐条重建 Activity RED。**

  只迁移测试，不复制原型生产代码。新增并单跑：

  ```text
  movesActivityPanelsWithoutLosingStackState
  rollsBackActivityMoveWhenTargetIsDestroyedSynchronously
  rollsBackActivityMoveWhenModelResetOverwritesPaneState
  cleansActivityMoveWhenContentIsDestroyedSynchronously
  cleansActivityMoveWhenSourceIsDestroyedSynchronously
  leavesThirdPartyOwnerWhenActivityMoveIsInterceptedSynchronously
  rejectsNestedSideRegistrationDuringActivityMove
  activityMoveRejectsPaneDestroyedByPanelMovedSignal
  activityMoveRollbackRejectsPaneDestroyedByPanelMovedSignal
  activitySyncRejectsPaneDestroyedByActiveStateSignal
  activityMoveRejectsFinalSizesSignalOverride
  activityMoveRejectsThirdPartyOwnerAfterFinalSizesSignal
  activityMoveRejectsModelReplacementAfterReset
  ```

  最后三条是第 5 轮阻塞发现的强制 RED：`panelSizesChanged` 回调分别隐藏 moved content、
  reparent 到第三方和删除 activity model。

- [ ] **步骤 2：验证 RED 由未实现 move 合同触发。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
    activityMoveRejectsFinalSizesSignalOverride
  ```

  预期：旧 task 8 Shell 没有提交 move 或错误保留污染状态；不得以测试崩溃作为最终 RED，
  生命周期用例若先崩溃应通过 QPointer fixture 收敛为可断言失败。

- [ ] **步骤 3：统一 Shell 事务门。**

  在 `ZzWorkspaceShellPrivate` 使用单一枚举替代两个松散 bool：

  ```cpp
  enum class ZzTransactionKind : std::uint8_t {
      None,
      LayoutRestore,
      ActivityMove
  };
  ZzTransactionKind transactionKind = ZzTransactionKind::None;
  ```

  Activity move 进入时用 RAII 设为 `ActivityMove`。所有布局敏感结果型 API 和内部 Activity
  activate/move 按全局约束拒绝重入；不得用多个 bool 形成非法组合。

- [ ] **步骤 4：实现 Activity move 事务类。**

  ```cpp
  class ZzWorkspaceActivityMoveTransactionPrivate final
  {
  public:
      explicit ZzWorkspaceActivityMoveTransactionPrivate(
          ZzWorkspaceShellPrivate &shell) noexcept;
      [[nodiscard]] bool execute(
          const QModelIndex &sourceIndex,
          ZzFluentUI::ZzActivityArea targetArea,
          int targetRow);
  };
  ```

  Prepare 捕获固定 snapshot，并调用任务 1 planner 得到 `const target`。Executor 每次调用
  take/add/move/visible/current/sizes/replaceRows 后立即检查：Pane、PanelStack、model、content
  QPointer 与 raw identity、record generation、stack membership 和 ancestry。最后一次
  `setPanelSizes()` 前已经持有固定 target；其信号返回后不得捕获 checkpoint。

- [ ] **步骤 5：实现同一执行器回滚。**

  正向和回滚都调用 `applyProjection(const ZzWorkspaceProjection&)`。第三方接管时不 reparent，
  返回 false 并调用 Shell 既有 interrupted removal cleanup 收敛注册表。modelReset 后重新要求
  `modelGuard == shell.activityModel`。

- [ ] **步骤 6：验证 Activity 全套 GREEN。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  for test_name in \
    movesActivityPanelsWithoutLosingStackState \
    rollsBackActivityMoveWhenTargetIsDestroyedSynchronously \
    rollsBackActivityMoveWhenModelResetOverwritesPaneState \
    activityMoveRejectsFinalSizesSignalOverride \
    activityMoveRejectsThirdPartyOwnerAfterFinalSizesSignal \
    activityMoveRejectsModelReplacementAfterReset; do
      build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest "$test_name"
  done
  ```

  预期：每次 QtTest 包含 init/cleanup 均为 3 passed、0 failed；第三方 owner 保持第三方 parent。

- [ ] **步骤 7：提交并审查。**

  ```bash
  git add \
    ZzPureTools/CMakeLists.txt \
    ZzPureTools/tests/ZzWorkspaceShellTest.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp
  git diff --cached --check
  git commit -m "框架：重构活动面板迁移事务" \
    -m "以原始快照和移动意图预先构造固定 Pane 与 Activity 目标。\n\n在全部同步信号边界审计模型、容器、内容身份和实际所有权，失败时使用原投影回滚且不夺回第三方内容。"
  ```

---

### 任务 4：实现 Workspace 布局恢复事务

**文件：**

- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [ ] **步骤 1：重建 schema、完整恢复和三条最终阻塞 RED。**

  从原型测试资产逐条迁移 v1/v2 encoder 与以下行为测试，不复制生产代码：

  ```text
  migratesVersionOneLayoutToVersionTwo
  boundsVersionOneLayoutDtos
  keepsQtMainWindowStateVersionIndependentFromEnvelopeVersion
  restoresCompleteVersionTwoWorkspaceState
  boundsVersionTwoLayoutDtos
  boundsVersionTwoSideStateWhenSaving
  rejectsInvalidSplitStateWhenSavingWorkspaceLayout
  restoreRejectsReentrantSideTransactions
  restoreRejectsReentrantBottomAndDockTransactions
  restoreRejectsDestroyedSubsystemBeforeSuccess
  restoreRejectsSideOwnershipLostAfterCommit
  restoreRejectsBottomContentOutsideStack
  restoreRejectsBottomCurrentOverwrittenWithinCommit
  restoreRejectsSideCurrentOverwrittenWithinCommit
  restoreRejectsTitleModeOverwrittenWithinCommit
  rollbackReportsDeletedBottomContent
  restoreRejectsLaterOverwriteOfCommittedSideState
  commitsWorkspaceSubsystemsInDocumentedOrder
  rollsBackEveryCommittedSubsystemWhenBottomCommitFails
  rollbackSynchronizesActivityAfterOwnershipAudit
  restoreRejectsSourcePaneDestroyedByCurrentWidgetSignal
  restoreRollsBackWhenTargetPaneIsDestroyedDuringTake
  restoreCleansRegistrationWhenContentGetsThirdPartyOwner
  ```

  新增三条熔断用例：

  ```cpp
  void restoreUsesPaneCurrentForUnknownCurrentFallback();
  void restoreRejectsSplitMutationFromLayoutChanged();
  void restorePreservesIndependentTitleAndAlwaysOnTopChanges();
  ```

  第一条先直接用公开 `sidePane()->setCurrentWidget(two)` 制造 Side current 与 Activity current
  不同，再恢复 unknown/empty current；第二条在 Split `layoutChanged` 中调用 `splitGroup()`；
  第三条证明 application/custom title 与 always-on-top 不被布局回滚覆盖。

- [ ] **步骤 2：运行目标 RED。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
    restoreUsesPaneCurrentForUnknownCurrentFallback
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
    restoreRejectsSplitMutationFromLayoutChanged
  ```

  预期：task 8 只支持 schema 1，或 Split/Side 污染未被固定 projection 检测；失败信息必须指向
  行为差异。

- [ ] **步骤 3：实现事务入口和固定 prepare。**

  ```cpp
  class ZzWorkspaceLayoutTransactionPrivate final
  {
  public:
      explicit ZzWorkspaceLayoutTransactionPrivate(
          ZzWorkspaceShellPrivate &shell) noexcept;
      [[nodiscard]] ZzCore::ZzResult<QByteArray> save() const;
      [[nodiscard]] ZzCore::ZzResult<void> restore(
          const QByteArray &encoded);
  };
  ```

  `restore()` 在 mutation 前依次完成 codec decode、subsystem/panel snapshot、shadow QMainWindow
  Dock target、Split canonical target 和纯 planner target，然后把 target 存为 const。

- [ ] **步骤 4：实现 Qt Dock shadow target。**

  创建不 show 的局部 QMainWindow；为每个注册 Dock 创建同 objectName 的临时 QDockWidget，
  先应用 snapshot area/floating/visible，再 `restoreState(request.qtState, 1)`，捕获目标逻辑
  projection。shadow 不连接 Shell 信号，不接管业务 content，离开 prepare 时全部销毁。

- [ ] **步骤 5：实现固定五阶段提交和阶段审计。**

  顺序必须为：

  ```text
  Qt Dock -> Split -> Side -> Bottom -> Activity/Title
  ```

  每个 QObject mutation 后检查相关 guard/identity/generation/owner；每阶段 observed 仅用于
  `equals(observed, expected)`。Split 终态对比 prepare 阶段 canonical blob；不得在
  `restoreLayout()` 返回后保存成 expected。Side fallback 使用 snapshot Side current。

- [ ] **步骤 6：实现统一反向回滚。**

  失败顺序：

  ```text
  Activity/Title -> Bottom -> Side -> Split -> Qt Dock
  ```

  复用同一 phase executor，以 snapshot projection 为 const target。回滚结束审计全部 panel
  kind；缺失 Bottom/Dock、membership/ancestry 分裂、generation replacement 或第三方 owner
  都必须产生包含 `rollback failed` 的 `InvalidState`。

- [ ] **步骤 7：缩减 Shell Private。**

  `saveLayout()` 和 `restoreLayout()` 只构造事务对象并转发；删除 Shell Private 中 codec、
  Split parser、learned projection、阶段 apply 和重复 rollback 大块。保留生命周期、注册表、
  标题连接和事务所需的最小 friend/accessor。不得同时保留新旧两套实现。

- [ ] **步骤 8：运行完整 WorkspaceShell GREEN。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest
  ctest --preset linux-gcc-debug \
    -R '^(puretools.workspace-shell|fluent.split-workspace|fluent.side-pane|fluent.bottom-pane)$' \
    --output-on-failure
  ```

  预期：WorkspaceShell 全部用例通过、无 warning；相关组件测试全绿。

- [ ] **步骤 9：提交并审查。**

  ```bash
  git add \
    ZzPureTools/CMakeLists.txt \
    ZzPureTools/tests/ZzWorkspaceShellTest.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp
  git diff --cached --check
  git commit -m "框架：重构工作区布局恢复事务" \
    -m "按请求与原始快照预先构造 Qt Dock、Split、Side、Bottom、Activity 和标题固定目标。\n\n分阶段提交并审计全部身份与所有权，失败时使用同一执行器反向恢复，彻底移除 mutation 后学习预期的旧实现。"
  ```

---

### 任务 5：完成事务资源预算与全门禁

**文件：**

- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`
- 按失败证据修改：任务 1 至任务 4 的 private 实现文件

- [ ] **步骤 1：增加对象和重复事务失败测试。**

  ```cpp
  void keepsObjectBudgetStableAcrossRepeatedTransactions()
  {
      ZzShellFixture fixture;
      const auto saved = fixture.shell->saveLayout();
      QVERIFY(saved);
      const int baselineObjects =
          fixture.host.findChildren<QObject *>().size();

      for (int iteration = 0; iteration < 1000; ++iteration) {
          QVERIFY(fixture.shell->restoreLayout(saved.value()));
      }

      QCOMPARE(
          fixture.host.findChildren<QObject *>().size(),
          baselineObjects);
  }
  ```

  再增加 1000 次 Activity 同侧/跨侧往返、1000 次 Split/Side 信号污染导致的失败恢复，断言
  QObject、QTimer、QAbstractAnimation、Activity rows、panel registrations 和 owner 回到基线。

- [ ] **步骤 2：先运行压力 RED。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest \
    keepsObjectBudgetStableAcrossRepeatedTransactions
  ```

  如果立即通过，执行变异检查：临时泄漏 shadow Dock 或跳过事务 guard 析构，确认测试失败；
  恢复生产代码后再继续。不得为了制造 RED 降低真实断言。

- [ ] **步骤 3：只修复压力测试证明的资源问题。**

  使用栈对象、RAII guard、预留容器和稳定连接；禁止加入 event-loop wait、timer、animation、
  processEvents 或无界重试。若没有资源问题，本步骤不修改生产代码。

- [ ] **步骤 4：运行 fresh 构建和定向门禁。**

  ```bash
  cmake --build --preset linux-gcc-debug --target clean
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  ctest --preset linux-gcc-debug \
    -R '^(puretools.workspace-layout-state-private|puretools.workspace-layout-codec-private|puretools.workspace-shell|fluent.split-workspace|fluent.side-pane|fluent.bottom-pane)$' \
    --output-on-failure
  ctest --preset linux-gcc-debug -L architecture --output-on-failure
  git diff --check
  ```

  Architecture 若仍只因既有 `OrderedPage` 缺少 `Zz` 前缀失败，按账本记录，不混入本提交；
  新文件不得产生任何新增架构失败。

- [ ] **步骤 5：运行 sanitizer 和静态分析。**

  ```bash
  cmake --preset linux-clang-asan -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-clang-asan --target ZzWorkspaceShellTest --parallel 2
  ctest --preset linux-clang-asan -R '^puretools.workspace-shell$' --output-on-failure

  cmake --preset linux-clang-tidy-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-clang-tidy-release --target ZzClangTidy --parallel 2

  cmake "-DZZ_SOURCE_DIR=${PWD}" \
    -P tests/Platform/PresetMatrixContract.cmake
  ctest --preset linux-gcc-debug \
    -R '^(platform.gate-script-contract|architecture.workspace-boundaries-contract|architecture.puretools-boundaries-contract|architecture.puretools-boundaries)$' \
    --output-on-failure
  ```

  `PresetMatrixContract.cmake` 必须验证 Windows MSVC 2022 shared/static、Windows MinGW
  shared/static 和 macOS AppleClang arm64/x86_64 shared/static preset 仍存在且继承正确；
  `platform.gate-script-contract` 必须验证对应平台 gate 脚本仍包含这些 preset。当前主机不下载
  跨平台 Qt，不伪造 Windows/macOS 运行证据，真实物理机运行继续保留为人工验证项。

- [ ] **步骤 6：运行完整 CTest 并区分既有失败。**

  ```bash
  ctest --preset linux-gcc-debug --output-on-failure
  ```

  只允许账本已有的 4 个 workspace screenshot 基线失败和 `OrderedPage` 架构命名失败；任何
  新失败都必须在提交前修复。

- [ ] **步骤 7：提交压力证据并审查。**

  ```bash
  git add ZzPureTools/tests/ZzWorkspaceShellTest.cpp
  git add ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
  git diff --cached --check
  git commit -m "测试：收紧工作区事务资源门禁" \
    -m "覆盖重复布局恢复、Activity 往返和同步污染失败路径的对象与状态稳定性。\n\n验证事务不创建持久 timer、animation 或 shadow Dock，并补齐 GCC、Clang、sanitizer 与架构门禁证据。"
  ```

  若步骤 3 未修改某个生产文件，不得为保持命令形式而暂存它；提交前以
  `git diff --cached --name-only` 精确复核。

---

## 最终审查与原计划恢复

1. 每个任务提交后使用 `review-package` 生成 exact diff，分派全新审查者，必须同时得到规格
   合规与任务质量通过；修复循环最多五轮。
2. 任务 5 通过后，从本计划起点到 HEAD 生成全分支 review package，执行一次宽范围审查。
3. 宽范围审查无承重问题后，使用 `superpowers:finishing-a-development-branch` 决定如何将
   feature worktree 集成到保留原型的主工作树；未经裁定不得删除主工作树的三个原型文件。
4. 集成后在主工作树确认 `git status` 只剩用户的 `temp_image/`，再把原计划账本中的任务 9
   标记为由任务 9R 取代并完成。
5. 只有任务 9R 正式审查通过并集成后，才能继续
   `docs/superpowers/plans/2026-08-22-ide-workbench-product-expansion.md` 的任务 10 至任务 15。
