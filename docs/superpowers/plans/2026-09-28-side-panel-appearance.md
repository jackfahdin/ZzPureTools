# 侧面板内容统一实现计划

**目标：** 会话、文件、组件导航共用库级树视图外观；属性、任务共用表单边距，左右侧一致。

**架构：** `ZzSidePane` 保持容器与宽度职责；新增无状态 `ZzSidePanelAppearance` 负责显式配置内容外观。
模型、表头、展开及页面跳转仍由调用方决定。文件示例使用独立模型，通过工厂注入视图。
用户已批准本方案并要求直接实施，在当前工作树完成代码与验证后中文提交，不推送。

**技术栈：** Qt 6.8+、C++20；本机 Qt 6.11.1、GCC 15。

## 文件与实施步骤

- [x] 在 `examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp` 添加回归：
  会话树占满其宿主宽度、文件树只读、三个视图同一条目高度，调整侧栏宽度后条目跟随；先运行确认失败。
- [x] 新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzSidePanelAppearance.h` 以及
  `widgets/src/ZzSidePanelAppearance.cpp`、`widgets/src/private/ZzSidePanelAppearancePrivate.h/.cpp`。
  `applyTreeView(QTreeView*)` 配置无边框、Window 背景角色、Fluent 委托、整行选择、逐像素滚动及鼠标跟踪；
  保留模型、表头、列宽、根装饰、编辑和展开策略。重复应用复用 Fluent 委托，不增加信号或计时器。
  `applyFormLayout(QLayout*)` 配置统一 12px 内容边距、8px 间距。修改 `ZzFluentUI/CMakeLists.txt` 纳入源码。
- [x] 在 `ZzFluentUI/widgets/src/private/ZzNavigationPanePrivate.cpp` 使用公共入口，保留导航语义。
  在 `ZzFluentUI/tests/ZzSidePaneTest.cpp` 验证重复配置不新增委托，不改变模型、选择及业务策略。
- [x] 新增 `examples/ZzPureToolsExample/ZzExampleFileModel.h/.cpp`，继承 `QStandardItemModel`，
  存放只读演示目录与两列表头。无新增私有状态，不额外分配 Pimpl。
  修改 `ZzExampleWindowShellPrivate.h/.cpp` 在文件面板首次创建时建立并注入模型。
  修改 Example 及其 tests 的 `CMakeLists.txt` 纳入模型。
- [x] 修改 `ZzExampleWorkspaceContent.h/.cpp`：会话包装布局零边距；文件 `QTreeView` 注入模型；
  两者调用 `ZzSidePanelAppearance::applyTreeView`，都只读；属性、任务调用 `applyFormLayout`。
- [x] 编译 Debug Example 和定向测试，检查实际渲染；必要时检查截图差异并更新对应基线。
  独立审查后补充公共入口文档、README，中文提交本轮代码。

## 验证命令与验收

```bash
cmake --build build/linux-gcc-debug --target ZzPureToolsExample \
  ZzExampleWorkspaceSmokeTest ZzSidePaneTest ZzNavigationPaneTest --parallel 2
ctest --test-dir build/linux-gcc-debug --output-on-failure \
  -R '^(fluent\.(side-pane|navigation-pane)|example\.(workspace-smoke|puretools-integration(-english)?|puretools-screenshot-.*)|architecture\.(complete-audit|documentation-audit))$'
```

检查浅色、深色及全局强调色变化；验证侧栏切换、收起、移动及宽度调整。
不改变路由生命周期、工作区事务或中央布局，不新增逐帧扫描，不宣称本次进行了全矩阵性能验证。

## 完成记录

2026-09-28：现有 Qt 6.11.1 / GCC 15 Debug 编译通过，23 项定向 CTest 通过。
包括侧面板、导航、工作区交互与导航接入、公共头和架构审计、Example 中英文冒烟及三组四档截图。
实际检查了会话/文件/组件并排渲染、100% 三主题和 200% Light。
更新 12 张综合 Example 基线及 SHA-256，保持原比较阈值，关闭更新模式后复测通过。
独立审查无阻塞问题；补充空模型和单列模型保护；临时抓图代码已移除。
未运行完整性能矩阵，Windows/macOS 运行验收仍需对应平台。
