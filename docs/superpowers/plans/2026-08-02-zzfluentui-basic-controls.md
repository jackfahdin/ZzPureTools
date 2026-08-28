# ZzFluentUI 基础控件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `Zz::FluentUI` 中交付可键盘操作、可访问、支持主题与高 DPI 的第一阶段 Qt Widgets 基础控件面。

**Architecture:** 阶段 5 已提供 `ZzThemeController`、不可变 `ZzThemeSnapshot`、`ZzFluentPainter`、`ZzFluentStyle` 和有界图标缓存；本计划只消费这些接口并扩展 Widgets 控件绘制，不重复定义 token、主题解析、DPI 换算或缓存。复选、单选、滑块、进度、文本输入、组合框、菜单、ToolTip、Dialog 和 Tab 直接保留 Qt 控件的原生语义，由 `ZzFluentStyle` 统一绘制；只有 Qt 缺少等价交互语义的控件才增加四文件 PIMPL 的 Zz 类型。

**Tech Stack:** Qt 6.8+ Core/Gui/Widgets/Svg/Test、C++20、CMake 3.23、CTest、Qt Test、`Zz::FluentFoundation`、`Zz::FluentUI`。

---

## 前置条件与执行边界

- 工作目录固定为 `/home/zz/Jackfahdin/github/ZzPureToolsFrame/ZzPureToolsFrame`。
- 先完成 `docs/superpowers/plans/2026-08-02-repository-cmake-baseline.md` 与阶段 5 的 FluentFoundation 计划。
- `Zz::FluentUI` 已公开 `ZzFluentStyle`、`ZzFluentPainter`；`ZzFluentStyle` 由应用显式安装并持有非空 `ZzThemeController`。
- 本计划不修改 Foundation token 值、主题模式解析、图标缓存 key/淘汰算法、资源许可清单或 `ZzThemeController` 生命周期。
- 本计划不实现 Qt Quick、日历、复杂卡片、可撕标签页、页面路由、业务 ViewModel、QWindowKit 连接或窗口命令执行。
- UI 代码只接收 Qt 展示类型、`QAbstractItemModel` 与 Foundation 公共类型；禁止包含 repository、database、network、domain、QWK、Qt Private 或第三方实现头。
- 公开有状态 QObject/QWidget 使用 `std::unique_ptr<...Private>`，析构函数在 `.cpp` 定义，public 与 private 对象之间不得形成 QObject parent 和智能指针双重所有权。
- 所有自定义类型使用 `Zz` 前缀，C++ namespace 只写 `namespace ZzFluentUI { ... }`，公共 API 使用简体中文 Doxygen。
- 逻辑尺寸均使用 Qt 的设备无关坐标；渲染图标时把实际 DPR 传给阶段 5 的缓存接口，不在 paint 路径解析 SVG、读取文件、加锁或构造临时容器。
- 主题颜色变化只触发 `update()`；字体或 metric 变化才允许 `updateGeometry()`。动画对象构造一次并复用，`SH_Widget_Animate == 0` 时立即到达终态。

## 公共控件策略

| 第一阶段能力 | 对外类型 | 责任边界 |
|---|---|---|
| 文字 | `QLabel` | 由 palette、typography 与 style 提供 Fluent 外观，不增加空包装类 |
| 普通/强调按钮 | `ZzPushButton` | 增加 `ZzButtonAppearance`，其余点击、默认按钮和快捷键语义继承 `QPushButton` |
| 图标按钮 | `ZzIconButton` | 接收 Foundation 的 `ZzIconDescriptor`，通过 style 有界缓存取得 DPR pixmap |
| 开关 | `ZzToggleSwitch` | 继承 `QCheckBox` 以复用 Space、焦点与屏幕阅读器 CheckBox 语义 |
| 复选/单选 | `QCheckBox`、`QRadioButton` | `ZzFluentStyle` 覆盖 indicator、focus、disabled 与高对比度绘制 |
| 滑块/进度 | `QSlider`、`QProgressBar` | 保留 Qt range/value API，style 覆盖 groove、handle、chunk 与 busy 状态 |
| 文本输入 | `QLineEdit`、`QTextEdit` | 保留输入法、选择、撤销与平台快捷键，style 覆盖 frame、focus 与 placeholder palette |
| 组合框/菜单/提示 | `QComboBox`、`QMenu`、`QToolTip` | 保留 popup、Escape、方向键、助记键和平台菜单语义 |
| Dialog/Message | `QDialog`、`ZzMessageBar` | Dialog 保留 Qt modality；MessageBar 只展示文本、严重性与关闭意图 |
| Navigation/Tab/Breadcrumb | `ZzNavigationView`、`QTabBar`、`ZzBreadcrumbBar` | Navigation 只消费 `QAbstractItemModel`；Tab 保留 Qt 语义；Breadcrumb 发出索引意图 |
| List/Table/Tree | `ZzFluentItemDelegate` | 只按 `QStyleOptionViewItem` 绘制，不访问领域对象，不按模型总行数分配 |
| 标题栏视觉 | `ZzFluentTitleBar` | 只布局、绘制和发出窗口意图，不包含 ZzWindowKit/QWK 调用 |

## 文件结构与责任

### 样式与标准 Qt 控件

- Modify: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h` — 声明 complex control、sub-control 与 style hint 覆写。
- Modify: `ZzFluentUI/widgets/src/ZzFluentStyle.cpp` — 分派标准基础控件的 Fluent 绘制。
- Modify: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h` — 声明无分配绘制辅助函数。
- Modify: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp` — 使用 palette 和 Foundation metric 执行绘制。
- Create: `ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp` — 覆盖复选、单选、滑块、进度、输入、组合框、菜单、ToolTip、Dialog 和 Tab。

### 自定义公共控件

- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzButtonAppearance.h` — 普通、强调、轻量按钮外观值类型。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzPushButton.h`
- Create: `ZzFluentUI/widgets/src/ZzPushButton.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzPushButtonPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzPushButtonPrivate.cpp` — 按钮外观状态与 option palette 组装。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzIconButton.h`
- Create: `ZzFluentUI/widgets/src/ZzIconButton.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzIconButtonPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzIconButtonPrivate.cpp` — 图标描述、DPR 和主题 revision 刷新。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzToggleSwitch.h`
- Create: `ZzFluentUI/widgets/src/ZzToggleSwitch.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzToggleSwitchPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzToggleSwitchPrivate.cpp` — 可复用切换动画与轨道/滑块绘制。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzMessageSeverity.h` — 消息展示严重性值类型。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzMessageBar.h`
- Create: `ZzFluentUI/widgets/src/ZzMessageBar.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzMessageBarPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzMessageBarPrivate.cpp` — 标签、关闭按钮和无障碍文本同步。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzNavigationView.h`
- Create: `ZzFluentUI/widgets/src/ZzNavigationView.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzNavigationViewPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzNavigationViewPrivate.cpp` — 默认 delegate 与键盘激活策略。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzBreadcrumbBar.h`
- Create: `ZzFluentUI/widgets/src/ZzBreadcrumbBar.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzBreadcrumbBarPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzBreadcrumbBarPrivate.cpp` — 动态文本、RTL 顺序与索引意图。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzItemDensity.h` — 标准/紧凑 delegate 密度值类型。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentItemDelegate.h`
- Create: `ZzFluentUI/widgets/src/ZzFluentItemDelegate.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentItemDelegatePrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentItemDelegatePrivate.cpp` — item option 标准化与局部绘制。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentTitleBar.h`
- Create: `ZzFluentUI/widgets/src/ZzFluentTitleBar.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp` — 标题、图标、系统按钮布局与意图转发。

### 测试、基准、示例与门禁

- Modify: `ZzFluentUI/CMakeLists.txt` — 把控件源码加入 `ZzFluentUI` target，并在 `ZZ_BUILD_TESTS` 时加入测试目录。
- Modify: `ZzFluentUI/tests/CMakeLists.txt` — 注册 unit/component/accessibility/screenshot/benchmark 测试。
- Create: `ZzFluentUI/tests/ZzButtonControlsTest.cpp`
- Create: `ZzFluentUI/tests/ZzToggleSwitchTest.cpp`
- Create: `ZzFluentUI/tests/ZzMessageBarTest.cpp`
- Create: `ZzFluentUI/tests/ZzNavigationControlsTest.cpp`
- Create: `ZzFluentUI/tests/ZzFluentItemDelegateTest.cpp`
- Create: `ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`
- Create: `ZzFluentUI/tests/ZzFluentAccessibilityTest.cpp`
- Create: `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`
- Create: `ZzFluentUI/tests/ZzBasicControlsBenchmark.cpp`
- Create: `ZzFluentUI/tests/private/ZzControlTestTheme.h` — 只构造测试应用级 controller/style。
- Create: `examples/ZzFluentControlsGallery/CMakeLists.txt`
- Create: `examples/ZzFluentControlsGallery/main.cpp`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGallery.h`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGallery.cpp`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.h`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp` — 无业务逻辑的全控件视觉面。
- Create: `tests/Architecture/CheckZzFluentUIBoundaries.cmake` — 扫描公共 API、私有依赖和 UI/业务隔离。
- Modify: `tests/Architecture/CMakeLists.txt` — 注册边界检查。
- Modify: `tests/InstallConsumer/main.cpp` — 从安装包只包含并构造公开控件。

## Task 1: 扩展 ZzFluentStyle 的标准控件绘制面

**Files:**
- Modify: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h`
- Modify: `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`
- Modify: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h`
- Modify: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`
- Create: `ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`

- [ ] **Step 1: 写标准控件失败测试**

Create `ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp`:

```cpp
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QProxyStyle>

#include <QtGui/QAction>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QTabBar>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

class ZzFluentStandardControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesStableLogicalMetrics()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller, nullptr);

        QCOMPARE(style.pixelMetric(QStyle::PM_IndicatorWidth), 18);
        QCOMPARE(style.pixelMetric(QStyle::PM_IndicatorHeight), 18);
        QCOMPARE(style.pixelMetric(QStyle::PM_SliderLength), 20);
        QCOMPARE(style.pixelMetric(QStyle::PM_TabBarTabHSpace), 24);
        QCOMPARE(style.styleHint(QStyle::SH_Menu_SubMenuPopupDelay), 200);
    }

    void preservesKeyboardSemantics()
    {
        QWidget host;
        QCheckBox checkBox(QStringLiteral("Check"), &host);
        QRadioButton radioButton(QStringLiteral("Radio"), &host);
        QSlider slider(Qt::Horizontal, &host);
        QLineEdit lineEdit(&host);
        QComboBox comboBox(&host);
        comboBox.addItems({QStringLiteral("A"), QStringLiteral("B")});

        checkBox.setFocus();
        QTest::keyClick(&checkBox, Qt::Key_Space);
        QVERIFY(checkBox.isChecked());

        radioButton.setFocus();
        QTest::keyClick(&radioButton, Qt::Key_Space);
        QVERIFY(radioButton.isChecked());

        slider.setRange(0, 10);
        slider.setValue(5);
        slider.setFocus();
        QTest::keyClick(&slider, Qt::Key_Right);
        QCOMPARE(slider.value(), 6);

        lineEdit.setFocus();
        QTest::keyClicks(&lineEdit, QStringLiteral("text"));
        QCOMPARE(lineEdit.text(), QStringLiteral("text"));

        comboBox.setCurrentIndex(0);
        comboBox.setFocus();
        QTest::keyClick(&comboBox, Qt::Key_Down);
        QCOMPARE(comboBox.currentIndex(), 1);
    }

    void drawsProgressAndPopupControls()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller, nullptr);

        QProgressBar progress;
        progress.setStyle(&style);
        progress.setRange(0, 100);
        progress.setValue(50);
        progress.resize(200, 24);

        QImage image(progress.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        progress.render(&painter);
        painter.end();
        QVERIFY(image.pixelColor(50, 12).alpha() > 0);

        QMenu menu;
        menu.setStyle(&style);
        QAction *action = menu.addAction(QStringLiteral("Open"));
        QVERIFY(action != nullptr);

        QDialog dialog;
        dialog.setStyle(&style);
        QTabBar tabs(&dialog);
        tabs.addTab(QStringLiteral("One"));
        tabs.addTab(QStringLiteral("Two"));
        QCOMPARE(tabs.count(), 2);
    }

    void drawsEveryPromisedFluentSurface();

    void respectsProgressDirectionAndPartialCheckState()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller, nullptr);
        QPalette palette;
        palette.setColor(QPalette::Mid, QColor(Qt::red));
        palette.setColor(QPalette::Highlight, QColor(Qt::green));

        QStyleOptionProgressBar progress;
        progress.rect = QRect(0, 0, 100, 12);
        progress.minimum = 0;
        progress.maximum = 100;
        progress.progress = 25;
        progress.state = QStyle::State_Enabled | QStyle::State_Horizontal;
        progress.direction = Qt::RightToLeft;
        progress.palette = palette;
        QImage image(progress.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        style.drawControl(QStyle::CE_ProgressBar, &progress, &painter);
        painter.end();
        QCOMPARE(image.pixelColor(90, 6), QColor(Qt::green));
        QCOMPARE(image.pixelColor(10, 6), QColor(Qt::red));

        QStyleOption check;
        check.rect = QRect(0, 0, 18, 18);
        check.state = QStyle::State_Enabled | QStyle::State_NoChange;
        check.palette = palette;
        image = QImage(check.rect.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        painter.begin(&image);
        style.drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, &painter);
        painter.end();
        QCOMPARE(image.pixelColor(9, 4), QColor(Qt::green));
    }
};

QTEST_MAIN(ZzFluentStandardControlsTest)

#include "ZzFluentStandardControlsTest.moc"
```

- [ ] **Step 2: 注册并运行红灯测试**

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzFluentStandardControlsTest
    ZzFluentStandardControlsTest.cpp
)
target_link_libraries(ZzFluentStandardControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentStandardControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentStandardControlsTest)
zz_enable_sanitizers(ZzFluentStandardControlsTest)
add_test(NAME fluent.standard-controls COMMAND ZzFluentStandardControlsTest)
set_tests_properties(fluent.standard-controls PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzFluentStandardControlsTest
ctest --preset linux-gcc-debug -R '^fluent.standard-controls$' --output-on-failure
```

Expected: build succeeds；CTest FAIL，`exposesStableLogicalMetrics()` reports at least one native base-style value different from `18/20/24/200`。

- [ ] **Step 3: 扩展完整 style 覆写面**

阶段 5 已经在 `ZzFluentStyle` 的 public section 声明 `pixelMetric()`、`styleHint()` 和 `drawPrimitive()`；先核对它们的签名与本任务后续定义一致，并保持每个成员只有一条声明，不得再次粘贴造成重复声明。只向 `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h` 增加以下三个新 override：

```cpp
void drawControl(
    ControlElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget = nullptr) const override;

void drawComplexControl(
    ComplexControl control,
    const QStyleOptionComplex *option,
    QPainter *painter,
    const QWidget *widget = nullptr) const override;

[[nodiscard]] QRect subControlRect(
    ComplexControl control,
    const QStyleOptionComplex *option,
    SubControl subControl,
    const QWidget *widget = nullptr) const override;
```

新增签名只额外需要 `QStyleOptionComplex`；若 Qt public 头尚未提供其声明，在 `namespace ZzFluentUI` 之前增加：

```cpp
class QStyleOptionComplex;
```

- [ ] **Step 4: 声明 private 无分配 painter**

Add to `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h`:

```cpp
void drawCheckIndicator(
    const QStyleOption *option,
    QPainter *painter,
    bool radio) const;
void drawPushButton(
    const QStyleOptionButton *option,
    QPainter *painter,
    const QWidget *widget) const;
void drawInputPanel(
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const;
void drawComboBox(
    const QStyleOptionComboBox *option,
    QPainter *painter,
    const QWidget *widget) const;
void drawTabBarTab(
    const QStyleOptionTab *option,
    QPainter *painter,
    const QWidget *widget) const;
void drawToolTipPanel(
    const QStyleOption *option,
    QPainter *painter) const;
void drawProgressBar(
    const QStyleOptionProgressBar *option,
    QPainter *painter,
    const QWidget *widget) const;
void drawSlider(
    const QStyleOptionSlider *option,
    QPainter *painter,
    const QWidget *widget) const;
void drawMenuItem(
    const QStyleOptionMenuItem *option,
    QPainter *painter,
    const QWidget *widget) const;
```

Add these Qt public includes to that private header:

```cpp
#include <QtWidgets/QStyleOption>
```

- [ ] **Step 5: 实现标准控件 metric 与绘制分派**

在 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp` 中替换阶段 5 已存在的 `pixelMetric()`、`styleHint()` 和 `drawPrimitive()` 三个函数体，再新增 `drawControl()`、`drawComplexControl()` 和 `subControlRect()`；`standardPalette()` 及其他阶段 5 函数保持不变。最终每个成员只能有一个 out-of-line 定义。使用以下完整函数体，未处理分支继续落到 `QProxyStyle`：

```cpp
int ZzFluentStyle::pixelMetric(
    PixelMetric metric,
    const QStyleOption *option,
    const QWidget *widget) const
{
    switch (metric) {
    case PM_ButtonMargin:
        return qRound(d_ptr->snapshot->metric(
            ZzMetricToken::HorizontalPadding));
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
        return 18;
    case PM_SliderLength:
        return 20;
    case PM_SliderThickness:
        return 4;
    case PM_ProgressBarChunkWidth:
        return 1;
    case PM_TabBarTabHSpace:
        return 24;
    case PM_TabBarTabVSpace:
        return 12;
    case PM_FocusFrameHMargin:
    case PM_FocusFrameVMargin:
        return 2;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int ZzFluentStyle::styleHint(
    StyleHint hint,
    const QStyleOption *option,
    const QWidget *widget,
    QStyleHintReturn *returnData) const
{
    if (hint == SH_Menu_SubMenuPopupDelay) {
        return 200;
    }
    if (hint == SH_Widget_Animate) {
        if (d_ptr->snapshot != nullptr && d_ptr->snapshot->reducedMotion()) {
            return 0;
        }
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void ZzFluentStyle::drawPrimitive(
    PrimitiveElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    if ((element == PE_IndicatorCheckBox || element == PE_IndicatorRadioButton)
        && option != nullptr && painter != nullptr) {
        d_ptr->drawCheckIndicator(option, painter, element == PE_IndicatorRadioButton);
        return;
    }
    if ((element == PE_PanelLineEdit || element == PE_FrameLineEdit)
        && option != nullptr && painter != nullptr) {
        d_ptr->drawInputPanel(option, painter, widget);
        return;
    }
    if (element == PE_PanelTipLabel && option != nullptr && painter != nullptr) {
        d_ptr->drawToolTipPanel(option, painter);
        return;
    }
    if (element == PE_FrameFocusRect && option != nullptr && painter != nullptr) {
        const qreal dpr = widget != nullptr ? widget->devicePixelRatioF() : 1.0;
        ZzFluentPainter::drawFocusRing(
            painter, option->rect, *d_ptr->snapshot, dpr);
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void ZzFluentStyle::drawControl(
    ControlElement element,
    const QStyleOption *option,
    QPainter *painter,
    const QWidget *widget) const
{
    if (element == CE_PushButton) {
        const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
        if (button != nullptr && painter != nullptr) {
            d_ptr->drawPushButton(button, painter, widget);
            return;
        }
    }
    if (element == CE_ProgressBar) {
        const auto *progress = qstyleoption_cast<const QStyleOptionProgressBar *>(option);
        if (progress != nullptr && painter != nullptr) {
            d_ptr->drawProgressBar(progress, painter, widget);
            return;
        }
    }
    if (element == CE_TabBarTab) {
        const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
        if (tab != nullptr && painter != nullptr) {
            d_ptr->drawTabBarTab(tab, painter, widget);
            return;
        }
    }
    if (element == CE_MenuItem) {
        const auto *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (menuItem != nullptr && painter != nullptr) {
            d_ptr->drawMenuItem(menuItem, painter, widget);
            return;
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void ZzFluentStyle::drawComplexControl(
    ComplexControl control,
    const QStyleOptionComplex *option,
    QPainter *painter,
    const QWidget *widget) const
{
    if (control == CC_Slider) {
        const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (slider != nullptr && painter != nullptr) {
            d_ptr->drawSlider(slider, painter, widget);
            return;
        }
    }
    if (control == CC_ComboBox) {
        const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option);
        if (combo != nullptr && painter != nullptr) {
            d_ptr->drawComboBox(combo, painter, widget);
            return;
        }
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}

QRect ZzFluentStyle::subControlRect(
    ComplexControl control,
    const QStyleOptionComplex *option,
    SubControl subControl,
    const QWidget *widget) const
{
    QRect result = QProxyStyle::subControlRect(control, option, subControl, widget);
    if (control == CC_Slider && subControl == SC_SliderHandle) {
        const int length = pixelMetric(PM_SliderLength, option, widget);
        const QPoint center = result.center();
        result.setSize(QSize(length, length));
        result.moveCenter(center);
    }
    return result;
}
```

- [ ] **Step 6: 实现 private painter 的最小完整逻辑**

Add the following definitions to `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`:

```cpp
void ZzFluentStylePrivate::drawCheckIndicator(
    const QStyleOption *option,
    QPainter *painter,
    bool radio) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool enabled = option->state.testFlag(QStyle::State_Enabled);
    const bool checked = option->state.testFlag(QStyle::State_On);
    const bool mixed = option->state.testFlag(QStyle::State_NoChange);
    const bool marked = checked || mixed;
    const QColor border = enabled
        ? option->palette.color(QPalette::Text)
        : option->palette.color(QPalette::Disabled, QPalette::Text);
    const QColor fill = marked
        ? option->palette.color(QPalette::Highlight)
        : option->palette.color(QPalette::Base);
    const QRectF rect = QRectF(option->rect).adjusted(1.0, 1.0, -1.0, -1.0);
    painter->setPen(QPen(border, 1.0));
    painter->setBrush(fill);
    if (radio) {
        painter->drawEllipse(rect);
    } else {
        painter->drawRoundedRect(rect, 3.0, 3.0);
    }
    if (marked) {
        painter->setPen(QPen(option->palette.color(QPalette::HighlightedText), 2.0));
        if (mixed) {
            painter->drawLine(
                QPointF(rect.left() + 4.0, rect.center().y()),
                QPointF(rect.right() - 4.0, rect.center().y()));
        } else if (radio) {
            painter->setBrush(option->palette.color(QPalette::HighlightedText));
            painter->drawEllipse(rect.center(), 4.0, 4.0);
        } else {
            QPainterPath path;
            path.moveTo(rect.left() + 4.0, rect.center().y());
            path.lineTo(rect.center().x() - 1.0, rect.bottom() - 4.0);
            path.lineTo(rect.right() - 3.0, rect.top() + 4.0);
            painter->drawPath(path);
        }
    }
    painter->restore();
}

void ZzFluentStylePrivate::drawProgressBar(
    const QStyleOptionProgressBar *option,
    QPainter *painter,
    const QWidget *widget) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF groove = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->setPen(Qt::NoPen);
    painter->setBrush(option->palette.color(QPalette::Mid));
    painter->drawRoundedRect(groove, 2.0, 2.0);

    const qint64 span = qint64(option->maximum) - qint64(option->minimum);
    const qreal ratio = span > 0
        ? std::clamp(
              qreal(qint64(option->progress) - qint64(option->minimum))
                  / qreal(span),
              qreal(0.0),
              qreal(1.0))
        : qreal(0.0);
    QRectF chunk = groove;
    const bool horizontal = option->state.testFlag(QStyle::State_Horizontal);
    if (horizontal) {
        chunk.setWidth(groove.width() * ratio);
        const bool fromRight = option->invertedAppearance
            != (option->direction == Qt::RightToLeft);
        if (fromRight) {
            chunk.moveRight(groove.right());
        }
    } else {
        chunk.setHeight(groove.height() * ratio);
        if (!option->invertedAppearance) {
            chunk.moveBottom(groove.bottom());
        }
    }
    painter->setBrush(option->palette.color(QPalette::Highlight));
    painter->drawRoundedRect(chunk, 2.0, 2.0);
    painter->restore();

    if (option->textVisible) {
        q_ptr->QProxyStyle::drawControl(
            QStyle::CE_ProgressBarLabel, option, painter, widget);
    }
}

void ZzFluentStylePrivate::drawSlider(
    const QStyleOptionSlider *option,
    QPainter *painter,
    const QWidget *widget) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRect groove = q_ptr->subControlRect(
        QStyle::CC_Slider, option, QStyle::SC_SliderGroove, widget);
    const QRect handle = q_ptr->subControlRect(
        QStyle::CC_Slider, option, QStyle::SC_SliderHandle, widget);
    painter->setPen(Qt::NoPen);
    painter->setBrush(option->palette.color(QPalette::Mid));
    painter->drawRoundedRect(QRectF(groove), 2.0, 2.0);
    QRectF active = groove;
    if (option->orientation == Qt::Horizontal) {
        if (option->upsideDown) {
            active.setLeft(handle.center().x());
        } else {
            active.setRight(handle.center().x());
        }
    } else if (option->upsideDown) {
        active.setTop(handle.center().y());
    } else {
        active.setBottom(handle.center().y());
    }
    painter->setBrush(option->palette.color(QPalette::Highlight));
    painter->drawRoundedRect(active, 2.0, 2.0);
    painter->setBrush(option->palette.color(QPalette::Highlight));
    painter->drawEllipse(QRectF(handle));
    if (option->state.testFlag(QStyle::State_HasFocus)) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(option->palette.color(QPalette::Highlight), 2.0));
        painter->drawEllipse(QRectF(handle).adjusted(-2.0, -2.0, 2.0, 2.0));
    }
    painter->restore();
}

void ZzFluentStylePrivate::drawMenuItem(
    const QStyleOptionMenuItem *option,
    QPainter *painter,
    const QWidget *widget) const
{
    QStyleOptionMenuItem adjusted = *option;
    if (adjusted.state.testFlag(QStyle::State_Selected)) {
        painter->fillRect(adjusted.rect, adjusted.palette.color(QPalette::Highlight));
        adjusted.palette.setColor(
            QPalette::Text,
            adjusted.palette.color(QPalette::HighlightedText));
    }
    q_ptr->QProxyStyle::drawControl(QStyle::CE_MenuItem, &adjusted, painter, widget);
}
```

在同一步实现新增的五个 helper，并扩展 progress helper；这是本任务承诺的完整视觉面，不得留给原生 base style：

- `drawPushButton()` 按 disabled/pressed/hover/default/checked 从不可变 snapshot 选择 fill/stroke，以 token radius 绘制背景，再只调用限定名 `q_ptr->QProxyStyle::drawControl(CE_PushButtonLabel, ...)` 绘制原生文字、图标和助记键；焦点环仍由 `PE_FrameFocusRect` 统一处理。
- `drawInputPanel()` 为 `QLineEdit`、`QTextEdit` 的 panel/frame 绘制 Surface/Base、ControlStroke 和 focus/disabled 状态，不绘制文本、selection、cursor 或 input-method 内容；这些语义继续由 Qt 控件负责。
- `drawComboBox()` 绘制同一输入 panel、用 `subControlRect(CC_ComboBox, ..., SC_ComboBoxArrow)` 绘制方向安全的箭头，再限定调用 base style 的 `CE_ComboBoxLabel`；popup、Escape、方向键和 editable line edit 不自行实现。
- `drawTabBarTab()` 根据 selected/hover/disabled 绘制 tab surface 与底部 accent indicator，再限定调用 base style 的 `CE_TabBarTabLabel`；使用 option direction，不重排逻辑 index。
- `drawToolTipPanel()` 只绘制高对比度安全的 SurfaceSecondary 与 stroke；tooltip 文字、换行和计时仍由 Qt 负责。
- `drawProgressBar()` 对 determinate 保留现有 RTL/invertedAppearance 比例逻辑；对 `minimum==0 && maximum==0` 绘制固定三分之一长度的 indeterminate accent segment。reduced-motion 下 segment 固定居中；允许动画时只消费 Qt 已提供的 style option/update 周期计算相位，不创建 timer、动画、容器或全局状态。两种分支都用 `CE_ProgressBarLabel` 绘制可选文本，文字矩形不得被 chunk 裁切。

`drawsEveryPromisedFluentSurface()` 使用带红/绿/蓝哨兵色的 `QStyleOptionButton`、`QStyleOptionFrame`、`QStyleOptionComboBox`、`QStyleOptionTab`、`QStyleOptionProgressBar(minimum=maximum=0)` 和 tooltip option 分别渲染到透明 `QImage`，逐项断言背景、边框、accent/arrow 的已知采样点与 snapshot/palette 一致，并断言 disabled、focus、RTL 各至少一个分支。另创建真实 `QLineEdit`、`QTextEdit`、`QComboBox`、`QTabBar`、`QPushButton`、busy `QProgressBar` 和 `QToolTip` 展示 fixture，确认每个控件的实际 style 是 `ZzFluentStyle`、渲染非空且键盘语义不变。这样不能只靠“对象存在”让未实现视觉分派通过。

Add the required standard and Qt public includes at the top of the same `.cpp`:

```cpp
#include <algorithm>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QWidget>
```

- [ ] **Step 7: 运行标准控件绿灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStandardControlsTest
ctest --preset linux-gcc-debug -R '^fluent.standard-controls$' --output-on-failure
```

Expected: PASS，`5 passed, 0 failed`；承诺的标准控件视觉均有像素级断言，键盘行为仍由 Qt 原生控件提供。

- [ ] **Step 8: 提交标准控件样式面**

```bash
git add ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h \
    ZzFluentUI/widgets/src/ZzFluentStyle.cpp \
    ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h \
    ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp \
    ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp \
    ZzFluentUI/tests/CMakeLists.txt
git commit -m "控件：扩展标准 Qt 控件的 Fluent 样式" \
    -m "覆盖选择、滑块、进度、输入、菜单和标签页绘制。" \
    -m "保留 Qt 的键盘、输入法、弹窗与无障碍语义。"
```

## Task 2: 实现普通按钮与图标按钮

**Files:**
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzButtonAppearance.h`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzPushButton.h`
- Create: `ZzFluentUI/widgets/src/ZzPushButton.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzPushButtonPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzPushButtonPrivate.cpp`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzIconButton.h`
- Create: `ZzFluentUI/widgets/src/ZzIconButton.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzIconButtonPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzIconButtonPrivate.cpp`
- Create: `ZzFluentUI/tests/ZzButtonControlsTest.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`

- [ ] **Step 1: 写按钮公共契约失败测试**

Create `ZzFluentUI/tests/ZzButtonControlsTest.cpp`:

```cpp
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzPushButton.h>

class ZzButtonControlsTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pushButtonPreservesQtActivation()
    {
        ZzFluentUI::ZzPushButton button(QStringLiteral("Apply"));
        button.setAppearance(ZzFluentUI::ZzButtonAppearance::Accent);
        button.setAccessibleName(QStringLiteral("Apply changes"));
        button.show();
        QVERIFY(QTest::qWaitForWindowExposed(&button));

        QSignalSpy clickedSpy(&button, &QPushButton::clicked);
        button.setFocus();
        QTest::keyClick(&button, Qt::Key_Space);

        QCOMPARE(clickedSpy.count(), 1);
        QCOMPARE(button.appearance(), ZzFluentUI::ZzButtonAppearance::Accent);
        QCOMPARE(button.accessibleName(), QStringLiteral("Apply changes"));
    }

    void iconButtonUsesStableToolButtonSemantics()
    {
        ZzFluentUI::ZzIconButton button;
        button.setAccessibleName(QStringLiteral("Refresh"));
        button.resize(32, 32);
        button.show();
        QVERIFY(QTest::qWaitForWindowExposed(&button));

        QSignalSpy clickedSpy(&button, &QToolButton::clicked);
        QTest::mouseClick(&button, Qt::LeftButton);

        QCOMPARE(clickedSpy.count(), 1);
        QVERIFY(button.autoRaise());
        QCOMPARE(button.toolButtonStyle(), Qt::ToolButtonIconOnly);
        QCOMPARE(button.accessibleName(), QStringLiteral("Refresh"));
    }
};

QTEST_MAIN(ZzButtonControlsTest)

#include "ZzButtonControlsTest.moc"
```

- [ ] **Step 2: 注册并验证缺少按钮类型的红灯**

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzButtonControlsTest ZzButtonControlsTest.cpp)
target_link_libraries(ZzButtonControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzButtonControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzButtonControlsTest)
zz_enable_sanitizers(ZzButtonControlsTest)
add_test(NAME fluent.buttons COMMAND ZzButtonControlsTest)
set_tests_properties(fluent.buttons PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzButtonControlsTest
```

Expected: FAIL，compiler reports `ZzFluentUI/ZzButtonAppearance.h: No such file or directory`。

- [ ] **Step 3: 定义按钮外观值类型与 ZzPushButton 公共头**

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzButtonAppearance.h`:

```cpp
#pragma once

#include <cstdint>

namespace ZzFluentUI {

/**
 * @brief 描述按钮的视觉强调级别。
 */
enum class ZzButtonAppearance : std::uint8_t
{
    Standard,
    Accent,
    Subtle
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzPushButton.h`:

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QPushButton>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzPushButtonPrivate;

/**
 * @brief 提供 Fluent 外观级别并保留 QPushButton 原生交互语义。
 *
 * 控件必须在 GUI 线程创建和使用，由 QObject parent 所有；控件不持有业务对象。
 */
class ZZ_FLUENT_UI_EXPORT ZzPushButton final : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(ZzFluentUI::ZzButtonAppearance appearance READ appearance WRITE setAppearance)
    Q_DISABLE_COPY_MOVE(ZzPushButton)

public:
    /**
     * @brief 创建无文本按钮。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzPushButton(QWidget *parent = nullptr);

    /**
     * @brief 创建显示指定文本的按钮。
     * @param text 可本地化的按钮文本。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzPushButton(const QString &text, QWidget *parent = nullptr);

    ~ZzPushButton() override;

    /**
     * @brief 返回当前视觉强调级别。
     * @return 当前按钮外观。
     */
    [[nodiscard]] ZzButtonAppearance appearance() const noexcept;

    /**
     * @brief 更新视觉强调级别而不改变按钮业务状态。
     * @param appearance 新外观。
     */
    void setAppearance(ZzButtonAppearance appearance);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    friend class ZzPushButtonPrivate;
    std::unique_ptr<ZzPushButtonPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

- [ ] **Step 4: 实现 ZzPushButton 四文件 PIMPL**

Create `ZzFluentUI/widgets/src/private/ZzPushButtonPrivate.h`:

```cpp
#pragma once

#include <QtWidgets/QStyleOptionButton>

#include <ZzFluentUI/ZzButtonAppearance.h>

namespace ZzFluentUI {

class ZzPushButton;

class ZzPushButtonPrivate final
{
public:
    explicit ZzPushButtonPrivate(ZzPushButton *publicObject) noexcept;

    void initStyleOption(QStyleOptionButton *option) const;

    ZzPushButton *q_ptr = nullptr;
    ZzButtonAppearance appearance = ZzButtonAppearance::Standard;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/src/private/ZzPushButtonPrivate.cpp`:

```cpp
#include "ZzPushButtonPrivate.h"

#include <ZzFluentUI/ZzPushButton.h>

namespace ZzFluentUI {

ZzPushButtonPrivate::ZzPushButtonPrivate(ZzPushButton *publicObject) noexcept
    : q_ptr(publicObject)
{
}

void ZzPushButtonPrivate::initStyleOption(QStyleOptionButton *option) const
{
    q_ptr->initStyleOption(option);
    if (appearance == ZzButtonAppearance::Accent) {
        option->palette.setColor(
            QPalette::Button,
            option->palette.color(QPalette::Highlight));
        option->palette.setColor(
            QPalette::ButtonText,
            option->palette.color(QPalette::HighlightedText));
    } else if (appearance == ZzButtonAppearance::Subtle) {
        QColor fill = option->palette.color(QPalette::Button);
        fill.setAlpha(0);
        option->palette.setColor(QPalette::Button, fill);
    }
}

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/src/ZzPushButton.cpp`:

```cpp
#include <ZzFluentUI/ZzPushButton.h>

#include <QtWidgets/QStylePainter>

#include "private/ZzPushButtonPrivate.h"

namespace ZzFluentUI {

ZzPushButton::ZzPushButton(QWidget *parent)
    : QPushButton(parent)
    , d_ptr(std::make_unique<ZzPushButtonPrivate>(this))
{
}

ZzPushButton::ZzPushButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent)
    , d_ptr(std::make_unique<ZzPushButtonPrivate>(this))
{
}

ZzPushButton::~ZzPushButton() = default;

ZzButtonAppearance ZzPushButton::appearance() const noexcept
{
    return d_ptr->appearance;
}

void ZzPushButton::setAppearance(ZzButtonAppearance appearance)
{
    if (d_ptr->appearance == appearance) {
        return;
    }
    d_ptr->appearance = appearance;
    update();
}

void ZzPushButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QStyleOptionButton option;
    d_ptr->initStyleOption(&option);
    QStylePainter painter(this);
    painter.drawControl(QStyle::CE_PushButton, option);
}

} // namespace ZzFluentUI
```

- [ ] **Step 5: 定义 ZzIconButton 公共头**

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzIconButton.h`:

```cpp
#pragma once

#include <memory>

#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzIconButtonPrivate;

/**
 * @brief 使用主题图标缓存绘制的仅图标按钮。
 *
 * 调用者必须设置非空 accessibleName；控件在 GUI 线程使用，由 QObject parent 所有。
 */
class ZZ_FLUENT_UI_EXPORT ZzIconButton final : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzIconButton)

public:
    /**
     * @brief 创建尚未绑定图标描述的按钮。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzIconButton(QWidget *parent = nullptr);
    ~ZzIconButton() override;

    /**
     * @brief 设置 Foundation 图标描述并刷新当前 DPR 的缓存图像。
     * @param descriptor 按值复制、不转移所有权的图标描述。
     */
    void setIconDescriptor(const ZzIconDescriptor &descriptor);

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    std::unique_ptr<ZzIconButtonPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

- [ ] **Step 6: 实现 ZzIconButton 四文件 PIMPL**

Create `ZzFluentUI/widgets/src/private/ZzIconButtonPrivate.h`:

```cpp
#pragma once

#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzIconButton;

class ZzIconButtonPrivate final
{
public:
    explicit ZzIconButtonPrivate(ZzIconButton *publicObject) noexcept;

    void refreshIcon();

    ZzIconButton *q_ptr = nullptr;
    ZzIconDescriptor descriptor;
    bool hasDescriptor = false;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/src/private/ZzIconButtonPrivate.cpp`:

```cpp
#include "ZzIconButtonPrivate.h"

#include <algorithm>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconButton.h>

namespace ZzFluentUI {

ZzIconButtonPrivate::ZzIconButtonPrivate(ZzIconButton *publicObject) noexcept
    : q_ptr(publicObject)
{
}

void ZzIconButtonPrivate::refreshIcon()
{
    if (!hasDescriptor) {
        q_ptr->setIcon(QIcon());
        return;
    }
    auto *fluentStyle = qobject_cast<ZzFluentStyle *>(q_ptr->style());
    if (fluentStyle == nullptr) {
        q_ptr->setIcon(QIcon());
        return;
    }
    const int extent = std::max(1, std::min(q_ptr->width(), q_ptr->height()) - 12);
    const QSize logicalSize(extent, extent);
    const qreal dpr = q_ptr->devicePixelRatioF();
    const QColor color = q_ptr->palette().color(QPalette::ButtonText);
    q_ptr->setIcon(QIcon(fluentStyle->iconPixmap(
        descriptor, logicalSize, dpr, color, q_ptr->layoutDirection())));
    q_ptr->setIconSize(logicalSize);
}

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/src/ZzIconButton.cpp`:

```cpp
#include <ZzFluentUI/ZzIconButton.h>

#include <QtCore/QEvent>
#include <QtGui/QResizeEvent>

#include "private/ZzIconButtonPrivate.h"

namespace ZzFluentUI {

ZzIconButton::ZzIconButton(QWidget *parent)
    : QToolButton(parent)
    , d_ptr(std::make_unique<ZzIconButtonPrivate>(this))
{
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setFocusPolicy(Qt::StrongFocus);
}

ZzIconButton::~ZzIconButton() = default;

void ZzIconButton::setIconDescriptor(const ZzIconDescriptor &descriptor)
{
    d_ptr->descriptor = descriptor;
    d_ptr->hasDescriptor = true;
    d_ptr->refreshIcon();
}

void ZzIconButton::changeEvent(QEvent *event)
{
    QToolButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::StyleChange
        || event->type() == QEvent::EnabledChange
        || event->type() == QEvent::DevicePixelRatioChange) {
        d_ptr->refreshIcon();
    }
}

void ZzIconButton::resizeEvent(QResizeEvent *event)
{
    QToolButton::resizeEvent(event);
    d_ptr->refreshIcon();
}

} // namespace ZzFluentUI
```

- [ ] **Step 7: 把按钮源文件加入 ZzFluentUI target**

Add only these translation units to `set(zz_fluent_ui_sources ...)` before `add_library(ZzFluentUI ...)` and `zz_configure_library_target(...)` in `ZzFluentUI/CMakeLists.txt`:

```cmake
    widgets/src/ZzPushButton.cpp
    widgets/src/private/ZzPushButtonPrivate.cpp
    widgets/src/ZzIconButton.cpp
    widgets/src/private/ZzIconButtonPrivate.cpp
```

Replace the separate AUTOMOC-only list with:

```cmake
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
    widgets/include/ZzFluentUI/ZzPushButton.h
    widgets/include/ZzFluentUI/ZzIconButton.h
)
```

Do not put public headers or private headers in `zz_fluent_ui_sources`; the baseline helper forwards only `zz_fluent_ui_sources` to clang-tidy and passes `zz_fluent_ui_moc_headers` to AUTOMOC.

- [ ] **Step 8: 运行按钮绿灯测试**

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzButtonControlsTest
ctest --preset linux-gcc-debug -R '^fluent.buttons$' --output-on-failure
```

Expected: PASS，Space 和鼠标各触发一次 `clicked`；公开头不包含 private 路径。

- [ ] **Step 9: 提交按钮控件**

```bash
git add ZzFluentUI/CMakeLists.txt ZzFluentUI/widgets ZzFluentUI/tests
git commit -m "控件：实现 Fluent 按钮与图标按钮" \
    -m "增加三种按钮外观和主题图标缓存接入。" \
    -m "保留 QPushButton 与 QToolButton 的键盘、焦点和无障碍语义。"
```

## Task 3: 实现可复用动画的 ZzToggleSwitch

**Files:**
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzToggleSwitch.h`
- Create: `ZzFluentUI/widgets/src/ZzToggleSwitch.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzToggleSwitchPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzToggleSwitchPrivate.cpp`
- Create: `ZzFluentUI/tests/ZzToggleSwitchTest.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`

- [ ] **Step 1: 写键盘、无障碍与动画复用失败测试**

Create `ZzFluentUI/tests/ZzToggleSwitchTest.cpp` with:

```cpp
#include <QtGui/QAccessible>
#include <QtCore/QVariantAnimation>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

/** @brief 为动效单元测试提供确定启用动画的基础样式。 */
class ZzAnimationEnabledBaseStyle final : public QProxyStyle
{
public:
    [[nodiscard]] int styleHint(
        StyleHint hint,
        const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr,
        QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == SH_Widget_Animate) {
            return 1;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

class ZzToggleSwitchTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void spaceTogglesExactlyOnce()
    {
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setText(QStringLiteral("Wi-Fi"));
        toggle.show();
        toggle.setFocus();
        QSignalSpy spy(&toggle, &QCheckBox::toggled);
        QTest::keyClick(&toggle, Qt::Key_Space);
        QVERIFY(toggle.isChecked());
        QCOMPARE(spy.count(), 1);
    }

    void exposesCheckBoxAccessibility()
    {
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setAccessibleName(QStringLiteral("Wi-Fi"));
        QAccessibleInterface *interface =
            QAccessible::queryAccessibleInterface(&toggle);
        QVERIFY(interface != nullptr);
        QCOMPARE(interface->role(), QAccessible::CheckBox);
        QCOMPARE(interface->text(QAccessible::Name), QStringLiteral("Wi-Fi"));
    }

    void reusesOneAnimationObject()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller, new ZzAnimationEnabledBaseStyle);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.show();
        QVERIFY(QTest::qWaitForWindowExposed(&toggle));
        const int before = toggle.findChildren<QVariantAnimation *>().size();
        for (int index = 0; index < 20; ++index) {
            toggle.setChecked(!toggle.isChecked());
        }
        QCOMPARE(toggle.findChildren<QVariantAnimation *>().size(), before);
        QCOMPARE(before, 1);
    }

    void stopsRunningAnimationForReducedMotion()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(
            &controller, new ZzAnimationEnabledBaseStyle);
        ZzFluentUI::ZzToggleSwitch toggle;
        toggle.setStyle(&style);
        toggle.show();
        QVERIFY(QTest::qWaitForWindowExposed(&toggle));
        auto *animation = toggle.findChild<QVariantAnimation *>();
        QVERIFY(animation != nullptr);

        toggle.setChecked(true);
        QCOMPARE(animation->state(), QAbstractAnimation::Running);
        controller.setReducedMotion(true);
        QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped);
    }
};

QTEST_MAIN(ZzToggleSwitchTest)

#include "ZzToggleSwitchTest.moc"
```

- [ ] **Step 2: 注册并确认类型缺失**

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzToggleSwitchTest ZzToggleSwitchTest.cpp)
target_link_libraries(ZzToggleSwitchTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzToggleSwitchTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzToggleSwitchTest)
zz_enable_sanitizers(ZzToggleSwitchTest)
add_test(NAME fluent.toggle-switch COMMAND ZzToggleSwitchTest)
set_tests_properties(fluent.toggle-switch PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzToggleSwitchTest
```

Expected: compile FAIL，缺少 `ZzToggleSwitch.h`。

- [ ] **Step 3: 声明四文件公开控件**

Create `ZzToggleSwitch.h` with:

```cpp
#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QCheckBox>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QHideEvent;

namespace ZzFluentUI {

class ZzToggleSwitchPrivate;

/**
 * @brief 保留 QCheckBox 语义的 Fluent 开关。
 *
 * 控件必须在 GUI 线程创建和调用；键盘、焦点和无障碍检查状态
 * 由 QCheckBox 提供，动画只影响呈现。
 */
class ZZ_FLUENT_UI_EXPORT ZzToggleSwitch final : public QCheckBox
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzToggleSwitch)

public:
    explicit ZzToggleSwitch(QWidget *parent = nullptr);
    explicit ZzToggleSwitch(QString text, QWidget *parent = nullptr);
    ~ZzToggleSwitch() override;

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzToggleSwitchPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

- [ ] **Step 4: 实现一个持久动画和无分配 paint**

Private declaration is exact:

```cpp
class ZzToggleSwitchPrivate final
{
public:
    explicit ZzToggleSwitchPrivate(ZzToggleSwitch *q);
    void moveTo(bool checked);
    void finishImmediately() noexcept;

    ZzToggleSwitch *const q_ptr;
    QVariantAnimation *animation = nullptr;
    qreal progress = 0.0;
};
```

构造函数只创建一次 `new QVariantAnimation(q)`，设置 `0.0..1.0`、`QEasingCurve::OutCubic` 和 167 ms。`valueChanged` 每帧先查询当前 style 的 `SH_Widget_Animate`：若已因 reduced-motion 变为 0，则调用 `finishImmediately()` 停止正在运行的动画、把 progress 同步为当前 checked 终态并 update；否则只更新 `progress` 并调用 `q->update()`。`moveTo()` 的完整分支固定为：

```cpp
void ZzToggleSwitchPrivate::moveTo(bool checked)
{
    const qreal target = checked ? 1.0 : 0.0;
    const bool animate = q_ptr->isVisible() && q_ptr->isEnabled()
        && q_ptr->style()->styleHint(QStyle::SH_Widget_Animate,
                                    nullptr, q_ptr) != 0;
    animation->stop();
    if (!animate || qFuzzyCompare(progress, target)) {
        progress = target;
        q_ptr->update();
        return;
    }
    animation->setStartValue(progress);
    animation->setEndValue(target);
    animation->start();
}
```

public 构造函数连接 `QCheckBox::toggled` 到 `moveTo()`，初值取 `isChecked()`；文本构造函数委托默认构造后 `setText(std::move(text))`。`hideEvent()` 先停止动画并同步终态，再调用基类。`changeEvent()` 对 `EnabledChange/PaletteChange/LayoutDirectionChange` 只同步终态并 `update()`；只有 `StyleChange/FontChange` 额外调用 `updateGeometry()`，避免纯颜色主题切换触发布局。

`sizeHint()` 固定 track 为 40x20 逻辑像素，文字间距 8；使用 `QFontMetrics::horizontalAdvance(text())` 与基类 contents margins 计算，不按 viewport 缩放字体。`paintEvent()` 只创建栈上 `QStyleOptionButton/QPainter`：RTL 时 track 放在内容右侧，否则左侧；knob 为 16x16，在 track 两端内缩 2 像素之间按 `progress` 插值；disabled 使用 `QPalette::Disabled`，checked 使用 Highlight，unchecked 使用 Mid。文字用 `drawItemText()`；有键盘焦点时调用当前 style 的 `PE_FrameFocusRect`，不得在 paint 中创建 pixmap、容器、文件对象、动画或 cache entry。

- [ ] **Step 5: 更新当前累计 Widgets 源文件清单**

Replace the complete `set(zz_fluent_ui_sources ...)` block in `ZzFluentUI/CMakeLists.txt` with the following block. It must remain before both `add_library(ZzFluentUI ...)` and `zz_configure_library_target(...)`:

```cmake
set(zz_fluent_ui_sources
    widgets/src/private/ZzFluentWidgetVersion.cpp
    widgets/src/ZzFluentPainter.cpp
    widgets/src/private/ZzStyleCache.cpp
    widgets/src/ZzFluentStyle.cpp
    widgets/src/private/ZzFluentStylePrivate.cpp
    widgets/src/ZzPushButton.cpp
    widgets/src/private/ZzPushButtonPrivate.cpp
    widgets/src/ZzIconButton.cpp
    widgets/src/private/ZzIconButtonPrivate.cpp
    widgets/src/ZzToggleSwitch.cpp
    widgets/src/private/ZzToggleSwitchPrivate.cpp
)
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
    widgets/include/ZzFluentUI/ZzPushButton.h
    widgets/include/ZzFluentUI/ZzIconButton.h
    widgets/include/ZzFluentUI/ZzToggleSwitch.h
)
```

- [ ] **Step 6: 运行绿灯和重复切换测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzToggleSwitchTest
ctest --preset linux-gcc-debug -R '^fluent.toggle-switch$' --repeat until-fail:20 --output-on-failure
```

Expected: PASS，20 次状态改变后仍只有一个 animation。

- [ ] **Step 7: 提交开关控件**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests ZzFluentUI/CMakeLists.txt
git commit -m "控件：实现 Fluent 开关" \
    -m "基于 QCheckBox 保留键盘、焦点和无障碍语义。" \
    -m "复用单一动画对象，并在 reduced-motion、隐藏和禁用状态立即到位。"
```

## Task 4: 实现具有超时与关闭意图的 ZzMessageBar

**Files:**
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzMessageSeverity.h`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzMessageBar.h`
- Create: `ZzFluentUI/widgets/src/ZzMessageBar.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzMessageBarPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzMessageBarPrivate.cpp`
- Create: `ZzFluentUI/tests/ZzMessageBarTest.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`

- [ ] **Step 1: 写内容、严重性、Escape 和超时失败测试**

Create `ZzFluentUI/tests/ZzMessageBarTest.cpp`，使用 `QSignalSpy` 和 `QTest` 覆盖 `textChanged`、severity property、名为 `zzMessageBarCloseButton` 的按钮 accessible name、两次 Escape 仍只发出一次 `closeRequested`、`timeoutMilliseconds=30` 最终发出一次关闭意图，以及鼠标 hover 超过完整 timeout 仍不关闭、离开后按剩余时长关闭。另覆盖两项生命周期契约：timer 运行中隐藏控件并等待超过 timeout 时不得关闭，重新显示后才重新计时；安装测试 translator 并发送 `LanguageChange` 后，关闭按钮的 tooltip 与 accessible name 都更新。所有异步断言使用 `QTRY_COMPARE_WITH_TIMEOUT(..., 500)`，不得用固定 sleep 判断 timer 已触发；控件不得自行修改模型、隐藏或删除自己。

Append exact registration to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzMessageBarTest ZzMessageBarTest.cpp)
target_link_libraries(ZzMessageBarTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzMessageBarTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzMessageBarTest)
zz_enable_sanitizers(ZzMessageBarTest)
add_test(NAME fluent.message-bar COMMAND ZzMessageBarTest)
set_tests_properties(fluent.message-bar PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 2: 运行红灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzMessageBarTest
```

Expected: compile FAIL，缺少 `ZzMessageBar.h`。

- [ ] **Step 3: 声明严重性和公开 API**

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzMessageSeverity.h`:

```cpp
#pragma once

#include <cstdint>

#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 标识消息条的纯展示严重性。 */
enum class ZzMessageSeverity : std::uint8_t
{
    Information,
    Success,
    Warning,
    Error
};

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzMessageSeverity)
```

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzMessageBar.h`:

```cpp
#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzMessageSeverity.h>

class QEnterEvent;
class QEvent;
class QHideEvent;
class QKeyEvent;
class QShowEvent;

namespace ZzFluentUI {

class ZzMessageBarPrivate;

/** @brief 展示文本、严重性和关闭意图，不拥有业务状态。 */
class ZZ_FLUENT_UI_EXPORT ZzMessageBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzMessageBar)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(ZzMessageSeverity severity READ severity WRITE setSeverity
               NOTIFY severityChanged)
    Q_PROPERTY(bool closable READ isClosable WRITE setClosable
               NOTIFY closableChanged)
    Q_PROPERTY(int timeoutMilliseconds READ timeoutMilliseconds
               WRITE setTimeoutMilliseconds NOTIFY timeoutMillisecondsChanged)

public:
    explicit ZzMessageBar(QWidget *parent = nullptr);
    ~ZzMessageBar() override;
    [[nodiscard]] QString text() const;
    void setText(QString text);
    [[nodiscard]] ZzMessageSeverity severity() const noexcept;
    void setSeverity(ZzMessageSeverity severity);
    [[nodiscard]] bool isClosable() const noexcept;
    void setClosable(bool closable);
    [[nodiscard]] int timeoutMilliseconds() const noexcept;
    void setTimeoutMilliseconds(int milliseconds);

Q_SIGNALS:
    void textChanged(const QString &text);
    void severityChanged(ZzMessageSeverity severity);
    void closableChanged(bool closable);
    void timeoutMillisecondsChanged(int milliseconds);
    /** @brief 请求宿主关闭；控件不会隐藏或删除自己。 */
    void closeRequested();

protected:
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    std::unique_ptr<ZzMessageBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

- [ ] **Step 4: 实现展示子控件和单一 QTimer**

Private 使用 `QHBoxLayout`、图标 `QLabel`、支持换行/选择的文本 `QLabel`、一个使用 `QStyle::SP_TitleBarCloseButton` 的 `QToolButton` 和一个 single-shot `QTimer`。子控件只由 MessageBar QObject parent 拥有，private 保存非拥有指针。精确状态如下：

```cpp
class ZzMessageBarPrivate final
{
public:
    explicit ZzMessageBarPrivate(ZzMessageBar *q);
    void refreshPresentation();
    void restartTimer();
    void pauseTimer() noexcept;
    void resumeTimer();
    void requestClose();

    ZzMessageBar *const q_ptr;
    QLabel *iconLabel = nullptr;
    QLabel *textLabel = nullptr;
    QToolButton *closeButton = nullptr;
    QTimer *timer = nullptr;
    QString text;
    ZzMessageSeverity severity = ZzMessageSeverity::Information;
    int timeoutMilliseconds = 0;
    int remainingMilliseconds = 0;
    bool closable = true;
    bool closePending = false;
    bool hovered = false;
};
```

超时 0 表示持续显示，负数输入收敛为 0。`restartTimer()` 先停止 timer，把 remaining 设为完整 timeout；仅当 bar 可见、未 hover、未 closePending 且 timeout>0 时启动。hover enter 用 `remainingTime()` 保存剩余值并停止，leave 用剩余值继续而非重置完整时长。`hideEvent()` 在调用基类前保存剩余值并停止 timer，保证隐藏期间绝不发超时意图；`showEvent()` 清除 closePending 并从完整 timeout 重启。timer、close button 与非 auto-repeat Escape 全部调用幂等 `requestClose()`，它先设置 closePending、停止 timer，再只 emit 一次 `closeRequested()`。宿主负责隐藏或销毁。

`refreshPresentation()` 使用当前 style 的四个标准 MessageBox icon；Success 允许复用 Information 图标但必须用 success palette 色。close button 的 tooltip 和 accessible name 均为 `tr("关闭")`；`changeEvent(LanguageChange)` 必须重新执行静态文案与 accessible text 的翻译刷新，其他事件交给基类。不可关闭时按钮隐藏且 Escape 交给基类。`setText/setSeverity/setClosable/setTimeoutMilliseconds` 只在值实际变化时发各自 NOTIFY，并分别更新可访问文本、视觉或 timer，不访问任何模型。

构造时设置 `closeButton->setObjectName(QStringLiteral("zzMessageBarCloseButton"))`，只作为稳定 UI 自动化标识，不进入业务逻辑。

- [ ] **Step 5: 更新当前累计 Widgets 源文件清单**

Replace the complete `set(zz_fluent_ui_sources ...)` block in `ZzFluentUI/CMakeLists.txt` with the following block. It must remain before both `add_library(ZzFluentUI ...)` and `zz_configure_library_target(...)`:

```cmake
set(zz_fluent_ui_sources
    widgets/src/private/ZzFluentWidgetVersion.cpp
    widgets/src/ZzFluentPainter.cpp
    widgets/src/private/ZzStyleCache.cpp
    widgets/src/ZzFluentStyle.cpp
    widgets/src/private/ZzFluentStylePrivate.cpp
    widgets/src/ZzPushButton.cpp
    widgets/src/private/ZzPushButtonPrivate.cpp
    widgets/src/ZzIconButton.cpp
    widgets/src/private/ZzIconButtonPrivate.cpp
    widgets/src/ZzToggleSwitch.cpp
    widgets/src/private/ZzToggleSwitchPrivate.cpp
    widgets/src/ZzMessageBar.cpp
    widgets/src/private/ZzMessageBarPrivate.cpp
)
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
    widgets/include/ZzFluentUI/ZzPushButton.h
    widgets/include/ZzFluentUI/ZzIconButton.h
    widgets/include/ZzFluentUI/ZzToggleSwitch.h
    widgets/include/ZzFluentUI/ZzMessageBar.h
)
```

- [ ] **Step 6: 运行 MessageBar 测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzMessageBarTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R '^fluent.message-bar$' --repeat until-fail:20 --output-on-failure
```

Expected: PASS，定时器每个实例始终只有一个，hover 期间不关闭。

- [ ] **Step 7: 提交消息条**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests ZzFluentUI/CMakeLists.txt
git commit -m "控件：实现 Fluent 消息条" \
    -m "增加信息、成功、警告和错误展示状态。" \
    -m "统一关闭按钮、Escape、hover 暂停和可选超时意图。"
```

## Task 5: 实现只消费展示模型的导航与面包屑

**Files:**
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzNavigationView.h`
- Create: `ZzFluentUI/widgets/src/ZzNavigationView.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzNavigationViewPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzNavigationViewPrivate.cpp`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzBreadcrumbBar.h`
- Create: `ZzFluentUI/widgets/src/ZzBreadcrumbBar.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzBreadcrumbBarPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzBreadcrumbBarPrivate.cpp`
- Create: `ZzFluentUI/tests/ZzNavigationControlsTest.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`

- [ ] **Step 1: 写 Model/View、键盘、RTL 和意图失败测试**

测试使用 `QStandardItemModel`，覆盖：设置 100000 行 model 后 `uniformItemSizes()==true`、`layoutMode()==QListView::Batched` 且 `batchSize()<=128`；设置 model 不创建页面对象；Up/Down 只改变 current index；Enter 发出一次 `navigationRequested(QModelIndex)`；disabled/不合法 index 不 emit；Breadcrumb 的中文长文本可伸缩；RTL 时视觉顺序反转但 `indexRequested(int)` 仍返回原逻辑 index。

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzNavigationControlsTest ZzNavigationControlsTest.cpp)
target_link_libraries(ZzNavigationControlsTest PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzNavigationControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzNavigationControlsTest)
zz_enable_sanitizers(ZzNavigationControlsTest)
add_test(NAME fluent.navigation-controls COMMAND ZzNavigationControlsTest)
set_tests_properties(fluent.navigation-controls PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 2: 运行红灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzNavigationControlsTest
```

Expected: compile FAIL，缺少两个公开控件。

- [ ] **Step 3: 声明 NavigationView 和 Breadcrumb API**

```cpp
class ZZ_FLUENT_UI_EXPORT ZzNavigationView final : public QListView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzNavigationView)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact
               NOTIFY compactChanged)

public:
    explicit ZzNavigationView(QWidget *parent = nullptr);
    ~ZzNavigationView() override;
    [[nodiscard]] bool isCompact() const noexcept;
    void setCompact(bool compact);

Q_SIGNALS:
    void compactChanged(bool compact);
    void navigationRequested(const QModelIndex &index);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    std::unique_ptr<ZzNavigationViewPrivate> d_ptr;
};

class ZZ_FLUENT_UI_EXPORT ZzBreadcrumbBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzBreadcrumbBar)

public:
    explicit ZzBreadcrumbBar(QWidget *parent = nullptr);
    ~ZzBreadcrumbBar() override;
    void setItems(QStringList items);
    [[nodiscard]] QStringList items() const;
    void setCurrentIndex(int index);
    [[nodiscard]] int currentIndex() const noexcept;

Q_SIGNALS:
    void indexRequested(int index);

protected:
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzBreadcrumbBarPrivate> d_ptr;
};
```

- [ ] **Step 4: 实现窄屏模式和可达面包屑**

NavigationView 保留 QListView 原生 selection/model API。构造时固定启用 `setUniformItemSizes(true)`、`setLayoutMode(QListView::Batched)` 和 `setBatchSize(64)`，避免 100000 行展示模型触发按总行数同步布局。Task 5 先安装定义在 private `.cpp` 的 `ZzNavigationViewDelegate`，它只在 compact 时清空复制 option 的 text；Task 6 创建公共 `ZzFluentItemDelegate` 后，必须修改 `ZzNavigationViewPrivate.cpp` 以改装该 delegate，避免临时行为遗留。compact 只把逻辑宽度从 240 改为 48 并隐藏展示文本，不修改 model。Enter/Return 在当前 index 有效且 flags 包含 `ItemIsEnabled` 时发一次意图并 accept；其他键交给 `QListView`。鼠标 `activated` 使用同一校验 helper，禁止一条输入产生重复信号。

Breadcrumb Private 使用 QObject-parented `QToolButton` 和非交互 separator label，每个按钮的 accessible name 是完整项文本；更新 items 可重建少量按钮，paint 不重建。空列表时 currentIndex=-1，越界输入收敛为 -1。按钮 property 保存原逻辑 index，layout direction 只改变视觉排列，因此 RTL click 仍发原 index。

- [ ] **Step 5: 更新当前累计 Widgets 源文件清单**

Replace the complete `set(zz_fluent_ui_sources ...)` block in `ZzFluentUI/CMakeLists.txt` with the following block. It must remain before both `add_library(ZzFluentUI ...)` and `zz_configure_library_target(...)`:

```cmake
set(zz_fluent_ui_sources
    widgets/src/private/ZzFluentWidgetVersion.cpp
    widgets/src/ZzFluentPainter.cpp
    widgets/src/private/ZzStyleCache.cpp
    widgets/src/ZzFluentStyle.cpp
    widgets/src/private/ZzFluentStylePrivate.cpp
    widgets/src/ZzPushButton.cpp
    widgets/src/private/ZzPushButtonPrivate.cpp
    widgets/src/ZzIconButton.cpp
    widgets/src/private/ZzIconButtonPrivate.cpp
    widgets/src/ZzToggleSwitch.cpp
    widgets/src/private/ZzToggleSwitchPrivate.cpp
    widgets/src/ZzMessageBar.cpp
    widgets/src/private/ZzMessageBarPrivate.cpp
    widgets/src/ZzNavigationView.cpp
    widgets/src/private/ZzNavigationViewPrivate.cpp
    widgets/src/ZzBreadcrumbBar.cpp
    widgets/src/private/ZzBreadcrumbBarPrivate.cpp
)
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
    widgets/include/ZzFluentUI/ZzPushButton.h
    widgets/include/ZzFluentUI/ZzIconButton.h
    widgets/include/ZzFluentUI/ZzToggleSwitch.h
    widgets/include/ZzFluentUI/ZzMessageBar.h
    widgets/include/ZzFluentUI/ZzNavigationView.h
    widgets/include/ZzFluentUI/ZzBreadcrumbBar.h
)
```

- [ ] **Step 6: 运行导航控件测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzNavigationControlsTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R '^fluent.navigation-controls$' --repeat until-fail:20 --output-on-failure
```

Expected: PASS，视图只传递 QModelIndex/整数意图，不访问路由或业务对象。

- [ ] **Step 7: 提交导航视图与面包屑**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests ZzFluentUI/CMakeLists.txt
git commit -m "控件：实现导航视图与面包屑" \
    -m "只消费 QAbstractItemModel 和展示文本，通过强类型信号转发用户意图。" \
    -m "覆盖键盘激活、长文本、紧凑布局和从右到左方向。"
```

## Task 6: 实现有界绘制的 ItemDelegate 和纯视觉 TitleBar

**Files:**
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzItemDensity.h`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentItemDelegate.h`
- Create: `ZzFluentUI/widgets/src/ZzFluentItemDelegate.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentItemDelegatePrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentItemDelegatePrivate.cpp`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentTitleBar.h`
- Create: `ZzFluentUI/widgets/src/ZzFluentTitleBar.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp`
- Modify: `ZzFluentUI/widgets/src/private/ZzNavigationViewPrivate.cpp`
- Create: `ZzFluentUI/tests/ZzFluentItemDelegateTest.cpp`
- Create: `ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`

- [ ] **Step 1: 写 delegate 大模型局部性与 titlebar 边界失败测试**

Delegate 测试使用 100000 行即时 model，并覆写 `multiData()` 记录每次请求的行号和调用次数。只渲染预先选定的 40 个可见 index，断言被访问的去重行号恰好是这 40 行、没有访问任何不可见行，且 `multiData()` 调用不超过 120；不得用固定的 `data()` 总次数阈值，因为 Qt 6.8 默认实现会为多个 role 分别调用 `data()`。同时断言 sizeHint 在 Standard/Compact 分别返回确定逻辑高度。TitleBar 测试断言 minimize/maximize/close 键盘 click 只发出意图，`setMaximized()` 更新 accessible name，所有 chrome getter 都是 titlebar 后代，且编译单元无 WindowKit/QWK include。

- [ ] **Step 2: 运行红灯测试**

Append to `ZzFluentUI/tests/CMakeLists.txt` before running the red test:

```cmake
add_executable(ZzFluentItemDelegateTest ZzFluentItemDelegateTest.cpp)
target_link_libraries(ZzFluentItemDelegateTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentItemDelegateTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentItemDelegateTest)
zz_enable_sanitizers(ZzFluentItemDelegateTest)
add_test(NAME fluent.item-delegate COMMAND ZzFluentItemDelegateTest)
set_tests_properties(fluent.item-delegate PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFluentTitleBarTest ZzFluentTitleBarTest.cpp)
target_link_libraries(ZzFluentTitleBarTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentTitleBarTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentTitleBarTest)
zz_enable_sanitizers(ZzFluentTitleBarTest)
add_test(NAME fluent.title-bar COMMAND ZzFluentTitleBarTest)
set_tests_properties(fluent.title-bar PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzFluentItemDelegateTest ZzFluentTitleBarTest
```

Expected: configure PASS；compile FAIL，诊断明确指出缺少 delegate/titlebar 头，而不是 `unknown target`。

- [ ] **Step 3: 声明 delegate 公开 API**

```cpp
enum class ZzItemDensity : std::uint8_t
{
    Standard,
    Compact
};

class ZZ_FLUENT_UI_EXPORT ZzFluentItemDelegate final
    : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFluentItemDelegate)

public:
    explicit ZzFluentItemDelegate(QObject *parent = nullptr);
    ~ZzFluentItemDelegate() override;
    void setDensity(ZzItemDensity density);
    [[nodiscard]] ZzItemDensity density() const noexcept;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const override;

private:
    std::unique_ptr<ZzFluentItemDelegatePrivate> d_ptr;
};
```

Private 每次只复制一个 `QStyleOptionViewItem`，调用 `initStyleOption()`，按 selected/hover/focus/disabled/RTL 绘制当前 index。不访问 rowCount()、不缓存 QModelIndex/模型指针、不为总行数分配内存。

- [ ] **Step 4: 声明纯视觉 TitleBar API**

```cpp
class ZZ_FLUENT_UI_EXPORT ZzFluentTitleBar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFluentTitleBar)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

public:
    explicit ZzFluentTitleBar(QWidget *parent = nullptr);
    ~ZzFluentTitleBar() override;
    [[nodiscard]] QString title() const;
    void setTitle(QString title);
    void setWindowIcon(const QIcon &icon);
    void setMaximized(bool maximized);
    void setSystemButtonsVisible(bool visible);
    [[nodiscard]] QWidget *windowIconWidget() const noexcept;
    [[nodiscard]] QWidget *minimizeButton() const noexcept;
    [[nodiscard]] QWidget *maximizeButton() const noexcept;
    [[nodiscard]] QWidget *closeButton() const noexcept;
    [[nodiscard]] QList<QWidget *> interactiveWidgets() const;

Q_SIGNALS:
    void titleChanged(const QString &title);
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();

protected:
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzFluentTitleBarPrivate> d_ptr;
};
```

Private 只管理 layout、title/icon 和三个 `QToolButton`。不 include ZzWindowKit/QWK，不调用 `showMinimized/showMaximized/close`，不读平台 API。maximize button 的 icon/tooltip/accessible name 由 `setMaximized()` 在“最大化”和“还原”之间切换；`changeEvent(LanguageChange)` 重新翻译最小化、最大化/还原、关闭的 tooltip 与 accessible name，其他事件交给基类。对应测试安装 test translator 后发送 `LanguageChange`，断言全部静态可访问文本更新。macOS 是否隐藏按钮由外层调用 `setSystemButtonsVisible(false)`。

- [ ] **Step 5: 运行 delegate/titlebar 测试**

Replace the complete `set(zz_fluent_ui_sources ...)` block in `ZzFluentUI/CMakeLists.txt` with the following current cumulative list. It must remain before both `add_library(ZzFluentUI ...)` and `zz_configure_library_target(...)`:

```cmake
set(zz_fluent_ui_sources
    widgets/src/private/ZzFluentWidgetVersion.cpp
    widgets/src/ZzFluentPainter.cpp
    widgets/src/private/ZzStyleCache.cpp
    widgets/src/ZzFluentStyle.cpp
    widgets/src/private/ZzFluentStylePrivate.cpp
    widgets/src/ZzPushButton.cpp
    widgets/src/private/ZzPushButtonPrivate.cpp
    widgets/src/ZzIconButton.cpp
    widgets/src/private/ZzIconButtonPrivate.cpp
    widgets/src/ZzToggleSwitch.cpp
    widgets/src/private/ZzToggleSwitchPrivate.cpp
    widgets/src/ZzMessageBar.cpp
    widgets/src/private/ZzMessageBarPrivate.cpp
    widgets/src/ZzNavigationView.cpp
    widgets/src/private/ZzNavigationViewPrivate.cpp
    widgets/src/ZzBreadcrumbBar.cpp
    widgets/src/private/ZzBreadcrumbBarPrivate.cpp
    widgets/src/ZzFluentItemDelegate.cpp
    widgets/src/private/ZzFluentItemDelegatePrivate.cpp
    widgets/src/ZzFluentTitleBar.cpp
    widgets/src/private/ZzFluentTitleBarPrivate.cpp
)
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
    widgets/include/ZzFluentUI/ZzPushButton.h
    widgets/include/ZzFluentUI/ZzIconButton.h
    widgets/include/ZzFluentUI/ZzToggleSwitch.h
    widgets/include/ZzFluentUI/ZzMessageBar.h
    widgets/include/ZzFluentUI/ZzNavigationView.h
    widgets/include/ZzFluentUI/ZzBreadcrumbBar.h
    widgets/include/ZzFluentUI/ZzFluentItemDelegate.h
    widgets/include/ZzFluentUI/ZzFluentTitleBar.h
)
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentItemDelegateTest ZzFluentTitleBarTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R '^fluent.(item-delegate|title-bar)$' --repeat until-fail:20 --output-on-failure
```

Expected: PASS，100000 行 model 只访问 40 个指定可见行且 `multiData()` 调用不超过 120；titlebar 只发出 UI 意图。

- [ ] **Step 6: 提交 delegate 与 titlebar**

```bash
git add ZzFluentUI/widgets ZzFluentUI/tests ZzFluentUI/CMakeLists.txt
git commit -m "控件：实现 Fluent 列表绘制与标题栏" \
    -m "增加与可见行数同阶的 List/Table/Tree delegate。" \
    -m "提供只负责布局、状态和窗口意图的 FluentTitleBar。"
```

## Task 7: 建立可访问性、截图和全控件画廊

**Files:**
- Create: `ZzFluentUI/tests/ZzFluentAccessibilityTest.cpp`
- Create: `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`
- Create: `ZzFluentUI/tests/baselines/linux/dpr-100/{light,dark,high-contrast}.png`
- Create: `ZzFluentUI/tests/baselines/linux/dpr-125/{light,dark,high-contrast}.png`
- Create: `ZzFluentUI/tests/baselines/linux/dpr-150/{light,dark,high-contrast}.png`
- Create: `ZzFluentUI/tests/baselines/linux/dpr-200/{light,dark,high-contrast}.png`
- Create: `examples/ZzFluentControlsGallery/CMakeLists.txt`
- Create: `examples/ZzFluentControlsGallery/main.cpp`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGallery.h`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGallery.cpp`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.h`
- Create: `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Modify: `examples/CMakeLists.txt`

- [ ] **Step 1: 写可访问性和 Tab 顺序测试**

构建包含所有交互控件的窗口，用 `QAccessible::queryAccessibleInterface()` 检查非空 name、预期 role、disabled/checked state；用 Tab/Shift+Tab 走完固定焦点顺序，每个焦点控件都有非透明焦点环渲染区域。Enter/Space/Escape 按各控件契约只触发一次。

- [ ] **Step 2: 实现容差截图测试**

`ZzFluentScreenshotTest` 接收必需参数 `--expected-dpr` 和 `--baseline-subdir`。`initTestCase()` 固定 `QLocale::c()`、布局方向、`DejaVu Sans` 10pt，并以 `QStyleFactory::create(QStringLiteral("Fusion"))` 创建 ZzFluentStyle 的 base style；若参考环境没有精确字体或 Fusion，立即失败，不能静默换字体。随后断言当前 screen DPR 与期望值误差不超过 0.01，再在固定 1200x800 逻辑尺寸窗口渲染 Light/Dark/HighContrast。CTest 分别以 1.0、1.25、1.5、2.0 四个常用 DPR 启动独立进程，图像物理尺寸必须等于逻辑尺寸乘 DPR 后的整数像素尺寸。比较前遍历 `QLabel`、全部 `QAbstractButton`、`QLineEdit`、`QTextEdit`、`QComboBox`、`QTabBar`、`QMenu`、带文本的 `QProgressBar` 和 item-view delegate 的文字矩形，将每个矩形向外扩 2 个逻辑像素后按 DPR 映射并合并为字体栅格化 mask；漏掉任一可见文字控件使测试失败。非文字像素每通道容差 3，差异像素比例不超过 0.5%。失败时写 actual/diff 到 build reports，不覆盖仓库 baseline。

- [ ] **Step 3: 生成并人工审查首次 baseline**

Run only in a documented Linux reference environment, using the same QPA as the automated screenshot CTest:

```bash
for entry in dpr-100:1.0 dpr-125:1.25 dpr-150:1.5 dpr-200:2.0; do
  tag=${entry%%:*}
  scale=${entry##*:}
  ZZ_UPDATE_SCREENSHOTS=1 \
  QT_QPA_PLATFORM=offscreen \
  QT_SCALE_FACTOR="$scale" \
  QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough \
    ./build/linux-gcc-release/ZzFluentUI/tests/ZzFluentScreenshotTest \
      --expected-dpr "$scale" --baseline-subdir "$tag"
done
```

Expected: 四个 DPR 子目录各生成 `light.png`、`dark.png`、`high-contrast.png`，共 12 张固定物理尺寸 PNG。人工检查无裁切、重叠、空白图标、不可见焦点、高对比度不足或 RTL 布局错误后才能提交 baseline。完成后保持相同 QPA/DPR、去掉更新环境变量逐项重跑，必须 PASS。X11/Wayland 原生视觉只记录在画廊和平台真机 checklist，不与此像素基线混用。

- [ ] **Step 4: 实现无业务逻辑控件画廊**

`ZzFluentControlsGallery` 使用四文件 PIMPL，首屏直接是可操作控件面：主题分段控件、标准/强调/图标按钮、开关、复选/单选、输入、菜单、消息、导航、Tab/Breadcrumb、List/Table/Tree 和 TitleBar。只使用本地 `QStandardItemModel`，不包含功能解说文本或业务访问。

- [ ] **Step 5: 运行无障碍、截图和真实画廊**

Append the two test targets to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzFluentAccessibilityTest ZzFluentAccessibilityTest.cpp)
target_link_libraries(ZzFluentAccessibilityTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentAccessibilityTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentAccessibilityTest)
zz_enable_sanitizers(ZzFluentAccessibilityTest)
add_test(NAME fluent.accessibility COMMAND ZzFluentAccessibilityTest)
set_tests_properties(fluent.accessibility PROPERTIES
    LABELS "fluent;accessibility;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFluentScreenshotTest ZzFluentScreenshotTest.cpp)
target_link_libraries(ZzFluentScreenshotTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
target_compile_definitions(ZzFluentScreenshotTest PRIVATE
    "ZZ_FLUENT_SCREENSHOT_BASELINE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/baselines/linux\""
)
set_target_properties(ZzFluentScreenshotTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentScreenshotTest)
zz_enable_sanitizers(ZzFluentScreenshotTest)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    function(zz_add_fluent_screenshot_test suffix scale)
        add_test(NAME "fluent.screenshot-${suffix}"
            COMMAND ZzFluentScreenshotTest
                --expected-dpr "${scale}"
                --baseline-subdir "dpr-${suffix}")
        set_tests_properties("fluent.screenshot-${suffix}" PROPERTIES
            LABELS "fluent;screenshot;component"
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=${scale};QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough;LANG=C.UTF-8;LC_ALL=C.UTF-8")
    endfunction()
    zz_add_fluent_screenshot_test(100 1.0)
    zz_add_fluent_screenshot_test(125 1.25)
    zz_add_fluent_screenshot_test(150 1.5)
    zz_add_fluent_screenshot_test(200 2.0)
endif()
```

Create `examples/ZzFluentControlsGallery/CMakeLists.txt` with:

```cmake
add_executable(ZzFluentControlsGallery
    main.cpp
    ZzFluentControlsGallery.cpp
    ZzFluentControlsGalleryPrivate.cpp
)
target_link_libraries(ZzFluentControlsGallery PRIVATE
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentControlsGallery PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentControlsGallery)
zz_enable_sanitizers(ZzFluentControlsGallery)
```

Append to the baseline `examples/CMakeLists.txt`; the root `ZZ_BUILD_EXAMPLES` branch already guards the entire directory:

```cmake
add_subdirectory(ZzFluentControlsGallery)
```

Run:

```bash
cmake --build --preset linux-gcc-release --target ZzFluentAccessibilityTest ZzFluentScreenshotTest ZzFluentControlsGallery
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-release -R '^fluent.(accessibility|screenshot-)' --output-on-failure
./build/linux-gcc-release/examples/ZzFluentControlsGallery/ZzFluentControlsGallery
```

Expected: accessibility 与四个 DPR 截图进程全部 PASS；画廊在当前 Linux 显示协议下无重叠/裁切，键盘/鼠标/主题切换可用。

- [ ] **Step 6: 提交视觉与无障碍基线**

```bash
git add ZzFluentUI/tests examples/ZzFluentControlsGallery examples/CMakeLists.txt
git commit -m "测试：建立 Fluent 控件视觉与无障碍基线" \
    -m "覆盖键盘焦点、辅助功能名称、四档常用 DPR 的三种主题截图和容差比较。" \
    -m "增加无业务依赖的全控件交互画廊。"
```

## Task 8: 建立性能、架构和安装消费门禁

**Files:**
- Create: `ZzFluentUI/tests/ZzBasicControlsBenchmark.cpp`
- Create: `tests/Architecture/CheckZzFluentUIBoundaries.cmake`
- Modify: `tests/Architecture/CMakeLists.txt`
- Modify: `tests/InstallConsumer/main.cpp`
- Modify: `tests/InstallConsumer/CMakeLists.txt`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`

- [ ] **Step 1: 创建 10 万行和控件稳定性 benchmark**

Benchmark 预热 10 轮、正式 100 轮，记录 40 可见行滚动/绘制 P50/P95/max、每帧 `multiData()` 调用数、被请求的唯一 model row、连续 1000 次 toggle/button hover 后 QObject 子对象数和图标 cache bytes。计数 model 覆写 `multiData()`，以 row 集合证明只访问指定 40 行且不访问不可见行，并强制每帧 `multiData()` 不超过 120；不对 Qt 6.8 的逐 role `data()` 内部调用次数设置伪复杂度阈值。所有机器都强制 animation/timer 对象数不增长、cache 不超过 Foundation 的 4 MiB 上限；只有 `ZZ_PERFORMANCE_REFERENCE=1` 的指定 Linux 参考机才断言 P95<=16.7 ms。普通 CI 把数值交给最终性能计划的同 runner 相对回归检查，不以噪声机器宣称绝对帧预算通过。

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
if(ZZ_BUILD_BENCHMARKS)
    add_executable(ZzBasicControlsBenchmark ZzBasicControlsBenchmark.cpp)
    target_link_libraries(ZzBasicControlsBenchmark PRIVATE
        Qt6::Test
        Qt6::Widgets
        Zz::FluentUI
    )
    set_target_properties(ZzBasicControlsBenchmark PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(ZzBasicControlsBenchmark)
    add_test(
        NAME benchmark.fluent-basic-controls
        COMMAND ZzBasicControlsBenchmark
    )
    set_tests_properties(benchmark.fluent-basic-controls PROPERTIES
        LABELS "benchmark;fluent"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
    )
    if(DEFINED ZZ_PERFORMANCE_REFERENCE)
        if(ZZ_PERFORMANCE_REFERENCE)
            set(zz_performance_reference_environment 1)
        else()
            set(zz_performance_reference_environment 0)
        endif()
        set_property(TEST benchmark.fluent-basic-controls APPEND PROPERTY
            ENVIRONMENT
                "ZZ_PERFORMANCE_REFERENCE=${zz_performance_reference_environment}")
    endif()
endif()
```

- [ ] **Step 2: 创建 UI 边界扫描**

Create `tests/Architecture/CheckZzFluentUIBoundaries.cmake` with:

```cmake
function(zz_read_source_without_comments source_path output_variable)
    file(READ "${source_path}" source_code)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" source_code "${source_code}")
    string(REGEX REPLACE "//[^\r\n]*" "" source_code "${source_code}")
    set(${output_variable} "${source_code}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE files
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets/*.h"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets/*.cpp"
)
foreach(file IN LISTS files)
    zz_read_source_without_comments("${file}" source_code)
    if(source_code MATCHES "#[ \t]*include[ \t]*[<\"]([^>\"]*/)?(Repository|Database|NetworkClient|DomainEntity)")
        message(FATAL_ERROR "Business dependency in Fluent UI: ${file}")
    endif()
    if(source_code MATCHES "Qt.*/private|QWK|ZzWindowKit|ZzPureTools")
        message(FATAL_ERROR "Forbidden implementation dependency in ${file}")
    endif()
    if(source_code MATCHES "namespace[ \t]+[A-Za-z0-9_]+::")
        message(FATAL_ERROR "Chained namespace in ${file}")
    endif()
endforeach()
```

另扫描 public headers 不得包含 `src/private`、Qt Private、QWK、repository/database/network/domain。

- [ ] **Step 3: 扩展安装消费者**

完整替换 `tests/InstallConsumer/main.cpp`，不得在基线的六组件版本检查后增量追加。最终文件只验证 FluentFoundation/FluentUI 可经 `Zz::FluentUI` 闭合消费：

```cpp
#include <QtWidgets/QApplication>

#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzFluentUI/ZzFluentItemDelegate.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzMessageBar.h>
#include <ZzFluentUI/ZzNavigationView.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzToggleSwitch.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzFluentUI::ZzThemeController themeController;
    ZzFluentUI::ZzPushButton pushButton;
    ZzFluentUI::ZzIconButton iconButton;
    ZzFluentUI::ZzToggleSwitch toggleSwitch;
    ZzFluentUI::ZzMessageBar messageBar;
    ZzFluentUI::ZzNavigationView navigationView;
    ZzFluentUI::ZzBreadcrumbBar breadcrumbBar;
    ZzFluentUI::ZzFluentItemDelegate itemDelegate;
    ZzFluentUI::ZzFluentTitleBar titleBar;

    (void)application;
    (void)themeController;
    (void)pushButton;
    (void)iconButton;
    (void)toggleSwitch;
    (void)messageBar;
    (void)navigationView;
    (void)breadcrumbBar;
    (void)itemDelegate;
    (void)titleBar;
    return 0;
}
```

删除 Core、WindowKit、AppCore、PureTools 的版本头和版本 API 调用。把 `tests/InstallConsumer/CMakeLists.txt` 中现有 link block 完整替换为：

```cmake
target_link_libraries(ZzInstallConsumer PRIVATE Zz::FluentUI)
```

依赖只允许通过 `Zz::FluentUI` 的安装 interface 传递；新建 consumer build tree 不可见源码树或 private include 路径。上述 main 不读取任何 Core、WindowKit、AppCore 或 PureTools API。

- [ ] **Step 4: 运行 Linux shared/static、Sanitizer 和 benchmark**

Run:

```bash
cmake --preset linux-gcc-release -DZZ_BUILD_BENCHMARKS=ON
cmake --build --preset linux-gcc-release --target ZzBasicControlsBenchmark
ctest --preset linux-gcc-release -R '^benchmark.fluent-basic-controls$' --output-on-failure
cmake --build --preset linux-clang-asan
ctest --preset linux-clang-asan -L fluent --output-on-failure
ctest --preset linux-gcc-release -R '^install.consumer$' --output-on-failure
ctest --preset linux-static-release -R '^install.consumer$' --output-on-failure
```

Expected: 算法/对象/缓存预算、ASan/UBSan 和两种安装消费全部 PASS；参考机额外满足 16.7 ms 绝对 P95，普通 CI 只记录 P50/P95/max。

- [ ] **Step 5: 执行最终规范扫描**

Run:

```bash
rg -n 'namespace[[:space:]]+[A-Za-z0-9_]+::|Qt.*/private|QWK|ZzWindowKit|ZzPureTools' ZzFluentUI/widgets
rg -n '#[[:space:]]*include.*(Repository|Database|NetworkClient|DomainEntity)' ZzFluentUI/widgets --pcre2
rg -L '/\*\*' ZzFluentUI/widgets/include/ZzFluentUI/Zz*.h
git diff --check
```

Expected: 前三条无违规匹配，`git diff --check` PASS。

- [ ] **Step 6: 提交基础控件最终门禁**

```bash
git add ZzFluentUI tests/Architecture tests/InstallConsumer
git commit -m "测试：锁定 Fluent 控件性能与分层" \
    -m "验证大模型绘制局部性、动画对象复用和有界图标缓存。" \
    -m "禁止 UI 访问业务、WindowKit、QWK 和 Qt Private，并验证 shared/static 安装消费。"
```

## 完成标准

- 标准 Qt 控件保留原生语义，只由 `ZzFluentStyle` 覆盖 Fluent 绘制。
- 自定义按钮、开关、消息、导航、面包屑、delegate 和 titlebar 全部使用 Zz 前缀、四文件 PIMPL 和中文 Doxygen。
- 键盘、焦点、禁用、HighContrast、RTL、长文本和屏幕阅读器名称有自动覆盖。
- paint 路径不解析 SVG/文件，不加锁，不为模型总行数分配对象。
- 动画和 timer 每实例只创建一次，图标缓存不超配置字节上限。
- `ZzFluentTitleBar` 不包含 WindowKit/QWK 调用，只向 ZzPureTools 组合层发出意图。
- Linux 控件画廊、三主题容差截图、无障碍、Sanitizer、shared/static install consumer 和 10 万行算法门禁通过；绝对 16.7 ms 只由有环境记录的参考机门禁判定。
