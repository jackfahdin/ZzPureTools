# ZzFluentUI 高性能轮播视图实施计划

**目标：** 将旧版 `ZzPromotionView` 中有价值的“单个当前媒体项、前后切换、位置指示和过渡”能力重构为 `ZzCarouselView`，使其可消费任意 `QAbstractItemModel`、保留 Qt Model/View 和无障碍协议，并让每帧绘制复杂度与模型总行数无关。

**架构：** `ZzCarouselView` 继承 `QAbstractItemView`，只通过标准 model role 读取当前索引的展示数据。默认 delegate 绘制图片、标题和说明；view 负责当前索引、键盘/滚轮/按钮导航、最多两个可见 item 的过渡以及有界位置指示。应用层负责创建 model、加载图片、自动推进、URL 打开、路由和业务命令。

**技术约束：** Qt 6.8+、C++20、四文件 Pimpl、简体中文 Doxygen、传统命名空间、无 Qt Private API、无 QSS、无动态属性、无每项 QWidget、无平台原生头、无内部 timer、无网络/文件/系统时间访问。

## 1. 批次边界

本批实现：

- 新增 `ZzCarouselView`，继承 `QAbstractItemView` 并遵循外部 model/selection model 所有权。
- 单项全幅展示、标题、说明、图片适配、空图片占位、enabled/disabled、focus、selected、RTL 和三主题绘制。
- 前一项/后一项命令、边界或环绕模式、键盘、滚轮、鼠标激活和两个无文本箭头按钮。
- 一个持久 `QVariantAnimation` 完成相邻项滑动；减少动效或隐藏状态直接同步终态。
- 最多七个位置圆点；模型再大也不按总行数创建对象或绘制全部指示器。
- 单元测试、安装消费、控件画廊、性能门禁和三主题四档 DPR 视觉基线。

本批不实现：

- 自动轮播、播放间隔、暂停策略、曝光统计或系统时间。应用 presenter 可持有 timer 并调用 `showNext()`。
- URL 打开、导航、下载、远程图片、文件解码、图片缓存、业务埋点或命令执行。
- 在 view 内创建、拥有或修改业务 model；view 只读取 `QAbstractItemModel` 的公开展示 role。
- 无限复制 model、首尾克隆 item 或为每行创建 widget/delegate/animation。
- 旧版 `ZzPromotionView`、`ZzPromotionCard` API 或几何动画兼容。

## 2. 阶段 10 剩余能力审计

- `ZzSlider`、`ZzProgressBar`、`ZzCheckBox`、`ZzRadioButton`、`ZzListView`、`ZzTableView`、`ZzTreeView`、`ZzToolButton` 和 popup 表面已经由标准 Qt 控件与应用级 `ZzFluentStyle` 覆盖，不再增加空包装类。
- 旧版 `ZzPivot` 的核心是水平标签选择，已由标准 `QTabBar` 和新的 `ZzTabBar` 覆盖；旧版独立 model、style 和 `QScroller` 不迁移。
- `ZzReminderCard`、`ZzPopularCard`、`ZzInteractiveCard` 已收敛为 `ZzActionCard`；`ZzPromotionCard` 与 `ZzAcrylicUrlCard` 的展示能力已收敛为 `ZzImageCard`。
- `ZzPromotionView` 的集合切换能力未被现有组件覆盖，且可独立于营销业务命名，因此本批将其重构为通用 `ZzCarouselView`。

## 3. 旧版逐行审计

审计来源为旧仓库 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzFluentUI`，只读取行为意图，不复制实现。

### 3.1 `ZzPromotionView.h`

| 行 | 结论 |
|---:|---|
| 1-9 | 公共头暴露旧导出宏、pixmap 和属性宏；新版只公开 Qt Model/View 契约，不暴露具体卡片类型。 |
| 10-12 | 前置声明 `ZzPromotionCard` 并以营销场景命名，使容器只能消费一种 widget；新版接受任意 `QAbstractItemModel`。 |
| 13-20 | 展开/收起宽度、当前索引、自动滚动和间隔混在同一个 UI 类型；新版只保留 current row、wrap 和动画时长，业务调度删除。 |
| 22-23 | 构造/析构有效，但旧类未禁用复制移动；新版显式 `Q_DISABLE_COPY_MOVE`。 |
| 25 | `appendPromotionCard()` 将数据、展示和 QObject 所有权绑定；新版沿用 `setModel()`，不接管 model 所有权。 |
| 27-29 | wheel 和 paint 是合理扩展点，但旧类没有键盘、焦点、selection model 或无障碍列表协议；新版由 `QAbstractItemView` 提供基础协议。 |

### 3.2 `ZzPromotionView.cpp`

| 行 | 结论 |
|---:|---|
| 1-9 | UI 类型直接依赖 timer、具体 PromotionCard 和全局主题单例；新版只依赖 Qt 公共 Model/View、foundation 和应用级 style/palette。 |
| 10-20 | 构造函数写入多份像素状态并固定 300 高度；新版提供合理 size hint，实际尺寸由 layout 决定。 |
| 21-22 | object name 与 QSS 只为透明背景服务；新版 viewport 使用 palette/style，不需要字符串路由。 |
| 24-30 | 每个 view 无条件创建自动轮播 timer，并在 timeout 中读取可见性和卡片数量；调度职责整体移出 UI。 |
| 32-33 | 永久连接全局主题单例，隐藏依赖和生命周期；新版由应用安装的 `ZzFluentStyle` 与 palette 响应主题。 |
| 36-38 | 空析构没有表达 timer、card 或 private 的所有权；新版 `unique_ptr` 与 QObject parent 明确所有权。 |
| 40-72 | 两个宽度 setter 重复校验并立即重排全部 card；新版单项 viewport 没有展开/收起双宽度状态。 |
| 74-88 | current index 通过搜索具体 card 改变，但 getter 从 private 副本读取；新版以 selection model 的 `currentIndex()` 为唯一真值。 |
| 90-103 | 开关自动滚动直接启停 timer，且同值也发信号；整体删除。 |
| 105-126 | interval 修改不更新已运行 timer，产生属性值与实际周期不一致；整体删除。 |
| 128-143 | append 修改传入 card 的尺寸、parent 和信号连接，调用方失去原所有权语义；新版不修改 model 生命周期。 |
| 145-150 | wheel 在空集合时仍进入索引运算，存在越界风险；新版空 model 明确 no-op。 |
| 151-168 | 使用两个 single-shot timer 做 400 ms 节流，快速输入被丢弃且每次创建调度对象；新版停止并复用唯一过渡 animation，最新意图立即成为目标。 |
| 169 | 无条件 accept wheel，阻止父滚动区域处理边界事件；新版只有实际完成切换时 accept，否则 ignore。 |
| 172-180 | paint 从全局主题读取 token；新版从 widget palette 和 style 获取当前 snapshot 结果。 |
| 181-194 | 指示器对每张 card 循环绘制，模型规模直接进入 paint 复杂度；新版最多绘制七个圆点。 |
| 183 | 偶数/奇数起点使用冗长单行表达式且混用整数/浮点；新版使用有界 count 和统一 `QRectF`。 |

### 3.3 `ZzPromotionViewPrivate.h/.cpp`

| 行 | 结论 |
|---:|---|
| h:1-18 | private 继承 QObject 并复制全部公开属性，只为宏和动画服务；新版 private 是普通 final 类，只让 animation/button 使用 QObject parent。 |
| h:20-23 | 空构造/析构和具体 card 点击入口没有通用 Model/View 价值。 |
| h:25-32 | 主题、timer、像素常量、节流标志和全部 card 指针构成多套状态；新版当前项来自 selection model，其余为固定数量视觉状态。 |
| h:33-38 | 每项 geometry/ratio 动画和手工环绕几何只适用于旧 card；新版最多同时绘制两个 model index。 |
| cpp:7-14 | private QObject 没有独立信号/槽或资源协议，属于不必要对象。 |
| cpp:16-29 | 切换先按 widget x 坐标推断方向，再从新索引的前一项开始遍历全集；对大集合是 O(N)。 |
| cpp:30-93 | 每次切换为每张 card 创建 geometry animation，并为部分 card 再创建 ratio animation；对象分配和 paint 工作均随总项数线性增长。 |
| cpp:39-89 | 多处分支依赖负坐标、固定宽度和魔法时间点 0.70/0.71，resize、RTL 和中途切换都难以保持连续。 |
| cpp:73-82 | 在 animation valueChanged 中暂停、重写关键帧并恢复，产生重入和生命周期风险。 |
| cpp:94-97 | current index 在动画启动后才更新，信号语义与视觉目标不同步；新版 selection model 先更新真值，动画只表达呈现。 |
| cpp:99-117 | 两个 helper 每次分配自删除 animation；新版只构造一个持久 `QVariantAnimation`。 |
| cpp:119-160 | 每次尺寸/集合变化遍历全部 widget 并写 geometry；新版 resize 只更新当前内容矩形和两个固定按钮。 |
| cpp:122-134 | 0 项分支虽然不访问列表，但 2 项分支依赖特定顺序；当前索引不参与初始布局，状态不完整。 |
| cpp:162-179 | 邻接算法没有空集合保护；新版统一验证 model、root、row count 和目标边界。 |
| cpp:182-186 | 未使用的右边界函数包含 count-3，少于三项时产生无意义值；删除。 |

### 3.4 与旧卡片的耦合结论

旧 `ZzPromotionCard` 在一次按压中可创建多个 property animation，release 不验证按钮、命中位置或 enabled 状态就发点击信号，并手工绘制多层业务标题。其通用图片卡片能力已经由 `ZzImageCard` 以 `QAbstractButton` 语义实现，本批不重新引入旧 card；Carousel 的默认 delegate 只读取标准展示数据。

## 4. 公开 API

新增 `ZzFluentUI/widgets/include/ZzFluentUI/ZzCarouselView.h`：

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QAbstractItemView>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzCarouselViewPrivate;

/**
 * @brief 以固定绘制复杂度展示当前模型项的 Fluent 轮播视图。
 */
class ZZ_FLUENT_UI_EXPORT ZzCarouselView final
    : public QAbstractItemView
{
    Q_OBJECT
    Q_PROPERTY(
        bool wrapAroundEnabled
        READ isWrapAroundEnabled
        WRITE setWrapAroundEnabled
        NOTIFY wrapAroundEnabledChanged)
    Q_PROPERTY(
        int animationDuration
        READ animationDuration
        WRITE setAnimationDuration
        NOTIFY animationDurationChanged)
    Q_PROPERTY(
        int currentRow
        READ currentRow
        WRITE setCurrentRow
        NOTIFY currentRowChanged)
    Q_DISABLE_COPY_MOVE(ZzCarouselView)

public:
    enum ItemDataRole {
        DescriptionRole = Qt::UserRole + 1
    };
    Q_ENUM(ItemDataRole)

    explicit ZzCarouselView(QWidget *parent = nullptr);
    ~ZzCarouselView() override;

    [[nodiscard]] bool isWrapAroundEnabled() const noexcept;
    void setWrapAroundEnabled(bool enabled);

    [[nodiscard]] int animationDuration() const noexcept;
    void setAnimationDuration(int durationMilliseconds);

    [[nodiscard]] int currentRow() const noexcept;
    void setCurrentRow(int row);

    void setModel(QAbstractItemModel *model) override;

    [[nodiscard]] QRect visualRect(const QModelIndex &index) const override;
    void scrollTo(
        const QModelIndex &index,
        ScrollHint hint = EnsureVisible) override;
    [[nodiscard]] QModelIndex indexAt(const QPoint &point) const override;

public Q_SLOTS:
    void showPrevious();
    void showNext();

Q_SIGNALS:
    void wrapAroundEnabledChanged(bool enabled);
    void animationDurationChanged(int durationMilliseconds);
    void currentRowChanged(int row);

protected:
    [[nodiscard]] QModelIndex moveCursor(
        CursorAction cursorAction,
        Qt::KeyboardModifiers modifiers) override;
    [[nodiscard]] int horizontalOffset() const override;
    [[nodiscard]] int verticalOffset() const override;
    [[nodiscard]] bool isIndexHidden(
        const QModelIndex &index) const override;
    void setSelection(
        const QRect &rect,
        QItemSelectionModel::SelectionFlags flags) override;
    [[nodiscard]] QRegion visualRegionForSelection(
        const QItemSelection &selection) const override;

    void currentChanged(
        const QModelIndex &current,
        const QModelIndex &previous) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void changeEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzCarouselViewPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

所有公开类型和方法在实际头文件中补全简体中文 Doxygen。命名空间使用传统单层形式，不使用 `namespace a::b`。

## 5. 数据与状态契约

### 5.1 Model role

- `Qt::DisplayRole`：当前项标题；默认 delegate 单行 elide。
- `ZzCarouselView::DescriptionRole`：可选说明；最多两行，空值不保留空白占位。
- `Qt::DecorationRole`：接受 `QPixmap`、`QImage` 或 `QIcon`；不接受路径或 URL，控件不触发 I/O。
- `Qt::AccessibleTextRole` 与 `Qt::AccessibleDescriptionRole`：交给 Qt item-view 无障碍实现；缺失时 DisplayRole 仍作为名称回退。
- `Qt::EnabledRole` 不存在；是否可交互只看 `Qt::ItemIsEnabled` flag。

### 5.2 当前项真值

- `QItemSelectionModel::currentIndex()` 是唯一当前项真值，private 不保存第二个 current row。
- 非空 model 首次安装后选择 root 下第 0 行第 0 列；空 model 保持无效索引和 `currentRow == -1`。
- `setCurrentRow()` 对越界行 no-op；合法同值不发信号。model insert/remove/reset 后比较派生 row，只在值实际变化时发一次 `currentRowChanged`。
- `setModel()` 不取得 model 所有权，断开旧 model 观察连接，并让基类重建 selection model。

### 5.3 导航

- `showPrevious()`/`showNext()` 只移动一个逻辑 row。边界下 wrap 关闭则 no-op，开启则在首尾间环绕。
- Left/Right 根据 layout direction 映射视觉方向；Home/End 跳到首尾，PageUp/PageDown 与前后项一致。
- wheel 优先使用 `pixelDelta()`，否则使用 `angleDelta()`；只有形成非零方向且成功切换时 accept，空 model 或边界 no-op 时 ignore，允许父滚动区域继续处理。
- 箭头按钮始终是两个持久 `QToolButton`，只用 Qt 标准 arrow icon，无可见文字；tooltip 和 accessible name 在 LanguageChange 时更新。
- disabled 当前项仍可展示；用户导航到直接相邻 disabled 项时 no-op，不扫描整个 model，避免全 disabled 大模型产生 O(N) 输入延迟。

## 6. 绘制与动效

### 6.1 固定复杂度绘制

- 普通帧只调用一次 delegate paint；过渡帧只调用前一项和当前项两次 delegate paint。
- 默认 delegate 使用 viewport 内容矩形，不创建 child widget、临时 pixmap 缩放副本或 QObject。
- DecorationRole 图片按 `KeepAspectRatioByExpanding` 计算源裁剪矩形并直接绘制；空图片使用标准 icon 占位。
- 标题和说明从 palette 取色，focus/selected/disabled 使用 `QStyleOptionViewItem` state；主题切换不重建 delegate。
- 底部指示器最多七个点。大模型时以当前 row 为中心选择有界窗口，不遍历所有 row。

### 6.2 单动画状态机

- private 构造唯一 `QVariantAnimation`，parent 为 view；时长默认 220 ms，setter 收敛到 0-1000 ms。
- currentChanged 先让 selection model 成为真值，再保存前一 `QPersistentModelIndex`、方向和 progress=0。
- 新导航发生时停止旧动画，并从当前呈现位置收敛到最新目标；不得排队创建 animation。
- `SH_Widget_Animate == 0`、view 不可见、view disabled 或 duration==0 时直接 progress=1。
- hide、model reset、旧 persistent index 失效时停止动画并只画当前项。

### 6.3 几何与 RTL

- viewport 内容区为逻辑坐标，箭头按钮各占固定 32x32，标题区域与指示器预留由 style metric 推导的边距。
- resize 只移动两个按钮并刷新当前 item visual rect，不遍历 model。
- RTL 翻转左右按钮的位置、图标和视觉移动方向，但 row 的前后含义保持 row-1/row+1。
- 固定格式 UI 不使用 viewport 宽度缩放字体；delegate 只使用 widget font 和 font metrics elide/wrap。

## 7. 测试计划

新增 `ZzFluentUI/tests/ZzCarouselViewTest.cpp`，至少覆盖：

- 空 model、单项、两项、十万项；安装/替换/清空 model 的 current row 和所有权。
- insert/remove/move/reset、root index、当前项前插入/删除、删除当前项和 signal 次数。
- 标准 role 的 QString/QPixmap/QImage/QIcon、空图片、长标题、说明缺失、disabled flag 和自定义 delegate。
- `setCurrentRow()` 越界/同值、previous/next、wrap、Home/End、PageUp/PageDown、LTR/RTL Left/Right。
- angleDelta/pixelDelta wheel；边界 no-op 必须 ignore，成功移动必须 accept。
- 两个箭头按钮的 enabled、tooltip、accessible name、LanguageChange 和点击单次切换。
- click、double click、Enter 的 QAbstractItemView `clicked`/`doubleClicked`/`activated` 标准信号不被重复发射。
- `visualRect()`、`indexAt()`、selection region 和 item delegate option 的 selected/focus/disabled state。
- Light/Dark/HighContrast、enabled/disabled、focus、RTL、reduced motion 与动画中途改目标。
- QAccessible 接口非空，view 具备 item-view role，当前 child 名称来自 AccessibleTextRole/DisplayRole。
- 1000 次切换、model reset 和主题变化后 QObject、animation、timer 数量不增长。

安装消费者在 `tests/InstallConsumer/Gui/main.cpp` 只包含安装后的 `ZzCarouselView.h`，用 `QStandardItemModel` 渲染一项并验证 current row、标准 role、公开 signal 和非空画面，不访问 private header。

## 8. 性能门禁

扩展 `ZzBasicControlsBenchmark`：

- 使用轻量只读 model 暴露 100000 行，共享一张预构造 pixmap；构建 view 和首次显示不得创建逐项 QObject。
- 连续执行 5000 次相邻/环绕 current row 更新，统计 model `data()` 调用；每次切换访问必须有固定上界，不得随 100000 行线性增长。
- 预构造 40 个可见 Carousel，预热后切换并离屏绘制 120 帧；在 reference 发布机记录 P50/P95/max，P95 初始硬门限为 `12 ms`。
- 对比 20 行与 100000 行 model 的重复绘制耗时，比例不得超过 `2.0`。
- 1000 次导航、resize、主题切换、model reset 后 descendants、animation、timer 不增长；每个 view 只允许一个过渡 animation 和固定数量内部控件。

如果本机 reference 首次测量证明 `12 ms` 与既有绘制成本不匹配，只能基于原始数据、60 Hz 预算和同环境重复结果调整一次门限，并在性能提交正文和最终交付记录中说明，不能为绕过回归随意放宽。

## 9. 示例与视觉基线

- 控件画廊新增“轮播视图”区域，使用本地 `QStandardItemModel` 和预构造 pixmap 展示三项；示例只连接当前 row 文本，不创建自动播放 timer 或业务服务。
- 截图 surface 覆盖图片、空图片、长标题、说明、disabled、focus、首尾边界、RTL 和位置指示器。
- 生成 `carousel-view-{light,dark,high-contrast}.png`，覆盖 DPR 1.0、1.25、1.5、2.0 共 12 张。
- 人工检查至少三主题 DPR 1.0 和 Light DPR 2.0，确认图片裁剪、文字 elide、按钮、焦点、指示器、RTL、disabled 和空态没有裁切、重叠或缩放异常。

## 10. CMake、安装与架构

- 四个实现文件加入 `zz_fluent_ui_sources`，公开头加入 `zz_fluent_ui_moc_headers`，保持 CMakeLists.txt 与 CMakePresets.json 配合构建。
- 新测试 target 使用 Qt6::Test、Qt6::Widgets 和 `Zz::FluentUI`，接入 warnings、sanitizer 和 CTest label。
- `CheckZzFluentUIBoundaries.cmake` 继续禁止 ZzPureTools/ZzWindowKit/QWK、Qt Private API、链式命名空间和业务层关键词。
- 新增生产代码只使用 Qt Core/Gui/Widgets 公共 API、标准 C++20 和组件 private header。
- Windows MSVC、Windows Qt SDK MinGW 与 macOS 只做源码、preset、公开 ABI、依赖和条件编译静态审计；没有对应工具链证据前不得记录为原生编译或真机通过。

## 11. 验证矩阵

每个代码提交运行对应 target 与定向 CTest；最终运行：

- GCC 15 shared/static Release 全量构建和 CTest。
- Clang 20 ASan+UBSan 全量构建和 CTest。
- shared/static `ZzClangTidy` 完整一方翻译单元。
- fresh install consumer、package relocation、公开头、完整架构、FluentUI 边界和二进制依赖。
- 控件画廊 shared/static offscreen smoke。
- DPR 1.0/1.25/1.5/2.0 完整截图回归和人工抽检。
- reference Release benchmark 与 sanitizer 定向 benchmark。
- preset matrix contract、gate script contract 和本批平台宏/原生头/Qt Private/绝对路径扫描。

## 12. 提交顺序

```text
文档：规划高性能Fluent轮播视图批次
控件：实现高性能Fluent轮播视图
测试：接入轮播视图质量与安装消费
性能：锁定轮播视图模型与绘制预算
测试：补齐轮播视图多主题视觉基线
文档：记录轮播视图批次交付结果
```

提交标题使用中文简述，正文使用多个中文段落记录实现、验证、性能和平台影响。每个逻辑批次验证后立即提交；不 push，不调用 GitHub CLI，不处理远端 CI，不下载 Qt。
