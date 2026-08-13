# ZzFluentUI 广度扩展详细实施计划

计划日期：2026-08-13  
目标分支：master  
实施目录：/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro  
主验证平台：Linux 参考机、Qt 6.11.1、GCC 15.2.0、C++20、CMake Presets  
跨平台边界：Windows MSVC、Windows Qt MinGW、macOS arm64/x86_64 在没有原生 runner 时只做静态检查，不把静态检查写成真实构建或真机验收通过。

## 1. 计划目的

当前 ZzFluentUI 已有 37 个公开可组合组件，组件深度已经覆盖主题、输入、导航、反馈、弹层、卡片、标签页、滚动和日期等核心路径。下一阶段重点从继续增加单个复杂控件切换为扩大可用场景覆盖面，让标准 Qt Widgets、旧版示例中常见的交互类型，以及新项目的 Gallery/Example 形成完整 Fluent 使用面。

本计划不把旧版头文件数量当作目标。旧版 /home/zz/Jackfahdin/github/ZzPureToolsPro/ZzFluentUI 中存在大量以下模式：

- 仅为换颜色、圆角或尺寸而继承 QCheckBox、QComboBox、QSlider、QProgressBar、QMenu、QToolBar 等标准控件；
- 每个控件创建独立 QProxyStyle、stylesheet、事件过滤器或动画对象；
- 把旧 Example 的页面导航、业务卡片、广告卡片和屏幕能力耦合进 UI 库；
- 复制 Qt 已有的 model/view、键盘、剪贴板、无障碍和 popup 状态机。

这些实现不直接迁移。新项目应通过应用级 ZzFluentStyle 覆盖标准控件视觉，通过少量真正有独立语义的 Zz 组件补齐 Fluent 交互，再由 ZzPureToolsExample 组合展示。

## 2. 审计基线

### 2.1 新项目当前公开组件

当前公开组件数为 37，清单以 README.md 为准：

| 分类 | 当前能力 |
|---|---|
| 基础与布局 | ZzPushButton、ZzSplitButton、ZzIconButton、ZzToggleSwitch、ZzProgressRing、ZzSpinBox、ZzDoubleSpinBox、ZzScrollBar、ZzScrollArea、ZzFlowLayout |
| 输入与选择 | ZzPasswordBox、ZzKeyBinder、ZzColorPicker、ZzRatingControl、ZzSuggestBox、ZzMultiSelectComboBox、ZzRoller、ZzRollerPicker、ZzCalendar、ZzCalendarPicker |
| 导航与内容 | ZzBreadcrumbBar、ZzNavigationView、ZzNavigationPane、ZzTabBar、ZzTabWidget、ZzPivot、ZzCarouselView、ZzExpander、ZzDrawer、ZzFluentItemDelegate |
| 反馈与表面 | ZzMessageBar、ZzInfoBadge、ZzContentDialog、ZzTeachingTip、ZzActionCard、ZzImageCard、ZzFluentTitleBar |

后续必须复用的基础设施：

- ZzFluentStyle：应用级统一 style，不创建每控件 style；
- ZzFluentPainter、ZzItemViewVisual：稳定表面、焦点、指示条和 popup 绘制原语；
- ZzThemeSnapshot：颜色、尺寸、字体和动效令牌的不可变 O(1) 快照；
- ZzIconDescriptor、ZzIconCacheKey 和现有字体/SVG 资源：图标不可新增第二套缓存；
- ZzScrollArea、ZzTabWidget、ZzNavigationPane、ZzContentDialog：已有容器语义，不用旧版页面类重复包装。

### 2.2 旧版差异分类与处理结论

| 旧版类型 | 新项目处理 | 结论与理由 |
|---|---|---|
| ZzAcrylicUrlCard | 不迁移 | 产品/网络语义和 Acrylic 背景耦合；新 WindowKit 软件材质不等于卡片业务背景。 |
| ZzCheckBox | 标准控件覆盖 | 使用 QCheckBox + ZzFluentStyle；旧类没有独立状态语义，新增包装会重复 checked、键盘和无障碍。 |
| ZzComboBox | 标准控件覆盖 | QComboBox popup、editable、model 和键盘由 Qt 管理，现有 style 已有复杂控件绘制入口。 |
| ZzFluentUIGlobal | 不迁移 | 旧宏/全局定义不属于可组合控件；新项目使用明确头文件和主题快照。 |
| ZzInteractiveCard | 复用 ZzActionCard/ZzImageCard | 两类卡片已经覆盖操作与图片展示，禁止再造同义卡片。 |
| ZzLCDNumber | 标准控件覆盖 | QLCDNumber 的数值、进制、段样式和溢出语义由 Qt 保留，frame 已由 ZzFluentStyle 覆盖。 |
| ZzLineEdit | 标准控件 + 现有组件 | 普通编辑使用 QLineEdit，密码使用 ZzPasswordBox，建议使用 ZzSuggestBox；不迁移旧 focus/style 逻辑。 |
| ZzListView | 标准控件覆盖 | QListView + ZzFluentStyle/ZzFluentItemDelegate 已支持统一选中背板和指示条。 |
| ZzMenu | 标准控件覆盖 | QMenu、QAction、submenu、shortcut 和 popup 生命周期必须保持 Qt 原生来源。 |
| ZzMenuBar | 标准控件覆盖 | QMenuBar 的平台菜单语义和助记键不能被业务包装类复制。 |
| ZzMessageButton | 组合 ZzMessageBar | 消息按钮只表达关闭/命令 intent；ZzMessageBar 已拥有展示和 closeRequested 语义。 |
| ZzNavigationBar | 复用 ZzNavigationPane + ZzPureTools | 旧类混合页面路由和 UI；新架构要求 UI 不创建页面、不访问业务模型。 |
| ZzPlainTextEdit | 标准控件覆盖 | QPlainTextEdit 的文档、输入法、撤销、选择和上下文菜单不可复制。 |
| ZzPopularCard | 不迁移 | 营销/热门内容是产品层，不是 Fluent 基础库契约。 |
| ZzProgressBar | 标准控件覆盖 | QProgressBar 的范围、值和不确定状态由 Qt 保留，style 已覆盖 CE_ProgressBar。 |
| ZzPromotionCard | 不迁移 | 促销文案、图片和业务点击属于 Example/产品组件，不进入基础 UI 库。 |
| ZzPromotionView | 复用 ZzCarouselView | 轮播容器已有模型、键盘和可见项语义，禁止为促销场景另建一套。 |
| ZzRadioButton | 标准控件覆盖 | QRadioButton 的 autoExclusive、button group、键盘和无障碍由 Qt 管理。 |
| ZzReminderCard | 复用 ZzActionCard | 提醒卡片的通用交互由 ActionCard 的标题、说明和 intent 表达。 |
| ZzScrollPage | 复用 ZzScrollArea + AppCore 页面壳 | 滚动容器和页面生命周期必须分离，不能迁移旧版 route/page 混合类。 |
| ZzScrollPageArea | 标准 QWidget/布局 + token | 只是旧页面的装饰区域，没有足够独立的公开语义。 |
| ZzSlider | 标准控件覆盖 | QSlider 的 range、tracking、orientation、RTL 和键鼠交互由 Qt 保留，style 已覆盖 slider。 |
| ZzStatusBar | 标准控件覆盖 | QStatusBar 的永久/临时消息和布局由 Qt 保留，style 覆盖表面。 |
| ZzTableView | 标准控件覆盖 | QTableView 与统一 item view visual 配合，不新增包装类。 |
| ZzTearOffWidget | 复用 ZzTabBar/ZzTabWidget intent | 新项目已将拖出表达为 intent；不提供旧版抽象工厂和隐式顶层窗口。 |
| ZzText | 使用 QLabel + ZzTypographyToken | 纯文字包装会增加 API 但不能增加语义；富文本由调用方选择 Qt 文本控件。 |
| ZzToggleButton | checkable ZzPushButton | Qt 的 QPushButton 已提供命令切换所需的 checked、toggled、键盘和无障碍语义；不新增同义包装类。 |
| ZzToolBar | 标准控件覆盖 | QToolBar action、dock、overflow 和平台行为由 Qt 保留。 |
| ZzToolButton | 标准控件 + ZzIconButton | 普通 action 使用 QToolButton，纯图标命令使用现有 ZzIconButton。 |
| ZzToolTip | 标准 QToolTip + ZzTeachingTip | ToolTip 是短暂被动态，TeachingTip 是持久交互反馈，两者边界已经明确。 |
| ZzTreeView | 标准控件覆盖 | QTreeView + ZzFluentItemDelegate/ZzFluentStyle 已覆盖树分支、选中和展开语义。 |

阶段目标不是旧版 52 个头文件全部同名存在，而是：

1. 标准 Qt 控件在 Fluent style 下有完整、可测试、可截图的视觉和交互覆盖；
2. 新增真正缺失的语义组件；
3. 旧版 Example 的业务卡片、页面壳和路由代码不污染基础库；
4. 文档明确哪些能力由标准 Qt 类型提供。

## 3. 广度目标与非目标

### 3.1 目标结果

- 旧版 ZzToggleButton 映射为 `ZzPushButton + setCheckable(true)`，公开组件数保持 37；
- ZzFluentStyle 对标准控件的覆盖、尺寸、状态、RTL、焦点和高对比断言形成独立标准表面验收组；
- Gallery 和 ZzPureToolsExample 展示基础命令、输入、数据视图、菜单/工具栏、状态栏、数字显示和弹层的完整组合；
- 旧版 31 个候选全部有迁移、复用或拒绝迁移记录；
- 新增公共 API、安装消费、截图基线、无障碍和性能数据可追溯；
- 不引入每实例 style、stylesheet、平台 native API、Qt Private、业务模型访问或无界对象增长。

### 3.2 明确不做

- 不把旧版业务卡片、促销组件、屏幕能力和页面路由迁入 ZzFluentUI；
- 不为纯样式差异创建空的 ZzCheckBox、ZzComboBox、ZzSlider、ZzProgressBar、ZzTableView 等包装类；
- 不新增 ZzTabView、ZzColorDialog、ZzToolTip、ZzTearOffWidget 等已有等价能力的同义 API；
- 不在本路线实现数据模型、网络搜索、文件读取、全局快捷键注册或业务命令执行；
- 不把 Windows/macOS 未执行结果写成已通过；
- 不在没有独立基线和噪声分析前修改性能阈值。

## 4. 共用实现标准

### 4.1 标准控件采用 style-first

标准 Qt 控件生产代码集中在：

- ZzFluentStyle.cpp：pixelMetric、sizeFromContents、drawPrimitive、drawControl、drawComplexControl 和 subControlRect 分派；
- ZzFluentStylePrivate.cpp：纯绘制和几何算法；
- ZzThemeSnapshot、ZzFluentPainter、ZzItemViewVisual：颜色、尺寸、字体和表面原语。

绘制热路径必须：

- 不创建临时 QObject、动画、定时器、大容器、SVG 解析器或文件对象；
- 不调用 setStyleSheet、不构造裸主题颜色、不写关键尺寸魔数；
- 保留 Qt 的 QAction、model/view、shortcut、focus、IME、selection、popup、RTL 和 accessibility 状态来源；
- 覆盖 Normal、Hover、Pressed、Checked/Selected、Disabled、Focus、RTL 和 HighContrast；
- 只在真的改变已有视觉契约时更新截图基线，并在同一逻辑提交中完成。

### 4.2 旧版 ToggleButton 的组合映射

新版不新增 `ZzToggleButton` 四文件包装类。命令切换使用 `ZzPushButton` 的原生
`setCheckable(true)`，由 `QAbstractButton` 管理 checked、toggled、鼠标取消、
Space/Enter、焦点和无障碍协议；`ZzFluentStyle` 根据 `State_On` 绘制选中 surface。
需要轨道和滑块表达的二值设置继续使用 `ZzToggleSwitch`，两者语义边界清晰。
`ZzButtonControlsTest` 和 Example smoke 合同分别锁定这两种用法。

公开契约：

- 不重复声明基类已有 checked 属性；
- 可复用既有 ZzButtonAppearance；
- checked 状态只改变 Fluent surface、文字/图标对比度和 focus ring，不改变布局尺寸；
- 默认 size hint、焦点、空格键、Enter 键、助记键和无障碍角色继承 QPushButton；
- 主题切换只触发有限重绘，不创建动画；
- 如果测试证明 QPushButton 的 checked accessibility 和视觉分派已完全足够，则取消新增类并改为标准 QPushButton::setCheckable(true)，不能为了数量保留空壳。

## 5. 分批实施顺序

### 第 0 批：广度盘点与标准表面合同

目的：先把旧版差异决策和标准控件覆盖变成可执行验收，不新增生产控件。

修改文件：

- ZzFluentStandardControlsTest.cpp：补充 QCheckBox、QRadioButton、QSlider、QProgressBar、QComboBox、QLineEdit、QPlainTextEdit、QLCDNumber、QMenu、QMenuBar、QToolBar、QStatusBar、QListView、QTableView、QTreeView 真断言；
- ZzFluentScreenshotTest.cpp：增加固定尺寸 standard-breadth 场景，覆盖三主题和四 DPR；
- ZzFluentControlsGalleryPrivate.cpp：加入 Standard surfaces 分区，只组合标准 Qt 控件；
- README.md：说明标准 Qt 控件由 ZzFluentStyle 提供 Fluent 外观，不要求同名 Zz 包装；
- PLATFORM_SUPPORT_ZH.md：补充标准控件覆盖属于 Qt Widgets 公共 API，真实交互仍需按平台清单验收；
- 本计划文档：记录实施结果和截图/测试证据。

真断言：

- 基础 property、minimum/size hint、focus、disabled、hover、pressed/checked 状态可设置且重绘非空；
- QSlider 横向/纵向、RTL、range/value/tracking 和键盘步进保持 Qt 语义；
- QCheckBox/QRadioButton checked、autoExclusive、Space 键和 accessible role 保持 Qt 语义；
- QComboBox editable/non-editable、popup、currentIndex、keyboard navigation 和 model 保持 Qt 语义；
- QMenu/QMenuBar/QToolBar 的 QAction 文本、shortcut、submenu、overflow 和 popup 生命周期没有被 style 破坏；
- QProgressBar 确定/不确定状态、方向和 textVisible 保持 Qt 语义；
- QLCDNumber mode、segmentStyle、overflow 和 display 保持 Qt 语义；
- List/Table/Tree 的 model、selection、RTL、alternate row、scroll 和统一选中指示条保持既有视觉契约；
- 标准控件 1000 次状态切换后对象数量不增长，style 不创建每控件私有对象。

验收命令：

    cmake --preset linux-gcc-reference -DZZ_BUILD_EXAMPLES=ON
    cmake --build --preset linux-gcc-reference --parallel 2
    ctest --preset linux-gcc-reference --output-on-failure -R 'fluent\.(standard-controls|screenshot-)'
    ctest --preset linux-gcc-reference --output-on-failure -R Architecture

视觉变更必须先运行现状测试，再使用 ZZ_UPDATE_SCREENSHOTS=1 重采 standard-breadth 的 12 张基线，关闭更新模式重新比较，并人工检查 Light/Dark/HighContrast、DPR 100/125/150/200 和 RTL。

提交标题：测试：补齐Fluent标准控件广度合同。

### 第 0 批实施结果（2026-08-13）

本批没有新增标准 Qt 控件的同义 `Zz` 包装类，保持 Qt 公共 API 负责模型、选择、
键盘、弹出、无障碍和 RTL 语义，`ZzFluentStyle` 负责应用级视觉覆盖。

- `ZzFluentStandardControlsTest` 新增选择范围、文本与 popup、菜单栏/工具栏/状态栏、
  列表/表格/树模型和选择、禁用/焦点/主题绘制以及 1000 次状态切换对象预算合同。
- `ZzFluentScreenshotTest` 新增固定 `standard-breadth` 场景，覆盖 Light、Dark、
  HighContrast 三主题和 DPR 100/125/150/200；文字遮罩同时覆盖标准菜单栏、工具栏
  和状态栏，继续复用原有像素比较流程。
- `ZzFluentControlsGallery` 增加 `Standard surfaces` 分区，只使用本地固定 Qt model，
  不访问业务数据；README 和平台支持文档同步说明标准控件覆盖边界。

Linux Qt 6.11.1/GCC 15.2.0 的 `linux-gcc-debug` 构建已通过，标准控件测试 18 项
全部通过，标准广度截图四个 DPR 档均在更新模式和关闭更新模式下通过；Windows MSVC、
Windows MinGW 与 macOS 本批仍只做静态检查，未声明真机通过。

### 第 1 批：命令切换语义合同

目的：新增唯一一个有独立 Fluent 语义的旧版缺失控件，不复制 Qt checked 状态机。

修改范围：

- 在 `ZzButtonControlsTest` 增加 checkable `ZzPushButton` 合同；
- Gallery 增加明确的命令切换按钮示例，不新增公共类型；
- Example smoke 检查该按钮的 checkable 属性和原生状态；
- README 与本计划记录旧版 `ZzToggleButton` 的组合映射。

行为合同：

- 主区鼠标、Space、助记键、focus 和 accessibility 与 QPushButton 一致；在 Qt
  对话框默认按钮上下文中，Return/Enter 继续遵循 QPushButton 原生激活语义；
- `setCheckable(true)` 由应用在需要命令切换语义时设置，checked 状态由 Qt 基类拥有；
- 重复 setChecked 不重复发业务信号；
- 禁用态拒绝交互但仍绘制可读文字；
- ZzButtonAppearance 和主题 snapshot 控制 surface，不允许组件写裸色值；
- 不创建动画、定时器、event filter 或 popup；
- 1000 次 checked/hover/focus 切换后子对象数量、style 地址和几何尺寸稳定；
- 无障碍名称来自 text/accessibility properties，不能用内部状态字符串替代。

提交标题：测试：固化可选中PushButton切换语义。

### 第 2 批：Example 广度串联与旧版功能映射

目的：将标准表面和 checkable `ZzPushButton` 串联进新 ZzPureToolsExample，验证前后端分离和实际视觉，不把旧页面代码直接迁入库。

修改范围：

- Example 展示页/Presenter/Model，仅复用旧版页面的展示意图和资源；
- 不复制旧版 ZzNavigationBar、ZzScrollPage、ZzPromotion*、ZzAcrylicUrlCard 实现；
- 业务数据由现有 Example model 提供，控件页只连接 intent 和展示模型；
- 增加标准控件、菜单、工具栏、状态栏、表格、树、列表、数字显示、进度和命令切换按钮组合页面；
- 更新 Example 截图烟测，仅在页面视觉确实改变时重采对应基线。

验收重点：

- 鼠标单击一次即可切换/导航，不引入双击依赖；
- 选中指示条、文字、内容安全区域不重叠；
- Setting/About 与其他导航项之间的 hover/selected 状态即时收敛；
- Activity 日志到达尾部时自动跟随，用户手动上移后保持阅读位置；
- checkable `ZzPushButton` 点击任意可见 surface 均生效，不要求命中内部圆点；
- 页面在 Linux 物理桌面/X11/Wayland 手工清单上记录实际结果，offscreen 只能作为自动测试。

提交标题：示例：串联Fluent标准表面与命令切换。

### Example 集成审计结果（2026-08-13）

新版 `ZzPureToolsExample` 已经完成标准表面组合，不需要直接迁移旧版页面类：

| 旧版展示意图 | 新版路由/实现 | 验证边界 |
|---|---|---|
| 首页与快捷导航 | `home`，`ZzExampleGalleryPage` + `ZzExampleNavigationPresenter` | 路由卡片单击触发导航，搜索、前进/后退均走 `ZzNavigationController` |
| 基础控件与设置 | `controls`，Gallery 页面 | `QLineEdit`、`QPlainTextEdit`、`QComboBox`、`QCheckBox`、`QRadioButton`、`QSlider`、`QProgressBar` 和 `ZzToggleSwitch` 组合展示 |
| 列表/表格/树 | `list-view`、`table-view`、`tree-view`，Data 页面 + ViewModel/Presenter | Qt model/view、统一 delegate、筛选/追加/恢复意图保持前后端分离 |
| 卡片、轮播、数字显示 | `cards`，Cards 页面 + ViewModel | `ZzActionCard`、`ZzImageCard`、`ZzCarouselView`、`QLCDNumber` 组合展示 |
| 导航、反馈、图标 | `navigation`、`feedback`、`icons`，Showcase 页面 | 复用现有 Zz 组件和 SVG/字体图标缓存，不引入旧版全局状态 |
| 窗口/平台/设置/关于 | `platform`、`settings`、`about`，System 页面 + Presenter | 设置通过 Context/SettingsStore 协调，UI 只发送 intent；活动 Dock 使用标准 `QDockWidget`/`QListView` |
| 旧版日志页 | WindowShell 活动 Dock | 共享 `ZzExampleActivityModel`，尾部跟随、手动上翻暂停、回到底部恢复 |

本阶段在 smoke 控制器中增加了页面级标准表面组合合同：导航到 Controls、List、Table、
Tree、Cards 和 Settings 时，验证对应标准控件、Qt model、delegate 及既有 Fluent 控件
真实存在；该合同只读取 UI 结构，不访问业务服务，也不改变正常运行路径。

Linux Qt 6.11.1/GCC 15.2.0 已通过 `example.puretools-integration`、英文翻译、三种
关闭守卫、多窗口、四档综合截图和 Activity model 测试。Windows MSVC、Windows MinGW
与 macOS 仍未进行真机验证。

### 第 3 批：性能、安装消费和平台静态检查收尾

目的：将广度扩展纳入质量门禁，但不虚构 Windows/macOS 结果。

性能：

- 扩展现有 ZzBasicControlsBenchmark，或新增独立 benchmark.fluent-standard-surfaces；
- 采集标准表面固定尺寸 render、checkable `ZzPushButton` 状态切换、对象数量和 style/cache 稳定性；
- 使用 ZzBenchmarkMetadata::populate 和 ZzPerformanceReporter，报告带 Qt、编译器、DPR、平台、GPU、commit 和 preset；
- local-release-xvfb 至少三轮噪声采样；新指标先进入 observe，有独立基线后再决定 gate；
- 不修改既有阈值，不把新场景直接加入正式回归循环，直到 schema、基线和比较器合同完整。

安装消费：

- tests/InstallConsumer 新增只链接 Zz::FluentUI 的标准控件和 checkable `ZzPushButton` 编译/运行消费；
- 验证 shared 与 static 的 include、导出符号、moc、资源和 CMake package 不暴露 private 头或旧版路径。

静态检查：

- Architecture 确认新代码无 stylesheet、裸主题色、QWindowKit、Qt Private、平台 native API 和业务模型依赖；
- Linux GCC/Clang-Tidy 执行真实检查；
- Windows MSVC、Windows MinGW、macOS 只检查条件编译、公共头、链接方向和 Qt 公共 API，未执行不得写成通过。

提交标题：质量：完成Fluent广度扩展门禁。

## 6. 测试与截图矩阵

每个新增公开语义组件必须有独立 QTest target；标准控件覆盖集中在 ZzFluentStandardControlsTest，不为包装类数量复制空测试。

最低断言维度：

1. 状态/信号幂等：重复 setter 不重复发信号；
2. Qt 语义保留：range、model、selection、popup、shortcut、focus、IME、RTL、accessibility；
3. 绘制非空：Light/Dark/HighContrast 下固定尺寸 render 不能透明或单色空白；
4. 几何安全：文本、图标、指示条、焦点环不重叠，DPR 四档尺寸稳定；
5. 生命周期：隐藏/销毁/重复切换后无残留 timer、animation、event filter 或子对象增长；
6. UI/业务边界：测试只提供 QWidget、Qt model 和展示属性，不构造 repository、网络或数据库。

新增或改动视觉时使用现有 ZzFluentScreenshotTest：

    ZZ_UPDATE_SCREENSHOTS=1 ctest --preset linux-gcc-reference --output-on-failure -R 'fluent.screenshot-'
    ctest --preset linux-gcc-reference --output-on-failure -R 'fluent.screenshot-'

基线覆盖 Light、Dark、HighContrast、DPR 100/125/150/200、LTR/RTL、Normal/Hover/Pressed/Checked/Selected/Disabled/Focus，以及 popup、菜单、工具栏、状态栏和 item view 边界。

## 7. CMake、提交和证据规则

所有命令使用 CMakeLists.txt 与 CMakePresets.json 配合：

- 默认 Linux 验证使用 linux-gcc-reference；
- 功能验证可使用 linux-gcc-debug 或 linux-gcc-release；
- benchmark 使用 linux-gcc-benchmarks；
- 不通过手工 qmake、临时编译命令或未记录 Qt 路径替代 preset；
- Windows MinGW 静态检查确认没有 MSVC 专属选项；macOS 确认没有 Cocoa/Objective-C 依赖扩散到公共 Widgets 代码。

每次提交前执行 git diff --check 和 git status --short。提交标题使用中文简述，正文用中文说明行为、边界、验证命令和未执行平台。temp_image/、build/、install/、临时性能报告、旧版源码和未经审核的截图不得提交。

## 8. 退出条件

- 旧版 31 个候选全部完成迁移/复用/拒绝迁移记录；
- 标准 Qt 控件 Fluent 视觉和 Qt 原生交互合同通过；
- checkable `ZzPushButton` 具备公开行为合同、Example 展示、测试和安装消费；`ZzToggleButton` 不作为独立公开类型；
- Linux GCC Debug/Release/Reference、Static、LTO 相关定向门禁通过；
- Clang-Tidy、Architecture、安装消费和适用 Sanitizer 通过；
- 变更视觉基线完成三主题、四 DPR、RTL 必要场景复核；
- Example 串联页面不包含 UI 内业务逻辑，不依赖旧版页面类；
- 新 benchmark 有统一报告、三轮噪声记录和独立逐指标策略，未验证指标保持 observe；
- Windows MSVC、Windows MinGW、macOS 实际状态分别记录为静态检查或未执行；
- README、平台支持文档、本计划实施结果和人工验收证据一致；
- 工作区不提交 temp_image/。

## 9. 预期提交记录

    文档：规划FluentUI广度扩展
    测试：补齐Fluent标准控件广度合同
    测试：固化可选中PushButton切换语义
    示例：串联Fluent标准表面与命令切换
    质量：完成Fluent广度扩展门禁

任何阶段若发现标准控件无法在 ZzFluentStyle 中保持 Qt 语义，应先增加失败测试并修正 style/几何算法；只有证明需要独立公开语义后，才允许新增新的 Zz 类。
