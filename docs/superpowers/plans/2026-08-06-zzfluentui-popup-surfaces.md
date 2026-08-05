# ZzFluentUI 标准弹出表面实施计划

**目标：** 让标准 `QMenu`、`QMenuBar` 与 `QToolTip` 在应用级 `ZzFluentStyle` 下获得一致、清晰且高性能的 Fluent popup surface、菜单项状态和提示面板，同时完整保留 Qt 原生 action、助记键、快捷键、子菜单、定位、计时、输入与无障碍语义。

**架构：** 本批不新增 `ZzMenu`、`ZzMenuBar` 或 `ZzToolTip` 空包装类。`QMenu/QMenuBar` 继续拥有 `QAction`、active/default action、submenu 与 popup 生命周期，`QToolTip` 继续负责延迟、显示时长、屏幕定位和文本布局；`ZzFluentStyle` 只负责逻辑尺寸、palette、primitive 与 control 绘制，不读取业务数据或建立第二状态源。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

## 1. 范围与前置结论

- 本批次继续总体设计阶段 10，只处理标准 `QMenu`、`QMenuBar` 与 `QToolTip` 的通用 Fluent 外观和质量门禁。
- `QAction` 继续负责 text、icon、shortcut、checkable、checked、enabled、visible、separator、section、menuRole 与 triggered/toggled 信号。
- `QActionGroup` 继续负责 exclusive/non-exclusive 选择，style 不复制 checked 状态。
- `QMenu` 继续负责 action geometry、default/active action、submenu、tear-off 配置、keyboard search、mnemonic、popup/exec、screen placement 与关闭策略。
- `QMenuBar` 继续负责 action navigation、Alt/mnemonic、overflow、native menu bar 策略与菜单激活；style 不查找或替换 Qt 内部 extension button。
- `QToolTip` 继续负责 wake-up/fall-asleep delay、显示时长、rich text、word wrap、多屏边界和 hide 行为；style 不创建独立 tooltip window。
- 带交互控件、任意 custom widget、命令面板、toast、teaching tip、flyout、context flyout 和业务异步内容不属于标准 tooltip，后续需要时单独设计公开组件。
- 组合框 popup 的 `CE_ItemViewItem/CE_MenuItem` 特殊路径继续优先分派，不得被普通菜单绘制覆盖。

## 2. 旧版代码审计

旧版 `ZzMenu`、`ZzMenuPrivate`、`ZzMenuStyle`、`ZzMenuBar`、`ZzMenuBarStyle`、`ZzToolTip` 与 `ZzToolTipPrivate` 只作为视觉参考，以下实现明确不迁移：

- 每个菜单和 menu bar 创建独立 `QProxyStyle`，控件析构时手动 `delete style()`，增加 QObject、连接、缓存碎片和 style 所有权风险。
- 菜单构造时覆盖 `Qt::Popup/FramelessWindowHint/NoDropShadowWindowHint`、透明属性和 object name，并在 `showEvent()` 修改窗口位置，改变平台 popup、阴影、焦点和多屏定位语义。
- 每次菜单显示都按 DPR 分配整窗 `QPixmap`、同步 `render()` 全部内容，再新建 `QPropertyAnimation` 滑动位图；这会复制绘制成本、占用峰值内存，并使快速打开/关闭产生延迟删除对象。
- 动画 lambda 捕获裸 `this/d`，且完成前 popup 可关闭或销毁；`DeleteWhenStopped` 不能消除生命周期竞态。
- `ZzMenuStyle::sizeFromContents()` 在每个 action 测量期间再次扫描整个菜单的 icon/submenu，形成 O(action²) 路径；同时用 mutable `_isAnyoneItemHasIcon/_iconWidth` 保存跨菜单绘制状态，结果依赖调用顺序。
- 旧菜单通过 action 动态属性和字体字形传递图标，在 paint 中调用 `actionAt()`、读取属性、拆分文本并手绘 shortcut，绕过 `QIcon` mode/state、助记键、字体度量、elide、RTL 和平台 action 布局。
- 旧菜单用固定百分比 padding、硬编码灰色和固定 32px 项高，未完整处理 section、default item、exclusive/non-exclusive check、widget action、长 shortcut、不同字体与高对比度。
- 旧 menu bar 通过 `findChild<QToolButton *>()` 和内部 object name `qt_menubar_ext_button` 猜测 overflow 按钮，未检查空指针便访问，并删除/替换 Qt 内部菜单。
- 旧 menu bar 去除文本中的 `&` 后手绘 label，破坏助记键；icon/text 几何依赖当前 widget height，RTL、字体增长和长翻译可能重叠。
- 旧 tooltip 是另一套 `QWidget` popup，不复用 `QToolTip` 的平台延迟、屏幕边界、文本布局和辅助功能；它通过 parent event filter 接管 Enter/Leave/MouseMove。
- tooltip 的 show/hide 使用多个不可取消 `QTimer::singleShot`，快速反向 hover 会执行过期回调；每次显示还创建新的 opacity animation，并始终从全局 cursor 位置定位。
- custom widget setter 会接管并延迟删除调用方 widget，所有权契约不清晰；交互内容却设置 `WA_TransparentForMouseEvents`，语义相互冲突。

可保留的视觉意图只有圆角 popup surface、细描边、菜单 hover/pressed、checked accent、separator、submenu chevron 和高对比度 tooltip；全部由既有 token、palette 与应用级 style 完成。

## 3. 生产代码设计

### 3.1 逻辑尺寸与 style hints

修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：

- `sizeFromContents(CT_MenuItem)` 必须先调用 base style，保留 icon、check column、shortcut、submenu、font 与 section 测量；普通 action 只保证最小 32 逻辑像素高度，separator 保持紧凑但不得小于 9px。
- `sizeFromContents(CT_MenuBarItem)` 先保留 base width，再只保证最小 32px 高度；`CT_MenuBar` 不读取当前 widget 固定高度。
- `PM_MenuPanelWidth`、`PM_MenuHMargin/VMargin`、`PM_MenuBarHMargin/VMargin`、`PM_MenuBarItemSpacing` 与 `PM_ToolTipLabelFrameWidth` 使用稳定逻辑像素，不能按屏幕 DPR 或控件宽度计算。
- 保留 `SH_Menu_SubMenuPopupDelay = 200`；tooltip wake-up、fall-asleep、display time、opacity 与 menu scroll/keyboard hints 继续委托 base style。
- 不设置 fixed size，不扫描 `QMenu::actions()`，不访问 action 动态属性。

### 3.2 菜单 popup surface

修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp` 与 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.*`：

- `PE_PanelMenu` 使用 `SurfaceSecondary`、`ControlStroke`、`StrokeThin` 与 `CornerRadiusMedium` 绘制唯一 popup surface；`PE_FrameMenu` 不再叠加第二条平台 frame。
- `CE_MenuEmptyArea` 使用相同 surface 补齐未被 action 覆盖的区域。
- 不更改 `QMenu` window flags、attributes、mask、contents margins、位置或 native shadow；这些继续由 Qt 与平台插件负责。
- paint 中不得 grab/render popup、创建 pixmap、style、animation、timer、event filter 或持久状态。

### 3.3 菜单项绘制

- `CE_MenuItem` 首先区分 normal、separator、section/submenu 与组合框 popup 特殊路径。
- enabled selected item 使用 `ControlFillHover` 圆角 surface，sunken 使用 `ControlFillPressed`；disabled item不绘制交互 surface。
- separator 使用方向无关的水平细线；带标题 section 必须保留 base style 的文字和字体语义，不把标题当普通可触发 action。
- checked item 使用 `Accent` 绘制清晰标记，但 checked 状态、exclusive/non-exclusive 行为仍由 action 提供；未选项保持 check/icon column 对齐。
- submenu chevron 必须按 LTR/RTL 镜像，颜色来自 enabled/disabled palette group，不使用字体图标或 SVG。
- icon、text、mnemonic、shortcut、default item 字体、elide 与 widget action 内容继续委托 base style；绘制前只清除已由 Fluent 绘制的 selected/sunken/check/arrow 状态，不能手工解析 action text。
- 所有临时 option 和 painter path 都是栈对象，不缓存 action/widget 裸指针。

### 3.4 Menu bar

- `PE_PanelMenuBar/CE_MenuBarEmptyArea` 使用应用窗口 surface，不创建独立卡片背景。
- `CE_MenuBarItem` 为 selected/hover 与 sunken/open 状态绘制小圆角 surface，再委托 base style 绘制 icon、文本和助记键。
- disabled、default palette group、LTR/RTL 与 Alt 下划线继续服从 Qt option/style hint。
- 不查找 extension button、不替换 overflow menu、不设置 native menu bar；调用方仍可使用 `QMenuBar::setNativeMenuBar()`。

### 3.5 ToolTip

- `PE_PanelTipLabel` 继续只绘制 `SurfaceSecondary` 与 `ControlStroke` 的圆角面板，padding 通过 `PM_ToolTipLabelFrameWidth` 提供。
- `standardPalette()` 的 `ToolTipBase/ToolTipText` 保证 Light、Dark、HighContrast 对比度；disabled tooltip 不建立独立颜色分支。
- tooltip text、rich text、wrap、duration、position、screen selection、show/hide 和辅助功能继续由 `QToolTip` 及 Qt 内部公开行为完成。
- 不新增 tooltip widget API；富交互 popup 不伪装成 tooltip。

## 4. 自动测试

新增 `ZzFluentUI/tests/ZzPopupSurfacesTest.cpp` 与 CTest `fluent.popup-surfaces`：

- `CT_MenuItem/CT_MenuBarItem` 保留 base 测量；普通、icon、checkable、shortcut、submenu、default、长文本、不同字体与 RTL 不裁切，separator 保持紧凑。
- popup panel、menu empty area、normal/hover/pressed/disabled item、separator、checked accent、submenu chevron 与 menu bar 状态在 Light、Dark、HighContrast 下使用已知 token/palette 颜色。
- `QAction` text/icon/shortcut/data/menuRole/checkable/checked/enabled/visible、defaultAction、activeAction 和 triggered/toggled 信号保持原生行为。
- 覆盖 exclusive/non-exclusive `QActionGroup`、section/separator、submenu 的 Right/Left、Up/Down、Enter/Space、Escape、mnemonic 与 keyboard search；不调用自定义事件处理器。
- `QMenuBar` 覆盖 action navigation、Alt/mnemonic、disabled action、submenu 激活和 `setNativeMenuBar(false)` 的 widget 路径。
- `QToolTip::showText/hideText` 覆盖 plain/rich text、显示时长参数、宿主 rect 和公开 top-level `Qt::ToolTip` 窗口；测试只按 window type、`QLabel` 与 `QAccessible` 公共接口观察，不匹配 Qt 私有类名。
- `QAccessible` role/name/state 覆盖 menu、menu bar、action child 与可观测 tooltip；不能以自定义 accessible interface 替换 Qt 原生实现。
- 预构造总计 100 个 menu/menu bar fixture，执行 1000 轮 active、enabled、checked、default、direction 与 action text 切换，恢复初始状态并处理 deferred delete 后，QObject、animation 与 timer 数量不得增长。

扩展既有 `ZzFluentStyleTest`/`ZzFluentStandardControlsTest`：

- 验证 popup/menu bar/tooltip metric 与主题更新只产生预期 repaint/geometry 更新。
- 验证组合框 `CE_MenuItem` 继续走组合框 selected accent，普通菜单走 popup surface 路径。
- 验证 context menu、push-button menu、menu bar menu 与 text editor 标准 context menu 使用相同 style 且不改变 action 语义。

## 5. 画廊、安装消费与公开边界

- 在 `examples/ZzFluentControlsGallery` 展示 menu bar，以及包含 icon、checkable、exclusive、shortcut、disabled、separator、section、submenu 和 RTL 的标准菜单；tooltip 由现有 icon button 和补充的长文本提示展示。
- `tests/InstallConsumer/Gui/main.cpp` 从安装包创建 `QMenu/QMenuBar`，验证 action、submenu、shortcut、checked、style 和最小尺寸；通过标准 `QWidget::setToolTip()` 验证公开消费路径。
- 本批不新增公开头；安装消费用于证明已安装 `Zz::FluentUI` 对标准 Qt popup surfaces 提供相同行为。
- 架构审计继续禁止 UI 依赖 repository、database、network、domain、QWindowKit、Qt Private 或第三方实现头。

## 6. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 预构造总计 100 个 `QMenu/QMenuBar` fixture，覆盖 icon、checkable、shortcut、submenu、disabled、LTR/RTL，并用 style option 覆盖 tooltip panel；10 帧预热、120 帧正式渲染，记录 P50/P95/max。
- 当前活动 Linux 参考发布环境设置绝对 P95 `<= 16.7 ms`；普通环境只记录数值。
- 执行 1000 轮 active action、checked、enabled、default action、direction 与 text 切换，恢复初始状态并处理 deferred delete 后，QObject、animation、timer 数量必须相同。
- paint/size 热路径不得扫描 actions、读取动态属性、创建 QObject/style/pixmap/animation/timer、调用 `processEvents()`、读文件或访问业务状态。

## 7. 视觉基线

扩展 `ZzFluentScreenshotTest`，增加独立固定尺寸 `popup-surfaces` surface：

- 覆盖 menu bar normal/hover/open/disabled，菜单 normal/hover/pressed/disabled、icon、check、radio、shortcut、section、separator、submenu、RTL 和 tooltip plain/rich text。
- 独立 popup 只通过 `QMenu::render()` 和公开 action geometry 合成到固定画布，不修改 popup window flags 或查找私有 container。
- tooltip 可使用 `PE_PanelTipLabel` 与标准 QLabel 文本夹具做确定性合成；实际 `QToolTip::showText()` 生命周期由功能测试覆盖。
- 建立 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张基线。
- menu/menu bar/tooltip 文字、shortcut 与 mnemonic 区域纳入显式文字遮罩；surface、stroke、hover、checked accent、separator、icon 和 chevron 继续参加严格比较。
- 更新后人工检查 DPR 1.0 三主题与 DPR 2.0 Light，确认无空白、裁切、重叠、双 frame、错误方向或文字不可读。

## 8. 跨平台静态检查

- Windows MSVC、Windows Qt SDK MinGW 与 macOS 只使用 Qt Widgets 公共 API 和标准 C++20；本批不增加平台分支。
- 运行 preset matrix、gate script contract、public headers、完整架构与 Fluent 边界审计。
- 本机不能把源码审计记录成 Windows/macOS 编译、安装消费或真机验证通过。

## 9. 提交顺序

```text
文档：规划Fluent标准弹出表面批次

记录旧版菜单与提示组件的所有权、动画、内部对象和输入语义风险。
确定标准QMenu、QMenuBar、QToolTip加应用级Style的无包装架构。
```

```text
控件：完善Fluent标准弹出表面

实现菜单、菜单栏和工具提示的尺寸、表面与状态绘制。
保留Qt原生action、助记键、快捷键、popup计时与定位语义。
```

```text
测试：接入弹出表面质量与安装消费

补齐键鼠、action、子菜单、无障碍、对象稳定性、性能、画廊和安装消费者。
覆盖公开头、架构边界和跨平台源码契约。
```

```text
测试：补齐弹出表面多主题视觉基线

新增三主题、四档DPR的菜单、菜单栏和工具提示参考图。
验证hover、pressed、disabled、checked、separator、submenu和RTL状态。
```

```text
文档：记录弹出表面批次交付结果

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

- 标准 `QMenu/QMenuBar/QToolTip` 具有一致 Fluent surface，业务 UI 不需要替换为 Zz 包装类型。
- Qt 原生 action、助记键、快捷键、default/active action、check group、submenu、popup 定位/计时和无障碍没有第二状态源。
- 生产代码每实例没有额外 QObject、style、pixmap、animation、timer、事件过滤器、stylesheet 或动态属性。
- Light、Dark、HighContrast x 四 DPR 视觉基线通过。
- 100 个 popup fixture 满足参考机帧预算，1000 轮状态切换恢复后无对象增长。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 待验证状态如实记录。

## 12. 交付结果

待实现完成后填写。
