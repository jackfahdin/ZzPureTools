# ZzFluentUI 第 2 批导航与容器扩展实施计划

**目标：** 在不改变既有 Navigation/List/Table/Tree 终态视觉的前提下，为导航与树形选中指示条加入有界动画，并新增 `ZzExpander`、`ZzPivot`、`ZzDrawer` 三个公开组件；同时完成 WinUI TabView 与现有 `ZzTabBar`/`ZzTabWidget` 的代码级差异评估。

**实施基线：** 本计划基于提交 `dff1459` 后的源码审计。第 0、1 批已经完成，当前公开 Fluent 组件数为 29。实现顺序、验收命令和提交边界以本文为准；总路线 `2026-08-10-zzfluentui-expansion-master-plan.md` 只保留批次级目标。

**平台边界：** 生产实现只能使用 Qt 6.8+ Widgets/Core/Gui 公共 API 和标准 C++20。Linux 参考机执行完整构建、测试、截图、Clang-Tidy、Sanitizer、安装消费和性能门禁；Windows MSVC、Windows Qt SDK MinGW、macOS arm64/x86_64 本批只做源码、预设、条件编译和依赖方向静态审计，未实际执行时不得写成已验证。

---

## 1. 已审计事实与本批定论

### 1.1 现有实现

- `ZzNavigationPane` 固定拥有 primary/footer 两个 `ZzNavigationView`，只投影非拥有的 `QAbstractItemModel`，不创建页面和不执行路由。自适应宽度切换当前是同步跳变，本批不为 pane 宽度增加动画。
- `ZzNavigationView` 的私有 delegate 最终通过 `ZzItemViewVisual::draw()` 绘制 3x16 逻辑像素强调条；普通项可能回退到 `ZzFluentItemDelegate`。动画状态因此必须归属于 view，而不能只放在某一条 delegate 绘制分支。
- `ZzFluentItemDelegatePrivate` 只为 Tree 观察 selection model，现状在选择变化后刷新完整 viewport。List/Table 保持静态强调条，本批只为 Tree 增加过渡。
- `ZzTabBar`/`ZzTabWidget` 已实现同容器重排、进程内跨容器转移、撕离意图、完整元数据恢复、伪造 MIME 拒绝、RTL/垂直插入槽位、原生 Ctrl+Tab 和 Qt 可访问性角色。
- `ZzContentDialogPrivate` 的 overlay 是对话框实例专用对象，包含宿主事件过滤、层级和焦点恢复职责。Drawer 不依赖该私有类，只复用 `ZzColorToken::OverlayScrim` 与 `ZzFluentPainter::drawOverlayScrim()`。
- `ZzWidgetTheme` 已提供 Fluent style 快照和非 Fluent style 回退快照。三个新组件都复用它，不缓存第二份颜色、字体或动效配置。

### 1.2 旧版参考结论

- 旧版 `ZzPivot` 的 model/view/style 三层、每实例 `QProxyStyle`、`setStyleSheet` 和手势滚动配置不迁移。新 Pivot 直接复用 `QTabBar` 的索引、键盘、滚动按钮和可访问性语义。
- 旧版 `ZzDrawerArea` 实际是常驻折叠区域，不是 WinUI 临时边缘 Drawer。本批的 `ZzDrawer` 是覆盖宿主内容的临时面板；常驻折叠内容由 `ZzExpander` 表达，常驻导航继续由 `ZzNavigationPane` 表达。
- 禁止迁移旧版“每次交互创建动画”“`DeleteWhenStopped`”“动画回调捕获无守护裸指针”“用 stylesheet 清透明背景”等实现。

### 1.3 决策

| 决策点 | 定论 | 理由 |
|---|---|---|
| 指示条动画归属 | Navigation 每个 view 一个；Tree 每个 delegate 一个 | 与可见 item 数量无关，普通/特殊导航 delegate 共用同一状态 |
| Expander 内容所有权 | `setContentWidget()` 接管，`takeContentWidget()` 交回 | 与 ContentDialog/TeachingTip 一致；避免内容在动画期间被外部删除造成悬空引用 |
| Pivot 基类 | `final : QTabBar` | 保留方向键、助记键、滚动、RTL 和 `PageTabList/PageTab` 角色，不复制 model/view/style |
| Drawer 宿主 | 有 parent 的普通 child widget，覆盖 `parentWidget()->rect()` | 不创建平台顶层窗口，不引入无边框或窗口激活差异 |
| Drawer 非模态输入 | 非模态时设置为当前 panel 区域 mask | 面板外鼠标事件继续到宿主；不能用透明全屏 child 阻断输入 |
| TabView | 不新增同义控件，暂不修改生产 API | 现有实现已覆盖有价值的文档标签能力；新增按钮可由 `setCornerWidget()` 组合，关闭继续是 intent-only |
| 新主题令牌 | 增加选择指示条厚度/长度和 Drawer 默认宽度 metric | 新源码不得引入关键尺寸魔数，也不扩大架构白名单 |

---

## 2. 共用动画状态机

### 2.1 新增私有类型

新建：

- `ZzFluentUI/widgets/src/private/ZzSelectionIndicatorTransition.h`
- `ZzFluentUI/widgets/src/private/ZzSelectionIndicatorTransition.cpp`

`ZzSelectionIndicatorTransition` 不是 QObject，不带 `Q_OBJECT`，内部只保存：

```cpp
QVariantAnimation *animation_ = nullptr; // 构造时创建，QObject parent 为所属 view/delegate
QPersistentModelIndex outgoingIndex_;
QPersistentModelIndex incomingIndex_;
qreal outgoingStartScale_ = 0.0;
qreal incomingStartScale_ = 0.0;
qreal outgoingScale_ = 0.0;
qreal incomingScale_ = 1.0;
```

公开范围仅限 private 源码，提供 `transitionTo(index, duration)`、`scaleFor(index, staticallySelected)`、`forcesIndicator(index)`、`outgoingIndex()`、`incomingIndex()` 和 `animation()`。它不读取 view/model 总行数，不保存容器，不创建 timer，不在 valueChanged 热路径分配集合。

### 2.2 两段式公式

动画归一化进度 `p` 范围为 0 到 1：

```text
0.0 <= p <= 0.5:
    outgoing = outgoingStart * (1 - 2p)
    incoming = incomingStart

0.5 < p <= 1.0:
    outgoing = 0
    incoming = incomingStart + (1 - incomingStart) * (2p - 1)
```

使用 `QEasingCurve::InOutSine`。旧条先缩短，新条再长出；矩形中心不移动，Navigation/Tree 沿高度缩放。动画结束后只保留 incoming 终态，清空 outgoing persistent index。

快速连续切换时，在 stop 前读取当前两个 scale：新目标若正是当前 outgoing/incoming，使用其当前 scale 作为 `incomingStart`；其余可见条中 scale 最大者作为 outgoing。不得先恢复到 0 或 1，不得增加动画对象。目标模型被 reset/删除后 persistent index 自动失效，下一次选择直接建立终态。

### 2.3 动效策略

每次过渡从当前 `ZzThemeSnapshot` 读取：

```cpp
const int duration = ZzAnimationPolicy::adjustedDuration(
    snapshot->duration(ZzMotionToken::Normal),
    snapshot->reducedMotion(),
    false);
```

`duration == 0` 时 stop 并同步进入新终态，不启动 event loop 动画。主题在运行中切换到 reduced motion 时，下一次 theme/style change 必须立即完成当前过渡。动画对象仍然固定存在，因此对象预算断言是“每个 owner 恰好一个且地址不变”，不是运行时反复创建/删除。

---

## 3. 任务一：导航与 Tree 指示条动画

### 3.1 文件修改

- 修改 `ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h`
- 修改 `ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp`
- 修改 `ZzFluentUI/tests/ZzThemeSnapshotTest.cpp`
- 修改 `ZzFluentUI/widgets/src/private/ZzItemViewVisual.h/.cpp`
- 修改 `ZzFluentUI/widgets/include/ZzFluentUI/ZzNavigationView.h`
- 修改 `ZzFluentUI/widgets/src/ZzNavigationView.cpp`
- 修改 `ZzFluentUI/widgets/src/private/ZzNavigationViewPrivate.h/.cpp`
- 修改 `ZzFluentUI/widgets/src/private/ZzFluentItemDelegatePrivate.h/.cpp`
- 修改 `ZzFluentUI/CMakeLists.txt`
- 修改 `ZzFluentUI/tests/ZzNavigationControlsTest.cpp`
- 修改 `ZzFluentUI/tests/ZzNavigationPaneTest.cpp`
- 修改 `ZzFluentUI/tests/ZzFluentItemDelegateTest.cpp`

新增 `SelectionIndicatorThickness=3`、`SelectionIndicatorExtent=16`、`DrawerDefaultWidth=320` 三个 metric。四档 DPR 物理对齐继续由 painter/Qt 完成，不用 DPR 乘逻辑尺寸。

### 3.2 绘制接口

扩展 private `ZzItemViewVisualOptions`：

```cpp
bool drawSurface = true;
bool ownsIndicator = true;
bool forceIndicator = false;
qreal indicatorScale = 1.0;
```

`draw()` 仍返回同一 `contentRect`，动画绝不改变文字、图标和 badge 的安全区域。强调条最终矩形由 metric token 计算；实际绘制矩形以静态 rect 中心为锚点，只缩放高度。只有以下条件同时成立才画条：owner 为 true、scale 大于 0、并且 item 静态选中或 `forceIndicator`。

### 3.3 Navigation 绑定

`ZzNavigationView` 公开覆盖 `setModel(QAbstractItemModel *)`，先调用 `QListView::setModel()`，再让 private 迁移 selection model 连接。它不接管 model 所有权。

`ZzNavigationViewPrivate` 新增 transition、selection connection 和以下职责：

- `bindSelectionModel()`：断开旧连接，连接新 `selectionChanged` 和 `modelReset`，从 `selectedIndexes()` 选择首个有效、启用、非 section header 的 column 0 index。
- `indicatorScale()`/`forcesIndicator()`：供私有导航 delegate 和 `ZzFluentItemDelegatePrivate` 的导航 fallback 读取同一 transition。
- valueChanged 每帧只对 outgoing/incoming 的 `visualRect()` 调 `viewport()->update(rect)`；index 不可见时跳过。
- 选择同步发生在 primary/footer 两个投影视图时，各自只动画自己的本地 proxy index；`ZzNavigationPane` 的 source index 契约不变。

为此，`ZzNavigationView` 只向 `ZzNavigationViewPrivate`、`ZzNavigationItemDelegate`、`ZzFluentItemDelegatePrivate` 声明 friend，不向公开 API 暴露动画进度。

### 3.4 Tree 绑定

`ZzFluentItemDelegatePrivate::observeTreeSelection()` 保留延迟迁移 selection model 的能力，但把“完整 viewport update”替换为 transition：

- 目标索引取 `selectedRows(treePosition())` 的第一个有效 index。
- transition 由 delegate 构造时创建一个 `QVariantAnimation`，QObject parent 为公开 delegate。
- 每帧只刷新 outgoing/incoming 所在整行的 viewport 矩形；跨列 surface 仍由既有 `PE_PanelItemViewRow` 绘制，强调条只在 treePosition 列绘制。
- List/Table 不调用该 transition，终态视觉与对象数保持不变。

### 3.5 测试

新增/调整断言：

1. Navigation 首次选择直接终态，第二次选择观察到“旧条缩短后新条增长”的两个阶段，finished 后 accent 像素几何与现有静态基线相同。
2. reduced motion 下选择立即终态，`animation()->state()` 为 Stopped。
3. 连续切换 1000 次，两个 pane view 各只有一个 `QVariantAnimation`，地址、QObject/timer 数量不变。
4. 普通无 icon/badge 的 fallback 行和带 descriptor/badge 行共享动画，不出现两个同时完整高度的强调条。
5. Tree 快速 A -> B -> A 时 scale 连续，不先跳到完整高度；selection model 替换后连接迁移。
6. RTL 下强调条仍位于逻辑 leading 侧；Navigation/List/Table/Tree 内容安全区域既有断言全部保留。
7. `tracksAdaptiveTopLevelWidthWithoutAnimation()` 继续断言 pane 宽度同步变化；只把“所有 animation 数为 0”改成固定选中动画预算。

验证：

```bash
cmake --build --preset linux-gcc-reference --target \
  ZzThemeSnapshotTest ZzNavigationControlsTest ZzNavigationPaneTest \
  ZzFluentItemDelegateTest
ctest --preset linux-gcc-reference -R \
  'fluent\.(navigation|navigation-pane|item-delegate)|foundation\.theme-snapshot' \
  --output-on-failure
```

提交标题：`动效：统一导航与树形选中指示条过渡`

---

## 4. 任务二：ZzExpander

### 4.1 新增文件与公共 API

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzExpander.h`
- `ZzFluentUI/widgets/src/ZzExpander.cpp`
- `ZzFluentUI/widgets/src/private/ZzExpanderPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzExpanderPrivate.cpp`
- `ZzFluentUI/tests/ZzExpanderTest.cpp`

公共类：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzExpander final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzExpander)
    Q_PROPERTY(QString headerText READ headerText WRITE setHeaderText
               NOTIFY headerTextChanged)
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded
               NOTIFY expandedChanged)
    Q_PROPERTY(QWidget *contentWidget READ contentWidget WRITE setContentWidget
               NOTIFY contentWidgetChanged)
public:
    explicit ZzExpander(QWidget *parent = nullptr);
    ~ZzExpander() override;
    [[nodiscard]] QString headerText() const;
    void setHeaderText(QString text);
    [[nodiscard]] bool isExpanded() const noexcept;
    void setExpanded(bool expanded);
    [[nodiscard]] QWidget *contentWidget() const noexcept;
    void setContentWidget(QWidget *widget);
    [[nodiscard]] QWidget *takeContentWidget();
Q_SIGNALS:
    void headerTextChanged(const QString &text);
    void expandedChanged(bool expanded);
    void contentWidgetChanged(QWidget *widget);
};
```

头文件 Doxygen 明确：header 点击、Space、Enter 切换的是纯 UI 展开状态；组件接管 content；替换会删除旧 content；业务状态和数据加载由调用方管理。

### 4.2 私有布局与动画

固定子对象：一个 `QToolButton`（objectName `zzExpanderHeaderButton`）、一个 content host、一个零边距 content layout、一个 `QVariantAnimation`。Header 使用 `ToolButtonTextBesideIcon`、逻辑 leading 文本、`Qt::RightArrow/DownArrow`，不创建图标 pixmap和 stylesheet。

动画只驱动 content host 的 `maximumHeight`：

- 展开：先 show host，从当前可见高度到 content layout 的有效 sizeHint；finished 后设为 `QWIDGETSIZE_MAX`，允许内容继续自适应。
- 收起：从当前高度到 0；finished 后 hide host。
- 中途反向：stop 前读取当前 `maximumHeight` 作为新起点，动画对象不变。
- content 替换、LayoutRequest、字体/style/palette 变化时重新计算目标；运行中只重定向目标，不重建 layout。
- duration 使用 Normal + AnimationPolicy；0 时一次同步到终态。

`LanguageChange` 刷新 header 的“展开内容/折叠内容” accessibleDescription；headerText 是调用方文本，不擅自翻译。焦点始终留在 header，折叠时若焦点位于 content 子树，先把焦点移回 header，避免隐藏焦点对象。

### 4.3 接线与验收

修改 `ZzFluentUI/CMakeLists.txt`、`ZzFluentUI/tests/CMakeLists.txt`、Gallery、Fluent screenshot、安装 consumer 和 README 控件清单。

测试覆盖：属性幂等、setter/take/替换/外部销毁所有权、鼠标和 Space/Enter、折叠焦点恢复、sizeHint 终态、动画反向、reduced motion、LanguageChange、RTL 箭头/文本方向、1000 次切换后一个 animation 且 QObject/timer 数不增长。

截图覆盖 Light/Dark/HighContrast 的展开与折叠终态；截图前启用 reduced motion，禁止依赖等待固定毫秒碰终态。

提交标题：`控件：新增 Fluent 折叠展开容器`

---

## 5. 任务三：ZzPivot

### 5.1 新增文件与公共 API

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzPivot.h`
- `ZzFluentUI/widgets/src/ZzPivot.cpp`
- `ZzFluentUI/widgets/src/private/ZzPivotPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzPivotPrivate.cpp`
- `ZzFluentUI/tests/ZzPivotTest.cpp`

公共类直接继承 `QTabBar`：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzPivot final : public QTabBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPivot)
public:
    explicit ZzPivot(QWidget *parent = nullptr);
    ~ZzPivot() override;
    int addItem(const QString &text);
    int insertItem(int index, const QString &text);
    void removeItem(int index);
    [[nodiscard]] QString itemText(int index) const;
    void setItemText(int index, const QString &text);
Q_SIGNALS:
    void itemCountChanged(int count);
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
private:
    std::unique_ptr<ZzPivotPrivate> d_ptr;
};
```

`currentIndex`、`count`、`currentChanged(int)`、方向键、滚动按钮和 accessible PageTab 语义沿用 `QTabBar`，不重复声明同名属性。头文件明确 Pivot 是不拥有页面的页面级导航，`ZzTabWidget` 是拥有页面且支持 close/transfer/tear-off 的文档容器。

### 5.2 绘制与几何

构造固定配置：不可移动、不可关闭、不扩展、顶部圆角 shape、启用 scroll buttons、ElideRight、drawBase false。禁止暴露或安装实例 `QProxyStyle`。

`paintEvent()` 不调用 `QTabBar::paintEvent()`，以避免标准 document tab 背板；逐个可见 `tabRect()` 构造 `QStyleOptionTab`，只委托 `CE_TabBarTabLabel` 绘制文字/助记键，不手绘文本。Hover/pressed surface 使用 theme token，focus 使用 `ZzFluentPainter::drawFocusRing()`。选中横条使用 Accent、`SelectionIndicatorThickness` 和与 label 宽度一致的终态 rect。

Private 固定持有一个 `QVariantAnimation` 和 `QRectF currentIndicatorRect`：

- currentChanged 时从当前正在显示的 rect 插值到新 tab 终态 rect；快速切换从当前 rect 重定向。
- 使用 Normal + AnimationPolicy 和 InOutSine；duration 0 直接终态。
- resize、font/style/palette、LayoutDirectionChange、tab 插入/删除时停止并重算当前终态，避免对已失效索引插值。
- RTL 直接信任 `tabRect()` 和 `QStyleOptionTab::direction`，不手工反转索引。

### 5.3 接线与验收

接入 CMake、独立 QTest、Gallery 导航列、Fluent screenshot、安装 consumer 和 README。

测试覆盖：add/insert/remove 与 count、文本幂等、单击一次切换、Left/Right/Home/End、助记键、RTL、超宽内容滚动按钮、原生可访问性角色/名称、动画终态/快速重定向/reduced motion、1000 次 currentIndex 切换后的固定对象预算。像素断言确认没有标准选中背板且 Accent 横条不覆盖文字。

提交标题：`控件：新增轻量 Fluent 页面枢轴`

---

## 6. 任务四：ZzDrawer

### 6.1 新增文件与枚举

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzDrawerEdge.h`
- `ZzFluentUI/widgets/include/ZzFluentUI/ZzDrawer.h`
- `ZzFluentUI/widgets/src/ZzDrawer.cpp`
- `ZzFluentUI/widgets/src/private/ZzDrawerPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzDrawerPrivate.cpp`
- `ZzFluentUI/tests/ZzDrawerTest.cpp`

枚举只包含物理边缘：

```cpp
enum class ZzDrawerEdge : std::uint8_t { Left, Right };
Q_DECLARE_METATYPE(ZzFluentUI::ZzDrawerEdge)
```

### 6.2 公共 API

```cpp
class ZZ_FLUENT_UI_EXPORT ZzDrawer final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzDrawer)
    Q_PROPERTY(ZzFluentUI::ZzDrawerEdge edge READ edge WRITE setEdge
               NOTIFY edgeChanged)
    Q_PROPERTY(bool modal READ isModal WRITE setModal NOTIFY modalChanged)
    Q_PROPERTY(int widthHint READ widthHint WRITE setWidthHint
               NOTIFY widthHintChanged)
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QWidget *contentWidget READ contentWidget WRITE setContentWidget
               NOTIFY contentWidgetChanged)
public:
    explicit ZzDrawer(QWidget *parent = nullptr);
    ~ZzDrawer() override;
    [[nodiscard]] ZzDrawerEdge edge() const noexcept;
    void setEdge(ZzDrawerEdge edge);
    [[nodiscard]] bool isModal() const noexcept;
    void setModal(bool modal);
    [[nodiscard]] int widthHint() const noexcept;
    void setWidthHint(int logicalWidth);
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] QWidget *contentWidget() const noexcept;
    void setContentWidget(QWidget *widget);
    [[nodiscard]] QWidget *takeContentWidget();
public Q_SLOTS:
    void openDrawer();
    void closeDrawer();
Q_SIGNALS:
    void edgeChanged(ZzDrawerEdge edge);
    void modalChanged(bool modal);
    void widthHintChanged(int logicalWidth);
    void openChanged(bool open);
    void contentWidgetChanged(QWidget *widget);
};
```

不声明 `close()`，避免隐藏 `QWidget::close()`。`widthHint=0` 表示使用 `DrawerDefaultWidth`；正值收敛到 1..4096，实际 panel 宽度再限制到 host 宽度。

### 6.3 私有状态、绘制和输入

固定子对象：一个 panel host、一个零边距 content layout、一个 `QVariantAnimation`。Drawer 自身覆盖 parent content rect；panel host 只承载内容且背景透明，surface 由 Drawer paint 一次绘制。

动画 progress 范围 0..1，panel rect 公式：

```text
Left:  x = -panelWidth + progress * panelWidth
Right: x = hostWidth - progress * panelWidth
```

快速 open/close 从当前 progress 反向，duration 按剩余距离缩放且至少为 1ms；reduced motion 直接终态。关闭 finished 后 hide Drawer，打开时先同步 host geometry、show/raise、再把焦点移到第一个可聚焦 content child；关闭保存并恢复打开前的 `QPointer<QWidget>` 焦点。

paint 顺序：modal 且 progress>0 时先以 painter opacity=progress 调 `drawOverlayScrim()`；再调 `drawPopupSurface(panelRect)`。不缓存全屏 pixmap，不 render 宿主。

输入语义：

- modal=true：Drawer 接收全宿主输入；左键按下在 panel 外调用 `closeDrawer()`；Escape 关闭；Tab 焦点保持在可见 panel 子树，若无可聚焦内容则 Drawer 自身可聚焦。
- modal=false：每次 progress/geometry 变化把 widget mask 更新为 panel rect，面板外输入到宿主；不画 scrim；Escape 在 panel 焦点链内仍关闭。
- parent resize/move/show/style/palette 通过固定 parent event filter 同步；ParentChange 时迁移连接和 filter；析构/parent 销毁不触发晚回调。
- Edge 是物理 Left/Right，不随 RTL 互换；content 自身继承布局方向。

### 6.4 接线与验收

接入 CMake、独立 QTest、Gallery 导航列、独立 Light/Dark/HighContrast screenshot、安装 consumer 和 README。Gallery 同时展示 modal 左 Drawer、non-modal 右 Drawer，不从组件内部加载页面。

测试覆盖：属性/信号幂等、内容所有权、无 parent 安全 no-op、左右几何、host resize、modal scrim 像素、非模态 mask/底层点击、外部点击和 Escape、焦点进入/恢复、动画反向/reduced motion、隐藏/销毁中途、1000 次开关后固定一个 animation/event filter/QObject/timer 预算。

提交标题：`控件：新增 Fluent 边缘抽屉`

---

## 7. 任务五：TabView 评估结论

| WinUI TabView 能力 | 当前实现 | 本批处理 |
|---|---|---|
| 选中/键盘/Ctrl+Tab | `QTabWidget/QTabBar` 原生 | 保留 |
| 关闭 intent | `tabCloseRequested`，页面不自动删除 | 保留 |
| 拖动重排 | `setMovable(true)` | 保留 |
| 跨窗口/容器转移 | 安全 MIME + `transferTabTo()` | 保留 |
| 撕离新窗口 | `tearOffRequested`，宿主创建窗口 | 保留前后端边界 |
| 新建标签按钮 | `QTabWidget::setCornerWidget()` 可组合 | Gallery 增加组合示例，不加容器 API |
| 中键关闭 | 平台/应用策略差异大 | 不默认接管；应用可在派生宿主或 event filter 发 intent |
| 页面所有权 | Qt parent + 可回滚转移 | 保留 |

结论：不新增 `ZzTabView`，也不向 `ZzTabBar` 增加与 `QTabBar` 重复的 property。只在 Gallery 用带 accessibleName 的 corner `ZzIconButton` 证明新建 intent 可组合，并在批次收尾文档记录决定。该项不单独产生生产源码提交。

---

## 8. 集成、截图与门禁

### 8.1 Gallery 与截图

`examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp`：

- Navigation column 增加 Expander、Pivot、Drawer 入口；示例只用本地 label/page 和信号更新展示状态。
- 现有 Tab 区增加 corner 新建按钮，点击只发/展示 intent，不在组件内部创建业务页面。
- 不增加功能说明卡片，不嵌套 card，不加入网络/文件/数据库访问。

`ZzFluentScreenshotTest.cpp`：

- 通用 controls 场景纳入 Expander 展开/折叠和 Pivot。
- Drawer 使用独立固定宿主场景，覆盖左右、modal/non-modal 的终态。
- 每个场景 Light/Dark/HighContrast x DPR 100/125/150/200；更新前先跑旧基线确认差异只来自新增区域。
- Navigation/Tree 现有截图必须在动画 stopped 终态与旧基线逐像素一致，不以重采掩盖意外视觉改变。

### 8.2 安装消费与架构

`tests/InstallConsumer/Gui/main.cpp` 从安装前缀 include 并构造三个新类，覆盖：Expander setter/take、Pivot add/current、Drawer content/open/close。shared/static consumer 都必须链接通过。

架构扫描要求：

- 新源码零 `setStyleSheet`、零裸 QColor/ARGB、零关键尺寸字面量白名单增量。
- public header 不 include private header；private 不进入 install。
- 不出现链式 namespace、平台原生 header、Qt private header、业务模块 include。
- paint 路径不创建 QObject、pixmap、timer、animation、event filter。

### 8.3 Linux 全量命令

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr

cmake --preset linux-gcc-reference
cmake --build --preset linux-gcc-reference --parallel 2
ctest --preset linux-gcc-reference --output-on-failure

cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug --parallel 2
cmake --build --preset linux-clang-debug --target ZzClangTidy
ctest --preset linux-clang-debug --output-on-failure

cmake --preset linux-clang-static
cmake --build --preset linux-clang-static --parallel 2
cmake --build --preset linux-clang-static --target ZzClangTidy
ctest --preset linux-clang-static --output-on-failure

scripts/ci/run-linux-gates.sh
```

Sanitizer 必须覆盖动画运行中销毁 Expander/Drawer、替换 content、Tree selection model 替换和模型 reset。性能门禁继续使用固定 Xvfb CPU 8、CTest CPU 10 和已有逐指标阈值；不得因新控件调整阈值或改 observe 来换绿灯。

### 8.4 文档收尾

修改：

- `README.md`：公开组件清单 29 -> 32，加入三个新组件。
- `docs/development/CODING_STANDARD_ZH.md`：补充“每实例固定动画对象、连续重定向、reduced motion 同步终态”规则。
- 总路线：把第 2 批标记完成并记录 TabView 不新增的决定。
- 本文末尾追加真实提交、测试数量、截图数量、性能结果和未执行平台；计划阶段不得预填通过。

---

## 9. 提交顺序与退出条件

1. `文档：细化 FluentUI 第二批导航扩展路线`
2. `动效：统一导航与树形选中指示条过渡`
3. `控件：新增 Fluent 折叠展开容器`
4. `控件：新增轻量 Fluent 页面枢轴`
5. `控件：新增 Fluent 边缘抽屉`
6. 必要时独立静态分析修复提交，不夹带功能变化。
7. `文档：完成第二批组件验收与路线状态同步`

每个功能提交必须同时包含自己的单元测试、CMake 接线、Gallery/安装消费改动和受影响截图；修改验证后立即提交，不把三个控件压成一个大提交。`temp_image/` 始终不跟踪、不提交。

本批只有在以下条件全部满足后关闭：

- 三个公开组件 API、Pimpl、中文 Doxygen、所有权和 intent 边界完成。
- Navigation/Tree 动画对象数量固定，快速切换连续，reduced motion 同步终态。
- 既有 Navigation/List/Table/Tree 终态视觉不变；新增组件四档 DPR 三主题基线通过。
- Linux reference、Clang shared/static、Clang-Tidy、ASan/UBSan、安装消费、架构和性能门禁全部通过。
- Windows/MSVC、MinGW、macOS 的真实验证状态如实记录。

---

## 10. 实施结果（完成后填写）

- **提交：** `f96f044` 固化本详细计划；`4c128dc` 完成 Navigation/Tree 指示条过渡；`7734291`、`e7d929f`、`47309c9` 分别完成 Expander、Pivot、Drawer；`633b44a` 在 Gallery 通过 corner widget 组合标签新建意图。本次文档提交同步编码规范与总路线状态。
- **Linux 功能门禁：** `scripts/ci/run-linux-gates.sh` 退出码为 0。`linux-gcc-debug`、`linux-clang-tidy-release`、`linux-clang-tidy-static`、`linux-clang-asan`、`linux-gcc-release`、`linux-static-release`、`linux-gcc-release-lto`、`linux-static-release-lto` 均为 125/125；功能、架构、安装消费与 shared/static 组合均通过。
- **截图：** 通用 controls 场景更新 12 张 Light/Dark/HighContrast x DPR 100/125/150/200 基线，覆盖 Expander 展开/折叠和 Pivot；Drawer 新增同一主题/DPR 矩阵的 12 张独立基线。共 24 张基线进入关闭更新模式后的自动比较，Navigation/Tree 仍以动画终态采集。
- **Clang-Tidy：** shared/static 各扫描 207 个一方翻译单元，无有效诊断；本批没有扩大视觉令牌架构白名单。
- **ASan/UBSan：** `linux-clang-asan` 125/125，并在性能压力阶段额外通过 2/2 个 Sanitizer 用例；覆盖动画运行中销毁、内容替换和模型/selection model 生命周期路径。
- **性能：** `linux-gcc-benchmarks` 147/147，12 个性能场景的逐指标相对比较全部通过，未调整阈值、参考基线或 `gate`/`observe` 策略。另行执行 `linux-gcc-reference -L benchmark` 时首轮 37/38，唯一失败是 startup 首次窗口未在时限内 exposed；同环境单独复跑 `benchmark.startup` 通过，正式 benchmark 门禁中的 startup 也一次通过，因此记录为一次窗口时序波动而不修改代码或阈值。
- **平台状态：** Windows MSVC、Windows Qt SDK MinGW、macOS arm64/x86_64 本批未实际构建；静态审计确认新增生产源码没有平台条件分支、原生平台头或 Qt Private API。Ubuntu 22.04 可选参考档案仍为 `pending-user-validation`，不得作为本批已验证发布环境。
