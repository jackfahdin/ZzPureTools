# 工作区 Activity、组件导航与设置窗口重构实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将左右 Activity Bar 改为单活动竖向 Tab，把组件导航迁入左侧 Side Panel，并以固定左下 Activity Action 打开每主窗口单实例的窗口模态设置页。

**架构：** `ZzActivityBar` 只根据模型 flags 绘制并发出激活、折叠和移动意图；`ZzWorkspaceShell` 统一管理 SidePanel 与 FixedAction 行、单活动状态、四区域移动和 v3 布局；`ZzApplicationWindow` 通过可回滚事务把既有 NavigationPane/PageHost 交给 Shell。Example 只注册内容、QAction 和设置窗口，不实现工作区状态机。

**技术栈：** Qt 6.8+ Widgets/Gui/Test、C++20、CMake Presets、ZzResult、QWindowKit 适配层、spdlog/ZzLog、Qt Model/View、QDataStream 版本化布局。

**规格：** `docs/superpowers/specs/2026-08-27-workspace-activity-navigation-settings-design.md`

---

## 文件结构

### 新增文件

- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceActivityId.h`：固定 Activity Action 的公开稳定标识值类型。
- `ZzPureTools/widgets/src/ZzWorkspaceActivityId.cpp`：标识规范化、有效性与哈希实现。
- `ZzPureTools/tests/ZzWorkspaceActivityIdTest.cpp`：值类型和元类型合同。
- `ZzPureTools/widgets/src/private/ZzWorkspaceNavigationIntegrationTransactionPrivate.h`：导航表面集成事务声明与快照。
- `ZzPureTools/widgets/src/private/ZzWorkspaceNavigationIntegrationTransactionPrivate.cpp`：NavigationPane/PageHost 拆分、提交和回滚。
- `ZzPureTools/tests/ZzWorkspaceNavigationIntegrationTest.cpp`：真实 `ZzApplicationWindow` 集成、拒绝与回滚测试。
- `examples/ZzPureToolsExample/ZzExampleSettingsWindow.h`：设置顶层窗口公开 View 接口。
- `examples/ZzPureToolsExample/ZzExampleSettingsWindow.cpp`：公开构造、生命周期和事件转发。
- `examples/ZzPureToolsExample/ZzExampleSettingsWindowPrivate.h`：WindowKit、TitleBar、设置页、ViewModel 与 Presenter 所有权。
- `examples/ZzPureToolsExample/ZzExampleSettingsWindowPrivate.cpp`：窗口模态外壳、主题连接和安全关闭实现。

### 修改文件

- `ZzFluentUI/widgets/src/private/ZzItemViewVisual.h/.cpp`：为统一选择指示增加物理左/右边缘放置策略。
- `ZzFluentUI/widgets/include/ZzFluentUI/ZzActivityBar.h`、`widgets/src/private/ZzActivityBarPrivate.h/.cpp`：non-selectable 激活、单指示条和按需“移动到”菜单。
- `ZzFluentUI/tests/ZzActivityBarTest.cpp`：点击、键盘、菜单、RTL、像素与对象预算回归。
- `ZzPureTools/CMakeLists.txt`、`ZzPureTools/tests/CMakeLists.txt`：登记新增源文件和测试目标。
- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`、`widgets/src/ZzWorkspaceShell.cpp`、`widgets/src/private/ZzWorkspaceShellPrivate.h/.cpp`：固定动作注册、统一行模型、单活动状态、空边显隐和导航集成入口。
- `ZzPureTools/widgets/include/ZzPureTools/ZzApplicationWindow.h`、`widgets/src/private/ZzApplicationWindowPrivate.h/.cpp`：保存原 body 身份并向集成事务开放最小私有协作边界。
- `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.h/.cpp`：单活动跨侧移动及 FixedAction 不参与持久顺序的事务审计。
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.h/.cpp`：v2 多活动到 v3 单活动的纯值规划。
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.h/.cpp`：读取 v1/v2/v3，固定写出 v3。
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp`：捕获、应用和回滚 v3 单活动状态。
- `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`、`ZzWorkspaceLayoutStatePrivateTest.cpp`、`ZzWorkspaceLayoutCodecPrivateTest.cpp`：Shell、规划、codec 和错误注入测试。
- `tests/PublicHeaderConsumer/CMakeLists.txt`、`tests/InstallConsumer/CMakeLists.txt`、`tests/InstallConsumer/Gui/main.cpp`：公开头与安装消费合同。
- `examples/ZzPureToolsExample/CMakeLists.txt`、`tests/CMakeLists.txt`：设置窗口源文件和测试依赖。
- `examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.h/.cpp`：默认六入口、设置 QAction、导航集成和逐窗口设置实例。
- `examples/ZzPureToolsExample/ZzExampleSessionModel.h/.cpp`：命令面板新增设置命令并映射同一 QAction。
- `examples/ZzPureToolsExample/ZzExampleRouteCatalog.cpp`、`ZzExamplePageFactory.cpp`、`main.cpp`：从中央导航注册中移除 settings，保留 About。
- `examples/ZzPureToolsExample/ZzExampleSmokeControllerPrivate.cpp`、`tests/ZzExampleWorkspaceSmokeTest.cpp`：更新自动冒烟与多窗口生命周期断言。
- `ZzPureTools/tests/ZzWorkspaceScreenshotTest.cpp` 及 `ZzPureTools/tests/baselines/linux/`：新增右侧空态、恢复态和物理边指示截图。
- `examples/ZzPureToolsExample/tests/baselines/`：更新综合示例四档 DPR、三主题基线。
- `benchmarks/ZzWorkspaceComponentsBenchmark.cpp`：锁定固定动作不分配逐行 QWidget、空边宽度和切换规模预算。
- `docs/development/BUILDING_ZH.md`、`docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`：记录新增定向命令和物理桌面验收项。

## 执行约束

- 只暂存当前任务列出的文件；不得读取、修改、暂存或提交顶层 `temp_image/`。
- 每次修改后提交。提交第一行使用中文简述，正文使用中文说明实现、测试和边界。
- 所有新公开类和公开方法写简体中文 Doxygen；不使用链式命名空间。
- 测试遵循红、绿、重构顺序。红灯必须来自本任务目标行为缺失，不接受编译环境错误充当红灯。
- Linux 使用现有 Qt，不下载 SDK：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export GCC_13_TOOLCHAIN_ROOT=/usr
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
```

## 任务 1：收敛 ActivityBar 通用交互与唯一物理边指示

**文件：**

- 修改：`ZzFluentUI/widgets/src/private/ZzItemViewVisual.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzItemViewVisual.cpp`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzActivityBar.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp`
- 测试：`ZzFluentUI/tests/ZzActivityBarTest.cpp`

- [ ] **步骤 1：为 non-selectable 激活、菜单和像素方向编写失败测试**

在 `ZzActivityBarTest` 增加以下独立场景：

```cpp
void enabledNonSelectableRowOnlyRequestsActivation();
void contextMenuListsOnlyThreeOtherAreas();
void contextMenuMoveMatchesDragMoveArguments();
void contextMenuRejectsFixedDisabledAndInvalidRows();
void indicatorUsesSingleShortPhysicalEdgeInLtrAndRtl();
void contextMenuDoesNotIncreaseSteadyObjectBudget();
```

`enabledNonSelectableRowOnlyRequestsActivation()` 先把一个 selectable SidePanel 设为
current，再点击只带 `Qt::ItemIsEnabled` 的行，断言：

```cpp
QCOMPARE(activationSpy.count(), 1);
QCOMPARE(collapseSpy.count(), 0);
QCOMPARE(bar.currentSourceIndex(), sidePanelIndex);
```

菜单测试向目标 viewport 发送 `QContextMenuEvent`，查找临时 `QMenu`，验证动作 data
严格为另外三个 `ZzActivityArea`，触发其中一个后比较 `moveRequested` 的 source、area、
targetRow。像素测试分别渲染 Left/Right bar，检查 Accent 像素只出现在物理左/右短条范围，
RTL 下物理边不翻转，条高等于 `SelectionIndicatorExtent` 而不是整行高度。

- [ ] **步骤 2：运行测试确认行为缺失**

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --target ZzActivityBarTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^fluent\.activity-bar$' --output-on-failure
```

预期：新测试因 fixed 行改变 current、缺少上下文菜单、右侧/RTL 指示位置错误或存在全高
第二指示而失败。

- [ ] **步骤 3：实现物理边选择指示合同**

在 `ZzItemViewVisual.h` 增加私有绘制枚举和默认保持旧行为的选项：

```cpp
enum class ZzItemIndicatorPlacement : unsigned char
{
    LogicalLeading,
    PhysicalLeft,
    PhysicalRight
};

struct ZzItemViewVisualOptions final
{
    bool drawSurface = true;
    bool ownsIndicator = true;
    bool forceIndicator = false;
    qreal indicatorScale = 1.0;
    ZzItemIndicatorPlacement indicatorPlacement =
        ZzItemIndicatorPlacement::LogicalLeading;
};
```

`ZzItemViewVisual::draw()` 只在 `LogicalLeading` 使用 `QStyle::visualRect()`；物理左右直接
由 `option.rect.left()/right()` 计算短条。Activity delegate 传入 Left/Right 放置策略，
删除现有 `paint()` 中 129-141 行的全高 `fillRect()` 分支。

- [ ] **步骤 4：实现 flags 驱动激活和按需菜单**

将 `activateSourceIndex()` 收敛为：

```cpp
if (!zzIsEnabled(index)) {
    return;
}
if (!index.flags().testFlag(Qt::ItemIsSelectable)) {
    Q_EMIT q_ptr->activationRequested(index);
    return;
}
if (currentSourceIndex == index) {
    Q_EMIT q_ptr->collapseRequested(index);
    return;
}
setCurrentSourceIndex(index);
Q_EMIT q_ptr->activationRequested(index);
```

新增 `showMoveContextMenu(QListView *, const QPoint &)`。只对 enabled、drag-enabled 的有效
源行创建栈上/父对象受控 `QMenu`，按左上、左下、右上、右下顺序跳过当前 area；动作只
发既有 `moveRequested(sourceIndex, targetArea, targetProjectionRowCount)`。菜单关闭即释放，
不得缓存逐行 QAction。键盘 Enter/Space 使用当前投影视图的焦点源索引，使 enabled 且
non-selectable 的 FixedAction 可键盘触发，但不覆盖 Shell 同步的 current SidePanel。

- [ ] **步骤 5：运行 ActivityBar 与共享 item visual 回归测试**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzActivityBarTest ZzFluentItemDelegateTest ZzFluentStandardControlsTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^fluent\.(activity-bar|item-delegate|standard-controls)$' \
  --output-on-failure
```

预期：全部通过；默认 `LogicalLeading` 保证 List/Table/Tree 既有几何不变化。

- [ ] **步骤 6：提交 ActivityBar 交付物**

```bash
git add ZzFluentUI/widgets/src/private/ZzItemViewVisual.h \
  ZzFluentUI/widgets/src/private/ZzItemViewVisual.cpp \
  ZzFluentUI/widgets/include/ZzFluentUI/ZzActivityBar.h \
  ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.h \
  ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp \
  ZzFluentUI/tests/ZzActivityBarTest.cpp
git commit -m "fix(ActivityBar): 统一活动入口交互与物理边指示" \
  -m "支持不可选择固定动作、三目标移动菜单和左右物理边短指示。补充点击、键盘、RTL、拖放一致性及对象预算测试。"
```

## 任务 2：增加固定 Activity Action 注册域

**文件：**

- 创建：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceActivityId.h`
- 创建：`ZzPureTools/widgets/src/ZzWorkspaceActivityId.cpp`
- 创建：`ZzPureTools/tests/ZzWorkspaceActivityIdTest.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/tests/CMakeLists.txt`
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`
- 修改：`ZzPureTools/widgets/src/ZzWorkspaceShell.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`
- 修改：`tests/PublicHeaderConsumer/CMakeLists.txt`
- 修改：`tests/InstallConsumer/CMakeLists.txt`
- 修改：`tests/InstallConsumer/Gui/main.cpp`

- [ ] **步骤 1：编写值类型与固定动作失败测试**

值类型测试覆盖 trim、空值、相等、哈希和 `QVariant`。Shell 测试覆盖：

```cpp
void registersFixedActivityWithoutSelectionOrDragFlags();
void fixedActivityTracksActionEnabledAndTriggeredState();
void destroyedFixedActionRemovesOnlyItsActivityRow();
void rejectsDuplicateIdsAcrossPanelAndActionDomains();
void rejectsForeignThreadFixedAction();
void fixedActionNeverAppearsInSavedLayout();
```

固定动作行的 flags 断言为：enabled QAction 时只含 `Qt::ItemIsEnabled`；禁用后为
`Qt::NoItemFlags`。点击 Activity 行与直接 `QAction::trigger()` 都必须使同一 action spy
增加一次。销毁 QAction 后使用 `QTRY_COMPARE` 等待行数减少，其他 panel/current 不变。

- [ ] **步骤 2：运行新增测试确认公开类型和 API 尚不存在**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceActivityIdTest ZzWorkspaceShellTest --parallel 2
```

预期：编译因 `ZzWorkspaceActivityId` 和 `registerFixedActivityAction()` 尚未定义而失败。

- [ ] **步骤 3：实现公开值类型与注册接口**

公开接口保持规格签名：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> registerFixedActivityAction(
    const ZzWorkspaceActivityId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    QAction *action);
```

`ZzWorkspaceActivityId` 与 `ZzWorkspacePanelId` 一样在构造时 trim，提供 `isValid()`、
`value()`、默认比较、`qHash()` 和 `Q_DECLARE_METATYPE`。将 `.cpp` 登记到
`zz_pure_tools_sources`。

- [ ] **步骤 4：把 Activity model 扩展为 SidePanel/FixedAction 私有行**

在 Shell Private 中增加：

```cpp
enum class ZzActivityRowKind : std::uint8_t
{
    SidePanel,
    FixedAction
};

struct ZzFixedActivityRecord final
{
    ZzWorkspaceActivityId id;
    QPointer<QAction> action;
    QAction *actionIdentity = nullptr;
    QMetaObject::Connection destroyedConnection;
    QMetaObject::Connection changedConnection;
};
```

模型行保存 `kind`、稳定字符串、可选 panel/action ID、title/icon/area/badge。SidePanel
返回 enabled/selectable/drag-enabled；FixedAction 根据 QAction enabled 只返回 enabled。
同一区域投影顺序固定为可移动 SidePanel 在前、FixedAction 按注册顺序在后，使左下设置
始终位于该区域底部。`activityRows()`、布局捕获和移动快照只返回 SidePanel；FixedAction
不得进入布局或移动事务。

统一重复检测比较规范化字符串，Side/Bottom/Dock/FixedAction 任意组合都不可重名。
QAction 销毁连接按 identity 审计后移除行和 record，再调用 `syncSideEdgeVisibility()`；
同步 signal 内 Shell/Action 销毁后通过 `QPointer` 停止访问。

- [ ] **步骤 5：验证注册、安装消费和公开头合同**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceActivityIdTest ZzWorkspaceShellTest \
  ZzPublicHeadersTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^puretools\.(workspace-activity-id|workspace-shell)$|^architecture\.public-headers$|^install\.consumer$|^platform\.package-relocation$' \
  --output-on-failure
```

预期：值类型、Shell 和 relocation 消费全部通过，安装 CMake 文件不泄漏源码路径。

- [ ] **步骤 6：提交固定动作注册域**

```bash
git add ZzPureTools tests/PublicHeaderConsumer tests/InstallConsumer
git commit -m "feat(工作区): 增加固定Activity动作注册" \
  -m "新增独立Activity标识和值类型，统一SidePanel与FixedAction模型行。实现QAction状态映射、跨域重复检测、销毁清理及安装消费合同。"
```

提交前使用 `git diff --cached --name-only` 确认没有暂存本任务以外文件。

## 任务 3：切换单活动状态、空边显隐与四区域移动

**文件：**

- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`
- 测试：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [ ] **步骤 1：编写单活动、折叠、移动和空边失败测试**

增加以下场景并复用现有重入 Widget/事务审计夹具：

```cpp
void sideUsesSingleModeAndShowsOnlyCurrentPanel();
void reactivatingCurrentPanelCollapsesButKeepsCurrentIndicator();
void switchingPanelHidesPreviousWithoutDestroyingEitherContent();
void movingCurrentAcrossSidesTransfersExpandedCurrent();
void movingLastRightEntryRemovesBarPaneWidthAndHitTarget();
void movingEntryBackRestoresOnlyRightBarUntilActivation();
void fixedLeftActionKeepsBarButNeverKeepsEmptyPaneWidth();
void failedMoveRestoresCurrentExpandedOwnershipAndEdgeVisibility();
```

右侧空态测试记录 workspace root 的中心区域 geometry、右 bar/pane 的 visible、sizeHint、
鼠标命中结果；移走最后一项后断言右边缘不占宽，移入后只显示 bar，点击后才展开 pane。

- [ ] **步骤 2：运行 Shell 测试确认旧 Stacked/multi-active 行为失败**

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^puretools\.workspace-shell$' --output-on-failure
```

预期：同侧可见多个面板、右边缘保留宽度或移动 current 规则与新断言不一致。

- [ ] **步骤 3：实现每侧 currentPanel 与 paneExpanded 单一状态**

构造时设置：

```cpp
leftSidePane->setMode(ZzFluentUI::ZzSidePaneMode::Single);
rightSidePane->setMode(ZzFluentUI::ZzSidePaneMode::Single);
leftActivityBar->setMultiActiveEnabled(false);
rightActivityBar->setMultiActiveEnabled(false);
```

把 `activateSidePanel()` 改为统一 `activateActivity()`：FixedAction 只对受保护 QAction
调用 `trigger()`；SidePanel 首次 materialize 后切换唯一 current。再次激活当前只切换
collapsed，bar current 不清空。删除/销毁 current 后按同侧 `LeftPrimary + LeftSecondary`
或 `RightPrimary + RightSecondary` 顺序选择首个合法 SidePanel；无候选则清空并折叠。

- [ ] **步骤 4：调整移动事务和 FixedAction 行换算**

`moveRequested.targetRow` 是目标投影视图行号，事务进入纯 SidePanel 规划前将其换算成
该 area 中位于目标行之前的 SidePanel 数量；FixedAction 不移动且不进入目标顺序。
跨侧移动 current 时目标侧 current 设为 moved ID 且 expanded，源侧选择首个剩余项；
同侧移动保持 current/collapsed。快照增加 bar visible、pane visible、collapsed/current，
回滚审计必须逐项比对。

- [ ] **步骤 5：实现三态边缘显隐**

`syncSideEdgeVisibility()` 分别计算 `hasActivityEntry`、`hasSidePanel`、
`hasExpandedPanel`。bar visible 等于入口存在；pane 只有 current 合法且 expanded 时显示并
占 `paneWidth`。隐藏 pane 后必须从 layout sizeHint 和 splitter/resizer hit test 退出，
但允许 QObject 实例保持稳定。

- [ ] **步骤 6：运行 Shell、SidePane 和移动回归**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceShellTest ZzSidePaneTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^puretools\.workspace-shell$|^fluent\.side-pane$' \
  --output-on-failure
```

- [ ] **步骤 7：提交单活动与移动事务**

```bash
git add ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h \
  ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.h \
  ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp \
  ZzPureTools/tests/ZzWorkspaceShellTest.cpp
git commit -m "feat(工作区): 收敛侧栏单活动与空边状态" \
  -m "将左右侧栏改为单活动竖向Tab，完善当前项折叠、跨侧移动、FixedAction行换算、右侧空态退出布局和失败回滚审计。"
```

## 任务 4：升级工作区布局 schema v3

**文件：**

- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceLayoutCodecPrivateTest.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [ ] **步骤 1：编写 v3 round-trip 和 v2 迁移失败测试**

覆盖以下数据合同：

```cpp
void versionThreeRoundTripStoresOneCurrentAndExpandedPerSide();
void versionTwoPrefersValidCurrentOverVisibleRows();
void versionTwoFallsBackToFirstVisibleInAreaOrder();
void versionTwoFallsBackToFirstRegisteredPanelCollapsed();
void versionTwoDropsAdditionalVisibleAndFixedActionRows();
void versionThreeRejectsUnknownDuplicateAndWrongSideIds();
void failedVersionThreeRestoreRollsBackRuntimeState();
```

读取 envelope 头断言 schema 为 `quint16(3)`；用现有 v2 writer 构造 left visible 三项、
current 第二项的输入，restore 后只允许第二项成为 current/visible。无 current 时按四区域
稳定顺序选第一项。FixedAction 稳定 ID 不出现在编码字节中。

- [ ] **步骤 2：运行 codec/state/Shell 测试确认 schema 仍为 2**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceLayoutCodecPrivateTest ZzWorkspaceLayoutStatePrivateTest \
  ZzWorkspaceShellTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^puretools\.workspace-(layout-codec-private|layout-state-private|shell)$' \
  --output-on-failure
```

- [ ] **步骤 3：实现兼容读取和固定 v3 写出**

将 source schema 扩展为：

```cpp
enum class ZzSourceSchema : unsigned char
{
    VersionOne = 1,
    VersionTwo = 2,
    VersionThree = 3
};
```

`decode()` 接受 envelope 1/2/3；保留 v1、v2 reader 仅用于迁移，新增
`encodeVersionThree()` 作为 `saveLayout()` 唯一 writer。v3 每侧只写
`expanded/collapsed`、合法 width 和 current，不写多 visible 集合或 FixedAction。
解码后为内部事务派生 `visible = current ? {current} : {}` 和单一 size，避免同时维护两套
运行时语义。

- [ ] **步骤 4：在纯值规划中按注册表完成 v2 到 v3 归一化**

`buildRestoreTarget()` 使用 snapshot 的 SidePanel 身份过滤请求：有效旧 current 优先；
否则使用旧 visible 中按 area/order 第一项；仍无项时使用同侧首个注册 panel 并强制
collapsed。未知 ID、重复 ID、左右区域不一致和对象 identity 中断返回 `std::nullopt`。
事务应用或回滚期间都不改变 FixedAction 模型行。

- [ ] **步骤 5：运行布局完整回归**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceLayoutCodecPrivateTest ZzWorkspaceLayoutStatePrivateTest \
  ZzWorkspaceShellTest --parallel 2
ctest --test-dir build/linux-gcc-debug -L workspace --output-on-failure
```

- [ ] **步骤 6：提交 v3 布局**

```bash
git add ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.* \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutCodecPrivate.* \
  ZzPureTools/widgets/src/private/ZzWorkspaceLayoutTransactionPrivate.cpp \
  ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp \
  ZzPureTools/tests/ZzWorkspaceLayoutCodecPrivateTest.cpp \
  ZzPureTools/tests/ZzWorkspaceShellTest.cpp
git commit -m "feat(工作区): 升级单活动布局schema v3" \
  -m "固定保存每侧唯一当前项、展开状态和宽度，兼容读取v1/v2并按稳定顺序迁移多活动状态。补充坏数据拒绝和完整回滚测试。"
```

## 任务 5：事务集成 ApplicationWindow 导航表面

**文件：**

- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceNavigationIntegrationTransactionPrivate.h`
- 创建：`ZzPureTools/widgets/src/private/ZzWorkspaceNavigationIntegrationTransactionPrivate.cpp`
- 创建：`ZzPureTools/tests/ZzWorkspaceNavigationIntegrationTest.cpp`
- 修改：`ZzPureTools/CMakeLists.txt`
- 修改：`ZzPureTools/tests/CMakeLists.txt`
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`
- 修改：`ZzPureTools/widgets/src/ZzWorkspaceShell.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzApplicationWindow.h`
- 修改：`ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp`

- [ ] **步骤 1：编写真实窗口成功、拒绝和回滚失败测试**

新增测试进程使用 `ZzWindowKitBootstrap::prepare()` 与 `ZzPureApplication`，通过
`ZzApplicationBuilder::setWindowSetupCallback()` 在窗口显示前创建 Shell。覆盖：

```cpp
void integratesNavigationPaneAndPageHostWithoutChangingRoute();
void pinsPageHostAndRejectsSecondIntegration();
void rejectsPlainMainWindowWithoutMutation();
void rejectsReplacedBodyOrForeignNavigationParent();
void duplicatePanelIdRollsBackBodyModelCurrentAndOwnership();
void synchronousDestructionDuringTransferRollsBackOrFailsClosed();
```

成功断言：NavigationPane 是左 SidePane 后代；PageHost 是 SplitWorkspace 固定、不可关闭
中央 tab；原 body 已销毁；原 model/controller 指针和 current route 不变。失败断言保存
调用前 `centralWidget()`、父子关系、tab 数、panel 数和 route，调用后逐项相等。

- [ ] **步骤 2：运行新增测试确认集成 API 缺失**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceNavigationIntegrationTest --parallel 2
```

预期：编译因 `integrateApplicationNavigation()` 尚未定义而失败。

- [ ] **步骤 3：保存 ApplicationWindow 原始 body 身份并开放最小 friend 边界**

`ZzApplicationWindowPrivate` 增加 `QPointer<QWidget> body` 和 raw identity，在 initialize
创建 body 时记录。`ZzApplicationWindow` 只 friend
`ZzWorkspaceNavigationIntegrationTransactionPrivate`，不新增公开 take getter，不让 Example
直接访问 d_ptr。

- [ ] **步骤 4：实现公开 Shell 集成入口与事务**

公开签名严格使用规格：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> integrateApplicationNavigation(
    const ZzWorkspacePanelId &panelId,
    const QString &panelTitle,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    const QString &centralTabTitle);
```

事务先校验 host 是当前 `ZzApplicationWindow`、body/layout/navigation/pageHost/controller
身份稳定且尚未集成，再捕获父对象、layout index、current route、tab/current、活动模型和
transaction kind。执行顺序为：解除 NavigationPane、注册 SidePanel、解除 PageHost、加入
固定不可关闭 tab、验证模型/controller/route、`takeCentralWidget()` 并删除空 body、提交。
任一步失败按相反顺序移除 tab、取回 panel、恢复 layout/central/body/current route。
transaction kind 新增 `NavigationIntegration`，与 layout restore/activity move 互斥。

- [ ] **步骤 5：运行集成、Builder、导航和 Shell 回归**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceNavigationIntegrationTest ZzApplicationBuilderTest \
  ZzNavigationControllerTest ZzWorkspaceShellTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^puretools\.(workspace-navigation-integration|navigation|workspace-shell)$|^puretools\.application-builder\.' \
  --output-on-failure
```

- [ ] **步骤 6：提交导航集成事务**

```bash
git add ZzPureTools
git commit -m "feat(工作区): 事务集成应用导航表面" \
  -m "将ApplicationWindow的NavigationPane迁入SidePanel并把PageHost固定到中央标签。实现宿主校验、单次集成、同步销毁防护和逐阶段失败回滚。"
```

## 任务 6：实现逐主窗口设置窗口与同一 QAction 命令入口

**文件：**

- 创建：`examples/ZzPureToolsExample/ZzExampleSettingsWindow.h`
- 创建：`examples/ZzPureToolsExample/ZzExampleSettingsWindow.cpp`
- 创建：`examples/ZzPureToolsExample/ZzExampleSettingsWindowPrivate.h`
- 创建：`examples/ZzPureToolsExample/ZzExampleSettingsWindowPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/CMakeLists.txt`
- 修改：`examples/ZzPureToolsExample/tests/CMakeLists.txt`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.h`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleSessionModel.h`
- 修改：`examples/ZzPureToolsExample/ZzExampleSessionModel.cpp`
- 修改：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`

- [ ] **步骤 1：编写设置生命周期和命令一致性失败测试**

在 Example smoke 增加：

```cpp
void settingsActionCreatesOneWindowModalChildPerMainWindow();
void repeatedSettingsActivationRaisesExistingWindow();
void closingSettingsAllowsRecreation();
void settingsWindowsAreIsolatedAcrossTwoMainWindows();
void commandPaletteAndActivityUseTheSameSettingsAction();
void closingMainWindowClosesOnlyItsSettingsWindow();
```

断言 window modality 为 `Qt::WindowModal`，flags 不含
`Qt::WindowStaysOnTopHint`，parent 是对应 `ZzApplicationWindow`，且不同主窗口得到不同
实例。命令测试取得 `zzExampleSettingsAction`，分别点击 Activity 行和选择 command row，
用 `QSignalSpy(QAction::triggered)` 证明都触发该对象。

- [ ] **步骤 2：运行 Example smoke 确认独立设置窗口不存在**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzExampleWorkspaceSmokeTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^example\.workspace-smoke$' --output-on-failure
```

- [ ] **步骤 3：按四文件结构实现设置窗口 View**

公开创建接口返回 `ZzResult`，不让构造函数吞掉 WindowKit 错误：

```cpp
[[nodiscard]] static ZzCore::ZzResult<ZzExampleSettingsWindow *> create(
    ZzPureTools::ZzApplicationWindow *parentWindow,
    std::shared_ptr<ZzExampleApplicationContext> context,
    ZzPureTools::ZzPureApplication *application,
    ZzExampleWindowShell *shell);
```

窗口继承 `QMainWindow`，设置 `Qt::Window`、`Qt::WindowModal` 和
`Qt::WA_DeleteOnClose`。Private 创建 `ZzFluentTitleBar`、`ZzExampleSystemPage(Settings)`、
`ZzExampleSystemViewModel`、既有 `ZzExampleSystemPresenter` 与 `ZzWindowAgent`，按
ApplicationWindow 同样的 chrome 顺序连接最小化/最大化/关闭。View 不访问
`ZzSettingsStore`；Presenter 继续负责读写、主题、日志和 Dock 状态。

- [ ] **步骤 4：注册固定设置 QAction 并复用到命令模型**

`ZzExampleWindowShellPrivate` 保存 `QPointer<ZzExampleSettingsWindow>` 和
`QAction *settingsAction`。设置 action 使用 FontIcon `ZzFontIcon::Gear` 注册到
`LeftSecondary`；trigger 时若指针为空则 create，否则 `show()`、`raise()`、
`activateWindow()`。销毁信号只清空匹配 identity 的指针。

`ZzExampleCommandId` 增加 `ShowSettings`，固定命令描述增加“打开设置”；
`dispatchWorkspaceCommand(ShowSettings)` 只调用 `settingsAction->trigger()`，不复制创建
逻辑。更新枚举范围检查和命令模型行数测试。

- [ ] **步骤 5：运行设置、命令、主题和多窗口回归**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzExampleWorkspaceSmokeTest ZzExampleActivityModelTest \
  ZzTranslationLifecycleTest ZzMultiWindowIsolationTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^example\.(workspace-smoke|puretools-activity-model)$|^puretools\.(translation|multi-window)\.' \
  --output-on-failure
```

- [ ] **步骤 6：提交设置窗口**

```bash
git add examples/ZzPureToolsExample
git commit -m "feat(示例): 增加逐窗口模态设置页" \
  -m "以四文件Pimpl结构组合FluentTitleBar、WindowKit和既有设置Presenter。固定Activity与命令面板复用同一QAction，并覆盖重复激活、关闭重建和多窗口隔离。"
```

## 任务 7：按默认六入口串联组件导航和 Example

**文件：**

- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleRouteCatalog.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExamplePageFactory.cpp`
- 修改：`examples/ZzPureToolsExample/main.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleSmokeControllerPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`
- 修改：`examples/ZzPureToolsExample/translations/ZzPureToolsExample_en.ts`

- [ ] **步骤 1：把 Example smoke 改成最终布局的失败断言**

默认投影必须严格等于：

```text
LeftPrimary:   sessions, files, components
LeftSecondary: settings
RightPrimary:  properties, tasks
RightSecondary: empty
```

测试还断言：中央对象树没有 `ZzNavigationPane`；点击 components 后左 pane 当前内容就是
`window.navigationPane()`；中央固定 tab 内容就是 `window.pageHost()`；路由导航继续改变
PageHost 页面；About 仍位于 Navigation Footer；settings 不在 NavigationModel 和
PageFactory 路由注册中。

- [ ] **步骤 2：运行 smoke 和路由测试确认旧默认布局失败**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzExampleWorkspaceSmokeTest ZzPureToolsExample --parallel 2
ctest --test-dir build/linux-gcc-debug -R '^example\.' --output-on-failure
```

- [ ] **步骤 3：替换旧 central body 嵌套方式**

删除 `takeCentralWidget()` 后将整个 body 加为 tab 的代码。Shell 创建后调用：

```cpp
auto integrated = workspace->integrateApplicationNavigation(
    zzPanelId("components"),
    QCoreApplication::translate("ZzPureToolsExample", "组件"),
    ZzFluentUI::ZzIconDescriptor::fromFontIcon(
        ZzFluentUI::ZzFontIcon::PuzzlePiece),
    ZzFluentUI::ZzActivityArea::LeftPrimary,
    QCoreApplication::translate("ZzPureToolsExample", "组件示例"));
```

只有集成成功后才把 `workspaceWidget()` 设为 central。sessions/files 都注册
`LeftPrimary`，properties/tasks 都注册 `RightPrimary`；注册顺序与最终六入口一致。

- [ ] **步骤 4：从路由目录移除 settings 并更新自动 smoke**

`ZzExampleRouteCatalog` 固定数组由 12 改为 11，删除 settings descriptor；PageFactory 的
`zzSystemPageKind()` 不再由 route ID 返回 Settings，但保留 enum 值供设置窗口直接构造。
SmokeController 中原 settings route 搜索、导航和页面对象断言改为触发
`zzExampleSettingsAction` 并检查 `zzExampleSettingsWindow`。更新翻译源并运行 lrelease
构建，禁止手工编辑 `.qm`。

- [ ] **步骤 5：运行 Example、导航和完整组件定向测试**

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzPureToolsExample ZzExampleWorkspaceSmokeTest \
  ZzNavigationPaneTest ZzWorkspaceShellTest --parallel 2
ctest --test-dir build/linux-gcc-debug \
  -R '^example\.|^fluent\.navigation-pane$|^puretools\.workspace-shell$' \
  --output-on-failure
QT_QPA_PLATFORM=offscreen timeout 15s \
  build/linux-gcc-debug/bin/ZzPureToolsExample --smoke-test
```

- [ ] **步骤 6：提交最终 Example 串联**

```bash
git add examples/ZzPureToolsExample
git commit -m "feat(示例): 串联IDE式六入口工作区" \
  -m "将会话、文件和组件并列到左上，将属性和任务并列到右上，固定设置到左下。组件导航迁入SidePanel，中央只保留PageHost，并更新路由、翻译和自动冒烟。"
```

## 任务 8：更新视觉基线、对象预算和性能证据

**文件：**

- 修改：`ZzPureTools/tests/ZzWorkspaceScreenshotTest.cpp`
- 修改：`ZzPureTools/tests/baselines/linux/`
- 修改：`examples/ZzPureToolsExample/tests/baselines/`
- 修改：`benchmarks/ZzWorkspaceComponentsBenchmark.cpp`
- 修改：`docs/development/BUILDING_ZH.md`
- 修改：`docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`
- 条件修改：`docs/performance/reference/linux/`，仅在三轮证据证明应重建基线时修改。

- [ ] **步骤 1：扩展截图与性能合同测试**

截图 fixture 增加 default、right-empty、right-restored、components、settings、RTL、
keyboard-focus 场景；每个场景覆盖 Light/Dark/HighContrast 与 100/125/150/200 DPR。
Workspace benchmark 增加以下确定性指标：

```text
activity-row-widgets = 0
fixed-action-steady-object-growth = 0
right-empty-layout-width = 0
single-side-visible-panels <= 1
```

在 reference 模式下先运行，预期旧截图不匹配或新指标缺少，形成有效红灯。

- [ ] **步骤 2：生成四档工作区和 Example 截图基线**

```bash
for suffix in 100 125 150 200; do
  case "$suffix" in
    100) scale=1.0 ;;
    125) scale=1.25 ;;
    150) scale=1.5 ;;
    200) scale=2.0 ;;
  esac
  ZZ_UPDATE_SCREENSHOTS=1 QT_QPA_PLATFORM=offscreen \
    QT_SCALE_FACTOR="$scale" QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough \
    build/linux-gcc-benchmarks/ZzPureTools/tests/ZzWorkspaceScreenshotTest \
      --expected-dpr "$scale" --baseline-subdir "dpr-$suffix"
done
```

按 `examples/ZzPureToolsExample/tests/baselines/README.md` 的受控命令生成 Example 基线。
关闭两个更新环境变量后重新运行四档测试，逐张人工检查指示条、侧栏宽度、设置 modality
和文字无重叠；不得以提高像素容差掩盖差异。

- [ ] **步骤 3：运行 workspace-components 和 Example 性能门禁**

```bash
cmake --preset linux-gcc-benchmarks \
  -DZZ_BUILD_EXAMPLES=ON -DZZ_BUILD_BENCHMARKS=ON \
  -DZZ_PERFORMANCE_REFERENCE=ON
cmake --build --preset linux-gcc-benchmarks --parallel 2
ctest --preset linux-gcc-benchmarks \
  -R '^benchmark\.(workspace-components|example-startup|example-navigation|example-theme-switch|example-large-model|example-idle)$|^puretools\.workspace-screenshot-(100|125|150|200)$' \
  --output-on-failure -j1
scripts/ci/run-linux-performance-gates.sh
```

不得放宽 `regression-thresholds.json`。只有环境指纹一致、三轮完整报告通过且功能变化使
参考值永久失真时，才按现有性能基线流程更新 reference JSON；该更新必须在提交正文中
列出每个 metric 的旧值、新值和原因。

- [ ] **步骤 4：更新构建与人工验收文档**

`BUILDING_ZH.md` 增加本功能定向测试命令。Linux 人工清单增加：四区域菜单仅三个目标、
同侧切换/折叠、右侧消失恢复、右物理边指示、设置 WindowModal/重复 raise、多窗口隔离、
X11/Wayland/Qt fallback。没有真实桌面日志时保持“未执行”。

- [ ] **步骤 5：提交视觉与性能证据**

```bash
git add ZzPureTools/tests/ZzWorkspaceScreenshotTest.cpp \
  ZzPureTools/tests/baselines/linux \
  examples/ZzPureToolsExample/tests/baselines \
  benchmarks/ZzWorkspaceComponentsBenchmark.cpp \
  docs/development/BUILDING_ZH.md \
  docs/release/MANUAL_LINUX_CHECKLIST_ZH.md
git commit -m "test(工作区): 更新视觉与性能验收证据" \
  -m "覆盖四档DPR、三主题、左右物理边、右侧空态、组件导航和设置窗口。补充零逐行控件、单活动、空边宽度及现有性能阈值门禁。"
```

性能 reference JSON 若确需更新，单独提交，不与截图提交混合。

## 任务 9：执行完整质量矩阵并记录实施证据

**文件：**

- 修改：`docs/superpowers/plans/2026-08-27-workspace-activity-navigation-settings.md`

- [ ] **步骤 1：运行 Linux GCC shared/static/LTO 完整矩阵**

```bash
for preset in linux-gcc-debug linux-gcc-release linux-static-release \
              linux-gcc-release-lto linux-static-release-lto; do
  cmake --preset "$preset" -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset "$preset" --parallel 2
  ctest --preset "$preset" --output-on-failure
done
```

每个 preset 记录配置、构建、测试总数和失败数。`linux-static-release` 表示一方组件静态
链接，Qt 仍使用匹配 ABI 的动态 SDK。

- [ ] **步骤 2：运行 Clang、clang-tidy 与 ASan/UBSan**

```bash
for preset in linux-clang-tidy-release linux-clang-tidy-static; do
  cmake --preset "$preset" -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset "$preset" --parallel 2
  cmake --build --preset "$preset" --target ZzClangTidy
  ctest --preset "$preset" --output-on-failure
done

cmake --preset linux-clang-asan -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-clang-asan --parallel 2
ctest --preset linux-clang-asan --output-on-failure
```

任何 sanitizer、strict warning 或 tidy 失败必须定位修复，不能加入忽略规则绕过一方代码。

- [ ] **步骤 3：运行安装消费、架构、截图和三轮性能门禁**

```bash
ctest --preset linux-gcc-benchmarks \
  -R '^architecture\.complete-audit$|^platform\.package-relocation$|^puretools\.workspace-screenshot-(100|125|150|200)$' \
  --output-on-failure
scripts/ci/run-linux-performance-gates.sh
```

保持同一 Xvfb/xcb、CPU affinity、runner digest、GPU identity 与现有 reference profile。
环境不匹配必须报告 `INVALID`，不得记录为性能失败或通过。

- [ ] **步骤 4：执行静态跨平台合同并记录真实边界**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzGitHubActionsContract.cmake
cmake -DZZ_PRESETS_FILE="$PWD/CMakePresets.json" \
  -P tests/Platform/PresetMatrixContract.cmake
```

检查公共代码没有 Linux 专用 API 泄漏；Windows MSVC/MinGW 和 macOS 没有本地工具链日志
时记录“未执行”，不使用 Linux 结果替代。

- [ ] **步骤 5：在计划末尾追加实施证据并提交**

追加“实施证据”章节，逐项列出 commit、命令、通过/失败/未执行状态、测试数量、性能报告
目录和人工验收状态。然后执行：

```bash
git diff --check
git status --short
git add docs/superpowers/plans/2026-08-27-workspace-activity-navigation-settings.md
git commit -m "docs(工作区): 记录导航与设置重构验收" \
  -m "汇总GCC shared/static/LTO、Clang、clang-tidy、ASan/UBSan、安装消费、截图、性能及跨平台静态合同的真实结果和未执行边界。"
```

最终 `git status --short` 只允许显示用户已有且始终未读取、未修改、未暂存的
`?? temp_image/`。
