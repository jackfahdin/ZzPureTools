# 工作区 Activity、组件导航与设置窗口重构设计

## 1. 文档目的

本文定义 `ZzActivityBar`、`ZzWorkspaceShell`、`ZzApplicationWindow` 与
`ZzPureToolsExample` 的下一轮工作区交互重构。目标是把左右 Activity Bar 统一为
IDE 风格的竖向 Tab，把组件导航从中央区域迁入左侧 Side Panel，并以固定 Activity
Action 打开窗口模态设置页。

本文是实现规格，不是效果建议。后续实现计划必须逐项覆盖本文接口、状态、回滚、
迁移、测试和性能要求，不得把组件能力下沉到 Example。

## 2. 已确认的产品行为

默认 Activity 布局如下：

| 区域 | 默认入口 | 行为 |
|---|---|---|
| 左上 `LeftPrimary` | 会话、文件、组件 | 可移动 Side Panel，连续排列 |
| 左下 `LeftSecondary` | 设置 | 固定 Activity Action，不可移动 |
| 右上 `RightPrimary` | 属性、任务 | 可移动 Side Panel，连续排列 |
| 右下 `RightSecondary` | 空 | 接收移动后的 Side Panel |

同一物理侧最多只有一个当前 Side Panel。点击同侧其他入口切换面板；再次点击当前
入口折叠该侧 Side Pane，但保留当前入口身份和短指示条。固定设置动作不改变任何
Side Panel 当前状态。

可移动入口支持拖拽和右键“移动到”。右键子菜单只列出另外三个区域，必须排除当前
区域；菜单移动把入口追加到目标区域末尾，拖拽继续负责目标区域内的精确排序。

右上和右下都没有可移动入口时，右 Activity Bar 与右 Side Pane 必须一起隐藏并释放
布局宽度和命中区域。新入口移入右侧后恢复 Activity Bar；Side Pane 保持折叠，等待
用户激活入口。左侧即使所有 Side Panel 都被移走，也因固定设置动作保留 Activity
Bar；没有左侧面板时只隐藏左 Side Pane。

设置入口固定在左下。点击后打开当前主窗口的 `Qt::WindowModal` 设置窗口，只阻止
所属主窗口，不阻止其他应用窗口，不使用 `Qt::ApplicationModal`，也不使用系统级
`Qt::WindowStaysOnTopHint`。

## 3. 现状与根因

### 3.1 默认区域注册错误

Example 当前把会话注册为 `LeftPrimary`、文件注册为 `LeftSecondary`，把属性注册为
`RightPrimary`、任务注册为 `RightSecondary`。`ZzActivityBar` 使用上下两个固定
`QListView` 投影，并在两者之间加入弹性空间，因此文件和任务被推到侧栏底部，而不是
与同侧入口连续排列。

### 3.2 中央区域嵌套导航

`ZzApplicationWindowPrivate` 当前创建包含 `ZzNavigationPane` 与 `ZzPageHost` 的完整
中央 body。Example 通过 `takeCentralWidget()` 取得整个 body，再把它作为“组件示例”
标签加入 `ZzWorkspaceShell`。结果是组件导航与 PageHost 一起进入中央标签，形成第二块
中央导航区域。

### 3.3 多活动状态不符合竖向 Tab 语义

`ZzWorkspaceShellPrivate` 当前把左右 `ZzSidePane` 设置为 `Stacked`，并为两个
`ZzActivityBar` 启用 `multiActiveEnabled`。Shell 同步所有可见面板到
`activeSourceIndexes`，允许同侧多个入口同时保持活动指示。该行为适用于堆叠监控面板，
但不符合本次确认的单活动竖向 Tab。

### 3.4 指示条重复且右侧方向错误

`ZzActivityItemDelegate` 先调用 `ZzItemViewVisual::draw()` 绘制标准选择状态，随后又为
`activeSourceIndexes` 绘制一个全行高指示条，因此选择与活动状态会产生两套视觉。
当前额外指示条使用 `QStyle::visualRect()` 的逻辑 leading 边，在 LTR 下左右 Activity
Bar 都落在条目左侧，无法表达右侧物理边缘。

### 3.5 设置仍是组件路由

`settings` 当前属于 `ZzExampleRouteCatalog` 的 Footer 路由，并由中央导航与 PageHost
创建。它既不是稳定的工作区 Activity Action，也没有独立模态窗口生命周期。

### 3.6 现有能力可以保留

`ZzActivityArea` 已完整定义左上、左下、右上、右下四个区域；Activity Bar 已有安全的
进程内拖放令牌和 `moveRequested`；`ZzWorkspaceActivityMoveTransactionPrivate` 已负责
模型、Widget 所有权、当前项、可见集合和顺序的事务迁移。新实现必须复用这些能力，
不得建立第二套移动引擎。

## 4. 组件职责

### 4.1 `ZzActivityBar`

`ZzActivityBar` 是模型驱动的纯展示组件，负责：

- 把同一模型投影为当前物理侧的 Primary 与 Secondary 两组；
- 绘制图标、Badge、Hover、Pressed、Focus 和唯一短指示条；
- 处理鼠标点击、键盘遍历、拖拽和上下文菜单；
- 对可移动入口发出 `moveRequested`；
- 对普通激活发出 `activationRequested` 或 `collapseRequested`。

它不得修改源模型，不得访问 Workspace 面板注册表，不得打开设置窗口，也不得执行
任何业务命令。

### 4.2 `ZzWorkspaceShell`

`ZzWorkspaceShell` 负责：

- Side Panel 与固定 Activity Action 的统一 Activity 模型；
- 每个物理侧的唯一当前 Side Panel；
- Side Pane 的切换、折叠、边缘显隐和对象所有权；
- 四区域移动事务、区域顺序与布局持久化；
- `ZzApplicationWindow` 导航表面的事务集成；
- 把固定 Activity Action 激活转发到调用方拥有的 `QAction`。

Shell 不读取设置存储、业务模型或页面 ViewModel。

### 4.3 `ZzApplicationWindow`

`ZzApplicationWindow` 继续拥有导航模型、导航控制器和 PageHost 生命周期。它向同一
`ZzWorkspaceShell` 提供可回滚的导航表面拆分能力，不把导航业务复制到 Shell。

### 4.4 `ZzPureToolsExample`

Example 只负责：

- 注册会话、文件、组件、属性、任务五个具体面板；
- 注册固定设置 QAction；
- 创建设置窗口及其 View/Presenter；
- 提供具体页面、会话模型与设置存储。

Example 不实现 Activity 移动、布局迁移、指示状态或空侧显隐。

## 5. 公开接口

### 5.1 固定 Activity 标识

新增轻量值类型 `ZzWorkspaceActivityId`，语义与 `ZzWorkspacePanelId` 分离。它保存经
校验的稳定字符串标识，用于固定动作注册、重复检测、诊断和测试。Side Panel 继续使用
`ZzWorkspacePanelId`，不得把设置伪装成 Panel。

### 5.2 固定动作注册

`ZzWorkspaceShell` 新增：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> registerFixedActivityAction(
    const ZzWorkspaceActivityId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    QAction *action);
```

契约如下：

- `id`、标题、图标和 `action` 必须合法；
- `action` 必须属于 Shell GUI 线程；
- `QAction` 由调用方拥有，Shell 使用 `QPointer` 非拥有观察；
- QAction 的 enabled 状态映射到 Activity 行；
- QAction 销毁时自动移除入口并同步边缘显隐；
- 固定动作不设置 `Qt::ItemIsDragEnabled`，不显示移动菜单；
- QAction 启用时，固定动作只设置 `Qt::ItemIsEnabled`，不得设置
  `Qt::ItemIsSelectable` 或 `Qt::ItemIsDragEnabled`；QAction 禁用时同步移除
  `Qt::ItemIsEnabled`；
- 固定动作不进入 current panel、可见 Side Panel 或布局 JSON；
- Side Panel 与固定动作的稳定标识在统一 Activity 注册域内不得产生歧义。

### 5.3 应用导航集成

`ZzWorkspaceShell` 新增：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> integrateApplicationNavigation(
    const ZzWorkspacePanelId &panelId,
    const QString &panelTitle,
    ZzFluentUI::ZzIconDescriptor icon,
    ZzFluentUI::ZzActivityArea area,
    const QString &centralTabTitle);
```

该接口只允许用于 Shell 当前宿主，且宿主必须是尚未集成导航表面的
`ZzApplicationWindow`。操作必须作为一个事务完成：

1. 校验 `ZzNavigationPane`、`ZzPageHost`、导航控制器和原 body 的身份及父子关系；
2. 从原 body 中安全解除 NavigationPane 与 PageHost；
3. 把 NavigationPane 注册为 Side Panel；
4. 把 PageHost 加入当前 SplitWorkspace 的固定、不可关闭中央标签；
5. 保持 NavigationModel、NavigationController 和 PageFactory 的既有连接；
6. 成功后删除空原 body；
7. 任一步失败时恢复原 layout、父子关系、中心控件、面板注册和当前路由。

同一个 ApplicationWindow 只允许集成一次。普通 `QMainWindow` 创建的 WorkspaceShell
调用该接口必须返回明确 `InvalidState`，不得崩溃或静默忽略。

## 6. Activity 模型与交互

### 6.1 行种类

Shell 内部 Activity 行区分：

- `SidePanel`：关联 `ZzWorkspacePanelId`，可移动，可成为当前项；
- `FixedAction`：关联 `ZzWorkspaceActivityId` 与 `QAction`，不可移动，不成为当前项。

该区分属于 Shell 私有模型，不向 `ZzActivityBar` 暴露业务类型。Activity Bar 仅根据
模型 flags 决定通用交互：启用的 SidePanel 设置
`Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled`；启用的 FixedAction
只设置 `Qt::ItemIsEnabled`。`ZzActivityBar::activateSourceIndex()` 遇到 enabled 但
non-selectable 的行时，只发出 `activationRequested`，不得调用
`setCurrentSourceIndex()`，也不得发出 `collapseRequested`。对于 selectable 行，Activity
Bar 才执行当前项切换或再次激活折叠。Shell 收到激活意图后再根据私有行种类切换
Side Panel 或触发 QAction。

### 6.2 默认顺序

Example 必须按以下顺序注册：

1. 会话 `LeftPrimary`；
2. 文件 `LeftPrimary`；
3. 组件 `LeftPrimary`；
4. 设置 `LeftSecondary`，FixedAction；
5. 属性 `RightPrimary`；
6. 任务 `RightPrimary`。

同一区域维持模型顺序。Primary 与 Secondary 之间仍使用弹性空间，不在同一区域条目
之间插入伸缩项或分割面板。

### 6.3 单活动状态

每侧维护：

- 一个可空 `currentPanel`；
- 一个 `paneExpanded`；
- 一个与 currentPanel 对应的短指示条。

`ZzSidePane` 使用 `Single` 模式；`ZzActivityBar` 关闭 multi-active。Shell 不再把所有
visibleWidgets 同步为多个活动索引。切换当前项时，旧面板隐藏、新面板显示，Side Pane
对象与页面对象均不销毁。

再次激活当前项只切换 `paneExpanded`，不清空 currentPanel。模型 reset、行删除、
跨区移动或内容销毁后，如果 currentPanel 失效，则按同侧区域顺序选择首个合法面板；
同侧没有面板时清空并折叠。

### 6.4 右键移动

Activity Bar 在可移动条目的上下文菜单中创建“移动到”子菜单。子菜单按稳定顺序列出：

1. 左上；
2. 左下；
3. 右上；
4. 右下；

创建时删除当前区域，因此始终恰好剩余三个动作。选择目标后，以目标区域当前行数作为
`targetRow` 发出既有 `moveRequested`。菜单不得直接调用模型 mutation。

上下文菜单按需创建并在关闭后释放；不得为每个行创建 QAction/QMenu 常驻对象。固定
动作、禁用动作或无效索引不显示移动菜单。

### 6.5 移动后的当前项

- 移动非当前项：目标区域追加，两个物理侧当前项均保持不变；
- 同侧移动当前项：保持当前与展开状态；
- 跨侧移动当前项：目标侧切换到该项并保持展开，目标侧旧当前面板隐藏；
- 源侧选择剩余顺序中的首个合法面板；没有剩余面板时清空并折叠；
- 右侧最后一个条目移走时，只有事务提交成功后才隐藏右边缘；
- 任一失败必须恢复移动前区域、顺序、当前项、展开状态、所有权和边缘可见性。

## 7. 指示条设计

Activity Item Delegate 只能绘制一条激活指示。实现不得在
`ZzItemViewVisual::draw()` 的标准选择指示之外再叠加全高活动条。

唯一指示条使用现有 Fluent 选择长度、厚度、圆角和颜色令牌，垂直居中：

- 左 Activity Bar 固定贴条目左侧；
- 右 Activity Bar 固定贴条目右侧；
- 物理边缘不随 LTR/RTL 改变；
- 折叠但仍为 current 的入口继续显示；
- FixedAction 不显示持久激活指示。

Hover、Pressed、Disabled 与键盘 Focus 继续使用现有状态表面。Focus 只在键盘焦点存在
时出现，不得形成第二条持久边框或指示线。

## 8. 边缘显隐

`syncSideEdgeVisibility()` 必须分别计算：

- `hasActivityEntry`：该边是否存在 SidePanel 或 FixedAction；
- `hasSidePanel`：该边是否存在可移动 SidePanel；
- `hasExpandedPanel`：该边当前面板是否展开。

规则如下：

- Activity Bar 的 visible 等于 `hasActivityEntry`；
- Side Pane 在没有 SidePanel 时必须隐藏并折叠；
- 有 SidePanel 但未展开时 Side Pane 隐藏或保持零宽，不保留 resize 命中区域；
- 有展开面板时 Side Pane 才占用 paneWidth；
- 从空边移入条目时只恢复 Activity Bar，不自动展开 Side Pane；
- 固定设置动作保证左 Activity Bar 存在，但不能让空左 Side Pane 占宽；
- 右侧无任何入口时，Activity Bar 与 Side Pane 都不参与布局。

对象可以保持稳定创建以满足性能预算，但隐藏状态必须在布局、sizeHint 与 hit test 上等价
于不占用边缘。

## 9. 布局持久化与迁移

工作区布局 schema 从 v2 升级为 v3。v3 保存：

- 所有 SidePanel 的 area 与区域内顺序；
- 每侧 currentPanel；
- 每侧 paneExpanded 与合法宽度；
- 既有 SplitWorkspace、BottomPane、Dock 与标签状态。

FixedAction 不保存，其位置完全由注册代码决定。

v2 到 v3 的迁移规则：

1. 保留 SidePanel 区域与顺序；
2. 每侧优先使用 v2 的 current panel；
3. current 无效时，选择 v2 可见集合中按区域顺序的第一项；
4. 仍无合法项时选择该侧首个已注册面板，但保持折叠；
5. 丢弃其余同时可见状态，迁移为单活动；
6. FixedAction 由当前代码重新注册，不从 v2 内容推断；
7. 任一未知 ID、重复项、无效区域或所有权中断都使 restore 失败并恢复调用前状态。

## 10. 设置窗口

新增 Example 展示类 `ZzExampleSettingsWindow`，按项目四文件结构实现公开类与 Private。
它是设置 View，不直接访问 `ZzSettingsStore`；现有 Context/Presenter 继续负责读取、写入
和错误报告。

窗口生命周期：

- parent 为触发动作所属 `ZzApplicationWindow`；
- modality 为 `Qt::WindowModal`；
- 每个主窗口最多一个实例；
- 重复触发调用 `show()`、`raise()`、`activateWindow()`，不重复创建；
- 关闭后销毁并清空观察指针；
- 不设置 ApplicationModal 或 WindowStaysOnTopHint；
- 使用现有 `ZzWindowKit` 与 `ZzFluentTitleBar` 组合提供窗口外壳；
- 跟随应用主题、语言、DPR 与 Reduced Motion；
- 关闭主窗口时先关闭所属设置窗口，不影响其他主窗口。

`settings` 从 `ZzExampleRouteCatalog`、中央 NavigationModel 和 PageHost 注册中移除。
`about` 保留为组件导航 Footer。设置 QAction 同时注册到左下 FixedAction，并可进入命令
面板；唯一常驻可见入口仍是左下 Activity Bar。

## 11. 错误处理与重入

所有新增 Shell mutation 返回 `ZzResult`，延续现有 GUI 线程、对象身份和事务种类检查。

- Activity move、layout restore 与 navigation integration 不得并发；
- QAction、窗口、模型或 QWidget 在同步信号中销毁时必须使用 QPointer 审计；
- QAction trigger 导致 Shell 或主窗口销毁时不得继续访问私有状态；
- 上下文菜单关闭、取消或目标动作销毁不得发出移动；
- 设置窗口创建失败由 Example Presenter 写日志并在当前窗口展示可理解错误；
- 所有失败路径保持调用前模型顺序、对象所有权、当前项和可见性。

## 12. 测试要求

### 12.1 `ZzActivityBarTest`

- 每侧唯一当前项和再次激活折叠意图；
- 键盘跨 Primary/Secondary 遍历并跳过禁用项；
- 左指示贴左、右指示贴右、RTL 不反转物理边；
- 每个 active row 只有一条短指示，不存在全高重复条；
- 右键菜单恰好三个目标且排除当前区域；
- FixedAction/不可拖拽行没有移动菜单；
- enabled 且 non-selectable 的 FixedAction 点击后只发激活，不改变 current，也不发折叠；
- 右键与拖拽产生一致 `moveRequested` 参数；
- 菜单取消、伪造 MIME、模型销毁和对象预算。

### 12.2 `ZzWorkspaceShellTest`

- SidePanel 与 FixedAction 注册、重复 ID、线程、disabled 和销毁合同；
- 每侧最多一个可见 Side Panel；
- 当前项折叠保留、同侧切换和跨侧移动规则；
- 右侧最后条目移走后的 bar/pane/width/hit-test 消失与恢复；
- 移动事务的所有失败注入与完整回滚；
- navigation integration 成功、重复调用、普通 QMainWindow 拒绝与逐阶段回滚；
- v2 多活动到 v3 单活动迁移、v3 round-trip 与坏数据拒绝；
- FixedAction 不进入保存结果。

### 12.3 Example 测试

- 默认六个入口的区域、顺序与类型；
- 中央对象树不再包含嵌套 NavigationPane；
- 点击组件后左 Side Pane 显示 NavigationPane，中央 PageHost 路由继续更新；
- 设置固定左下，About 仍在组件导航 Footer；
- 设置窗口 `WindowModal`、每主窗口单实例、关闭后可重建；
- 多窗口的设置窗口和 Side Panel 当前状态隔离；
- 命令面板触发设置使用同一 QAction。

### 12.4 视觉、无障碍与性能

更新 Light、Dark、HighContrast 与 100/125/150/200 DPR 截图。覆盖默认双侧、右侧
清空、右侧恢复、组件导航、设置窗口、RTL 和键盘 Focus。

Activity Bar 保持模型/Delegate 渲染，不新增逐行 QWidget；上下文菜单按需创建并释放，
不新增常驻 timer 或 animation。重新执行对象数、启动、空闲 CPU/RSS、导航、主题切换
和 workspace-components 基准，不放宽现有绝对或相对阈值。

## 13. 平台验收

Linux 自动验收包括 GCC shared/static/LTO、Clang、ASan/UBSan、clang-tidy、安装消费、
截图和三轮性能门禁。

Linux 物理桌面人工验收增加：

- 四区域右键菜单与三个目标；
- 同侧切换、当前折叠和跨侧移动；
- 右侧完全消失与恢复；
- 右侧物理边短指示；
- 设置 WindowModal、重复激活和主窗口关闭；
- IME、键盘、DPR、X11/Wayland 与 Qt fallback。

Windows MSVC、Windows MinGW 与 macOS 在对应环境验证公共 Qt API、构建、窗口 modality
与条件编译。没有真机日志时继续记录为未执行，不用 Linux 结果替代。

## 14. 非目标

本批不实现：

- 任意数量的 Activity Bar 边；
- 浮动 Activity Bar 或自由像素定位；
- 同一侧多面板同时展开；
- 设置业务模型重构或新增设置项；
- 系统全局置顶设置窗口；
- Windows/macOS 原生真机结果伪造；
- 重写现有 Activity 移动事务、SplitWorkspace 或 Bottom/Dock 系统。

## 15. 完成标准

只有以下条件全部满足才能宣称本设计完成实施：

1. 五个 Side Panel、一个 FixedAction 的默认区域与顺序正确；
2. 组件导航位于左 Side Pane，中央只保留 PageHost 内容；
3. 每侧单活动且每项只有一条物理边短指示；
4. 右键菜单只有三个非当前目标，并复用现有事务移动路径；
5. 右侧空时完全退出布局，移入后正确恢复；
6. 设置窗口为每主窗口单实例的 WindowModal；
7. v2 到 v3 迁移和所有失败回滚通过；
8. Linux 自动门禁与性能阈值通过；
9. Windows/macOS 状态按真实证据记录；
10. 工作树只剩用户已有且未读取、未修改的 `temp_image/`。
