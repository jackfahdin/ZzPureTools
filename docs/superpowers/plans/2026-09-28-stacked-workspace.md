# 中央堆叠页面实现计划

**目标：** 默认中央区域使用无标签堆叠页面，由应用导航切换；应用可以在页面中自行嵌入 ZzTabWidget 或 ZzSplitWorkspace。

**架构：** ZzWorkspaceShell 默认创建 QStackedWidget，并公开页面容器。已有多标签工作区改为显式选择，保留独立组件和它们的迁移事务。组件页面宿主直接放入中央堆叠容器，不再包装成固定标签。Example 的终端入口改用堆叠页面并提供页面切换入口。

**技术栈：** Qt 6.8+、C++20；本机 Qt 6.11.1 / GCC 15。

## 实现与验证

- [x] Shell：在 ZzWorkspaceShell.h/.cpp、ZzWorkspaceShellPrivate.h/.cpp 增加中央容器类型与 stackWidget()；默认堆叠，显式 Tabbed 才创建标签组件。先在 ZzWorkspaceShellTest 增加默认无 QTabBar 的失败测试，再实现页面切换、标题跟随与自嵌标签的验证。
- [x] 导航与窗口：在 ZzWorkspaceNavigationIntegrationTransactionPrivate.cpp 让现有迁入/回滚流程支持堆叠容器；同一路由重新激活也返回组件页面。独立窗口配置和关闭继续有效；标签迁移只用于显式标签工作区。原多标签测试显式选择 Tabbed，不削减原事务覆盖。
- [x] Example：调整 ZzExampleWindowShell 系列的默认接入，移除固定组件标签和自动创建的终端标签。保留新建/关闭终端动作，用菜单提供已打开页面入口；组件导航、窗口创建和设置窗口保持可访问。
- [x] 验证：构建 linux-gcc-debug 的 Example 与工作区相关测试；运行导航、页面生命周期、Shell、窗口协调和 Example 集成测试，检查实际截图后更新受影响的 Example 基线。记录结果、更新使用文档并中文提交，不推送。

## 边界

- 不引入整页截图动画，不改业务模型与页面生命周期策略。
- 无标签堆叠模式不提供标签拖拽/标签拓扑持久化；应用自行嵌入标签组件时由该组件管理。
- 模式在创建 Shell 时确定，避免运行中搬运页面与切换所有权。
- 不修改参考项目；不提交已有 docs/research/ 内容。

## 交付记录

- 已实现默认 Stacked、显式 Tabbed；默认不创建隐藏的标签栏或分屏对象。
- 已验证页面切换、标题同步、页面内自嵌标签、侧栏布局保存恢复、跨模式布局拒绝。
- 导航接入正常路径同时运行 Stacked / Tabbed；新增堆叠页面迁入被打断后的回滚及重试。
- 新增堆叠窗口 Deny / Delegate / Allow 关闭与通知期间重入验证。
- Example 默认无外层标签；终端创建、菜单切页、同路由重新激活、关闭回退、多窗口均有集成验证。
- Linux Qt 6.11.1 / GCC 15：Debug Example 构建成功，91 项定向 CTest 全部通过。
- Example 12 张截图已更新；关闭更新开关后四档 DPR 比较通过。检查了 100% 三主题与 200% Light。
- 原工作区四档 DPR 截图保持原基线并通过；未放宽截图或性能阈值。
- 完成独立只读审查；本轮未执行 Windows / macOS 实机测试、全性能矩阵或远端 CI。

运行验证：

```bash
ctest --test-dir build/linux-gcc-debug --output-on-failure \
  -R '^(puretools\.(workspace-shell|workspace-public-api|workspace-navigation-integration|workspace-window-coordinator\..*|workspace-layout-codec-private|workspace-layout-state-private|workspace-screenshot-.*|page-lifecycle|navigation)|example\.(workspace-smoke|puretools-integration(-english)?|puretools-screenshot-.*)|architecture\.(public-headers|complete-audit|documentation-audit))$' \
  -j 2
```
