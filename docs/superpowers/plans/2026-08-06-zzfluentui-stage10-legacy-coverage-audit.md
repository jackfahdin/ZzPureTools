# ZzFluentUI 阶段 10 旧组件覆盖闭环审计

**目标：** 对旧版 `ZzFluentUI` 的全部公开组件逐项给出新版去向，区分“保留为自定义组件”“由标准 Qt 控件覆盖”“由已有组件组合”“明确删除”和“仍需独立批次”的能力，防止按旧文件数量机械迁移。

**审计基线：** 旧版源码位于新仓库同级的 `../ZzFluentUI`，仅用于行为和视觉意图审计。新版不复制旧实现，不承诺源码或二进制兼容。

**结论：** 旧版 51 个公开组件均已获得明确处置和交付闭环。`ZzNavigationBar` 已拆分为展示面板、值模型、强类型控制器、页面宿主和窗口 composition root；图标、静态分区、固定 footer、短 badge 与自适应模式也已完成。搜索、动态树和页面新窗口被明确保留在应用层或未来真实产品需求中，不构成旧组件迁移缺口。

## 1. 判定规则

每个旧组件只能归入以下一种主要处置：

1. **自定义组件：** Qt 没有等价协议，且存在可复用的通用交互或布局能力。
2. **标准 Qt + Fluent style：** Qt 已拥有完整状态、输入、模型、无障碍或平台协议，只需统一绘制。
3. **组合替代：** 旧类型把多个职责绑在一起，新版使用职责单一的现有组件组合，不新增空包装类。
4. **合并或改名：** 保留通用能力，但删除营销、业务或实现细节命名。
5. **明确删除：** 能力属于业务副作用、全局状态、重复状态或有缺陷的手工协议。

新增公开类型还必须同时满足：无法由 Qt 标准类型或现有 Zz 组件直接表达、不会复制 Qt 状态、不会让 UI 访问业务模型，并且可以建立固定复杂度、无障碍和跨平台契约。

## 2. 全部公开组件处置表

| 旧版类型 | 新版处置 | 代码级结论 |
|---|---|---|
| `ZzAcrylicUrlCard` | 合并为 `ZzImageCard` | 图片、标题和按钮语义已覆盖；URL 打开由应用连接 `clicked`，控件不访问桌面服务。 |
| `ZzBreadcrumbBar` | 保留为新版 `ZzBreadcrumbBar` | 使用逻辑索引发出意图，保留 RTL、键盘、焦点和无障碍契约。 |
| `ZzCalendar` | 保留为新版 `ZzCalendar` | 复用 `QCalendarWidget` 的日期模型和键盘协议，只增加 Fluent 展示与选择意图。 |
| `ZzCalendarPicker` | 保留为新版 `ZzCalendarPicker` | 继承 `QDateEdit`，不复制日期范围、解析或 locale 状态。 |
| `ZzCheckBox` | `QCheckBox` + `ZzFluentStyle` | 标准控件已拥有 check state、三态、快捷键和无障碍协议。 |
| `ZzComboBox` | `QComboBox` + `ZzFluentStyle` | 保留标准 model、editor、popup、validator 和输入法协议。 |
| `ZzDoubleSpinBox` | 保留为新版 `ZzDoubleSpinBox` | 继承 `QDoubleSpinBox`；与标准 spin box 共用 style 几何和命中测试。 |
| `ZzFlowLayout` | 保留为新版 `ZzFlowLayout` | 重写为拥有明确 item 所有权、height-for-width、RTL 和溢出保护的布局。 |
| `ZzIconButton` | 保留为新版 `ZzIconButton` | 继承 `QAbstractButton`，使用有界图标缓存和 Qt 原生输入语义。 |
| `ZzImageCard` | 保留为新版 `ZzImageCard` | 继承 `QAbstractButton`，只保存展示值，不执行 URL、网络或路由。 |
| `ZzInteractiveCard` | 合并为 `ZzActionCard` | 标题、说明、图标和点击意图是通用能力；业务动作由应用连接。 |
| `ZzLCDNumber` | `QLCDNumber` + `ZzFluentStyle` | 标准控件保留 digit count、mode、segment style 和数值可访问语义。 |
| `ZzLineEdit` | `QLineEdit` + `ZzFluentStyle` | 不复制 document、cursor、selection、validator、clipboard 或输入法状态。 |
| `ZzListView` | `QListView` + `ZzFluentItemDelegate` | 标准 Model/View 已提供虚拟化、selection、滚动和无障碍协议。 |
| `ZzMenu` | `QMenu` + `ZzFluentStyle` | 标准 action、submenu、shortcut、popup 和平台行为保持不变。 |
| `ZzMenuBar` | `QMenuBar` + `ZzFluentStyle` | 标准 action 与键盘菜单协议保持不变。 |
| `ZzMessageBar` | 保留为新版 `ZzMessageBar` | 只展示消息并发出关闭意图；timer 有界且不自行删除控件。 |
| `ZzMessageButton` | `ZzPushButton` + `ZzMessageBar` | 旧类在按钮内部创建消息，是应用连接职责；不保留该耦合类型。 |
| `ZzMultiSelectComboBox` | 保留为新版 `ZzMultiSelectComboBox` | 使用值模型和事务式 popup，不接触远程数据、历史或持久化。 |
| `ZzNavigationBar` | 拆分并完成导航展示增强 | `ZzNavigationPane`、`ZzNavigationModel`、`ZzNavigationController`、`ZzPageHost` 和窗口 composition root 分担展示、值数据、路由、页面与装配；不恢复旧大而全类型。 |
| `ZzPivot` | `ZzTabBar` 或标准 `QTabBar` | 水平标签选择已覆盖；旧独立 model、`QScroller` 和重复 current 状态删除。 |
| `ZzPlainTextEdit` | `QPlainTextEdit` + `ZzFluentStyle` | 保留标准 document、undo、selection、clipboard 和输入法协议。 |
| `ZzPopularCard` | 合并为 `ZzActionCard` | 营销命名和固定布局删除，保留通用信息卡片与激活意图。 |
| `ZzProgressBar` | `QProgressBar` + `ZzFluentStyle` | 标准 range、value、format、orientation 和无障碍值协议保持不变。 |
| `ZzProgressRing` | 保留为新版 `ZzProgressRing` | 继承 `QProgressBar`，只增加环形绘制和一个可复用忙碌动画。 |
| `ZzPromotionCard` | 合并为 `ZzImageCard` | 图片卡片能力已覆盖；营销字段和每次按压分配的动画删除。 |
| `ZzPromotionView` | 改名为 `ZzCarouselView` | 使用 `QAbstractItemModel`、固定两个可见项和一个持久动画替代每项 widget。 |
| `ZzPushButton` | 保留为新版 `ZzPushButton` | 继承 `QPushButton`，只增加 Fluent appearance，不复制 checked 或输入状态。 |
| `ZzRadioButton` | `QRadioButton` + `ZzFluentStyle` | 标准 exclusive、button group、快捷键和无障碍协议保持不变。 |
| `ZzReminderCard` | 合并为 `ZzActionCard` | 提醒调度和业务状态删除，只保留展示与点击意图。 |
| `ZzRoller` | 保留为新版 `ZzRoller` | 重写为确定的当前索引、奇数可见行、键盘/滚轮/触摸和固定绘制窗口。 |
| `ZzRollerPicker` | 保留为新版 `ZzRollerPicker` | 多列确认使用事务状态；日期联动和业务校验留给 presenter。 |
| `ZzScrollArea` | 保留为新版 `ZzScrollArea` | 组合两条 `ZzScrollBar`，保留标准 viewport/widget 所有权。 |
| `ZzScrollBar` | 保留为新版 `ZzScrollBar` | 继承 `QScrollBar`，不再镜像 origin scrollbar 的 range/value 状态。 |
| `ZzScrollPage` | 组合替代 | `ZzScrollArea`、页面 layout、`ZzPageHost`、`ZzNavigationController` 和 `ZzBreadcrumbBar` 分别承担职责。 |
| `ZzScrollPageArea` | 普通 `QWidget`/frame + layout | 旧类只有固定高度圆角背景；应用可用 palette、style 和 layout 表达，不建立公共 ABI。 |
| `ZzSlider` | `QSlider` + `ZzFluentStyle` | 标准 range、tracking、orientation、tick 和无障碍协议保持不变。 |
| `ZzSpinBox` | 保留为新版 `ZzSpinBox` | 继承 `QSpinBox`，与标准 spin box 共用 style 几何和命中测试。 |
| `ZzStatusBar` | `QStatusBar` + `ZzFluentStyle` | 标准临时消息、永久控件和 size grip 协议保持不变。 |
| `ZzSuggestBox` | 保留为新版 `ZzSuggestBox` | 使用 `QCompleter` 和外部 model，只实现本地同步建议展示。 |
| `ZzTabBar` | 保留为新版 `ZzTabBar` | 在 `QTabBar` 协议上增加稳定拖出意图，不重写键盘导航。 |
| `ZzTabWidget` | 保留为新版 `ZzTabWidget` | 管理 tab 页面所有权、拖出事务和重新接回，不访问业务页面模型。 |
| `ZzTableView` | `QTableView` + `ZzFluentItemDelegate` | 标准虚拟化、header、selection 和 Model/View 协议保持不变。 |
| `ZzTearOffWidget` | 合并进 `ZzTabWidget` 的拖出窗口 | 独立类型没有可复用公共语义；拖出生命周期由 tab 宿主统一管理。 |
| `ZzText` | `QLabel` + typography token/palette | 文本、换行、对齐、buddy 和无障碍由 `QLabel` 提供；图标使用 `ZzIconButton` 或标准 decoration。 |
| `ZzToggleButton` | checkable `ZzPushButton` | `setCheckable(true)` 已提供 checked、toggled、键盘和无障碍 toggle 语义。 |
| `ZzToggleSwitch` | 保留为新版 `ZzToggleSwitch` | 开关形态不是标准按钮皮肤；新版复用一个持久动画并保留 `QAbstractButton` 协议。 |
| `ZzToolBar` | `QToolBar` + `ZzFluentStyle` | 标准 action、停靠、浮动、overflow 和 orientation 协议保持不变。 |
| `ZzToolButton` | `QToolButton` + `ZzFluentStyle` | 标准 action、popup mode、auto raise 和可访问命令协议保持不变。 |
| `ZzToolTip` | `QToolTip` + `ZzFluentStyle` | 标准全局 tooltip 生命周期、多屏定位和平台事件协议保持不变。 |
| `ZzTreeView` | `QTreeView` + `ZzFluentItemDelegate` | 标准层级索引、展开、selection、虚拟化和无障碍树协议保持不变。 |

## 3. 已完成批次证据

| 能力组 | 设计与交付记录 |
|---|---|
| 基础按钮、标准控件、消息、导航、面包屑和 item view | `2026-08-02-zzfluentui-basic-controls.md` |
| 日历与日期选择 | `2026-08-05-zzfluentui-calendar-controls.md` |
| Action/Image 卡片收敛 | `2026-08-05-zzfluentui-card-controls.md` |
| 环形进度 | `2026-08-05-zzfluentui-progress-ring.md` |
| 滚动条与滚动区域 | `2026-08-05-zzfluentui-scroll-controls.md` |
| 标签与拖出窗口 | `2026-08-05-zzfluentui-tear-off-tabs.md` |
| 轮播视图 | `2026-08-06-zzfluentui-carousel-view.md` |
| 组合框、多选、建议、滚轮和数值输入 | 对应 `2026-08-06-zzfluentui-*.md` 独立计划 |
| 菜单、tooltip、工具栏和状态栏 | popup 与 command/status 两个独立计划 |
| 文本输入、LCD、FlowLayout | 对应 text-input、digital-display、flow-layout 独立计划 |

上述计划包含各自旧版源文件的逐行或连续行段审计、公开 API、性能预算、视觉矩阵和最终验证结果。本文件负责跨批次索引和剩余能力闭环，不重复复制已完成批次的全部分析。

## 4. `ZzNavigationBar` 代码级边界审计

### 4.1 公开头

| 旧文件行 | 结论 |
|---:|---|
| `ZzNavigationBar.h:1-10` | 公开头依赖旧宏、全局枚举和具体 `ZzSuggestBox`，导致导航 API 与旧 UI 基础设施绑定；不迁移。 |
| `ZzNavigationBar.h:11-21` | 单个 `QWidget` 同时暴露透明度、新窗口、宽度和 Pimpl，职责过宽；新版继续使用独立 Model/View/controller/host。 |
| `ZzNavigationBar.h:23-32` | expander、page、footer、category 和 `QWidget *page` 由同一对象创建，混合展示树和页面所有权；禁止恢复。 |
| `ZzNavigationBar.h:34-47` | 展开、删除、badge、改标题、路由和 display mode 全靠字符串 key；新版只允许强类型 route，展示元数据使用值对象。 |
| `ZzNavigationBar.h:49-57` | 新窗口计数、搜索数据和节点增删信号把应用行为塞入 UI；只有用户激活索引的意图可保留。 |
| `ZzNavigationBar.h:59-61` | 背景绘制可由 palette/style 处理，不需要大而全的自绘宿主。 |

### 4.2 公开实现

| 旧文件行 | 结论 |
|---:|---|
| `ZzNavigationBar.cpp:1-22` | 同时依赖树、footer、菜单、搜索、主题、动画、滚动和布局，证明旧类不是可隔离组件。 |
| `ZzNavigationBar.cpp:23-90` | 构造时固定 300 像素、创建两套 model/view 并连接全局主题；新版尺寸由布局和显示策略决定，主题由 style/palette 传播。 |
| `ZzNavigationBar.cpp:92-112` | 宽度下限和当前模式耦合；可重构为展示层的 regular/compact/adaptive 契约。 |
| `ZzNavigationBar.cpp:113-301` | 多组 add API 同时修改树、页面 map、stack、compact menu 和 signal；新版 builder 只收集拥有值的页面注册和导航元数据。 |
| `ZzNavigationBar.cpp:303-388` | 展开、折叠和递归删除依赖内部节点指针；动态树第一批不迁移，未来如需要必须使用标准 `QAbstractItemModel`。 |
| `ZzNavigationBar.cpp:390-466` | badge 和标题是通用展示数据，但 setter/getter 不应通过 UI 查找字符串 key；新版由不可变节点值和 model reset 更新。 |
| `ZzNavigationBar.cpp:467-526` | UI 直接执行路由、回退和新窗口计数；已有 `ZzNavigationController` 与每窗口 composition root 覆盖这些职责。 |
| `ZzNavigationBar.cpp:527-555` | 搜索数据和背景绘制不属于导航 model；搜索应由应用使用 proxy/completer 组合。 |

### 4.3 私有实现

| 旧文件行 | 结论 |
|---:|---|
| `ZzNavigationBarPrivate.h:1-78` | private 持有页面 stack、两套 view/model、节点指针 map、route list、动画和主题，全都共享可变状态；不作为新版 Pimpl 基础。 |
| `ZzNavigationBarPrivate.cpp:1-220` | 点击处理创建窗口、切 page、维护 route、selection、footer 和 stack；拆分到 controller、host 与应用连接。 |
| `ZzNavigationBarPrivate.cpp:221-250` | event filter 处理 resize/mouse 的宿主行为；只保留可证明的自适应显示状态，不保留全局事件分发。 |
| `ZzNavigationBarPrivate.cpp:251-383` | 每次变化递归遍历整棵树重建 QModelIndex、selection 和展开状态，复杂度与全部节点数绑定；第一批保持平面虚拟化模型。 |
| `ZzNavigationBarPrivate.cpp:384-435` | page stack、raise 和平滑滚动分别由 `ZzPageHost`、窗口 composition 和标准 item view 处理。 |
| `ZzNavigationBarPrivate.cpp:436-549` | display mode 每次创建多个 property animation 并保存节点展开状态；新版只允许固定数量持久动画，减少动效时同步终态。 |

### 4.4 旧导航模型与 footer 模型

| 旧文件行 | 结论 |
|---:|---|
| `DeveloperComponents/ZzNavigationModel.h:1-50` | model 暴露具体节点裸指针和增删 API，调用方可绕过模型事务；不迁移接口。 |
| `DeveloperComponents/ZzNavigationModel.cpp:1-139` | 手写 parent/index/rowCount 并依赖节点内部 QModelIndex，容易在结构变化后留下失效索引。 |
| `DeveloperComponents/ZzNavigationModel.cpp:140-335` | add API 重复生成字符串 key、查找父节点和分配节点；新版使用构建期值校验和强类型 route。 |
| `DeveloperComponents/ZzNavigationModel.cpp:336-414` | 删除和查询返回节点裸指针列表；新版 model 对外只返回节点副本和 `QModelIndex` 数据。 |
| `DeveloperComponents/ZzFooterModel.h:1-28` | footer 被做成另一套具体节点 model，和主 model 共享旧节点类型；通用需求是 placement，而不是第二套业务模型 API。 |
| `DeveloperComponents/ZzFooterModel.cpp:1-80` | 线性查找、裸指针所有权和手工节点删除不迁移；新版可由一个值模型按 placement 投影到固定 footer。 |

## 5. 其余组合替代项的代码级审计

### 5.1 `ZzScrollPage`

| 旧文件行 | 结论 |
|---:|---|
| `ZzScrollPage.h:1-31` | 页面类型同时公开中央 widget、custom widget、内部导航和标题布局；不存在单一可复用协议。 |
| `ZzScrollPage.cpp:1-55` | 构造时创建标题、breadcrumb、stack 和全局 route 连接，页面直接参与应用路由；新版页面 view 不访问导航控制器。 |
| `ZzScrollPage.cpp:56-108` | `addCentralWidget()` 配置手势、滚动、stack 和 ownership；由 `ZzScrollArea` 与页面 layout 显式组合。 |
| `ZzScrollPage.cpp:109-155` | custom widget、navigation、spacing 和 title visibility 是不同层级职责；分别由页面、host 和 layout 管理。 |
| `ZzScrollPagePrivate.h:1-37` | private 持有标题、breadcrumb、stack、route key 和 custom widget，重复页面宿主状态。 |
| `ZzScrollPagePrivate.cpp:1-55` | 切页动画每次创建 opacity effect/animation 并访问全局 route；违反固定动画和窗口隔离要求。 |

### 5.2 `ZzText`、`ZzToggleButton`、`ZzMessageButton` 和 `ZzScrollPageArea`

| 旧文件 | 连续行结论 |
|---|---|
| `ZzText.h/.cpp` | 公开部分重复 `QLabel` 的 style/icon 属性；实现硬编码像素字体、全局主题和自绘换行。新版 typography token、palette、`QLabel` 及图标组件已经覆盖全部通用职责。 |
| `private/ZzTextPrivate.h/.cpp` | QObject private 只保存主题和两个绘制标量，且直接修改 palette；没有独立资源或状态机价值。 |
| `ZzToggleButton.h/.cpp` | 用 `QWidget` 手写 press/release 和 toggled，缺少键盘、cancel、checkable 与按钮无障碍协议，并在每次 release 分配动画；checkable `ZzPushButton` 完整替代。 |
| `private/ZzToggleButtonPrivate.h/.cpp` | 重复保存 pressed/checked/alpha，均已由 `QAbstractButton` 和 style state 提供。 |
| `ZzMessageButton.h/.cpp` | 按钮属性直接持有消息内容、目标 widget、位置、超时和 severity，并在 clicked 内创建消息；应由应用连接 `ZzPushButton::clicked` 与消息 presenter。 |
| `private/ZzMessageButtonPrivate.h/.cpp` | private 只是业务参数袋和手写按压状态；删除后不损失 UI 协议。 |
| `ZzScrollPageArea.h/.cpp` | 固定 75 像素高度并只画圆角背景，无法表达内容尺寸、layout 或语义；普通 widget/palette 足够。 |
| `private/ZzScrollPageAreaPrivate.h/.cpp` | QObject private 只保存 radius 和全局 theme，属于不必要对象与间接访问。 |

## 6. 导航展示增强交付边界

### 6.1 已补齐

- **图标：** `ZzNavigationModel` 通过通用 role 提供 `ZzIconDescriptor`，导航 delegate 已读取 descriptor 并保留 `QIcon` 回退，应用框架导航图标真实绘制。
- **分区：** 静态 section/group 元数据投影为不可选择的 synthetic header，不会被伪装成可路由页面。
- **固定 footer：** 少量路由项由同一值模型按 placement 投影到固定底部，仍由所属窗口的强类型节点驱动。
- **badge：** 有界短文本由 model 局部更新并进入 tooltip/无障碍描述，控件不执行通知查询或业务计算。
- **自适应模式：** regular/compact 可显式设置；adaptive 只根据顶层 QWidget 逻辑宽度选择二者，不访问平台私有 API。

### 6.2 第一批明确不做

- 动态树、任意深度 expander、拖拽重排或运行时节点编辑。
- 内置搜索框、模糊检索、远程搜索、历史和持久化。
- 在 UI 内创建页面、执行路由、维护 back stack 或打开新窗口。
- 为每个节点创建 QWidget、animation、timer、menu 或 proxy model。
- 复制旧字符串 key、裸节点指针、全局主题或全局路由对象。

### 6.3 已证明

- FluentUI 展示角色位于 Foundation，生产实现不依赖 `ZzPureTools`；依赖方向保持 `ZzPureTools -> ZzFluentUI`。
- section/footer/badge 可由任意 `QAbstractItemModel` 的通用 role 表达；`ZzNavigationModel` 只是框架提供的值模型实现。
- 平面十万行 model 继续使用 batch layout；绘制、滚动、激活与 source/proxy 映射不扫描总 row count，结构 reset 才进入 O(N) 冷路径。
- footer 上限固定为六项，每个 pane 始终只有两个 view 和两个 projection，不按节点增长 QObject。
- compact 只改变绘制，Display、ToolTip 与 Accessible role 保持完整；隐藏可见文字不会删除模型语义。
- Windows MSVC/MinGW 和 macOS 源码静态审计确认只使用 Qt 公共 API，没有导航批次平台条件分支；原生编译和真机验证仍明确待办。

## 7. 阶段 10 闭环状态

| 状态 | 组件范围 |
|---|---|
| 已完成生产、测试、性能、视觉与交付记录 | 旧版 51 个公开组件中的全部可复用通用能力，包括导航展示增强 |
| 已由标准 Qt 覆盖，不新增公开类型 | checkbox/radio/slider/progress、输入、菜单、item view、toolbar/status/tooltip、LCD |
| 已由应用组合覆盖，不新增公开类型 | message button、scroll page、text、toggle button、scroll page area |
| 已拆分并交付 | 导航图标、section、footer、badge、自适应 regular/compact、强类型路由与页面宿主 |
| 明确非目标 | 旧版业务命名兼容、内置路由/页面/搜索/新窗口、动态树和全局单例 |

导航展示增强已通过同级功能、性能、视觉、安装、sanitizer、静态分析和架构质量门禁，阶段 10 的旧组件迁移审计于 2026-08-06 关闭。详细提交、测试数量、参考机数据和人工平台验证边界见 `2026-08-06-zzfluentui-navigation-pane.md` 第 17 节。后续新增控件必须由真实产品需求驱动，不再以旧仓库文件清单为输入。
