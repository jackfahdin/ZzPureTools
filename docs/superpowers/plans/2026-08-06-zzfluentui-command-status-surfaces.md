# ZzFluentUI 标准命令与状态表面实施计划

**目标：** 让标准 `QToolBar`、`QToolButton` 与 `QStatusBar` 在应用级 `ZzFluentStyle` 下获得统一、清晰且高性能的 Fluent 命令与状态表面，同时完整保留 Qt 原生 action、快捷键、菜单、停靠、浮动、溢出、临时消息、永久控件、size grip 和无障碍协议。

**架构：** 本批不新增 `ZzToolBar`、`ZzToolButton` 或 `ZzStatusBar` 空包装类。标准 Qt 控件继续拥有全部状态和生命周期；`ZzFluentStyle` 只在对应 primitive、control、content size 与 pixel metric 路径绘制固定复杂度表面。应用层负责创建 `QAction`、选择图标、连接业务命令、组织 toolbar/status bar 以及决定持久消息内容。

**技术约束：** Qt 6.8+、C++20、简体中文 Doxygen、传统命名空间、无 Qt Private API、无 QSS、无动态属性协议、无每控件 proxy style、无平台原生头、无文件或网络 I/O、无内部 timer、无业务模型访问。

## 1. 批次边界

本批实现：

- 标准 `QToolBar` 的水平/垂直面板、停靠/浮动背景、拖动 handle、separator 与 overflow extension 的 Fluent 绘制。
- 标准 `QToolButton` 的 normal、hover、pressed、checked、disabled、focus、menu 与 RTL 表面；icon、text、arrow 和 menu geometry 继续由 Qt 基础 style 绘制。
- 标准 `QStatusBar` 的背景、顶部边界和无额外 item frame 的安静表面；临时消息、普通/永久 widget 与 size grip 继续由 Qt 管理。
- 保留 base style 测量后，为工具按钮与工具栏 chrome 提供稳定的最小逻辑尺寸。
- 行为、无障碍、安装消费、性能、控件画廊和三主题四档 DPR 视觉门禁。

本批不实现：

- `QAction` 子类、命令总线、快捷键管理器、撤销栈、业务权限、路由或 telemetry。
- 字体图标枚举、action 动态属性、从磁盘或 URL 加载图标。应用使用标准 `QIcon`，需要主题图标时可预先使用现有图标服务构造。
- 自定义 docking 系统、浮动窗口阴影、跨屏定位、原生标题栏或 `ZzWindowKit` 集成。
- 自定义 status message 队列、自动消失策略、进度任务、通知中心或日志读取。
- 旧版 API 或固定像素几何兼容。

## 2. 旧版逐行审计

审计来源为旧仓库 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzFluentUI`，只保留通用视觉意图，不复制实现。

### 2.1 `ZzToolBar.h`

| 行 | 结论 |
|---:|---|
| 1-8 | include guard、旧导出宏和全局定义属于旧构建体系；新版不新增公开头。 |
| 9-17 | `QToolBar` 空派生和两个转发构造没有独立协议价值；标准类已完整提供 title、orientation、areas、movable 与 floatable API。 |
| 19-20 | spacing 直接穿透内部 layout，依赖 `QToolBar` 实现细节；新版使用 `PM_ToolBarItemSpacing` 应用级度量。 |
| 22-23 | 每实例 tool button 固定尺寸破坏文本、DPI 和应用 `iconSize`；新版只对 base 结果设置最小值。 |
| 25-26 | 两个 action helper 把 action 创建、私有字体图标和 toolbar 混合；新版调用方使用标准 `addAction()` 与 `QIcon`。 |
| 28-29 | 整体重写 paint 只为浮动阴影和 handle，容易绕过平台 toolbar 协议；新版只实现标准 style element。 |

### 2.2 `ZzToolBar.cpp`

| 行 | 结论 |
|---:|---|
| 1-10 | 生产控件直接依赖 painter、全局主题、字体图标和专用 proxy style；新版依赖收敛到一个应用级 `ZzFluentStyle`。 |
| 11-20 | 每实例分配 style、写 object name、修改内部 layout 和固定 margin；新版不建立字符串路由或实例级 style 对象。 |
| 22-29 | 每个 toolbar 永久连接主题单例且只在 floating 时刷新；新版主题控制器一次更新应用 style 和所有相关 widget。 |
| 30 | 无条件透明原生窗口属性可能改变 Windows/macOS 浮动工具栏合成；删除。 |
| 32-41 | top-level 变化时手工改 margin 为阴影留位，覆盖平台 docking layout；新版保留 Qt 几何。 |
| 44-48 | title 构造只转发 `setWindowTitle()`，标准 `QToolBar(title, parent)` 已提供。 |
| 50-54 | 手工删除 proxy style 暴露所有权风险；新版应用统一拥有一个 style。 |
| 56-76 | spacing 与 size setter 直接访问内部 layout/private style；删除这些非通用 API。 |
| 78-95 | helper 忽略传入 icon，始终生成 Broom 图标，并用动态属性传字体 glyph；存在确定性 bug且破坏标准 icon mode/state，整体删除。 |
| 97-111 | 浮动状态每帧绘制软件阴影和圆角背景，成本随面积增长且无法表达平台窗口阴影；新版只绘制轻量 surface/border。 |
| 112-119 | 手工请求 handle 子区域仍是合理意图，但应由 `PE_IndicatorToolBarHandle` 统一处理。 |
| 121-127 | docked 分支先绘制透明圆角再调用基类，没有视觉收益；新版直接绘制确定面板。 |

### 2.3 `ZzToolBarPrivate.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-14 | private 继承 QObject、包含 `Q_OBJECT` 和空构造/析构，却没有信号、槽或独立生命周期；属于无效对象开销。 |
| h:16-20 | 只保存实例 style、主题副本和 6px 阴影常量；三份状态都由应用 style snapshot 或平台窗口系统取代。 |
| cpp:1-10 | 实现只有空构造与空析构，没有可迁移功能。 |

### 2.4 `ZzToolBarStyle.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-18 | 每个 toolbar 一个 `QProxyStyle`，重复虚表对象和主题连接；新版扩展现有应用级 style。 |
| h:20-24 | style 保存全局主题副本并拆分手绘 icon/text/indicator；新版只手绘 panel/handle/separator，内容委托 Qt。 |
| cpp:12-22 | 构造忽略传入 base style，且每实例连接全局单例；删除。 |
| cpp:24-31 | 完全吞掉 `PE_PanelButtonTool`，使 normal/hover/focus 只能依赖后续自绘；新版在该标准 primitive 中绘制状态表面。 |
| cpp:32-40 | toolbar style 顺带接管 menu frame，扩大职责且与 popup 批次重复；删除。 |
| cpp:42-63 | handle 只画一条点线并强制转换为旧包装类；新版依据 `QStyleOptionToolBar::orientation` 绘制固定六点，不要求自定义 widget。 |
| cpp:64-83 | separator 使用浮点比例和旧包装类型；新版在有效 rect 内绘制物理像素对齐的中线并支持 RTL/两种 orientation。 |
| cpp:92-168 | 重写整个 `CE_ToolButtonLabel`，手工组合状态、内容和扩展按钮；会丢失平台菜单、助记符和新 Qt 状态，整体不迁移。 |
| cpp:171-184 | extension 固定 16px 命中区域偏小；新版提供 28px 最小 extent。 |
| cpp:187-204 | 有效固定尺寸直接替换 base measurement，长文本和 TextUnderIcon 可裁切；新版使用 `expandedTo()`。 |
| cpp:206-235 | menu indicator 错用 `SC_ScrollBarSubLine`，几何协议不属于 tool button；新版保留 `CC_ToolButton`/`SC_ToolButtonMenu` 原生路径。 |
| cpp:237-318 | 手工取 default action、动态属性和字体 glyph，未完整处理 icon mode/state/DPR/RTL；新版委托 `CE_ToolButtonLabel`。 |
| cpp:320-359 | 文本按固定 12px 和 icon width 排布，没有 elide、助记键或 RTL；新版委托 base style。 |

### 2.5 `ZzStatusBar.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-15 | `QStatusBar` 空派生只转发构造和析构，没有独立公开协议；新版直接使用标准类。 |
| cpp:1-6 | 引入 painter/timer 但没有使用，实际只为专用 style 服务；删除。 |
| cpp:7-15 | object name + QSS、固定 28px 高度、固定左 margin 与每实例 style 会破坏字体、DPI、RTL 和 ownership；新版全部通过应用 style 与 layout 自然测量。 |
| cpp:17-19 | 空析构没有功能。 |

### 2.6 `ZzStatusBarStyle.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-20 | 每实例 proxy style 与可变主题副本重复应用级状态；新版复用现有 snapshot。 |
| cpp:8-18 | 构造忽略 base style 参数并连接全局主题单例；删除。 |
| cpp:20-35 | `PE_PanelStatusBar` 背景和边界是可保留视觉意图；新版使用 `SurfaceSecondary` 与 `ControlStroke` 绘制。 |
| cpp:36-51 | item frame 根据 size grip 状态画 3px 强调色竖条，混淆 item 分隔与 resize affordance；新版 item frame 保持无框。 |
| cpp:60-75 | 吞掉 `CE_SizeGrip` 使 resize affordance 消失；新版完整委托 base style。 |
| cpp:77-86 | size 与 metric override 只原样转发，没有价值。 |

## 3. 样式路由设计

### 3.1 `ZzFluentStyle`

在现有公开 override 内增加路由，不新增公开方法或导出类型：

- `sizeFromContents(CT_ToolButton)`：先调用 `QProxyStyle` 保留 icon、文本、font、popup mode 和应用设置，再 `expandedTo(QSize(32, 32))`；绝不缩小 base 结果。
- `pixelMetric()`：为 `PM_ToolBarFrameWidth`、`PM_ToolBarHandleExtent`、`PM_ToolBarItemSpacing`、`PM_ToolBarItemMargin`、`PM_ToolBarSeparatorExtent`、`PM_ToolBarExtensionExtent` 和 `PM_ToolBarIconSize` 返回稳定逻辑值。widget 显式 `iconSize` 仍由 Qt 优先处理。
- `drawPrimitive(PE_PanelButtonTool)`：只绘制 normal/hover/pressed/checked/disabled surface，不接触 action、icon、text 或 menu。
- `drawPrimitive(PE_PanelToolBar)` 与 `drawControl(CE_ToolBar)`：使用同一个 helper 绘制背景和边界，避免两条路径视觉不一致；每次实际 Qt 调用只绘制一次。
- `drawPrimitive(PE_IndicatorToolBarHandle)`：依据 `QStyleOptionToolBar::orientation` 绘制固定六个圆点，超小 rect 安全 no-op。
- `drawPrimitive(PE_IndicatorToolBarSeparator)`：在 horizontal toolbar 绘制垂直线，在 vertical toolbar 绘制水平线；线宽按当前 DPR 对齐一个物理像素。
- `drawPrimitive(PE_PanelStatusBar)`：绘制 secondary surface 和一条顶部边界。
- `drawPrimitive(PE_FrameStatusBarItem)`：不绘制装饰 frame；item 内容、布局和 enabled 状态仍由 widget 自身负责。
- `CC_ToolButton`、`CE_ToolButtonLabel`、menu arrow、`CE_SizeGrip` 和 hit test 全部继续委托 base style。

### 3.2 `ZzFluentStylePrivate`

新增带简体中文 Doxygen 的固定复杂度 helper：

```cpp
void drawToolButtonPanel(
    const QStyleOption *option,
    QPainter *painter) const;
void drawToolBarPanel(
    const QStyleOption *option,
    QPainter *painter) const;
void drawToolBarHandle(
    const QStyleOptionToolBar *option,
    QPainter *painter) const;
void drawToolBarSeparator(
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const;
void drawStatusBarPanel(
    const QStyleOption *option,
    QPainter *painter) const;
```

- helper 只读取不可变 `snapshot`、`QStyleOption` 和 widget orientation，不修改 widget/palette/geometry。
- tool button normal 状态保持透明；hover 使用 `ControlFillHover`，pressed 使用 `ControlFillPressed`，checked 使用 `ControlFill` 并绘制 `ControlStroke`，disabled 不制造高对比噪声。
- toolbar/status background 分别使用 `Surface`/`SurfaceSecondary`，边界使用 `ControlStroke`；HighContrast 仍由 token 保证对比。
- 不分配 QObject、QPixmap、QPainterPath、容器或字符串，不读 resource，不加锁，不创建 animation/timer。

## 4. Qt 原生协议边界

- `QAction` 继续拥有 text、icon、shortcut、checkable、enabled、visible、separator 和 triggered/toggled 信号。
- `QToolButton` 继续处理 `InstantPopup`、`MenuButtonPopup`、`DelayedPopup`、default action、auto repeat、keyboard activation、accessible action 与 menu subcontrol。
- `QToolBar` 继续处理 orientation、allowed areas、movable、floatable、toggle view action、docking、floating、overflow extension、widget action 和 action geometry。
- `QStatusBar` 继续处理 `showMessage()` timeout、`clearMessage()`、`messageChanged`、normal/permanent widgets、stretch、visibility 和 size grip。
- style 不查找 `qt_toolbar_ext_button` 等私有 object name，不访问 Qt private header，不通过 child 顺序反推状态。
- 应用业务命令只连接 `QAction` 信号，UI style 不调用 command、route、repository、network 或 logger。

## 5. 测试计划

新增 `ZzFluentUI/tests/ZzCommandStatusSurfacesTest.cpp` 与 CTest `fluent.command-status-surfaces`：

- 验证工具栏七项 metric 与 `CT_ToolButton` 最小值，同时确认长文本、TextUnderIcon 和大字体的 base measurement 不被缩小。
- 验证 horizontal/vertical、LTR/RTL、movable/floatable/allowed areas、toggle view action、separator、widget action 和窄宽度 overflow 不改变 Qt 公共状态。
- 验证 tool button default action、shortcut、checkable、disabled、四种 `toolButtonStyle`、三种 popup mode、菜单触发和 keyboard Space/Enter 语义。
- 验证 normal/hover/pressed/checked/disabled/focus 的渲染状态；icon/text/menu 区域由 base style 绘制且位于控件 rect 内。
- 验证 status bar normal/permanent widget、stretch、临时消息、clear、timeout、`messageChanged` 和 size grip 开关。
- 通过 `QAccessible::queryAccessibleInterface()` 验证 toolbar、tool button、status bar 和可见 message 的原生 role/name/action 没有被替换。
- Light/Dark/HighContrast 切换后背景、边界和状态色更新；颜色变化只触发绘制更新，不创建 per-widget style。
- 1000 次 action 状态、orientation、RTL、临时消息和主题切换后，除 Qt 明确拥有的 timeout timer 外，QObject、animation、style 数量不增长。

测试不得依赖内部 child object name、Qt private type 或平台像素完全一致；需要验证 overflow 时只通过公开 `actionGeometry()`、可见 action 与控件尺寸建立关系。

## 6. 安装消费与架构

- `tests/InstallConsumer/Gui/main.cpp` 从安装包创建 `QMainWindow`、`QToolBar`、`QToolButton`、`QStatusBar` 和标准 actions，安装 `ZzFluentStyle` 后离屏渲染并验证信号与非空画面。
- 不新增公开头；安装消费者只包含 Qt 公共头和已安装的 `ZzFluentStyle`/`ZzThemeController`。
- 新测试 target 链接 `Qt6::Test`、`Qt6::Widgets` 与 `Zz::FluentUI`，接入 warnings、sanitizer 和 CTest label。
- `CheckZzFluentUIBoundaries.cmake` 继续禁止 ZzPureTools/ZzWindowKit/QWK、Qt Private API、链式命名空间和业务层关键词。
- Windows MSVC、Windows Qt SDK MinGW 与 macOS 只做源码、preset、公开 ABI、依赖和条件编译静态审计；没有对应工具链证据前不记录为原生通过。

## 7. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 预构造 30 个可见 `QToolBar`，每个包含 8 个 action、一个 separator、一个 checkable action 和一个 menu action；另构造 30 个带 normal/permanent widget 的 `QStatusBar`。
- 预热后循环切换 checked/enabled、orientation 与短消息，并离屏绘制 120 帧；reference Release P95 初始硬门限为 `12 ms`。
- 采样期不新增 action、widget、menu、style、animation 或长期 timer；临时消息使用 `showMessage(text, 0)`，避免 benchmark 引入 timeout timer。
- 1000 次状态和主题切换后记录 descendants、animations、timers 与 `ZzFluentStyle` 实例数，数量必须回到稳定基线。
- 对比每个 toolbar 4 个与 40 个 action、但相同可见 viewport 的重复绘制耗时，复杂度比不得超过 `2.0`；不可见 overflow action 不得触发逐 action 自定义绘制。

若首次 reference 数据证明门限与既有离屏绘制成本不匹配，只能基于原始数据、60 Hz 帧预算和同环境重复结果调整一次，并在性能提交与交付记录中说明。

## 8. 示例与视觉基线

- 控件画廊新增“Command and status”区域，使用 `QMainWindow` 或无业务本地宿主演示 horizontal toolbar、vertical toolbar、checkable/disabled/menu action、separator、overflow 与 status message；所有 action 只更新本地展示文本。
- 截图 surface 覆盖 horizontal/vertical、icon only、text beside icon、checked、pressed、disabled、focus、separator、menu、overflow、RTL、normal status、temporary status、permanent widget 和 size grip。
- 生成 `command-status-{light,dark,high-contrast}.png`，覆盖 DPR 1.0、1.25、1.5、2.0 共 12 张。
- 人工检查至少三主题 DPR 1.0 和 Light DPR 2.0，确认 icon、text、menu arrow、handle、separator、overflow、status text 和 size grip 不裁切、不重叠且对比清晰。

## 9. 验证矩阵

每个代码提交运行对应 target 与定向 CTest；最终运行：

- GCC 15 shared/static Release 全量构建和 CTest。
- Clang 20 ASan+UBSan 定向构建与测试；若本机全树仍受 `xkbcommon` 私有头影响，只记录实际完成范围。
- 使用成功 compilation database 对所有新增或修改的一方翻译单元执行 clang-tidy 20。
- fresh install consumer、package relocation、公开头、完整架构、FluentUI 边界和二进制依赖。
- 控件画廊 shared/static offscreen smoke。
- DPR 1.0/1.25/1.5/2.0 完整截图回归和人工抽检。
- reference Release benchmark 与 sanitizer 定向 benchmark。
- preset matrix contract、gate script contract和本批平台宏/原生头/Qt Private/绝对路径扫描。

## 10. 提交顺序

```text
文档：规划Fluent命令与状态表面批次
样式：完善Fluent命令与状态表面
测试：接入命令与状态表面质量门禁
性能：锁定命令与状态表面绘制预算
测试：补齐命令与状态表面视觉基线
文档：记录命令与状态表面交付结果
```

提交标题使用中文简述，正文使用多个中文段落记录实现、验证、性能和平台影响。每个逻辑批次验证后立即提交；不 push，不调用 GitHub CLI，不处理远端 CI，不下载 Qt。

## 11. 交付结果

**状态：** 已于 2026-08-06 完成本批次实现与本机质量门禁。代码验证基于提交 `ee5158be50adc468cc1cdb607d5d0ec633f44636`，使用本机 Qt 6.11.1、GCC 15.2.0、Clang/clang-tidy 20.1.8 和 CMake 4.3.3；全程复用 `/home/zz/Qt/6.11.1/gcc_64`，没有下载新的 Qt SDK。

### 11.1 生产实现与架构边界

- 标准 `QToolBar`、`QToolButton` 和 `QStatusBar` 直接消费应用级 `ZzFluentStyle`；没有新增 `ZzToolBar`、`ZzToolButton`、`ZzStatusBar` 包装类、公开头或导出 ABI。
- `ZzFluentStyle` 在 base style 测量结果之上提供稳定的工具栏 metric 和工具按钮最小尺寸，只绘制 button panel、toolbar panel、handle、separator、status panel 与顶部边界。图标、文字、菜单箭头、size grip、命中测试及 action geometry 继续委托 Qt。
- 工具按钮 normal 保持透明，hover、pressed、checked 与 disabled 使用主题 snapshot token；工具栏 handle 固定为六点，separator 和边界按当前 DPR 对齐一个物理像素。Light、Dark 与 HighContrast 均沿用同一条无分支绘制路径。
- Qt 继续拥有 action、快捷键、菜单、停靠、浮动、方向、allowed area、overflow、状态消息、普通/永久 widget、timeout、size grip 与无障碍协议；style 不保存业务状态，也不访问模型、命令、路由、日志、文件或网络。
- 绘制 helper 不创建 QObject、style、animation、timer、pixmap cache、容器或字符串，没有 QSS、动态属性、Qt Private API、平台原生头、链式命名空间或 `QWindowKit::` 依赖泄漏。

### 11.2 行为、无障碍与安装消费

- `fluent.command-status-surfaces` 覆盖七项 toolbar metric、base measurement、horizontal/vertical、LTR/RTL、allowed area、toggle view action、separator、widget action、overflow 与浮动公开状态。
- 工具按钮测试覆盖 default action、checkable/disabled、四种显示模式、三种 popup mode、menu action、Space/Enter 激活和 focus；status bar 测试覆盖普通/永久控件、临时消息、清除、timeout、messageChanged 与 size grip 切换。
- 通过 Qt 公共 `QAccessibleInterface` 验证 toolbar、tool button 与 status bar 的 role、name 和原生 action 没有被包装层替换。
- 1000 次 action、方向、RTL、消息和主题变化后，对象、animation、timer 与 style 数量保持稳定；测试不查找 Qt 私有 child object name，也不包含 Qt private header。
- fresh install GUI consumer 从安装包创建标准主窗口、工具栏、工具按钮和状态栏，并验证 action 信号与非空离屏渲染；shared/static 的安装消费和 package relocation 均通过。

### 11.3 提交记录

- `a0c1f54`：完成旧版 toolbar/status bar 相关文件的逐行审计，确定标准 Qt 控件与应用级 style 的边界。
- `48c3a1f`：实现工具按钮状态表面、工具栏 panel/handle/separator、状态栏 panel 和稳定 metric。
- `facb0f9`：接入行为、无障碍、对象稳定性、安装消费和架构质量门禁。
- `3d62d79`：接入三十组工具栏与状态栏绘制预算、对象稳定性和 40/4 action 复杂度门禁。
- `ee5158b`：接入控件画廊、三主题四档 DPR 的十二张新基线，并同步更新受工具按钮样式影响的既有基线。

### 11.4 Linux 自动验证

- `linux-gcc-release` shared Release 最终全量 CTest 通过，共 `97/97`。
- `linux-static-release` static Release 最终全量 CTest 通过，共 `97/97`。
- fresh install consumer、package relocation、公开头独立编译、二进制依赖、完整架构审计、FluentUI 边界、文档审计、preset/gate 契约与平台编译探针通过。
- shared/static 控件画廊 offscreen smoke 通过；DPR 1.0、1.25、1.5、2.0 下的 shared/static 完整截图回归通过。
- Clang 20 ASan+UBSan 下的命令与状态行为测试、benchmark、DPR 1.0 完整截图和控件画廊 offscreen smoke 定向运行通过，未报告内存错误或未定义行为。
- 使用成功的 GCC compilation database 对本批修改的 style、专项测试、安装消费者、benchmark、截图和画廊翻译单元执行 clang-tidy 20 定向检查，均以 0 退出。本机未安装 `clang-format-20`，因此没有把该命令记录为通过；格式由 warnings-as-errors 编译、`git diff --check` 和既有源码格式约束验证。

### 11.5 性能结果

活动 Linux reference 发布机使用固定 CPU 亲和、Xvfb 1920x1080x24 和 Mesa llvmpipe。预构造 30 个可见 `QToolBar` 和 30 个 `QStatusBar`，每个工具栏包含 8 个 action、separator、checkable action 与 menu action，预热后采集 120 帧：

```text
P50: 1.475 ms
P95: 1.496 ms
max: 1.505 ms
40 action 与 4 action 相同可见尺寸复杂度比: 1.031
```

P95 低于 `12 ms` 硬门限，复杂度比低于 `2.0` 硬门限。1000 次方向、RTL、checked、enabled、status message 和主题变化后，完整夹具的 QObject descendants 保持 `1020`、animation 保持 `0`、timer 保持 `0`、子级 style 保持 `0`。

Clang 20 ASan+UBSan 插桩环境定向结果为 P50 `2.531 ms`、P95 `2.738 ms`、max `2.882 ms`、复杂度比 `0.976`，且对象数量同样稳定。Sanitizer 数据只用于验证插桩路径，不替代上述 reference Release 原始结果。

### 11.6 视觉检查

新增 `command-status-{light,dark,high-contrast}.png`，每个主题覆盖 DPR 1.0、1.25、1.5、2.0，共 12 张。固定 surface 覆盖 horizontal/vertical、icon only、text beside/under icon、checked、hover、pressed、disabled、focus、separator、menu arrow、overflow、RTL、正常状态、临时消息、永久 widget 与 size grip。

工具按钮新表面同时改变了既有总览、tabs 和 suggest-box 中的非文字区域，因此按实际完整回归结果更新 30 张旧基线，没有改动其他 PNG。已人工检查 Light、Dark、HighContrast 的 DPR 1.0 和 Light 的 DPR 2.0；图标、文字、菜单箭头、handle、separator、overflow、status text 与 size grip 均清晰，没有裁切、重叠、文字越界或不连贯缩放。

### 11.7 跨平台状态

- 本批继续保留 Windows MSVC shared/static、Windows Qt SDK MinGW shared/static，以及 macOS arm64/x86_64 shared/static 的 CMake preset 和安装消费契约。
- 生产修改只使用 Qt Core/Gui/Widgets 公共 style API 和标准 C++20；没有新增公开 ABI、平台宏、编译器专用扩展、原生窗口调用或平台链接依赖。Windows 与 macOS 可继续使用 Qt 自身的 action、toolbar、menu 和 status bar 平台适配。
- Windows MSVC、Windows Qt SDK MinGW 与 macOS 当前只完成源码、preset、公开 ABI、依赖方向和条件编译静态审计，尚未在对应平台完成编译、安装消费、像素基线或真机交互验证；不得将本节结果表述为这些平台已经运行通过。
- 本批未访问 GitHub CLI、未运行或读取远端 CI、未 push；远端 CI 按用户要求继续暂缓。
