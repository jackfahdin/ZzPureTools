# 中央堆叠页面接入

`ZzWorkspaceShell::create()` 默认创建无标签中央区域。左右 Activity Bar、侧面板、
底部工具面板及命令面板不变。框架不再强制为内容增加“组件示例”之类的外层标签。

## 普通页面

```cpp
#include <QtWidgets/QStackedWidget>
#include <ZzPureTools/ZzWorkspaceShell.h>

auto result = ZzPureTools::ZzWorkspaceShell::create(&window, titleBar);
if (!result) {
    return ZzCore::ZzResult<void>::failure(result.error());
}
workspace_ = std::move(result).value(); // 由应用的窗口装配对象持有
window.setCentralWidget(workspace_->workspaceWidget());
auto *stack = workspace_->stackWidget();
auto *page = new QWidget;
page->setWindowTitle(QStringLiteral("我的页面"));
stack->addWidget(page);
stack->setCurrentWidget(page);
```

页面由 Qt 父子树管理；`removeWidget(page)` 不销毁页面，需要由应用决定删除还是重新挂载。
切页后，当前页面的 `windowTitle` 用于 `CurrentTab` / `CurrentTabAndApplication` 标题策略。
这些策略同样适用于无标签页面。页面不必一次创建齐全，业务内容仍可按原页面生命周期策略延迟创建。

使用路由的应用继续调用 `integrateApplicationNavigation()`：它将原导航树移入侧面板，
将原 `ZzPageHost` 直接接入堆叠容器，复用原模型、控制器、历史和页面实例。重新点击当前
路由也会切回页面宿主，因此可以从其他中央页面返回组件展示。

## 页面内自行使用多标签

```cpp
#include <ZzFluentUI/ZzTabWidget.h>

auto *documents = new ZzFluentUI::ZzTabWidget;
documents->setWindowTitle(QStringLiteral("文档"));
documents->addTab(new QWidget, QStringLiteral("文档一"));
documents->addTab(new QWidget, QStringLiteral("文档二"));
stack->addWidget(documents);
stack->setCurrentWidget(documents);
```

这样只有应用选择的“文档”页面存在标签，外层中央区域仍是堆叠页面。也可以放入独立的
`ZzSplitWorkspace`。框架不会搜索或接管页面内部的标签组件。

若应用明确需要内置标签撕出、跨窗口回收和 `ZZWT` 拓扑恢复，可以在创建 Shell 时选择
`ZzWorkspaceCenterMode::Tabbed`。该模式保留独立测试覆盖。模式创建后不变。

## 窗口与持久化边界

- 两种模式都支持窗口协调器登记、创建窗口、独立配置、Deny / Delegate / Allow 关闭策略。
- Stacked 页面属于当前窗口；关闭窗口时不会将这些页面搬入其他窗口。
- `saveLayout()` / `restoreLayout()` 在 Stacked 模式仍保存侧栏、底栏、停靠与标题策略。
  中央页面与当前路由由应用恢复；布局中只记录稳定的无标签模式标记，不创建隐藏标签控件。
- `saveTopology()` 的标签拓扑能力只适用于 Tabbed。包含 Stacked 窗口时返回 Unsupported，
  不会悄悄丢弃应用页面。标签撕出到 Stacked 窗口也会失败并清理临时窗口。
- 不添加整页截图动画、常驻动画计时器或额外页面副本。

## Example 操作

- 启动后直接显示组件首页，左侧树切换页面。
- “文件 → 新建终端”创建一个无标签终端演示页。
- “视图 → 已打开的页面”切换组件页和已打开的终端；会话面板点击同名会话复用已有页面。
- “文件 → 关闭终端”关闭当前终端并返回组件页，不关闭组件页。
- 组件展示中的 TabWidget / 分屏示例仍可展示其自身控件，它们不是框架外层标签。
