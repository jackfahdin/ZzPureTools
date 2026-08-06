# ZzFluentUI 高性能导航展示增强实施计划

**状态：** 已于 2026-08-06 完成生产实现、本机质量门禁与阶段 10 旧组件覆盖闭环。Windows MSVC、Windows Qt SDK MinGW 与 macOS 仍等待对应原生工具链和真机验证。

**目标：** 补齐应用框架导航图标、静态分区、固定 footer、badge、自适应 regular/compact 和路由选择同步能力，同时保持强类型路由、多窗口隔离、Qt Model/View、无障碍与固定热路径复杂度。

**架构：** Fluent Foundation 定义不依赖 Widgets 的展示 role 与枚举；`ZzNavigationView` 继续是只消费单个列表模型的基础 view；新增 `ZzNavigationPane` 以两个固定 view 和两个固定投影模型组合主导航与 footer。`ZzNavigationModel` 仍只保存拥有值的展示节点，`ZzNavigationController` 仍独占路由与历史，`ZzApplicationWindow` 只在 composition root 连接索引意图、强类型路由和当前选择。

**技术约束：** Qt 6.8+、C++20、四文件 Pimpl、简体中文 Doxygen、传统命名空间、无 Qt Private API、无 QSS、无动态属性、无每项 QWidget、无平台原生头、无业务对象、无内部 timer、无宽度过渡动画。

## 1. 前置结论

阶段 10 旧组件闭环审计记录在 `2026-08-06-zzfluentui-stage10-legacy-coverage-audit.md`。本批只处理其中确认的导航展示缺口，不恢复旧 `ZzNavigationBar`：

- 当前 `ZzNavigationModel` 在 `ZzNavigationRole::Icon` 返回 `ZzIconDescriptor`，但 `ZzFluentItemDelegate` 只读取 Qt 标准 decoration，应用框架导航图标不会显示。
- 当前 `ZzNavigationView` 只有一个平面列表和手动 compact bool，无法表达置底项、分区标题、badge 或基于窗口可用宽度的自适应模式。
- `ZzApplicationWindowPrivate` 在 controller 首次导航、回退或程序化导航后没有按 route 同步 view selection。
- 旧版树、搜索、页面 stack、新窗口和 route list 已有更清晰的新架构替代，不能借导航视觉增强回流 UI。

## 2. 本批范围

### 2.1 实现

- 公共 `ZzNavigationItemRole`：Icon、Section、Placement、Badge。
- 公共 `ZzNavigationPlacement`：Primary、Footer。
- 公共 `ZzNavigationDisplayMode`：Regular、Compact、Adaptive。
- `ZzNavigationView` 识别上述展示 role，绘制 descriptor/QIcon、标题、短 badge、section 标题行、选择、hover、focus、disabled、RTL 和三主题。
- 新增 `ZzNavigationPane`，消费一个外部平面 `QAbstractItemModel`，投影为主列表和固定 footer。
- section role 表示“在当前目标项前插入一个标题行”；标题行不可选择、不可激活、没有 route。
- adaptive 只观察所属顶层 QWidget 的逻辑宽度，并同步切换 240/48 逻辑像素；不使用平台私有 API。
- compact 行保留 display/tooltip/accessibility 语义，视觉上隐藏普通文本并保留图标与 badge 提示。
- `ZzNavigationNode` 增加可翻译 section、badge 和 placement 值；旧四字段 aggregate 初始化保持有效。
- `ZzNavigationModel` 增加 O(1) route 查找和 badge 局部更新，不以 reset 更新 badge。
- `ZzApplicationWindow` 使用 pane，并在 controller route 或 model reset 后按强类型 route 同步选择。

### 2.2 不实现

- 动态树、expander、任意层级、拖拽重排或运行时节点增删 API。
- 内置搜索、模糊匹配、网络查询、历史、持久化或应用命令。
- 页面 factory、page stack、back stack、新窗口创建或窗口计数。
- 每项 widget、delegate、animation、timer、menu 或 proxy model。
- regular/compact 过渡动画；宽度是布局状态，切换时同步终态。
- 旧字符串 node key、裸节点指针或 `ZzNavigationBar` 兼容层。

## 3. 公共 Foundation 契约

### 3.1 `ZzNavigationItemRole`

新增 `ZzFluentUI/foundation/include/ZzFluentUI/ZzNavigationItemRole.h`：

```cpp
#pragma once

#include <QtCore/Qt>

namespace ZzFluentUI {

/** @brief 定义与业务路由无关的导航展示模型角色。 */
enum class ZzNavigationItemRole : int
{
    /** @brief 返回 ZzIconDescriptor。 */
    Icon = Qt::UserRole + 0x100,
    /** @brief 返回在当前项前显示的可选 QString 分区标题。 */
    Section,
    /** @brief 返回 ZzNavigationPlacement。 */
    Placement,
    /** @brief 返回可选的短 QString 徽标文本。 */
    Badge
};

} // namespace ZzFluentUI
```

使用高于普通应用 role 的固定区间，避免与 `ZzPureTools::ZzNavigationRole::Route` 冲突。FluentUI 不读取 route role。

### 3.2 `ZzNavigationPlacement`

新增 `ZzFluentUI/foundation/include/ZzFluentUI/ZzNavigationPlacement.h`：

```cpp
#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定导航目标位于主滚动区或固定页脚区。 */
enum class ZzNavigationPlacement : std::uint8_t
{
    Primary,
    Footer
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzNavigationPlacement)
```

缺少或无法转换的 Placement role 一律按 Primary 处理。footer 投影最多同时展示六行，更多行保留在标准滚动 view 中，不增加可见对象数量。

### 3.3 `ZzNavigationDisplayMode`

新增 `ZzFluentUI/foundation/include/ZzFluentUI/ZzNavigationDisplayMode.h`：

```cpp
#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 指定导航面板的常规、紧凑或自适应展示策略。 */
enum class ZzNavigationDisplayMode : std::uint8_t
{
    Regular,
    Compact,
    Adaptive
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzNavigationDisplayMode)
```

Foundation 头只能依赖 Qt Core。三个头分别包含同名主类型，满足公开头命名和独立编译门禁。

## 4. `ZzNavigationPane` 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzNavigationPane.h`：

```cpp
#pragma once

#include <memory>

#include <QtCore/QModelIndex>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzNavigationDisplayMode.h>

class QAbstractItemModel;
class QEvent;

namespace ZzFluentUI {

class ZzNavigationPanePrivate;

/**
 * @brief 将一个平面展示模型投影为主导航、分区和固定页脚的 Fluent 面板。
 *
 * model 是非拥有观察值；控件不创建页面、不执行路由，也不访问业务对象。
 * 全部方法只能在 GUI 线程调用。
 */
class ZZ_FLUENT_UI_EXPORT ZzNavigationPane final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzNavigationPane)
    Q_PROPERTY(
        ZzFluentUI::ZzNavigationDisplayMode displayMode
        READ displayMode
        WRITE setDisplayMode
        NOTIFY displayModeChanged)
    Q_PROPERTY(
        bool compact
        READ isCompact
        NOTIFY effectiveCompactChanged)
    Q_PROPERTY(
        int adaptiveThreshold
        READ adaptiveThreshold
        WRITE setAdaptiveThreshold
        NOTIFY adaptiveThresholdChanged)

public:
    explicit ZzNavigationPane(QWidget *parent = nullptr);
    ~ZzNavigationPane() override;

    void setModel(QAbstractItemModel *model);
    [[nodiscard]] QAbstractItemModel *model() const noexcept;

    void setDisplayMode(ZzNavigationDisplayMode mode);
    [[nodiscard]] ZzNavigationDisplayMode displayMode() const noexcept;
    [[nodiscard]] bool isCompact() const noexcept;

    void setAdaptiveThreshold(int logicalWidth);
    [[nodiscard]] int adaptiveThreshold() const noexcept;

    void setCurrentSourceIndex(const QModelIndex &index);
    [[nodiscard]] QModelIndex currentSourceIndex() const;

Q_SIGNALS:
    void modelChanged(QAbstractItemModel *model);
    void displayModeChanged(ZzNavigationDisplayMode mode);
    void effectiveCompactChanged(bool compact);
    void adaptiveThresholdChanged(int logicalWidth);
    void navigationRequested(const QModelIndex &sourceIndex);

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::unique_ptr<ZzNavigationPanePrivate> d_ptr;
};

} // namespace ZzFluentUI
```

契约：

- 默认模式 Adaptive，默认阈值 900，顶层宽度小于阈值时 effective compact。
- 未显示或没有所属顶层 widget 时，Adaptive 暂按 Regular；Show、ParentChange 和顶层 Resize 后重算。
- Regular 固定宽度 240，Compact 固定宽度 48；不按 viewport 比例缩放字体或宽度。
- `setAdaptiveThreshold()` 把值收敛到 `[480, 4096]`，同值 no-op。
- `setModel()` 不接管 model；model 销毁后两个投影清空、当前索引失效并发出一次 `modelChanged(nullptr)`。
- `setCurrentSourceIndex()` 只接受当前 source model 的 column 0 顶层索引；无效输入清除两个 view 的 selection。
- 主区和 footer 选择互斥；用户激活时先同步唯一选择，再发出 source index。
- section 标题行永不发出 `navigationRequested`。

## 5. 私有投影与复杂度

新增：

```text
ZzFluentUI/widgets/src/ZzNavigationPane.cpp
ZzFluentUI/widgets/src/private/ZzNavigationPanePrivate.h
ZzFluentUI/widgets/src/private/ZzNavigationPanePrivate.cpp
ZzFluentUI/widgets/src/private/ZzNavigationProjectionModel.h
ZzFluentUI/widgets/src/private/ZzNavigationProjectionModel.cpp
```

`ZzNavigationProjectionModel` 是 private `QAbstractProxyModel`，构造时固定为 Primary 或 Footer：

- Primary 只投影 placement=Primary 的 source row；非空 Section role 在对应目标项前插入一个 synthetic header row。
- Footer 只投影 placement=Footer，不插入 section。
- 每个投影保存连续 entry vector 和 source-row-to-proxy-row vector；`mapToSource()`、`mapFromSource()`、`data()` 和 `flags()` 为 O(1)。
- section synthetic row 的 `DisplayRole` 为 section 文本，flags 不含 selectable/enabled，内部 private role 标识 header。
- source set/reset、rows inserted/removed/moved、layout changed，以及 Section/Placement role 变化时允许 O(N) 重建映射。这些是结构冷路径。
- 普通 Display/Icon/Badge/ToolTip/Accessible role 变化不得 reset；只把投影可见范围标记 `dataChanged`，selection 和 scroll position保持。
- paint、sizeHint、hover、selection、activation 和滚动不得调用 source `rowCount()` 或扫描 entry vector。
- 两个 projection、两个 view 和一个 layout 是每个 pane 的固定对象；不按节点创建 QObject。

footer view 高度为 `min(rowCount, 6) * itemHeight`，零项时隐藏。超出六项使用标准滚动条，不扩大面板或创建额外 view。

## 6. 导航 item 绘制

`ZzNavigationViewPrivate` 安装一个 private navigation delegate：

- 没有 Icon/Badge 且不是 synthetic section 时，调用现有 `ZzFluentItemDelegate`，保证旧普通列表视觉和行为不变化。
- Icon role 优先读取 `ZzIconDescriptor`；存在 `ZzFluentStyle` 时使用其有界 cache 生成当前 DPR、颜色和 RTL pixmap。
- descriptor 无效或不是 Fluent style 时，回退到 `Qt::DecorationRole` 的 `QIcon`；两者都无效时不预留图标矩形。
- Regular item 高 40，Compact item 高 32，section 使用相同固定高度，继续允许 `setUniformItemSizes(true)`。
- Regular 从 leading edge 依次布局 icon、单行 elided title、badge；Compact 图标居中，badge 收敛为右上角短胶囊或状态点。
- badge 文本最多显示八个 UTF-16 code unit，超长由 model 拒绝；绘制仍使用 elide 作为防御。
- selected、hover、disabled、focus 使用 palette 与 style，不硬编码主题色，不读取全局 controller。
- HighContrast 的 selected/focus/badge 必须保持边框或颜色对比，不能只靠透明度区分。
- RTL 使用 `QStyle::visualRect()` 或 leading/trailing 逻辑坐标，source row 和激活语义不反转。
- compact 不修改 model DisplayRole；tooltip 和辅助功能仍能读取完整标题与 badge。

`ZzNavigationViewPrivate::activateIndex()` 同时要求 `ItemIsEnabled` 和 `ItemIsSelectable`，防止 section 或展示行被激活。

## 7. `ZzNavigationNode` 与 Model

### 7.1 节点值

在现有四个字段末尾追加默认字段，保持旧 aggregate 初始化源码兼容：

```cpp
QString sectionTranslationContext;
QString sectionSourceText;
QString badgeText;
ZzFluentUI::ZzNavigationPlacement placement =
    ZzFluentUI::ZzNavigationPlacement::Primary;
```

语义：

- section 两个翻译字段必须同时为空或同时非空；非空表示在该节点前插入标题。
- footer 节点不允许 section，避免固定区出现滚动分组标题。
- badge trim 后必须与原值一致、不得包含换行、最多八个 UTF-16 code unit；空值表示不绘制。
- placement 只接受 Primary/Footer。
- route、title、icon、section、badge 和 placement 全是展示值；不持有页面或业务对象。

### 7.2 Model role 与 API

扩展 `ZzNavigationRole`：

- Route 保持 `Qt::UserRole + 1`。
- Icon、Section、Placement、Badge 的整数值直接对应 Foundation `ZzNavigationItemRole`。

`ZzNavigationModel::data()` 还应支持：

- `Qt::ToolTipRole`：完整翻译标题；badge 非空时附加简短 badge 文本。
- `Qt::AccessibleDescriptionRole`：badge 非空时返回 badge 文本。
- Section：翻译后的 section 标题。
- Placement：`QVariant::fromValue(ZzNavigationPlacement)`。

新增公开 API：

```cpp
/** @brief 按强类型路由返回当前顶层 model index。 */
[[nodiscard]] ZzCore::ZzResult<QModelIndex> indexForRoute(
    const ZzRouteId &routeId) const;

/** @brief 只更新一个路由的短徽标并发出局部 dataChanged。 */
[[nodiscard]] ZzCore::ZzResult<void> setBadge(
    const ZzRouteId &routeId,
    QString badgeText);
```

private 在 `setNodes()` 校验成功后一次性构建 `QHash<ZzRouteId, int>`，route 查找和 badge 更新 O(1)。`setBadge()` 同值 no-op，不 reset model，只发 Badge、ToolTip 和 AccessibleDescription roles。

`refreshTranslations()` 同时重建 title 和 section 缓存；只有非空 model 才发 Display、ToolTip、Section roles 的一次范围 `dataChanged`。

## 8. 应用框架装配

修改 `ZzApplicationWindowPrivate`：

- 用 `ZzNavigationPane` 替换直接放入 layout 的 `ZzNavigationView`。
- pane 仍观察窗口独占 `ZzNavigationModel`，不接管它。
- `navigationRequested(sourceIndex)` 读取当前窗口 model 的 `nodeAt()`，提取 route 后调用当前窗口 controller。
- 连接 `ZzNavigationController::currentRouteChanged` 到 `syncNavigationSelection()`。
- 连接 `ZzNavigationModel::modelReset` 到同一 helper，覆盖节点 reorder/reset 后的选择恢复。
- helper 使用 `indexForRoute(controller->currentRoute())`；失败时清除选择，不猜测 row。
- 初始化顺序保持 model -> pane -> controller -> connections -> WindowKit -> initial navigate。
- 每个窗口仍拥有独立 pane、projection、selection model、model、controller、host 和 WindowAgent。

`ZzApplicationBuilderPrivate` 在启动模块前执行同样的 section、badge 和 footer 校验，确保无效展示配置不会在模块启动后才失败。最多允许六个 footer 节点。

## 9. 自动测试

### 9.1 `ZzNavigationControlsTest`

扩展基础 view 测试：

- synthetic section 无 enabled/selectable flags，Enter、Return、双击和 activated 均不发意图。
- 普通 disabled 或 non-selectable row 不发意图。
- compact 不修改 Display/Icon/Badge role。
- descriptor icon 在 Fluent style 下生成非空目标像素，QIcon fallback 在普通 style 下可绘制。
- Regular/Compact、LTR/RTL 的 icon/title/badge 矩形互不重叠。
- 无自定义 role 的现有普通文本路径仍走通用 delegate。

### 9.2 新增 `ZzNavigationPaneTest`

至少覆盖：

- 单 source model 正确投影 primary、section 和 footer，source model 所有权不变。
- section 插入不改变 source row；激活 primary/footer 都映射回正确 source index。
- 主区/footer selection 互斥，programmatic source selection 可双向映射。
- placement/section 变化重建投影，普通 badge/title 变化不触发 model reset。
- source model reset、reorder、销毁、替换后没有悬空 index 或重复 signal。
- Regular=240、Compact=48；Adaptive 在顶层跨过阈值时只发一次有效状态变化。
- Adaptive reparent/show/hide 后只观察当前顶层窗口。
- 100000 source row、固定 section/footer 比例时两个 view 保持 Batched、batchSize=64、每个 pane 只有两个 view/两个 projection，不创建每项 QObject。
- 1000 次 mode、model reset、badge、selection 和 RTL 操作后 descendants、animations、timers 不增长。

### 9.3 PureTools 测试

扩展 `ZzNavigationControllerTest`：

- model 返回全部通用角色、roleNames、tooltip 和 accessible description。
- section 字段不配对、footer section、超长/带换行 badge、非法 placement 和超过六个 footer 原子失败。
- `indexForRoute()` 对 reorder 后 route 仍正确，未知/空 route 返回错误。
- `setBadge()` 只更新目标行并发出一次局部 `dataChanged`，同值 no-op。
- translation refresh 同时更新 title 和 section。

扩展 `ZzMultiWindowIsolationTest`：

- 每窗拥有不同 pane/projection/selection。
- 首次路由、点击、程序化 navigate、goBack 和 model reorder 后选择与 route 一致。
- footer 激活只影响所属窗口。

扩展 builder 测试，确保无效 section/badge/footer 在模块启动前失败。

## 10. 无障碍与输入

- pane 和两个 view 使用 Qt List/Item 可访问协议，不创建自定义无障碍工厂。
- destination 的 DisplayRole 是 accessible name；Badge 进入 AccessibleDescriptionRole。
- section 是不可操作列表标题，不获取 current/focus，不响应 Enter/Return。
- compact 只改变绘制，不清除 display、tooltip 或 accessible role。
- Up/Down、Home/End、PageUp/PageDown 由 `QListView` 处理；Enter/Return 只转发有效 destination。
- Tab 在主 view、footer view 和页面内容之间遵循 Qt focus chain；隐藏的空 footer 不进入 focus chain。
- 鼠标、touch synthesis 和双击继续使用 Qt activated 协议，不手写 press/release 状态机。

## 11. 性能门禁

扩展 `ZzBasicControlsBenchmark`，新增 `measuresNavigationPaneProjectionAndPaint()`：

- 40 个 pane，共用一个 100000 行只读 model；每个 pane 的 viewport 固定只显示有限行。
- 120 帧主题/selection/badge 视觉变化，记录 P50/P95/max。
- 1000 次 source index 映射与激活，证明 map/activation 不扫描总行数。
- 比较 40 行与 100000 行 model 的同 viewport 重复 paint，复杂度比不超过 1.5。
- 1000 次 display mode、RTL、model reset 和 badge update 后记录 descendants、animations、timers、projection 数量。

参考发布机硬门限：

- 40 个 pane 的 P95 不超过 12 ms。
- 40/100000 行同 viewport paint 比不超过 1.5。
- 每个 pane 恰好两个 `ZzNavigationView` 和两个 private projection；零 animation、零 timer。
- 热路径操作后 QObject descendants 不增长。

结构 reset 的 O(N) 时间单独报告，不混入帧预算；100000 行 reset P95 目标低于 40 ms，硬门限 80 ms。

## 12. 画廊与视觉基线

画廊新增本地 navigation pane 工作区：

- Primary 包含两个 section、descriptor icon、普通/选中/disabled、短 badge。
- Footer 包含 Settings/About，固定在底部。
- command menu 提供 Regular/Compact/Adaptive 三种模式；只修改展示状态。
- 使用本地 `QStandardItemModel`，不创建页面、不调用 route 或网络。

新增独立 `navigation-pane` screenshot surface：

- 同图展示 Regular、Compact 和 RTL Regular 三列。
- 覆盖 section、主项、footer、selected、hover、disabled、icon、badge、focus。
- 固定逻辑尺寸，文本短且稳定，不允许文字、icon 和 badge 重叠。
- Light、Dark、HighContrast × DPR 1.0/1.25/1.5/2.0，共 12 张基线。
- 新增专用文字 mask，覆盖 section/title/badge；不得扩大 mask 掩盖非文字绘制差异。
- 若通用无 role 的旧 navigation surface 像素变化，必须先证明 fallback 路径行为改变；否则视为回归。

## 13. 安装、架构与跨平台门禁

- shared/static fresh install consumer 创建 `ZzNavigationPane`、Foundation enum 和扩展 `ZzNavigationNode`。
- relocation consumer 与全部已安装公开头独立编译通过。
- `ZzFluentFoundation` 公开头不包含 Qt Widgets。
- `ZzFluentUI` 不 include `ZzPureTools`；role 只描述展示数据。
- `ZzPureTools` 公开头只依赖已有 public `Zz::FluentFoundation`，不泄漏 private `Zz::FluentUI` target。
- 架构扫描继续禁止链式 namespace、Qt Private、QWindowKit 越界、业务/存储/网络依赖和非 Zz 类型。
- Windows MSVC、Windows Qt SDK MinGW、macOS arm64/x86_64 静态审计：只使用 Qt Core/Gui/Widgets 公开 API，没有编译器或平台分支。

## 14. 提交顺序

### 提交 A：计划

```text
文档：规划高性能导航展示增强

冻结导航展示 role、组合面板、投影模型和应用框架选择同步边界。
明确结构冷路径、绘制热路径、性能预算和跨平台质量门禁。
```

### 提交 B：FluentUI 生产组件

- Foundation role/display/placement 头。
- `ZzNavigationPane` 四文件 Pimpl 与 private projection。
- `ZzNavigationView` 私有 delegate 增强。
- FluentUI 核心行为测试与 CMake。

```text
控件：实现分区与页脚导航面板

增加固定双视图投影、图标徽标绘制和自适应显示模式。
保持外部模型所有权、索引映射、键盘语义和固定对象数量。
```

### 提交 C：PureTools 模型与装配

- Node/Model role、校验、route map、badge update。
- Builder fail-fast。
- ApplicationWindow pane 装配和 selection sync。
- PureTools 测试、demo 和 install consumer。

```text
框架：接入导航展示元数据与选择同步

扩展强类型导航节点并以常数时间查找路由和更新徽标。
让每个窗口独立同步首次导航、回退、重排和页脚选择。
```

### 提交 D：质量与性能

- accessibility、architecture、clang-tidy、benchmark。
- 画廊工作区和 smoke。

```text
测试：接入导航面板质量与性能门禁

覆盖模型销毁、索引映射、自适应、无障碍、多窗口和对象稳定性。
锁定大模型绘制复杂度、结构重建预算与参考机帧耗时。
```

### 提交 E：视觉基线

```text
测试：补齐导航面板多主题视觉基线

记录三主题四档 DPR 下常规、紧凑、页脚、分区、徽标和 RTL 表面。
验证文字掩码、几何边界和 shared/static 截图一致性。
```

### 提交 F：交付记录

```text
文档：记录导航展示增强交付结果

记录提交、测试、性能、视觉、安装消费和跨平台静态审计结果。
关闭阶段十旧组件迁移审计并明确后续功能由产品需求驱动。
```

## 15. 本机验证矩阵

设置 `QT_ROOT` 指向本机现有 Qt 6.11.1 SDK 后使用对应 presets，不重新下载 Qt：

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release --parallel 2
ctest --preset linux-gcc-release --output-on-failure

cmake --preset linux-static-release
cmake --build --preset linux-static-release --parallel 2
ctest --preset linux-static-release --output-on-failure
```

定向开发循环先运行：

```bash
cmake --build --preset linux-gcc-debug --parallel 2 \
    --target ZzNavigationControlsTest ZzNavigationPaneTest \
             ZzNavigationControllerTest ZzMultiWindowIsolationTest
ctest --preset linux-gcc-debug --output-on-failure \
    -R 'fluent.navigation|puretools.navigation|puretools.multi-window'
```

最终还必须通过：

- shared/static fresh install consumer、package relocation 和公开头独立编译。
- 全部架构测试和 FluentUI 边界审计。
- shared/static 四档完整 screenshot 与画廊 smoke。
- Clang 20 ASan+UBSan DPR 1.0 完整 screenshot 与画廊 smoke。
- 定向 `clang-tidy-20`。
- 参考发布机 `ZzBasicControlsBenchmark::measuresNavigationPaneProjectionAndPaint`。

## 16. 完成定义

- 应用框架真实显示 `ZzNavigationNode::icon`，没有失效公开字段。
- section 是不可路由展示标题，footer 固定置底，badge 可局部更新。
- Regular/Compact/Adaptive、RTL、三主题和四档 DPR 正确。
- 首次导航、点击、程序化导航、回退和 model reset 后 route 与 selection 一致。
- FluentUI 不知道 route/page，PureTools 不泄漏 private FluentUI 依赖，多窗口没有共享选择状态。
- paint/activation/map 为 O(1)，结构重建 O(N) 只发生在冷路径且满足预算。
- 无每项 QObject、无动画、无 timer、无全局单例、无 Qt Private 或平台分支。
- shared/static 全量验证、安装消费、性能、视觉、sanitizer 和静态分析通过。
- 在交付结果写入实际提交、测试数量、参考机数据和剩余人工平台验证范围后，阶段 10 旧组件迁移闭环关闭。

## 17. 实际交付结果

代码级验证基于提交 `2e2ec46c6b3fb4c4ebbc92e3b37edc8c50e40b34`。本机使用 Qt 6.11.1、GCC 15.2.0、Clang/clang-tidy 20.1.8 和 CMake 4.3.3；全部构建复用 `/home/zz/Qt/6.11.1/gcc_64`，没有下载新的 Qt SDK。

### 17.1 生产实现与职责边界

- `ZzNavigationPane` 已按公开类、公开实现、private 类和 private 实现拆分，并组合两个固定 `ZzNavigationView` 与两个 private `ZzNavigationProjectionModel`。控件只观察外部 `QAbstractItemModel`，不拥有 model、不创建页面、不执行路由。
- `ZzNavigationItemRole`、`ZzNavigationPlacement` 和 `ZzNavigationDisplayMode` 位于 Fluent Foundation，只依赖 Qt Core。FluentUI 不读取 `ZzRouteId`，也不 include `ZzPureTools`。
- Primary 投影可插入不可选择的 synthetic section，Footer 投影固定置底且最多六个目标。投影缓存 source/proxy 双向映射，索引转换与激活为 O(1)，只有结构变化、section 或 placement 变化进入 O(N) 重建冷路径。
- 导航 delegate 支持 `ZzIconDescriptor`、`QIcon` 回退、短 badge、regular/compact、LTR/RTL、disabled、selected、hover、focus 与 HighContrast；无扩展 role 的普通列表仍复用原 `ZzFluentItemDelegate`。
- `ZzNavigationModel` 已增加可翻译 section、badge、placement、tooltip 和无障碍描述，使用 `QHash<ZzRouteId, int>` 提供 O(1) 路由查找；`setBadge()` 只发目标行的局部 `dataChanged`。
- Builder 在启动模块前校验 section 配对、footer section、badge 格式、placement 和六项 footer 上限。应用窗口只在 composition root 把 pane 索引意图连接到强类型 controller，并在首次导航、点击、程序化导航、回退和 model reset 后同步选择。
- 每个窗口独立拥有 pane、两个投影、两个 selection model、导航 model、controller、page host 和 WindowAgent；没有引入全局导航状态、逐项 QWidget、逐项 QObject、内部动画或 timer。

### 17.2 提交记录

- `ad628bd`：冻结导航展示角色、组合面板、投影复杂度、应用装配和验证矩阵。
- `848a9ac`：实现 Foundation 契约、四文件 `ZzNavigationPane`、双投影和导航 delegate，并接入组件测试。
- `ffe4186`：扩展 PureTools 导航节点/model、Builder fail-fast、应用窗口选择同步、多窗口和安装消费。
- `2077762`：把控件画廊升级为包含分区、图标、badge、footer 和三种显示模式的完整导航工作区。
- `3b04330`：增加四十面板、十万行 model、映射、绘制复杂度、reset 和对象稳定性性能门禁。
- `4eb1a26`：增加三主题四档 DPR 的视觉基线，并修复 hover 跟踪、compact 分隔线和 hover 反馈。
- `cd92207`：同步主机重启后的物理内存身份，重新锁定参考机档案摘要。
- `bb7467d`：更新七份同源性能报告，并把导航相对回归与 ASan/UBSan 压力场景接入本机发布脚本。
- `2e2ec46`：为有意构造的越界 placement 测试添加精确静态分析标注，保留公开 model 的防御覆盖。

### 17.3 Linux 自动验证

- `linux-gcc-release` shared Release 完整构建与 CTest 通过，共 `101/101`。
- `linux-static-release` static Release 完整构建与 CTest 通过，共 `101/101`。
- 两套完整 CTest 都覆盖导航组件、PureTools 装配、多窗口、翻译、四档截图、fresh install consumer、package relocation、公开头、二进制依赖、四个示例 smoke 和发布契约。
- 修正最终测试夹具后，14 项架构审计再次以 `14/14` 通过；preset matrix contract 和 Linux/Windows/macOS 原生门禁脚本契约通过。
- `linux-clang-tidy-release` 与 `linux-clang-tidy-static` 均使用 Clang 20 对 `152/152` 个首方翻译单元完成分析，`--warnings-as-errors=*` 下无未抑制诊断。
- `linux-clang-asan-benchmarks` 构建通过；窗口生命周期与导航面板压力场景在 Xvfb 下以 `2/2` 通过，未报告 ASan/UBSan/LeakSanitizer 诊断。
- 重启后 preset 环境变量未自动恢复，Clang fresh 配置通过命令作用域显式使用 Qt 6.11.1、Clang 20、GCC 15 external toolchain，以及文档已登记的本机 xkbcommon 头和运行库；这些机器路径没有写入共享 preset。

### 17.4 性能结果

活动参考报告来自源码提交 `cd9220712f1b3d39df7689af3d5172994e00d08e`。后续提交只更新报告、发布脚本、测试标注和本文档，没有修改导航生产热路径。固定环境为 Intel Core i7-14700、32 GiB RAM、Ubuntu 26.04、Qt 6.11.1、GCC 15.2.0、Xvfb 1920x1080x24 和 Mesa llvmpipe；档案摘要为 `sha256:f3b3982a44212a5f9b2c15c034290d920439fc3712b8361c5a11aecf19899e41`。

四十个可见 pane 共用十万行只读 model，预热后采集 120 帧：

```text
整帧 P50:              7.777867 ms
整帧 P95:              7.890631 ms
整帧 max:              8.035851 ms
绘制 P95:              7.817203 ms
状态更新 P95:          0.074130 ms
映射/激活 P95:         2.214 us
40/100000 行绘制比:    0.967326
100000 行 reset P95:  17.144409 ms
每 pane view/projection: 2 / 2
```

整帧 P95 低于 `12 ms`、绘制复杂度低于 `1.5`、reset P95 低于 `80 ms`。1000 次 mode、RTL、model reset、badge 和 selection 混合操作后，QObject descendants 不增长，animation 与 timer 保持为零。完整参考组为 `16/16`，包含七个报告生产者与九项绝对门禁；七份 JSON 与 reporter 输出逐字一致，并通过相对性能比较。

### 17.5 视觉与交互检查

- 新增 `navigation-pane-{light,dark,high-contrast}.png`，每个主题覆盖 DPR 1.0、1.25、1.5、2.0，共 12 张。
- 固定 surface 同图展示 Regular、Compact 和 RTL Regular，覆盖两个 section、descriptor/QIcon、普通/选中/hover/disabled、badge、focus、主区和固定 footer。
- shared/static 的四档完整截图测试均通过。已人工检查 Light、Dark、HighContrast 的 DPR 1.0 和 Light 的 DPR 2.0：标题、图标、badge、focus、hover、compact 分隔线与 footer 没有裁切或重叠，RTL 的 leading/trailing 布局正确，HighContrast 不只依赖透明度区分状态。
- 控件画廊和 PureTools 示例使用本地静态数据展示生产组件；示例不创建页面路由、网络请求、自动播放 timer 或业务服务。

### 17.6 安装、ABI 与架构结果

- 安装消费者实例化 `ZzNavigationPane`、Foundation 枚举和扩展后的 `ZzNavigationNode`；shared/static fresh install consumer 与移动安装前缀后的 package relocation 均通过。
- 新增公开头已进入安装清单并逐文件独立编译。公开 `ZzNavigationPane` 只暴露 `std::unique_ptr<ZzNavigationPanePrivate>`，private 投影与对象布局不进入 ABI。
- Fluent Foundation 公开导航头不依赖 Widgets；FluentUI 没有 `ZzPureTools`、`ZzWindowKit`、QWindowKit 或业务层依赖；PureTools 只依赖公开的 Fluent Foundation/FluentUI 契约。
- 本批新增生产差异没有链式 namespace、Qt Private 头、平台原生头、QSS、动态属性、编译器扩展、绝对机器路径或平台条件分支。

### 17.7 跨平台状态与人工边界

- CMake preset 矩阵继续登记 Windows MSVC shared/static、Windows Qt SDK MinGW shared/static，以及 macOS arm64/x86_64 shared/static；Windows 保持 `/utf-8`、严格警告、MSVC analyze 或 Qt SDK MinGW 工具链约束，macOS 保持 deployment target 13.3、Clang-Tidy 和 LTO 约束。
- 源码静态审计确认导航生产实现只使用标准 C++20 与 Qt Core/Gui/Widgets 公共 API，不含 `Q_OS_*`、`_WIN32`、`__APPLE__` 分支、Windows/Cocoa/X11 原生头或编译器专用 ABI。
- Windows MSVC、Windows Qt SDK MinGW、macOS arm64/x86_64 当前没有对应原生编译、安装消费、像素基线或真机交互证据，因此不得表述为这些平台已经通过。Linux 物理显示、多桌面环境、Wayland、触摸和真实高刷新率交互也仍需人工验证。
- 本批未 push、未调用 GitHub CLI、未读取或处理远端 CI；远端 CI 按用户要求继续暂缓。

### 17.8 阶段结论

导航图标、静态 section、固定 footer、短 badge、Regular/Compact/Adaptive 与强类型路由选择同步均已落到生产组件，并通过与其他阶段 10 组件同级的功能、性能、视觉、安装、sanitizer 和静态分析门禁。旧版 51 个公开组件至此全部获得已交付替代或明确删除结论；后续功能只能由真实产品需求驱动，不再依据旧仓库文件清单继续迁移。
