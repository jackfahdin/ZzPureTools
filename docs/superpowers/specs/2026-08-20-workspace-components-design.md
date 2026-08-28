# ZzPureToolsFrame 通用工作区组件设计

> 文档状态：已确认，等待书面规格审查
>
> 确认日期：2026-08-20
>
> 目标分支：`master`
>
> 基线提交：`9abfb070986559e9c534bbf363869ffc36ce85ec`
>
> 主验证平台：Linux、Qt 6.11.1、GCC 15.2.0、C++20

## 1. 文档目的

本文定义下一阶段通用工作区组件的代码级规格。第一使用场景是类似 MobaXterm、
WindTerm 的 SSH 客户端，但所有公开类型必须能够独立用于其他 Qt Widgets 应用。

本文必须能够直接交给另一位开发者或 AI 编写实施计划。实施者不得把 SSH、SFTP、
终端协议、设置持久化或业务命令执行写入 `ZzFluentUI`。如实施中发现需要改变模块
依赖、公开所有权或布局格式，必须先修改本文并重新确认。

本文中的措辞含义如下：

- “必须”“禁止”：验收硬条件；
- “应”：默认实现，偏离时必须记录原因；
- “可”：允许扩展，但不属于本阶段完成条件。

## 2. 已确认目标

### 2.1 产品目标

本阶段必须交付以下可复用组件能力：

1. 应用标题栏：应用图标、自适应菜单、居中标题、主题、置顶和系统窗口按钮；
2. 活动侧栏：左右两侧、主次分组、徽标、选中、排序和跨侧移动；
3. 侧面板：标题、页面堆栈、折叠、恢复宽度和拖拽调整宽度；
4. 资源浏览侧栏：标题、命令区、搜索和虚拟化树视图；
5. 多标签工作区：新建、固定、脏状态、注意状态、关闭规则、转移和拖出；
6. 命令面板：模型驱动搜索、排序、键盘导航和激活意图；
7. 停靠面板：标题栏、关闭、停靠、浮动、重新停靠和内容所有权；
8. 工作区协调器：注册、状态同步、标题同步和版本化布局导入导出；
9. `ZzPureToolsExample` 中的完整 SSH 风格组合场景；
10. 每个组件的单元测试、集成测试、截图和性能观测基准。

Example 只能提供假数据、创建内容 QWidget、注册 QAction 并连接最终业务意图。
上述布局、筛选、拖拽、状态同步和布局恢复行为必须由组件自身或
`ZzWorkspaceShell` 实现。

### 2.2 技术目标

- Qt 6.8+，C++20，CMakeLists.txt 与 CMakePresets.json 配合构建；
- Linux 为主实现和动态验证平台；
- Windows MSVC 2022、Qt SDK MinGW-w64、macOS arm64/x86_64 保持源码和构建边界；
- 使用 Qt 公开 API，不新增一方 Qt Private 头；
- `QMainWindow/QDockWidget/QAbstractItemModel/QMenu/QAction` 的原生协议优先；
- 全部新增导出 QObject/QWidget 使用四文件 PIMPL；
- 所有新增类型和文件以 `Zz` 开头，公开接口使用简体中文 Doxygen；
- 不使用链式 C++ namespace；
- 不改变现有 Qt 6.8+、C++20、严格警告、Sanitizer 和安装消费门禁。

### 2.3 明确非目标

本阶段不实现：

- SSH、Telnet、Mosh、SFTP、串口或终端仿真引擎；
- 终端分屏树和终端进程生命周期；
- 主机凭据、密钥、代理、网络发现或同步；
- 业务设置、数据库、QSettings 读写或云端持久化；
- 自研 Dock 布局引擎；
- 跨进程标签或面板拖拽；
- 默认启用编辑距离等高成本模糊搜索；
- 为标准 Qt 状态机建立空包装类型；
- 直接复制旧版 `ZzAppBar`、`ZzIdeWindow` 或 `ZzDockWidget` 实现。

## 3. 现有代码基线与差距

### 3.1 可直接复用的能力

- `ZzFluentTitleBar` 已有图标、标题、最小化、最大化/还原和关闭意图；
- `ZzTabWidget/ZzTabBar` 已有同容器重排、跨容器转移和拖出意图；
- `ZzNavigationPane` 已建立模型投影、源索引和自适应模式范式；
- `ZzFluentItemDelegate` 已覆盖标准列表、表格和树的 Fluent 视觉；
- `ZzInfoBadge` 已提供有界徽标绘制；
- `ZzIconDescriptor`、字体图标、SVG 图标和缓存体系已经稳定；
- `ZzFluentStyle` 已覆盖 `QMenuBar/QMenu/QToolBar/QDockWidget` 所需标准表面；
- `ZzWindowAgent::configureChrome()` 已支持系统按钮和任意非拖动交互控件；
- `ZzPerformanceReporter` 已提供统一性能报告和 `observe` 标签。

### 3.2 必须回收的 Example 实现泄漏

当前 `ZzExampleWindowShellPrivate` 直接创建 `QToolBar`、`QDockWidget`、活动列表、
尾部跟随状态和窗口命令。这些代码证明标准 Qt 协议可用，但不能继续作为通用工作区
能力的唯一实现。新 Example 完成后，下列行为必须只通过公开组件接口获得：

- 活动栏与侧面板同步；
- Dock 标题栏、显隐、浮动和布局恢复；
- 命令面板搜索与键盘操作；
- 工作区 Tab 的固定、脏状态和关闭规则；
- 当前 Tab 到窗口标题的同步。

活动日志的“位于尾部时自动跟随、手动上翻时暂停”是日志视图策略，不并入本阶段的
通用工作区组件；现有 Example 实现继续保留，未来可按真实复用需求单独立项。

### 3.3 旧版参考边界

旧版 `ZzIdeWindow` 的左右 Activity Bar、Side Panel 和中央内容结构可以作为行为参考。
以下旧实现禁止迁移：

- 全局 `eApp/eTheme/eWinHelper`；
- 控件直接执行置顶、关闭、主题切换和平台 API；
- Windows 消息宏和手写非客户区命中；
- 每次主题切换抓取整窗并创建动画对象；
- 通过字符串动态属性传递拖拽所有权；
- 固定像素、裸色值和每项 QWidget 的 Activity Bar。

## 4. 模块架构

### 4.1 依赖方向

```text
SSH 客户端或其他应用
  |  展示模型、QAction、内容 QWidget、持久化服务
  v
Zz::PureTools
  `- ZzWorkspaceShell：工作区协调、标题策略、布局编解码
       |
       v
Zz::FluentUI
  |- ZzFluentTitleBar
  |- ZzActivityBar / ZzSidePane / ZzExplorerPane
  |- ZzTabWidget / ZzCommandPalette / ZzDockPanel
  `- ZzFluentStyle / ZzIconDescriptor / ZzFluentItemDelegate
       |
       +-> Qt6::Widgets：QMainWindow、QDockWidget、Model/View、QMenu/QAction
       `-> Zz::WindowKit：仅由 Zz::PureTools 的窗口装配层协调
```

`ZzFluentUI` 禁止依赖 `ZzPureTools` 或 `ZzWindowKit`。`ZzWorkspaceShell` 位于
`ZzPureTools/widgets`，允许依赖 `ZzFluentUI`、`ZzWindowKit` 和 Qt Widgets。

### 4.2 选择模块化混合架构的理由

1. Qt 原生 Dock 保留三平台浮动、多屏、键盘和状态恢复协议；
2. Fluent 组件可以脱离 SSH 客户端独立使用；
3. Shell 只协调 UI 状态，不把所有行为重新塞进一个顶层窗口子类；
4. 现有路由型 `ZzApplicationWindow` 不需要被破坏性改造成工作区窗口；
5. 性能热路径继续使用 Model/View 和固定对象，不实现第二套布局引擎。

### 4.3 工作区挂载方式

`ZzWorkspaceShell` 创建一个 `workspaceWidget()`，内部包含左右 Activity Bar、左右
Side Pane、中央 `ZzTabWidget` 和 `ZzCommandPalette`。Shell 不自动调用
`QMainWindow::setCentralWidget()`，原因是宿主可能已经由其他 composition root 管理。

调用方有两种合法挂载方式：

```text
普通 QMainWindow：window.setCentralWidget(shell.workspaceWidget())

ZzApplicationWindow：把 shell.workspaceWidget() 作为一个 ZzPageHost 页面返回
```

两种方式都只做一次公开挂载调用，不在 Example 中实现工作区内部行为。Dock 始终通过
Shell 注册到同一个 `QMainWindow`。应用负责在工作区页面不可见时决定 Dock 是否隐藏。

## 5. 新增和增强的公开类型

### 5.1 ZzFluentUI 类型清单

| 类型 | 处理方式 | 责任 |
|---|---|---|
| `ZzFluentTitleBar` | 增强现有类型 | 自适应菜单、居中标题、主题与置顶意图 |
| `ZzTitleBarMenuDisplayMode` | 新增枚举 | Expanded、Compact 和 Adaptive 菜单策略 |
| `ZzActivityArea` | 新增枚举 | 左/右和主/次四个活动入口区 |
| `ZzActivityItemRole` | 新增枚举 | Activity 模型的 Area 与 Badge 角色 |
| `ZzActivityBar` | 新增四文件 PIMPL | 两组虚拟化入口、选择、键盘和移动意图 |
| `ZzSidePaneEdge` | 新增枚举 | 左右侧面板边缘 |
| `ZzSidePane` | 新增四文件 PIMPL | 标题、页面堆栈、折叠和宽度调整 |
| `ZzExplorerPane` | 新增四文件 PIMPL | 标题、命令、搜索和虚拟化树 |
| `ZzCommandItemRole` | 新增枚举 | 命令关键词、快捷键、分组和优先级角色 |
| `ZzCommandPalette` | 新增四文件 PIMPL | 覆盖式命令搜索、排序、键盘和激活意图 |
| `ZzDockPanel` | 新增四文件 PIMPL | Fluent Dock 标题栏和 Qt 原生停靠协议 |
| `ZzTabWidget/ZzTabBar` | 增强现有类型 | 固定、脏状态、注意状态、新建和关闭规则 |

### 5.2 ZzPureTools 类型清单

| 类型 | 处理方式 | 责任 |
|---|---|---|
| `ZzWorkspacePanelId` | 新增值类型 | 非空、稳定、可哈希的面板 ID |
| `ZzWorkspaceTitleMode` | 新增枚举 | 应用、当前 Tab、组合和自定义标题策略 |
| `ZzWorkspaceShell` | 新增四文件 PIMPL | 组件创建、面板注册、同步和布局编解码 |

所有枚举单独成文件。`ZzWorkspacePanelId` 是小型值类型，不使用 PIMPL；其余新增导出
QObject/QWidget 必须使用公开 `.h/.cpp` 和私有 `.h/.cpp` 四文件结构。

## 6. ZzFluentTitleBar 设计

### 6.1 固定空间结构

```text
[应用图标] [横向菜单或折叠菜单] | [窗口几何中心标题] |
                  [主题] [置顶] [最小化] [最大化] [关闭]
```

标题不与 Tab Bar 合并。当前 Tab 标题由 `ZzWorkspaceShell` 计算后以字符串设置，标题栏
禁止查找 `ZzTabWidget`、页面或业务模型。

### 6.2 菜单

- 标题栏内部固定拥有一个 `QMenuBar`，并强制 `setNativeMenuBar(false)`；
- `menuBar()` 返回非拥有观察指针，应用使用标准 `addMenu()` 和 `QAction` API；
- Adaptive 是默认模式；宽窗口显示横向菜单，窄窗口显示一个折叠菜单按钮；
- 折叠菜单复用每个顶层 `QMenu::menuAction()`，不得复制 QAction 状态；
- 只在 ActionAdded、ActionRemoved、LanguageChange 或布局尺寸变化时重算结构；
- 临界宽度设置 24 个逻辑像素的迟滞区，避免反复切换；
- Compact、Expanded 和 Adaptive 三种显示模式由 `ZzTitleBarMenuDisplayMode` 表示。

### 6.3 居中标题

标题必须相对整个标题栏宽度居中，而不是相对左右布局剩余空间居中。私有布局算法：

1. 分别计算左侧交互区末端和右侧交互区起点；
2. 以标题栏几何中心生成期望标题矩形；
3. 将矩形收敛到左右安全边界；
4. 空间不足时使用 `QFontMetrics::elidedText()`；
5. 标题矩形为空时隐藏文本，不允许覆盖任何按钮。

RTL 时左右区域镜像，但“相对窗口中心”规则不变。标题变化只更新标题 label，不触发
菜单重建。

### 6.4 主题和置顶

- `setThemeMode()` 只同步确认后的显示状态；
- 用户选择 System、Light、Dark 时发出 `themeModeRequested(ZzThemeMode)`；
- 外部设置 HighContrast 时标题栏必须能够正确展示该状态，但本阶段不要求把它加入
  默认三项快捷菜单；应用仍可通过标准菜单提供高对比入口；
- `setAlwaysOnTop()` 只同步确认后的置顶状态；
- 用户点击置顶按钮发出 `alwaysOnTopRequested(bool)`，标题栏不调用窗口 API；
- `ZzWorkspaceShell` 负责将置顶意图应用到宿主，再回写真实状态；
- 主题和置顶按钮必须有 tooltip、accessibleName、checked 状态和键盘焦点。

### 6.5 WindowKit 契约

保留现有 `windowIconWidget()`、`minimizeButton()`、`maximizeButton()` 和
`closeButton()`。新增 `hitTestVisibleWidgets()`，只返回菜单栏、折叠菜单、主题和置顶
等非系统交互控件。系统按钮仍通过 `ZzWindowChromeConfiguration` 的专用字段配置，
禁止重复放入 `interactiveWidgets`。

`ZzApplicationWindowPrivate` 必须把 `hitTestVisibleWidgets()` 写入 Chrome 配置。菜单
模式切换只改变稳定子控件的可见性，因此不需要每次重新配置 WindowKit。

## 7. ZzActivityBar 与 ZzSidePane 设计

### 7.1 Activity 模型契约

`ZzActivityBar` 接收非拥有 `QAbstractItemModel`，只处理 column 0 顶层行：

| 数据角色 | 语义 |
|---|---|
| `Qt::DisplayRole` | tooltip、可访问名称和紧凑回退文本 |
| `Qt::DecorationRole` | QIcon 或可转换图标值 |
| `ZzActivityItemRole::Area` | `ZzActivityArea` 四区域之一 |
| `ZzActivityItemRole::Badge` | 非负整数；0 不显示，大于 99 显示 99+ |

入口是否可用通过 `QAbstractItemModel::flags()` 中的 `Qt::ItemIsEnabled` 判断，不建立
不存在的 enabled 数据角色，也不复制可用状态。

单个 Shell 为左右 Activity Bar 提供同一个内部模型。每个 Bar 只投影本侧 Primary 和
Secondary 两个区域，不复制展示数据。

### 7.2 Activity 交互

- 使用两个固定 `QListView` 和 delegate，不为每个入口创建按钮 QWidget；
- 当前项通过源 `QModelIndex` 同步；
- 点击当前入口发出折叠意图，点击其他入口发出激活意图；
- Up/Down、Home/End、Enter/Space 和上下分组跨越必须确定；
- 拖拽使用进程内带随机令牌的 MIME 载荷，拒绝伪造和跨进程数据；
- 拖动只发出 `moveRequested(sourceIndex, targetArea, targetRow)`；
- Activity Bar 不直接修改外部模型，Shell 在验证 ID 后提交移动；
- 拖拽取消或提交失败时当前模型和页面所有权保持不变；
- RTL 只镜像视觉与指示条，不改变 Left/Right 的物理 Dock 语义。

### 7.3 SidePane

`ZzSidePane` 是独立可复用容器，内部固定包含标题区、`QStackedWidget` 和 4 逻辑像素
拖拽把手。公开能力至少包含：

- `addWidget(QWidget *, QString title)`：接管页面 QObject 所有权；
- `takeWidget(QWidget *)`：从堆栈移除、解除父子关系并归还所有权；
- `setCurrentWidget(QWidget *)`：只接受已注册页面；
- `setCollapsed(bool)`；
- `setMinimumPaneWidth()`、`setMaximumPaneWidth()`、`setPaneWidth()`；
- `lastExpandedWidth()`；
- `ZzSidePaneEdge` Left/Right。

折叠时隐藏 SidePane 但保留最后展开宽度。展开宽度必须钳制到当前最小/最大范围。
拖拽只修改当前 SidePane 宽度，不遍历页面或重建 layout。所有权变化必须通过公开方法
完成，禁止靠动态属性保存 ID。

## 8. ZzExplorerPane 设计

### 8.1 组成

`ZzExplorerPane` 固定包含：

1. 标题 label；
2. 可选命令区，内部使用标准 `QToolBar`；
3. 可选搜索框；
4. 一个 `QTreeView` 和 `ZzFluentItemDelegate`。

`toolBar()` 和 `treeView()` 返回稳定的非拥有观察指针，允许应用添加 QAction 或配置
标准 selection mode，但应用不得替换内部 view。

### 8.2 模型与筛选

- `setModel(QAbstractItemModel *)` 使用 QPointer 观察外部模型；
- 内部私有递归 proxy 负责大小写折叠后的精确、前缀和包含匹配；
- 对外的 current index、activated 和 selection 信号全部映射回源模型；
- 空查询直接透传，不遍历并复制源模型；
- 默认使用一个持久的 60 ms 搜索延迟定时器，可在 0 至 500 ms 设置；
- 每次输入只重启同一个定时器，禁止创建 singleShot lambda 链；
- 模型 reset、rowsInserted、rowsRemoved 和 dataChanged 只失效相关缓存；
- 模型销毁后清空 proxy、选择和搜索结果，不访问悬空索引；
- 组件不保存会话 ID、主机地址或业务节点指针。

## 9. ZzTabWidget 增强设计

### 9.1 新增展示状态

现有转移和拖出事务保持不变。新增状态按页面 QWidget 指针存放在私有容器中，不占用
应用可使用的 `QTabBar::tabData()`：

- pinned：固定标签；
- modified：未保存/脏状态；
- attention：需要注意；
- closeEnabled：是否允许用户请求关闭。

状态在同容器重排、跨容器成功转移和拖出确认后随页面移动。页面外部销毁时必须移除
私有状态。重复 setter 不发重复信号。

### 9.2 交互规则

- 右侧稳定的新建按钮发出 `newTabRequested()`；
- pinned 标签保持在所有普通标签之前；
- pinned 标签默认不显示关闭按钮，批量关闭跳过 pinned 和 closeEnabled=false 页面；
- 单标签关闭继续使用 `tabCloseRequested(int)` 意图，不自行删除页面；
- 批量关闭发出按页面指针组成的 `tabsCloseRequested(QList<QWidget *>)`；
- 上下文菜单提供关闭、关闭其他、关闭右侧、固定/取消固定；
- modified 和 attention 只影响展示，不推导业务保存状态；
- attention 不因选中自动清除，必须由应用显式确认；
- `setPageTitle(QWidget *, QString)` 同时更新 tabText 和页面 windowTitle，并发出
  `pagePresentationChanged(QWidget *)`；
- Shell 监听 currentChanged、pagePresentationChanged 和当前页面 WindowTitleChange；
- 应用直接调用基类 `setTabText()` 时不保证标题栏立即同步，必须使用新入口或同步设置
  页面 `windowTitle`。

## 10. ZzCommandPalette 设计

### 10.1 模型角色

组件接收非拥有、平面的 `QAbstractItemModel`：

| 数据角色 | 语义 |
|---|---|
| `Qt::DisplayRole` | 命令名称 |
| `Qt::DecorationRole` | 可选图标 |
| `Qt::ToolTipRole` | 可选说明 |
| `Qt::AccessibleTextRole` | 可选无障碍完整名称 |
| `ZzCommandItemRole::Keywords` | QStringList 关键词 |
| `ZzCommandItemRole::Shortcut` | QKeySequence 或可显示字符串 |
| `ZzCommandItemRole::Group` | 可选分组名称 |
| `ZzCommandItemRole::Priority` | 可选整数，值越大越靠前 |

Enabled 状态通过 `QAbstractItemModel::flags()` 判断，不建立第二份 enabled 状态。

### 10.2 搜索与排序

私有 proxy 为每行缓存规范化名称和关键词。排序优先级固定为：

1. 名称完全匹配；
2. 名称前缀匹配；
3. 名称 token 前缀匹配；
4. 名称包含匹配；
5. 关键词匹配；
6. Priority 降序；
7. 源模型行号稳定排序。

默认不使用编辑距离。查询长度钳制为 512 个 UTF-16 code unit。模型结构或相关数据
变化时增量失效缓存；查询变化不得创建 QObject、item widget 或每行正则表达式。

### 10.3 覆盖层行为

- Palette 是 parent workspace 内的覆盖 QWidget，不创建顶层窗口；
- 打开时覆盖 parent rect、保存原焦点并聚焦搜索框；
- Escape、外部点击或显式关闭恢复原焦点；
- Up/Down、PageUp/PageDown、Home/End 选择结果；
- Enter 只激活 enabled 源索引，发出 `commandActivated(QModelIndex)` 后关闭；
- 组件不执行 QAction、路由、SSH 命令或其他业务副作用；
- parent resize、隐藏或销毁时同步几何或安全关闭；
- 减少动效时直接显示终态；如增加动效，只允许一个持久动画。

## 11. ZzDockPanel 设计

`ZzDockPanel final : public QDockWidget`，保留全部 Qt 原生协议。新增价值只包括：

- 使用 Fluent 标题栏展示图标、标题、浮动/重新停靠和关闭按钮；
- 标题栏按钮显隐跟随 `QDockWidget::features()`；
- `setIconDescriptor()` 使用现有图标缓存；
- 内容所有权继续通过 QDockWidget `setWidget()/widget()`；
- 增加 `takeContentWidget()`，归还内容且不删除；
- 浮动、关闭和重新停靠使用 Qt 公共方法，不调用平台 API；
- `toggleViewAction()`、allowedAreas、features、dockLocation 和 objectName 保持原生；
- Dock 浮动窗口不附加第二个 `ZzWindowAgent`，由 Qt 管理原生浮动窗口；
- 绘制使用 palette、token 和现有 `ZzFluentStyle`，禁止裸色值与 stylesheet。

## 12. ZzWorkspaceShell 设计

### 12.1 生命周期与创建

Shell 使用失败可见的工厂：

```cpp
static ZzCore::ZzResult<std::unique_ptr<ZzWorkspaceShell>> create(
    QMainWindow *host,
    ZzFluentUI::ZzFluentTitleBar *titleBar = nullptr);
```

要求：

- host 非空、是顶层 `QMainWindow`、与当前线程一致；
- titleBar 可空；非空时必须是 host 的后代且同线程；
- Shell 自身由返回的 unique_ptr 拥有；
- Shell 创建的 workspace root、Activity Bar、Side Pane、Tab、Palette 和 Dock 由 Shell
  逻辑拥有，并挂入 host 的 QObject 树；
- host 先销毁时 QPointer 全部失效，Shell 析构不得访问已释放对象；
- Shell 先销毁时同步移除其 Dock 并销毁仍由其拥有的内容；调用方如需保留内容必须
  先通过 `takePanel()` 取回。

### 12.2 面板 ID

`ZzWorkspacePanelId` 包装非空、trim 后不为空的 QString，提供 `isValid()`、`value()`、
比较、`qHash` 和 QVariant 元类型。ID 在一个 Shell 的 Side Panel 和 Dock Panel 中全局
唯一。布局持久化以 ID 恢复，禁止以标题、指针或数组索引作为稳定身份。Shell 为每个
Dock 生成 `zzWorkspaceDock:<id>` 形式的确定 objectName，调用方不得在注册后修改；
`QMainWindow::saveState()` 依靠这个名称恢复原生 Dock 状态。

### 12.3 核心公开入口

以下签名是实施约束，允许补充 const getter 和通知信号，不允许改变所有权语义：

```cpp
[[nodiscard]] QWidget *workspaceWidget() const noexcept;
[[nodiscard]] ZzFluentUI::ZzTabWidget *tabWidget() const noexcept;
[[nodiscard]] ZzFluentUI::ZzCommandPalette *commandPalette() const noexcept;

[[nodiscard]] ZzCore::ZzResult<void> registerSidePanel(
    const ZzWorkspacePanelId &id,
    QString title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    QWidget *content);

[[nodiscard]] ZzCore::ZzResult<void> registerDockPanel(
    const ZzWorkspacePanelId &id,
    QString title,
    ZzFluentUI::ZzIconDescriptor icon,
    Qt::DockWidgetArea area,
    QWidget *content);

[[nodiscard]] ZzCore::ZzResult<QWidget *> takePanel(
    const ZzWorkspacePanelId &id);

[[nodiscard]] ZzCore::ZzResult<void> showPanel(
    const ZzWorkspacePanelId &id,
    bool visible = true);

[[nodiscard]] ZzCore::ZzResult<void> setPanelBadge(
    const ZzWorkspacePanelId &id,
    int value);

[[nodiscard]] ZzCore::ZzResult<QByteArray> saveLayout() const;
[[nodiscard]] ZzCore::ZzResult<void> restoreLayout(
    const QByteArray &state);
```

Side Panel 注册成功后 content 由对应 `ZzSidePane` 拥有。Dock Panel 注册成功后 Shell
创建 `ZzDockPanel` 并将 content 交给它。任何失败必须发生在所有权转移之前。

### 12.4 标题策略

`ZzWorkspaceTitleMode`：

- `Application`：只显示应用标题；
- `CurrentTab`：当前页面标题，无页面时回退应用标题；
- `CurrentTabAndApplication`：`当前页面 - 应用标题`；
- `Custom`：显示显式自定义标题，无值时回退应用标题。

Shell 维护 `applicationTitle` 和 `customTitle` 展示值，同时更新宿主 `windowTitle` 与
`ZzFluentTitleBar::title`。当前页标题优先读取 QWidget `windowTitle`，为空时读取
`ZzTabWidget::tabText()`。

### 12.5 置顶和主题数据流

```text
用户点击标题栏
  -> ZzFluentTitleBar 发出请求
  -> ZzWorkspaceShell（置顶）或应用 presenter（主题）执行
  -> 读取实际结果
  -> setAlwaysOnTop / setThemeMode 回写标题栏
```

Shell 只负责宿主窗口置顶。主题切换涉及应用级 `ZzThemeController`，由应用连接
`themeModeRequested`；Shell 不查找全局主题单例。

应用置顶时必须保留当前可见性、最大化/全屏状态，避免简单 `setWindowFlags()` 导致
窗口永久隐藏。实现只能使用 Qt 公开 API，并增加 Linux、Windows、macOS 条件编译
静态检查；不得在 `ZzFluentUI` 中出现平台代码。

### 12.6 布局格式

最大输入固定为 1 MiB。二进制格式为：

```text
magic "ZZWS"              4 bytes
schemaVersion              quint16，首版为 1
streamVersion              quint16，固定 Qt_6_8
payloadLength              quint32
payload                    payloadLength bytes
sha256(payload)            32 bytes
```

payload 使用明确字段序列，不序列化 C++ 对象内存：

1. `QMainWindow::saveState(1)`；
2. 左右 Side Pane 展开状态和最后宽度；
3. 当前左右 Side Panel ID；
4. Side Panel 的 area 与顺序；
5. 当前 Tab 页面索引只作为展示恢复提示，不保存页面内容；
6. 标题模式。

应用必须先注册全部面板，再调用 restore。恢复流程：

1. 完整验证 header、长度、版本、digest 和字段范围；
2. 保存当前 Qt state 和 Shell state 作为回滚快照；
3. 只对已注册 ID 应用 area、顺序、宽度和显隐；
4. 未知 ID 忽略，缺失 ID 保持默认位置；
5. 调用 `QMainWindow::restoreState(payload, 1)`；
6. 任一步失败则恢复快照并返回 `ZzError`；
7. 成功后一次性同步 Activity 选择、Side Pane 和标题。

布局不包含 SSH 会话、文件、凭据、命令历史、窗口几何或业务设置。窗口 geometry 由
应用使用自己的设置服务独立保存。

## 13. 线程、错误和所有权规则

### 13.1 线程

全部 QWidget、model 绑定、面板注册和 Shell 方法只能在 GUI 线程调用。公开入口：

- Debug 下使用 `Q_ASSERT(QThread::currentThread() == thread())`；
- Release 下仍必须返回 InvalidState、false 或安全 no-op；
- 禁止跨线程直接读取 model 或 QWidget；
- 后台 SSH 数据必须由应用通过 queued signal 更新 GUI 线程 model。

### 13.2 错误语义

- `ZzWorkspaceShell` 的可失败操作返回 `ZzCore::ZzResult`；
- 重复 ID、空标题/ID、空 content、非法 area、跨线程和未注册 ID 都是显式错误；
- FluentUI setter 对非法索引使用拒绝操作或 bool，不能抛异常；
- 标签、Activity 和布局移动先验证再提交；
- 失败不得丢失页面、改变父对象或留下半更新选择；
- model 被销毁时组件自动清空，属于生命周期收敛，不作为错误弹窗；
- 组件禁止显示 QMessageBox 报告程序错误。

### 13.3 所有权表

| 对象 | 所有权 |
|---|---|
| 外部 QAbstractItemModel | 调用方拥有，组件 QPointer 观察 |
| 外部 QAction/QMenu | 调用方或 menu QObject 树拥有，标题栏/工具栏观察 |
| workspaceWidget 和内部组件 | Shell 逻辑拥有，挂入 host QObject 树 |
| 注册后的 Side Panel content | ZzSidePane 拥有，takePanel 后交还 |
| 注册后的 Dock content | ZzDockPanel 拥有，takePanel 后交还 |
| Tab 页面 | ZzTabWidget 按 Qt parent 规则拥有，关闭意图不删除 |
| ZzWorkspaceShell | create 返回的 unique_ptr 拥有 |
| QMainWindow / ZzFluentTitleBar | 调用方拥有，Shell QPointer 观察 |

## 14. 性能设计与预算

### 14.1 热路径硬约束

paint、hover、mouse move、tab currentChanged 和 Dock resize 路径禁止：

- 创建 QObject、动画、定时器、SVG renderer 或 style；
- 解析 SVG/TTF、访问文件、网络、QSettings 或环境变量；
- 遍历全部页面内容 QWidget；
- 创建与总模型行数等量的 QWidget；
- 每帧构造大 QStringList、QRegularExpression 或 pixmap；
- 调用 `QApplication::processEvents()`。

所有动效最多使用每组件一个持久动画，并服从 `ZzAnimationPolicy` 和 reduced motion。

### 14.2 复杂度目标

| 操作 | 目标 |
|---|---|
| 标题、主题、置顶状态更新 | O(1) |
| 菜单模式切换 | O(顶层菜单数)，仅结构/尺寸变化 |
| Activity 激活 | O(1) 查 ID + 固定视图更新 |
| Side Pane 宽度拖拽 | O(1) |
| Tab 切换和标题同步 | O(1) |
| Tab 跨容器转移 | O(1) 元数据 + Qt 插入/移除 |
| Command 查询 | O(命令行数)，无每行 QObject |
| Explorer 查询 | O(可搜索节点数)，由 60 ms 延迟合并输入 |
| Dock 显隐和浮动 | 委托 Qt，不扫描其他 Dock 内容 |
| 布局保存/恢复 | O(已注册面板数 + Qt Dock 数) |

### 14.3 首轮 benchmark 场景

新增 `benchmark.workspace-components`，至少记录：

1. 100 个顶层菜单 action 下 2000 次宽窄切换；
2. 500 个 Activity model 行下 1000 次激活与移动请求；
3. 100000 个 Explorer 节点下固定 20 条查询；
4. 200 个 Tab 下 1000 次切换和固定/脏状态更新；
5. 10000 条命令下固定 20 条查询和键盘选择；
6. 64 个 Side Panel、32 个 Dock 下保存/恢复 100 次；
7. 完整 workspace render；
8. 每场景前后 QObject 数、样式缓存和进程 RSS。

首轮指标使用 `observe`，不修改现有正式阈值。Linux 参考机连续三轮采样后，在同一
指纹下评估 P50/P95/最大值和噪声带，再单独提交硬阈值变更。无论是否 gate，以下
结构性条件从首版即为硬失败：

- 重复操作后 QObject 数持续增长；
- 可见列表为每行创建 QWidget；
- 布局失败没有回滚；
- 绘制结果为空或单色；
- 模型销毁后访问失效对象；
- 1000 次状态切换后定时器/动画对象数量增加。

## 15. 测试设计

### 15.1 TDD 顺序

每个组件必须严格按下列顺序：

1. 新增最小失败测试；
2. 运行并确认因目标能力缺失而失败；
3. 写最小实现使测试通过；
4. 补充边界、无障碍、RTL、DPR、生命周期和对象预算；
5. 再接入 Example、截图、benchmark 和安装消费；
6. 运行定向测试后立即中文提交。

禁止先写完整实现再补测试。

### 15.2 组件测试矩阵

| 测试 | 必须覆盖 |
|---|---|
| `ZzFluentTitleBarTest` | 菜单自适应/迟滞、居中不重叠、动态 action、主题/置顶意图、RTL、Chrome widgets |
| `ZzActivityBarTest` | 两组投影、激活/折叠、键盘、badge、拖拽拒伪造、模型销毁、对象稳定 |
| `ZzSidePaneTest` | 所有权、take、宽度钳制、左右把手、折叠恢复、页面销毁 |
| `ZzExplorerPaneTest` | 源索引映射、递归筛选、延迟合并、增删/reset、模型销毁、100k 模型 |
| `ZzTabControlsTest` | pinned/modified/attention、批量关闭、状态转移、标题同步、失败回滚 |
| `ZzCommandPaletteTest` | 匹配排序、disabled、键盘、焦点恢复、模型更新/销毁、无 item widget |
| `ZzDockPanelTest` | features、allowedAreas、toggle action、float/redock、内容 take、标题栏无障碍 |
| `ZzWorkspaceShellTest` | 注册、重复 ID、所有权、侧栏同步、标题策略、置顶、布局校验与回滚 |

### 15.3 集成与视觉

- `ZzFluentScreenshotTest` 增加 titlebar、activity/side pane、command palette、dock/tab
  场景，覆盖 Light、Dark、HighContrast 和 DPR 100/125/150/200；
- 截图必须断言标题、指示条、文字、badge、菜单和系统按钮不重叠；
- `ZzPureToolsExample` 增加独立 workspace 场景，使用固定本地模型模拟会话、SFTP、
  日志、属性和任务面板；
- smoke 测试通过公开接口验证单击激活、命令搜索、Tab 新建/关闭意图、Dock 浮动、
  标题跟随和布局 round-trip；
- `tests/InstallConsumer` 在 shared/static 包中实例化新增公共类型；
- PublicHeaderConsumer 独立编译全部新增头；
- ArchitectureAudit 检查新源码无业务依赖、裸色、stylesheet 和 Qt Private 头。

### 15.4 平台边界

- Linux：Debug、Release、Static、Clang-Tidy、ASan/UBSan、截图、benchmark；
- Windows MSVC：源码条件分支、严格警告和公共头静态检查；有环境后执行原生构建；
- Windows MinGW：避免 MSVC 专属 API/语法，使用同一 Qt 公共协议；
- macOS：`QMenuBar::setNativeMenuBar(false)` 合同、Dock 浮动和标题按钮静态检查；
- 未执行原生构建的平台必须记录“未执行”，不得写成通过。

## 16. 分批交付边界

实施计划应按以下顺序拆分，每个逻辑修改立即提交：

1. **标题栏增强**：自适应菜单、居中布局、主题/置顶、WindowKit hit-test；
2. **Activity 与 Side Pane**：模型投影、交互、面板容器；
3. **Tab 工作区增强**：固定、脏状态、注意、批量关闭和标题入口；
4. **Explorer 与 Command Palette**：大模型筛选和命令搜索；
5. **DockPanel 与 WorkspaceShell**：注册、协调、置顶和布局编解码；
6. **Example 串联**：SSH 风格假数据和完整公开接口使用；
7. **性能与质量收尾**：benchmark、截图、安装消费、静态分析和文档。

单个导出组件的实现、测试和最小 Gallery/Example 展示应在同一个功能提交中。跨组件
基础设施可独立提交，但不得创建长期无生产消费者的 API。

## 17. 验收条件

阶段完成必须同时满足：

1. 七个工作区 UI 能力和一个 Shell 均为公开可安装组件；
2. Example 不包含 Activity、Tab、Command、Dock 或标题同步内部算法；
3. SSH 风格组合场景可以只靠公开接口建立；
4. 标题栏满足已确认的 A 方案和自适应菜单方案；
5. Dock 支持 Qt 原生浮动窗口；
6. UI 不访问 SSH/SFTP/数据库/网络/设置服务；
7. 所有所有权转移和失败回滚由测试锁定；
8. 大模型场景不创建每行 QWidget，重复操作无 QObject 增长；
9. Linux 全量门禁通过，新 benchmark 有三轮观测报告；
10. Windows MSVC、Windows MinGW、macOS 状态如实记录；
11. shared/static 安装消费均能包含新增公共接口；
12. 所有源码和公开 API 遵守中文 Doxygen、PIMPL、命名和 namespace 规范。

## 18. 已确认的设计决策

用户已明确确认：

- 三类能力最终都需要，优先服务 SSH 客户端真实工作流；
- SSH 只作为验收场景，组件不得绑定 SSH 业务；
- 会话侧栏、多标签工作区、命令面板和停靠面板必须是组件能力；
- 采用模块化混合架构；
- Dock 第一版支持拖成独立浮动窗口；
- 标题栏选择 A：左应用入口、中间居中标题、右窗口命令；
- 菜单选择自适应：宽窗口横向菜单，窄窗口折叠菜单；
- 已确认本文的数据流、所有权、交互、错误和性能方向。
