# ZzPureTools 产品驱动 IDE Workbench 与组件扩展设计

**日期：** 2026-08-22

**状态：** 设计已确认，尚未开始实现

**适用仓库：** `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro`

**当前代码基线：** `be6267a`

**技术基线：** Qt 6.8+、C++20、CMakeLists.txt + CMakePresets.json

## 1. 目标

本阶段不以增加 WinUI 同名控件数量为目标，而是优先补齐能够承载 SSH、SFTP、
终端、远程监控等高密度桌面业务的 IDE Workbench 能力。

最终主窗口应形成以下稳定结构：

```text
+--------------------------------------------------------------------------+
| 应用图标 | 菜单 | 动态标题 | 主题 | 置顶 | 最小化 | 最大化 | 关闭       |
+--------------------------------------------------------------------------+
| Left  | Left Side  |                                  | Right Side | Right|
| Act.  | PanelStack |      ZzSplitWorkspace            | PanelStack | Act. |
| Bar   |            |  +-------------+---------------+ |            | Bar  |
|       |            |  | TabGroup A  | TabGroup B    | |            |      |
|       |            |  +-------------+---------------+ |            |      |
|       |            |  | ZzBottomPane                | |            |      |
+--------------------------------------------------------------------------+
```

组件库负责组织、分屏、拖放、显隐、尺寸和布局恢复。应用层只注册页面、面板、
命令和模型，并处理 SSH/SFTP 等业务意图。不得在 Example 中重新实现工作台骨架。

## 2. 参考基线与审计结论

### 2.1 旧版 ZzPureTools

旧版 `ZzIdeWindow` 已实现：

- 左右 Activity Bar；
- 左右 Side Panel；
- 主/次四组入口；
- 面板跨侧移动；
- 单一中央内容。

旧版仍存在以下结构限制：

- 每侧只能显示一个面板；
- 中央没有多个 Tab Group 和递归分屏；
- 面板使用整数自增 ID，缺少持久稳定标识；
- 状态、主题单例、窗口和业务页面耦合；
- 拖放不是完整事务，缺少销毁重入和失败回滚合同。

因此旧版只作为交互意图参考，不迁移其所有权和全局状态实现。

### 2.2 NyaTerm

只读审计基线：

```text
repository: https://github.com/nyakang/nyaterm
commit:     7236e8887836a0b76a251ead805fd38e4b75eeae
date:       2026-08-20
license:    MIT
```

NyaTerm 用于确认目标工作台拓扑和交互，不复制其 React/Tauri 源码。需要吸收的能力：

- 左右 Activity Bar 与 Side Panel；
- 同侧多个面板上下堆叠并分别调整高度；
- 中央标签组水平/垂直分屏；
- 中央底部辅助面板；
- 面板入口在左右主次四组间移动；
- 布局恢复和当前会话上下文联动。

### 2.3 WinUI 3 Gallery

审计基线为官方 WinUI Gallery `8854551b0464b6ad824d6b9e1c79933888f14acc`。

当前旧版 51 个公开组件均已有新版去向，不存在大规模漏迁。WinUI 对照得到的高价值
缺口为 CommandBar、AnnotatedScrollBar，以及现有 SplitButton、Pivot 的合同补强。
SelectorBar 不单独新增类型，避免与 Pivot 重复。

## 3. 已确认的设计决策

1. 采用产品垂直切片，不追求 WinUI Gallery 数量覆盖率。
2. 左右同一侧允许多个面板同时打开、上下堆叠并分别调整高度。
3. 中央工作区允许多个 Tab Group 递归水平或垂直分屏。
4. 中央底部使用专用 `ZzBottomPane`；`ZzDockPanel` 保留给可浮动、自由停靠工具。
5. Activity Bar 可以同时显示多个已打开面板的激活指示。
6. UI 控件只发意图；`ZzWorkspaceShell` 校验并提交状态。
7. 页面转移、面板迁移和布局恢复必须全成或全不成。
8. 复用 Qt 的 QAction、QSplitter、QDrag、QTabWidget、QMenu 和无障碍语义。
9. 不使用 Qt Private API，不访问平台私有布局接口。
10. 当前工作区布局版本 1 必须可迁移到版本 2。

## 4. 非目标

本阶段明确不实现：

- SSH、SFTP、终端解析、会话连接或文件传输业务；
- NyaTerm 的 AI、监控、同步、凭据等业务面板；
- WebView、地图、媒体播放、Windows Shell 通知；
- 触控优先的 Swipe、PullToRefresh、SemanticZoom；
- 任意窗口管理器或 Qt Private Dock 替代品；
- 旧版源码或 ABI 兼容层；
- 自动序列化任意业务页面内部状态。

## 5. 模块与依赖边界

```text
ZzCore
  ^
  |
ZzFluentFoundation
  |- ZzTabGroupId
  |- ZzScrollMarkerRole / ZzScrollMarkerKind
  |- 新增必要的颜色、尺寸 token
  ^
  |
ZzFluentUI
  |- ZzPanelStack
  |- ZzSidePane / ZzActivityBar 增强
  |- ZzSplitWorkspace
  |- ZzBottomPane
  |- ZzCommandBar
  |- ZzAnnotatedScrollBar
  |- ZzSplitButton / ZzPivot 合同增强
  ^
  |
ZzPureTools
  |- ZzWorkspaceShell 注册、协调、布局版本迁移和回滚
  ^
  |
Application Presenter / Controller
  |- 会话、终端、SFTP、监控和业务模型
```

约束：

- `ZzFluentUI` 禁止依赖 `ZzPureTools`、`ZzWindowKit` 或应用业务类型；
- `ZzWorkspaceShell` 不读取终端内容、主机地址、会话对象或 SFTP 模型；
- Example 只能消费公开 API，不得访问 private 头或重建工作区协调算法；
- 禁止链式命名空间，继续使用 `namespace ZzFluentUI {}` 等传统形式；
- 所有新增公开类、方法和复杂事务使用简体中文 Doxygen 注释。

## 6. 交付顺序

实施顺序固定为：

1. `ZzPanelStack`；
2. `ZzSidePane` 多面板模式与 `ZzActivityBar` 多激活状态；
3. `ZzSplitWorkspace`；
4. `ZzBottomPane`；
5. `ZzWorkspaceShell` 集成与布局版本 2；
6. `ZzCommandBar`；
7. `ZzAnnotatedScrollBar`；
8. `ZzSplitButton` 可切换合同和 `ZzPivot` 图标项；
9. Example 串联、安装消费、截图和性能收口。

后续批次不得越过前一批次的所有权和失败回滚测试直接实现外观。

## 7. ZzPanelStack

### 7.1 文件结构

```text
ZzFluentUI/widgets/include/ZzFluentUI/ZzPanelStack.h
ZzFluentUI/widgets/src/ZzPanelStack.cpp
ZzFluentUI/widgets/src/private/ZzPanelStackPrivate.h
ZzFluentUI/widgets/src/private/ZzPanelStackPrivate.cpp
```

采用四文件 PIMPL。Private 持有一个纵向 `QSplitter`、面板记录和固定标题框架。

### 7.2 公开接口草图

```cpp
namespace ZzFluentUI {

class ZZ_FLUENT_UI_EXPORT ZzPanelStack final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPanelStack)

public:
    explicit ZzPanelStack(QWidget *parent = nullptr);
    ~ZzPanelStack() override;

    [[nodiscard]] int panelCount() const noexcept;
    [[nodiscard]] int visiblePanelCount() const noexcept;
    [[nodiscard]] QList<QWidget *> panels() const;
    [[nodiscard]] QList<QWidget *> visiblePanels() const;

    bool addPanel(
        QWidget *content,
        const QString &title,
        const ZzIconDescriptor &icon = {});
    [[nodiscard]] QWidget *takePanel(QWidget *content);
    bool movePanel(QWidget *content, int targetIndex);

    bool setPanelVisible(QWidget *content, bool visible);
    [[nodiscard]] bool isPanelVisible(QWidget *content) const;
    bool setCurrentPanel(QWidget *content);
    [[nodiscard]] QWidget *currentPanel() const noexcept;

    bool setPanelTitle(QWidget *content, const QString &title);
    [[nodiscard]] QString panelTitle(QWidget *content) const;
    bool setPanelIconDescriptor(
        QWidget *content,
        const ZzIconDescriptor &icon);

    [[nodiscard]] QList<int> panelSizes() const;
    bool setPanelSizes(const QList<int> &sizes);

Q_SIGNALS:
    void panelVisibilityChanged(QWidget *content, bool visible);
    void currentPanelChanged(QWidget *content);
    void panelMoved(QWidget *content, int index);
    void panelCloseRequested(QWidget *content);
    void panelSizesChanged(const QList<int> &sizes);
};

} // namespace ZzFluentUI
```

### 7.3 所有权与行为

- `addPanel()` 只接受非空、GUI 线程、无父对象的 `QWidget`；成功后 Stack 接管。
- 失败不得修改 content 的父对象、可见性或当前面板。
- `takePanel()` 移除包装框架、解除 content 父对象并归还，不删除 content。
- 标题栏关闭按钮只发 `panelCloseRequested()`，不自行删除或隐藏业务页面。
- `setPanelVisible()` 控制对应包装框架；隐藏时保存最后非零高度。
- `setPanelSizes()` 只接受与当前可见面板数量一致的正整数列表。
- 面板销毁后自动移除记录、尺寸和当前指针，只发一次状态变化。
- 重复 setter 不发信号，不重建标题框架或 QSplitter。

### 7.4 实现约束

- 每个注册面板只创建一个私有 `ZzPanelFrame`；不得每次显隐重建。
- 一个 Stack 只有一个 `QSplitter`，不得为每对面板嵌套 splitter。
- 标题框架使用固定 QLabel、图标绘制和关闭 QToolButton。
- 面板标题高度、把手尺寸和间距来自 `ZzMetricToken`。
- 不使用 stylesheet，不通过 QObject 动态属性保存页面身份。
- 显隐和拖动不启用高度动画，避免连续布局和终端重绘。

## 8. ZzSidePane 与 ZzActivityBar 增强

### 8.1 ZzSidePaneMode

新增公开枚举文件：

```text
ZzFluentUI/foundation/include/ZzFluentUI/ZzSidePaneMode.h
```

```cpp
enum class ZzSidePaneMode : std::uint8_t
{
    Single,
    Stacked
};
```

`Single` 保留当前单页行为；`Stacked` 允许多个面板同时显示。默认值继续为 `Single`，
避免已有消费者无意改变布局。

### 8.2 ZzSidePane 新接口

```cpp
[[nodiscard]] ZzSidePaneMode mode() const noexcept;
void setMode(ZzSidePaneMode mode);
[[nodiscard]] ZzPanelStack *panelStack() const noexcept;
bool setWidgetVisible(QWidget *widget, bool visible);
[[nodiscard]] bool isWidgetVisible(QWidget *widget) const;
[[nodiscard]] QList<QWidget *> visibleWidgets() const;
```

现有 `addWidget()`、`takeWidget()`、`setCurrentWidget()` 继续存在并委托给固定
`ZzPanelStack`：

- Single 模式下切换当前页时隐藏其他页；
- Stacked 模式下切换当前页只确保目标可见，不关闭其他页；
- Side Pane 整体折叠时保留各子面板可见状态和高度；
- 恢复时一次性还原，不逐面板播放动画。

### 8.3 Activity Bar 多激活状态

新增：

```cpp
[[nodiscard]] bool isMultiActiveEnabled() const noexcept;
void setMultiActiveEnabled(bool enabled);
void setActiveSourceIndexes(const QModelIndexList &indexes);
[[nodiscard]] QModelIndexList activeSourceIndexes() const;
```

规则：

- `currentSourceIndex` 表示键盘焦点和最后激活入口；
- `activeSourceIndexes` 表示当前可见面板集合；
- Single 模式下 active 最多一个，并与 current 同步；
- Stacked 模式下多个入口绘制独立指示条；
- active 列表只接受当前源模型 column 0 的顶层有效索引；
- model reset、移除或销毁后清理失效索引；
- `activationRequested`、`collapseRequested` 和 `moveRequested` 仍只发意图。

当前 `ZzActivityBar::moveRequested()` 尚未由 Shell 提交，必须在本阶段接线。

## 9. ZzSplitWorkspace

### 9.1 文件结构

```text
ZzFluentUI/foundation/include/ZzFluentUI/ZzTabGroupId.h
ZzFluentUI/foundation/src/ZzTabGroupId.cpp
ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitWorkspace.h
ZzFluentUI/widgets/src/ZzSplitWorkspace.cpp
ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.h
ZzFluentUI/widgets/src/private/ZzSplitWorkspacePrivate.cpp
```

`ZzTabGroupId` 采用与 `ZzWorkspacePanelId` 相同的强值类型模式：trim 后非空、可比较、
可哈希、可注册为 metatype。自动生成值使用无花括号 UUID 字符串。

### 9.2 树结构

```text
Node
|- Branch: QSplitter + orientation + children
`- Leaf:   ZzTabGroupId + ZzTabWidget
```

规则：

- 初始始终有一个叶子；
- 分支最少两个子节点；
- 同方向相邻分支提交后扁平化，减少无意义深度；
- 删除空叶子后，单子分支自动提升其唯一子节点；
- 最后一个叶子永不自动删除；
- 最多 64 个叶子，最大树深 16；
- 所有叶子只使用已有 `ZzTabWidget`，不复制标签页实现。

### 9.3 公开接口草图

```cpp
namespace ZzFluentUI {

enum class ZzSplitPlacement : std::uint8_t
{
    Before,
    After
};

enum class ZzWorkspaceDropZone : std::uint8_t
{
    Center,
    Left,
    Top,
    Right,
    Bottom
};

class ZZ_FLUENT_UI_EXPORT ZzSplitWorkspace final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSplitWorkspace)

public:
    explicit ZzSplitWorkspace(QWidget *parent = nullptr);
    ~ZzSplitWorkspace() override;

    [[nodiscard]] QList<ZzTabGroupId> groupIds() const;
    [[nodiscard]] ZzTabGroupId activeGroupId() const;
    bool setActiveGroup(const ZzTabGroupId &id);
    [[nodiscard]] ZzTabWidget *tabWidget(
        const ZzTabGroupId &id) const noexcept;
    [[nodiscard]] ZzTabGroupId groupId(
        const ZzTabWidget *tabs) const;

    [[nodiscard]] std::optional<ZzTabGroupId> splitGroup(
        const ZzTabGroupId &source,
        Qt::Orientation orientation,
        ZzSplitPlacement placement,
        const ZzTabGroupId &requestedId = {});
    bool removeEmptyGroup(const ZzTabGroupId &id);

    bool transferTab(
        const ZzTabGroupId &source,
        int sourceIndex,
        const ZzTabGroupId &target,
        int targetIndex = -1);
    bool moveTabToDropZone(
        const ZzTabGroupId &source,
        int sourceIndex,
        const ZzTabGroupId &target,
        ZzWorkspaceDropZone zone);

    bool setPageLayoutKey(QWidget *page, const QString &key);
    [[nodiscard]] QString pageLayoutKey(QWidget *page) const;
    [[nodiscard]] ZzTabGroupId savedGroupForPageKey(
        const QString &key) const;

    [[nodiscard]] QByteArray saveLayout() const;
    bool restoreLayout(const QByteArray &state);

Q_SIGNALS:
    void activeGroupChanged(const ZzTabGroupId &id);
    void groupAdded(const ZzTabGroupId &id);
    void groupAboutToBeRemoved(const ZzTabGroupId &id);
    void layoutChanged();
    void tabDropCommitted(
        QWidget *page,
        const ZzTabGroupId &source,
        const ZzTabGroupId &target);
};

} // namespace ZzFluentUI
```

### 9.4 页面布局键

组件不能序列化业务页面本身。应用可为页面设置可选、稳定、唯一的布局键：

- 最大 256 个 UTF-16 code unit；
- 空值表示不参与跨启动页面位置恢复；
- 重复键设置失败且不修改旧状态；
- 布局保存记录 keyed page 的组、顺序和当前状态；
- 恢复时缺失页面不会导致结构恢复失败；
- `savedGroupForPageKey()` 允许应用重建页面后放入原组；
- 已存在的 keyed page 在恢复事务中通过 `transferTabTo()` 移动；
- unkeyed page 在结构恢复时必须保留，无法保留原叶子时移入活动叶子。

页面布局键只用于 UI 布局身份，不允许保存主机地址、凭据或业务对象指针。

### 9.5 拖放

拖放覆盖层使用一个持久、按需显示的子 QWidget，不为每个叶子创建覆盖层。

每个目标组有五个区域：

- Center：转移到现有组；
- Left/Right：创建水平分支；
- Top/Bottom：创建垂直分支。

事务顺序固定为：

1. 验证进程内随机令牌、来源容器、页面和 sourceIndex；
2. 计算目标组和 DropZone；
3. 边缘区域暂建空叶子，不删除来源组；
4. 调用 `ZzTabWidget::transferTabTo()`；
5. 成功后移除空来源组并扁平化分支；
6. 更新活动组并发出最终信号；
7. 失败时删除临时叶子并恢复原比例。

不得截图页面、复制 QWidget、创建临时顶层窗口或在 Drop 之前移除来源页面。

### 9.6 键盘与无障碍

- 每个叶子继续暴露 QTabWidget/QTabBar 原生可访问结构；
- SplitWorkspace 提供按方向聚焦相邻组的内部算法，后续可由 KeyBinder 绑定；
- 焦点进入组后记为 active，不自动改变业务当前会话；
- Drop overlay 不接受焦点，不进入无障碍树；
- QSplitter handle 保留 Qt 原生键盘和无障碍行为；
- RTL 镜像 Left/Right 的视觉 DropZone，但物理组顺序按 QSplitter 真实几何保存。

## 10. ZzBottomPane

### 10.1 文件结构

```text
ZzFluentUI/widgets/include/ZzFluentUI/ZzBottomPane.h
ZzFluentUI/widgets/src/ZzBottomPane.cpp
ZzFluentUI/widgets/src/private/ZzBottomPanePrivate.h
ZzFluentUI/widgets/src/private/ZzBottomPanePrivate.cpp
```

### 10.2 组成和合同

`ZzBottomPane` 固定包含：

1. 顶部 4 逻辑像素高度调整把手；
2. 一个轻量工具切换栏，复用 `ZzPivot`；
3. 一个 `QStackedWidget`；
4. 当前工具关闭/折叠按钮。

公开能力至少包含：

```cpp
bool addWidget(
    QWidget *widget,
    const QString &title,
    const ZzIconDescriptor &icon = {});
[[nodiscard]] QWidget *takeWidget(QWidget *widget);
bool setCurrentWidget(QWidget *widget);
[[nodiscard]] QWidget *currentWidget() const noexcept;
void setCollapsed(bool collapsed);
[[nodiscard]] bool isCollapsed() const noexcept;
void setPaneHeight(int height);
[[nodiscard]] int paneHeight() const noexcept;
void setMinimumPaneHeight(int height);
void setMaximumPaneHeight(int height);
[[nodiscard]] int lastExpandedHeight() const noexcept;
```

BottomPane 同时只显示一个工具。折叠保留当前工具和最后高度。它不支持浮动；需要浮动的
工具继续注册为 `ZzDockPanel`。

## 11. ZzWorkspaceShell 集成

### 11.1 新工作区对象树

```text
workspaceRoot / QHBoxLayout
|- leftActivityBar
|- leftSidePane(mode = Stacked)
|- centerHost / QVBoxLayout
|  |- splitWorkspace (stretch = 1)
|  `- bottomPane
|- rightSidePane(mode = Stacked)
|- rightActivityBar
`- commandPalette (overlay child)
```

### 11.2 新公开入口

```cpp
[[nodiscard]] ZzFluentUI::ZzSplitWorkspace *splitWorkspace() const noexcept;
[[nodiscard]] ZzFluentUI::ZzBottomPane *bottomPane() const noexcept;

[[nodiscard]] ZzCore::ZzResult<void> registerBottomPanel(
    const ZzWorkspacePanelId &id,
    const QString &title,
    ZzFluentUI::ZzIconDescriptor icon,
    QWidget *content);
```

现有 `tabWidget()` 保留，但语义改为返回当前 active group 的 `ZzTabWidget`。公开注释必须
明确该返回指针可能随 active group 改变；需要固定组的调用方应使用 `splitWorkspace()`。

### 11.3 面板记录

`ZzPanelKind` 扩充为 Side、Bottom、Dock。Side 记录新增：

- 是否可见；
- 在 PanelStack 中的顺序；
- 最近非零高度；
- ActivityArea；
- content 与身份观察值。

Shell 连接 `ZzActivityBar::moveRequested()`：

1. 解析 source index 对应 PanelId；
2. 验证目标 area 和 target row；
3. 同侧只更新入口顺序与分组；
4. 跨侧时从来源 SidePane take，再添加到目标 SidePane；
5. 保留原可见状态和可恢复高度；
6. 目标接管失败时重新放回来源并恢复模型；
7. 成功后再更新 Activity model 和多激活索引。

### 11.4 标题同步

标题来源从单一 `tabs` 改为：

```text
splitWorkspace.activeGroupId()
  -> tabWidget(groupId)->currentWidget()
  -> 页面 windowTitle / tabText
  -> ZzWorkspaceTitleMode
  -> host + ZzFluentTitleBar
```

切换 active group、当前 tab 或当前页面标题时刷新。标题栏仍不得反向查找页面或业务模型。

## 12. 工作区布局版本 2

### 12.1 版本常量分离

当前实现把外层工作区 schema 和 `QMainWindow::saveState()` 版本共用同一个常量。
版本 2 必须拆分：

```cpp
constexpr quint16 zzWorkspaceEnvelopeVersion = 2;
constexpr int zzQtMainWindowStateVersion = 1;
constexpr auto zzLayoutStreamVersion = QDataStream::Qt_6_8;
```

新增分屏不能直接把 2 传给旧 `restoreState()`，否则版本 1 Dock 数据会被拒绝。

### 12.2 版本 2 负载

```text
qtState
left:  collapsed, width, visible PanelId[], sizes[], current PanelId
right: collapsed, width, visible PanelId[], sizes[], current PanelId
sideEntries: PanelId, ActivityArea, order
splitWorkspaceState
bottom: collapsed, height, current PanelId
titleMode
```

继续保留：

- `ZZWS` magic；
- QDataStream Qt 6.8 格式；
- 1 MiB 总上限；
- payload 长度；
- SHA-256 摘要。

新增解码限制：

- 每侧最多 32 个可见面板；
- 总注册侧面板布局项不得超过 4096；
- SplitWorkspace 最多 64 个组、深度最多 16；
- PanelId、GroupId 和 page key 长度有界；
- 同一集合禁止重复 ID；
- sizes 必须与 visible ID 数量一致且为正数；
- 所有枚举先验证范围再转换。

### 12.3 版本 1 迁移

读取版本 1 时：

- `leftCurrent` 变为左侧唯一可见面板；
- `rightCurrent` 变为右侧唯一可见面板；
- 左右宽度和折叠状态保持；
- 单一 TabWidget 变为 SplitWorkspace 初始根组；
- `currentTabIndex` 应用于根组；
- BottomPane 默认为折叠；
- Qt Dock state 仍以版本 1 恢复；
- 下次保存写出版本 2，不回写版本 1。

### 12.4 恢复事务

恢复前必须完整解码和静态验证，不得边读边修改 UI。提交顺序：

1. 捕获当前 Qt Dock、PanelStack、SplitWorkspace、BottomPane 和标题状态；
2. 恢复 Qt Dock；
3. 恢复 SplitWorkspace；
4. 恢复左右面板位置、可见集合和尺寸；
5. 恢复 BottomPane；
6. 同步 Activity active 集合和标题；
7. 任一步失败则按反向顺序恢复快照。

若回滚本身失败，返回包含明确上下文的 `InvalidState`，不得继续宣称当前布局有效。

## 13. ZzCommandBar

### 13.1 独立价值

标准 `QToolBar` 已完整提供 docking、QAction、平台菜单和普通 overflow，因此
`ZzCommandBar` 不能是换皮空派生。它新增的独立合同是：

- 主命令和次命令分组；
- 次命令固定进入更多菜单；
- 主命令按宽度从尾部迁入更多菜单；
- Expanded、Compact、Auto 三种统一标签策略；
- 不克隆 QAction，不维护第二套 checked/enabled/shortcut 状态。

### 13.2 组成

采用四文件 PIMPL，公开类继承 `QWidget`。Private 固定拥有：

- 一个不可移动、不可浮动的内部 `QToolBar`；
- 一个更多 `QToolButton`；
- 一个 `QMenu`；
- 主/次 QAction 的 `QPointer` 有序列表；
- 缓存后的显示模式和可见主命令数量。

### 13.3 公开接口草图

```cpp
enum class ZzCommandBarDisplayMode : std::uint8_t
{
    Auto,
    Compact,
    Expanded
};

class ZZ_FLUENT_UI_EXPORT ZzCommandBar final : public QWidget
{
    Q_OBJECT

public:
    explicit ZzCommandBar(QWidget *parent = nullptr);
    ~ZzCommandBar() override;

    bool insertPrimaryAction(int index, QAction *action);
    bool insertSecondaryAction(int index, QAction *action);
    void addPrimaryAction(QAction *action);
    void addSecondaryAction(QAction *action);
    [[nodiscard]] QAction *addPrimaryAction(
        const QIcon &icon,
        const QString &text);
    [[nodiscard]] QAction *addSecondaryAction(
        const QIcon &icon,
        const QString &text);
    bool removeAction(QAction *action);

    [[nodiscard]] QList<QAction *> primaryActions() const;
    [[nodiscard]] QList<QAction *> secondaryActions() const;
    [[nodiscard]] ZzCommandBarDisplayMode displayMode() const noexcept;
    void setDisplayMode(ZzCommandBarDisplayMode mode);
    [[nodiscard]] int visiblePrimaryActionCount() const noexcept;

Q_SIGNALS:
    void displayModeChanged(ZzCommandBarDisplayMode mode);
    void visiblePrimaryActionCountChanged(int count);
    void actionTriggered(QAction *action);
};
```

### 13.4 自适应算法

仅在宽度、字体、主题、布局方向或 QAction 展示数据变化时重新计算，paint 不计算布局：

1. Auto 先测量全部 Expanded 主命令；
2. 不适配时测量全部 Compact 主命令；
3. 仍不适配时从逻辑尾部迁入 overflow，直到更多按钮可见且布局合法；
4. Expanded 强制文字在图标旁，但仍允许尾部 overflow；
5. Compact 强制 icon-only，但仍保留 tooltip 和 accessible name；
6. RTL 只改变视觉尾部和排列，不改变 primaryActions 的逻辑顺序。

同一个 QAction 在内部 QToolBar 和 QMenu 间转移，不复制。外部 QAction 不转移所有权；
便利重载创建的 QAction 由 CommandBar 拥有。

## 14. ZzAnnotatedScrollBar

### 14.1 继承边界

将 `ZzScrollBar final` 改为可继承的 `ZzScrollBar`，不改变现有构造、PIMPL 或动画合同。
新增 `ZzAnnotatedScrollBar final : public ZzScrollBar`，因此仍被 `ZzFluentStyle` 识别并
复用单一悬停动画。

### 14.2 标记模型

新增：

```text
ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerRole.h
ZzFluentUI/foundation/include/ZzFluentUI/ZzScrollMarkerKind.h
```

角色：

| 角色 | 类型 | 语义 |
|---|---|---|
| `Qt::DisplayRole` | QString | 可选简短标签 |
| `Qt::ToolTipRole` | QString | 鼠标悬停说明 |
| `Qt::AccessibleTextRole` | QString | 可选完整无障碍文本 |
| `ZzScrollMarkerRole::Position` | qreal | 归一化位置 `[0.0, 1.0]` |
| `ZzScrollMarkerRole::Kind` | ZzScrollMarkerKind | Information/Success/Warning/Error/Bookmark/SearchMatch/Custom |
| `ZzScrollMarkerRole::Color` | QColor | 仅 Custom 使用 |
| `ZzScrollMarkerRole::Priority` | int | 同一像素碰撞时的可选优先级 |

非法、NaN、无穷或越界 Position 被忽略，不修改外部模型。

### 14.3 公开接口

```cpp
void setMarkerModel(QAbstractItemModel *model);
[[nodiscard]] QAbstractItemModel *markerModel() const noexcept;
void setMarkersInteractive(bool interactive);
[[nodiscard]] bool markersInteractive() const noexcept;
[[nodiscard]] QModelIndex markerAt(const QPoint &position) const;

Q_SIGNALS:
    void markerModelChanged(QAbstractItemModel *model);
    void markersInteractiveChanged(bool interactive);
    void markerActivated(const QModelIndex &sourceIndex);
```

模型是非拥有、平面的 QAbstractItemModel。模型销毁后自动清空标记并发一次
`markerModelChanged(nullptr)`。

### 14.4 绘制与交互

- 不为标记创建 QWidget、QObject、动画或 timer；
- 数据变化后缓存规范化轻量记录；
- 几何缓存按滚动条可用像素桶聚合，同像素按 Priority 和 Kind 合并；
- paint 只遍历像素桶，不逐次读取整个模型；
- 点击标记时将归一化位置映射到当前 QScrollBar range，设置 value，再发源索引；
- 未命中标记时完整委托 QScrollBar 的轨道、滑块、键盘和上下文菜单；
- ToolTip 使用源模型 `Qt::ToolTipRole`；
- 同时支持水平和垂直方向；
- HighContrast 使用现有 Information/Success/Warning/Error/Accent token，Custom 颜色必须经过
  最小对比度修正或回退到 Information。

## 15. ZzSplitButton 与 ZzPivot

### 15.1 SplitButton

不新增 ToggleSplitButton 类，也不新增 checked 属性或信号。继续使用 QPushButton 的：

```cpp
setCheckable(bool)
setChecked(bool)
isChecked()
toggled(bool)
clicked(bool)
```

合同：

- 主区鼠标点击、Space、Enter/Return 按 Qt 原生状态机切换 checked；
- 菜单区点击、Down、Alt+Down 只打开菜单，绝不切换 checked；
- checked 绘制不覆盖 Accent/Subtle 的可访问对比度；
- disabled、RTL、菜单同步组装和外部菜单销毁行为保持。

### 15.2 Pivot

不新增 `ZzSelectorBar`。补充明确的图标项便利 API：

```cpp
int addItem(const QIcon &icon, const QString &text);
int insertItem(int index, const QIcon &icon, const QString &text);
[[nodiscard]] QIcon itemIcon(int index) const;
void setItemIcon(int index, const QIcon &icon);
```

绘制继续委托 `CE_TabBarTabLabel`，保留 QTabBar 的 overflow、RTL、键盘和无障碍语义。
指示条不得与图标或文字重叠。

## 16. 视觉与主题约束

新增或审核以下度量 token：

- PanelHeaderHeight；
- PanelSplitterExtent；
- WorkspaceDropTargetExtent；
- BottomPaneHeaderHeight；
- CommandBarHeight；
- CommandBarMoreExtent；
- AnnotatedScrollBarExtent；
- ScrollMarkerThickness。

规则：

- 所有尺寸为设备无关逻辑像素，由 Snapshot O(1) 读取；
- 不按 viewport 宽度缩放字体；
- 指示条、文字、图标、焦点环不能重叠；
- Light、Dark、HighContrast 均使用 token，不内嵌主题颜色；
- 100%、125%、150%、200% DPR 下物理单像素线必须对齐；
- Reduced Motion 时立即到终态，不保留运行中动画；
- PanelStack、SplitWorkspace 和 BottomPane 默认不使用布局动画；
- 拖放覆盖层是功能反馈，不使用渐变、模糊或截图背景。

## 17. 性能预算

### 17.1 结构预算

| 组件 | 预算 |
|---|---|
| ZzPanelStack | 每侧最多 32 个可见面板；每面板一个固定框架；无 timer/animation |
| ZzSplitWorkspace | 最多 64 个组、树深最多 16；一个持久 Drop overlay |
| ZzBottomPane | 一个 Pivot、一个 QStackedWidget、一个把手；无后台唤醒 |
| ZzCommandBar | 一个 QToolBar、一个 QToolButton、一个 QMenu；不克隆 QAction |
| ZzAnnotatedScrollBar | 标记数量不改变 QObject 数；paint 遍历像素桶 |

### 17.2 参考机门禁

Linux 活动发布参考机继续使用已登记的 `local-release-xvfb` 环境和
`linux-gcc-reference` preset。所有结果采集三轮，记录 P50、P95、max、对象数、
动画数、timer 数和原始环境。

正式门禁：

- 组合工作区 120 帧离屏渲染 P95 不超过 12 ms；
- 结构操作 P95 不超过 16.7 ms；
- 4 组和 32 组、20 标记和 100000 标记的稳定 paint 复杂度比不超过 2.0；
- 1000 次显隐、分割、合并、主题切换后 QObject 数回到预期稳定值；
- 隐藏页面无运行中动画、timer 或持续 update；
- 性能失败不得通过提高既有阈值解决，必须先定位热路径。

这些数字是控件渲染和结构操作预算，不表述为窗口合成器帧率。

## 18. 测试设计

### 18.1 新增定向测试

```text
ZzFluentUI/tests/ZzPanelStackTest.cpp
ZzFluentUI/tests/ZzSplitWorkspaceTest.cpp
ZzFluentUI/tests/ZzBottomPaneTest.cpp
ZzFluentUI/tests/ZzCommandBarTest.cpp
ZzFluentUI/tests/ZzAnnotatedScrollBarTest.cpp
```

增强：

```text
ZzFluentUI/tests/ZzActivityBarTest.cpp
ZzFluentUI/tests/ZzSidePaneTest.cpp
ZzFluentUI/tests/ZzSplitButtonTest.cpp
ZzFluentUI/tests/ZzPivotTest.cpp
ZzPureTools/tests/ZzWorkspaceShellTest.cpp
ZzPureTools/tests/ZzWorkspaceScreenshotTest.cpp
```

### 18.2 必测场景

PanelStack：

- 所有权、take、外部销毁；
- 多个同时可见、独立显隐和尺寸恢复；
- 非法 sizes、重复页面和销毁重入；
- 1000 次显隐后对象数稳定。

SplitWorkspace：

- 水平/垂直分割、扁平化和空组收敛；
- center/left/top/right/bottom 拖放；
- transfer 中来源、目标或页面同步销毁；
- 第三方同步接管页面时不强行取回；
- 最大组数、最大深度和损坏布局拒绝；
- page key 唯一、缺失页面和 unkeyed 页面保留；
- RTL、键盘焦点和可访问 TabGroup。

WorkspaceShell：

- 左右多个可见面板和 Activity 多激活同步；
- Activity 入口同组重排、跨组和跨侧迁移；
- Bottom/Dock/Side 三类 PanelId 唯一；
- active group 标题同步；
- 版本 1 到版本 2 迁移；
- 版本 2 round trip、摘要错误、截断、重复 ID 和回滚失败路径。

CommandBar：

- 主次顺序、外部 QAction 借用和便利 QAction 所有权；
- Auto/Compact/Expanded 阈值；
- overflow 后 checked、enabled、shortcut 和 menu 状态一致；
- RTL、键盘、无障碍和 action 外部销毁；
- action 数量增加不增加自定义 paint 复杂度。

AnnotatedScrollBar：

- model reset、insert/remove/dataChanged 和销毁；
- 0、1、100000 标记；
- 非法 position、碰撞优先级和 Custom 颜色；
- 水平/垂直、RTL、DPR、tooltip、点击和普通轨道委托；
- 标记数量变化时 QObject 数固定。

### 18.3 截图

覆盖 Light、Dark、HighContrast 和 DPR 100/125/150/200：

- 双侧多面板工作台；
- 窄窗口折叠；
- 两组和四组分屏；
- 五区 Drop overlay；
- BottomPane 展开和折叠；
- CommandBar expanded、compact、overflow；
- AnnotatedScrollBar 多种语义标记。

截图测试只负责稳定视觉，不替代鼠标、键盘、所有权和性能断言。

### 18.4 安装与架构消费

必须更新：

- `tests/InstallConsumer/Gui/main.cpp`；
- `tests/InstallConsumer/CMakeLists.txt`；
- `tests/PublicHeaderConsumer/CMakeLists.txt`；
- `tests/Architecture/ZzArchitectureAudit.cmake`；
- `tests/Platform/ZzPerformanceThresholdContract.cmake`；
- `tests/Platform/PresetMatrixContract.cmake`。

安装后的 shared/static 包应能仅使用公开头创建最小 Workbench、分割标签、注册三个面板、
创建 CommandBar 和 AnnotatedScrollBar。ArchitectureAudit 继续检查：

- PIMPL；
- 文件名与类名；
- 中文 Doxygen；
- 禁止链式命名空间；
- 禁止 Qt Private；
- 禁止 stylesheet；
- 禁止业务词泄漏到通用组件。

## 19. 跨平台策略

### 19.1 Linux

本机动态验收使用现有 Qt 6.11.1，不重新下载 Qt。每个批次至少通过：

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2
ctest --preset linux-gcc-debug --output-on-failure

cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release --parallel 2
ctest --preset linux-gcc-release --output-on-failure

cmake --preset linux-static-release
cmake --build --preset linux-static-release --parallel 2
ctest --preset linux-static-release --output-on-failure

cmake --preset linux-clang-release
cmake --build --preset linux-clang-release --parallel 2
ctest --preset linux-clang-release --output-on-failure

cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan --parallel 2
ctest --preset linux-clang-asan --output-on-failure

cmake --preset linux-clang-tidy-release
cmake --build --preset linux-clang-tidy-release --parallel 2
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy
```

最终收口运行 `scripts/ci/run-linux-gates.sh` 和物理桌面验收。

### 19.2 Windows

必须保持：

- `windows-msvc2022-release`；
- `windows-msvc2022-static`；
- `windows-mingw-release`；
- `windows-mingw-static`。

MSVC 保持 `/utf-8`、warnings-as-errors 和 `/analyze`；MinGW 必须使用与 Qt SDK 匹配的
工具链。人工物理机重点验证：

- 标签五区拖放；
- 多面板上下调整；
- 125%、150%、200% DPI；
- 静态 Example 启动；
- shared Example 加载正确 Qt DLL；
- 菜单、IME、焦点和窗口标题栏交互。

没有物理机证据时文档只能写“待 Windows 验证”，不得写“Windows 已通过”。

### 19.3 macOS

保持 AppleClang arm64/x86_64、shared/static preset 和公共 Qt API 静态检查。不得为本阶段
增加 Objective-C++ 或原生 Cocoa 布局代码。物理机可用后补：

- 触控板与拖放；
- 系统菜单栏；
- 全屏和窗口切换；
- Retina DPR；
- Intel/Apple Silicon 安装包运行。

没有物理机证据时如实记录“静态合同通过，运行待验证”。

## 20. 提交与证据规则

- 每个逻辑批次完成验证后立即提交；
- commit 首行使用中文简述；
- commit 正文使用中文分段说明代码、测试、性能和平台状态；
- 不提交 `temp_image/`、构建目录、视觉伴侣临时文件或下载审计仓库；
- 不提高性能阈值掩盖回归；
- 不把 CI 未运行表述为通过；
- 不 push，除非用户明确要求；
- 推送前确认远端无分叉且 HEAD 与目标提交一致。

## 21. 完成定义

本设计全部完成需要同时满足：

1. IDE Workbench 骨架由公开组件提供，Example 不重建协调逻辑；
2. 左右同侧多面板可同时打开、调整和迁移；
3. 中央标签可在多个组间转移并向四边创建分屏；
4. BottomPane 和 DockPanel 职责清晰且均可独立消费；
5. 版本 1 布局可安全迁移到版本 2；
6. CommandBar 和 AnnotatedScrollBar 在真实工作区场景中使用；
7. SplitButton 可切换合同和 Pivot 图标项通过测试；
8. Linux 完整动态门禁、截图、安装消费和参考机性能通过；
9. Windows MSVC/MinGW 与 macOS 的真实证据边界准确记录；
10. 所有新增公开类满足命名、PIMPL、中文 Doxygen 和模块依赖约束。

## 22. 后续计划文档要求

实现计划必须按上述交付顺序拆分为可独立验证和提交的任务。每个任务至少写明：

- 精确新增/修改文件；
- 先写的失败测试；
- 最小实现范围；
- 定向构建和 CTest 命令；
- 所有权与失败回滚断言；
- 性能或对象稳定性断言；
- shared/static 与安装消费影响；
- 中文 commit 标题和正文要点。

正式实施计划在本规格经用户审查后另行生成。
