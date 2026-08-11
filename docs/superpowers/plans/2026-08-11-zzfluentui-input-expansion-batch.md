# ZzFluentUI 第 3 批输入组件扩展实施计划

**目标：** 在不复制 Qt 文本编辑、快捷键、菜单和无障碍状态机的前提下，新增 `ZzPasswordBox`、`ZzSplitButton`、`ZzRatingControl`、`ZzKeyBinder`、`ZzColorPicker` 五个公开组件，把公开 Fluent 组件数由 32 增至 37，并完成标准 `QColorDialog` 的公开 API 可达性评估。

**实施基线：** 本计划基于提交 `0c657d5` 后的源码、旧版同级目录和 Qt 6.11.1 公共头审计。第 0、1、2 批已经完成；本文负责第 3 批的逐文件、逐状态和逐门禁定论，总路线 `2026-08-10-zzfluentui-expansion-master-plan.md` 只保留批次级状态。

**平台边界：** 生产实现只使用 Qt 6.8+ Core/Gui/Widgets/Svg 公共 API 和标准 C++20。Linux 参考机执行构建、功能、截图、Clang-Tidy、Sanitizer、安装消费、架构和性能门禁；Windows MSVC、Windows Qt SDK MinGW、macOS arm64/x86_64 未真实执行时只记录源码、预设、条件编译和依赖方向静态审计。

---

## 1. 已审计事实与本批定论

### 1.1 现有生产能力

- 应用级 `ZzFluentStyle` 已覆盖 `QLineEdit`、`QTextEdit`、`QPlainTextEdit` 和 `QTextBrowser` 的 frame、surface、hover、focus、disabled 与尺寸；密码文本、输入法、validator、光标、选择、撤销和剪贴板仍由 Qt 管理。
- `ZzIconButton` 已通过 `ZzIconDescriptor` 和 `ZzFluentStyle::iconPixmap()` 使用有界 SVG/字体图标缓存。ZzAwesome 已包含 Eye、EyeSlash、ChevronDown、Keyboard、Palette 和 Star 字形，本批不复制 TTF/SVG，不新建第二套图标缓存。
- `QMenu`、`QMenuBar` 和 `QToolButton` popup surface 已由 `ZzFluentStyle` 覆盖；SplitButton 只负责两个命中区域和打开意图，不复制 action、submenu、shortcut 或 popup 窗口协议。
- `ZzSpinBox` 保留 `QSpinBox` 原生范围、输入和无障碍语义，可直接作为 ColorPicker 的 RGB(A) 数值编辑器。
- `ZzWidgetTheme` 已为组合/自绘组件提供 Fluent snapshot 和非 Fluent style 回退 snapshot；新组件不得缓存第二套主题真值。
- 第 2 批已把“每实例固定对象、快速操作从当前状态重定向、reduced motion 同步终态、对象地址与数量预算”写入编码规范。本批没有必要的持续动画，不为装饰效果引入 timer 或 animation。

### 1.2 旧版参考结论

- 旧版 `ZzLineEdit` 及其每实例 style 不迁移。新 PasswordBox 继承 `QLineEdit`，只增加 reveal 交互；不得新建 password 字符串副本或重新实现键盘/输入法。
- 旧版没有可复用的 SplitButton、RatingControl 和 PasswordBox 生产源码；旧示例 PNG 只用于核对功能覆盖，不作为像素基线或实现来源。
- 旧版 `ZzKeyBinder` 使用 `QLabel`、应用模态对话框、平台 `nativeVirtualKey`、手写鼠标键码、强制抢回焦点和 stylesheet。新实现不迁移这些行为，只保存跨平台 `QKeySequence`，冲突判断由 Presenter/调用方负责。
- 旧版 `ZzColorDialog` 固定 600x600、内嵌自定义标题栏、每实例 `QProxyStyle`、重复 RGB/HSV/HTML 状态和全局自定义色表；旧 `ZzColorPicker` 在构造时逐像素生成 360x360 图像且每次鼠标事件重复颜色换算。以上实现均不迁移。

### 1.3 决策表

| 决策点 | 定论 | 理由 |
|---|---|---|
| PasswordBox 基类 | `final : QLineEdit` | 完整保留密码编辑、输入法、选择、撤销和 EditableText 无障碍语义 |
| 密码显示策略 | `ZzPasswordRevealMode::{Hidden, Peek, Visible}` | 对齐永久隐藏、按住查看、永久显示三类明确状态，不增加 password 副本 |
| SplitButton 基类 | `final : QPushButton` | 主区复用原生按钮 click/default/键盘/无障碍语义，菜单区由一个控件内的独立命中状态管理 |
| SplitButton 菜单所有权 | 非拥有 `QPointer<QMenu>` | 与 Qt 组合习惯一致，菜单 action 和业务命令继续属于调用方 |
| Rating 数据 | `qreal rating` + Whole/Half 精度 | 不把内部半星单位泄露给公共接口；鼠标、键盘和无障碍都读取同一值 |
| Rating 无障碍 | 私有 `QAccessibleWidget` + `QAccessibleValueInterface` | 原生 QWidget 没有正确的 qreal rating 角色和值范围，不能用内部整数冒充公开值 |
| KeyBinder 基类 | `final : QKeySequenceEdit` | 复用 Qt 的组合键解析、平台显示、结束组合和 Editable/Hotkey 无障碍支持 |
| KeyBinder 范围 | 仅 `QKeySequence` | 禁止公共 API 暴露 Windows native key 或自造鼠标键码；全局快捷键注册不属于 UI |
| QColorDialog | 不包装、不修改内部对象 | 原生/非原生实现都拥有 private HSV/色板绘制，公开 QStyle 只能覆盖标准子控件，无法统一完整 Fluent 视觉 |
| ColorPicker | 新增可组合 `ZzColorPicker` | palette grid + RGB(A) + hex 是可用最小闭环，可嵌入 `ZzContentDialog`，不重复窗口协议 |
| 截图 | 独立 input-expansion 场景 | 避免扩大通用 controls 画布，把五组件三主题/四 DPR 差异隔离为 12 张基线 |

---

## 2. 共用令牌、热路径与状态规则

### 2.1 主题尺寸

修改：

- `ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h`
- `ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp`
- `ZzFluentUI/tests/ZzThemeSnapshotTest.cpp`

新增具名 metric：

```cpp
SplitButtonMenuExtent, // 32，菜单命中区逻辑宽度
RatingGlyphExtent,     // 24，单个星级图标逻辑边长
ColorSwatchExtent,     // 32，色板单元逻辑边长
ColorSwatchGap,        // 8，色板单元逻辑间隔
```

Password reveal button 复用 `ControlHeight`、`IconSmall` 和现有 padding；边框、圆角、焦点、颜色全部复用现有令牌。实际用户颜色是组件数据而非主题色，允许作为 `QColor`/`QBrush` 内容绘制，但控件框架、空态、边框和选择反馈不得从内容颜色反推主题色。

### 2.2 固定对象与缓存预算

- PasswordBox 每实例固定一个 `ZzIconButton`，无 timer、animation、event filter 和第二个 line edit。
- SplitButton 不创建两个子按钮；每实例只有 private 值状态及非拥有 menu 连接，menu popup 由调用方的 `QMenu` 管理。
- RatingControl 固定缓存 empty/filled 两张星级 pixmap；theme revision、DPR、glyph extent、palette group 或 enabled 状态变化时才重取缓存，paint 只复制隐式共享句柄并 clip。
- KeyBinder 只使用 `QKeySequenceEdit` 自带对象；开始/取消 1000 次不得增加 QObject 子对象。不得把 Qt 的内部 timer id 当成项目 QObject timer。
- ColorPicker 构造一次 model、view、delegate、preview、四个 spin box 和 hex edit；palette reset 只更新 model 数据，不重建 widget/delegate/layout。

### 2.3 事件与业务边界

- 五组件只维护输入和纯展示状态，发出 text/click/rating/key/color intent；不得访问 settings、快捷键注册器、文件、数据库、网络或领域对象。
- 程序化 setter 幂等；无效输入必须拒绝或规范化后只发一次信号。内部控件同步使用 guard 或 `QSignalBlocker`，不得形成信号回环。
- 所有鼠标命中使用 `QMouseEvent::position()` 和逻辑坐标；RTL 使用 `QStyle::visualRect()` 或 layout direction，不用平台宏反转。
- HighContrast 仍保留焦点、边框和当前值；不能只靠 hover、透明度或颜色差表达状态。

---

## 3. 任务一：ZzPasswordBox

### 3.1 新增文件与 API

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzPasswordRevealMode.h`
- `ZzFluentUI/widgets/include/ZzFluentUI/ZzPasswordBox.h`
- `ZzFluentUI/widgets/src/ZzPasswordBox.cpp`
- `ZzFluentUI/widgets/src/private/ZzPasswordBoxPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzPasswordBoxPrivate.cpp`
- `ZzFluentUI/tests/ZzPasswordBoxTest.cpp`

枚举：

```cpp
enum class ZzPasswordRevealMode : std::uint8_t
{
    Hidden,
    Peek,
    Visible,
};
```

公共类：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzPasswordBox final : public QLineEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPasswordBox)
    Q_PROPERTY(
        ZzFluentUI::ZzPasswordRevealMode revealMode
        READ revealMode WRITE setRevealMode NOTIFY revealModeChanged)
    Q_PROPERTY(
        bool passwordVisible
        READ isPasswordVisible NOTIFY passwordVisibilityChanged)

public:
    explicit ZzPasswordBox(QWidget *parent = nullptr);
    ~ZzPasswordBox() override;
    [[nodiscard]] ZzPasswordRevealMode revealMode() const noexcept;
    void setRevealMode(ZzPasswordRevealMode mode);
    [[nodiscard]] bool isPasswordVisible() const noexcept;

Q_SIGNALS:
    void revealModeChanged(ZzPasswordRevealMode mode);
    void passwordVisibilityChanged(bool visible);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    using QLineEdit::setEchoMode;
    std::unique_ptr<ZzPasswordBoxPrivate> d_ptr;
};
```

不声明 `password` property；继承的 `text` 是唯一密码状态。把 `setEchoMode` 在 ZzPasswordBox 静态类型上隐藏，防止调用方绕过 revealMode 破坏约束；通过 `QLineEdit *` 强制改 echoMode 属于未支持用法。

### 3.2 私有装配与交互

构造默认 `revealMode=Peek`、`echoMode=Password`。固定 reveal button：

- 类型 `ZzIconButton`，objectName `zzPasswordRevealButton`。
- 使用 `ZzFontIcon::Eye`，按下可改为 EyeSlash；`accessibleName`、tooltip 和 `accessibleDescription` 由 `LanguageChange` 刷新。
- 位于逻辑 trailing 侧，尺寸由 `ControlHeight`、`IconSmall`、padding 计算；resize、font/style/palette、DPR、LayoutDirectionChange 后更新几何和文本安全边距。
- Hidden：按钮隐藏且 Password；Visible：按钮隐藏且 Normal；Peek：非空时按钮可见，pressed 时 Normal，released/cancel/focus out/window deactivate 时立即 Password。
- button 支持鼠标和 Space，按住才显示，不以单击切换持久状态。控件禁用、清空、模式切换或按钮隐藏前必须先结束 peek。

Private 保存原始逻辑文本 margins，并集中计算 effective margins，防止图标覆盖文本；组件内部不会每次 resize 累加边距。

### 3.3 测试与接线

测试覆盖：默认 Password/Peek、三模式幂等、按下/释放、拖出释放、focus out/window deactivate、空文本按钮、禁用态、RTL trailing 几何、长文本与自定义字体不被按钮覆盖、LanguageChange、EditableText 无障碍值不泄露明文、reveal button Button 角色，以及 1000 次 press/release 后按钮地址和 QObject 数不变。

定向命令：

```bash
cmake --build --preset linux-gcc-reference --target ZzPasswordBoxTest
ctest --preset linux-gcc-reference -R 'fluent.password-box' \
  --output-on-failure
```

提交标题：`控件：新增 Fluent 密码输入框`

---

## 4. 任务二：ZzSplitButton

### 4.1 新增文件与 API

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzSplitButton.h`
- `ZzFluentUI/widgets/src/ZzSplitButton.cpp`
- `ZzFluentUI/widgets/src/private/ZzSplitButtonPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzSplitButtonPrivate.cpp`
- `ZzFluentUI/tests/ZzSplitButtonTest.cpp`

公共类继承 `QPushButton`，继续使用其 text、icon、clicked、default、autoDefault、shortcut 和 accessible Button 语义：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzSplitButton final : public QPushButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzSplitButton)
    Q_PROPERTY(
        ZzFluentUI::ZzButtonAppearance appearance
        READ appearance WRITE setAppearance NOTIFY appearanceChanged)
    Q_PROPERTY(QMenu *menu READ menu WRITE setMenu NOTIFY menuChanged)

public:
    explicit ZzSplitButton(QWidget *parent = nullptr);
    explicit ZzSplitButton(const QString &text, QWidget *parent = nullptr);
    ~ZzSplitButton() override;
    [[nodiscard]] ZzButtonAppearance appearance() const noexcept;
    void setAppearance(ZzButtonAppearance appearance);
    [[nodiscard]] QMenu *menu() const noexcept;
    void setMenu(QMenu *menu);
    [[nodiscard]] QSize sizeHint() const override;

public Q_SLOTS:
    void showMenu();

Q_SIGNALS:
    void appearanceChanged(ZzButtonAppearance appearance);
    void menuChanged(QMenu *menu);
    void menuRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    [[nodiscard]] bool hitButton(const QPoint &position) const override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzSplitButtonPrivate> d_ptr;
};
```

### 4.2 命中、菜单与绘制

- Private 用 `QStyle::visualRect()` 把全 rect 划分为 main rect 和 `SplitButtonMenuExtent` 宽的逻辑 trailing menu rect；高度过小时 menu rect 收敛到可用宽度。
- main press/release 交给 `QPushButton`；重写 `hitButton()` 只接受 main rect，保证拖到箭头区释放不会误发 `clicked()`。
- menu press 不调用 QPushButton base，不发 clicked；在同一 menu rect release 后调用 `showMenu()`。Down、Alt+Down 打开 menu，Space/Enter 仍触发 main click。
- `showMenu()` 先发 `menuRequested()`，允许调用方同步填充或设置菜单，再用 `QMenu::popup()` 在逻辑 leading 对齐的按钮下方打开。Private 以 `QPointer<QMenu>` 借用菜单并观察 destroyed/aboutToHide；不接管 menu 和 QAction 所有权。
- paint 只绘制一个完整圆角 surface、内部一条 stroke 分隔线、main label、trailing ChevronDown 和完整 focus ring。hover/pressed 按 main/menu 区分别选择 ControlFillHover/Pressed；Accent/Subtle 与 `ZzPushButton` 的 token 规则一致。
- label 使用 `CE_PushButtonLabel` 处理 icon、助记键、disabled 与 RTL；箭头使用缓存字体图标或标准 `PE_IndicatorArrowDown` 回退，不手绘折线。

### 4.3 测试与接线

测试覆盖：appearance/menu 幂等、外部 menu 销毁、main 单击一次、menu 区不发 clicked、拖出取消、menuRequested 后延迟装配、popup anchor LTR/RTL、Down/Alt+Down、Space/Enter、disabled、default button、focus ring、Button/PopupMenu 无障碍，以及 1000 次打开关闭后 SplitButton 子对象数不增长。

提交标题：`控件：新增 Fluent 分割按钮`

---

## 5. 任务三：ZzRatingControl

### 5.1 新增文件与 API

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzRatingPrecision.h`
- `ZzFluentUI/widgets/include/ZzFluentUI/ZzRatingControl.h`
- `ZzFluentUI/widgets/src/ZzRatingControl.cpp`
- `ZzFluentUI/widgets/src/private/ZzRatingControlPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzRatingControlPrivate.cpp`
- `ZzFluentUI/tests/ZzRatingControlTest.cpp`

枚举：

```cpp
enum class ZzRatingPrecision : std::uint8_t
{
    Whole,
    Half,
};
```

公共类：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzRatingControl final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzRatingControl)
    Q_PROPERTY(qreal rating READ rating WRITE setRating NOTIFY ratingChanged)
    Q_PROPERTY(
        int maximumRating
        READ maximumRating WRITE setMaximumRating NOTIFY maximumRatingChanged)
    Q_PROPERTY(
        ZzFluentUI::ZzRatingPrecision precision
        READ precision WRITE setPrecision NOTIFY precisionChanged)
    Q_PROPERTY(
        bool readOnly READ isReadOnly WRITE setReadOnly NOTIFY readOnlyChanged)

public:
    explicit ZzRatingControl(QWidget *parent = nullptr);
    ~ZzRatingControl() override;
    [[nodiscard]] qreal rating() const noexcept;
    void setRating(qreal rating);
    [[nodiscard]] int maximumRating() const noexcept;
    void setMaximumRating(int maximum);
    [[nodiscard]] ZzRatingPrecision precision() const noexcept;
    void setPrecision(ZzRatingPrecision precision);
    [[nodiscard]] bool isReadOnly() const noexcept;
    void setReadOnly(bool readOnly);
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    void ratingChanged(qreal rating);
    void maximumRatingChanged(int maximum);
    void precisionChanged(ZzRatingPrecision precision);
    void readOnlyChanged(bool readOnly);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzRatingControlPrivate> d_ptr;
};
```

### 5.2 值、绘制与无障碍

- 默认 rating=0、maximum=5、precision=Whole、readOnly=false；maximum 收敛到 1..10。setter 先 clamp，再按 Whole/Half 量化；NaN/Inf 拒绝，幂等不发信号。
- 逻辑 cell 从 leading 到 trailing 排列。mouse hover 只更新 preview，press/drag/release 映射为量化值并提交；leave 恢复真实 rating。readOnly 和 disabled 不接受输入。
- Left/Right 按视觉方向减/增，Up/Down 增/减，Home=0，End=maximum；步长由 precision 决定。按键只修改 UI 值，不做评分提交业务。
- paint 使用 `ZzFontIcon::Star` 的两张缓存 pixmap：empty 使用 TextSecondary/disabled，filled 使用 Accent。每个 cell 先画 empty，再按 rating 或 hover preview 的 0/0.5/1 clip filled；focus ring 包围完整组件，不用纯颜色表达键盘焦点。
- 在 private cpp 注册一次 `ZzAccessibleRatingControl`，角色为 Slider，`QAccessibleValueInterface` 直接返回 qreal rating、0、maximum 和 0.5/1 step；readOnly 时拒绝 setCurrentValue。rating 变化发送 `QAccessibleValueChangeEvent`，Name 取 accessibleName，Value/Description 使用本地化的“当前值/最大值”。

### 5.3 测试与接线

测试覆盖：最大值收敛、Whole/Half 量化、NaN/Inf、幂等信号、每个 cell 与半星命中、拖动、hover 不改真值、readOnly/disabled、键盘、RTL、主题/高对比/DPR cache 失效、像素 clip 不覆盖相邻星、Slider qreal 无障碍，以及 1000 次 hover/value 更新后 pixmap/QObject 数量有界。

提交标题：`控件：新增 Fluent 星级评分`

---

## 6. 任务四：ZzKeyBinder

### 6.1 新增文件与 API

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzKeyBinder.h`
- `ZzFluentUI/widgets/src/ZzKeyBinder.cpp`
- `ZzFluentUI/widgets/src/private/ZzKeyBinderPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzKeyBinderPrivate.cpp`
- `ZzFluentUI/tests/ZzKeyBinderTest.cpp`

公共类直接继承 `QKeySequenceEdit`，沿用其 `keySequence`、`clearButtonEnabled`、`maximumSequenceLength`、`finishingKeyCombinations`、`keySequenceChanged` 与 `editingFinished`：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzKeyBinder final : public QKeySequenceEdit
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzKeyBinder)
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)

public:
    explicit ZzKeyBinder(QWidget *parent = nullptr);
    explicit ZzKeyBinder(
        const QKeySequence &sequence,
        QWidget *parent = nullptr);
    ~ZzKeyBinder() override;
    [[nodiscard]] bool isRecording() const noexcept;

public Q_SLOTS:
    void startRecording();
    void cancelRecording();

Q_SIGNALS:
    void recordingChanged(bool recording);
    void recordingCanceled();
    void recordingAccepted(const QKeySequence &sequence);

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzKeyBinderPrivate> d_ptr;
};
```

### 6.2 录制状态与边界

- 默认 maximumSequenceLength=1，调用方仍可用 Qt 公开 API 改到 1..4；显示文本使用 `QKeySequence::NativeText`，序列值保持跨平台 Qt 表达。
- startRecording 保存开始前 sequence、设置 StrongFocus 并进入 recording；首次键盘输入或 focus in 同样进入。Qt base 处理修饰键组合、auto repeat、结束组合和超时。
- Escape 在 recording 时恢复快照、退出并发 `recordingCanceled()`；Backspace 调用 base `clear()` 并继续录制。base `editingFinished` 或 focus out 接受当前值、退出并发 `recordingAccepted()`；状态信号幂等。
- LanguageChange 刷新“按下快捷键/Escape 取消/Backspace 清除”的 accessibleDescription 和 tooltip。组件不弹 ContentDialog、不强制抢焦点、不读取 nativeVirtualKey、不捕获鼠标按钮、不注册全局快捷键。
- 冲突检查由 Presenter/调用方监听 `keySequenceChanged` 或 `recordingAccepted` 后执行；组件本身不知道应用命令表，也不提供误导性的 conflict 真值。

### 6.3 测试与接线

测试覆盖：构造与程序化 setter、Ctrl/Shift/普通键组合、maximumSequenceLength、finishing combinations、auto repeat、Escape 恢复、Backspace 清除、focus out 接受、录制信号幂等、NativeText、LanguageChange、Qt 原生无障碍 Name/Value，以及 1000 次开始/取消后 QObject 数不增长。平台测试不得断言 Windows/Linux/macOS 具体显示字符串相同，只断言 `QKeySequence` 值一致。

提交标题：`控件：新增跨平台快捷键录制器`

---

## 7. 任务五：QColorDialog 评估与 ZzColorPicker

### 7.1 QColorDialog 评估结论

Qt 6.8+ 的 `QColorDialog` 公共 API 只暴露 currentColor、options、standard/custom colors 和窗口结果；非原生实现的 HSV field、luminance picker、swatch grid、preview 和 layout 均属于 Qt private 实现，自绘区域不经过可稳定识别的公开 `QStyle::ControlElement`。应用 style 可以覆盖内部标准 button/spin box/line edit/combo box，但不能在不包含 private 头、不查内部 objectName、不依赖 Qt minor 布局的前提下统一剩余关键视觉。

因此本批定论：

- 不把 `QColorDialog` 声称为完整 Fluent 组件。
- 不新增只转发 `QColorDialog` 的 `ZzColorDialog` 空包装，也不访问 Qt private 子控件。
- 需要平台原生颜色对话框的应用仍可直接使用 `QColorDialog`。
- 需要一致 Fluent 体验的应用使用 `ZzColorPicker`，按需嵌入现有 `ZzContentDialog`；窗口模态、确认/取消和颜色持久化由组合层管理。

### 7.2 新增文件与 API

新建：

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzColorPicker.h`
- `ZzFluentUI/widgets/src/ZzColorPicker.cpp`
- `ZzFluentUI/widgets/src/private/ZzColorPickerPrivate.h`
- `ZzFluentUI/widgets/src/private/ZzColorPickerPrivate.cpp`
- `ZzFluentUI/tests/ZzColorPickerTest.cpp`

公共 API：

```cpp
class ZZ_FLUENT_UI_EXPORT ZzColorPicker final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzColorPicker)
    Q_PROPERTY(
        QColor currentColor
        READ currentColor WRITE setCurrentColor NOTIFY currentColorChanged)
    Q_PROPERTY(
        bool alphaEnabled
        READ isAlphaEnabled WRITE setAlphaEnabled NOTIFY alphaEnabledChanged)
    Q_PROPERTY(
        int paletteColorCount
        READ paletteColorCount NOTIFY paletteColorsChanged)

public:
    explicit ZzColorPicker(QWidget *parent = nullptr);
    ~ZzColorPicker() override;
    [[nodiscard]] QColor currentColor() const noexcept;
    void setCurrentColor(QColor color);
    [[nodiscard]] bool isAlphaEnabled() const noexcept;
    void setAlphaEnabled(bool enabled);
    [[nodiscard]] QList<QColor> paletteColors() const;
    void setPaletteColors(QList<QColor> colors);
    [[nodiscard]] int paletteColorCount() const noexcept;
    void resetPaletteColors();

Q_SIGNALS:
    void currentColorChanged(const QColor &color);
    void alphaEnabledChanged(bool enabled);
    void paletteColorsChanged();

protected:
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzColorPickerPrivate> d_ptr;
};
```

### 7.3 私有模型、输入同步与绘制

- Private 内部类型均以 Zz 开头：`ZzColorPaletteModel`、`ZzColorSwatchDelegate`、`ZzColorPreviewWidget`。它们只在 private cpp 定义，不进入安装头。
- palette model 是颜色列表的唯一集合真值。`setPaletteColors()` 过滤 invalid、按 RGBA 去重并限制最多 256 项；空列表允许，`resetPaletteColors()` 恢复固定、跨主题一致的默认内容色。内容 QColor 不充当主题令牌。
- 固定 QListView 使用 IconMode、LeftToRight flow、wrapping、uniform item size、SingleSelection；单击一次即设置 currentColor，方向键/Space/Enter 沿用 view 语义。delegate 用 `ColorSwatchExtent/Gap`、ControlStroke、FocusStroke 绘制色块和当前项；DisplayRole/accessible name 使用 `#RRGGBB` 或 `#AARRGGBB`。
- 固定 editor 行包含 R/G/B 和预构造 A 四个 `ZzSpinBox`（0..255）及一个 `QLineEdit` hex editor。alphaEnabled=false 时隐藏 A 并使用 `#RRGGBB`；true 时显示 A 并使用 `#AARRGGBB`。`QRegularExpressionValidator` 限制格式，editingFinished 时用 Qt 颜色解析 API提交，无效文本恢复当前值。
- currentColor 是唯一当前值；palette click、spin、hex 和 setter 都进入一个 normalize/apply 函数。内部同步使用 guard + `QSignalBlocker`，一次有效用户操作只发一个 `currentColorChanged`。
- preview 以当前 QColor 覆盖主题 Surface/SurfaceSecondary 棋盘，显示 alpha 而不硬编码白灰；外框和 focus 使用主题令牌。任意颜色不会改变控件文字、边框或主题。
- alphaEnabled=false 不丢弃 currentColor 的 alpha，只隐藏 alpha 编辑并以 RGB 文本展示；RGB 改动保留现有 alpha。切回 true 时原 alpha 可继续编辑。

### 7.4 测试与接线

测试覆盖：默认 palette、invalid/重复/256 上限、空与 reset、setter 幂等、单击一次、键盘导航、RGB/A spin 同步、hex 两种格式、无效编辑恢复、alpha 显隐且不丢值、selection 与自定义非 palette 色、Light/Dark/HighContrast、RTL、List/ListItem 与 SpinBox 无障碍，以及 1000 次 palette reset/currentColor 更新后 model/view/delegate/editor 地址和 QObject 数不变。

提交标题：`控件：新增可组合 Fluent 颜色选择器`

---

## 8. 集成、截图、安装消费与架构

### 8.1 CMake 与公共消费

每个组件提交同步修改：

- `ZzFluentUI/CMakeLists.txt`：source/private source/moc header。
- `ZzFluentUI/tests/CMakeLists.txt`：独立 target、warnings、sanitizers、CTest labels/offscreen。
- `tests/InstallConsumer/Gui/main.cpp`：从安装前缀 include、构造、设值和验证公开契约；shared/static 都必须通过 fresh install consumer。
- `README.md`：按实际已完成组件逐步更新公开清单，批次结束为 37。

安装消费至少覆盖：Password reveal mode；SplitButton 外部 QMenu 借用和 menuRequested；Rating Half 3.5；KeyBinder `Ctrl+Shift+P` round trip；ColorPicker palette、alpha 和 RGBA round trip。

### 8.2 Gallery 与最终集成示例

`examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp` 的 controls/input 区增加五组件：

- PasswordBox 展示 Peek 与 Hidden/Visible mode 切换。
- SplitButton 主区更新本地 label，menu QAction 只更新本地 label。
- Rating 显示 Whole/Half 与 readOnly。
- KeyBinder 把 `recordingAccepted` 文本写入本地 label，不注册系统快捷键。
- ColorPicker 把 currentColor 映射到本地 preview/文本，不写设置或文件。

`examples/ZzPureToolsExample/ZzExampleShowcasePagePrivate.{h,cpp}` 增加输入 showcase 区或复用现有 Feedback 页面中的输入分区，并同步 `translations/ZzPureToolsExample_en.ts`。示例只能组合组件和本地 label，不访问 appcore service。

### 8.3 独立截图场景

`ZzFluentUI/tests/ZzFluentScreenshotTest.cpp` 新增固定尺寸 input-expansion surface：

- PasswordBox：Hidden 和 Visible 终态，Peek 不依赖正在按住的瞬时帧。
- SplitButton：普通、Accent、disabled 终态，menu popup 继续由既有 popup 截图覆盖。
- Rating：Whole、Half 3.5、readOnly/disabled。
- KeyBinder：预设 `Ctrl+Shift+P` 的非录制终态。
- ColorPicker：固定 palette、RGBA 数值和 alphaEnabled 终态。

基线命名 `input-expansion-{light,dark,high-contrast}.png`，每个主题覆盖 DPR 100/125/150/200，共 12 张。截图前不启动 transient menu、timer 或输入录制；文本遮罩沿用现有规则。更新模式重采后必须关闭更新模式逐张比较并人工检查文字/图标无重叠、半星 clip、色块边框、RTL 和高对比焦点。

### 8.4 架构红线

- 新源码零 stylesheet、零平台 native key、零 Qt Private、零每实例 `QProxyStyle`、零业务模块 include。
- 主题视觉不增加 allowlist；颜色选择器的 QColor 是内容数据，框架 surface/stroke/focus/尺寸仍全部走 token。
- paint 不创建 QObject、timer、animation、model、delegate、layout 或逐像素图像；Rating 图标与 ColorPicker 子对象在构造/环境变化路径准备。
- public header 不 include private header；Pimpl private 不安装；新增枚举和类逐头编译。
- SplitButton/Password/Rating/ColorPicker 的内部状态不得通过 dynamic property 或 objectName 反向驱动生产逻辑；objectName 只供测试定位自家固定子控件。

---

## 9. Linux 门禁与平台静态检查

定向开发完成后执行：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr

cmake --preset linux-gcc-reference
cmake --build --preset linux-gcc-reference --parallel 2
ctest --preset linux-gcc-reference -R \
  'fluent\.(password-box|split-button|rating-control|key-binder|color-picker)' \
  --output-on-failure
ctest --preset linux-gcc-reference -R 'fluent.screenshot-' \
  --output-on-failure
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

性能阈值、参考报告和 `gate`/`observe` 策略不得因本批换绿。Windows/macOS 静态检查重点：无 `nativeVirtualKey`、VK_*、XKB、Carbon、Objective-C、Win32 头；QKeySequence 显示文本测试不锁死平台字符串；menu anchor、RTL 和 DPR 只用 Qt 公共逻辑坐标。

---

## 10. 提交顺序与退出条件

1. `文档：细化 FluentUI 第三批输入扩展路线`
2. `控件：新增 Fluent 密码输入框`
3. `控件：新增 Fluent 分割按钮`
4. `控件：新增 Fluent 星级评分`
5. `控件：新增跨平台快捷键录制器`
6. `控件：新增可组合 Fluent 颜色选择器`
7. 必要时独立静态分析修复提交，不夹带功能变化。
8. `文档：完成第三批组件验收与路线状态同步`

每个组件提交必须同时包含自己的 API/Pimpl、独立 QTest、CMake、安装消费、Gallery/最终示例接线和受影响截图；截图场景可由第一个组件建立并随本批后续提交逐步补齐。`temp_image/` 始终不跟踪、不提交。

本批关闭条件：

- 五个公开组件 API、Pimpl、中文 Doxygen、所有权、UI intent 和跨平台边界全部完成。
- 无输入状态复制：Password 只有 QLineEdit text，KeyBinder 只有 QKeySequenceEdit sequence，ColorPicker 只有一个 currentColor 和一个 palette model。
- 单击、键盘、焦点、RTL、HighContrast、DPR、无障碍和对象预算测试通过。
- 12 张独立三主题/四 DPR 基线通过关闭更新模式比较并完成视觉检查。
- Linux reference、Clang shared/static、Clang-Tidy、ASan/UBSan、安装消费、架构和性能门禁全部通过。
- Windows MSVC、Windows MinGW、macOS 的真实验证状态如实记录，未执行不得写成通过。

---

## 11. 实施结果（完成后填写）

- 提交：待实施。
- Linux 测试：待实施。
- 截图：待实施。
- Clang-Tidy：待实施。
- ASan/UBSan：待实施。
- 性能：待实施。
- Windows/macOS：待实施。
