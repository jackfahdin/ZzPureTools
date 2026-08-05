# ZzFluentUI 卡片控件 Implementation Plan

**Goal:** 在 `Zz::FluentUI` 中交付可复用、高性能、可键盘操作且不包含业务副作用的 `ZzActionCard` 与 `ZzImageCard`，覆盖旧版复杂卡片的核心展示和交互能力。

**Architecture:** 两个控件均直接继承 `QAbstractButton`，复用 Qt 的鼠标、Space/Enter、checkable、焦点和无障碍 Button 语义。控件只保存隐式共享的文字、图标或 `QPixmap`，使用当前 `QStyle`、`QPalette` 和设备无关几何同步绘制，不创建每项子控件、定时器、动画、网络请求或浮层。应用层通过原生 `clicked` 信号执行导航、URL 打开和业务命令。

**Tech Stack:** Qt 6.8+ Core/Gui/Widgets/Test、C++20、CMake 3.23、CTest、Qt Test、`Zz::FluentFoundation`、`Zz::FluentUI`。

---

## 1. 边界与旧版审计

- 工作目录固定为 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro`。
- 本批次属于总体设计阶段 10，在已完成 Calendar 批次后实施；可撕标签页继续使用后续独立计划。
- 旧仓库 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzFluentUI` 仅用于核对外观意图，不复制宏属性系统、全局主题单例或固定尺寸。
- 旧版 `ZzInteractiveCard`、`ZzReminderCard` 和 `ZzPopularCard` 的共同能力收敛为 `ZzActionCard`。
- 旧版 `ZzImageCard` 和 `ZzPromotionCard` 的共同能力收敛为新的 `ZzImageCard`。
- 旧版 `ZzAcrylicUrlCard` 的 URL 打开行为不进入 UI 组件；调用方连接 `QAbstractButton::clicked` 后自行调用平台服务。
- 不实现网络图片下载、磁盘缓存、URL 校验、业务徽标模型、延时浮层、推荐算法、路由或命令总线。
- 不使用 `QGraphicsEffect`，不在 hover/press 期间创建 `QPropertyAnimation`，不为每张卡创建 `QLabel`、`QTimer` 或中间 `QImage`。
- 不访问 Qt Private API，不依赖内部对象名，不增加 QWindowKit 依赖。
- 所有颜色从 `QStyleOption`/`QPalette` 获得；所有尺寸为设备无关逻辑像素，描边按 DPR 对齐。
- 两个公开类采用四文件 PIMPL；private 对象不是 `QObject`，只保存值和执行无分配布局/绘制。

## 2. 设计结论

### 2.1 为什么继承 QAbstractButton

直接继承 `QWidget` 并自行解析鼠标事件会重复实现按下捕获、键盘激活、checkable、autoRepeat、焦点和无障碍语义。`QAbstractButton` 已经提供这些行为，卡片只需呈现 `State_MouseOver`、`State_Sunken`、`State_On`、`State_HasFocus` 和 `State_Enabled`。

控件不新增 `activated`、`requested` 等同义信号，统一使用 Qt 原生：

```text
pressed -> released -> clicked(bool checked)
toggled(bool checked) 仅在 checkable=true 时产生
```

### 2.2 为什么不保留六个旧类

旧版六个类重复维护标题、副标题、图片尺寸、圆角和主题状态，并存在以下问题：

- hover 时动态分配多个 `QPropertyAnimation`；
- `PopularCard` 在控件内部持有定时器、浮层和操作按钮；
- `AcrylicUrlCard` 在 UI 类内部直接调用 `QDesktopServices::openUrl()`；
- 固定 180×200、320×120、330×105 等尺寸不适配字体、翻译和小窗口；
- 手写全局主题订阅，与当前 `ZzFluentStyle`/palette 生命周期重复；
- 多处绘制不处理 RTL、disabled、键盘焦点和长文本省略。

两个新组件通过职责而不是营销场景分类。业务可以用同一 `ZzActionCard` 表达提醒、推荐、设置入口，用同一 `ZzImageCard` 表达推广、媒体或项目预览。

### 2.3 绘制与热路径

两个控件都构造 `QStyleOptionButton`，清空 option 的默认文字与图标后调用 `QStyle::CE_PushButton` 绘制 Fluent surface、hover、press、checked、disabled 和 focus ring，再绘制卡片内容。

这样能够复用当前 `ZzFluentStyle`，并在调用方使用其他 `QStyle` 时保持平台可用。paint path 只做：

- 常数次 `QFontMetrics` 查询和 `elidedText()`；
- 常数次矩形计算；
- `QIcon::paint()` 或 `QPainter::drawPixmap()`；
- 文字和可选尾部 forward 指示器绘制。

paint path 禁止 `scaled()`、文件读取、SVG 解析、网络访问、对象创建、锁和容器增长。

### 2.4 图片策略

`ZzImageCard` 只接收调用方提供的 `QPixmap`。默认模式为 `Qt::KeepAspectRatioByExpanding`：计算源裁剪矩形后直接绘制，不生成缩放副本。

- `Qt::KeepAspectRatioByExpanding`：居中裁剪源图，填满图片区域。
- `Qt::KeepAspectRatio`：完整显示源图，目标区域可能留白。
- `Qt::IgnoreAspectRatio`：拉伸到图片区域。

空 pixmap 使用 palette 的 `AlternateBase` 填充和平台标准文件图标作为占位，不读取资源。

## 3. 公共 API

### 3.1 ZzActionCard

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzActionCard.h`:

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QAbstractButton>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzActionCardPrivate;

/** @brief 显示图标、标题、说明和可选尾部指示器的操作卡片。 */
class ZZ_FLUENT_UI_EXPORT ZzActionCard final : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(
        QString description
        READ description
        WRITE setDescription
        NOTIFY descriptionChanged)
    Q_PROPERTY(
        bool trailingIndicatorVisible
        READ isTrailingIndicatorVisible
        WRITE setTrailingIndicatorVisible
        NOTIFY trailingIndicatorVisibleChanged)
    Q_DISABLE_COPY_MOVE(ZzActionCard)

public:
    /** @brief 创建无标题的操作卡片。 */
    explicit ZzActionCard(QWidget *parent = nullptr);

    /** @brief 创建带标题和说明的操作卡片。 */
    explicit ZzActionCard(
        const QString &text,
        const QString &description = {},
        QWidget *parent = nullptr);

    /** @brief 销毁私有展示状态。 */
    ~ZzActionCard() override;

    /** @brief 返回辅助说明文字。 */
    [[nodiscard]] QString description() const;

    /** @brief 更新辅助说明并同步默认可访问描述。 */
    void setDescription(QString description);

    /** @brief 返回是否显示随布局方向变化的尾部指示器。 */
    [[nodiscard]] bool isTrailingIndicatorVisible() const noexcept;

    /** @brief 设置是否显示尾部指示器。 */
    void setTrailingIndicatorVisible(bool visible);

    /** @brief 返回适合单行标题和说明的建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

    /** @brief 返回不会让图标和文字区域互相覆盖的最小尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /** @brief 说明文字变化后发出。 */
    void descriptionChanged(const QString &description);

    /** @brief 尾部指示器可见性变化后发出。 */
    void trailingIndicatorVisibleChanged(bool visible);

protected:
    /** @brief 使用当前 style、palette 和按钮状态绘制操作卡片。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在字体、style、palette、布局方向变化后刷新几何。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzActionCardPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

标题、图标、图标尺寸、checkable 和 `clicked` 全部使用 `QAbstractButton` 原生 API。

### 3.2 ZzImageCard

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzImageCard.h`:

```cpp
#pragma once

#include <memory>

#include <QtCore/Qt>
#include <QtGui/QPixmap>
#include <QtWidgets/QAbstractButton>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzImageCardPrivate;

/** @brief 显示本地 pixmap、标题和说明的可操作图片卡片。 */
class ZZ_FLUENT_UI_EXPORT ZzImageCard final : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(
        QPixmap pixmap
        READ pixmap
        WRITE setPixmap
        NOTIFY pixmapChanged)
    Q_PROPERTY(
        QString description
        READ description
        WRITE setDescription
        NOTIFY descriptionChanged)
    Q_PROPERTY(
        Qt::AspectRatioMode aspectRatioMode
        READ aspectRatioMode
        WRITE setAspectRatioMode
        NOTIFY aspectRatioModeChanged)
    Q_DISABLE_COPY_MOVE(ZzImageCard)

public:
    /** @brief 创建空图片卡片。 */
    explicit ZzImageCard(QWidget *parent = nullptr);

    /** @brief 创建带标题和说明的图片卡片。 */
    explicit ZzImageCard(
        const QString &text,
        const QString &description = {},
        QWidget *parent = nullptr);

    /** @brief 销毁隐式共享图片引用和私有状态。 */
    ~ZzImageCard() override;

    /** @brief 返回当前隐式共享 pixmap。 */
    [[nodiscard]] QPixmap pixmap() const;

    /** @brief 更新本地 pixmap，不执行文件或网络读取。 */
    void setPixmap(QPixmap pixmap);

    /** @brief 返回图片下方的辅助说明。 */
    [[nodiscard]] QString description() const;

    /** @brief 更新辅助说明并同步默认可访问描述。 */
    void setDescription(QString description);

    /** @brief 返回图片适配策略。 */
    [[nodiscard]] Qt::AspectRatioMode aspectRatioMode() const noexcept;

    /** @brief 更新图片适配策略。 */
    void setAspectRatioMode(Qt::AspectRatioMode mode);

    /** @brief 返回包含 16:9 图片区和双行文字区的建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

    /** @brief 返回仍可辨认图片和标题的最小尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /** @brief pixmap 引用变化后发出。 */
    void pixmapChanged(const QPixmap &pixmap);

    /** @brief 说明文字变化后发出。 */
    void descriptionChanged(const QString &description);

    /** @brief 图片适配策略变化后发出。 */
    void aspectRatioModeChanged(Qt::AspectRatioMode mode);

protected:
    /** @brief 使用当前 style 和 palette 绘制图片、文字与按钮状态。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在字体、style、palette、DPR 变化后刷新几何和绘制。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzImageCardPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

## 4. 文件与所有权

### 4.1 生产代码

- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzActionCard.h`
- Create: `ZzFluentUI/widgets/src/ZzActionCard.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzActionCardPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzActionCardPrivate.cpp`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzImageCard.h`
- Create: `ZzFluentUI/widgets/src/ZzImageCard.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzImageCardPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzImageCardPrivate.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`

每个公开对象唯一拥有一个 `std::unique_ptr<...Private>`。private 不继承 `QObject`，不拥有 QWidget 子对象，不保存 painter、style、palette 或裸图片数据指针。

### 4.2 测试与示例

- Create: `ZzFluentUI/tests/ZzCardControlsTest.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/ZzFluentAccessibilityTest.cpp`
- Modify: `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`
- Modify: `ZzFluentUI/tests/ZzBasicControlsBenchmark.cpp`
- Modify: `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp`
- Modify: `tests/InstallConsumer/Gui/main.cpp`

## 5. Task 1：行为契约

- [ ] 注册 `ZzCardControlsTest`，测试名 `fluent.card-controls`，标签 `fluent;unit;component;accessibility`，使用 offscreen、严格告警和 Sanitizer。
- [ ] 验证两个控件默认 StrongFocus、非 checkable、非 autoRepeat，且不创建子 QWidget、timer 或 animation。
- [ ] 验证 ActionCard 的 description 和 trailing property 同值赋值不重复发信号。
- [ ] 验证 ImageCard 的 pixmap、description 和 aspect mode 同值赋值不重复发信号。
- [ ] 非法 `Qt::AspectRatioMode` 输入收敛到 `KeepAspectRatioByExpanding`，不保留未定义枚举值。
- [ ] Space 和 Enter 各产生一次原生 `clicked`；disabled 时不产生点击。
- [ ] checkable 卡片通过键盘切换状态并只产生一次 `toggled`。
- [ ] RTL 不改变逻辑内容顺序，尾部指示器自动移动到视觉左侧。
- [ ] 长标题、长说明、空图、零尺寸图和 1×1 图绘制不崩溃、不越界。
- [ ] 重复 1000 次属性更新和 render 后，后代 QObject、timer、animation 数量不变。

## 6. Task 2：实现 ZzActionCard

- [ ] 创建四文件 PIMPL 和中文 Doxygen 公共接口。
- [ ] 构造函数设置 StrongFocus、鼠标追踪、建议尺寸策略，不设置固定尺寸。
- [ ] private 计算 leading icon、标题、说明和 trailing indicator 的逻辑矩形；所有计算同时支持 LTR/RTL。
- [ ] 标题使用加粗字体单行省略，说明使用 palette secondary text 单行省略。
- [ ] icon 复用 `QAbstractButton::icon()`/`iconSize()`，空 icon 时文字区域自动占用空间。
- [ ] trailing indicator 使用 `QStyle::SP_ArrowForward`，不嵌入手写 SVG 或字体图标。
- [ ] checked 状态使用 Highlight/HighlightedText，disabled 使用 Disabled color group。
- [ ] style 绘制 surface 和 focus ring；private 不复制 `ZzFluentStyle` 颜色 token。
- [ ] `changeEvent()` 对 Font/Style 更新 geometry，对 Palette/LayoutDirection/Enabled 更新 paint。

## 7. Task 3：实现 ZzImageCard

- [ ] 创建四文件 PIMPL 和中文 Doxygen 公共接口。
- [ ] 使用隐式共享 `QPixmap` 值语义，setter 移动赋值；paint 不调用 `scaled()`。
- [ ] private 分别计算 fit 目标矩形、crop 源矩形和 stretch 矩形，处理空尺寸和浮点除零。
- [ ] 图片区采用稳定 16:9 约束，文字区保留标题、说明和 padding，不按 viewport 字号缩放。
- [ ] 图片裁剪区域使用与 card surface 一致的中等圆角，不产生 `QBitmap` mask。
- [ ] 空图使用 AlternateBase 与 `SP_FileIcon` 占位；disabled 图片降低 painter opacity，但不修改源 pixmap。
- [ ] 标题和说明在内容区分别单行省略，永不覆盖图片或卡片边界。
- [ ] checkable、focus、hover、press、RTL 与 accessibility 均沿用 ActionCard 相同语义。

## 8. Task 4：质量面

### 8.1 可访问性

- [ ] 在统一 accessibility 测试中确认两个对象均暴露 Button role、名称、描述、focused/disabled/checked 状态。
- [ ] 键盘 Tab 顺序和 Space/Enter 激活不依赖鼠标坐标。
- [ ] 不注册自定义 `QAccessibleInterface`，不把绘制文字复制成隐藏 QLabel。

### 8.2 视觉

- [ ] 建立独立 card screenshot surface，避免继续压缩基础控件和 Calendar 画布。
- [ ] 固定展示 ActionCard 的 normal/hover/focus/disabled/checked/RTL 和 ImageCard 的 crop/fit/empty 状态。
- [ ] 图片测试资产在内存中确定性生成，不读取网络、当前主题外部文件或系统照片。
- [ ] 扩展文字遮罩准确覆盖卡片标题和说明，不遮罩边框、图片、focus ring 或状态背景。
- [ ] 更新 Light/Dark/HighContrast × DPR 1.0/1.25/1.5/2.0 基线，并人工检查 1.0 与 2.0。

### 8.3 性能

- [ ] 预构造 100 张 ActionCard 与 40 张 ImageCard，图片由一份隐式共享 pixmap 提供。
- [ ] 预热 10 帧、测量 100 帧，循环改变 hover/checked/disabled 状态并 render 到复用图像。
- [ ] 记录 P50/P95/max；当前参考机绝对 P95 预算为 16.7 ms，普通环境只记录。
- [ ] 1000 次状态切换前后对象、timer、animation 和 pixmap cacheKey 保持稳定。

### 8.4 安装与边界

- [ ] GUI 安装消费者包含并构造两个新公共头。
- [ ] public-header aggregate 自动发现两个新头。
- [ ] `architecture.complete-audit`、`architecture.zzfluentui-boundaries` 和安装消费通过。
- [ ] Windows/MSVC/MinGW 与 macOS 条件分支静态审查：新增源码不得包含平台宏或平台私有 API。

## 9. Task 5：画廊

- [ ] 在滚动画廊加入 ActionCard 和 ImageCard 固定示例，不使用嵌套卡片。
- [ ] 只连接 `clicked` 更新局部展示状态，不访问 AppCore、网络、存储或路由。
- [ ] 800×600 使用既有滚动区浏览；1280×840 默认窗口无重叠和裁切。
- [ ] 示例图片由内存确定生成，不加入来源不明的第三方素材。

## 10. 提交边界

### 提交 A：计划

```text
文档：规划Fluent卡片控件批次

定义ActionCard与ImageCard的Qt按钮语义、无分配绘制、
图片裁剪、无障碍、视觉、性能和安装消费边界。
```

### 提交 B：生产契约

```text
控件：实现Fluent操作与图片卡片

复用QAbstractButton的键盘、焦点、checkable和无障碍语义，
增加palette驱动的行式卡片与无缩放副本图片裁剪绘制。
```

### 提交 C：完整质量面

```text
测试：补齐卡片视觉与性能门禁

将卡片接入无障碍、截图、性能、安装消费和交互画廊，
验证多主题、高DPI、RTL、长文本和对象数量稳定。
```

## 11. 本机完成门禁

```bash
cmake --build --preset linux-gcc-release --parallel 2
ctest --preset linux-gcc-release --output-on-failure
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy --parallel 2
cmake --build --preset linux-clang-asan --parallel 2
ctest --preset linux-clang-asan --output-on-failure
```

本批次继续忽略远端 CI，不调用 GitHub CLI，不推送。Windows、MinGW 和 macOS 只进行源码静态边界检查，等待用户后续人工验证。

## 12. 完成定义

- 两个公共类不包含 URL、网络、路由、业务模型或平台副作用。
- paint path 不读文件、不创建缩放 pixmap、不分配 QObject、不持锁。
- 键盘、焦点、checkable、disabled、RTL 和无障碍语义来自 QAbstractButton 并有测试。
- 长文本和空图在最小尺寸、默认尺寸和高 DPI 下不重叠、不越界。
- 四种图片适配输入（含非法枚举）具有确定收敛行为。
- 视觉、性能、安装、公共头、架构、GCC、Clang Tidy 和 ASan/UBSan 门禁通过。
- 旧版 Popular floater、Acrylic URL 副作用和 Promotion 业务命名不进入新组件。
- 可撕标签页保持下一独立批次，不被本计划隐式引入。
