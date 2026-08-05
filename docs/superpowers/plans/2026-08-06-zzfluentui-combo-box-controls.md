# ZzFluentUI 标准组合框控件实施计划

**目标：** 让标准 `QComboBox` 在应用级 `ZzFluentStyle` 下获得完整、稳定且高性能的 Fluent 选择面板、方向安全箭头和 popup item 状态，同时完整保留 Qt 原生 model/view、editable、validator、completer、键盘、鼠标、滚轮、信号与无障碍语义。

**架构：** 本批不新增空包装 `ZzComboBox`。标准 `QComboBox` 继续拥有选中索引、model、view、line edit、popup 与输入状态；`ZzFluentStyle` 只负责逻辑尺寸、sub-control 几何和绘制。popup item 通过公开 QWidget 父链确认归属于组合框后再绘制，不依赖 Qt 私有类名、object name、动态属性或内部布局。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

## 1. 范围与前置结论

- 本批次继续总体设计阶段 10，只处理标准单选 `QComboBox` 的 closed panel、editable editor、popup list 与质量门禁。
- 非 editable 与 editable 组合框都直接使用 Qt 原生类型，不建立第二份 current index/current text 状态。
- `QComboBox` 继续负责 model ownership、root model index、role、insert policy、duplicates、placeholder、validator、completer、signals、popup 生命周期和输入事件。
- popup 继续由 Qt 创建和定位；style 不替换 view、不修改 popup window flag、不拆装内部 layout，也不接管 show/hide。
- 多选组合框、tag/chip、搜索建议、异步检索、历史记录和业务数据过滤属于后续独立批次。
- `QMenu`、menu bar 与 `QToolTip` 保持下一独立 popup surfaces 批次，本计划不顺带扩大范围。

## 2. 旧版代码审计

旧版 `ZzComboBox`、`ZzComboBoxPrivate`、`ZzComboBoxStyle` 与 `ZzComboBoxView` 只作为视觉参考，以下实现明确不迁移：

- 每个控件实例创建独立 `QProxyStyle` 并在析构中手动删除，增加 QObject、连接、缓存碎片与 style 所有权风险。
- 构造时强制替换 `QListView`、滚动条和浮动滚动条，改变调用方提供的 view、delegate、selection model 与滚动策略。
- 把 view 改为 `NoSelection`，自定义 view 拦截 mouse press 后 `ignore()`，绕过 Qt 原生 selection、activation 与辅助功能状态机。
- 通过 `findChild<QFrame *>()` 猜测 Qt 内部 popup container，修改 window flags、透明属性、object name、layout 和 margins，依赖未承诺的内部对象树。
- `showPopup()` 临时修改全局 `Qt::UI_AnimateCombo`，会影响同进程其他窗口与并发打开的控件。
- 每次打开/关闭动态创建多组 `QPropertyAnimation` 并使用 `DeleteWhenStopped`；lambda 捕获 container、view 与 layout 裸指针，销毁和快速反向操作存在竞态。
- 关闭时向 parent 伪造坐标为 `(-1, -1)` 的鼠标事件，改变焦点与输入语义。
- 固定 35px 高度和每项高度，覆盖字体、DPI、触控密度与平台 style 测量。
- 借用 `SC_ScrollBarSubLine`、`SC_ScrollBarAddPage` 表示组合框文字和箭头区域，违反 `CC_ComboBox` sub-control 契约。
- 手绘 current text，没有完整保留 decoration、icon size、elide、placeholder、RTL、disabled role 与 editable 模式。
- 在 `paintEvent()` 中检查并修改 line edit palette，绘制期间可能触发额外更新。
- QSS、字体图标、Windows DWM helper 和平台条件分支扩大了跨平台差异面。

可保留的视觉意图只有圆角输入 surface、普通/hover/focus/disabled 层次、尾部 chevron、popup hover 与 selected accent；全部由既有 token、palette 与应用级 style 完成。

## 3. 生产代码设计

### 3.1 内容尺寸

修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：

- `sizeFromContents(CT_ComboBox)` 必须先调用 base style，保留字体、图标、文字宽度和 editable editor 测量，再只保证不小于 `96 x 32` 逻辑像素。
- popup item 只在确认属于组合框时保证最小 32 逻辑像素高度；普通 list/table/tree item 不受影响。
- 不设置 fixed size，不读取 screen DPR，不缓存物理像素尺寸。

### 3.2 Sub-control 几何

修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp` 与 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.*`：

- 为 `CC_ComboBox` 明确定义 `SC_ComboBoxFrame`、`SC_ComboBoxEditField` 与 `SC_ComboBoxArrow`。
- 箭头区域使用稳定的 32 逻辑像素宽度；edit field 保留 12px 起始内边距与尾部箭头空间。
- 先在 LTR 逻辑坐标构造矩形，再使用 `QStyle::visualRect()` 映射 RTL；禁止散落平台分支。
- 无效 option 或不支持的 sub-control 回退 base style，不返回越界或负尺寸矩形。
- base label、editable line edit 和原生鼠标命中必须消费同一组几何，避免绘制与点击区域分叉。

### 3.3 Closed panel 绘制

修改 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`：

- panel 复用 `drawInputPanel()` 的 normal、hover、focus 与 disabled 状态。
- current label 委托 base `CE_ComboBoxLabel`，保留 icon、text role、placeholder、elide、alignment 与 editable 分支。
- chevron 使用 `QPainterPath` 绘制，不依赖字体图标或 SVG；颜色按 enabled/disabled palette group 获取。
- popup open 可以使用 Qt 已提供的 state 改变 chevron 方向，但不得为旋转创建 animation、timer 或持久状态。
- editable line edit 继续由 Qt 管理输入和 palette，组合框 frame 只绘制一次。

### 3.4 Popup item 绘制

修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp` 与 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.*`：

- 只通过公开 QWidget parent chain 查找所属 `QComboBox`，不得匹配 `QComboBoxPrivateContainer`、内部类名、object name 或 dynamic property。
- 组合框 popup item 的 hover 使用 `ControlFillHover`，selected 使用 accent indicator 与清晰的选中 surface；disabled item 使用 disabled palette group。
- 在绘制 background/indicator 后委托 base `CE_ItemViewItem` 绘制 decoration、check state、text、focus 与 role 数据；不得手写 model text。
- 普通 item view 和显式使用 `ZzFluentItemDelegate` 的 table/tree 不改变现有行为。
- popup frame 只在能够公开确认归属组合框时使用 Fluent surface/stroke；否则回退 base style。

## 4. 自动测试

新增 `ZzFluentUI/tests/ZzComboBoxControlsTest.cpp` 与 CTest `fluent.combo-box-controls`：

- `CT_ComboBox` 保留 base 测量并满足最小 `96 x 32`；长文本、图标、字体变化和 editable 模式不得被固定尺寸裁切。
- `SC_ComboBoxFrame/EditField/Arrow` 均有效、不重叠、位于控件范围内，并在 RTL 中镜像。
- Light、Dark、HighContrast 下 normal、hover、focus、disabled 与 popup-open panel/chevron 使用已知 token/palette 颜色。
- non-editable 覆盖 current index/text/data、placeholder、decoration role、enabled/disabled item、insert/remove/reset 与 root model index。
- editable 覆盖 line edit、validator、completer、insert policy、duplicates、selection、keyboard editing 与原生信号。
- 键盘覆盖 Up/Down、Home/End、Alt+Down、Escape、Enter；popup 可通过 `view()` 和 `view()->window()` 公共接口观测。
- 鼠标点击箭头和 item 触发 Qt 原生 selection/activated 路径；style 不 override `showPopup()`、`hidePopup()` 或事件处理器。
- `QAccessible` role、name、value、focusable、focused、disabled 与 editable 状态和控件一致。
- 100 个组合框执行 1000 轮 current index、enabled、editable、direction、model 数据与 focus 切换后，回到初始状态并处理 deferred delete，QObject、animation 与 timer 数量不得增长。

扩展既有 `ZzFluentStyleTest`/`ZzFluentStandardControlsTest`：

- 验证 combo metric、geometry 与主题更新只产生预期 repaint/geometry 更新。
- 验证 popup item 分派不会覆盖普通 list/table/tree 或 `ZzFluentItemDelegate`。
- 验证 editable 内部 line edit 不产生双 frame。

## 5. 画廊、安装消费与公开边界

- 在 `examples/ZzFluentControlsGallery` 展示普通、placeholder、icon、editable+completer、disabled、RTL 与可打开 popup 状态；示例只设置展示 model，不包含业务筛选。
- `tests/InstallConsumer/Gui/main.cpp` 从安装包创建 non-editable 与 editable `QComboBox`，验证 style、最小尺寸、model 数据、line edit 和键盘语义。
- 本批不新增公开头；安装消费用于证明已安装 `Zz::FluentUI` 对标准 Qt 组合框提供相同行为。
- 架构审计继续禁止 UI 依赖 repository、database、network、domain、QWindowKit、Qt Private 或第三方实现头。

## 6. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 预构造 100 个标准 `QComboBox`，覆盖 non-editable、editable、icon、disabled 与 RTL；10 帧预热、120 帧正式渲染，记录 P50/P95/max。
- 当前活动 Linux 参考发布环境设置绝对 P95 `<= 16.7 ms`；普通环境只记录数值。
- 执行 1000 轮 current index、enabled、editable、direction 与 model 数据切换，恢复初始状态并处理 deferred delete 后，QObject、animation、timer 数量必须相同。
- paint 热路径不得读取 model 内容、创建 style、view、delegate、animation/timer、调用 `processEvents()`、读文件或访问业务状态。

## 7. 视觉基线

扩展 `ZzFluentScreenshotTest`，增加独立固定尺寸 `combo-box-controls` surface：

- 覆盖 normal、placeholder、icon、editable、focus、hover、disabled、RTL、long text 与打开的 popup。
- popup 通过 `QComboBox::view()` 和 `view()->window()` 获取并合成到固定画布位置，不查找 Qt 私有 container 类型。
- 建立 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张基线。
- closed label、editable text 与 popup item text 纳入显式文字遮罩；panel、chevron、icon、popup frame、hover surface、selected accent 与滚动条仍参加严格比较。
- 更新后人工检查 DPR 1.0 三主题与 DPR 2.0 Light，确认无空白、裁切、重叠、双 frame 或 popup 内容错位。

## 8. 跨平台静态检查

- Windows MSVC、Windows Qt SDK MinGW 与 macOS 只使用 Qt Widgets 公共 API 和标准 C++20；本批不增加平台分支。
- 运行 preset matrix、gate script contract、public headers、完整架构与 Fluent 边界审计。
- 本机不能把源码审计记录成 Windows/macOS 编译、安装消费或真机验证通过。

## 9. 提交顺序

```text
文档：规划Fluent标准组合框批次

记录旧版组合框的内部对象树、动画、所有权和输入语义风险。
确定标准QComboBox加应用级Style的无包装架构。
```

```text
控件：完善Fluent标准组合框样式

实现方向安全的组合框尺寸、sub-control、面板、箭头和popup item绘制。
保留Qt原生model/view、editable、输入与无障碍语义。
```

```text
测试：接入组合框质量与安装消费

补齐model、editable、键鼠、无障碍、对象稳定性、性能、画廊和安装消费者。
覆盖公开头、架构边界和跨平台源码契约。
```

```text
测试：补齐组合框多主题视觉基线

新增三主题、四档DPR的独立组合框与popup参考图。
验证focus、hover、禁用、RTL、editable和selected item状态。
```

```text
文档：记录组合框批次交付结果

记录Linux GCC、Clang、sanitizer、clang-tidy、截图、性能和安装消费结果。
明确Windows与macOS仅完成源码静态审计，远端CI继续暂缓。
```

## 10. 本机验证命令

使用现有环境，不下载 Qt：

```bash
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
```

每个代码提交运行对应 target 与 CTest；最终运行 GCC Release、Clang ASan/UBSan、shared/static `ZzClangTidy`、public headers、shared/static 安装消费、四档截图、benchmark 与画廊 smoke。

## 11. 完成定义

- 标准 `QComboBox` 具有一致 Fluent closed panel 与 popup item，业务 UI 不需要替换为 Zz 包装类型。
- Qt 原生 model/view、current index、editable、validator、completer、signals、键鼠、popup 与无障碍没有第二状态源。
- 生产代码每实例没有额外 QObject、style、view、delegate、animation、timer、事件过滤器、stylesheet 或动态属性。
- Light、Dark、HighContrast x 四 DPR 视觉基线通过。
- 100 个组合框满足参考机帧预算，1000 轮状态切换恢复后无对象增长。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 待验证状态如实记录。

## 12. 交付结果

待实现完成后填写。
