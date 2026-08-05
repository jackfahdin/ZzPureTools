# ZzFluentUI 环形进度控件 Implementation Plan

**Goal:** 在 `Zz::FluentUI` 中交付复用 Qt 原生进度语义、支持确定与不确定状态、遵循主题与减少动效设置且具有有界运行开销的 `ZzProgressRing`。

**Architecture:** `ZzProgressRing` 继承 `QProgressBar`，范围、值、格式文本、信号和无障碍值接口全部沿用 Qt 公共契约；Zz 代码只负责圆环呈现和单一持久动画。控件不访问业务模型，不创建顶层窗口，不依赖 ZzPureTools、ZzWindowKit 或 QWindowKit。

**Tech Stack:** Qt 6.8+ Core/Gui/Widgets/Test、C++20、四文件 PIMPL、CMake/CTest、GCC 15、Clang 20、ASan/UBSan、clang-tidy、容差截图与 Release 性能测量。

---

## 1. 批次边界与旧版审计

- 本批次继续总体设计阶段 10，只新增环形进度控件；自动完成、下载、网络请求、任务取消和业务状态不属于 UI 控件职责。
- 旧版 `ZzProgressRing` 自行复制 `minimum/maximum/value`，没有复用 `QProgressBar` 的范围收敛、标准信号和可访问值接口。
- 旧版百分比直接使用 `value / (maximum - minimum)`，忽略非零 minimum；`minimum == maximum` 时存在除零路径。
- 旧版为不确定状态永久创建并驱动两条无限 `QPropertyAnimation`，隐藏、禁用和系统关闭动效时没有统一停止契约。
- 旧版通过全局主题单例和对象级 stylesheet 取色，绕开 palette/style 生命周期，并把尺寸固定为 70 x 70。
- 旧版 `setMinimum()`、`setMaximum()`、`setRange()` 和 `setValue()` 的校验策略互不一致，调用者无法得到 Qt 标准行为。
- 新实现只参考旧版的“圆环可表达确定/不确定进度”这一产品意图，不复制其属性宏、全局状态、动画和绘制代码。

## 2. 公开契约

新增：`ZzFluentUI/widgets/include/ZzFluentUI/ZzProgressRing.h`

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QProgressBar>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QHideEvent;
class QPaintEvent;
class QShowEvent;

namespace ZzFluentUI {

class ZzProgressRingPrivate;

/**
 * @brief 使用 QProgressBar 范围和值语义绘制 Fluent 圆环进度。
 *
 * minimum 与 maximum 同为 0 时进入 Qt 标准不确定状态。控件必须在
 * GUI 线程创建和调用；动画只影响呈现，不改变值或业务状态。
 */
class ZZ_FLUENT_UI_EXPORT ZzProgressRing final : public QProgressBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzProgressRing)
    Q_PROPERTY(
        int ringWidth
        READ ringWidth
        WRITE setRingWidth
        NOTIFY ringWidthChanged)

public:
    /**
     * @brief 创建范围为 0 到 100、值为 0 的环形进度控件。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzProgressRing(QWidget *parent = nullptr);

    /** @brief 停止持久动画并销毁私有呈现状态。 */
    ~ZzProgressRing() override;

    /** @brief 返回设备无关逻辑像素表示的圆环线宽。 */
    [[nodiscard]] int ringWidth() const noexcept;

    /**
     * @brief 设置圆环线宽。
     * @param width 逻辑像素；收敛到 1 至 64。
     */
    void setRingWidth(int width);

    /** @brief 返回稳定的默认正方形建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

    /** @brief 返回能够呈现圆环轮廓的最小正方形尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

public Q_SLOTS:
    /** @brief 转发 Qt 范围设置并立即同步不确定动画状态。 */
    void setRange(int minimum, int maximum);

    /** @brief 转发 Qt 最小值设置并立即同步不确定动画状态。 */
    void setMinimum(int minimum);

    /** @brief 转发 Qt 最大值设置并立即同步不确定动画状态。 */
    void setMaximum(int maximum);

Q_SIGNALS:
    /** @brief 有效圆环线宽实际变化后发出。 */
    void ringWidthChanged(int width);

protected:
    /** @brief 使用 palette、范围和值绘制圆环和可选文本。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在样式、palette、启用状态或字体变化时同步呈现。 */
    void changeEvent(QEvent *event) override;

    /** @brief 可见后按当前范围和 style 动效偏好启动至多一条动画。 */
    void showEvent(QShowEvent *event) override;

    /** @brief 隐藏前停止动画，避免后台唤醒。 */
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzProgressRingPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

设计约束：

- 不新增 `busy`、`minimum`、`maximum`、`value`、`format` 或 `textVisible` 的重复属性；调用者直接使用 `QProgressBar` API。
- Qt 公开的 `QProgressBar` 没有 `rangeChanged` 信号，因此派生类用同签名 public slots 转发三个范围 setter，并在基类完成状态收敛后立即同步动画；状态仍只存于 `QProgressBar`。绘制入口再次同步，以覆盖通过基类指针或属性系统写入范围的路径。
- `setRange(0, 0)` 是唯一不确定状态；其他相等范围仍按 Qt 的确定值契约呈现，不执行除法。
- `ringWidth` 是唯一新增外观属性，范围固定为 1 至 64，重复设置不发信号。
- 不暴露动画相位、动画对象、主题控制器或 painter；截图和测试不得依赖 private API。
- 不新增可写“完成百分比”；UI 不推导任务结果，不启动或取消后台任务。

## 3. 四文件私有实现

新增：

```text
ZzFluentUI/widgets/src/ZzProgressRing.cpp
ZzFluentUI/widgets/src/private/ZzProgressRingPrivate.h
ZzFluentUI/widgets/src/private/ZzProgressRingPrivate.cpp
```

`ZzProgressRingPrivate` 持有：

- 非拥有 `ZzProgressRing *const q_ptr`。
- 构造时一次性创建、以公开控件为 QObject parent 的 `QVariantAnimation *animation`。
- `qreal phase`，范围保持在 0 到 1。
- `int ringWidth = 4`。

私有方法：

```cpp
/** @brief 根据可见性、启用状态、范围与 style 偏好同步动画。 */
void syncAnimation();

/** @brief 停止动画并把相位复位到确定的静态起点。 */
void stopAnimation() noexcept;

/** @brief 返回 minimum/maximum 是否表达 Qt 不确定进度。 */
[[nodiscard]] bool isIndeterminate() const noexcept;
```

生命周期规则：

- 只创建一条 `QVariantAnimation`，线性循环时长固定为 1200 ms，不在状态切换时重新分配。
- 只有 `isVisible() && isEnabled() && isIndeterminate()` 且 `SH_Widget_Animate != 0` 时运行动画。
- 构造时控件不可见，不启动动画；`showEvent()` 后同步。
- `hideEvent()`、禁用、切回确定范围或 style 关闭动效时立即停止并把 phase 复位为 0。
- private 析构先停止并断开动画回调，随后由 QWidget 的 QObject 子对象析构删除动画，不发生悬空回调。
- 不使用 `QTimer`、轮询、`processEvents()`、全局动画注册表或静态可变状态。

## 4. 绘制与几何

- `sizeHint()` 返回稳定的 48 x 48 与 base style 建议的较大值，不因 value 或格式文本变化而抖动。
- `minimumSizeHint()` 至少为 24 x 24，并考虑 `2 * ringWidth + 4`；线宽变化后调用 `updateGeometry()`。
- 在 `contentsRect()` 中取居中的最大正方形，按实际可用半径收敛绘制线宽；非正尺寸直接返回。
- 使用 `QStyleOptionProgressBar` 获取方向、palette、启用状态、文本和 `invertedAppearance`，不读取全局主题单例。
- 轨道使用当前颜色组的 `QPalette::Mid`，前景使用 `QPalette::Highlight`，文字使用 `QPalette::Text`；HighContrast 由应用 palette 决定。禁用状态在 Disabled 颜色基础上与对应 `Window` 色做固定比例混合，保证状态可辨认。
- pen 使用圆端点与抗锯齿；不构造 `QPainterPath`，不解析 SVG，不读取文件，不创建子 QWidget。
- 确定状态按 `(value - minimum) / (maximum - minimum)` 计算，先转换为 64 位整数并收敛到 0 至 1。
- `maximum <= minimum` 的非不确定状态使用离散终态，不执行除法。
- 默认从十二点方向顺时针绘制；`invertedAppearance` 反转方向。圆形几何不因 RTL 镜像，文本仍遵循 style option。
- 不确定状态绘制固定 96 度弧，相位只改变起始角；动效关闭或控件禁用时仍显示 phase 为 0 的静态弧。
- 仅在确定状态且 `isTextVisible()` 为 true 时绘制 `QProgressBar::text()`；可用内圆宽度不足时使用右侧省略，文字不覆盖圆环。

## 5. 核心自动测试

新增 `ZzFluentUI/tests/ZzProgressRingTest.cpp` 与 CTest `fluent.progress-ring`：

- 默认 range/value、48 x 48 建议尺寸和 4 px 线宽正确。
- `ringWidth` 对 0、1、64、65 的收敛正确，重复有效值不发信号。
- 继承的 `setRange()`、`setMinimum()`、`setMaximum()`、`setValue()` 保持 Qt clamp 与信号行为。
- 非零 minimum、0%、中间值、100%、相等非零范围和 `(0, 0)` 均可渲染，不崩溃、不除零、不产生空白图像。
- value 小于 maximum 时前景不是整圆，value 等于 maximum 时前景形成完整圆。
- `invertedAppearance` 改变弧方向但不改变完成比例。
- `textVisible`、`format` 与长文本不会越过内圆或圆环边界。
- 不确定状态可见且允许动效时只有一条 `QAbstractAnimation` 运行；隐藏、禁用、切回确定范围或 style 禁止动效后停止。
- 1000 次确定/不确定切换后 animation 对象仍为 1，timer 子对象为 0，QObject 后代数量不增长。
- `QAccessible` role 保持 `ProgressBar`，名称、当前值、最小值和最大值来自 Qt 标准接口。
- 删除运行中的不确定控件后处理 deferred delete，ASan/UBSan 下无悬空回调。

测试只使用 Qt 公共 API；允许通过 `findChildren<QAbstractAnimation *>()` 检查公开控件拥有的动画数量和运行状态，不包含或转换 private 类型。

## 6. 安装、架构与静态门禁

- `ZzFluentUI/CMakeLists.txt` 把两个 `.cpp` 加入 `zz_fluent_ui_sources`，把公开头加入 `zz_fluent_ui_moc_headers`。
- `tests/InstallConsumer/CMakeLists.txt` 增加 `ZzProgressRing.h` 的独立 installed-header 翻译单元。
- `tests/InstallConsumer/Gui/main.cpp` 只链接 `Zz::FluentUI`，构造确定与不确定环并验证公开属性。
- public header 自包含；shared/static 安装、重定位和外部消费均必须通过。
- 架构扫描禁止链式 namespace、Qt Private include、QWK、ZzWindowKit、ZzPureTools、业务模型、动态属性裸指针、`QTimer` 和 `processEvents()`。
- Windows MSVC、Windows Qt MinGW 与 macOS 只使用相同 Qt 公共 API；本批次不增加平台条件分支。

## 7. 画廊、视觉与性能

画廊：

- 在 `ZzFluentControlsGallery` 的进度区域加入 25%、72%、完成、不确定和禁用不确定状态。
- 示例只用固定展示值，不绑定任务、网络或业务模型，不显示使用说明或架构说明。
- 不确定示例随画廊隐藏停止，关闭画廊后不保留动画对象。

视觉：

- 在 `ZzFluentScreenshotTest` 增加独立固定尺寸 progress-ring surface，不改写已有基础、日历、卡片或标签页基线。
- 覆盖 0%、25%、72%、100%、自定义线宽、文本、禁用与静态不确定状态。
- 不确定截图使用禁用状态或 style 禁止动效，避免采样时间成为像素输入。
- 新增 Light、Dark、HighContrast x DPR 1.0/1.25/1.5/2.0 共 12 张 `progress-rings-*.png`。
- 自动比较通过后人工检查 DPR 1.0 三主题和 DPR 2.0 Light，确认圆环居中、线宽稳定、文字不遮挡且无裁切。

性能：

- 在 `ZzBasicControlsBenchmark` 增加 100 个可见环（80 个确定、20 个不确定）的 10 帧预热与 120 帧 Release render。
- 记录 P50/P95/max；当前本机发布参考环境要求 P95 <= 16.7 ms。
- 对 100 个控件执行 1000 轮 range/value 切换，验证控件总数、animation 数、timer 数和 QObject 后代数不增长。
- benchmark 退出前隐藏宿主并验证所有不确定动画停止；不允许为测量调用 `processEvents()` 驱动生产逻辑。

## 8. 提交边界

### 提交 A：计划

```text
文档：规划Fluent环形进度控件批次

审计旧版范围、除零、全局主题与双动画问题。
定义复用QProgressBar语义的公开API、单动画生命周期和圆环绘制规则。
列明测试、安装消费、视觉、性能与跨平台静态门禁。
```

### 提交 B：生产契约

```text
控件：实现Fluent环形进度控件

新增基于QProgressBar公共契约的ZzProgressRing四文件PIMPL。
实现palette驱动的确定与不确定圆环，以及可见性和动效偏好约束的单一持久动画。
覆盖范围、绘制、动画生命周期、对象稳定性和无障碍行为。
```

### 提交 C：质量与消费

```text
测试：接入环形进度质量与安装消费

把公开头加入shared/static安装消费和重定位门禁。
在控件画廊加入无业务绑定的确定与不确定进度示例。
增加百控件渲染、千轮状态切换和隐藏停机性能检查。
```

### 提交 D：视觉基线

```text
测试：补齐环形进度多主题视觉基线

增加独立环形进度截图面和十二张Linux参考图。
覆盖三主题、四DPR、确定值、静态不确定、禁用、文本和线宽状态。
```

### 提交 E：交付记录

```text
文档：记录环形进度批次交付结果

记录Linux GCC、Clang、sanitizer、clang-tidy、截图、性能和安装消费结果。
明确Windows与macOS仅完成源码静态审计，远端CI继续暂缓。
```

## 9. 本机验证命令

使用现有环境，不下载 Qt：

```bash
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
```

每个代码提交至少运行对应 target 与 CTest；最终运行 GCC Release、Clang ASan/UBSan、`ZzClangTidy`、public headers、shared/static 安装消费、截图和 benchmark。远端 CI、GitHub CLI 与 push 按用户要求暂缓。

## 10. 完成定义

- `ZzProgressRing` 公开 API、shared/static 安装包和独立消费者可用。
- 范围、值、格式、信号与无障碍沿用 `QProgressBar`，没有重复状态源。
- 确定、不确定、禁用、隐藏、减少动效、非零 minimum 和相等范围路径均有自动测试。
- 生产代码只有一条持久动画，不使用 QTimer、轮询、事件泵、Qt Private API 或全局主题状态。
- Light、Dark、HighContrast x 四 DPR 视觉基线通过。
- 100 控件渲染满足参考机预算，1000 轮切换无 QObject、animation 或 timer 增长，隐藏后无运行动画。
- UI 不访问业务模型、任务执行器、网络、存储或窗口后端。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 待验证状态如实记录。

## 11. 交付结果

待实现完成后填写。
