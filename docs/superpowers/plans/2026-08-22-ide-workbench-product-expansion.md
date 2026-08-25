# 产品驱动 IDE Workbench 扩展实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度；每个任务完成验证后立即创建中文 commit。

**目标：** 在不引入 SSH/SFTP 业务依赖的前提下，为 `ZzPureTools` 交付可复用的高密度 IDE Workbench、多组标签分屏、左右多面板、中央底部工具区、CommandBar 和 AnnotatedScrollBar，并完成布局版本迁移与性能门禁。

**架构：** `ZzFluentFoundation` 提供稳定值类型、模型角色和视觉令牌；`ZzFluentUI` 只负责 QWidget 结构、输入、绘制和原子转移；`ZzWorkspaceShell` 负责面板身份、跨组件事务、标题和布局 envelope。Example 只注册页面、模型和 QAction，不重建工作台协调算法。

**技术栈：** Qt 6.8+（Linux 动态验证使用本机 Qt 6.11.1）、C++20、CMakeLists.txt 与 CMakePresets.json、Qt Test、QAction、QSplitter、QTabWidget、QAbstractItemModel、QDataStream、SHA-256。

**前置规格：** `docs/superpowers/specs/2026-08-22-ide-workbench-product-expansion-design.md`

**代码基线：** `549d8ec`

---

## 执行前约束

- 只在 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro` 中修改和提交。
- 不删除、不修改、不暂存 `temp_image/`。
- 本机使用现有 Qt，不下载新的 Qt SDK：

  ```bash
  export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
  export GCC_13=/usr/bin/gcc-15
  export GXX_13=/usr/bin/g++-15
  cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  ```

- 每个任务严格执行红、绿、重构循环；先看到目标测试按预期失败，再写最小实现。
- 所有公开类和有状态控件采用 PIMPL；公开方法和复杂事务使用简体中文 Doxygen。
- 禁止链式命名空间、Qt Private API、stylesheet、业务模型访问和新的视觉债务白名单。
- 每次 commit 只暂存任务列出的文件，提交前运行 `git diff --cached --check`。
- 不 push，不调用 GitHub CLI，不把未执行的平台写成通过。

## 文件结构与职责

### Foundation

- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzSidePaneMode.h`：Single/Stacked 模式。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzTabGroupId.h` 与 `ZzFluentUI/foundation/src/ZzTabGroupId.cpp`：trim 后非空、可比较和可哈希的组 ID。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerRole.h`：标记模型角色。
- 创建 `ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerKind.h`：标记语义枚举。
- 修改 `ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h`、`ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp`：加入工作台和新控件尺寸令牌。
- 修改 `ZzFluentUI/CMakeLists.txt`：注册 Foundation 源和 FluentUI 新源/MOC 头。

### Workbench 控件

- 创建 `ZzPanelStack`、`ZzSplitWorkspace`、`ZzBottomPane` 各自的公开 `.h`、公开 `.cpp`、Private `.h`、Private `.cpp`。
- 修改 `ZzActivityBar` 四文件：多个 active source index 的绘制、模型收敛和信号。
- 修改 `ZzSidePane` 四文件：固定 `ZzPanelStack`、Single/Stacked 兼容语义和显隐 API。
- 创建 `ZzCommandBar` 四文件：QAction 主次分组、自适应展示和 overflow。
- 创建 `ZzAnnotatedScrollBar` 四文件，并修改 `ZzScrollBar.h` 移除 `final`：模型缓存、像素桶绘制和点击。
- 修改 `ZzSplitButton` 四文件和 `ZzPivot` 四文件：原生 checkable 合同和图标项。

### Shell 与布局

- 修改 `ZzWorkspaceShell` 四文件：SplitWorkspace、BottomPane、多活动侧栏、PanelKind::Bottom、标题同步和跨侧事务。
- 修改 `ZzPureTools/CMakeLists.txt`：保持 `Zz::PureTools` 对 `Zz::FluentUI` 的既有依赖，不新增反向依赖。
- 修改 `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`：布局版本 1 迁移、版本 2 round trip 和完整回滚。

### 消费、示例与质量

- 创建 `ZzFluentUI/tests/ZzPanelStackTest.cpp`、`ZzSplitWorkspaceTest.cpp`、`ZzBottomPaneTest.cpp`、`ZzCommandBarTest.cpp`、`ZzAnnotatedScrollBarTest.cpp`。
- 修改现有 ActivityBar、SidePane、SplitButton、Pivot、ThemeSnapshot、WorkspaceShell、截图和 Example smoke 测试。
- 修改 `tests/InstallConsumer/Gui/main.cpp`、`tests/InstallConsumer/CMakeLists.txt`、`tests/PublicHeaderConsumer/CMakeLists.txt` 和架构审计。
- 修改 `examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.*`、`ZzExampleWorkspaceContent.*`、Example CMake 和 smoke 测试。
- 修改 `benchmarks/ZzWorkspaceComponentsBenchmark.cpp`、性能合同和中文开发/平台/性能/人工验收文档。

## 里程碑一：工作台结构

### 任务 1：新增 Foundation 值类型、模型角色和视觉令牌

**文件：**
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzSidePaneMode.h`
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzTabGroupId.h`
- 创建：`ZzFluentUI/foundation/src/ZzTabGroupId.cpp`
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerRole.h`
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerKind.h`
- 修改：`ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h`
- 修改：`ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 测试：`ZzFluentUI/tests/ZzThemeSnapshotTest.cpp`

- [ ] **步骤 1：编写失败测试并注册值类型源。** 在 `ZzThemeSnapshotTest.cpp` 增加如下断言，并在 `ZzFluentUI/CMakeLists.txt` 把 `foundation/src/ZzTabGroupId.cpp` 放入 `zz_fluent_foundation_sources`：

  ```cpp
  void exposesWorkbenchTokensAndStableGroupIds()
  {
      const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
          ZzFluentUI::ZzThemeMode::Light, QColor(), 1, true);
      QVERIFY(snapshot.metric(ZzFluentUI::ZzMetricToken::PanelHeaderHeight) > 0.0);
      QVERIFY(snapshot.metric(ZzFluentUI::ZzMetricToken::PanelSplitterExtent) > 0.0);
      QVERIFY(snapshot.metric(ZzFluentUI::ZzMetricToken::CommandBarHeight) > 0.0);
      QVERIFY(snapshot.metric(ZzFluentUI::ZzMetricToken::ScrollMarkerThickness) > 0.0);

      const ZzFluentUI::ZzTabGroupId first(QStringLiteral("  group-a  "));
      const ZzFluentUI::ZzTabGroupId same(QStringLiteral("group-a"));
      QCOMPARE(first.value(), QStringLiteral("group-a"));
      QCOMPARE(first, same);
      QVERIFY(first.isValid());
      QVERIFY(!ZzFluentUI::ZzTabGroupId().isValid());
      QCOMPARE(qHash(first), qHash(same));
  }
  ```

- [ ] **步骤 2：运行测试验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzThemeSnapshotTest --parallel 2
  ```

  预期：编译失败，错误明确指出 `ZzTabGroupId` 和新增 `ZzMetricToken` 尚不存在。

- [ ] **步骤 3：实现最小 Foundation 合同。** `ZzTabGroupId` 复用 `ZzWorkspacePanelId` 的规范化和哈希模式，但导出宏使用 `ZZ_FLUENT_FOUNDATION_EXPORT`。新增枚举必须是具名、定长底层类型：

  ```cpp
  enum class ZzSidePaneMode : std::uint8_t { Single, Stacked };

  enum class ZzScrollMarkerKind : std::uint8_t {
      Information, Success, Warning, Error,
      Bookmark, SearchMatch, Custom
  };

  enum class ZzScrollMarkerRole : int {
      Position = Qt::UserRole + 0x200,
      Kind,
      Color,
      Priority
  };
  ```

  在 `ZzMetricToken::Count` 前依次加入 `PanelHeaderHeight`、`PanelSplitterExtent`、`WorkspaceDropTargetExtent`、`BottomPaneHeaderHeight`、`CommandBarHeight`、`CommandBarMoreExtent`、`AnnotatedScrollBarExtent`、`ScrollMarkerThickness`，并在 `ZzThemeSnapshot.cpp` 数组尾部写入 `32.0, 4.0, 48.0, 32.0, 40.0, 32.0, 16.0, 3.0`。

- [ ] **步骤 4：验证 Foundation 与公共头。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzThemeSnapshotTest ZzPublicHeadersTest --parallel 2
  ctest --preset linux-gcc-debug -R '^(fluent.theme-snapshot|architecture.public-headers)$' --output-on-failure
  ```

  预期：两个测试通过，新增公开头可独立包含。

- [ ] **步骤 5：提交。**

  ```bash
  git add \
    ZzFluentUI/foundation/include/ZzFluentUI/ZzSidePaneMode.h \
    ZzFluentUI/foundation/include/ZzFluentUI/ZzTabGroupId.h \
    ZzFluentUI/foundation/src/ZzTabGroupId.cpp \
    ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerRole.h \
    ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerKind.h \
    ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h \
    ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp \
    ZzFluentUI/CMakeLists.txt \
    ZzFluentUI/tests/ZzThemeSnapshotTest.cpp
  git diff --cached --check
  git commit -m "基础：新增工作台值类型与视觉令牌" -m "新增侧栏模式、标签组标识和滚动标记模型合同。\n\n扩充工作台、命令栏和标记滚动条所需尺寸令牌，并通过 Foundation 与公共头测试验证导出边界。"
  ```

### 任务 2：实现 ZzPanelStack 多面板容器

**文件：**
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzPanelStack.h`
- 创建：`ZzFluentUI/widgets/src/ZzPanelStack.cpp`
- 创建：`ZzFluentUI/widgets/src/private/ZzPanelStackPrivate.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzPanelStackPrivate.cpp`
- 创建：`ZzFluentUI/tests/ZzPanelStackTest.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [ ] **步骤 1：先写所有权、显隐和尺寸失败测试。** 测试类至少包含以下用例：

  ```cpp
  void ownsTakesAndKeepsVisibleSizes()
  {
      ZzFluentUI::ZzPanelStack stack;
      auto *first = new QWidget;
      auto *second = new QWidget;
      QVERIFY(stack.addPanel(first, QStringLiteral("Sessions")));
      QVERIFY(stack.addPanel(second, QStringLiteral("Files")));
      QCOMPARE(stack.visiblePanels(), QList<QWidget *>({first, second}));
      QVERIFY(stack.setPanelSizes({180, 320}));
      QVERIFY(stack.setPanelVisible(first, false));
      QCOMPARE(stack.visiblePanels(), QList<QWidget *>({second}));
      QVERIFY(!stack.setPanelSizes({0}));
      QVERIFY(stack.setPanelVisible(first, true));
      QCOMPARE(stack.visiblePanelCount(), 2);
      QCOMPARE(stack.takePanel(first), first);
      QCOMPARE(first->parent(), nullptr);
      delete first;
  }
  ```

  同文件增加：有父对象页面被拒绝且父对象不变、重复页面不重建 frame、外部销毁自动清理、关闭按钮只发 `panelCloseRequested`、1000 次显隐后 QObject/QTimer/QAbstractAnimation 数不变。

- [ ] **步骤 2：运行测试验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzPanelStackTest --parallel 2
  ```

  预期：编译失败，原因是 `ZzPanelStack` 公开头或目标尚不存在。

- [ ] **步骤 3：实现固定 Frame 与单一 QSplitter。** Private 中使用一条记录对应一个固定 frame：

  ```cpp
  struct ZzPanelRecord final
  {
      QPointer<QWidget> content;
      QWidget *identity = nullptr;
      ZzPanelFrame *frame = nullptr;
      QString title;
      ZzIconDescriptor icon;
      int lastNonZeroSize = 160;
      QMetaObject::Connection destroyedConnection;
  };

  QSplitter *splitter = nullptr;
  QList<ZzPanelRecord> panels;
  QPointer<QWidget> currentPanel;
  ```

  `ZzPanelFrame` 只定义在 Private 实现文件中，固定持有标题、缓存图标绘制和关闭 `QToolButton`。`addPanel()` 在校验非空、GUI 线程、无父对象和未注册后才设置父对象；`takePanel()` 先验证再移除 frame 并把 content 设为无父；`setPanelSizes()` 只接受与可见面板数相等的正整数列表。

- [ ] **步骤 4：运行定向与架构测试。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzPanelStackTest --parallel 2
  ctest --preset linux-gcc-debug -R '^(fluent.panel-stack|architecture.boundaries|architecture.complete-audit)$' --output-on-failure
  ```

  预期：所有权、销毁重入、尺寸和对象稳定性断言通过，源码没有硬编码尺寸或业务词泄漏。

- [ ] **步骤 5：提交。**

  ```bash
  git add \
    ZzFluentUI/CMakeLists.txt \
    ZzFluentUI/tests/CMakeLists.txt \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzPanelStack.h \
    ZzFluentUI/widgets/src/ZzPanelStack.cpp \
    ZzFluentUI/widgets/src/private/ZzPanelStackPrivate.h \
    ZzFluentUI/widgets/src/private/ZzPanelStackPrivate.cpp \
    ZzFluentUI/tests/ZzPanelStackTest.cpp
  git diff --cached --check
  git commit -m "组件：新增可调尺寸的多面板堆栈" -m "实现单一纵向 QSplitter、固定面板框架、显隐尺寸恢复和所有权归还。\n\n补充非法输入、外部销毁、关闭意图和千次显隐对象稳定性测试。"
  ```

### 任务 3：扩展 SidePane 多面板模式与 ActivityBar 多激活状态

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSidePane.h`
- 修改：`ZzFluentUI/widgets/src/ZzSidePane.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSidePanePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSidePanePrivate.cpp`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzActivityBar.h`
- 修改：`ZzFluentUI/widgets/src/ZzActivityBar.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzSidePaneTest.cpp`
- 修改：`ZzFluentUI/tests/ZzActivityBarTest.cpp`

- [ ] **步骤 1：为兼容模式和多个 active index 写失败测试。** SidePane 测试中断言默认 Single 行为不变，Stacked 下两个页面同时可见且独立尺寸可恢复：

  ```cpp
  pane.setMode(ZzFluentUI::ZzSidePaneMode::Stacked);
  QVERIFY(pane.setWidgetVisible(first, true));
  QVERIFY(pane.setWidgetVisible(second, true));
  QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({first, second}));
  QCOMPARE(pane.panelStack()->visiblePanelCount(), 2);
  pane.setCollapsed(true);
  pane.setCollapsed(false);
  QCOMPARE(pane.visibleWidgets(), QList<QWidget *>({first, second}));
  ```

  ActivityBar 测试设置三个源索引，断言去重、拒绝 child/column 1/其他模型索引、model reset 后失效项清理，以及 `currentSourceIndex` 仍只表示最后焦点项。

- [ ] **步骤 2：运行测试验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzSidePaneTest ZzActivityBarTest --parallel 2
  ```

  预期：新增模式、可见集合和 active API 缺失导致编译失败。

- [ ] **步骤 3：让 SidePane 委托固定 PanelStack。** 删除 Private 的 `QStackedWidget`、单独标题 QLabel 和标题哈希，改为：

  ```cpp
  ZzPanelStack *panelStack = nullptr;
  ZzSidePaneMode mode = ZzSidePaneMode::Single;
  QList<QPointer<QWidget>> stackedVisible;
  ```

  `addWidget()`、`takeWidget()`、`setCurrentWidget()` 保持原签名；Single 模式切换当前项时只显示目标，Stacked 模式只确保目标可见。新增 `modeChanged`，重复 setter 不发信号；折叠只隐藏 `contentHost`，不改子面板显隐和尺寸状态。

- [ ] **步骤 4：实现 Activity 多激活缓存和绘制。** Private 使用有序 `QList<QPersistentModelIndex>` 保存 active 集合，新增：

  ```cpp
  bool multiActiveEnabled = false;
  QList<QPersistentModelIndex> activeSourceIndexes;

  void sanitizeActiveIndexes();
  [[nodiscard]] bool isSourceIndexActive(const QModelIndex &index) const;
  ```

  delegate 在每个 active 项的逻辑 leading 边缘绘制 `SelectionIndicatorThickness` 指示条；选择模型仍只同步 current。模型 rowsRemoved/modelReset/layoutChanged 后清理并只发一次 `activeSourceIndexesChanged`。

- [ ] **步骤 5：验证现有兼容与新行为。**

  ```bash
  ctest --preset linux-gcc-debug -R '^(fluent.side-pane|fluent.activity-bar|fluent.panel-stack)$' --output-on-failure
  ```

  预期：原 Single 测试继续通过；Stacked 和多激活状态在 LTR/RTL 下不与图标重叠，重复同步不增加 QObject。

- [ ] **步骤 6：提交。**

  ```bash
  git add \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzSidePane.h \
    ZzFluentUI/widgets/src/ZzSidePane.cpp \
    ZzFluentUI/widgets/src/private/ZzSidePanePrivate.h \
    ZzFluentUI/widgets/src/private/ZzSidePanePrivate.cpp \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzActivityBar.h \
    ZzFluentUI/widgets/src/ZzActivityBar.cpp \
    ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.h \
    ZzFluentUI/widgets/src/private/ZzActivityBarPrivate.cpp \
    ZzFluentUI/tests/ZzSidePaneTest.cpp \
    ZzFluentUI/tests/ZzActivityBarTest.cpp
  git diff --cached --check
  git commit -m "组件：支持侧栏多面板与多入口激活" -m "将 SidePane 改为委托固定 PanelStack，并保留默认 Single 兼容语义。\n\n扩展 ActivityBar 的当前焦点与多个可见入口状态，补齐模型收敛、RTL 绘制和对象预算测试。"
  ```

### 任务 4：实现 SplitWorkspace 树结构与组生命周期

**文件：**
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- 创建：`ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp`
- 创建：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 创建：`ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [ ] **步骤 1：写初始组、分割、扁平化和上限测试。**

  ```cpp
  void splitsFlattensAndKeepsOneLeaf()
  {
      ZzFluentUI::ZzSplitWorkspace workspace;
      QCOMPARE(workspace.groupIds().size(), 1);
      const auto root = workspace.groupIds().constFirst();
      workspace.tabWidget(root)->addTab(
          new QWidget, QStringLiteral("Pinned"));
      const auto right = workspace.splitGroup(
          root, Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
      QVERIFY(right.has_value());
      const auto farRight = workspace.splitGroup(
          right.value(), Qt::Horizontal, ZzFluentUI::ZzSplitPlacement::After);
      QVERIFY(farRight.has_value());
      QCOMPARE(workspace.groupIds().size(), 3);
      QVERIFY(workspace.removeEmptyGroup(right.value()));
      QCOMPARE(workspace.groupIds().size(), 2);
      QVERIFY(!workspace.removeEmptyGroup(root));
  }
  ```

  增加 requestedId 重复拒绝、64 组上限、16 层上限、空组删除只在无 tab 时成功、active group 幂等信号和每组恰好一个 `ZzTabWidget` 的断言。用 `QAccessible::queryAccessibleInterface()` 验证叶子保留 PageTabList 语义；聚焦任意组后应成为 active，`focusAdjacentGroup(Qt::RightEdge)` 应按实际几何聚焦右侧最近组，边界无相邻组时返回 false 且焦点不变。

- [ ] **步骤 2：运行测试验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzSplitWorkspaceTest --parallel 2
  ```

  预期：公开类和目标不存在导致编译失败。

- [ ] **步骤 3：实现递归 Node 与固定根宿主。** Private 使用 C++20 `std::variant` 和 `std::unique_ptr` 管理树，不把业务身份放入动态属性：

  ```cpp
  struct ZzNode;
  struct ZzBranch final {
      Qt::Orientation orientation = Qt::Horizontal;
      QSplitter *splitter = nullptr;
      std::vector<std::unique_ptr<ZzNode>> children;
  };
  struct ZzLeaf final {
      ZzTabGroupId id;
      ZzTabWidget *tabs = nullptr;
  };
  struct ZzNode final {
      ZzNode *parent = nullptr;
      std::variant<ZzBranch, ZzLeaf> value;
  };
  ```

  初始构造创建一个 UUID 无花括号 ID 的叶子；同方向相邻 branch 在提交后扁平化；删除叶子后单子 branch 提升唯一 child；最后一个叶子不删除。公开补充 `bool focusAdjacentGroup(Qt::Edge direction)`，Private 根据各叶子 `mapToGlobal(rect())` 的中心点筛选指定半平面，再按主轴距离、次轴距离和稳定组顺序排序，不访问平台布局 API。只在成功提交后发 `groupAdded`、`groupAboutToBeRemoved`、`layoutChanged`。

- [ ] **步骤 4：验证结构和销毁稳定性。**

  ```bash
  ctest --preset linux-gcc-debug -R '^fluent.split-workspace$' --output-on-failure
  ```

  预期：分割/合并 1000 次后组数、QSplitter 数、QTabWidget 数与树一致，无 timer 和 animation。

- [ ] **步骤 5：提交。**

  ```bash
  git add \
    ZzFluentUI/CMakeLists.txt \
    ZzFluentUI/tests/CMakeLists.txt \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h \
    ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp \
    ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h \
    ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp \
    ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp
  git diff --cached --check
  git commit -m "组件：新增递归标签分屏工作区" -m "实现有界 Branch/Leaf 树、标签组强标识、同向扁平化和空组收敛。\n\n通过组上限、深度上限、信号幂等和千次分割合并对象稳定性测试。"
  ```

### 任务 5：实现 SplitWorkspace 标签转移与五区拖放事务

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- 修改：`ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp`

- [ ] **步骤 1：写中心转移、边缘分屏和失败不变测试。** 测试分别覆盖 Center/Left/Top/Right/Bottom，并保存来源页面、索引、父对象、组列表和 splitter sizes 作为失败前快照：

  ```cpp
  QVERIFY(workspace.transferTab(sourceId, 0, targetId, 0));
  QCOMPARE(workspace.tabWidget(targetId)->widget(0), page);

  const QList<ZzFluentUI::ZzTabGroupId> before = workspace.groupIds();
  QVERIFY(!workspace.moveTabToDropZone(
      targetId, 99, targetId, ZzFluentUI::ZzWorkspaceDropZone::Left));
  QCOMPARE(workspace.groupIds(), before);
  QCOMPARE(workspace.tabWidget(targetId)->widget(0), page);
  ```

  增加 transfer 信号中同步销毁 source、target、page，以及第三方同步接管 page 的用例；断言失败时不强取回第三方页面。

- [ ] **步骤 2：运行目标用例确认失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzSplitWorkspaceTest --parallel 2
  build/linux-gcc-debug/ZzFluentUI/tests/ZzSplitWorkspaceTest transfersTabsAndRollsBackEdgeDrops
  ```

  预期：新增测试失败，转移或 DropZone 事务尚未实现。

- [ ] **步骤 3：实现统一 transfer 事务。** Center 直接调用已有 `ZzTabWidget::transferTabTo()`；边缘区域按以下顺序实现，禁止提前取出 page：

  ```cpp
  const ZzTreeSnapshot snapshot = captureTreeSnapshot();
  ZzLeaf *temporary = createAdjacentLeaf(target, orientationFor(zone), placementFor(zone));
  if (temporary == nullptr
      || !sourceTabs->transferTabTo(temporary->tabs, sourceIndex)) {
      restoreTreeSnapshot(snapshot);
      return false;
  }
  removeSourceIfEmpty(source);
  flattenBranches();
  setActiveGroup(temporary->id);
  ```

  所有原始对象使用 `QPointer`/稳定 ID 重新查找，不在跨 Qt 信号调用后解引用裸指针。成功后只发一次 `tabDropCommitted` 和一次 `layoutChanged`。

- [ ] **步骤 4：实现单一 Drop overlay 和进程内令牌。** Private 只创建一个按需显示的 overlay 子 QWidget；拖拽 MIME 包含随机令牌、source group ID 和 source index，令牌保存在工作区实例内并有界清理。overlay 的五区几何使用 `WorkspaceDropTargetExtent`，不接受焦点，不为每个组创建 QWidget。

- [ ] **步骤 5：验证转移、RTL 与对象预算。**

  ```bash
  ctest --preset linux-gcc-debug -R '^fluent.split-workspace$' --output-on-failure
  ```

  预期：五区拖放、伪造令牌拒绝、同步销毁、第三方接管、RTL Left/Right 镜像均通过；任意组数量下 overlay 始终只有一个。

- [ ] **步骤 6：提交。**

  ```bash
  git add \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h \
    ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp \
    ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h \
    ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp \
    ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp
  git diff --cached --check
  git commit -m "组件：完善标签分屏转移与五区拖放" -m "在 SplitWorkspace 中复用 TabWidget 原子转移，加入中心和四边 DropZone 事务。\n\n补齐伪造载荷、同步销毁、第三方接管、RTL 与单一覆盖层对象预算测试。"
  ```

### 任务 6：实现 SplitWorkspace 页面键与布局恢复

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h`
- 修改：`ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp`

- [ ] **步骤 1：写 page key 和二进制布局测试。** 覆盖 trim 后空键表示取消、最大 256 UTF-16 code unit、重复键拒绝且旧值不变、缺失 keyed page 可恢复、unkeyed page 保留、损坏/超限布局拒绝且状态不变：

  ```cpp
  QVERIFY(workspace.setPageLayoutKey(firstPage, QStringLiteral("terminal:a")));
  QVERIFY(!workspace.setPageLayoutKey(secondPage, QStringLiteral("terminal:a")));
  const QByteArray saved = workspace.saveLayout();
  QVERIFY(!saved.isEmpty());
  QVERIFY(workspace.transferTab(sourceId, 0, targetId));
  QVERIFY(workspace.restoreLayout(saved));
  QCOMPARE(workspace.savedGroupForPageKey(QStringLiteral("terminal:a")), sourceId);
  QCOMPARE(workspace.pageLayoutKey(firstPage), QStringLiteral("terminal:a"));
  ```

- [ ] **步骤 2：运行目标用例确认失败。**

  ```bash
  build/linux-gcc-debug/ZzFluentUI/tests/ZzSplitWorkspaceTest savesAndRestoresKeyedPagesTransactionally
  ```

  预期：测试失败，页面键和布局序列化尚未实现。

- [ ] **步骤 3：实现有界独立布局格式。** SplitWorkspace 自身格式使用 `ZZSW` magic、版本 1、`QDataStream::Qt_6_8`、payload 长度和 SHA-256；payload 写入树节点 tag、orientation、ID、splitter sizes、active ID，以及 keyed page 的 key/group/order/current。解码先构造纯数据 DTO 并验证：组数不超过 64、深度不超过 16、字符串长度有界、ID/key 唯一、sizes 数量匹配且为正。

- [ ] **步骤 4：实现事务提交和回滚。** 恢复前捕获树、组顺序、所有现存 page 所在组/索引和当前状态；新树先在离屏临时根下构造，验证完成后再交换到公开布局。keyed page 通过 `transferTabTo()` 移动；缺失 key 只保留映射供 `savedGroupForPageKey()` 查询；unkeyed page 无法保留旧叶子时移动到 active 叶子。任一步失败恢复原树与页面位置。

- [ ] **步骤 5：运行完整 SplitWorkspace 测试。**

  ```bash
  ctest --preset linux-gcc-debug -R '^fluent.split-workspace$' --output-on-failure
  ```

  预期：round trip、损坏摘要、截断、重复 ID/key、超深树和同步销毁回滚全部通过。

- [ ] **步骤 6：提交。**

  ```bash
  git add \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h \
    ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp \
    ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h \
    ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp \
    ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp
  git diff --cached --check
  git commit -m "组件：持久化标签分屏与页面布局键" -m "为 SplitWorkspace 增加有界二进制布局、稳定页面键和缺失页面恢复语义。\n\n通过完整预解码、离屏建树和页面位置快照保证恢复全成或全不成。"
  ```

### 任务 7：实现 ZzBottomPane 中央底部工具区

**文件：**
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzBottomPane.h`
- 创建：`ZzFluentUI/widgets/src/ZzBottomPane.cpp`
- 创建：`ZzFluentUI/widgets/src/private/ZzBottomPanePrivate.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzBottomPanePrivate.cpp`
- 创建：`ZzFluentUI/tests/ZzBottomPaneTest.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [ ] **步骤 1：写所有权、切换、折叠和高度测试。**

  ```cpp
  void keepsCurrentToolAndHeightAcrossCollapse()
  {
      ZzFluentUI::ZzBottomPane pane;
      auto *terminal = new QWidget;
      auto *problems = new QWidget;
      QVERIFY(pane.addWidget(terminal, QStringLiteral("Terminal")));
      QVERIFY(pane.addWidget(problems, QStringLiteral("Problems")));
      QVERIFY(pane.setCurrentWidget(terminal));
      pane.setPaneHeight(260);
      pane.setCollapsed(true);
      QCOMPARE(pane.currentWidget(), terminal);
      QCOMPARE(pane.lastExpandedHeight(), 260);
      pane.setCollapsed(false);
      QCOMPARE(pane.paneHeight(), 260);
      QCOMPARE(pane.takeWidget(problems), problems);
      delete problems;
  }
  ```

  增加有父对象/重复内容拒绝、最小最大钳制、4 px 把手拖动、当前工具关闭按钮只发 `widgetCloseRequested`、外部销毁和 1000 次折叠对象稳定性。

- [ ] **步骤 2：运行测试验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzBottomPaneTest --parallel 2
  ```

  预期：类和目标缺失导致编译失败。

- [ ] **步骤 3：实现固定对象树。** Private 固定拥有一个 `ZzPivot`、一个 `QStackedWidget`、一个把手和一个关闭 `QToolButton`。图标项暂时通过继承自 QTabBar 的 `addTab(icon, text)` 添加；不依赖后续 Pivot 便利 API。`setCollapsed()` 隐藏内容区并保存最后合法高度，不销毁工具；把手只在展开时处理鼠标。

- [ ] **步骤 4：验证定向与无障碍测试。**

  ```bash
  ctest --preset linux-gcc-debug -R '^(fluent.bottom-pane|fluent.pivot)$' --output-on-failure
  ```

  预期：工具切换、键盘、折叠恢复、take 和对象预算通过，BottomPane 不创建 Dock 或顶层窗口。

- [ ] **步骤 5：提交。**

  ```bash
  git add \
    ZzFluentUI/CMakeLists.txt \
    ZzFluentUI/tests/CMakeLists.txt \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzBottomPane.h \
    ZzFluentUI/widgets/src/ZzBottomPane.cpp \
    ZzFluentUI/widgets/src/private/ZzBottomPanePrivate.h \
    ZzFluentUI/widgets/src/private/ZzBottomPanePrivate.cpp \
    ZzFluentUI/tests/ZzBottomPaneTest.cpp
  git diff --cached --check
  git commit -m "组件：新增中央底部工具面板" -m "实现固定 Pivot、页面堆栈、关闭意图和可调高度的 BottomPane。\n\n验证页面所有权、折叠恢复、把手边界、外部销毁与千次状态切换稳定性。"
  ```

### 任务 8：将 SplitWorkspace 和 BottomPane 接入 WorkspaceShell

**文件：**
- 修改：`ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h`
- 修改：`ZzPureTools/widgets/src/ZzWorkspaceShell.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [ ] **步骤 1：写对象树、Bottom 注册和活动组标题测试。**

  ```cpp
  QVERIFY(fixture.shell->splitWorkspace() != nullptr);
  QVERIFY(fixture.shell->bottomPane() != nullptr);
  QCOMPARE(
      fixture.shell->tabWidget(),
      fixture.shell->splitWorkspace()->tabWidget(
          fixture.shell->splitWorkspace()->activeGroupId()));

  auto *output = new QWidget;
  QVERIFY(fixture.shell->registerBottomPanel(
      zzPanelId("output"), QStringLiteral("Output"), {}, output));
  QVERIFY(fixture.shell->showPanel(zzPanelId("output"), true));
  QCOMPARE(fixture.shell->bottomPane()->currentWidget(), output);
  ```

  再创建第二个组和页面，切换 active group/current tab/page windowTitle，断言宿主和 FluentTitleBar 使用活动组标题；Side/Bottom/Dock 之间重复 PanelId 必须拒绝且不接管内容。

- [ ] **步骤 2：运行 WorkspaceShell 测试确认失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest createsSplitWorkspaceAndBottomPane
  ```

  预期：新访问器、Bottom 注册接口和 PanelKind 尚不存在。

- [ ] **步骤 3：替换中央对象树并保持 tabWidget 兼容入口。** Private 用 `centerHost/QVBoxLayout` 依次挂 `splitWorkspace` 和 `bottomPane`，左右 SidePane 默认设为 Stacked。`tabs` 成员删除；`tabWidget()` 每次从 active group 查询，宿主销毁后返回 nullptr。公开头明确该指针会随 active group 改变。

- [ ] **步骤 4：扩展面板注册和标题连接。** `ZzPanelKind` 加入 Bottom，`registerBottomPanel()` 复用全局 ID 校验、所有权观察和 rollback 机制。标题观察连接改为：

  ```cpp
  splitWorkspace->activeGroupChanged
      -> refreshActiveTabConnections();
  activeTabs->currentChanged
      -> refreshCurrentTabConnection();
  activeTabs->pagePresentationChanged
      -> refreshTitle();
  currentPage->windowTitleChanged
      -> refreshTitle();
  ```

  Bottom 的 `takePanel()`、`showPanel()` 和外部销毁路径与 Side/Dock 使用同一注册表，不允许 switch 的 default 分支吞掉新枚举。

- [ ] **步骤 5：验证 Shell 与旧消费入口。**

  ```bash
  ctest --preset linux-gcc-debug -R '^(puretools.workspace-shell|fluent.split-workspace|fluent.bottom-pane)$' --output-on-failure
  ```

  预期：旧 `tabWidget()` 消费仍工作，活动组变化同步标题，三类 PanelId 全局唯一且 take 后恢复无父对象。

- [ ] **步骤 6：提交。**

  ```bash
  git add \
    ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceShell.h \
    ZzPureTools/widgets/src/ZzWorkspaceShell.cpp \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h \
    ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp \
    ZzPureTools/tests/ZzWorkspaceShellTest.cpp
  git diff --cached --check
  git commit -m "框架：接入分屏工作区与底部面板" -m "将 WorkspaceShell 中央区域升级为 SplitWorkspace 和 BottomPane，并保留活动组 tabWidget 兼容入口。\n\n扩展 Bottom 面板注册、三类全局 ID、所有权回滚和活动组标题同步测试。"
  ```

### 任务 9：实现 WorkspaceShell 多面板事务与布局版本 2

**状态（2026-08-25）：** 本任务已由
`docs/superpowers/plans/2026-08-23-workspace-shell-transaction-redesign.md` 的任务 9R
取代并完成。最终实现以不可变目标投影、有界 codec、统一 Activity move/布局事务和反向回滚
替代本节原定的两阶段原型方案；`feature/workspace-shell-transaction-redesign` 已在本地合并到
`master`，最终代码与验收文档收口提交为 `7acd165`。

**文件：**
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [x] **步骤 1：写 Activity 重排、跨侧迁移和多可见同步测试。** 注册左侧三个面板和右侧一个面板，直接发出 `moveRequested`，验证同组重排、跨组、跨侧、可见状态、高度和 active index 集合：

  ```cpp
  leftBar->moveRequested(
      leftBar->model()->index(0, 0),
      ZzFluentUI::ZzActivityArea::RightPrimary,
      0);
  QCOMPARE(rightPane->visibleWidgets().constFirst(), movedContent);
  QVERIFY(rightBar->activeSourceIndexes().contains(
      rightBar->model()->index(0, 0)));
  QVERIFY(!leftPane->visibleWidgets().contains(movedContent));
  ```

  在 SidePane `addWidget()` 的同步 ParentChange 信号中销毁目标、来源或第三方接管 content，断言迁移失败时来源 Area、顺序、父对象、显隐和尺寸全部回滚。

- [x] **步骤 2：写版本 1 迁移和版本 2 有界解码测试。** 保留测试内的版本 1 encoder，断言旧 `leftCurrent/rightCurrent/currentTabIndex` 迁移为每侧唯一可见面板、根组当前标签和默认折叠 Bottom。版本 2 测试覆盖每侧 32 可见面板、4096 side entries、64 组、16 深度、重复 ID、sizes 数量错误、非法枚举、摘要错误、1 MiB 上限和 Qt Dock state 版本分离。

- [x] **步骤 3：运行目标测试确认失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzWorkspaceShellTest --parallel 2
  build/linux-gcc-debug/ZzPureTools/tests/ZzWorkspaceShellTest migratesVersionOneLayoutToVersionTwo
  ```

  预期：当前 decoder 只接受 schema 1，且 Activity move 仍未提交。

- [x] **步骤 4：实现 Activity move 两阶段事务。** 先把源面板记录、Activity 行顺序、来源/目标 PanelStack sizes 和显隐集合复制为值快照；同侧只改模型 Area/order；跨侧先 `takeWidget()`，再 `addWidget()`，最后更新模型。目标接管失败时重新插入来源原索引并恢复所有快照；成功后统一调用 `syncActivityState()`，避免逐入口发中间状态。

- [x] **步骤 5：拆分布局版本常量并实现双 decoder。** 使用以下常量，任何 `saveState/restoreState` 调用只传 Qt state version：

  ```cpp
  constexpr quint16 zzWorkspaceEnvelopeVersion = 2;
  constexpr quint16 zzLegacyWorkspaceEnvelopeVersion = 1;
  constexpr int zzQtMainWindowStateVersion = 1;
  constexpr auto zzLayoutStreamVersion = QDataStream::Qt_6_8;
  ```

  `zzDecodeLayout()` 只校验 envelope 并返回 schema；`zzReadVersionOnePayload()` 填充迁移后的 DTO；`zzReadVersionTwoPayload()` 读取 left/right visible IDs 与 sizes、side entries、SplitWorkspace blob、Bottom 状态和 title mode。读取阶段不得调用 QWidget。

- [x] **步骤 6：实现完整反向回滚。** 提交前捕获 Qt Dock state、左右 PanelStack、SplitWorkspace、BottomPane 和标题快照；按 Qt Dock、Split、Side、Bottom、Activity/Title 顺序提交，失败按反向顺序恢复。若任一回滚步骤失败，返回 `InvalidState` 且技术消息包含 `rollback failed`。

- [x] **步骤 7：运行 WorkspaceShell 完整测试。**

  ```bash
  ctest --preset linux-gcc-debug -R '^puretools.workspace-shell$' --output-on-failure
  ```

  预期：v1 迁移、v2 round trip、跨侧迁移、同步销毁和回滚失败路径通过；再次保存只写 schema 2。

- [x] **步骤 8：提交。**

  ```bash
  git add ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.h ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp ZzPureTools/tests/ZzWorkspaceShellTest.cpp
  git diff --cached --check
  git commit -m "框架：升级工作区布局与面板迁移事务" -m "接通 Activity 入口同组重排、跨组和跨侧迁移，并对父对象、显隐、顺序和尺寸执行失败回滚。\n\n拆分外层 schema 与 Qt Dock state 版本，支持版本 1 迁移和有界版本 2 布局恢复。"
  ```

## 里程碑二：通用 Fluent 控件

### 任务 10：实现 ZzCommandBar 自适应 QAction 命令栏

**文件：**
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzCommandBar.h`
- 创建：`ZzFluentUI/widgets/src/ZzCommandBar.cpp`
- 创建：`ZzFluentUI/widgets/src/private/ZzCommandBarPrivate.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzCommandBarPrivate.cpp`
- 创建：`ZzFluentUI/tests/ZzCommandBarTest.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [x] **步骤 1：写 QAction 身份、所有权和 overflow 失败测试。**

  ```cpp
  void keepsOneActionIdentityAcrossOverflow()
  {
      ZzFluentUI::ZzCommandBar bar;
      QAction build(QStringLiteral("Build"), nullptr);
      build.setCheckable(true);
      QAction deploy(QStringLiteral("Deploy"), nullptr);
      QVERIFY(bar.insertPrimaryAction(0, &build));
      QVERIFY(bar.insertSecondaryAction(0, &deploy));
      bar.resize(120, 40);
      bar.show();
      QCoreApplication::processEvents();
      QVERIFY(bar.primaryActions().contains(&build));
      QVERIFY(bar.secondaryActions().contains(&deploy));
      build.trigger();
      QVERIFY(build.isChecked());
      QCOMPARE(build.parent(), nullptr);
  }
  ```

  增加同一 QAction 重复/跨组插入拒绝、便利重载 action 由 CommandBar 拥有、外部 action 销毁清理一次、Auto/Compact/Expanded 阈值、RTL 视觉尾部、checked/enabled/shortcut/menu 在 overflow 中保持、键盘与 accessibleName 测试。

- [x] **步骤 2：运行测试验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzCommandBarTest --parallel 2
  ```

  预期：类和测试目标尚不存在。

- [x] **步骤 3：实现固定组合和非拥有 action 记录。** Private 固定拥有一个不可移动/浮动的 `QToolBar`、一个 `QToolButton`、一个 `QMenu` 和两个 `QPointer<QAction>` 有序列表。便利重载使用 `new QAction(icon, text, q_ptr)`；指针重载不设置父对象。每个 action 只连接一个 destroyed 和一个 changed 观察，移除时断开。

- [x] **步骤 4：实现布局缓存与 overflow。** 缓存每个 action 在 Expanded/Compact 下的逻辑宽度，只在 width/font/style/palette/layoutDirection/action changed 时置 dirty。Auto 依次尝试 Expanded、Compact，再从逻辑尾部移入 overflow；secondary 固定在分隔线后。使用同一 QAction 在 toolbar/menu 间 add/remove，不复制 QAction，不在 `paintEvent` 中计算布局。

  ```cpp
  enum class ZzCommandBarDisplayMode : std::uint8_t {
      Auto, Compact, Expanded
  };

  void ZzCommandBarPrivate::rebuildPresentation()
  {
      const ZzPresentation next = calculatePresentation(q_ptr->width());
      applyToolButtonStyle(next.compact);
      moveActionsWithoutCloning(next.visiblePrimaryCount);
      updateMoreButton(next.hasOverflow);
  }
  ```

- [x] **步骤 5：验证定向和对象预算。**

  ```bash
  ctest --preset linux-gcc-debug -R '^fluent.command-bar$' --output-on-failure
  ```

  预期：1000 次宽度切换后 QAction 数、QMenu 数和自定义 QObject 观察连接稳定；触发始终命中原 QAction。

- [x] **步骤 6：提交。**

  ```bash
  git add \
    ZzFluentUI/CMakeLists.txt \
    ZzFluentUI/tests/CMakeLists.txt \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzCommandBar.h \
    ZzFluentUI/widgets/src/ZzCommandBar.cpp \
    ZzFluentUI/widgets/src/private/ZzCommandBarPrivate.h \
    ZzFluentUI/widgets/src/private/ZzCommandBarPrivate.cpp \
    ZzFluentUI/tests/ZzCommandBarTest.cpp
  git diff --cached --check
  git commit -m "组件：新增自适应 Fluent 命令栏" -m "实现 QAction 主次分组、三种展示模式和按宽度迁移的更多菜单。\n\n保持 QAction 单一身份与外部所有权，并验证 RTL、键盘、无障碍和重复布局对象稳定性。"
  ```

### 任务 11：实现 ZzAnnotatedScrollBar 模型标记滚动条

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzScrollBar.h`
- 创建：`ZzFluentUI/widgets/include/ZzFluentUI/ZzAnnotatedScrollBar.h`
- 创建：`ZzFluentUI/widgets/src/ZzAnnotatedScrollBar.cpp`
- 创建：`ZzFluentUI/widgets/src/private/ZzAnnotatedScrollBarPrivate.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzAnnotatedScrollBarPrivate.cpp`
- 创建：`ZzFluentUI/tests/ZzAnnotatedScrollBarTest.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`
- 修改：`ZzFluentUI/tests/CMakeLists.txt`

- [x] **步骤 1：写模型生命周期、碰撞和交互失败测试。** 使用 `QStandardItemModel` 设置 Position/Kind/Priority，覆盖 reset、insert/remove/dataChanged、模型销毁、NaN/Inf/越界忽略、水平/垂直、RTL、tooltip 和点击：

  ```cpp
  bar.setRange(0, 1000);
  bar.setMarkerModel(&model);
  bar.setMarkersInteractive(true);
  const QModelIndex expected = model.index(0, 0);
  const QPoint hit = markerPointFor(bar, 0.75);
  QCOMPARE(bar.markerAt(hit), expected);
  QSignalSpy activated(&bar, &ZzFluentUI::ZzAnnotatedScrollBar::markerActivated);
  QTest::mouseClick(&bar, Qt::LeftButton, Qt::NoModifier, hit);
  QCOMPARE(activated.count(), 1);
  QVERIFY(qAbs(bar.value() - 750) <= 1);
  ```

  再构造 20 和 100000 标记，断言 QObject/QTimer/animation 数相同，稳定 paint 访问的 bucket 数不超过滚动条可用像素数。

- [x] **步骤 2：运行测试验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzAnnotatedScrollBarTest --parallel 2
  ```

  预期：`ZzScrollBar final` 无法继承，Annotated 类不存在。

- [x] **步骤 3：开放继承并实现规范化缓存。** 只移除 `ZzScrollBar` 的 `final`，不改变其构造、动画或 PIMPL。Annotated Private 保存非拥有 `QPointer<QAbstractItemModel>` 和轻量记录：

  ```cpp
  struct ZzMarker final
  {
      QPersistentModelIndex source;
      qreal position = 0.0;
      ZzScrollMarkerKind kind = ZzScrollMarkerKind::Information;
      QColor color;
      int priority = 0;
  };

  struct ZzPixelBucket final
  {
      int pixel = 0;
      qsizetype markerIndex = -1;
  };
  ```

  模型变化时 O(n) 重建 marker cache；尺寸、方向或 marker cache 变化时重建像素桶。paint 先调用 `ZzScrollBar::paintEvent()`，然后只遍历桶；不得在 paint 中读取整个模型或创建 QObject。

- [x] **步骤 4：实现点击、tooltip 和颜色合同。** 命中 marker 时按归一化 position 映射到 range，设置 value 后发源索引；未命中调用基类鼠标事件。`event(QEvent::ToolTip)` 读取缓存源索引的 `Qt::ToolTipRole`。内置 kind 使用主题语义色；Custom 颜色若与 Surface 对比不足则回退 Information。

- [x] **步骤 5：验证标记规模与原生滚动行为。**

  ```bash
  ctest --preset linux-gcc-debug -R '^(fluent.annotated-scroll-bar|fluent.scroll-controls)$' --output-on-failure
  ```

  预期：原 ZzScrollBar 行为无回归；0/1/100000 标记、模型销毁和未命中轨道操作通过。

- [x] **步骤 6：提交。**

  ```bash
  git add \
    ZzFluentUI/CMakeLists.txt \
    ZzFluentUI/tests/CMakeLists.txt \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzScrollBar.h \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzAnnotatedScrollBar.h \
    ZzFluentUI/widgets/src/ZzAnnotatedScrollBar.cpp \
    ZzFluentUI/widgets/src/private/ZzAnnotatedScrollBarPrivate.h \
    ZzFluentUI/widgets/src/private/ZzAnnotatedScrollBarPrivate.cpp \
    ZzFluentUI/tests/ZzAnnotatedScrollBarTest.cpp
  git diff --cached --check
  git commit -m "组件：新增模型驱动的标记滚动条" -m "在保留 ZzScrollBar 原生输入与单动画合同的基础上实现语义标记、像素桶缓存和可选点击。\n\n覆盖十万标记、非法位置、碰撞优先级、模型生命周期和固定 QObject 预算。"
  ```

### 任务 12：补强 SplitButton 可切换合同与 Pivot 图标项

**文件：**
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitButton.h`
- 修改：`ZzFluentUI/widgets/src/ZzSplitButton.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitButtonPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzSplitButtonPrivate.cpp`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzPivot.h`
- 修改：`ZzFluentUI/widgets/src/ZzPivot.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzPivotPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzPivotPrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzSplitButtonTest.cpp`
- 修改：`ZzFluentUI/tests/ZzPivotTest.cpp`

- [x] **步骤 1：写 SplitButton 主区切换与菜单区隔离测试。**

  ```cpp
  button.setCheckable(true);
  QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, mainPoint);
  QVERIFY(button.isChecked());
  QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, menuPoint);
  QVERIFY(button.isChecked());
  QTest::keyClick(&button, Qt::Key_Space);
  QVERIFY(!button.isChecked());
  QTest::keyClick(&button, Qt::Key_Down, Qt::AltModifier);
  QVERIFY(!button.isChecked());
  ```

  对 Standard/Accent/Subtle、disabled、RTL、Enter/Return、外部菜单销毁重复上述合同；断言只使用 QPushButton 的 `toggled(bool)`，不新增重复 checked 信号。

- [x] **步骤 2：写 Pivot 图标项和指示条安全区域测试。**

  ```cpp
  const QIcon icon = style.standardIcon(QStyle::SP_ComputerIcon);
  QCOMPARE(pivot.addItem(icon, QStringLiteral("Sessions")), 0);
  QCOMPARE(pivot.itemIcon(0).cacheKey(), icon.cacheKey());
  pivot.setItemIcon(0, QIcon());
  QVERIFY(pivot.itemIcon(0).isNull());
  ```

  在 LTR/RTL、长文字、icon-only、overflow、DPR 1.25/1.5/2.0 下渲染，断言指示条像素只位于 tab 底部 gutter，不进入 `QStyle::SE_TabBarTabText` 与图标区域。

- [x] **步骤 3：运行测试确认失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzSplitButtonTest ZzPivotTest --parallel 2
  ```

  预期：Pivot 图标便利 API 缺失；SplitButton 的新增边界断言暴露原生状态机或绘制差异。

- [x] **步骤 4：实现最小合同。** SplitButton 主区仍把 press/release/Space/Enter 交给 `QPushButton`，菜单区始终拦截并只调用 `showMenu()`；绘制时用 `QStyle::State_On` 表示 checked，但 checked 不等同于 Accent appearance。Pivot 便利 API直接委托 `addTab/insertTab/tabIcon/setTabIcon`；`targetIndicatorRect()` 使用 style 提供的 label/text 子区域和底部 gutter 计算，不再只按文字中心估算。

- [x] **步骤 5：运行回归测试。**

  ```bash
  ctest --preset linux-gcc-debug -R '^(fluent.split-button|fluent.pivot|fluent.bottom-pane)$' --output-on-failure
  ```

  预期：鼠标与键盘切换只发生在主区；菜单操作不改变 checked；图标、文字和指示条在四个 DPR 下不重叠。

- [x] **步骤 6：提交。**

  ```bash
  git add \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitButton.h \
    ZzFluentUI/widgets/src/ZzSplitButton.cpp \
    ZzFluentUI/widgets/src/private/ZzSplitButtonPrivate.h \
    ZzFluentUI/widgets/src/private/ZzSplitButtonPrivate.cpp \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzPivot.h \
    ZzFluentUI/widgets/src/ZzPivot.cpp \
    ZzFluentUI/widgets/src/private/ZzPivotPrivate.h \
    ZzFluentUI/widgets/src/private/ZzPivotPrivate.cpp \
    ZzFluentUI/tests/ZzSplitButtonTest.cpp \
    ZzFluentUI/tests/ZzPivotTest.cpp
  git diff --cached --check
  git commit -m "组件：补强分割按钮与图标枢轴合同" -m "让 SplitButton 的主区完整遵循 QPushButton 可切换状态机，并确保菜单区只打开菜单。\n\n为 Pivot 增加图标项便利 API，收紧多方向、多 DPI 下的指示条安全区域。"
  ```

## 里程碑三：消费、示例与质量收口

### 任务 13：补齐 CMake 安装消费、公共头和架构门禁

**文件：**
- 修改：`tests/InstallConsumer/Gui/main.cpp`
- 修改：`tests/InstallConsumer/CMakeLists.txt`
- 修改：`tests/PublicHeaderConsumer/CMakeLists.txt`
- 修改：`tests/Architecture/CheckZzWorkspaceBoundaries.cmake`
- 修改：`tests/Architecture/ZzArchitectureAudit.cmake`
- 修改：`tests/Architecture/ZzFluentVisualTokenContract.cmake`
- 修改：`tests/Platform/PresetMatrixContract.cmake`

- [x] **步骤 1：先扩展安装消费源码。** 仅包含安装后的公开头并创建一个最小工作台：两个同侧面板、两个 tab group、一个 Bottom 工具、CommandBar 和 AnnotatedScrollBar。关键消费代码必须使用公开 API：

  ```cpp
  ZzFluentUI::ZzCommandBar commandBar;
  commandBar.addPrimaryAction(QIcon(), QStringLiteral("Connect"));
  ZzFluentUI::ZzAnnotatedScrollBar markers(Qt::Vertical);
  markers.setMarkerModel(&markerModel);
  auto secondGroup = workspaceShell->splitWorkspace()->splitGroup(
      workspaceShell->splitWorkspace()->activeGroupId(),
      Qt::Horizontal,
      ZzFluentUI::ZzSplitPlacement::After);
  if (!secondGroup.has_value()) {
      return 41;
  }
  ```

- [x] **步骤 2：运行 shared 安装消费并确认未接线时失败。**

  ```bash
  ctest --preset linux-gcc-debug -R '^install.consumer$' --output-on-failure
  ```

  预期：若公共头、导出源或传递依赖有遗漏，安装消费在编译或链接阶段失败。

- [x] **步骤 3：扩展架构规则。** 将 `QScrollBar` 加入公开 QWidget PIMPL 检查；在 `ZzArchitectureAudit.cmake` 添加新增公开头存在性清单后再运行递归扫描，并禁止 SSH/SFTP/network/domain 类型、Qt Private、stylesheet、链式命名空间和无 token 尺寸。视觉测试 fixture 增加使用新 `ZzMetricToken` 的合法样例，禁止为新增文件加入 allowlist；Preset 合同补充断言 Windows MSVC/MinGW 基础 preset 始终启用测试，使安装消费在这些工具链上保持可编译。

- [x] **步骤 4：验证 shared/static 包和跨平台静态合同。**

  ```bash
  ctest --preset linux-gcc-debug -R '^(architecture\.|install\.consumer$)' --output-on-failure
  cmake --preset linux-static-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-static-release --parallel 2
  ctest --preset linux-static-release -R '^(architecture\.|install\.consumer$)' --output-on-failure
  ```

  预期：shared/static 的公共头和安装消费均通过；Preset 合同仍包含 Windows MSVC/MinGW 与 macOS 四组既有 preset，不新增平台私有代码。

- [x] **步骤 5：提交。**

  ```bash
  git add \
    tests/InstallConsumer/Gui/main.cpp \
    tests/InstallConsumer/CMakeLists.txt \
    tests/PublicHeaderConsumer/CMakeLists.txt \
    tests/Architecture/CheckZzWorkspaceBoundaries.cmake \
    tests/Architecture/ZzArchitectureAudit.cmake \
    tests/Architecture/ZzFluentVisualTokenContract.cmake \
    tests/Platform/PresetMatrixContract.cmake
  git diff --cached --check
  git commit -m "质量：补齐工作台安装消费与架构门禁" -m "使用安装后的公开 API 消费分屏、多面板、底部工具、命令栏和标记滚动条。\n\n扩展 PIMPL、业务依赖、视觉令牌和跨平台 preset 静态合同，并同时验证 shared/static 包。"
  ```

### 任务 14：升级 ZzPureToolsExample 与截图场景

**文件：**
- 修改：`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.h`
- 修改：`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.h`
- 修改：`examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/CMakeLists.txt`
- 修改：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceScreenshotTest.cpp`
- 修改：`ZzPureTools/tests/CMakeLists.txt`

- [x] **步骤 1：先把 Example smoke 改成目标产品流程。** 使用公开 `ZzWorkspaceShell` 注册左侧 Sessions/Files、右侧 Properties/Tasks、Bottom 的 Terminal/Problems/Output；创建两个 tab group，并放入终端和 SFTP 展示页。测试必须断言：单击 Activity 即显隐、同侧两个面板同时打开、标签四边分屏、Bottom 切换、CommandBar QAction 和布局 round trip。

  ```cpp
  QCOMPARE(shell->sidePane(ZzFluentUI::ZzSidePaneEdge::Left)
               ->visibleWidgets().size(), 2);
  QVERIFY(shell->splitWorkspace()->moveTabToDropZone(
      firstGroup, 0, firstGroup, ZzFluentUI::ZzWorkspaceDropZone::Right));
  QCOMPARE(shell->splitWorkspace()->groupIds().size(), 2);
  QCOMPARE(shell->bottomPane()->currentWidget(), terminalTool);
  ```

- [x] **步骤 2：运行 smoke 验证失败。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzExampleWorkspaceSmokeTest --parallel 2
  ctest --preset linux-gcc-debug -R '^example.workspace-smoke$' --output-on-failure
  ```

  预期：Example 尚未注册完整多面板/分屏/Bottom/CommandBar 场景，断言失败。

- [x] **步骤 3：只用公开组件升级 Example。** `ZzExampleWindowShellPrivate` 仍只保存业务 QAction、模型和观察值；不得创建 QSplitter、重排 Activity model 或自行编码布局。`ZzExampleWorkspaceContent` 只增加纯本地展示页和标记模型数据，不包含连接、凭据、网络或传输逻辑。将原 Activity Dock 内容迁入 BottomPane，DockPanel 继续展示真正需要浮动的工具。

- [x] **步骤 4：扩展截图矩阵。** 在现有 100/125/150/200 DPR 基线中增加双侧多面板、两组/四组分屏、Bottom 展开/折叠、CommandBar 三模式和 AnnotatedScrollBar 语义标记。每个场景分别运行 Light、Dark、HighContrast；截图用固定窗口尺寸和 Reduced Motion，禁止依赖本机字体以外的临时资源。五区 overlay 无法在 offscreen 下经公开输入稳定保留可见态；按用户裁定不新增测试 API、不引用 private 实现，其真实拖放与截图证据转入任务 15 Linux 物理桌面验收。

- [x] **步骤 5：运行 Example 与截图测试。**

  ```bash
  cmake --build --preset linux-gcc-debug --target ZzPureToolsExample ZzExampleWorkspaceSmokeTest ZzWorkspaceScreenshotTest --parallel 2
  ctest --preset linux-gcc-debug -R '^(example.workspace-smoke|puretools.workspace-screenshot-)' --output-on-failure
  ```

  预期：Example smoke 通过；截图报告没有空白、裁切、指示条重叠或主题裸色差异。只有在人工检查差异后才更新受版本控制的基线。

- [x] **步骤 6：提交。**

  ```bash
  git add \
    examples/ZzPureToolsExample/ZzExampleWorkspaceContent.h \
    examples/ZzPureToolsExample/ZzExampleWorkspaceContent.cpp \
    examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.h \
    examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp \
    examples/ZzPureToolsExample/CMakeLists.txt \
    examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp \
    ZzPureTools/tests/ZzWorkspaceScreenshotTest.cpp \
    ZzPureTools/tests/CMakeLists.txt
  git add -u ZzPureTools/tests/baselines/linux
  git diff --cached --check
  git commit -m "示例：串联高密度 IDE 工作区组件" -m "用公开 WorkspaceShell API 组合双侧多面板、标签分屏、中央底部工具、命令栏和标记滚动条。\n\n扩展 Example smoke 与多主题、多 DPI 截图证据，Example 不再承担工作台协调算法。"
  ```

### 任务 15：收紧性能预算并完成 Linux 全门禁

**文件：**
- 修改：`benchmarks/ZzWorkspaceComponentsBenchmark.cpp`
- 修改：`tests/Platform/ZzPerformanceThresholdContract.cmake`
- 修改：`docs/performance/PERFORMANCE_BASELINE_ZH.md`
- 修改：`docs/development/PLATFORM_SUPPORT_ZH.md`
- 修改：`docs/development/BUILDING_ZH.md`
- 修改：`docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`
- 修改：`docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md`
- 修改：`docs/release/MANUAL_MACOS_CHECKLIST_ZH.md`

- [ ] **步骤 1：先扩展 benchmark 结构断言。** 场景固定创建 32 个可见侧面板、4 和 32 个 tab group、Bottom 三个工具、40 个 CommandBar action，以及 20/100000 标记两个模型。记录：

  ```cpp
  reporter.addSample({QStringLiteral("panel-toggle-time"), "ms", elapsed});
  reporter.addSample({QStringLiteral("group-structure-time"), "ms", elapsed});
  reporter.addSample({QStringLiteral("workspace-paint-4-groups-time"), "ms", fourGroupElapsed});
  reporter.addSample({QStringLiteral("workspace-paint-32-groups-time"), "ms", thirtyTwoGroupElapsed});
  reporter.addSample({QStringLiteral("workspace-render-time"), "ms", renderElapsed});
  reporter.addSample({QStringLiteral("marker-paint-20-time"), "ms", smallElapsed});
  reporter.addSample({QStringLiteral("marker-paint-100000-time"), "ms", largeElapsed});
  reporter.addSample({QStringLiteral("object-count"), "count", objectCount});
  ```

  在进程内直接失败：render P95 > 12 ms、结构操作 P95 > 16.7 ms、32 组相对 4 组的稳定 paint 比值 > 2.0、100000 标记相对 20 标记的稳定 paint 比值 > 2.0、1000 次显隐/分割/合并/主题切换后对象/timer/animation 不回稳、隐藏页仍有运行中 animation/timer。

- [ ] **步骤 2：运行普通 benchmark，定位而不是放宽阈值。**

  ```bash
  cmake --preset linux-gcc-benchmarks -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-gcc-benchmarks --target ZzWorkspaceComponentsBenchmark --parallel 2
  ctest --preset linux-gcc-benchmarks -R '^benchmark.workspace-components$' --output-on-failure
  ```

  预期：生成 `build/linux-gcc-benchmarks/reports/benchmark.workspace-components.json`，所有正式预算通过。若失败，只优化热路径，不改大阈值。

- [ ] **步骤 3：连续三轮参考机采集。**

  ```bash
  cmake --preset linux-gcc-reference -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-gcc-reference --parallel 2
  ctest --preset linux-gcc-reference -R '^benchmark.workspace-components$' --output-on-failure
  scripts/ci/run-linux-gates.sh
  ```

  运行三轮并保存 P50/P95/max、QObject、timer、animation、RSS、Qt/编译器/CPU/GPU/commit/preset 指纹。`ZzPerformanceThresholdContract.cmake` 必须验证新 metric 存在、三轮齐全和规模比阈值。

- [ ] **步骤 4：运行 Linux 编译与测试矩阵。**

  ```bash
  cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-gcc-debug --parallel 2
  ctest --preset linux-gcc-debug --output-on-failure

  cmake --preset linux-gcc-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-gcc-release --parallel 2
  ctest --preset linux-gcc-release --output-on-failure

  cmake --preset linux-static-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-static-release --parallel 2
  ctest --preset linux-static-release --output-on-failure

  cmake --preset linux-clang-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-clang-release --parallel 2
  ctest --preset linux-clang-release --output-on-failure

  cmake --preset linux-clang-asan -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-clang-asan --parallel 2
  ctest --preset linux-clang-asan --output-on-failure

  cmake --preset linux-clang-tidy-release -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-clang-tidy-release --parallel 2
  cmake --build --preset linux-clang-tidy-release --target ZzClangTidy
  ```

  预期：GCC shared/release/static、Clang、ASan/UBSan 和 clang-tidy 全部通过。命令失败时保存真实日志，不跳过测试、不删除构建证据。

- [ ] **步骤 5：完成 Linux 物理桌面人工验收并如实记录其他平台。** Linux 检查鼠标拖放、键盘、IME、DPR、主题、标题栏、静态 Example 启动和布局重启恢复；必须通过真实 tab 拖拽显示五区 overlay，并保存其可见态截图证据。Windows 文档只记录 MSVC/MinGW 静态合同以及“物理机待验证”；macOS 只记录 AppleClang 公共 Qt API 静态合同以及“运行待验证”。

- [ ] **步骤 6：提交最终证据。**

  ```bash
  git add \
    benchmarks/ZzWorkspaceComponentsBenchmark.cpp \
    tests/Platform/ZzPerformanceThresholdContract.cmake \
    docs/performance/PERFORMANCE_BASELINE_ZH.md \
    docs/development/PLATFORM_SUPPORT_ZH.md \
    docs/development/BUILDING_ZH.md \
    docs/release/MANUAL_LINUX_CHECKLIST_ZH.md \
    docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md \
    docs/release/MANUAL_MACOS_CHECKLIST_ZH.md
  git diff --cached --check
  git commit -m "质量：收口 IDE 工作台性能与平台证据" -m "扩展工作区结构、渲染、标记规模和对象稳定性性能门禁，并记录三轮参考机数据。\n\n完成 Linux 全矩阵与物理桌面验收，准确保留 Windows MSVC/MinGW 和 macOS 的待验证边界。"
  ```

## 最终自检

### 规格覆盖度

- `ZzPanelStack` 所有权、显隐、尺寸和对象预算：任务 2。
- SidePane Stacked 与 ActivityBar 多 active、move 意图：任务 3、任务 9。
- SplitWorkspace 树、五区拖放、页面键和事务布局：任务 4、任务 5、任务 6。
- BottomPane 与 DockPanel 职责分离：任务 7、任务 8、任务 14。
- WorkspaceShell 三类 PanelId、活动组标题、v1 到 v2 迁移和回滚：任务 8、任务 9。
- CommandBar 主次 QAction、展示模式和 overflow：任务 10。
- AnnotatedScrollBar 模型、像素桶、交互和十万标记复杂度：任务 11、任务 15。
- SplitButton checkable 与 Pivot 图标项：任务 12。
- 视觉令牌、Light/Dark/HighContrast、DPR、Reduced Motion：任务 1、任务 12、任务 14。
- 安装消费、公共头、架构、Example、截图、性能和跨平台证据：任务 13 至任务 15。

### 类型一致性

- 标签组始终使用 `ZzTabGroupId`，活动组访问器始终为 `activeGroupId()`。
- 标签分割方向使用 `Qt::Orientation`，前后位置使用 `ZzSplitPlacement`，拖放使用 `ZzWorkspaceDropZone`。
- SidePane 模式始终为 `ZzSidePaneMode::Single/Stacked`；可见集合为 `QList<QWidget *>`。
- 标记模型始终使用 `ZzScrollMarkerRole` 和 `ZzScrollMarkerKind`；公开信号返回源 `QModelIndex`。
- 外层工作区版本始终为 2，`QMainWindow::saveState/restoreState` 版本始终为 1。
- `tabWidget()` 始终表示当前 active group 的兼容入口，不保存为长期固定组指针。

### 禁止项扫描

执行以下命令，结果中不得出现计划占位符、链式命名空间、Qt Private 或新增视觉债务：

```bash
rg -n 'TO[D]O|待[定]|后续实[现]|类似任[务]|适当的错误处[理]' docs/superpowers/plans/2026-08-22-ide-workbench-product-expansion.md
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Architecture/RunArchitectureChecks.cmake
ctest --preset linux-gcc-debug -R '^(architecture.boundaries|architecture.complete-audit|architecture.fluent-visual-token-contract)$' --output-on-failure
```

第一条命令预期无输出；其余命令预期通过。

## 完成定义

1. 所有 15 个任务分别有失败测试证据、通过证据和独立中文 commit。
2. Example 没有 QSplitter、Activity 重排、布局 envelope 或跨组件回滚算法。
3. shared/static 安装包可仅通过公开头消费完整 Workbench。
4. Linux 全矩阵、截图、架构、安装消费和三轮参考机性能均通过。
5. Windows MSVC/MinGW 与 macOS 的真实验证边界记录准确，没有用静态合同冒充运行通过。
6. 工作区只剩用户已有的 `temp_image/` 或其他与本计划无关的用户改动。
