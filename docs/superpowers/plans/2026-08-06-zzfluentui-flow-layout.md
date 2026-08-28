# ZzFluentUI 高性能流式布局实施计划

**目标：** 在 `Zz::FluentUI` 中交付可安装、可测试、支持 RTL 且具有明确性能预算的 `ZzFlowLayout`。布局负责把普通 widget、spacer 和嵌套 layout 按可用宽度自动换行，不保存业务数据，不创建逐项对象或动画，并完整遵循 Qt 的 `QLayoutItem` 所有权和 height-for-width 契约。

**架构：** `ZzFlowLayout` 继承 `QLayout`，公开层只声明布局 API，几何计算、item 容器和单宽度高度缓存放入非 `QObject` 的 `ZzFlowLayoutPrivate`。每次有效重排只线性访问可见 item 一次；RTL 通过 Qt 公开的视觉矩形转换完成；布局变化同步生效，动画由调用方在页面层显式编排。

**技术约束：** Qt 6.8+、C++20、四文件 Pimpl、传统命名空间、简体中文 Doxygen、无 Qt Private API、无 QSS、无动态属性、无平台原生头、无业务模型访问。

## 1. 批次边界

本批新增：

- `ZzFlowLayout` 公共布局类型、独立单元测试和安装消费验证。
- 横向起始方向流动、自动换行、LTR/RTL、隐藏项、spacer、嵌套 layout 和 height-for-width item 支持。
- 独立水平/垂直间距，`-1` 表示使用 Qt style metric；修改后同步失效并重新布局。
- 1000 个 item 的 Release 重排性能预算、画廊响应式示例和四主题/四档 DPR 截图基线。

本批不实现：

- 瀑布流、虚拟化、拖拽重排、业务筛选、分页或 model/view 数据绑定。
- 垂直优先流动、跨行 stretch、按业务权重分配剩余空间或 masonry 高度平衡。
- 布局动画。几何动画会改变同步布局契约并为每次 resize 引入对象、事件和中间状态，页面层需要动画时应在布局稳定后独立处理。
- 对标准 `QLayout`、`QBoxLayout` 或 `QGridLayout` 的包装。

标准 `QSlider` 已由应用级 `ZzFluentStyle` 覆盖 Fluent 轨道、活动区和手柄，不新增只转发 Qt API 的 `ZzSlider`。旧版其余控件继续按价值和依赖关系分批审计，不能因为文件存在就机械迁移。

## 2. 旧版逐行审计结论

审计来源固定为旧仓库 `/home/zz/Jackfahdin/github/ZzPureToolsFrame/ZzFluentUI`，只读取交互意图，不复制实现。

### 2.1 `ZzFlowLayout.h`

| 行 | 结论 |
|---:|---|
| 1-10 | include guard 可用，但公共头暴露了未使用的 `QMap`、`QStyle` 和旧 `ZzProperty.h`，增加消费者依赖。 |
| 11-15 | 私有类位于全局命名空间并依赖旧宏生成 d-pointer；新版必须进入 `ZzFluentUI` 单一命名空间并显式使用 `std::unique_ptr`。 |
| 17-19 | 两个构造入口有价值，但 margin 参数重复了 `QLayout::setContentsMargins()`；新版构造只接收 parent 和轴向 spacing。 |
| 21-32 | 覆盖的 `QLayout` 基础接口方向正确，但所有公开方法缺少中文 Doxygen，`heightForWidth` 参数也无语义名称。 |
| 34 | `setIsAnimation()` 只有 setter、没有 getter/通知/reduced-motion 契约，且把页面动画混入同步布局；新版删除。 |

### 2.2 `ZzFlowLayout.cpp`

| 行 | 结论 |
|---:|---|
| 8-25 | 两个构造函数重复初始化，裸 `new` 私有对象的父子/释放语义依赖宏；新版委托构造并使用 RAII。 |
| 28-35 | 析构逐个删除仍归 layout 所有的 `QLayoutItem` 符合 Qt 自定义布局模式，应保留但由私有容器明确执行。 |
| 37-40 | `addItem()` 未拒绝空指针、未清理 height-for-width 缓存、未显式 invalidate；新版固定这些边界。 |
| 43-67 | 轴向 spacing 回退 style metric 的意图正确；旧版每个 item 又重复回退一次 `layoutSpacing()`，新版在一次布局中解析稳定值。 |
| 68-88 | count/itemAt/takeAt 基本语义正确；`takeAt()` 没有清除 `_lastGeometryMap`，删除布局项后缓存仍保存悬空地址。 |
| 90-94 | 动画开关会让相同布局 API 产生同步或异步两种完成语义，应删除。 |
| 96-109 | 非扩展和 height-for-width 契约正确；宽度为负或小于左右 margin 时未规范化，可能返回异常高度。 |
| 112-117 | 每次 `setGeometry()` 无条件完整重排；新版保留 O(n) 必要路径，并用 invalidate generation 避免相同矩形的冗余工作。 |
| 119-135 | `sizeHint()` 直接等于 minimum，minimum 只取单项最大值；作为可换行布局可接受，但旧版未跳过隐藏项，也没有按 item minimum/maximum/height-for-width 规范化尺寸。 |

### 2.3 `ZzFlowLayoutPrivate.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-17 | private 继承 `QObject`、声明 `Q_OBJECT` 却没有信号、属性或事件，给每个布局增加不必要的 meta-object 和父对象成本；新版 private 是普通 final 类。 |
| h:20-26 | `QMap<QLayoutItem*, QPoint>` 使查找为 O(log n)，且不能随 `takeAt()` 正确清理；新版不保存逐项历史几何。间距字段还存在未初始化风险。 |
| cpp:16-24 | 单次顺序流动的基础思路正确，但没有处理空有效矩形、RTL 或非 widget item。 |
| cpp:25-37 | 每项直接解引用 `item->widget()`；spacer 或嵌套 layout 的 `widget()` 为 null，会崩溃。spacing 也在循环中重复调用 style。 |
| cpp:39-46 | 同一 item 多次调用 `sizeHint()`，使用 `QRect::right()` 的包含边界计算易产生一像素换行误差。 |
| cpp:47-75 | `_lastGeometryMap` 在 test-only 的 `heightForWidth()` 中也写入；const 查询改变长期状态。移动时为每项分配 400 ms animation，快速 resize 会产生大量 QObject、事件和临时几何。 |
| cpp:53-56 | 用 `(0, 0)` 判断“首次布局”不成立，合法目标原点与未布局状态无法区分。 |
| cpp:59-74 | animation 只适用于 widget，spacer/nested layout 再次空指针；`DeleteWhenStopped` 不能阻止 resize 期间并行动画争用同一 geometry。 |
| cpp:76-85 | 非动画路径同步设置几何可保留；旧版未跳过 `isEmpty()` 项，隐藏 widget 仍参与换行和高度。 |
| cpp:88-104 | smart spacing 的 parent widget/layout 分支意图正确，但强转 layout 依赖调用方类型假设，且未处理 style 返回负数后的稳定 fallback。 |

## 3. 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzFlowLayout.h`：

```cpp
namespace ZzFluentUI {

class ZzFlowLayoutPrivate;

class ZZ_FLUENT_UI_EXPORT ZzFlowLayout final : public QLayout
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFlowLayout)
    Q_PROPERTY(int horizontalSpacing READ horizontalSpacing
                   WRITE setHorizontalSpacing
                   NOTIFY horizontalSpacingChanged)
    Q_PROPERTY(int verticalSpacing READ verticalSpacing
                   WRITE setVerticalSpacing
                   NOTIFY verticalSpacingChanged)

public:
    explicit ZzFlowLayout(QWidget *parent = nullptr);
    ZzFlowLayout(
        int horizontalSpacing,
        int verticalSpacing,
        QWidget *parent = nullptr);
    ~ZzFlowLayout() override;

    [[nodiscard]] int horizontalSpacing() const noexcept;
    void setHorizontalSpacing(int spacing);
    [[nodiscard]] int verticalSpacing() const noexcept;
    void setVerticalSpacing(int spacing);

    void addItem(QLayoutItem *item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem *itemAt(int index) const override;
    [[nodiscard]] QLayoutItem *takeAt(int index) override;
    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize minimumSize() const override;
    [[nodiscard]] QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;
    void invalidate() override;

Q_SIGNALS:
    void horizontalSpacingChanged(int spacing);
    void verticalSpacingChanged(int spacing);

private:
    std::unique_ptr<ZzFlowLayoutPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

公开类、构造函数、属性、覆盖方法和信号全部写简体中文 Doxygen。spacing 写入小于 `-1` 时规范为 `-1`；相同规范值无副作用。公开 getter 返回配置值，`-1` 明确表示运行时读取 parent/style 的逻辑像素间距。

## 4. 所有权与失效契约

- `addItem(nullptr)` 在 Debug 触发 `Q_ASSERT`，Release 不改变状态；有效 item 追加到末尾、清除缓存并调用 `QLayout::invalidate()`。
- `itemAt()` 对负数和越界返回 null；`takeAt()` 对无效 index 返回 null且无副作用，有效移除返回所有权给调用方并立即失效。
- 析构只删除仍保存在布局中的 `QLayoutItem`；widget 生命周期继续遵循 Qt 父子对象规则，不由 layout item 析构重复删除。
- private 不继承 QObject，不持有逐 item QObject、动画、timer、pixmap、业务数据或平台对象。
- `invalidate()` 清除最近一次 `heightForWidth(width)` 缓存和相同 geometry 快速路径。item 添加/移除、spacing 修改以及 Qt 传入的 widget size hint/layout request 都必须进入此路径。
- 单宽度缓存只保存 `width + calculatedHeight + generation`，不保存 item 地址或业务状态；不同宽度重新线性计算。

## 5. 几何算法

### 5.1 有效 item 与尺寸

- 遍历容器时跳过 null；仅当 item 包含 widget 且 `isEmpty()` 时跳过，使隐藏 widget 不占行、间距或高度。spacer 和嵌套 layout 即使报告 empty 也继续按尺寸契约参与布局；重新显示 widget 后 Qt 的 LayoutRequest 触发 invalidate。
- 每个有效 item 每次重排只读取一次 size hint、minimum、maximum 和 control type。首选尺寸先扩展到 minimum，再限制到 maximum。
- item 首选宽度大于可用行宽时，在不低于 minimum width 的前提下收敛到可用宽度；minimum 本身仍超宽时允许单项溢出，但必须独占一行，后续项不能重叠。
- `item->hasHeightForWidth()` 时用最终分配宽度计算高度，再限制到 item 的 minimum/maximum height。
- spacer 和嵌套 layout 不要求 `widget()` 非空；style 查询优先使用 parent widget，缺失时使用 `QApplication::style()`。

### 5.2 换行、间距与 RTL

- contents margin 先从输入 rect 中扣除；负宽度按 0 处理，空布局高度只包含上下 margin。
- 水平/垂直配置 spacing 非负时直接使用。值为 `-1` 时先读取 `PM_LayoutHorizontalSpacing/PM_LayoutVerticalSpacing`；style 仍返回负数时，按相邻 item 的 `controlTypes()` 调用 `layoutSpacing()`，最终负值稳定回退为 0。
- 第一项没有前导 spacing，最后一项没有尾随 spacing；只有下一项的 `x + width` 严格超过有效宽度时换行，消除 `QRect::right()` 包含边界歧义。
- 算法在逻辑 LTR 坐标中确定行和矩形，然后使用 `QStyle::visualRect(parentLayoutDirection, effectiveRect, logicalRect)` 镜像；RTL 从右侧开始且保持 item 的逻辑添加顺序。
- 每行高度是该行 item 最终高度最大值。默认顶端对齐；item 显式带 `AlignVCenter` 或 `AlignBottom` 时只在本行内调整垂直位置，不改变行高。无效的多重垂直标志按顶端处理。
- `setGeometry()` 同步完成所有有效 item 几何设置。生产代码不创建或启动任何动画。

### 5.3 尺寸提示

- `hasHeightForWidth()` 恒为 true；`heightForWidth(width)` 返回给定总宽度下的完整换行高度并包含 margins，重复查询相同 width 且 generation 未变时 O(1) 返回。
- `minimumSize()` 取所有有效 item minimum 的最大宽高并加 margins，表示至少能容纳一项的稳定下界。
- `sizeHint()` 以所有有效 item 的规范化首选尺寸计算单行首选宽度和最大高度并加 margins；宽度累加使用有界整数运算，超过 `QLAYOUTSIZE_MAX` 时饱和，避免大量 item 溢出。
- 空布局的 minimum/sizeHint 为 margins 尺寸，height-for-width 同样只返回垂直 margins。

## 6. 单元与组件测试

新增 `ZzFluentUI/tests/ZzFlowLayoutTest.cpp` 和 CTest `fluent.flow-layout`：

- 构造默认值、spacing 规范化、相同值无重复信号、属性写入后 invalidate。
- add/count/itemAt/takeAt 顺序、无效 index、空 item 防御和析构所有权。
- 固定宽度下同一行、边界刚好容纳、超过一像素换行、超宽单项独占行。
- contents margins、不同水平/垂直 spacing、style metric fallback 和零 spacing。
- LTR/RTL 镜像，逻辑添加顺序不变；resize 后同步恢复/增加行数。
- 隐藏 widget 不占空间，再显示后重新进入布局。
- `QSpacerItem` 和嵌套 `QLayout` 不崩溃并获得正确 geometry。
- 自定义 height-for-width 测试 item 证明最终宽度传入高度计算，minimum/maximum 得到遵守。
- `AlignTop/AlignVCenter/AlignBottom` 在同一行使用相同行高并得到正确纵向位置。
- 重复同 width 的 `heightForWidth()` 命中缓存；add/take/spacing/size hint invalidation 后重新计算。
- 使用 `QTest::newRow()` 覆盖 LTR/RTL、0/1/多项和极窄宽度，避免只验证单一路径。

## 7. 构建、安装与架构门禁

- 将四个文件接入 `ZzFluentUI/CMakeLists.txt`，公共头加入 AUTOMOC 和安装导出。
- `tests/InstallConsumer/Gui/main.cpp` 实例化已安装的 `ZzFlowLayout`，添加 widget、执行 geometry 并核对换行高度。
- 公开头逐文件编译、public MOC、metatype、fresh install consumer 和 package relocation 必须自动覆盖新头。
- `CheckZzFluentUIBoundaries.cmake` 继续禁止 `ZzPureTools`、`ZzWindowKit`、QWK、Qt Private API、链式命名空间和业务层关键词。
- 源码只能使用 Qt Core/Gui/Widgets 公共 API 和标准 C++20；Windows MSVC、Windows Qt SDK MinGW、macOS arm64/x86_64 先执行 preset、include、条件编译和平台头静态审计，未在对应工具链实际编译前不得写“已通过”。

## 8. 性能预算

扩展 `ZzBasicControlsBenchmark`：

- 构造一个含 1000 个固定 size hint 轻量 item 的可见 `ZzFlowLayout`，预热后在 320/640/960/1280 四种宽度间循环 200 次 `setGeometry()`。
- 输出 P50、P95、max、每次访问 item 数、layout QObject 后代、animation 和 timer 数。
- reference 发布机强制 P95 不超过 4 ms；非 reference 环境只记录。若首轮实测显示门限与现有发布机稳定噪声不匹配，只允许依据原始数据单独提交调整，不能静默放宽。
- 同 width 连续调用 `heightForWidth()` 10000 次必须保持 O(1) 缓存路径；变更 spacing 后首个查询重新计算，后续再次命中。
- 100 项与 1000 项重排耗时比不超过 15，锁定线性复杂度并允许固定测量开销。
- private 后代、`QPropertyAnimation` 和 `QTimer` 数必须为 0；布局本身只增加一个公开 QObject。

## 9. 示例与视觉基线

- 在 `ZzFluentControlsGallery` 增加“流式布局”区域，以不同长度的标准按钮/标签展示窄宽换行、宽屏合并、隐藏项和 RTL；示例只承载展示文本，不访问业务 model/service。
- gallery 的外层 `ZzScrollArea` 保证小窗口仍可浏览，flow 容器通过真实 resize 触发响应式重排，不用手写位置。
- `ZzFluentScreenshotTest` 增加独立固定尺寸 `flow-layout` surface：同图显示窄 LTR、宽 LTR 和窄 RTL 三组，使用稳定短文本并记录 item 几何覆盖断言。
- 生成 Light、Dark、HighContrast 三主题与 DPR 1.0/1.25/1.5/2.0 共 12 张参考图，继续执行严格比较；新增 surface 不修改既有图片容差。
- 人工检查至少 Light/Dark/HighContrast 的 DPR 1.0 和 Light 的 DPR 2.0，确认无重叠、裁切、错误行距或 RTL 反序。

## 10. 提交顺序

每个逻辑批次验证后立即提交，提交标题为中文简述，正文用多个中文段落记录细节：

```text
文档：规划高性能流式布局批次
控件：实现高性能流式布局
测试：接入流式布局质量与安装消费
性能：锁定流式布局重排预算
测试：补齐流式布局多主题视觉基线
文档：记录流式布局批次交付结果
```

不 push，不调用 GitHub CLI，不处理远端 CI，不下载 Qt。

## 11. 最终验证矩阵

使用本机 Qt `/home/zz/Qt/6.11.1/gcc_64`：

- GCC 15 shared Release 全量 build + CTest。
- GCC 15 static Release 全量 build + CTest。
- Clang 20 ASan+UBSan 全量 build + CTest。
- shared/static `ZzClangTidy`。
- public headers、public MOC、metatype、架构边界和 preset matrix。
- fresh install consumer、package relocation、二进制依赖与许可证安装审计。
- 四个示例 smoke、flow-layout 四档 DPR 截图和 Release benchmark。

最终结果追加到本文，必须记录 commit、Qt/编译器、自动验证范围、性能原始数据、截图人工检查范围，以及 Windows/macOS 尚待对应工具链或真机验证的限制。

## 12. 交付结果

**状态：** 已于 2026-08-06 完成本批次实现与本机质量门禁。验证基于提交 `11c1d729ce6789b29b760c41348bef050731f2c7`，使用 Qt 6.11.1、GCC 15.2.0 和 Clang 20.1.8。

### 12.1 提交记录

- `dba3120`：规划高性能流式布局批次，完成旧版逐行审计、公开契约、性能预算和验证矩阵设计。
- `9e5d241`：实现四文件 Pimpl 的 `ZzFlowLayout`，覆盖 widget、spacer、嵌套 layout、隐藏项、height-for-width、RTL 与双轴 spacing。
- `5fad78b`：接入 25 项布局单元测试、安装消费、公开头和架构质量门禁。
- `390986d`：增加 1000 项重排、线性复杂度、高度缓存和对象稳定性性能门禁。
- `11c1d72`：接入控件画廊以及 Light、Dark、HighContrast 三主题四档 DPR 的 12 张视觉基线。

### 12.2 Linux 自动验证

- `linux-gcc-release` shared Release 全量构建与 CTest 通过，共 `95/95`。
- `linux-static-release` static Release 全量构建与 CTest 通过，共 `95/95`。
- `linux-clang-asan-benchmarks` 在 ASan+UBSan 下全量构建与 CTest 通过，共 `107/107`。
- shared/static `ZzClangTidy` 均完成 `144/144` 个翻译单元，未输出诊断。
- fresh install consumer、package relocation、公开头独立编译、完整架构审计、FluentUI 边界、二进制依赖和许可证安装审计通过。
- `ZzPlatformCompileTest` 与四个示例的 offscreen smoke 通过；截图回归在 DPR 1.0、1.25、1.5、2.0 四档全部通过。

### 12.3 性能结果

在活动 Linux reference 发布机的固定 CPU 亲和、Xvfb 和 llvmpipe 环境中，1000 个 item 连续重排 200 帧的 P50 为 `0.014 ms`、P95 为 `0.016 ms`、max 为 `0.019 ms`，低于 `4 ms` 门限。累计访问 `200000` 个 item，单帧最大访问 `1000` 个 item。

10000 次同宽度 `heightForWidth()` 缓存查询耗时 `0.013 ms`；1000 项与 100 项的耗时比为 `10.085`，低于线性复杂度门限 `15`。布局拥有 1 个 QObject 后代、0 个 animation 和 0 个 timer，重排过程没有产生逐项对象。

### 12.4 视觉检查

新增 `flow-layout-{light,dark,high-contrast}.png`，每个主题覆盖 DPR 1.0、1.25、1.5、2.0，共 12 张。已人工检查 Light、Dark、HighContrast 的 DPR 1.0 和 Light 的 DPR 2.0：窄容器稳定换为三行，宽容器保持单行，RTL 与 LTR 正确镜像，没有重叠、裁切、错误行距或逻辑顺序反转。

### 12.5 跨平台状态

- preset matrix contract 与 Linux/Windows/macOS gate script contract 通过，矩阵继续登记 Windows MSVC shared/static、Windows Qt SDK MinGW shared/static，以及 macOS arm64/x86_64 shared/static。
- 新增源码没有 `Q_OS_*`、`_WIN32`、`__APPLE__` 平台分支，没有 Qt Private API、平台原生头、编译器扩展、绝对路径、`QWindowKit::` 依赖泄漏或链式命名空间。
- Windows MSVC、Windows Qt SDK MinGW 与 macOS 当前只完成源码、preset、公开 ABI、依赖方向和条件编译静态审计，尚未在对应平台完成编译、安装消费或真机交互验证；不得将本节结果表述为这些平台已经运行通过。
