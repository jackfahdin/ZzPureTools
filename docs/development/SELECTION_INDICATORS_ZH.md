# 选中指示条契约

本契约适用于表示选中行、单元格或活动选项的指示条。实现基于 Qt 公开 API，
不修改模型、选择集合、currentIndex 或业务活动状态。

## 外观与布局

| 令牌 | 默认逻辑像素 | 含义 |
|---|---|---|
| SelectionIndicatorThickness | 3 | 指示条厚度 |
| SelectionIndicatorExtent | 16 | 纵向标准长度和原生标签短条长度 |
| SelectionIndicatorLeading | 4 | 行起始边到指示条的距离 |
| SelectionIndicatorContentGap | 3 | 指示条到内容槽位的最小间隔 |

纵向总槽位为 4 + 3 + 3 = 10，与统一前的行式内容默认留白相同。
颜色取主题 Accent，圆角为实际短边的一半；小行高限制长轴长度，防止越界。
槽位不随选中状态改变。绘制、复选框、文字和编辑器使用同一内容区域，
不把展开箭头或树层级缩进计入指示条间隔。

逻辑起始侧跟随 RTL；活动栏按停靠侧使用物理左右边。活动栏图标和徽标
共享剩余内容宽度预算，大徽标不能覆盖图标。横向标签为内容预留厚度加间距的区域。
Pivot 保留与内容宽度适配的下划线；原生 Tab 使用标准短条。

### 标签文字尺寸契约

`ZzFluentStyle::sizeFromContents(CT_TabBarTab)` 必须在基础样式测得的尺寸之外，
增加指示条厚度与内容间距的预算，默认合计 6 个逻辑像素，并补偿基础样式的
未选中标签偏移（Fusion 默认另需 2 个逻辑像素）。横向标签增加高度，
竖向标签增加宽度，与绘制时扣除的槽位配套；不得缩小字体或裁切文字来获得间距。
选中和未选中使用相同预算，RTL 不改变预算大小。宿主应尊重控件的 `sizeHint`，
字号和 DPI 改变后不能继续使用不足以容纳内容的固定高度。窗口过窄时仍允许 Qt
原有的标签滚动或省略行为，这与字体上下被裁切是不同的问题。

`ZzTabControlsTest` 的尺寸契约用例覆盖八种标签方向形状、12/24 pt 字号、LTR/RTL
和选中状态；实际控件用例覆盖 QTabBar、ZzTabBar 与 ZzTabWidget，包含图标和关闭按钮。
在独立进程设置 `QT_SCALE_FACTOR=1/1.25/1.5/2` 可重复缩放检查。
示例“基础控件 / 选中指示条对比”提供中英文混排标题与“大字体标签”开关；
应检查 `Agjp` 的上伸部、下伸部和中文字形是否完整，而不只看短标题。

## 动画与所有权

- 纵向单项切换采用旧条收缩、新条展开，总时长使用 Normal（默认 167 ms），缓动为 InOutSine。
- 横向 Pivot 和 Tab 使用位置、尺寸插值，同样使用 Normal 和 InOutSine。
- 动画对象按视图或控件长期复用；快速反向切换从当前显示状态继续。
- 首次选中直接显示；清空选择只退出旧条，无效索引不获得可见比例。
- reduced motion 使用零时长终态。隐藏、禁用及模型重置不能留下悬挂过渡。
- 普通视图状态按视图隔离，即使共享 Fluent Delegate 也不共享动画目标。
- 视图析构期间的 Hide 事件必须检查对象仍是 QAbstractItemView；仅检查 QPointer 非空不足以保证视图接口可调用。
- 表格整行选择按行归一化，指示条属于首个可见视觉列。单元格选择不转为行选择。
- 多选、批量变化和多活动项保持已有静态标记语义，不把 selectedIndexes 展开为逐项动画对象。
- 活动栏的 Fluent 选中背景和指示条只消费已提交的活动源索引，不使用内部列表鼠标按下产生的临时选中状态。释放后保留延迟激活及模型有效性复核；提交后按共用时序收缩旧条、展开新条，不能先画新条再收回。悬停、键盘焦点和重复点击折叠意图保持独立。

## 入口覆盖

| 入口 | 布局与绘制 | 过渡策略 |
|---|---|---|
| 原生 QListView 行模式、QTreeView、QTableView | ZzFluentStyle → ZzItemViewVisual | 每视图 ZzItemSelectionAnimation |
| 显式 ZzFluentItemDelegate | ZzItemViewVisual，已调整内容 option 防止重复留白 | 同一视图级状态 |
| ZzNavigationView、NavigationPane 主区和底区 | ZzItemViewVisual | 现有导航状态复用 ZzSelectionIndicatorTransition |
| ZzActivityBar 主区和次区 | ZzItemViewVisual、共享内容宽度预算 | 源活动索引驱动的 ZzSelectionIndicatorTransition |
| Pivot | 内容适配的底部条及统一厚度、间距 | 连续滑动变体 |
| QTabBar、ZzTabBar、TabWidget 内部标签栏 | Style 绘制统一短条 | 每标签栏滑动状态 |
| QComboBox 普通列表弹出项 | 统一行式短条和槽位，保留弹出项背板 | 有有效模型索引时使用视图过渡 |
| QComboBox 菜单式弹出项 | 同一短条几何和槽位 | checked 当前项标记，重开弹层直接显示终态 |

原生 IconMode 网格不新增行式短条。Carousel 分页圆点、卡片或按钮上的方向箭头、
拖拽插入线、输入焦点线、进度条和滚动标记属于其他语义，不改变其造型或状态机。

## 验证入口

- `fluent.selection-indicator`：共用状态、原生与 Fluent Delegate 的三视图接入、视图隔离、多选、横向滑动及主题方向矩阵。
- 现有 Fluent ItemDelegate、Navigation、ActivityBar、Pivot、Tab、ComboBox、StandardControls 和 ThemeSnapshot 测试继续作为回归约束。
- ActivityBar 的 mouseActivationMatchesNavigationFrames 使用真实鼠标按下、释放和事件队列，检查提交前没有新条，并与 Navigation 在 0/41/83/125/167 ms 比较指示条像素；覆盖左右物理边、主次分组、RTL 和减少动态效果。
- 设置 `ZZ_INDICATOR_REPORT_DIR` 可让新测试的 `visualMatrix` 保存当前环境截图；这些图片属于本机观察证据，不替代 Linux 参考基线。
- 视觉变更仍需在项目规定的 Linux 参考环境更新受影响基线、关闭更新模式重跑并人工核对；Windows 本机图片不得写入 Linux 基线目录。

## 本次实施状态

2026-09-08 在 Windows 11、Qt 6.11.0、MSVC 环境中完成本机定向回归。
析构阶段崩溃的复现用例通过，Fluent 与架构检查共 67 个独立 CTest 条目全部通过。
安装消费检查在该析构修复前通过，修复后未重跑。

Linux 参考截图、正式性能基准以及完整原生窗口人工交互验收仍未完成，
不能将本机回归通过解释为完整视觉或跨平台验收通过。
具体执行记录及未确认条目保留在
docs/superpowers/plans/2026-09-08-selection-indicator-unification.md。

### Tab 文字裁切修复验收

- 实现按同一厚度和间距令牌补齐自然尺寸，并补偿基础样式的未选中偏移；公开方法签名不变，头文件补充测量与绘制配套的契约。
- 新增 64 个尺寸契约场景和 12 个实际控件场景。相同测试程序使用修复前 DLL 时 76 个场景失败，修复后全部通过。
- DPR 1/1.25/1.5/2 下运行完整 ZzTabControlsTest，每档 106 项通过；Windows 原生后端运行新增场景，78 项通过（含初始化和清理）。
- Fluent 与架构回归共 67 个独立 CTest 条目通过。公开头检查首次因未初始化 MSVC 环境失败，在开发者环境中重跑后通过。
- ZzPureToolsExample 编译成功，包含混排标题和大字体开关；原生 ZzTabWidget 的 24 pt 截图已观察，中文及 Agjp 未发生上下裁切。未把控件截图检查表述为完整示例的人工交互验收。
- 本机证据保存在 build/indicator-tests 下的 tab-layout-negative-control.txt、tab-layout-dpr-*.txt、tab-layout-native.txt、tab-layout-regression-final.log 和 reports/tab-layout；不替代 Linux 参考基线。

### 活动栏鼠标切换闪跳修复验收

- 根因通过鼠标关键帧复现：活动项提交前，内部 QListView 的临时 State_Selected 使新条提前出现。修复将 Fluent 选中背景及指示条的静态依据统一为已提交的活动源索引，保留延迟激活、安全复核、悬停、焦点和多活动项语义。
- 16 个新增场景覆盖左右物理边、同组及跨组、LTR/RTL 和减少动态效果；修复前提交前像素断言失败，修复后四档缩放均通过。提交后的五个受控时间点与 Navigation 指示条像素一致。
- 测试对照导航使用独立的普通模型，不将活动栏 Area 数据交给导航解释；这两类模型的私有角色可能重合，不能仅因行数相同就共用测试模型。局部样式测试也显式给活动栏内部视图设置 Fluent 样式。
- Windows 原生后端新增场景 18 项通过（含初始化及清理）；Fluent 与架构回归 67 项通过；ZzPureToolsExample 编译成功，并补充按住、释放活动项的观察说明。
- 完整 ActivityBar 测试在 DPR 1 和 2 下各 58 项通过，在 1.25 和 1.5 下各 56 项通过、2 项字体图标像素测试失败。这两个失败在修复前 DLL 上也能复现，本次不扩展修复、不标记为通过。
- 本机日志：build/indicator-tests/activity-click-before.txt、activity-click-after.txt、activity-click-dpr-*.txt、activity-click-native.txt、activity-click-regression.log，以及 activity-icon-baseline-*.txt。未更新 Linux 截图基线，未进行完整工作区人工交互验收。
