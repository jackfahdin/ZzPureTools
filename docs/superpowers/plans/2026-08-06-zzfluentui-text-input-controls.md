# ZzFluentUI 标准文本输入控件实施计划

**目标：** 让 `QLineEdit`、`QTextEdit` 与 `QPlainTextEdit` 在应用级 `ZzFluentStyle` 下获得一致、稳定且高性能的 Fluent 输入面板，同时完整保留 Qt 原生输入法、文本编辑、选择、撤销、剪贴板、平台快捷键和无障碍语义。

**架构：** 本批不新增 `ZzLineEdit`、`ZzTextEdit` 或 `ZzPlainTextEdit` 空包装类。三种标准 Qt 控件直接作为公开 UI 类型，`ZzFluentStyle` 只负责 frame、surface、focus、hover、disabled 与逻辑尺寸，不访问 document、cursor、selection、validator、clipboard 或 input method 状态。搜索建议和组合选择继续使用后续独立批次。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

## 1. 范围与前置结论

- 本批次继续总体设计阶段 10，只处理单行、富文本和纯文本编辑器的通用 Fluent frame 与质量门禁。
- `QLineEdit` 负责 validator、input mask、echo mode、clear action、selection、undo/redo 和 `editingFinished`；style 不建立第二状态源。
- `QTextEdit` 负责 `QTextDocument`、rich text、cursor、selection、undo/redo 和滚动；style 不解析或复制文档内容。
- `QPlainTextEdit` 负责大文本块的 plain-text document layout、maximum block count、cursor、undo/redo 和滚动；style 不按 block 数量分配或遍历文档。
- `QTextBrowser` 继承 `QTextEdit`，应自然复用同一 frame，但不为它增加专有行为。
- 搜索 suggestion model、异步检索、popup、debounce、历史记录、业务验证消息和组合框不属于本批。

## 2. 旧版代码审计

旧版 `ZzLineEdit`、`ZzPlainTextEdit` 及其 private/style 文件只作为外观参考，以下实现明确不迁移：

- 每个实例创建独立 `QProxyStyle`，并在控件析构时手动 `delete style()`，存在额外 QObject、缓存碎片和所有权风险。
- 通过全局事件总线监听窗口鼠标消息并主动清除焦点，改变 Qt 原生 focus reason 与平台输入法行为。
- 每次 focus in/out 动态创建 `QPropertyAnimation`，使用 `DeleteWhenStopped`，在高频表单中产生分配和延迟删除压力。
- 在 `paintEvent()` 中检查并修改 palette，可能引发绘制期间状态变化和额外更新。
- 使用固定高度、stylesheet、强制 `setVisible(true)`、额外 letter spacing 和自定义 context menu，覆盖平台尺寸、快捷键、密码模式、剪贴板策略与辅助功能。
- 手工实现删除选区和菜单 enable 状态，重复 Qt 已维护的 editor/document 状态并产生语义分叉。

可保留的视觉意图只有圆角输入 surface、普通/hover/focus/disabled 层次、主题文字和 placeholder 对比度；全部由现有应用级 style 与 palette 完成。

## 3. 生产代码设计

### 3.1 内容尺寸

修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：

- `sizeFromContents(CT_LineEdit)` 先调用 base style 保留字体、action、文本边距和平台测量，再仅保证不小于 `96 x 32` 逻辑像素。
- 不为多行编辑器设置固定高度；其 size hint、minimum size、document 和 layout 继续由 Qt/调用方决定。
- 所有尺寸使用 Qt 设备无关坐标，不读取 screen DPR 或缓存物理像素尺寸。

### 3.2 标准控件识别

修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：

- 保留 `PE_PanelLineEdit` 与 `PE_FrameLineEdit` 分派。
- `PE_Frame` 的文本 frame 识别覆盖 `QLineEdit`、`QTextEdit` 和 `QPlainTextEdit`；`QTextBrowser` 通过继承关系自然覆盖。
- 只使用公开 `qobject_cast` 和 Qt Widgets 头，不依赖 Qt Private API、object name、dynamic property 或平台条件分支。
- 不把所有 `QAbstractScrollArea` 一概当作文本输入，避免覆盖 list/table/tree/calendar 等视图 frame。

### 3.3 面板绘制

修改 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`：

- disabled 使用 `ControlFillDisabled`；focus 使用 palette `Base` 与 `Highlight` stroke；hover 使用 `ControlFillHover`；normal 使用 palette `Base` 与 `ControlStroke`。
- 使用 snapshot 的 `StrokeThin` 和 `CornerRadiusMedium`，只绘制输入 panel，不绘制 text、placeholder、selection、cursor、document 或 clear action。
- 绘制路径不得创建 QObject、animation、timer、事件过滤器、stylesheet、容器或持久缓存。
- 主题颜色变化只由既有 style snapshot/palette 更新路径驱动，不在 paint 中写 palette 或 geometry。

## 4. 自动测试

新增 `ZzFluentUI/tests/ZzTextInputControlsTest.cpp` 与 CTest `fluent.text-input-controls`：

- `QLineEdit`、`QTextEdit`、`QPlainTextEdit` 和 `QTextBrowser` 使用同一 `ZzFluentStyle`，均产生非空 Fluent frame。
- `CT_LineEdit` 保留 base 测量并满足最小 `96 x 32`；长文本、leading/trailing action 和字体变化不得被固定尺寸裁切。
- focus、hover、disabled、read-only、LTR/RTL frame 使用期望 fill/stroke；三种主题均验证已知颜色。
- 单行输入覆盖键盘输入、validator、input mask、selection、undo/redo、password echo、clear button 和标准 context menu。
- 富文本覆盖 plain/rich text、cursor selection、undo/redo；纯文本覆盖 block、maximum block count、selection 和 undo/redo。
- 输入法相关测试只通过 Qt 公共 `inputMethodQuery()`/`QInputMethodEvent`，不依赖平台 private input context。
- `QAccessible` role、value/text 与 read-only/disabled/focusable 状态和控件一致。
- 100 个编辑器执行 1000 轮 text、placeholder、read-only、enabled、direction 和 focus 切换后，QObject、animation 与 timer 数量不增长。
- 标准 Qt context menu 继续由控件创建；生产类不得 override `contextMenuEvent()`。

扩展既有 `ZzFluentStyleTest`/`ZzFluentStandardControlsTest`：

- 验证 `QPlainTextEdit` 不再落回平台 frame。
- 验证 style 对文本输入只发送绘制更新；metric/font 变化才触发 geometry 更新。
- 验证标准 Qt 编辑器和 spin box 内部无 frame 的 line edit 不发生重复 frame 绘制。

## 5. 画廊、安装消费与公开边界

- 在 `examples/ZzFluentControlsGallery` 展示单行 placeholder/clear action、password/read-only、rich text、plain text、disabled 和 RTL 状态；示例只设置展示数据，不加入业务逻辑。
- `tests/InstallConsumer/Gui/main.cpp` 从安装包创建三种 Qt 编辑器并挂接 `ZzFluentStyle`，验证 palette、最小单行尺寸和文本语义。
- 本批不新增公开头；安装消费用于证明已安装 `Zz::FluentUI` 对标准 Qt 编辑器提供相同行为。
- 架构审计继续禁止 UI 依赖 repository、database、network、domain、QWindowKit、Qt Private 或第三方实现头。

## 6. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 预构造合计 100 个 `QLineEdit/QTextEdit/QPlainTextEdit`，10 帧预热、120 帧正式渲染，记录 P50/P95/max。
- 当前活动 Linux 参考发布环境设置绝对 P95 `<= 16.7 ms`；普通环境只记录数值。
- 执行 1000 轮 text、placeholder、read-only、enabled 和 LTR/RTL 切换，前后 QObject、animation、timer 数量必须相同。
- paint 热路径不得解析文本、创建 style、创建 animation/timer、调用 `processEvents()`、读文件或访问全局业务状态。

## 7. 视觉基线

扩展 `ZzFluentScreenshotTest`，增加独立固定尺寸 `text-input-controls` surface：

- 覆盖 `QLineEdit/QTextEdit/QPlainTextEdit`、placeholder、clear action、password、rich/plain text、focus、hover、read-only、disabled 和 RTL。
- 建立 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张基线。
- 文本、placeholder、selection 和 cursor 区域纳入显式文字遮罩；frame、surface、focus stroke、clear icon 和滚动条仍参与严格比较。
- 更新基线后人工检查 DPR 1.0 三主题与 DPR 2.0 Light，确认无空白、裁切、重叠、双 frame 或不可读状态。

## 8. 跨平台静态检查

- Windows MSVC、Windows Qt SDK MinGW 与 macOS 只使用 Qt Widgets 公共 API 和标准 C++20；本批不增加平台分支。
- 运行 preset matrix、gate script、public headers、完整架构与 Fluent 边界审计。
- 本机不能把源码审计记录成 Windows/macOS 编译、安装消费或真机验证通过。

## 9. 提交顺序

```text
文档：规划Fluent标准文本输入批次

记录旧版文本输入的所有权、动画、焦点和输入语义风险。
确定标准Qt编辑器加应用级Style的无包装架构。
```

```text
控件：完善Fluent标准文本输入样式

覆盖单行、富文本和纯文本编辑器的尺寸、面板和状态绘制。
保留Qt原生输入法、编辑、剪贴板和无障碍语义。
```

```text
测试：接入文本输入质量与安装消费

补齐原生语义、对象稳定性、性能、画廊和安装消费者。
覆盖公开头、架构边界和跨平台源码契约。
```

```text
测试：补齐文本输入多主题视觉基线

新增三主题、四档DPR的独立文本输入参考图。
验证focus、hover、只读、禁用、RTL和多行状态。
```

```text
文档：记录文本输入批次交付结果

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

- 三种标准 Qt 编辑器具有一致 Fluent frame，且不要求业务 UI 替换为 Zz 包装类型。
- Qt 原生输入法、validator、document、selection、undo/redo、clipboard、快捷键、context menu 和无障碍没有第二状态源。
- 生产代码每实例没有额外 QObject、animation、timer、style、事件过滤器或 stylesheet。
- Light、Dark、HighContrast x 四 DPR 视觉基线通过。
- 100 个编辑器满足参考机帧预算，1000 轮状态切换无对象增长。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 待验证状态如实记录。

## 12. 交付结果

本批次已于 2026-08-06 完成交付，结果如下。

### 12.1 生产实现

- `ZzFluentStyle` 已统一覆盖 `QLineEdit`、`QTextEdit`、`QPlainTextEdit` 与派生的 `QTextBrowser`，没有新增空包装控件或第二份编辑状态。
- `CT_LineEdit` 保留 Qt base style 测量，并只保证最小 `96 x 32` 逻辑尺寸；多行编辑器继续使用 Qt 原生 document、viewport 与布局尺寸。
- 输入 frame 已覆盖 normal、hover、focus、disabled、read-only、LTR 与 RTL；style 不读取文本内容，不接触输入法、validator、document、cursor、selection、clipboard 或 context menu。
- 生产路径没有新增每实例 style、事件过滤器、animation、timer、stylesheet、动态属性或平台条件分支。

### 12.2 功能与安装验证

- Linux GCC 15 shared Release：全量 CTest `89/89` 通过。
- Linux GCC 15 static Release：全量 CTest `89/89` 通过。
- Linux Clang 20 ASan+UBSan：全量 CTest `89/89` 通过，无 sanitizer 报告。
- shared、static 与 sanitizer 构建均通过 fresh producer、install、consumer 流程；安装消费者成功创建并验证 `QLineEdit`、`QTextEdit` 与 `QPlainTextEdit`。
- 公开头、生成代码、包重定位、二进制依赖、完整架构、Fluent 边界、画廊 smoke 与应用示例均包含在全量门禁中通过。

### 12.3 静态分析与视觉验证

- shared `linux-clang-tidy-release`：项目翻译单元 `128/128` 通过。
- static `linux-clang-tidy-static`：项目翻译单元 `128/128` 通过。
- 新增 Light、Dark、HighContrast x DPR 1.0、1.25、1.5、2.0 共 12 张独立文本输入基线；关闭更新模式后四档截图测试 `4/4` 通过。
- 已人工检查 DPR 1.0 三主题与 DPR 2.0 Light：画面非空，无裁切、重叠或双 frame，focus、hover、disabled、clear action、RTL 与多行状态清晰可辨。
- 应用级 `QLineEdit` 样式引起的既有全控件与数值输入基线变化已同步，并继续参加严格像素比较。

### 12.4 性能结果

本机参考发布环境为 Ubuntu 26.04、GCC 15.2.0、Clang 20.1.8、Qt 6.11.1。`linux-gcc-reference` 下 100 个文本编辑器、10 帧预热与 120 帧正式渲染结果为：

```text
P50: 2.154 ms
P95: 2.169 ms
max: 2.193 ms
descendants: 725
animations: 0
timers: 0
```

P95 低于 `16.7 ms` 参考门限；1000 轮 text、placeholder、read-only、enabled、direction 与 focus 状态切换后没有 QObject、animation 或 timer 增长。

### 12.5 跨平台状态

- preset matrix、Linux/Windows/macOS gate script contract、公开头和完整架构边界检查均通过。
- 本批源码未引入 `Q_OS_*`、`_WIN32`、`__APPLE__` 分支、Qt Private 头、`QWindowKit::` 目标泄漏或链式命名空间，使用范围限于 Qt Widgets 公共 API 与标准 C++20。
- Windows MSVC、Windows Qt SDK MinGW 与 macOS 本批只完成源码静态审计，尚未在对应平台编译、安装消费或真机验证；不得将当前结果表述为这些平台已经运行通过。
- 按当前项目决策，本批未访问 GitHub CLI、未运行远端 CI、未 push，也未下载新的 Qt SDK。

### 12.6 提交记录

```text
9e79031 文档：规划Fluent标准文本输入批次
dd94151 控件：完善Fluent标准文本输入样式
6c6824f 测试：接入文本输入质量与安装消费
7284af1 测试：补齐文本输入多主题视觉基线
```
