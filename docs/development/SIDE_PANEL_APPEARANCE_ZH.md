# 侧面板内容外观

`ZzSidePane` 管理左右容器、宽度、拖动与收起；`ZzSidePanelAppearance` 配置容器里面的内容。
普通面板通过 `ZzWorkspaceShell::registerSidePanelFactory()` 延迟创建；
应用导航通过 `integrateApplicationNavigation()` 接入已有导航和中央页面宿主。
这两个入口共用侧面板容器，导航入口另外负责保留页面跳转关系。

## 树视图

```cpp
#include <ZzFluentUI/ZzSidePanelAppearance.h>
#include <QtWidgets/QTreeView>

auto *tree = new QTreeView(parent);
ZzFluentUI::ZzSidePanelAppearance::applyTreeView(tree);
tree->setModel(model); // 调用方负责模型生命周期。
tree->setHeaderHidden(true); // 表头是否显示由内容决定。
tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
```

公共入口统一无边框、Window 背景角色、Fluent 标准行高（40px）、16px 层级缩进、整行选择、
逐像素滚动和鼠标跟踪；背景与选中色继续随应用主题、强调色变化，不固定写入 palette 颜色。
宿主布局使用零边距，文本的内部留白由 Fluent 条目绘制负责，选择区域跟随视口宽度。

该入口不改变模型、选择模型、当前选择、表头显隐、列宽、编辑策略或展开状态。
它会安装或复用 `ZzFluentItemDelegate`；需要自定义委托时，在调用后安装自己的委托。
默认开启统一行高，包含可变高度条目的自定义委托应另行关闭 `uniformRowHeights`。
重复配置已使用 Fluent 委托的视图不会新增委托、计时器或信号连接。

## 表单与说明内容

```cpp
auto *layout = new QFormLayout(panel);
ZzFluentUI::ZzSidePanelAppearance::applyFormLayout(layout);
```

统一四边 12px 内容边距和 8px 布局间距，适用于属性表单、任务说明等非树内容。
这些规则与面板位于左侧或右侧无关。

## Example 接入

| 内容 | 数据与显示方式 |
| --- | --- |
| 会话 | 外部 `ZzExampleSessionModel` 注入只读树，零外层边距，无表头 |
| 文件 | 首次显示时创建 `ZzExampleFileModel` 并注入只读树；保留名称、类型两列，名称列随面板伸缩 |
| 组件 | `ZzNavigationPane` 的树视图使用相同外观入口，保留路由、分组和双击展开 |
| 属性、任务 | 使用相同表单边距，保留各自内容布局 |

文件模型属于 Example 的演示数据层，不访问文件系统或网络；库不包含会话或文件业务数据。
此次只调整视图装配和配置，继续复用 Qt 的可见条目绘制及现有 Fluent 样式缓存。
