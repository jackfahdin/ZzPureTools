# ZzFluentUI 日历控件 Implementation Plan

**Goal:** 以 Qt 6.8+ 的成熟日期交互为内核，在 `Zz::FluentUI` 中交付高性能、可键盘操作、可访问、支持高 DPI 与多主题的 `ZzCalendar` 和 `ZzCalendarPicker`。

**Architecture:** `ZzCalendar` 直接继承 `QCalendarWidget`，保留 Qt 的日期范围、当前页、区域设置、键盘和无障碍模型，只覆写日期单元格呈现。`ZzCalendarPicker` 直接继承 `QDateEdit`，通过 `setCalendarPopup(true)` 和 `setCalendarWidget()` 装配唯一的 `ZzCalendar`。两个公开类均采用四文件 PIMPL；私有对象只保存展示状态和非拥有 Qt 子对象指针，不复制日期模型，不访问业务数据。

**Tech Stack:** Qt 6.8+ Core/Gui/Widgets/Test、C++20、CMake 3.23、CTest、Qt Test、`Zz::FluentFoundation`、`Zz::FluentUI`。

---

## 1. 前置条件与执行边界

- 工作目录固定为 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro`。
- 阶段 0-9 的工程、Foundation、基础控件、应用框架和本机 Linux 门禁已经完成。
- 本批次属于总体设计的阶段 10，只实现日历与日期选择器；复杂卡片和可撕标签页必须使用后续独立计划。
- 旧仓库 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzFluentUI` 只用于核对用户可见行为和命名，不复制其自建 model/delegate、全局主题单例、宏属性系统、固定尺寸或动画截图方案。
- 不引入 Qt Private API，不依赖内部对象名，不查找 `qt_calendar_*` 子控件，不访问 `QCalendarModel` 等实现类型。
- 不新增日历业务模型、节假日服务、农历、网络、数据库或领域对象。业务层通过 Qt 已有 `QDate`、范围和信号 API 与控件交互。
- 日历选择模式保持 Qt 的单日期语义。本批次不实现日期区间、多选、时间选择或时区转换。
- 逻辑尺寸使用设备无关坐标；绘制使用控件 palette 和 style metric，不读取文件、不解析 SVG、不持有锁。
- 公共头仅声明接口，使用 `std::unique_ptr<...Private>`；公开析构函数在 `.cpp` 定义。
- 所有公开类和公开方法必须有简体中文 Doxygen；命名空间使用单层 `namespace ZzFluentUI { ... }`。
- 每个逻辑任务本机验证后立即提交。CI 暂不作为本阶段阻断项，不调用 GitHub CLI。

## 2. 关键设计结论

### 2.1 为什么复用 QCalendarWidget

Qt 已经实现以下高风险语义，Zz 不应重新实现：

- 公历及 `QCalendar` 后端的日期换算；
- `minimumDate`、`maximumDate`、`selectedDate` 和当前页同步；
- 方向键、PageUp/PageDown、Home 及焦点导航；
- `QLocale`、周起始日、星期标题和 RTL；
- `clicked`、`activated`、`selectionChanged`、`currentPageChanged`；
- 屏幕阅读器可见的表格和日期单元格语义。

`ZzCalendar` 只负责 Fluent 外观。这样不会为 42 个可见日期创建 42 个自定义 QWidget，也不会按可用日期范围预分配模型数据。

### 2.2 为什么 ZzCalendarPicker 继承 QDateEdit

`QDateEdit` 已经提供输入法、文本选择、步进、校验、日期范围、快捷键和可访问 SpinBox 语义。`ZzCalendarPicker` 只改变默认装配：

```text
ZzCalendarPicker : QDateEdit
  -> calendarPopup = true
  -> calendarWidget = one ZzCalendar
  -> displayFormat = construction-time locale short date format
```

调用者之后设置的 `displayFormat` 不会在主题、语言或 palette 事件中被控件覆盖。

### 2.3 所有权

```text
QWidget parent tree
  ZzCalendarPicker
    QDateEdit internal children
    ZzCalendar (ownership transferred through setCalendarWidget)

std::unique_ptr tree
  ZzCalendarPicker
    ZzCalendarPickerPrivate (only non-owning ZzCalendar*)
```

禁止让 `std::unique_ptr` 同时拥有具有 QObject parent 的子控件。`ZzCalendarPrivate` 只保存值和固定数组，不继承 QObject。

### 2.4 绘制与缓存

`ZzCalendar::paintCell()` 委托给 private helper。每次绘制只执行常数次日期比较和 painter 操作：

1. 根据 `minimumDate()`、`maximumDate()` 和 `isEnabled()` 选择 palette group。
2. 根据 `yearShown()`、`monthShown()` 判断相邻月份日期。
3. 选中日使用 `QPalette::Highlight` 圆角填充与 `HighlightedText`。
4. 今日使用 `QPalette::Highlight` 描边；今日同时被选中时不重复绘制边框。
5. 相邻月份日期使用 `QPalette::Disabled` 文本，不改变其 Qt 点击语义。
6. 焦点仅在选中日期上绘制可见边框，不创建动画对象。
7. 日期文本从 private 的 `std::array<QString, 31>` 读取，paint path 不重复格式化 1-31。

颜色由 `ZzFluentStyle::standardPalette()` 和当前控件 palette 提供。日历不直接持有 `ZzThemeController`，因此应用切换 style 或 palette 时仍遵循 Qt 生命周期。

## 3. 公共 API

### 3.1 ZzCalendar

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzCalendar.h`:

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QCalendarWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QPainter;

namespace ZzFluentUI {

class ZzCalendarPrivate;

/**
 * @brief 保留 Qt 日期语义并提供 Fluent 单元格呈现的日历。
 *
 * 控件必须在 GUI 线程创建和访问。日期范围、区域设置、键盘导航、
 * 选择信号和无障碍表格语义由 QCalendarWidget 提供。
 */
class ZZ_FLUENT_UI_EXPORT ZzCalendar final : public QCalendarWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzCalendar)

public:
    /**
     * @brief 创建默认显示当前月份和当前日期的日历。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzCalendar(QWidget *parent = nullptr);

    /** @brief 销毁私有绘制缓存，Qt 子对象由 parent 关系释放。 */
    ~ZzCalendar() override;

protected:
    /** @brief 使用当前 palette 绘制单个可见日期。 */
    void paintCell(
        QPainter *painter,
        const QRect &rect,
        QDate date) const override;

    /** @brief 在 palette、style、字体或启用状态变化后刷新可见日期。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzCalendarPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

不重复声明 `selectedDate`、`minimumDate`、`maximumDate`、`clicked` 等 Qt 已有 API，也不增加同义属性。

### 3.2 ZzCalendarPicker

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzCalendarPicker.h`:

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QDateEdit>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzCalendar;
class ZzCalendarPickerPrivate;

/** @brief 使用 ZzCalendar 弹层的 Fluent 日期编辑器。 */
class ZZ_FLUENT_UI_EXPORT ZzCalendarPicker final : public QDateEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzCalendarPicker)

public:
    /**
     * @brief 创建可键盘编辑并可弹出日历的日期选择器。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzCalendarPicker(QWidget *parent = nullptr);

    /** @brief 销毁私有装配状态，日历由 QDateEdit 所有。 */
    ~ZzCalendarPicker() override;

    /**
     * @brief 返回当前唯一的 Zz 日历弹层。
     * @return 非空、非拥有指针，生命周期不超过本选择器。
     */
    [[nodiscard]] ZzCalendar *calendar() const noexcept;

private:
    std::unique_ptr<ZzCalendarPickerPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

日期、范围、格式、弹层开关和 `dateChanged` 使用 `QDateEdit` 原生 API。

## 4. 文件结构

### 4.1 生产代码

- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzCalendar.h`
- Create: `ZzFluentUI/widgets/src/ZzCalendar.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzCalendarPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzCalendarPrivate.cpp`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzCalendarPicker.h`
- Create: `ZzFluentUI/widgets/src/ZzCalendarPicker.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzCalendarPickerPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzCalendarPickerPrivate.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`

### 4.2 测试与质量门禁

- Create: `ZzFluentUI/tests/ZzCalendarControlsTest.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/ZzFluentAccessibilityTest.cpp`
- Modify: `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`
- Modify: `ZzFluentUI/tests/ZzBasicControlsBenchmark.cpp`
- Modify: `tests/InstallConsumer/main.cpp`
- Modify: `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.h`
- Modify: `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp`

## 5. Task 1：建立日历与日期选择器行为契约

- [ ] 在 `ZzFluentUI/tests/CMakeLists.txt` 注册 `ZzCalendarControlsTest`，链接 `Qt6::Test`、`Qt6::Widgets` 和 `Zz::FluentUI`，启用 AUTOMOC、严格告警和 Sanitizer。
- [ ] 测试名固定为 `fluent.calendar-controls`，标签为 `fluent;unit;component;accessibility`，使用 `QT_QPA_PLATFORM=offscreen`。
- [ ] 先写以下失败测试，再加入生产源码：
  - 默认 selected date 有效，grid 关闭，垂直周号隐藏，选择模式为单选。
  - `setDateRange()` 与 `setSelectedDate()` 保持 Qt 范围收敛行为。
  - 方向键改变选中日期，PageUp/PageDown 改变当前页。
  - `QLocale`、first day of week 和 RTL 设置不会破坏日期选择。
  - `ZzCalendarPicker::calendar()` 非空且与 `calendarWidget()` 为同一对象。
  - picker 与 calendar 的 date/min/max 双向同步由 `QDateEdit` 保证。
  - 重复设置相同日期不产生额外业务包装信号；只观察 Qt 原生 `dateChanged`。

红灯命令：

```bash
cmake --build --preset linux-gcc-debug --target ZzCalendarControlsTest
```

预期：target 或公共头不存在，构建失败；不提交红灯状态。

## 6. Task 2：实现 ZzCalendar

- [ ] 创建公开头和四文件 PIMPL。
- [ ] private 构造时一次性缓存字符串 `1` 到 `31`。
- [ ] 构造函数设置：
  - `setGridVisible(false)`；
  - `setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader)`；
  - `setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames)`；
  - `setSelectionMode(QCalendarWidget::SingleSelection)`；
  - `setFocusPolicy(Qt::StrongFocus)`。
- [ ] 连接 `currentPageChanged` 到 `updateCells()`，不得创建中间模型。
- [ ] `paintCell()` 对 painter 空指针或无效 date 采用 Debug 断言并安全返回。
- [ ] 使用 `QPainter::save()/restore()`，开启抗锯齿和文字抗锯齿。
- [ ] 选中背景、今日边框、焦点、禁用态、相邻月份和高对比度全部从 palette 推导，不写死主题 RGB。
- [ ] 边框宽度按 `devicePixelRatioF()` 对齐至少一个物理像素。
- [ ] `changeEvent()` 先调用基类，再对 PaletteChange、StyleChange、FontChange、EnabledChange、ApplicationPaletteChange 执行 `updateCells()`；只有字体和 style 变化调用 `updateGeometry()`。

最小绿灯：

```bash
cmake --build --preset linux-gcc-debug --target ZzCalendarControlsTest
ctest --preset linux-gcc-debug -R '^fluent\.calendar-controls$' --output-on-failure
```

## 7. Task 3：实现 ZzCalendarPicker

- [ ] 创建公开头和四文件 PIMPL。
- [ ] 构造唯一 `ZzCalendar`，调用 `setCalendarPopup(true)` 和 `setCalendarWidget(calendar)`。
- [ ] 初始 `displayFormat` 使用构造时 `locale().dateFormat(QLocale::ShortFormat)`；之后不覆盖调用者格式。
- [ ] 默认 date 使用 `QDate::currentDate()`，保留 `QDateEdit` 的 date range 默认值。
- [ ] 设置 StrongFocus、accelerated 和 wrapping 的明确默认值：`setAccelerated(true)`、`setWrapping(false)`。
- [ ] `calendar()` 只返回 private 中的非拥有指针，不提供替换日历 API。
- [ ] 不覆写 `showPopup()`，不手工计算屏幕坐标，不创建 `Qt::Popup` 窗口。

完成后扩展 Task 1 测试，并验证 picker 销毁后没有悬空弹层或 QObject 双重释放。

## 8. Task 4：可访问性、视觉与性能

### 8.1 可访问性

- [ ] 在 `ZzFluentAccessibilityTest.cpp` 中构造 calendar 和 picker。
- [ ] 为顶层控件设置可本地化 `accessibleName`，确认 `QAccessible::queryAccessibleInterface()` 非空。
- [ ] 确认 picker 可聚焦，calendar 的内部可访问子树由 Qt 提供；禁止注册替代 Qt 日期表格的自定义 interface。
- [ ] 用键盘完成 date 改变，测试不得依赖鼠标坐标或平台弹层位置。

### 8.2 视觉

- [ ] 在 `ZzFluentScreenshotTest.cpp` 增加一个固定 7×6 日期面的 calendar 和一个 picker。
- [ ] 日期固定为 `2026-08-05`，locale 固定 `QLocale::c()`，first day 固定 Monday，避免当前日期和系统区域造成基线漂移。
- [ ] 覆盖 Light、Dark、HighContrast、Disabled、selected、today 与相邻月份文本。
- [ ] 使用现有四个 DPR 基线和文字遮罩策略，不新增另一套截图框架。
- [ ] 只在本机 Qt 6.11.1 参考环境更新基线；其他 Qt minor 使用既有兼容容差。

### 8.3 性能

- [ ] 在 `ZzBasicControlsBenchmark.cpp` 增加日历可见月重复 render 测量。
- [ ] 预热 10 次、测量 100 次，页面在 12 个月之间循环。
- [ ] 每轮 render 前只修改 current page 和 selected date，不创建新控件。
- [ ] 记录 P50/P95/max；参考环境 P95 预算为 16.7ms，普通开发环境只记录不执行绝对阈值。
- [ ] 1000 次页面切换前后，calendar/picker 后代 QObject、动画和 timer 数量必须保持不变。

## 9. Task 5：示例、安装消费与边界

- [ ] 在 `ZzFluentControlsGallery` 增加不带业务逻辑的 Calendar 区域，展示固定日期、范围和 picker/date 同步。
- [ ] 连接只允许更新相邻展示标签，不访问数据模型、网络、存储或 AppCore。
- [ ] gallery 必须在 1024×720 与 800×600 下不遮挡；需要滚动时复用现有页面容器。
- [ ] 在 `tests/InstallConsumer/main.cpp` 只包含并构造两个安装后的公共头，验证 `Zz::FluentUI` 传递依赖完整。
- [ ] 重新配置后，现有 public-header aggregate 必须自动发现两个新头；不得手写第二份头文件列表。
- [ ] 运行 `architecture.complete-audit` 与 `architecture.zzfluentui-boundaries`，确认无 Qt Private、QWK、业务或第三方实现依赖。

## 10. 提交边界

### 提交 A：计划

```text
文档：规划Fluent日历控件批次

定义ZzCalendar与ZzCalendarPicker的Qt语义复用、PIMPL所有权、
绘制缓存、测试、性能和安装消费边界。
```

### 提交 B：生产契约

```text
控件：实现Fluent日历与日期选择器

基于QCalendarWidget和QDateEdit保留日期、键盘、区域设置与无障碍语义，
增加无分配日期文本缓存和palette驱动的Fluent单元格绘制。
```

生产契约提交必须同时包含 `ZzCalendarControlsTest`，禁止提交不能链接的公共头。

### 提交 C：完整质量面

```text
测试：补齐日历视觉与性能门禁

将日历控件接入无障碍、截图、性能、安装消费和交互画廊，
验证多主题、高DPI、对象数量稳定和公开依赖边界。
```

## 11. 本机完成门禁

每个实现提交至少执行对应定向测试。整个批次结束前执行：

```bash
cmake --build --preset linux-gcc-release --parallel 2
ctest --preset linux-gcc-release --output-on-failure
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy --parallel 2
cmake --build --preset linux-clang-asan --parallel 2
ctest --preset linux-clang-asan --output-on-failure
```

若修改 screenshot baseline 或 benchmark，再执行本机参考 preset 对应测试。Windows 与 macOS 只做源码级条件分支审查，不调用 GitHub CLI，不把远端 CI 作为当前开发阻断项。

## 12. 完成定义

- `ZzCalendar` 与 `ZzCalendarPicker` 的公开 API 不重复 Qt 已有日期属性。
- 日历 paint path 不读取文件、不解析资源、不创建 QWidget、不按日期范围分配。
- picker 不自建 popup 定位或输入校验器。
- 键盘、RTL、locale、范围、禁用和无障碍测试通过。
- Light、Dark、HighContrast 与 1.0/1.25/1.5/2.0 DPR 视觉门禁通过。
- 页面切换与 render 性能满足参考预算，对象、timer 和 animation 数量稳定。
- 安装消费者、公共头、架构边界、GCC、Clang Tidy 和 ASan/UBSan 通过。
- 复杂卡片和可撕标签页仍保持独立待实施批次，不被本提交隐式引入。
