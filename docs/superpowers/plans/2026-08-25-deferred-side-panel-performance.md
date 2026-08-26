# 延迟侧面板性能修复实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度；每个任务完成验证后立即创建中文 commit。

**目标：** 为 `ZzWorkspaceShell` 增加可失败、可重试、可回滚的延迟 Side Panel factory，并让 `ZzPureToolsExample` 首帧只发布四个 Activity 入口而不创建四个侧面板 QWidget，从而恢复既有 startup 与 idle RSS 性能门禁。

**架构：** Shell 注册表同时保存逻辑面板身份和物理内容状态，`Pending -> Materializing -> Ready` 状态机由 Shell 私有实现统一驱动。旧 eager API 保持立即接管并显示；新 factory API 只在首次显示、Activity 激活、Pending take 或布局要求可见时实例化，并复用既有 PanelStack 所有权审计与布局事务回滚。

**技术栈：** Qt 6.8+（Linux 动态验证使用 Qt 6.11.1）、C++20、`std::function`、`std::unique_ptr`、Qt Widgets、Qt Test、CMakePresets、Xvfb 性能采样。

**前置规格：** `docs/superpowers/specs/2026-08-25-deferred-side-panel-performance-design.md`

**所属主计划：** `docs/superpowers/plans/2026-08-22-ide-workbench-product-expansion.md` 任务 15

**代码基线：** `cdd743a`

---

## 执行约束

- 只在 `.worktrees/command-bar` 的 `feature/command-bar` 分支执行本计划。
- 不读取、不修改、不暂存 `temp_image/`；不 push，不调用 GitHub CLI，不处理远端 CI。
- 不下载 Qt；本机固定使用：

  ```bash
  export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
  export GCC_13=/usr/bin/gcc-15
  export GXX_13=/usr/bin/g++-15
  ```

- 严格执行 TDD：先增加一个能证明缺失合同的失败测试，运行并确认按预期失败，再写最小实现使其通过。
- 新公开 API 和复杂状态事务使用简体中文 Doxygen；禁止链式命名空间、Qt Private API、平台分支、stylesheet、timer 和 animation。
- 不修改序列化 magic、schema 版本、摘要、1 MiB 上限和 Qt Dock state version。
- 不覆盖历史性能 metric、不修改 `regression-thresholds.json`、不提高 10% 相对阈值。
- 每个任务只暂存该任务列出的文件；提交前运行 `git diff --cached --check` 和对应定向测试。
- Windows MSVC、Windows MinGW 与 macOS 只做公共 C++20/Qt API、源清单、导出和安装消费静态检查，不宣称真机运行通过。

## 文件结构与职责

- 修改 `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`：声明 `ZzWorkspacePanelFactory` 和 `registerSidePanelFactory()` 公共合同。
- 修改 `ZzPureTools/widgets/src/ZzWorkspaceShell.cpp`：执行 GUI 线程门禁并转发 factory 注册。
- 修改 `ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`：保存 factory、实例化状态和共享接管/回滚 helper 声明。
- 修改 `ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`：拆分逻辑注册与物理接管，实现创建、重试、Pending take、Activity 激活和销毁收敛。
- 修改 `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`：让审计区分逻辑全量记录和 Ready 物理子集，Pending 迁移不得实例化。
- 修改 `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp`：保存 Pending 逻辑位置，恢复时只实例化目标可见/current 项，并回滚本轮新内容。
- 修改 `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`：覆盖 eager 兼容、factory 状态机、非法返回、重入、take、badge、迁移和布局事务。
- 修改 `examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`：把 Sessions、Files、Properties、Tasks 改成延迟 factory 注册，保持 Bottom/Dock eager。
- 修改 `examples/ZzPureToolsExample/ZzExampleWorkspaceContent.h` 和 `.cpp`：为四个 Side 内容提供按需构造入口，并保留稳定 objectName。
- 修改 `examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`：验证首建零 Side 内容、首次激活单次创建及既有工作流。
- 修改 `tests/InstallConsumer/Gui/main.cpp`：从安装后的公共 API 注册并激活一个最小延迟 Side 面板。
- 必要时修改 `tests/Architecture/CheckZzWorkspaceBoundaries.cmake`：只补充新公共 factory 合同的边界断言，不新增白名单。
- 最终修改任务 15 的性能、平台和人工验收文档及三轮 evidence；12 份历史 reference JSON 只允许迁移 `environment.memoryBytes` 和 `environment.runnerImageDigest`。

## 任务 1：实现延迟注册、创建和失败事务

**交付物：** 新 factory API 在注册时只发布逻辑 Activity 行；`showPanel(true)`、Activity 激活与 Pending `takePanel()` 按合同创建一次，所有失败都保留 Pending 注册并允许重试；旧 eager API 行为不变。

**文件：**

- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`
- 修改：`ZzPureTools/widgets/src/ZzWorkspaceShell.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 测试：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [x] **步骤 1：写 eager 兼容与 factory 延迟注册失败测试。** 在 `ZzWorkspaceShellTest` 增加 `keepsEagerSideRegistrationVisible()` 与 `registersDeferredSidePanelWithoutCreatingContent()`。后者必须保存调用次数，并断言 Activity 行存在、两个物理栈为空、Pane 折叠、Activity 行 inactive：

  ```cpp
  int calls = 0;
  QVERIFY(fixture.shell->registerSidePanelFactory(
      zzPanelId("sessions"), QStringLiteral("Sessions"), {},
      ZzFluentUI::ZzActivityArea::LeftPrimary,
      [&calls] {
          ++calls;
          return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
              std::make_unique<QWidget>());
      }));
  QCOMPARE(calls, 0);
  QCOMPARE(fixture.shell->activityBar(
      ZzFluentUI::ZzSidePaneEdge::Left)->model()->rowCount(), 1);
  QVERIFY(fixture.shell->sidePane(
      ZzFluentUI::ZzSidePaneEdge::Left)->panelStack()->panels().isEmpty());
  QVERIFY(fixture.shell->sidePane(
      ZzFluentUI::ZzSidePaneEdge::Left)->isCollapsed());
  ```

- [x] **步骤 2：运行失败测试。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  ```

  预期：编译失败，错误明确指出 `ZzWorkspacePanelFactory` 或 `registerSidePanelFactory()` 尚不存在；旧 eager 用例仍能编译。

- [x] **步骤 3：声明公共 factory 合同和私有状态。** 在公开命名空间中加入：

  ```cpp
  using ZzWorkspacePanelFactory =
      std::function<ZzCore::ZzResult<std::unique_ptr<QWidget>>()>
  ;
  ```

  在 `ZzWorkspaceShell` 增加简体中文 Doxygen 和按值 factory 参数：

  ```cpp
  [[nodiscard]] ZzCore::ZzResult<void> registerSidePanelFactory(
      const ZzWorkspacePanelId &id,
      const QString &title,
      ZzFluentUI::ZzIconDescriptor icon,
      ZzFluentUI::ZzActivityArea area,
      ZzWorkspacePanelFactory factory);
  ```

  Private 记录增加：

  ```cpp
  enum class ZzMaterializationState : std::uint8_t
  {
      Pending,
      Materializing,
      Ready
  };

  ZzWorkspacePanelFactory factory;
  ZzMaterializationState materialization = ZzMaterializationState::Ready;
  ```

  eager Side、Bottom、Dock 记录显式为 `Ready`；factory 注册记录为 `Pending`，`content`、`contentIdentity` 和 owner 字段为空。

- [x] **步骤 4：实现仅逻辑注册。** `registerSidePanelFactory()` 复用 eager 输入门禁：宿主存活、PanelId 合法、trim 后标题非空、area 合法、factory 非空、全局 ID 不重复、无其他事务。先预占 `registrationGeneration`，再 append Activity 行并审计模型、左右 Bar、Pane、注册表身份；成功时保持 Activity current/active 为空并调用 `syncSideEdgeVisibility()` 只显示对应 Activity Bar，不展开 Pane。

  `syncSideEdgeVisibility()` 的 `hasPanel` 判断改为逻辑 Side 记录，而不是 `content != nullptr`：

  ```cpp
  return record.kind == ZzPanelKind::Side
      && !record.removalInProgress
      && zzIsLeftArea(record.activityArea) == left;
  ```

  注册过程中发生同步模型破坏时，只移除本次逻辑记录和本次 Activity 行；不得调用 factory。

- [x] **步骤 5：验证延迟注册转绿。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  ctest --preset linux-gcc-debug -R '^puretools.workspace-shell$' --output-on-failure
  ```

  预期：新增延迟注册和原 eager 兼容用例通过，现有 WorkspaceShell 全部用例继续通过。

- [x] **步骤 6：写首次显示、Activity 激活和单次创建失败测试。** 增加 `materializesDeferredSidePanelOnlyOnce()` 与 `activityActivationMaterializesDeferredSidePanel()`。测试应让 factory 返回带稳定 objectName 的 QWidget，先调用或单击入口，再断言 calls 为 1、内容已进入正确 SidePane、current/visible/active 一致；重复显隐和重复点击后 calls 仍为 1。

- [x] **步骤 7：实现共享物理接管 helper。** 把 eager 注册中从 `ZzPanelOwnerObserver` 到 canonical Stack 顺序审计的代码抽成私有 `adoptSidePanelContent()`，输入固定 PanelId、registration generation、无父 QWidget 和 `activate` 标志。eager 路径用 `activate=true` 保持旧语义；延迟 materialize 用 `activate=false`，只完成 `Ready` 接管，随后由 `showPanel()` 执行既有显隐/当前/Activity 同步。

  新增 `materializeSidePanel(id)`，固定顺序为：

  1. 校验 GUI 线程、宿主、记录、目标 Pane/Stack/Activity model；
  2. 把 `Pending` 改为 `Materializing` 并捕获 generation；
  3. 在 `try/catch` 中调用 factory；
  4. 拒绝 error、空 unique_ptr、带父对象或非 GUI 线程 QWidget；
  5. 以当前 Activity 逻辑顺序计算 Ready 子序列插入位置；
  6. 通过共享接管 helper 提交 `Ready`，清空 record 中的 factory；
  7. 任意失败恢复 `Pending` 和原 factory，不改变 Activity、Pane、sizes、current、active 与可见集合。

  `showPanel(id, false)` 对 Pending 返回成功且不得创建；`showPanel(id, true)` 在任何 UI 更新前调用 `materializeSidePanel()`。

- [x] **步骤 8：写并实现失败矩阵。** 使用 data-driven 测试 `deferredFactoryFailureIsAtomic_data()` / `deferredFactoryFailureIsAtomic()` 覆盖 `ZzError`、success null、带父对象、错误线程、`std::runtime_error` 与未知异常。每个数据行都在失败前后比较 `saveLayout()`、Activity current/active、Pane collapsed/current/visible/sizes、Stack panels 及原父对象，并让第二次 factory 返回合法 QWidget 证明可重试。

  稳定错误映射为：factory 自身 `ZzError` 原样保留；非法返回使用 `InvalidState`；标准和未知异常转换为 `InvalidState`，异常不能越过 Qt 回调。

- [x] **步骤 9：写并实现同 ID 同步重入。** 增加 `rejectsReentrantDeferredMaterialization()`：factory 第一次执行时同步调用同一 ID 的 `showPanel(true)` 和 `takePanel()`，两者必须返回 `InvalidState`，calls 保持 1，外层成功后内容为 Ready。`Materializing` 状态不得被嵌套调用清除。

- [x] **步骤 10：写并实现 Pending take 和 badge。** 增加 `takesPendingPanelWithoutShowingIt()`、`failedPendingTakePreservesRegistration()` 与 `updatesBadgeBeforeMaterialization()`。Pending take 成功时调用 factory 一次，归还非空、无父、未显示 QWidget，移除 Activity 行和记录；失败时 factory 和 Activity 行保留。badge 必须在 Pending 阶段写入 Activity model，并在 materialize 后保持。

- [x] **步骤 11：运行任务 1 全部验证。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  ctest --preset linux-gcc-debug -R '^(puretools.workspace-shell|architecture.boundaries|architecture.complete-audit)$' --output-on-failure
  ```

  预期：WorkspaceShell 全套测试和两项架构检查通过；factory 失败与异常没有未捕获输出、QObject 泄漏或状态漂移。

- [x] **步骤 12：提交任务 1。**

  ```bash
  git add \
    ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h \
    ZzPureTools/widgets/src/ZzWorkspaceShell.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp \
    ZzPureTools/tests/ZzWorkspaceShellTest.cpp
  git diff --cached --check
  git commit -m "性能：新增延迟侧面板工厂" \
    -m "为 WorkspaceShell 增加 Pending、Materializing、Ready 状态机，并保持旧 eager 注册语义不变。" \
    -m "补充首次创建、Activity 激活、失败重试、异常隔离、同步重入、Pending take 与 badge 的事务测试。"
  ```

## 任务 2：支持 Pending 迁移、保存恢复与整批回滚

**交付物：** Pending 与 Ready 面板可以混合迁移和持久化；save 永不创建 Pending；restore 只创建目标布局要求可见/current 的 Pending，并在任何失败时把本轮新内容反向回滚为 Pending。

**文件：**

- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp`
- 测试：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [x] **步骤 1：写 Pending 跨侧迁移失败测试。** 增加 `movesPendingPanelWithoutMaterializingIt()`：注册 LeftPrimary Ready、LeftSecondary Pending、RightPrimary Ready，触发 Activity `moveRequested` 把 Pending 移到 RightSecondary；断言 factory calls 为 0、Activity Area 与全局顺序已更新、左右 Stack QWidget 子集不变。随后 `showPanel(true)`，断言 Pending 内容按最新逻辑顺序插入右侧 Ready 子序列。

- [x] **步骤 2：运行失败测试。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  ctest --preset linux-gcc-debug -R '^puretools.workspace-shell$' --output-on-failure
  ```

  预期：迁移审计因 Pending 没有物理 QWidget identity 而失败，或错误地调用 factory。

- [x] **步骤 3：让移动事务区分逻辑全量与物理子集。** `ZzAuditIndex` 对所有 Side 记录建立 `recordRows` 和 Activity area；只有 `Ready` 且 identity 有效的记录进入 `idsByWidget`、frames 和物理内容审计。目标投影的逻辑顺序允许 Pending ID，Stack order/visible/sizes/current 只允许 Ready ID。

  `applyProjection()` 遇到 Pending 移动时只更新 record `activityArea` 和 Activity model 投影；不得 detach/attach QWidget。Ready 移动继续沿用现有 `ZzMutationObserver` 和回滚路径。物理插入索引通过目标逻辑顺序中过滤 Ready ID 计算，不能用全量 Activity row 直接作为 Stack index。

- [x] **步骤 4：验证 Pending 与混合迁移转绿。** 增加 `movesMixedPendingAndReadyPanelsConsistently()`，在四个 Activity area 中混排至少两个 Pending 和两个 Ready，执行同侧重排、跨侧迁移、materialize、take 后检查逻辑全量 ID 唯一、物理 QWidget 子集唯一且顺序与过滤后的逻辑顺序一致。

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  ctest --preset linux-gcc-debug -R '^puretools.workspace-shell$' --output-on-failure
  ```

- [x] **步骤 5：写 save 不创建与逻辑 round trip 失败测试。** 增加 `savesPendingPanelsWithoutMaterializingThem()`：source 同时注册 Pending/Ready，调整 Area 与顺序后保存；calls 必须保持 0。target 用同 ID factory 注册后 restore，断言未保存为 visible/current 的 Pending 仍未创建，Activity Area 与顺序完整 round trip，重新 save 得到等价逻辑布局。

- [x] **步骤 6：实现 Pending 保存投影。** 捕获 snapshot 时：

  ```cpp
  // Activity / sideEntries: 全部逻辑 Side 记录
  // side.order / visible / sizes / current: 仅 Ready 物理内容
  // identity.widget: Pending 允许为空，Ready 必须稳定
  ```

  `save()` 的审计不再要求 Side 注册数等于两个 Stack 内容总数。Pending 不写入新 schema 字段，继续由现有 `sideEntries` 的 ID、Area、order 表示；visible/current 列表自然不含 Pending。

- [x] **步骤 7：写 restore 选择性创建失败测试。** 增加 `restoresOnlyDeferredPanelsRequestedVisible()`：source 用 eager 内容保存一份仅 `files` 和 `tasks` visible/current 的布局；target 四个 ID 都用计数 factory 注册。restore 后只允许 files/tasks calls 为 1，另两个为 0，目标 Pane current/visible/sizes 与 source 一致。

- [x] **步骤 8：实现 restore prepare 阶段的选择性 materialize。** 解码和完整 ID/Area/order 校验完成后，计算 `leftVisible + rightVisible + leftCurrent + rightCurrent` 的去重目标 ID。只对其中处于 Pending 的记录调用内部 materialize；创建时先保持 hidden，不提前修改 Activity active/current 或 Pane collapsed。全部创建成功后再应用既有五阶段投影。

- [x] **步骤 9：写 restore 中途失败的整批回滚测试。** 增加 `rollsBackNewlyMaterializedPanelsWhenRestoreFails()`：目标布局要求两个 Pending 可见，第一个 factory 成功，第二个返回 error；断言 restore 失败后两个记录均为 Pending，第一个新 QWidget 已销毁，两个 factory 均可重试，Activity rows/badge/Area/order、Pane/Stack/current/visible/sizes、原布局 bytes 与 restore 前完全一致。

- [x] **步骤 10：实现 restore 创建日志与反向回滚。** Layout transaction 保存本轮成功 materialize 的 `{id, generation, factory}` 日志。后续创建或投影提交失败时按逆序从 PanelStack 取回并销毁本轮 QWidget，断开 destroyed connection，清空 content/owner identity，恢复 factory 和 `Pending`；之后再应用既有原始投影回滚。事务期间嵌套 show/take/move/save/restore 继续由 `transactionKind` 拒绝。

- [x] **步骤 11：增加混合 take/save/restore 审计并运行任务 2 验证。** `auditsMixedPendingReadyLifecycle()` 依次执行 badge、move、show、take、save、restore，最终验证每个逻辑 Side ID 恰有一个 Activity row，每个 Ready ID 恰有一个物理内容，Pending ID 没有 QWidget，所有 factory 成功后至多调用一次。

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest ZzWorkspaceLayoutStatePrivateTest ZzWorkspaceLayoutCodecPrivateTest --parallel 2
  ctest --preset linux-gcc-debug -R '^puretools.workspace-(shell|layout-state-private|layout-codec-private)$' --output-on-failure
  ctest --preset linux-gcc-debug -R '^(architecture.boundaries|architecture.complete-audit)$' --output-on-failure
  ```

  预期：三项 workspace 测试和架构审计通过；现有 schema 1 迁移、schema 2 round trip、摘要和 1 MiB 上限测试不变。

- [x] **步骤 12：提交任务 2。**

  ```bash
  git add \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp \
    ZzPureTools/tests/ZzWorkspaceShellTest.cpp
  git diff --cached --check
  git commit -m "性能：完善延迟面板布局事务" \
    -m "区分 Activity 逻辑全量与 PanelStack Ready 子集，使 Pending 面板迁移和保存不触发实例化。" \
    -m "布局恢复仅创建目标可见内容，并在工厂或提交失败时反向销毁本轮内容、恢复 Pending 状态与原布局。"
  ```

## 任务 3：接入 Example、安装消费并完成性能收口

**交付物：** Example 四个 Side 面板首帧零实例，首次激活按需创建；smoke、公共安装消费、shared/static、性能复采和统一 Linux 门禁通过，平台文档准确记录人工与静态边界。

**文件：**

- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.h`
- 修改：`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.cpp`
- 修改：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`
- 修改：`tests/InstallConsumer/Gui/main.cpp`
- 必要时修改：`tests/Architecture/CheckZzWorkspaceBoundaries.cmake`
- 修改：`docs/development/BUILDING_ZH.md`
- 修改：`docs/development/PLATFORM_SUPPORT_ZH.md`
- 修改：`docs/performance/PERFORMANCE_BASELINE_ZH.md`
- 修改：`docs/performance/evidence/workspace-components/2026-08-22/round-1.json`
- 修改：`docs/performance/evidence/workspace-components/2026-08-22/round-2.json`
- 修改：`docs/performance/evidence/workspace-components/2026-08-22/round-3.json`
- 修改：`docs/performance/reference/linux/*.json` 中 12 份场景文件，仅限两个 environment 字段
- 修改：`docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`
- 修改：`docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md`
- 修改：`docs/release/MANUAL_MACOS_CHECKLIST_ZH.md`

- [x] **步骤 1：把 Example smoke 改成首建零 Side 内容的失败测试。** 首次创建 `ZzExampleWindowShell` 后仍断言四个 Activity row 及两个 Activity Bar 存在，但以下四个 objectName 均不存在，左右 Pane collapsed，两个 Stack 为空：

  ```cpp
  QCOMPARE(window->findChild<QWidget *>(
      QStringLiteral("zzExampleSessionPanel")), nullptr);
  QCOMPARE(window->findChild<QWidget *>(
      QStringLiteral("zzExampleSftpPanel")), nullptr);
  QCOMPARE(window->findChild<QWidget *>(
      QStringLiteral("zzExamplePropertiesPanel")), nullptr);
  QCOMPARE(window->findChild<QWidget *>(
      QStringLiteral("zzExampleTasksPanel")), nullptr);
  ```

  通过 Activity 单击创建 Sessions，再激活 Files，断言对应 objectName 只出现一个、左侧 Stacked 同时 visible；右侧 Properties/Tasks 重复相同验证。继续执行现有标签分屏、Bottom、CommandBar、跨侧迁移和布局 round trip 流程。

- [x] **步骤 2：运行 smoke 确认失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzExampleWorkspaceSmokeTest --parallel 2
  ctest --preset linux-gcc-debug -R '^example.workspace-smoke$' --output-on-failure
  ```

  预期：首建断言失败，因为当前四个 Side QWidget 仍被 eager 创建。

- [x] **步骤 3：用 factory 接入 Example。** `ZzExampleWindowShellPrivate::initialize()` 不再提前调用四个 `create*Panel()`。改为捕获稳定的 `ZzExampleApplicationContext`/模型依赖，并分别调用 `registerSidePanelFactory()`；lambda 返回 `ZzResult<std::unique_ptr<QWidget>>`，只负责构造内容，不访问 SidePane、Activity model 或布局 envelope。

  注册成功后不调用 `showPanel(false)` 模拟延迟；Shell 合同本身保证初始 inactive/hidden。三个 Bottom 面板和现有 Dock 保持 eager。每个内容继续由 `ZzExampleWorkspaceContent` 创建并设置原 objectName，确保 smoke、命令和布局身份不变。

- [x] **步骤 4：验证 Example 完整工作流。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest --parallel 2
  ctest --preset linux-gcc-debug -R '^(example.workspace-smoke|puretools.workspace-shell)$' --output-on-failure
  ```

  预期：首次构建四个 Side 内容不存在；每个首次激活创建一次；现有 workspace smoke 和 Shell 测试全部通过。

- [x] **步骤 5：增加安装消费失败测试并实现。** 在 `tests/InstallConsumer/Gui/main.cpp` 增加安装后公共 API 调用：

  ```cpp
  int deferredCalls = 0;
  const auto registeredDeferred = workspaceShell->registerSidePanelFactory(
      ZzPureTools::ZzWorkspacePanelId(QStringLiteral("deferred")),
      QStringLiteral("Deferred"), {},
      ZzFluentUI::ZzActivityArea::RightSecondary,
      [&deferredCalls] {
          ++deferredCalls;
          return ZzCore::ZzResult<std::unique_ptr<QWidget>>::success(
              std::make_unique<QWidget>());
      });
  if (!registeredDeferred || deferredCalls != 0
      || !workspaceShell->showPanel(
          ZzPureTools::ZzWorkspacePanelId(QStringLiteral("deferred")))
      || deferredCalls != 1) {
      return 29;
  }
  ```

  先在未安装旧头状态确认消费者编译失败，再安装当前实现并验证 shared/static 消费。

- [x] **步骤 6：运行公共头、架构与安装消费矩阵。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzPublicHeadersTest --parallel 2
  ctest --preset linux-gcc-debug -R '^(architecture.public-headers|architecture.boundaries|architecture.complete-audit)$' --output-on-failure

  cmake --build --preset linux-gcc-release --parallel 2
  ctest --preset linux-gcc-release -R '^(install.consumer|architecture.public-headers|platform.binary-dependencies|platform.package-relocation)$' --output-on-failure

  cmake --build --preset linux-static-release --parallel 2
  ctest --preset linux-static-release -R '^(install.consumer|architecture.public-headers|platform.binary-dependencies|platform.package-relocation)$' --output-on-failure
  ```

  若仓库中的实际 target 名与命令不同，先用 `cmake --build --preset <preset> --target help` 和 `ctest --preset <preset> -N` 解析现有名字，只允许调整命令，不允许跳过对应合同。

- [x] **步骤 7：运行三轮 Example startup/idle 性能复采。** 使用 `docs/performance/profiles/local-release-xvfb.json` 和门禁实际采用的 `linux-gcc-benchmarks` 构建，不覆盖 reference。先按 `scripts/ci/run-linux-gates.sh` 的 Xvfb、`taskset -c 10`、`DISPLAY` 和 `QT_QPA_PLATFORM=xcb` 环境启动固定显示服务器，再执行：

  ```bash
  cmake --preset linux-gcc-benchmarks -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-gcc-benchmarks --parallel 2
  for round in 1 2 3; do
    taskset -c 10 ctest --preset linux-gcc-benchmarks \
      -R '^benchmark\.example-(startup|idle)$' --output-on-failure -j1
    cmake -E make_directory \
      "build/gate-evidence/task-15-deferred-side-panel/round-${round}"
    cmake -E copy_directory \
      build/linux-gcc-benchmarks/reports \
      "build/gate-evidence/task-15-deferred-side-panel/round-${round}/reports"
  done
  ```

  每轮分别保留 reporter JSON 和 gate 输出到新的 `build/gate-evidence/task-15-deferred-side-panel/round-N/`。必须验证首帧前四个 Side factory calls 为 0，且四个 Side QWidget 不计入首帧 QObject/QWidget。

- [x] **步骤 8：比较相对性能且不得放宽阈值。** 对 `example-startup`、`example-idle` 分别调用仓库现有比较脚本，参数固定使用：

  ```text
  baseline = docs/performance/reference/linux/<scenario>.json
  candidate = build/linux-gcc-benchmarks/reports/benchmark.<scenario>.json
  thresholds = docs/performance/reference/linux/regression-thresholds.json
  profile = docs/performance/profiles/local-release-xvfb.json
  ```

  预期：startup P95/max、idle 起止 RSS 和增长率均不超过既有相对 gate。若仍失败，保留阶段计时、对象/RSS 与调度证据继续定位；禁止刷新历史 metric 或提高阈值。

- [x] **步骤 9：运行统一 Linux 门禁。**

  ```bash
  scripts/ci/run-linux-gates.sh
  ```

  预期：GCC Debug/Release/static、Clang tidy shared/static、ASan/UBSan、GCC shared/static LTO、架构、安装消费、截图和性能门禁全部通过。命令运行期间持续保存完整日志，不得以此前历史结果代替本次实现后的结果。

- [ ] **步骤 10：执行并记录平台边界。** Linux 五区 Overlay 只能在物理桌面运行 `scripts/release/run-linux-desktop-acceptance.sh` 并人工拖放验收；Xvfb/offscreen 结果不得写成物理桌面通过。Windows MSVC、Windows MinGW 与 macOS 记录公共 API/CMake 静态检查结果和“真机待验证”，不伪造运行证据。当前静态边界已记录，物理桌面人工验收待执行。

- [x] **步骤 11：更新性能与平台证据。** 三轮 workspace evidence 写入本次 commit、reporter 路径、环境和测量结果。12 份 `docs/performance/reference/linux/*.json` 保留原 commit 和全部 metrics，只把：

  ```json
  {
    "environment": {
      "memoryBytes": "本机稳定物理内存字节数",
      "runnerImageDigest": "当前本机发布参考环境摘要"
    }
  }
  ```

  迁移到稳定环境指纹。文档明确：当前只有这台本机可作为发布参考机，原计划参考机仍被记录，Windows/macOS 真机验证待补充。

- [x] **步骤 12：提交任务 3。** 先用 `git diff --name-only` 确认没有 `temp_image/`，再只暂存本任务文件：

  ```bash
  git diff --cached --check
  git commit -m "质量：收口延迟侧面板性能证据" \
    -m "将 Example 四个 Side 面板改为按需创建，并通过 smoke、公共头、安装消费和架构合同验证。" \
    -m "完成三轮 startup/idle 复采与 Linux 门禁，保留历史性能指标并准确记录当前发布参考机及跨平台待验证边界。"
  ```

## 最终完成标准

1. 旧 `registerSidePanel(QWidget *)` 立即显示、激活和所有权合同没有变化。
2. factory 注册后 Activity 入口立即存在，Side Pane 折叠，首帧 factory calls 和 Side QWidget 数均为零。
3. factory 错误、非法 QWidget、异常和同步重入都不改变 Shell UI、注册表和 QWidget 所有权，并可重试。
4. Pending take、badge、同侧重排、跨侧迁移、save 和 restore 均满足规格；restore 中途失败能销毁本轮内容并恢复 Pending。
5. Example 四个 Side 内容只在首次显式激活时创建一次；Bottom/Dock 保持 eager。
6. 公共头、安装消费、架构、shared/static、sanitizer、tidy、LTO、截图和统一 Linux 门禁有本提交后的通过证据。
7. `example-startup` 与 `example-idle` 在不修改历史 metric 和 10% 阈值的前提下通过相对性能 gate。
8. Linux 物理桌面五区 Overlay 由人工验收；Windows MSVC、Windows MinGW、macOS 未执行真机验证时明确保留待验证标记。

## 计划自检

- 规格第 3 至 5 节的公共 API、状态机、失败原子性和异常隔离由任务 1 覆盖。
- 规格第 6 节的 Pending take、迁移、save/restore 与回滚由任务 1、任务 2 覆盖。
- 规格第 7 至 10 节的 Example、12 项测试、性能阈值和平台边界由任务 3 覆盖。
- 全文不存在待定实现、模糊的“适当错误处理”或引用未定义方法；类型名统一为 `ZzWorkspacePanelFactory`、`ZzMaterializationState`、`registerSidePanelFactory()`、`materializeSidePanel()` 和 `adoptSidePanelContent()`。
- 三个任务分别产生可独立测试和审查的逻辑提交；计划文档本身单独提交，不与代码或既有性能证据混合。

## 执行记录（2026-08-26）

任务 1、任务 2 和任务 3 的代码与测试已分别在以下提交完成：

- `0c4387c`：新增延迟 Side 面板 factory 及 Pending/Materializing/Ready 状态机。
- `2bae9d6`：完善 Pending 迁移、布局保存恢复和整批回滚事务。
- `c3d208c`、`d8f1f0d`：Example 延迟接入与安装消费者覆盖。
- `f59e4d1`：收口性能、平台和人工验收文档及三轮 workspace evidence。

本轮补充 `CMAKE_BUILD_RPATH_USE_ORIGIN` 的 Linux 目录级约束，避免 CMake 重新配置后
构建目标继续携带绝对构建路径；并记录三轮 `example-startup`/`example-idle` 原始
报告。统一 gate 在本机完成 Debug 151/151 CTest、两档 clang-tidy 和后续发布组合；
Linux 物理桌面以及 Windows/macOS 真机仍按清单保持“未执行”。
