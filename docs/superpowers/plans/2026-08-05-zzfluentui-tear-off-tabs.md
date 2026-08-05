# ZzFluentUI 可撕标签页 Implementation Plan

**Goal:** 在 `Zz::FluentUI` 中交付不依赖 Qt Private API、QWindowKit、业务模型或全局拖拽状态的 `ZzTabBar` 与 `ZzTabWidget`，支持同容器重排、跨容器移动和可回滚的拖出窗口意图。

**Architecture:** `ZzTabBar` 只负责公开 `QTabBar` 能力之上的拖拽手势、插入位置计算和拖出意图；`ZzTabWidget` 保存页面并执行受控的容器间转移。拖拽期间页面始终留在来源容器，目标确认放下后才同步移动；拖到容器外只发出意图，由应用层创建顶层窗口并调用同一个转移 API。进程内拖拽使用私有 `QMimeData` 子类保存 `QPointer`，同时发布版本化 MIME 标识，但不序列化地址、不使用动态属性或全局注册表。

**Tech Stack:** Qt 6.8+ Core/Gui/Widgets/Test、C++20、CMake 3.23、CTest、Qt Test、`Zz::FluentFoundation`、`Zz::FluentUI`。

---

## 1. 边界与旧版审计

- 工作目录固定为 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro`。
- 本批次属于总体设计阶段 10，在 Calendar 和 Card 批次之后实施。
- 旧仓库 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzFluentUI` 与 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzWindowKit` 只用于核对交互意图，不复制实现。
- 旧版直接包含 `private/qtabbar_p.h`，并读写 `QTabBarPrivate::pressedIndex`、`tabList`、`dragOffset` 和内部布局；新版只使用 `tabAt()`、`tabRect()`、`moveTab()` 等公开 API。
- 旧版通过 `QMimeData::setProperty()` 传播 `QWidget*`、`ZzTabBar*`、`ZzTabWidget*` 和浮窗指针；新版 MIME 字节不含地址，外部进程伪造相同格式也不能触发页面转移。
- 旧版依赖全局 `ZzDragMonitor` 单例；新版一次拖拽只拥有一个栈式/RAII 会话，不保留跨拖拽状态。
- 旧版每次拖动创建 10 ms `QTimer` 持续移动浮窗，人工发送鼠标事件并调用 `qApp->processEvents()`；新版交给 Qt DnD 事件循环，不轮询光标、不伪造事件、不主动重入。
- 旧版在拖拽开始时先移除页面并创建浮窗，取消和失败依赖析构回填；新版在放下成功前不改变页面所有权，因此 Escape、无效目标和目标销毁天然回滚。
- 旧版 `ZzCustomTabWidget` 析构时读取页面动态属性并尝试回填来源；来源可能已经销毁，关闭行为也混合了“关闭页面”和“送回原处”。新版关闭仍是 Qt 原生 `tabCloseRequested(int)` 意图，不删除、不回填页面。
- `ZzFluentUI` 不创建 `QDialog`、`QMainWindow` 或无边框窗口，不依赖 `ZzWindowKit`、`ZzPureTools`、QWK、领域模型、路由或命令总线。
- 画廊可以作为调用方创建普通演示窗口，证明 `tearOffRequested` 协议可落地；该演示不成为库的所有权规则。
- 两个公开类采用四文件 PIMPL；private 类不是 `QObject`，拖拽对象只在一次手势中按需创建。

## 2. 所有权与状态机

### 2.1 稳定态

每个页面在稳定态只属于一个 `ZzTabWidget`。标签文字、图标、工具提示、What's This、启用态、tab data 和文字颜色与页面一起移动。库不接管调用方页面的业务生命周期；关闭信号不调用 `deleteLater()`。

### 2.2 拖拽状态

```text
Idle
  -> Pressed(index, page)
  -> Dragging(source, guarded page, original index)
      -> Drop on same source: moveTab，回到 Idle
      -> Drop on valid target: transferTabTo，成功后回到 Idle
      -> Escape / invalid target / target destroyed: 不修改来源，回到 Idle
      -> Release outside compatible target: 发出 tearOffRequested，回到 Idle
```

约束：

- `Pressed` 仅记录公开 `tabAt()` 得到的索引和 `QPointer<QWidget>`，鼠标移动超过 `QApplication::startDragDistance()` 后才开始拖拽。
- 拖拽开始后索引可能因外部代码变化，所有提交动作都再次验证 `source->widget(index) == guardedPage`；不依赖旧索引本身。
- MIME 私有对象保存 `QPointer<ZzTabWidget>` 与 `QPointer<QWidget>`；任一对象销毁后目标拒绝放下。
- 目标只接受同一进程内、能转换为私有 MIME 类型、版本匹配且来源/页面仍一致的 MoveAction。
- 跨容器移动使用一次同步 `transferTabTo()`；调用返回前页面要么已在目标，要么仍在来源。
- `tearOffRequested` 发出时页面仍在来源。调用方应先创建目标 `ZzTabWidget`，再调用 `source->transferTabTo(target, index)`；创建失败或不处理信号不会丢页。
- Escape 通过拖拽期间的局部事件过滤器标记为取消；取消不发出 `tearOffRequested`。
- 同一页面不允许重复插入目标；目标为 `nullptr`、目标是来源但位置无效、索引失效或目标禁用接收时返回 `false`。

### 2.3 信号重入

- 转移前完整复制标签元数据，使用 `QPointer` 在每个可能触发 Qt 信号的步骤后复核来源、目标和页面。
- 页面在来源移除前不改 parent；来源移除后立即插入目标，不进入事件循环。
- 不在转移函数内调用 `processEvents()`、启动 timer 或等待 queued callback。
- 目标插入成功后才发出 `tabTransferred`。失败时按原索引和元数据恢复来源；如果来源已被同步销毁，则页面按 Qt 父子对象规则销毁，不解引用悬空地址。
- 测试覆盖 `currentChanged` 回调中 `deleteLater()`、目标在 drop 前销毁和页面在 drag 中销毁；不承诺支持槽函数直接 `delete this` 的非 Qt 常规用法。

## 3. 公共 API

### 3.1 ZzTabBar

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzTabBar.h`:

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QTabBar>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzTabBarPrivate;

/** @brief 提供 Fluent 外观、公开 API 拖拽和拖出意图的标签栏。 */
class ZZ_FLUENT_UI_EXPORT ZzTabBar final : public QTabBar
{
    Q_OBJECT
    Q_PROPERTY(
        bool tearOffEnabled
        READ isTearOffEnabled
        WRITE setTearOffEnabled
        NOTIFY tearOffEnabledChanged)
    Q_PROPERTY(
        bool tabTransferEnabled
        READ isTabTransferEnabled
        WRITE setTabTransferEnabled
        NOTIFY tabTransferEnabledChanged)
    Q_DISABLE_COPY_MOVE(ZzTabBar)

public:
    /** @brief 创建支持移动和进程内拖拽的标签栏。 */
    explicit ZzTabBar(QWidget *parent = nullptr);

    /** @brief 销毁私有拖拽状态。 */
    ~ZzTabBar() override;

    /** @brief 返回释放到兼容目标外时是否发出拖出意图。 */
    [[nodiscard]] bool isTearOffEnabled() const noexcept;

    /** @brief 设置是否允许发出拖出意图。 */
    void setTearOffEnabled(bool enabled);

    /** @brief 返回是否接受其他 ZzTabWidget 的标签。 */
    [[nodiscard]] bool isTabTransferEnabled() const noexcept;

    /** @brief 设置是否接受进程内标签转移。 */
    void setTabTransferEnabled(bool enabled);

Q_SIGNALS:
    /** @brief 拖出能力变化后发出。 */
    void tearOffEnabledChanged(bool enabled);

    /** @brief 标签转移接收能力变化后发出。 */
    void tabTransferEnabledChanged(bool enabled);

    /** @brief 标签释放到兼容目标外时发出，不改变页面所有权。 */
    void tearOffRequested(int index, const QPoint &globalPosition);

protected:
    /** @brief 记录公开 tabAt() 命中的按下标签。 */
    void mousePressEvent(QMouseEvent *event) override;

    /** @brief 超过平台拖拽阈值后启动一次 Qt MoveAction。 */
    void mouseMoveEvent(QMouseEvent *event) override;

    /** @brief 清理尚未开始拖拽的按下状态。 */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /** @brief 仅接受有效进程内标签载荷。 */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /** @brief 更新按布局方向计算的插入位置。 */
    void dragMoveEvent(QDragMoveEvent *event) override;

    /** @brief 将有效标签移动到当前宿主。 */
    void dropEvent(QDropEvent *event) override;

    /** @brief 离开目标时清除插入位置。 */
    void dragLeaveEvent(QDragLeaveEvent *event) override;

    /** @brief 绘制轻量插入指示线后保留 Qt/Fluent 标签绘制。 */
    void paintEvent(QPaintEvent *event) override;

private:
    friend class ZzTabWidget;
    std::unique_ptr<ZzTabBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

默认值：`tearOffEnabled=true`、`tabTransferEnabled=true`、`acceptDrops=true`、`elideMode=Qt::ElideRight`、`usesScrollButtons=true`。`movable` 仍由 `QTabWidget::setMovable()` 控制；构造 `ZzTabWidget` 时设为 `true`，调用方可显式关闭。

### 3.2 ZzTabWidget

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzTabWidget.h`:

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QTabWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzTabBar;
class ZzTabWidgetPrivate;

/** @brief 保存标签页并提供同步、可回滚的容器间转移。 */
class ZZ_FLUENT_UI_EXPORT ZzTabWidget final : public QTabWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzTabWidget)

public:
    /** @brief 创建使用 ZzTabBar 的可移动标签容器。 */
    explicit ZzTabWidget(QWidget *parent = nullptr);

    /** @brief 销毁容器及仍由容器拥有的页面。 */
    ~ZzTabWidget() override;

    /** @brief 返回本容器拥有的公开标签栏。 */
    [[nodiscard]] ZzTabBar *fluentTabBar() const noexcept;

    /**
     * @brief 将指定标签同步移动到目标容器。
     * @param target 目标标签容器。
     * @param sourceIndex 来源逻辑索引。
     * @param targetIndex 目标插入索引，负数表示末尾。
     * @return 成功移动或同容器重排时返回 true。
     */
    bool transferTabTo(
        ZzTabWidget *target,
        int sourceIndex,
        int targetIndex = -1);

Q_SIGNALS:
    /**
     * @brief 请求调用方为仍在本容器中的页面创建新宿主。
     * @param index 发出信号时的来源索引。
     * @param page 由本容器继续拥有的页面，仅供同步识别。
     * @param globalPosition 建议的新宿主屏幕位置。
     */
    void tearOffRequested(
        int index,
        QWidget *page,
        const QPoint &globalPosition);

    /** @brief 页面成功移入另一个容器后在目标容器发出。 */
    void tabTransferred(
        ZzTabWidget *source,
        int sourceIndex,
        int targetIndex,
        QWidget *page);

private:
    std::unique_ptr<ZzTabWidgetPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

`ZzTabWidget` 不增加 `closeTab()`：关闭语义继续使用 `tabsClosable` 与 `tabCloseRequested(int)`，调用方决定隐藏、缓存、转移还是删除页面。

## 4. 私有实现

### 4.1 文件

- Create `ZzFluentUI/widgets/src/ZzTabBar.cpp`
- Create `ZzFluentUI/widgets/src/private/ZzTabBarPrivate.h`
- Create `ZzFluentUI/widgets/src/private/ZzTabBarPrivate.cpp`
- Create `ZzFluentUI/widgets/src/ZzTabWidget.cpp`
- Create `ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.h`
- Create `ZzFluentUI/widgets/src/private/ZzTabWidgetPrivate.cpp`

所有新增类、公开方法和非显然的状态转换使用简体中文 Doxygen 注释。命名空间使用单层 `namespace ZzFluentUI {}`，禁止链式命名空间。

### 4.2 MIME 协议

- 格式固定为 `application/x-zz-fluent-tab-v1`。
- MIME bytes 只写固定版本标记，不写指针、QObject 名、页面文本或业务数据。
- 私有 `ZzTabMimeData final : public QMimeData` 保存来源、页面和起始索引的 `QPointer`。
- 接收端先检查 `hasFormat()`，再 `dynamic_cast<const ZzTabMimeData *>`，最后验证指针、页面当前索引和能力开关。
- 仅接受 `Qt::MoveAction`；不支持跨进程复制、不从不受信 MIME 创建 QWidget。
- `QDrag` 每次手势创建一次，使用标签区域 `grab()` 生成带 DPR 的拖拽图；拖拽结束立即销毁。

### 4.3 插入位置与 RTL

- 水平 LTR：光标位于标签中心前插入该逻辑索引，中心后继续扫描。
- 水平 RTL：逻辑扫描顺序不变，但“中心前”是 `x > center.x()`。
- 垂直方向按 y 从上到下处理。
- 空目标返回 0；超过最后一个逻辑标签返回 `count()`。
- 同容器从较小索引移向较大插槽时，在移除语义后将目标索引减一。
- 插入指示线使用 palette `Highlight`，宽度为 2 个逻辑像素并按 DPR 对齐；不创建动画或缓存 pixmap。

### 4.4 元数据事务

转移快照包含：

```text
page + text + icon + toolTip + whatsThis
+ enabled + tabData + textColor + sourceCurrent
```

执行顺序：验证 -> 快照 -> 来源 removeTab -> 目标 insertTab -> 恢复元数据 -> 选择移入页面 -> 发出 `tabTransferred`。失败则按钳制后的原索引恢复来源和元数据。任何路径都不删除页面。

## 5. 自动测试

Create `ZzFluentUI/tests/ZzTabControlsTest.cpp`，接入 `ZzFluentUI/tests/CMakeLists.txt`。

覆盖：

- 默认使用 `ZzTabBar`，movable、tear-off、transfer、scroll button 和 elide 策略符合契约。
- `transferTabTo()` 保留页面身份和全部标签元数据，不复制或删除 QWidget。
- 向空目标、目标首部、中部和末尾移动；同容器向前/向后重排。
- 非法来源索引、非法目标、来源等于目标的无变化位置、已存在页面、禁用目标均拒绝且来源不变。
- 目标接收一次有效私有 MIME 后只转移一次；普通 `QMimeData` 即使伪造格式也被拒绝。
- Escape、无效 drop、目标销毁、页面销毁和未连接 tear-off 信号均不丢失页面、不改变来源顺序。
- 拖出请求只发一次，发出时 `widget(index)==page`，同步槽可创建目标并调用 `transferTabTo()`。
- `tabCloseRequested` 只发意图，不移除或删除页面。
- `currentChanged`、`tabBarClicked`、Space/Enter、Ctrl+Tab 和焦点顺序保留 Qt 行为。
- accessible interface 非空，角色为 `PageTabList`/Qt 实际稳定角色，标签名称来自 tab text；disabled 状态可见。
- RTL 与 LTR 的插入位置镜像，逻辑索引和页面顺序符合契约。
- North、South、West、East 四个位置均不越界，长文本省略且滚动按钮不覆盖标签。
- 1000 次同容器重排与 1000 次双容器往返后 QObject、timer、animation 数量稳定。

测试优先直接调用受保护事件适配 fixture 和 `transferTabTo()`，仅用少量真实鼠标拖拽覆盖阈值与 signal；不依赖窗口管理器实现私有 DnD 行为。

## 6. 可访问性、键盘与视觉

- 不自绘标签文字、关闭按钮或焦点框；标签主体继续由 `ZzFluentStyle::CE_TabBarTab` 和 Qt 原生 `QTabBar` 绘制，保留平台无障碍实现。
- `ZzTabBar` 不覆盖键盘事件。方向键、Home/End、Ctrl+Tab、焦点和快捷键语义由 Qt 提供。
- `tabCloseRequested` 不等同 Delete 键；库不为删除业务内容绑定默认键。
- `tearOffRequested` 是指针拖拽意图；键盘用户可以通过应用命令调用公开 `transferTabTo()`，不要求模拟拖拽。
- 更新 `ZzFluentAccessibilityTest`，将 `ZzTabWidget` 纳入固定 Tab/Backtab 顺序并断言标签可访问名称。
- 更新 `ZzFluentScreenshotTest` 和独立标签截图面，覆盖 Light、Dark、HighContrast × DPR 1.0/1.25/1.5/2.0。
- 截图包含 selected、hover、focus、disabled、closable、long text、overflow、LTR、RTL 和插入指示状态。
- 文字继续纳入现有 QTabBar mask；非文字差异沿用每通道 3、差异像素比例 0.5% 的门禁。

## 7. 性能与对象稳定性

更新 `ZzBasicControlsBenchmark`：

- 构建一个包含 100 个轻量页面的 `ZzTabWidget`，只渲染可见标签栏，不为不可见页面执行自定义绘制。
- 采集 120 帧标签选择/重排渲染时间，Release 参考发布机 P95 预算为 16.7 ms。
- 1000 次两个各 20 页容器间往返转移后，页面地址集合完全一致，无页面被删除或复制。
- 稳定态不持有 `QDrag`、`QMimeData`、`QTimer` 或 `QAbstractAnimation`；完成拖拽后对象计数回到基线。
- paint path 只增加常数次插入线计算；无活动拖拽时直接调用基类，不分配容器或 pixmap。

## 8. 画廊与实际拖出示例

更新 `ZzFluentControlsGalleryPrivate`：

- 用 `ZzTabWidget` 替换当前仅展示标签头的 `QTabBar`。
- 提供三个本地静态页面，允许重排、关闭意图和跨两个容器移动。
- 连接 `tearOffRequested` 后创建 `WA_DeleteOnClose` 的普通 QWidget 演示宿主，在其中创建另一个 `ZzTabWidget` 并调用 `transferTabTo()`。
- 演示窗口关闭时若仍有页面，先同步转回来源；来源已销毁时由演示窗口正常拥有并销毁页面。
- 这段回填是示例应用策略，不进入 `ZzFluentUI` 控件。
- 画廊不显示功能说明、快捷键说明或架构文字。

## 9. 安装消费与静态门禁

- 在 `tests/InstallConsumer/CMakeLists.txt` 增加 `ZzTabBar.h`、`ZzTabWidget.h` 公共头探针。
- 在 `tests/InstallConsumer/Gui/main.cpp` 构造两个容器并执行一次 `transferTabTo()`。
- 安装消费者只链接 `Zz::FluentUI`，不得看见源码树/private include。
- public header 自包含，并在 shared/static 安装树中通过。
- 文本审计禁止 `private/qtabbar_p.h`、`QTabBarPrivate`、`qApp->processEvents`、10 ms drag timer、动态属性指针、QWK、ZzWindowKit 和 ZzPureTools。
- Windows MSVC、Windows Qt MinGW、macOS arm64/x86_64 当前只做公开 API 与条件编译静态检查；远端 CI 按用户要求暂缓，不作为本批次本机提交阻塞项。
- 不下载新的 Qt；Linux 使用现有 `/home/zz/Qt/6.11.1/gcc_64`。

## 10. 实施与提交边界

### Task 1：计划冻结

- 写完本文件。
- 核对 API 没有顶层窗口、业务模型、Qt Private 或跨模块反向依赖。
- 文档审计后提交：

```text
文档：规划Fluent可撕标签页批次

记录旧版私有 API、裸指针 MIME 和浮窗生命周期问题。
定义页面不提前移除的拖拽状态机、同步转移 API 与应用层拖出边界。
列明交互、无障碍、截图、性能、安装消费和跨平台静态门禁。
```

### Task 2：生产控件与核心测试

- 新增两个公开类的四文件 PIMPL、CMake source/MOC 注册和 `ZzTabControlsTest`。
- 先验证属性、元数据事务、同/跨容器移动、错误输入和关闭意图。
- 提交：

```text
控件：实现Fluent可撕标签页

新增基于 Qt 公开 API 的 ZzTabBar 与 ZzTabWidget。
实现私有进程内 MIME、布局方向感知插入位置和同步页面转移事务。
页面在成功放下前保留于来源，取消与无效目标不改变所有权。
```

### Task 3：质量与消费

- 补齐真实 DnD、Escape、销毁、RTL、键盘、无障碍和对象稳定性测试。
- 接入安装消费者与 public-header 审计。
- 提交：

```text
测试：接入标签页质量与安装消费

覆盖拖拽取消、跨容器所有权、关闭意图、键盘、无障碍和 RTL。
验证伪造 MIME 不可触发转移，并保证 installed public headers 自包含。
```

### Task 4：画廊、视觉与性能

- 接入画廊实际拖出宿主、独立标签截图面、12 张基线和 benchmark。
- 提交：

```text
示例：接入可撕标签页交互与质量基线

在控件画廊演示应用层浮窗宿主与同步回填策略。
补齐三主题四 DPR 截图和标签选择、重排、跨容器转移性能门禁。
```

### Task 5：最终验证与记录

- GCC 15 Release：完整 build、CTest、examples、benchmarks、install consumer。
- Clang 20 ASan/UBSan：完整 build 与 CTest。
- Clang 20 clang-tidy：全部一方翻译单元。
- 文档和依赖审计通过后，把实测数量、时间、未执行远端平台验证和风险写回本文件。
- 提交：

```text
文档：记录可撕标签页批次交付结果

记录 Linux GCC、Clang、sanitizer、clang-tidy、截图、性能和安装消费结果。
明确 Windows 与 macOS 当前仅完成静态代码审计，远端 CI 暂缓。
```

## 11. 本机验证命令

环境：

```bash
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
```

Release：

```bash
cmake --preset linux-gcc-release \
  -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_BUILD_BENCHMARKS=ON \
  -DCMAKE_EXECUTABLE_FORMAT=ELF
cmake --build --preset linux-gcc-release --parallel 2
xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  ctest --preset linux-gcc-release --output-on-failure
```

Sanitizer 与 tidy 沿用仓库既有 Clang preset 和 `ZzClangTidy` target，不访问 GitHub CLI、不推送、不下载 Qt。

## 12. 退出条件

- `ZzTabBar`、`ZzTabWidget` public API 与安装包可消费。
- 同容器重排、跨容器转移和应用层拖出三条路径均有自动测试。
- Escape、无效/伪造 MIME、目标或页面销毁不会丢页或解引用悬空指针。
- 生产代码不含 Qt Private API、全局拖拽单例、动态属性裸指针、鼠标伪造、轮询 timer 或 `processEvents()`。
- UI 层不创建顶层窗口，不依赖 ZzPureTools、ZzWindowKit、QWK 或业务模型。
- Light、Dark、HighContrast × 四 DPR 视觉基线通过。
- Release 性能和对象稳定性满足门禁。
- Linux GCC、Clang、ASan/UBSan、clang-tidy、安装消费通过；Windows/macOS 的待验证状态如实记录。

## 13. 交付结果

待实现完成后填写。
