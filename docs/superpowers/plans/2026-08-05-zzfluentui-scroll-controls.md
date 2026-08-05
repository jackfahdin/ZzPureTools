# ZzFluentUI 滚动控件 Implementation Plan

**Goal:** 在 `Zz::FluentUI` 中交付保留 Qt 原生范围、输入和无障碍语义，具有 Fluent 视觉、稳定对象数量及跨平台公共 API 边界的 `ZzScrollBar` 与 `ZzScrollArea`。

**Architecture:** `ZzScrollBar` 继承 `QScrollBar`，只增加单一持久悬停呈现动画，不复制 range/value、滚轮、拖动或 action 状态；`ZzScrollArea` 继承 `QScrollArea`，直接安装水平和垂直 `ZzScrollBar`，不维护第二套同步滚动条。标准 `QScrollBar` 也由 `ZzFluentStyle` 获得无动画 Fluent 绘制，保证 `QTableView`、`QTreeView` 等现有 Qt 视图一致。

**Tech Stack:** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

---

## 1. 范围与旧版审计

- 本批次继续总体设计阶段 10，只实现 `ZzScrollBar`、`ZzScrollArea` 及 `ZzFluentStyle` 的滚动条绘制。
- `ZzScrollPage` 同时承担路由、标题、页面栈和页面切换，不是滚动原语；它必须等待应用页面容器独立计划，不能进入本批次。
- 平滑滚轮、触摸拖动、overshoot 和 kinetic scrolling 会改变平台输入语义，第一版不实现；Qt 触控板像素滚动、滚轮、键盘、拖动、上下文菜单和辅助技术继续走 `QScrollBar/QScrollArea`。
- 旧版 `ZzScrollBar` 同时保留 origin scrollbar 与浮动 scrollbar，双向连接 value/range 并固定覆盖几何，存在重复状态、重入、替换后悬空引用和 RTL/DPI 风险。
- 旧版 range 变化、展开和收起会创建 `DeleteWhenStopped` 动画，且每实例创建 `QTimer`；频繁变化时对象数量和调度成本不受控。
- 旧版覆盖 `wheelEvent()` 并自行按 `angleDelta()/120` 计算步数，会丢失高精度触控板 `pixelDelta()` 和平台自然滚动行为。
- 旧版手动 `delete style()`，将 QObject 所有权和代理样式所有权混合；新实现不创建每控件 style，也不手动释放 style。
- 旧版 `ZzScrollArea` 使用 stylesheet、强制关闭原生 scrollbar policy 并引入 `QScroller`；新实现不使用 stylesheet、Qt Private API 或隐式手势劫持。

## 2. 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzScrollBar.h`：

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QScrollBar>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEnterEvent;
class QEvent;
class QHideEvent;

namespace ZzFluentUI {

class ZzFluentStylePrivate;
class ZzScrollBarPrivate;

/** @brief 保留 QScrollBar 完整语义的 Fluent 滚动条。 */
class ZZ_FLUENT_UI_EXPORT ZzScrollBar final : public QScrollBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzScrollBar)

public:
    /** @brief 创建垂直 Fluent 滚动条。 */
    explicit ZzScrollBar(QWidget *parent = nullptr);

    /** @brief 创建指定方向的 Fluent 滚动条。 */
    explicit ZzScrollBar(
        Qt::Orientation orientation,
        QWidget *parent = nullptr);

    /** @brief 停止并销毁唯一持久呈现动画。 */
    ~ZzScrollBar() override;

protected:
    /** @brief 指针进入时展开滑块视觉，不改变范围和值。 */
    void enterEvent(QEnterEvent *event) override;

    /** @brief 指针离开时收拢滑块视觉。 */
    void leaveEvent(QEvent *event) override;

    /** @brief 隐藏前停止动画，避免后台唤醒。 */
    void hideEvent(QHideEvent *event) override;

    /** @brief 在启用、样式和 palette 变化时同步呈现终态。 */
    void changeEvent(QEvent *event) override;

private:
    friend class ZzFluentStylePrivate;
    std::unique_ptr<ZzScrollBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzScrollArea.h`：

```cpp
#pragma once

#include <QtWidgets/QScrollArea>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzScrollBar;

/** @brief 直接安装 Fluent 滚动条并保留 QScrollArea 内容语义。 */
class ZZ_FLUENT_UI_EXPORT ZzScrollArea final : public QScrollArea
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzScrollArea)

public:
    /** @brief 创建无边框、使用两条 ZzScrollBar 的滚动区域。 */
    explicit ZzScrollArea(QWidget *parent = nullptr);

    /** @brief 使用 Qt 父子所有权销毁 viewport、内容和滚动条。 */
    ~ZzScrollArea() override;

    /** @brief 返回当前水平 ZzScrollBar；被调用方替换后可为空。 */
    [[nodiscard]] ZzScrollBar *fluentHorizontalScrollBar() const noexcept;

    /** @brief 返回当前垂直 ZzScrollBar；被调用方替换后可为空。 */
    [[nodiscard]] ZzScrollBar *fluentVerticalScrollBar() const noexcept;
};

} // namespace ZzFluentUI
```

`ZzScrollBar` 有动画状态，采用四文件 PIMPL。`ZzScrollArea` 没有独立状态，只通过 Qt 已有 getter 查询当前 scrollbar，因此使用 `.h/.cpp` 两文件；为空 PIMPL 支付额外分配成本没有收益。

## 3. ZzScrollBar 生命周期

新增：

```text
ZzFluentUI/widgets/src/ZzScrollBar.cpp
ZzFluentUI/widgets/src/private/ZzScrollBarPrivate.h
ZzFluentUI/widgets/src/private/ZzScrollBarPrivate.cpp
```

`ZzScrollBarPrivate` 持有：

- 非拥有 `ZzScrollBar *const q_ptr`。
- 以公开控件为 QObject parent 的唯一 `QVariantAnimation *const animation`。
- 只用于绘制的 `qreal expansion`，范围固定为 0 到 1。

约束：

- 构造时只创建一次动画；禁止 `DeleteWhenStopped`、`QTimer`、轮询和 range/value 镜像。
- enter 从当前 progress 动画到 1，leave 动画到 0，默认 `OutCubic` 167 ms。
- 控件隐藏、禁用、style 禁止动效或 reduced motion 时停止动画并同步终态。
- 动画只调用 `update()`，不得写 minimum、maximum、value、sliderPosition 或业务状态。
- 析构前停止动画并断开捕获私有对象的回调。
- 不覆盖 mouse、wheel、key、context-menu 或 accessibility event；全部由 `QScrollBar` 处理。

## 4. ZzScrollArea 装配

新增 `ZzFluentUI/widgets/src/ZzScrollArea.cpp`：

- 构造时调用 `setFrameShape(QFrame::NoFrame)`。
- 直接调用 `setHorizontalScrollBar(new ZzScrollBar(Qt::Horizontal))` 与 `setVerticalScrollBar(new ZzScrollBar(Qt::Vertical))`，由 `QAbstractScrollArea` 接管所有权。
- 不强制 `widgetResizable`、scrollbar policy、背景透明、手势或 overshoot，保留 Qt 默认契约。
- typed getter 每次对当前 `horizontalScrollBar()/verticalScrollBar()` 执行 `qobject_cast`；用户替换为标准 `QScrollBar` 后返回空指针，不保存悬空缓存。

## 5. Fluent style 几何与绘制

修改：

```text
ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h
ZzFluentUI/widgets/src/ZzFluentStyle.cpp
ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h
ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp
```

实现规则：

- `PM_ScrollBarExtent` 固定 12 逻辑像素，作为稳定命中区域；`PM_ScrollBarSliderMin` 固定 24 逻辑像素。
- `subControlRect(CC_ScrollBar)` 不提供 add/sub line 按钮；groove 使用完整 rect，slider 长度按 `pageStep / (range + pageStep)` 计算并收敛到 24 与可用长度之间。
- slider 位置只调用 `QStyle::sliderPositionFromValue()`，使用 `QStyleOptionSlider::upsideDown` 支持水平/垂直、RTL 和 inverted appearance。
- add/sub page 区域由 slider 前后区域和 upsideDown 确定；不存在负宽高区域。
- 新增 `hitTestComplexControl()`，只命中 slider、add page、sub page，命中矩形与绘制/布局使用同一 helper。
- `drawComplexControl(CC_ScrollBar)` 绘制轨道与滑块；不绘制箭头。普通 `QScrollBar` 使用 0/1 静态展开状态，`ZzScrollBar` 使用私有 progress。
- 收拢滑块厚 3 px、展开厚 6 px，始终在 12 px 命中区域居中，不因悬停改变 layout geometry。
- enabled/disabled、focus、pressed 和 palette color group 必须可辨认；HighContrast 不降低关键滑块对比度。
- 绘制热路径只做常数次矩形计算和 QPainter 调用，不分配 QObject、容器、图像或资源，不读取文件，不加锁。

## 6. 核心自动测试

新增 `ZzFluentUI/tests/ZzScrollControlsTest.cpp` 与 CTest `fluent.scroll-controls`：

- 两种构造函数方向、Qt 默认 range/value 和 12 px extent 正确。
- `ZzScrollArea` 直接拥有水平/垂直 `ZzScrollBar`，frame 为 NoFrame；替换 scrollbar 后 typed getter 返回空，不访问已删除对象。
- 继承的 `setRange()`、`setValue()`、single/page step、`triggerAction()`、方向键、PageUp/PageDown、Home/End 与信号行为保持 Qt 契约。
- 标准 `QScrollBar` 和 `ZzScrollBar` 使用 `ZzFluentStyle` 均可渲染非空 Fluent 图像。
- 水平、垂直、RTL、upsideDown、零范围、超大 range 和 pageStep 为 0 时，slider/groove/page rect 均在控件范围内且不相互重叠。
- add/sub line rect 为空，hit test 与 page/slider rect 一致。
- hover 时只有一个 animation 运行；leave、hide、disable、reduced motion 后停止或同步终态。
- 1000 次 enter/leave、range/value 和 orientation 切换后 animation 数为 1、timer 数为 0、QObject 后代数不增长。
- `QAccessible` role 保持 `ScrollBar`，value/minimum/maximum 来自 Qt 标准值接口。
- 删除运行中滚动条并处理 deferred delete，ASan/UBSan 下无悬空回调。

测试允许使用公开 `findChildren<QAbstractAnimation *>()` 和 `findChildren<QTimer *>()` 检查对象预算，不包含或转换 private 类型。

## 7. 安装、画廊与性能

安装消费：

- `ZzFluentUI/CMakeLists.txt` 加入四个 ScrollBar 文件和两个 ScrollArea 文件。
- `tests/InstallConsumer/CMakeLists.txt` 为两个公开头增加独立 installed-header 翻译单元。
- `tests/InstallConsumer/Gui/main.cpp` 只链接 `Zz::FluentUI`，验证 area 中两个 typed scrollbar、范围和值。
- shared/static fresh A/B/consumer、public headers 和 package relocation 均必须通过。

画廊：

- `ZzFluentControlsGallery` 外层滚动容器替换为 `ZzScrollArea`。
- 增加水平、垂直、RTL、禁用和不同 pageStep 的固定展示状态；不显示架构说明或使用教程。
- 画廊关闭后不得保留运行中动画。

性能：

- `ZzBasicControlsBenchmark` 增加 100 个可见滚动条、10 帧预热与 120 帧 Release render。
- 记录 P50/P95/max；当前本机发布参考环境要求 P95 <= 16.7 ms。
- 100 个控件执行 1000 轮 range/value/orientation 和 enter/leave 切换，验证控件数、animation、timer 与 QObject 后代数不增长。
- benchmark 隐藏宿主后验证运行中 animation 为 0；不允许生产逻辑依赖 `processEvents()`。

## 8. 视觉基线

- `ZzFluentScreenshotTest` 增加独立固定尺寸 scroll-controls surface，不改写已有基线。
- 覆盖水平/垂直、普通/hover/pressed、RTL、disabled、短/长 slider、area 双轴以及标准 `QScrollBar` fallback。
- hover 截图必须把动画同步到确定终态，不能让采样时间成为像素输入。
- 新增 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张 `scroll-controls-*.png`。
- 自动比较通过后人工检查 DPR 1.0 三主题与 DPR 2.0 Light，确认命中区不改变布局、滑块居中、无箭头残留、无裁切和重叠。

## 9. 架构与跨平台门禁

- 生产代码禁止 Qt Private include、QWindowKit、ZzWindowKit、ZzPureTools、业务模型、动态属性裸指针、`QTimer`、轮询、手写 wheel delta 和 stylesheet。
- 禁止链式 namespace；公开类、方法和复杂几何 helper 使用简体中文 Doxygen。
- Windows MSVC、Windows Qt MinGW 和 macOS 只使用 Qt Widgets 公共 API 与标准 C++20；本批次不增加平台条件分支。
- 远端 CI、GitHub CLI、Qt 下载和 push 按用户要求继续暂缓。

## 10. 提交边界

### 提交 A：计划

```text
文档：规划Fluent滚动控件批次

审计旧版双滚动条、动态动画、定时器与输入语义问题。
定义原生Qt状态单一来源、Fluent几何绘制和质量门禁。
```

### 提交 B：生产契约

```text
控件：实现Fluent滚动条与滚动区域

新增保留QScrollBar语义的单动画ZzScrollBar和直接装配的ZzScrollArea。
为标准与自定义滚动条实现统一的Fluent绘制、几何和命中测试。
```

### 提交 C：质量与消费

```text
测试：接入滚动控件质量与安装消费

覆盖键盘、无障碍、对象稳定性和shared/static安装消费。
在画廊和性能测试中接入长页面与百控件滚动场景。
```

### 提交 D：视觉基线

```text
测试：补齐滚动控件多主题视觉基线

增加独立滚动控件截图面和十二张Linux参考图。
覆盖方向、RTL、交互、禁用、滑块长度和双轴区域。
```

### 提交 E：交付记录

```text
文档：记录滚动控件批次交付结果

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

- `ZzScrollBar` 与 `ZzScrollArea` 公开 API、shared/static 安装包和独立消费者可用。
- range、value、steps、action、keyboard、wheel、context menu 和无障碍没有重复状态源。
- 生产代码每个 `ZzScrollBar` 只有一个持久 animation、没有 timer；隐藏后没有运行中动画。
- 标准与 Zz 滚动条的 geometry、hit test 与 Fluent 绘制使用同一算法，覆盖方向和 RTL。
- Light、Dark、HighContrast x 四 DPR 视觉基线通过。
- 100 控件渲染满足参考机预算，1000 轮切换无 QObject、animation 或 timer 增长。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 待验证状态如实记录。

## 13. 交付结果

### 13.1 提交

- `0bcce73`：规划 Fluent 滚动控件批次，完成旧版双滚动条、动态动画、定时器和输入语义审计。
- `4e3125e`：实现 `ZzScrollBar`、`ZzScrollArea`、Fluent 滚动条几何、绘制与命中测试。
- `9882dc8`：补齐键盘、无障碍、对象稳定性、安装消费、画廊与性能门禁。
- `af28c1c`：修复 Qt 鼠标事件未携带 `subControls` 时的滑块拖动命中。
- `753e043`：区分按下态滑块颜色，并为双轴滚动区域绘制一致的角落背景。
- `5c256d8`：加入 Light、Dark、HighContrast 与四档 DPR 的 12 张视觉基线。
- `d58904b`：消除重复分支与测试非空控制流的 Clang-Tidy 诊断。

### 13.2 本机结果

- 环境：Ubuntu 26.04 LTS、Linux 7.0.0-28-generic x86_64、Qt 6.11.1、GCC 15.2.0、Clang/clang-tidy 20.1.8；全部验证使用已有 `/home/zz/Qt/6.11.1/gcc_64`，未下载 Qt。
- GCC Release shared：全量构建通过，87/87 项 CTest 通过，包含公开头、包重定位、fresh 安装消费、四档截图与四个示例 smoke。
- Clang ASan/UBSan shared：全量构建通过，87/87 项 CTest 通过；未发现内存错误、泄漏、悬空回调或未定义行为。
- GCC Release static：重新配置与全量构建通过，83/83 项 CTest 通过；`install.consumer`、包重定位、公开头自包含、架构边界、四档截图和发布契约均通过。
- Clang-Tidy：shared 配置的 137/137 个一方翻译单元和 static 配置的 124/124 个适用翻译单元在 `warnings-as-errors` 下通过。安装消费者不在主构建编译数据库内，其源码由 shared/static fresh 消费构建验证。
- 安装消费：shared 的 GCC Release 与 Clang sanitizer、static 的 GCC Release 均完成隔离安装、消费者重新配置、编译、链接和运行；消费者只链接 `Zz::FluentUI` 即可使用两个公开控件。
- 截图：`scroll-controls` 的 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张基线全部通过自动比较；DPR 1.0 三主题与 DPR 2.0 Light 已人工检查，无箭头残留、空白、裁切或重叠，滑块在稳定命中区内居中。
- Release 性能：100 个滚动条、10 帧预热、120 帧正式渲染的 P50 为 `1.392 ms`、P95 为 `1.411 ms`、max 为 `1.496 ms`，低于 `16.7 ms` 预算。
- 对象稳定性：100 个控件拥有 200 个 QObject 后代、100 个持久 animation 和 0 个 timer；每个控件执行 1000 轮状态切换后数量不增长，宿主隐藏后运行中 animation 为 0。
- 原生语义：range、value、step、action、keyboard、wheel、context menu 和无障碍继续由 `QScrollBar/QScrollArea` 提供；水平、垂直、RTL、inverted appearance、零范围与大范围几何均有自动测试。
- 静态审计：新增生产代码未发现链式 namespace、Qt Private API、QWindowKit、ZzWindowKit/ZzPureTools 反向依赖、平台库、平台条件分支、`QTimer`、轮询、stylesheet、`QScroller` 或滚轮/鼠标语义重写。
- 平台边界：Windows MSVC、Windows Qt SDK MinGW 与 macOS 当前只完成 Qt 公共 API、标准 C++20、CMake 安装清单、依赖方向和条件编译的源码静态审计；尚未在对应原生工具链完成编译、安装消费或真机交互验证，不得标记为目标平台验证通过。
- 执行边界：本批次未调用 GitHub CLI、未运行或读取远端 CI、未下载 Qt、未 push；远端矩阵按用户要求继续暂缓。
