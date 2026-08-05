# ZzFluentUI 数值输入控件 Implementation Plan

**Goal:** 在 `Zz::FluentUI` 中交付保留 Qt 原生范围、文本验证、locale、输入法、键盘、滚轮和无障碍语义，并具有稳定 Fluent 几何与绘制的 `ZzSpinBox`、`ZzDoubleSpinBox`。

**Architecture:** 两个公开类分别直接继承 `QSpinBox` 与 `QDoubleSpinBox`，不复制 value/range、line edit、validator 或 step 状态，也不持有 PIMPL；`ZzFluentStyle` 统一绘制标准 Qt 与 Zz 数值输入框，并使用同一组公开 `QStyleOptionSpinBox` 几何完成绘制、布局和命中测试。

**Tech Stack:** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

---

## 1. 范围与旧版审计

- 本批次继续总体设计阶段 10，只实现整数/浮点数值输入和 `CC_SpinBox` Fluent style；普通文本、搜索建议和组合选择分别留给后续独立批次。
- 旧版 `ZzSpinBox/ZzDoubleSpinBox` 为每个实例创建独立 proxy style，并在析构时手动删除 `style()`，混合 QObject 和 style 所有权。
- 旧版在每次 focus in/out 时创建 `DeleteWhenStopped` 动画，频繁切换焦点时对象数量和回调成本不受控。
- 旧版使用 stylesheet 调整内部 line edit，固定控件为 `115 x 35`，破坏字体、DPI、locale 长文本和布局策略。
- 旧版自行替换 context menu，并重复实现 step up/down 行为；新实现保留 Qt 标准 context menu、键盘、滚轮和可访问 action。
- 旧版公开 `ButtonMode` 与自定义 style 强耦合，且没有稳定的跨平台几何契约；新实现直接使用 Qt 公共 `QAbstractSpinBox::ButtonSymbols`。
- 普通 `QSpinBox/QDoubleSpinBox` 也必须通过应用级 `ZzFluentStyle` 获得相同绘制，不要求业务 UI 为换肤而替换类型。

## 2. 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzSpinBox.h`：

```cpp
#pragma once

#include <QtWidgets/QSpinBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/** @brief 保留 QSpinBox 完整数值输入语义的 Fluent 整数输入框。 */
class ZZ_FLUENT_UI_EXPORT ZzSpinBox final : public QSpinBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSpinBox)

public:
    /** @brief 创建默认使用加减按钮符号的整数输入框。 */
    explicit ZzSpinBox(QWidget *parent = nullptr);

    /** @brief 使用 Qt 父子所有权销毁内部编辑器和动作。 */
    ~ZzSpinBox() override;
};

} // namespace ZzFluentUI
```

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzDoubleSpinBox.h`：

```cpp
#pragma once

#include <QtWidgets/QDoubleSpinBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/** @brief 保留 QDoubleSpinBox 完整数值输入语义的 Fluent 浮点输入框。 */
class ZZ_FLUENT_UI_EXPORT ZzDoubleSpinBox final : public QDoubleSpinBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzDoubleSpinBox)

public:
    /** @brief 创建默认使用加减按钮符号的浮点输入框。 */
    explicit ZzDoubleSpinBox(QWidget *parent = nullptr);

    /** @brief 使用 Qt 父子所有权销毁内部编辑器和动作。 */
    ~ZzDoubleSpinBox() override;
};

} // namespace ZzFluentUI
```

两个类只有构造默认值，没有独立状态，因此采用 `.h/.cpp` 两文件。为空 PIMPL 增加堆分配、间接访问和四个文件没有收益。

## 3. 控件实现约束

新增：

```text
ZzFluentUI/widgets/src/ZzSpinBox.cpp
ZzFluentUI/widgets/src/ZzDoubleSpinBox.cpp
```

- 构造函数只调用基类构造，并设置 `QAbstractSpinBox::PlusMinus` 作为 Fluent 默认符号。
- 不设置固定尺寸、stylesheet、自有 style、内部 line edit、validator、locale、alignment、range、value、step、wrapping 或 acceleration。
- 不覆盖 `paintEvent()`、focus、keyboard、wheel、mouse、input method、context menu 或 accessibility event。
- 不创建 PIMPL、QObject、animation、timer、menu 或事件过滤器。
- 用户仍可调用 `setButtonSymbols(UpDownArrows/PlusMinus/NoButtons)`；style 必须正确绘制三种模式。

## 4. Fluent style 几何与绘制

修改：

```text
ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h
ZzFluentUI/widgets/src/ZzFluentStyle.cpp
ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h
ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp
```

实现规则：

- `sizeFromContents(CT_SpinBox)` 保留 base style 的字体/文本测量结果，并只保证不小于 `96 x 32` 逻辑像素；不得固定最终尺寸。
- `subControlRect(CC_SpinBox)` 为 frame、edit field、up、down 提供稳定矩形；按钮列宽 28 px，奇数高度由上按钮多占 1 px，所有矩形必须完全位于 `option->rect`。
- LTR 按钮列位于右侧，RTL 使用 `QStyle::visualRect()` 镜像到左侧；`NoButtons` 时 up/down 为空，edit field 使用完整可用宽度。
- `hitTestComplexControl(CC_SpinBox)` 只使用上述矩形返回 up/down/edit field/frame，不委托可能采用不同平台几何的 base style。
- `drawComplexControl(CC_SpinBox)` 先复用输入面板绘制，再按 `subControls` 绘制按钮背景和符号；不得绘制或访问内部 line edit 文本。
- `UpDownArrows` 使用简单折线 chevron，`PlusMinus` 使用水平/垂直线，`NoButtons` 不绘制符号。
- hover、pressed、disabled、focus 与 step-at-bound 状态可辨认；不可执行的 step 使用 disabled text，按下只影响对应 subcontrol。
- HighContrast 使用 palette 的 `Base/Text/Highlight/HighlightedText`，不得硬编码只适合 Light/Dark 的颜色。
- 绘制热路径只执行常数次矩形和 `QPainter` 操作；禁止 QObject、容器、图像或资源分配，禁止文件读取、锁和动态属性。

## 5. 原生语义

- range/value/singleStep、prefix/suffix、specialValueText、displayIntegerBase、decimals、wrapping 和 correction mode 全部由 Qt 持有。
- 文本验证、fixup、locale 小数点与分组符、输入法、selection、undo/redo 和 clipboard 全部使用内部标准 line edit。
- Up/Down、PageUp/PageDown、Home/End、鼠标滚轮、按钮按住自动重复和 context menu 不得重新实现。
- `valueChanged`、`textChanged`、`editingFinished` 的顺序和次数不得包装或重复发送。
- `QAccessible` role、current/minimum/maximum value 与增减 action 继续由 Qt 标准接口提供。

## 6. 自动测试

新增 `ZzFluentUI/tests/ZzSpinBoxControlsTest.cpp` 与 CTest `fluent.spin-box-controls`：

- 两个构造函数默认使用 `PlusMinus`，不创建额外 QObject、animation 或 timer。
- 整数 range/value/step、prefix/suffix、special value、十六进制显示和 wrapping 保持 Qt 契约。
- 浮点 decimals、singleStep、locale 小数点、rounding 与文本提交保持 Qt 契约。
- Up/Down、PageUp/PageDown、Home/End、输入文本提交、wheel 与 `stepUp()/stepDown()` 信号次数正确。
- `UpDownArrows/PlusMinus/NoButtons`、LTR/RTL、奇偶高度和极小矩形的 subcontrol rect 在范围内且不重叠。
- hit test 与绘制矩形一致；minimum/maximum 边界的不可执行按钮有 disabled 绘制状态。
- 标准 `QSpinBox/QDoubleSpinBox` 与 Zz 类型使用 `ZzFluentStyle` 均产生非空 Fluent 图像。
- Light/Dark/HighContrast 下 enabled、hover、pressed、focus、disabled 与 read-only 状态可辨认。
- `QAccessible` role 为 `SpinBox`，current/minimum/maximum value 与控件一致。
- 1000 轮 range/value/buttonSymbols/RTL 切换后后代 QObject、animation 和 timer 数量不增长。

测试不得转换 Qt private 类型或依赖内部 line edit 的实现类；可通过公开 `findChild<QLineEdit *>()` 验证输入语义。

## 7. 安装、画廊与性能

安装消费：

- `ZzFluentUI/CMakeLists.txt` 加入两个公开头和两个实现文件。
- `tests/InstallConsumer/CMakeLists.txt` 为两个公开头增加独立 installed-header 翻译单元。
- `tests/InstallConsumer/Gui/main.cpp` 只链接 `Zz::FluentUI`，验证整数/浮点构造、range、value、decimals 与默认符号。
- shared/static fresh A/B/consumer、public headers 和 package relocation 必须通过。

画廊：

- Input form 增加整数、浮点、RTL、NoButtons 与 disabled 固定展示状态。
- 不增加架构说明、性能文字或使用教程；画廊只展示可直接操作的控件。

性能：

- `ZzBasicControlsBenchmark` 增加 100 个可见数值输入框、10 帧预热与 120 帧 Release render。
- 记录 P50/P95/max；本机发布参考环境要求 P95 <= 16.7 ms。
- 100 个控件执行 1000 轮 range/value/buttonSymbols/RTL 切换，验证 QObject、animation 和 timer 数量不增长。
- benchmark 不得依赖生产逻辑调用 `processEvents()`。

## 8. 视觉基线

- `ZzFluentScreenshotTest` 增加独立固定尺寸 `spin-box-controls` surface，不改写已有基线。
- 覆盖整数/浮点、plus-minus、arrow、no-buttons、LTR/RTL、prefix/suffix、special value、focus、hover、pressed、disabled 和 read-only。
- 新增 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张基线。
- 自动比较通过后人工检查 DPR 1.0 三主题与 DPR 2.0 Light，确认按钮列、文本、焦点描边无裁切或重叠。

## 9. 架构与跨平台门禁

- 生产代码禁止 Qt Private include、QWindowKit、ZzWindowKit、ZzPureTools、业务模型、stylesheet、事件总线、动态属性裸指针、`QTimer` 和每实例 style。
- 禁止链式 namespace；公开类、方法和复杂几何 helper 使用简体中文 Doxygen。
- Windows MSVC、Windows Qt SDK MinGW 和 macOS 只依赖 Qt Widgets 公共 API、`QStyleOptionSpinBox` 与标准 C++20；本批不增加平台条件分支。
- Windows/macOS 在本机只能完成源码静态审计，不能记录为目标平台编译或真机验证通过。
- 远端 CI、GitHub CLI、Qt 下载和 push 按用户要求继续暂缓。

## 10. 提交边界

### 提交 A：计划

```text
文档：规划Fluent数值输入控件批次

审计旧版每实例样式、动态动画、固定尺寸与重复输入行为。
定义原生Qt状态单一来源、统一几何绘制和完整质量门禁。
```

### 提交 B：生产契约

```text
控件：实现Fluent数值输入框

新增无额外状态的ZzSpinBox和ZzDoubleSpinBox。
为标准与自定义数值输入框实现统一绘制、几何和命中测试。
```

### 提交 C：质量与消费

```text
测试：接入数值输入质量与安装消费

覆盖编辑、键盘、locale、无障碍、对象稳定性和shared/static消费。
在画廊和性能测试中接入百控件数值输入场景。
```

### 提交 D：视觉基线

```text
测试：补齐数值输入多主题视觉基线

增加独立数值输入截图面和十二张Linux参考图。
覆盖符号、方向、前后缀、交互、只读与禁用状态。
```

### 提交 E：交付记录

```text
文档：记录数值输入批次交付结果

记录Linux GCC、Clang、sanitizer、clang-tidy、截图、性能和安装消费结果。
明确Windows与macOS仅完成源码静态审计，远端CI继续暂缓。
```

## 11. 本机验证命令

使用现有环境，不下载 Qt：

```bash
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
```

每个代码提交至少运行对应 target 与 CTest；最终运行 GCC Release、Clang ASan/UBSan、`ZzClangTidy`、public headers、shared/static 安装消费、四档截图、benchmark 与画廊 smoke。

## 12. 完成定义

- `ZzSpinBox` 与 `ZzDoubleSpinBox` 的公开 API、shared/static 安装包和独立消费者可用。
- 标准 Qt 与 Zz 数值输入框使用同一 Fluent geometry、hit test 和绘制算法。
- Qt 原生范围、验证、locale、输入、信号与无障碍状态没有第二状态源。
- 生产代码每实例没有额外 QObject、animation、timer、style 或事件过滤器。
- Light、Dark、HighContrast x 四 DPR 视觉基线通过。
- 100 控件渲染满足参考机预算，1000 轮切换无对象增长。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 待验证状态如实记录。

## 13. 交付结果

### 13.1 提交

- `e9fa66b`：规划 Fluent 数值输入控件批次，完成旧版实现、Qt 原生语义、性能风险与跨平台边界审计。
- `a0da76b`：实现 `ZzSpinBox`、`ZzDoubleSpinBox` 以及应用级 `ZzFluentStyle` 的尺寸、绘制、子控件几何和命中测试。
- `1c54186`：补齐原生输入语义、无障碍、对象稳定性、性能、安装消费、公开头和画廊接入。
- `e7c213a`：同步当前 Fluent 滚动条与固定日历夹具对应的既有全控件视觉基线。
- `fe18033`：新增数值输入独立截图面，以及三主题、四档 DPR 的 12 张视觉基线。

### 13.2 代码级交付

- `ZzSpinBox` 与 `ZzDoubleSpinBox` 分别直接继承 `QSpinBox` 与 `QDoubleSpinBox`；类本身不增加数据成员、PIMPL、QObject 后代、事件过滤器、style、animation 或 timer，每个实例只把默认按钮符号设为 `QAbstractSpinBox::PlusMinus`。
- value、range、single step、decimals、prefix/suffix、special value、validator、locale、输入法、键盘、滚轮、context menu、信号和无障碍继续由 Qt 原生控件维护，没有第二份状态或重复输入协议。
- `ZzFluentStyle` 统一处理 Zz 类型和标准 Qt spin box：`CT_SpinBox` 保留基础 style 的文本测量并保证最小尺寸，`CC_SpinBox` 使用同一组 `subControlRect()` 结果完成绘制与 `hitTestComplexControl()` 命中。
- `PlusMinus`、`UpDownArrows`、`NoButtons`，LTR/RTL、奇偶高度、禁用、只读、焦点、hover、pressed 和 step enabled 状态均由公开 `QStyleOptionSpinBox`/`QAbstractSpinBox` API 表达；绘制层不访问内部编辑器文字或业务数据。
- 未迁移旧版每实例 proxy style、手动删除 `style()`、动态动画、stylesheet、固定控件尺寸、自定义 context menu 和自定义 `ButtonMode`，避免额外所有权、状态同步和跨平台几何分歧。

### 13.3 本机验证

- 环境：活动 `local-release-xvfb` 本机发布参考环境，Ubuntu 26.04 LTS、Linux 7.0.0-28-generic x86_64、Intel Core i7-14700、Qt 6.11.1、GCC 15.2.0、Clang/clang-tidy 20.1.8；全部验证使用已有 `/home/zz/Qt/6.11.1/gcc_64`。
- GCC Release shared：重新配置与全量构建通过，88/88 项 CTest 通过，包含公开头、架构边界、fresh 安装消费、包重定位、四档截图和四个示例 smoke。
- Clang ASan/UBSan shared：重新配置与全量构建通过，88/88 项 CTest 通过；未发现内存错误、泄漏、悬空访问或未定义行为。
- GCC Release static：重新配置与全量构建通过，88/88 项 CTest 通过；静态安装消费、包重定位、公开头和示例 smoke 均通过。
- Clang-Tidy：shared 与 static 编译数据库各有 307 条记录，仓库过滤规则分别选择 127/127 个一方翻译单元；两套均在 `warnings-as-errors` 下通过，无项目诊断。
- 安装消费：GCC shared、Clang sanitizer shared 和 GCC static 三套隔离 fresh producer/A/B/consumer 流程均完成重新配置、编译、安装、链接和运行；消费者只链接 `Zz::FluentUI` 即可包含并构造两个公开类型。
- 截图：新增 `spin-box-controls` 的 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张基线，四档严格像素比较全部通过；DPR 1.0 三主题与 DPR 2.0 Light 已人工检查，无空白、裁切或重叠，LTR/RTL、按钮符号、焦点、按压、只读和禁用态均可辨认。测试还用真实 Qt 鼠标命中确认按压上按钮会执行原生增量。
- 既有基线：同步 12 张全控件参考图，使其与当前 Fluent 滚动条和固定日期 `2026-08-06` 的日历夹具一致；同步后四档严格比较通过，未通过提高容差掩盖差异。
- Release 性能：100 个数值输入框、120 个正式渲染帧的 P50 为 `0.621 ms`、P95 为 `0.659 ms`、max 为 `0.685 ms`，低于 `16.7 ms` 帧预算。
- 对象稳定性：100 个控件拥有 400 个 QObject 后代、0 个 animation 和 0 个 timer；执行 1000 轮 range、value、按钮符号和 LTR/RTL 切换后三项数量均不增长。

### 13.4 平台与执行边界

- CMake preset 矩阵契约通过，继续登记 Windows MSVC shared/static、Windows Qt SDK MinGW shared/static，以及 macOS arm64/x86_64 shared/static 组合；原生门禁脚本契约通过。
- 本批生产代码未引入平台条件分支、Windows/macOS/Linux 原生系统头、编译器扩展、绝对路径、Qt Private API、QWindowKit、ZzWindowKit/ZzPureTools 反向依赖或依赖平台 ABI 宽度的类型假设。
- Windows MSVC、Windows Qt SDK MinGW 与 macOS 当前只完成 Qt 公共 API、标准 C++20、CMake 源文件/安装清单、依赖方向和条件编译的源码静态审计；未在对应原生工具链完成编译、安装消费或真机交互验证，不得标记为目标平台验证通过。
- 当前发布参考环境仍是 `local-release-xvfb`；原 `ubuntu2204-github-ci` 参考档案继续保留为 `pending-user-validation`，本批结果没有冒充 Ubuntu 22.04 兼容验证。
- 本批次未调用 GitHub CLI、未运行或读取远端 CI、未下载 Qt、未 push；远端矩阵按用户要求继续暂缓。
