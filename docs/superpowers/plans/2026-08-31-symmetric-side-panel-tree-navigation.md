# 左右统一侧栏与 Tree View 组件导航实现计划

> **面向 AI 代理的工作者：** 本计划在当前工作区内执行，不创建 commit；完成后先由用户运行 Example 验收，再决定是否提交。

**目标：** 保留 Activity Bar，为左右两侧提供同类型的统一 Side Panel，并将组件页面入口改为纵向可展开的 Tree View。

**架构：** `ZzWorkspaceShell` 继续拥有左右两个 `ZzSidePane` 和两个 `ZzActivityBar`。应用导航面板继续通过现有工作区集成事务接入左侧，但 `ZzNavigationPane` 的工作区展示改为 Tree View 模式；右侧仍使用同一个 Side Pane 类型，只更换其面板内容。树模型负责把现有扁平导航模型按 Section 分组并保留源索引映射，路由控制器仍只接收源模型索引。

**技术栈：** Qt 6.8+ Widgets、C++20、QAbstractProxyModel、QTreeView、现有 ZzFluentItemDelegate 与 ZzSidePane。

---

### 任务 1：为树导航增加失败测试

**文件：**
- 修改：`ZzFluentUI/tests/ZzNavigationPaneTest.cpp`
- 修改：`ZzFluentUI/tests/ZzNavigationControlsTest.cpp`

- [ ] **步骤 1：编写失败的测试**

新增测试覆盖：

```cpp
void treeModeBuildsExpandableSectionRows()
{
    QStandardItemModel model;
    model.appendRow(zzNavigationItem("Buttons", "Controls"));
    model.appendRow(zzNavigationItem("Inputs", "Controls"));

    ZzFluentUI::ZzNavigationPane pane;
    pane.setTreeMode(true);
    pane.setModel(&model);

    auto *tree = pane.treeView();
    QVERIFY(tree != nullptr);
    QVERIFY(tree->inherits("QTreeView"));
    QCOMPARE(tree->model()->rowCount(), 1);
    QCOMPARE(tree->model()->rowCount(tree->model()->index(0, 0)), 2);
}

void treeModeActivatesLeafOnSingleClick()
{
    QStandardItemModel model;
    model.appendRow(zzNavigationItem("Buttons", "Controls"));
    ZzFluentUI::ZzNavigationPane pane;
    pane.setTreeMode(true);
    pane.setModel(&model);
    pane.resize(280, 320);
    pane.show();
    QCoreApplication::processEvents();

    QSignalSpy spy(&pane, &ZzFluentUI::ZzNavigationPane::navigationRequested);
    auto *tree = pane.treeView();
    const QModelIndex section = tree->model()->index(0, 0);
    const QModelIndex leaf = tree->model()->index(0, 0, section);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton,
                      Qt::NoModifier, tree->visualRect(leaf).center());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).value<QModelIndex>(), model.index(0, 0));
}
```

- [ ] **步骤 2：运行测试验证失败**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzNavigationPaneTest ZzNavigationControlsTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R 'ZzNavigation(Pane|Controls)Test' --output-on-failure
```

预期：编译失败，原因是 `ZzNavigationPane::setTreeMode()` 和 `treeView()` 尚不存在。

### 任务 2：实现导航树模型和 Tree View 模式

**文件：**
- 创建：`ZzFluentUI/widgets/src/private/ZzNavigationTreeModel.h`
- 创建：`ZzFluentUI/widgets/src/private/ZzNavigationTreeModel.cpp`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzNavigationPane.h`
- 修改：`ZzFluentUI/widgets/src/ZzNavigationPane.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzNavigationPanePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzNavigationPanePrivate.cpp`
- 修改：`ZzFluentUI/CMakeLists.txt`

- [ ] **步骤 1：实现最小树模型**

`ZzNavigationTreeModel` 使用 Section 作为一级节点，叶节点保存源模型的 `QPersistentModelIndex`；`mapToSource()` 对一级节点返回无效索引，对叶节点返回源索引；`mapFromSource()` 支持程序化选中；源模型 reset、结构变化和 Section/Placement 变化时重建，普通标题、图标和徽标变化只转发 `dataChanged`。

- [ ] **步骤 2：增加 Tree View 模式公开接口**

在 `ZzNavigationPane` 增加：

```cpp
void setTreeMode(bool enabled);
[[nodiscard]] bool isTreeMode() const noexcept;
[[nodiscard]] QTreeView *treeView() const noexcept;
```

默认值保持 `false`，保证现有公共导航面板行为不变。Tree 模式显示单个纵向 `QTreeView`，关闭原有主区/页脚视图；树节点关闭编辑、横向滚动和双击展开，叶节点单击直接发出 `navigationRequested`。

- [ ] **步骤 3：运行任务 1 测试确认通过**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzNavigationPaneTest ZzNavigationControlsTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R 'ZzNavigation(Pane|Controls)Test' --output-on-failure
```

预期：新增树模式测试和既有导航测试全部通过。

### 任务 3：将应用组件导航切换为 Tree View

**文件：**
- 修改：`ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceNavigationIntegrationTest.cpp`
- 修改：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`

- [ ] **步骤 1：编写集成失败断言**

在应用窗口初始化测试中断言：

```cpp
QVERIFY(window->navigationPane() != nullptr);
QVERIFY(window->navigationPane()->isTreeMode());
QVERIFY(window->navigationPane()->treeView() != nullptr);
```

- [ ] **步骤 2：启用应用导航 Tree View**

在 `ZzApplicationWindowPrivate::initialize()` 创建导航模型后，将导航面板切换到 Tree 模式，并使用 Tree View 的叶节点源索引连接现有 `ZzNavigationController`。不改变路由模型、页面模型或 Shell 的事务接口。

- [ ] **步骤 3：运行集成测试**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzWorkspaceNavigationIntegrationTest ZzExampleWorkspaceSmokeTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '(ZzWorkspaceNavigationIntegrationTest|ZzExampleWorkspaceSmokeTest)' --output-on-failure
```

预期：应用导航由 Tree View 展示，单击叶节点能切换中央页面，Activity Bar 折叠和恢复不改变树的展开状态。

### 任务 4：统一左右侧栏视觉与空侧栏行为

**文件：**
- 修改：`ZzFluentUI/widgets/src/private/ZzPanelStackPrivate.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzSidePanePrivate.cpp`
- 修改：`ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzSidePaneTest.cpp`
- 修改：`ZzPureTools/tests/ZzWorkspaceShellTest.cpp`

- [ ] **步骤 1：为左右宿主写行为测试**

断言左右 Side Pane 使用相同的标题/内容/状态对象命名，右侧指示条位于右边缘；移除右侧最后一个面板后右侧 Activity Bar 与 Side Pane 同时隐藏，重新注册面板后恢复。

- [ ] **步骤 2：实现统一容器合同**

只在公共 `ZzSidePane`/`ZzPanelStack` 中调整容器绘制与边缘方向；面板内容不再创建自己的标题栏、关闭按钮或外层边框。左右方向只影响 resize handle 和 indicator 的物理边缘。

- [ ] **步骤 3：运行侧栏回归测试**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzSidePaneTest ZzWorkspaceShellTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R '(ZzSidePaneTest|ZzWorkspaceShellTest)' --output-on-failure
```

### 任务 5：Example 视觉验收与全量回归

**文件：**
- 修改：`examples/ZzPureToolsExample/ZzExampleWorkspaceContent.cpp`（仅在需要补充 Tree View 样式或测试对象名时）
- 修改：`examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`

- [ ] **步骤 1：构建并运行 Example**

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample --parallel 2
./build/linux-gcc-debug/examples/ZzPureToolsExample/ZzPureToolsExample
```

- [ ] **步骤 2：人工检查**

检查 Activity Bar 点击、Tree View 展开/折叠、单击叶节点跳转、左右侧栏一致性、主题切换、面板收缩和空侧栏隐藏。

- [ ] **步骤 3：运行全量相关测试**

```bash
ctest --test-dir build/linux-gcc-debug --output-on-failure
```

暂不更新截图基线，先以用户人工验收结果为准；未得到确认前不创建 commit。
