# ZzFluentUI 主题基础层 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立 Qt Widgets Fluent 主题基础层，提供 O(1) 令牌读取、不可变主题快照、系统主题控制、有界样式缓存、动画/DPI/可访问性策略和可安装的 `ZzFluentStyle`。

**Architecture:** `Zz::FluentFoundation` 只依赖 ZzCore 与 Qt Core/Gui，以定长数组保存颜色、尺寸、字体和动效令牌；四文件 PIMPL 的 `ZzThemeController` 在 GUI 线程生成完整快照后原子替换。`Zz::FluentUI` 只依赖 Foundation 与 Qt Widgets/Svg，四文件 PIMPL 的 `ZzFluentStyle` 通过私有定长缓存和无状态绘制原语覆盖基础 Fluent 行为，不接触领域、存储或网络；未来 `ZzFluentQuick` 只消费 Foundation，不依赖 Widgets target。

**Tech Stack:** C++20、Qt 6.8+ Core/Gui/Widgets/Svg/Test、CMake 3.23、Qt Test、QProxyStyle、QStyleHints、CTest、ASan/UBSan。

---

## 前置条件

- 工作目录是 `/home/zz/Jackfahdin/github/ZzPureToolsFrame/ZzPureToolsFrame`。
- 已依次完成仓库基线、ZzLog、ZzCore 与 ZzWindowKit 计划。
- `Zz::FluentFoundation`、`Zz::FluentUI`、公共导出宏、安装包和 Linux presets 已存在。
- 本计划只建立主题与样式基础；不实现按钮、输入、导航、标题栏或 Qt Quick/QML 控件。
- 所有测试用 `QT_QPA_PLATFORM=offscreen` 时只验证确定性 Qt 行为；Linux 真实显示会话另行运行 smoke，不用 offscreen 结果代替原生运行结果。

## 文件职责映射

### Foundation 公共值类型

- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeMode.h`：应用级主题选择。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzColorToken.h`：颜色令牌及定长数组边界。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h`：逻辑像素尺寸令牌。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzTypographyToken.h`：排版令牌。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzMotionToken.h`：动效时长令牌。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeChangeKind.h`：颜色、几何、动效和可访问性变更分类。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemePalette.h`：Light/Dark/HighContrast 定长调色板。
- Create: `ZzFluentUI/foundation/src/ZzThemePalette.cpp`：调色板固定值与强调色派生。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeSnapshot.h`：不可变 O(1) 快照。
- Create: `ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp`：快照工厂和边界检查。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzAnimationPolicy.h`：正常/减少动效的统一时长规则。
- Create: `ZzFluentUI/foundation/src/ZzAnimationPolicy.cpp`：时长裁剪实现。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzDpiScale.h`：DPR 量化与逻辑像素转换。
- Create: `ZzFluentUI/foundation/src/ZzDpiScale.cpp`：有限数值和上限处理。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzIconDescriptor.h`：不包含 Widgets 的图标描述。
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzIconCacheKey.h`：资源、尺寸、DPR、RGBA、revision 完整缓存键。
- Create: `ZzFluentUI/foundation/src/ZzIconCacheKey.cpp`：稳定相等与哈希实现。

### Foundation 有状态控制器

- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeController.h`：公开 QObject API、中文契约与信号。
- Create: `ZzFluentUI/foundation/src/ZzThemeController.cpp`：PIMPL 转发。
- Create: `ZzFluentUI/foundation/src/private/ZzThemeControllerPrivate.h`：GUI 线程状态和系统信号连接。
- Create: `ZzFluentUI/foundation/src/private/ZzThemeControllerPrivate.cpp`：完整快照构造、比较和一次交换。

### Widgets 样式基础

- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentPainter.h`：无状态背景、边框和焦点环绘制原语。
- Create: `ZzFluentUI/widgets/src/ZzFluentPainter.cpp`：DPR、RTL、高对比度安全绘制。
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h`：QProxyStyle 公开入口和图标缓存 API。
- Create: `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：PIMPL 转发与 QStyle override。
- Create: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h`：controller 非拥有引用、快照和缓存所有权。
- Create: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`：主题传播、图标预热和缓存失效。
- Create: `ZzFluentUI/widgets/src/private/ZzStyleCache.h`：固定槽视觉缓存与有界图标缓存。
- Create: `ZzFluentUI/widgets/src/private/ZzStyleCache.cpp`：O(1) 热路径查询、字节预算和 LRU 淘汰。

### 测试、性能、架构与运行验证

- Create: `ZzFluentUI/tests/CMakeLists.txt`：注册 Foundation/Widgets 测试及 label。
- Create: `ZzFluentUI/tests/ZzThemeSnapshotTest.cpp`：令牌、边界和高对比度测试。
- Create: `ZzFluentUI/tests/ZzThemeControllerTest.cpp`：模式、系统变化、线程和信号测试。
- Create: `ZzFluentUI/tests/ZzAnimationDpiTest.cpp`：减少动效、DPR 与缓存键测试。
- Create: `ZzFluentUI/tests/ZzFluentStyleTest.cpp`：palette、metric、focus、缓存容量和无障碍测试。
- Create: `ZzFluentUI/tests/resources/ZzFluentTestSquare.svg`：自有的纯几何测试图标，仅验证资源渲染、着色、RTL 与缓存命中。
- Create: `ZzFluentUI/tests/ZzThemeSwitchBenchmark.cpp`：500 控件主题切换 P50/P95/最大值测量。
- Create: `examples/ZzFluentFoundationDemo/CMakeLists.txt`：Linux 原生运行 target。
- Create: `examples/ZzFluentFoundationDemo/main.cpp`：Light/Dark/System/HighContrast 切换 smoke。
- Create: `tests/Architecture/CheckZzFluentBoundaries.cmake`：依赖、命名、namespace、Private、业务词和 Quick 边界扫描。
- Modify: `ZzFluentUI/CMakeLists.txt`：加入两层源码、依赖和测试。
- Modify: `tests/Architecture/CMakeLists.txt`：注册边界测试。
- Modify: `tests/InstallConsumer/main.cpp`：消费 Foundation 与 Widgets 公开 API。
- Modify: `examples/CMakeLists.txt`：按项目选项加入 demo。

## Task 1: 固定 Foundation 类型契约和构建边界

**Files:**
- Create: `ZzFluentUI/tests/CMakeLists.txt`
- Create: `ZzFluentUI/tests/ZzThemeSnapshotTest.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`

- [ ] **Step 1: 写入快照编译失败测试**

Create `ZzFluentUI/tests/ZzThemeSnapshotTest.cpp` with:

```cpp
#include <QtGui/QColor>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

class ZzThemeSnapshotTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesDeterministicTokens()
    {
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::Light,
            QColor(QStringLiteral("#0067c0")),
            7,
            false);

        QCOMPARE(snapshot.revision(), quint64{7});
        QCOMPARE(
            snapshot.color(ZzFluentUI::ZzColorToken::Accent),
            QColor(QStringLiteral("#0067c0")));
        QVERIFY(snapshot.metric(ZzFluentUI::ZzMetricToken::CornerRadiusMedium) > 0.0);
        QVERIFY(snapshot.duration(ZzFluentUI::ZzMotionToken::Fast) > 0);
        QVERIFY(!snapshot.font(ZzFluentUI::ZzTypographyToken::Body).family().isEmpty());
    }

    void keepsHighContrastLegible()
    {
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::HighContrast,
            QColor(Qt::yellow),
            1,
            true);

        QCOMPARE(snapshot.color(ZzFluentUI::ZzColorToken::Surface), QColor(Qt::black));
        QCOMPARE(snapshot.color(ZzFluentUI::ZzColorToken::TextPrimary), QColor(Qt::white));
        QCOMPARE(snapshot.duration(ZzFluentUI::ZzMotionToken::Fast), 0);
        QVERIFY(snapshot.reducedMotion());
    }
};

QTEST_MAIN(ZzThemeSnapshotTest)

#include "ZzThemeSnapshotTest.moc"
```

- [ ] **Step 2: 注册单个红灯 target**

Create `ZzFluentUI/tests/CMakeLists.txt` with:

```cmake
add_executable(ZzThemeSnapshotTest
    ZzThemeSnapshotTest.cpp
)
target_link_libraries(ZzThemeSnapshotTest PRIVATE
    Qt6::Test
    Zz::FluentFoundation
)
set_target_properties(ZzThemeSnapshotTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzThemeSnapshotTest)
zz_enable_sanitizers(ZzThemeSnapshotTest)
add_test(NAME fluent.theme-snapshot COMMAND ZzThemeSnapshotTest)
set_tests_properties(fluent.theme-snapshot PROPERTIES
    LABELS "unit;fluent;foundation"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Append to `ZzFluentUI/CMakeLists.txt`:

```cmake
if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 3: 运行红灯并记录明确失败**

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzThemeSnapshotTest
```

Expected: configure 返回 0；编译失败并指出 `ZzFluentUI/ZzColorToken.h` 不存在。

- [ ] **Step 4: 检查 target 依赖方向**

Run:

```bash
cmake --build --preset linux-gcc-debug --target help | rg "ZzFluent(Foundation|UI)"
```

Expected: 输出包含 `ZzFluentFoundation` 和 `ZzFluentUI`；本步骤不修改实现，红灯保留到 Task 2。

## Task 2: 实现定长 token、palette 和不可变 snapshot

**Files:**
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeMode.h`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzColorToken.h`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzTypographyToken.h`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzMotionToken.h`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemePalette.h`
- Create: `ZzFluentUI/foundation/src/ZzThemePalette.cpp`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeSnapshot.h`
- Create: `ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp`
- Modify: `ZzFluentUI/CMakeLists.txt`

- [ ] **Step 1: 创建五个带 Count 哨兵的公开枚举**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeMode.h` with:

```cpp
#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 指定应用级主题来源。 */
enum class ZzThemeMode : std::uint8_t
{
    System,
    Light,
    Dark,
    HighContrast
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzColorToken.h` with:

```cpp
#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识可在绘制热路径中 O(1) 读取的 Fluent 颜色。 */
enum class ZzColorToken : std::uint16_t
{
    TextPrimary,
    TextSecondary,
    ControlFill,
    ControlFillHover,
    ControlFillPressed,
    ControlFillDisabled,
    ControlStroke,
    Accent,
    AccentText,
    FocusStroke,
    Surface,
    SurfaceSecondary,
    Error,
    Count
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzMetricToken.h` with:

```cpp
#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识与设备无关的逻辑像素尺寸。 */
enum class ZzMetricToken : std::uint16_t
{
    CornerRadiusSmall,
    CornerRadiusMedium,
    StrokeThin,
    FocusStrokeWidth,
    ControlHeight,
    HorizontalPadding,
    VerticalPadding,
    IconSmall,
    IconMedium,
    Count
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzTypographyToken.h` with:

```cpp
#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识使用平台字体族构造的排版角色。 */
enum class ZzTypographyToken : std::uint16_t
{
    Caption,
    Body,
    BodyStrong,
    Subtitle,
    Title,
    Count
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzMotionToken.h` with:

```cpp
#pragma once

#include <cstdint>

namespace ZzFluentUI {

/** @brief 标识非业务状态动画的标准时长。 */
enum class ZzMotionToken : std::uint16_t
{
    Fast,
    Normal,
    Slow,
    PageTransition,
    Count
};

} // namespace ZzFluentUI
```

- [ ] **Step 2: 创建公开调色板值类型**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemePalette.h` with:

```cpp
#pragma once

#include <array>

#include <QtGui/QColor>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentFoundationExport.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace ZzFluentUI {

/** @brief 保存一个完整且不可变的主题调色板。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzThemePalette final
{
public:
    /**
     * @brief 根据明确模式和强调色构造完整调色板。
     * @param mode 必须是 Light、Dark 或 HighContrast。
     * @param accent 有效强调色；无效颜色使用平台无关蓝色。
     * @return 可复制的定长调色板，调用线程不限。
     */
    [[nodiscard]] static ZzThemePalette create(ZzThemeMode mode, QColor accent);

    /**
     * @brief 按令牌读取颜色。
     * @param token 必须小于 Count；越界时断言并返回透明色。
     * @return 调色板拥有值的副本，调用线程不限。
     */
    [[nodiscard]] QColor color(ZzColorToken token) const noexcept;

private:
    explicit ZzThemePalette(
        std::array<QColor, static_cast<std::size_t>(ZzColorToken::Count)> colors);

    std::array<QColor, static_cast<std::size_t>(ZzColorToken::Count)> colors_;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/src/ZzThemePalette.cpp` with:

```cpp
#include <ZzFluentUI/ZzThemePalette.h>

#include <QtCore/QtGlobal>

#include <utility>

namespace ZzFluentUI {

namespace {

using ZzColors = std::array<QColor, static_cast<std::size_t>(ZzColorToken::Count)>;

ZzColors lightColors(const QColor &accent)
{
    return {QColor("#1a1a1a"), QColor("#5d5d5d"), QColor("#ffffff"),
            QColor("#f5f5f5"), QColor("#e8e8e8"), QColor("#f0f0f0"),
            QColor("#d1d1d1"), accent, QColor("#ffffff"), QColor("#000000"),
            QColor("#f9f9f9"), QColor("#ffffff"), QColor("#c42b1c")};
}

ZzColors darkColors(const QColor &accent)
{
    return {QColor("#ffffff"), QColor("#c5c5c5"), QColor("#323232"),
            QColor("#3b3b3b"), QColor("#454545"), QColor("#2a2a2a"),
            QColor("#5a5a5a"), accent, QColor("#000000"), QColor("#ffffff"),
            QColor("#202020"), QColor("#2b2b2b"), QColor("#ff99a4")};
}

ZzColors highContrastColors(const QColor &)
{
    return {QColor(Qt::white), QColor(Qt::white), QColor(Qt::black),
            QColor(Qt::black), QColor(Qt::black), QColor(Qt::black),
            QColor(Qt::white), QColor(Qt::yellow), QColor(Qt::black),
            QColor(Qt::yellow),
            QColor(Qt::black), QColor(Qt::black), QColor(Qt::red)};
}

} // namespace

ZzThemePalette::ZzThemePalette(ZzColors colors)
    : colors_(std::move(colors))
{
}

ZzThemePalette ZzThemePalette::create(ZzThemeMode mode, QColor accent)
{
    if (!accent.isValid()) {
        accent = QColor("#0067c0");
    }
    if (mode == ZzThemeMode::HighContrast) {
        return ZzThemePalette(highContrastColors(accent));
    }
    if (mode == ZzThemeMode::Dark) {
        return ZzThemePalette(darkColors(accent));
    }
    return ZzThemePalette(lightColors(accent));
}

QColor ZzThemePalette::color(ZzColorToken token) const noexcept
{
    const auto index = static_cast<std::size_t>(token);
    if (index >= colors_.size()) {
        Q_ASSERT(false);
        return QColor(Qt::transparent);
    }
    return colors_[index];
}

} // namespace ZzFluentUI
```

HighContrast 固定使用黑/白/黄/红系统无关高对比组合并忽略自定义 accent，避免黑色或低对比强调色破坏可读性；Light/Dark 才消费调用者 accent。对应测试必须传入黑色 accent，仍断言 `Accent == yellow`、`AccentText == black`、`FocusStroke == yellow`。

- [ ] **Step 3: 创建不可变快照完整接口**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeSnapshot.h` with:

```cpp
#pragma once

#include <array>

#include <QtGui/QFont>

#include <ZzFluentUI/ZzFluentFoundationExport.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemePalette.h>
#include <ZzFluentUI/ZzTypographyToken.h>

namespace ZzFluentUI {

/** @brief 提供一次主题 revision 的完整只读令牌快照。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzThemeSnapshot final
{
public:
    /**
     * @brief 使用当前应用字体构造完整快照。
     * @param mode 必须解析为 Light、Dark 或 HighContrast。
     * @param accent 强调色，无效值使用默认色。
     * @param revision 单调递增的主题版本。
     * @param reducedMotion 是否关闭非必要动画。
     * @return 线程可读的不可变值。
     * @pre 已构造 QGuiApplication，且调用发生在应用所属 GUI 线程。
     * @note 构造阶段读取 QGuiApplication::font()；构造完成后，调用者可将值或
     * shared_ptr<const ZzThemeSnapshot> 传给其他线程只读使用。
     */
    [[nodiscard]] static ZzThemeSnapshot create(
        ZzThemeMode mode,
        QColor accent,
        quint64 revision,
        bool reducedMotion);

    [[nodiscard]] QColor color(ZzColorToken token) const noexcept;
    [[nodiscard]] qreal metric(ZzMetricToken token) const noexcept;
    [[nodiscard]] QFont font(ZzTypographyToken token) const;
    [[nodiscard]] int duration(ZzMotionToken token) const noexcept;
    [[nodiscard]] quint64 revision() const noexcept;
    [[nodiscard]] bool reducedMotion() const noexcept;

private:
    ZzThemeSnapshot(
        ZzThemePalette palette,
        std::array<qreal, static_cast<std::size_t>(ZzMetricToken::Count)> metrics,
        std::array<QFont, static_cast<std::size_t>(ZzTypographyToken::Count)> fonts,
        std::array<int, static_cast<std::size_t>(ZzMotionToken::Count)> durations,
        quint64 revision,
        bool reducedMotion);

    ZzThemePalette palette_;
    std::array<qreal, static_cast<std::size_t>(ZzMetricToken::Count)> metrics_;
    std::array<QFont, static_cast<std::size_t>(ZzTypographyToken::Count)> fonts_;
    std::array<int, static_cast<std::size_t>(ZzMotionToken::Count)> durations_;
    quint64 revision_;
    bool reducedMotion_;
};

} // namespace ZzFluentUI
```

- [ ] **Step 4: 实现快照工厂与 O(1) 读取**

Create `ZzFluentUI/foundation/src/ZzThemeSnapshot.cpp` with:

```cpp
#include <ZzFluentUI/ZzThemeSnapshot.h>

#include <QtCore/QThread>
#include <QtCore/QtGlobal>
#include <QtGui/QGuiApplication>

#include <utility>

namespace ZzFluentUI {

namespace {

template<typename ZzArray, typename ZzToken>
std::size_t checkedIndex(const ZzArray &values, ZzToken token) noexcept
{
    const auto index = static_cast<std::size_t>(token);
    if (index >= values.size()) {
        Q_ASSERT(false);
        return 0;
    }
    return index;
}

QFont scaledFont(const QFont &base, qreal scale, qreal fallbackPointSize)
{
    QFont result(base);
    if (base.pointSizeF() > 0.0) {
        result.setPointSizeF(base.pointSizeF() * scale);
    } else if (base.pixelSize() > 0) {
        result.setPixelSize(qMax(1, qRound(base.pixelSize() * scale)));
    } else {
        result.setPointSizeF(fallbackPointSize);
    }
    return result;
}

} // namespace

ZzThemeSnapshot::ZzThemeSnapshot(
    ZzThemePalette palette,
    std::array<qreal, static_cast<std::size_t>(ZzMetricToken::Count)> metrics,
    std::array<QFont, static_cast<std::size_t>(ZzTypographyToken::Count)> fonts,
    std::array<int, static_cast<std::size_t>(ZzMotionToken::Count)> durations,
    quint64 revision,
    bool reducedMotion)
    : palette_(std::move(palette)),
      metrics_(std::move(metrics)),
      fonts_(std::move(fonts)),
      durations_(std::move(durations)),
      revision_(revision),
      reducedMotion_(reducedMotion)
{
}

ZzThemeSnapshot ZzThemeSnapshot::create(
    ZzThemeMode mode,
    QColor accent,
    quint64 revision,
    bool reducedMotion)
{
    Q_ASSERT(QGuiApplication::instance() != nullptr);
    Q_ASSERT(QThread::currentThread() == QGuiApplication::instance()->thread());
    if (mode == ZzThemeMode::System) {
        mode = ZzThemeMode::Light;
    }
    const std::array<qreal, 9> metrics{2.0, 4.0, 1.0, 2.0, 32.0, 12.0, 6.0, 16.0, 20.0};
    QFont base = QGuiApplication::font();
    if (base.family().isEmpty()) {
        base.setFamily(QStringLiteral("Sans Serif"));
    }
    QFont caption = scaledFont(base, 0.9, 9.0);
    QFont body = scaledFont(base, 1.0, 10.0);
    QFont strong(body); strong.setWeight(QFont::DemiBold);
    QFont subtitle = scaledFont(strong, 1.4, 14.0);
    QFont title = scaledFont(strong, 2.0, 20.0);
    const std::array<QFont, 5> fonts{caption, body, strong, subtitle, title};
    std::array<int, 4> durations{83, 167, 250, 250};
    if (reducedMotion) {
        durations.fill(0);
    }
    return ZzThemeSnapshot(
        ZzThemePalette::create(mode, accent), metrics, fonts, durations,
        revision, reducedMotion);
}

QColor ZzThemeSnapshot::color(ZzColorToken token) const noexcept
{
    return palette_.color(token);
}

qreal ZzThemeSnapshot::metric(ZzMetricToken token) const noexcept
{
    return metrics_[checkedIndex(metrics_, token)];
}

QFont ZzThemeSnapshot::font(ZzTypographyToken token) const
{
    return fonts_[checkedIndex(fonts_, token)];
}

int ZzThemeSnapshot::duration(ZzMotionToken token) const noexcept
{
    return durations_[checkedIndex(durations_, token)];
}

quint64 ZzThemeSnapshot::revision() const noexcept
{
    return revision_;
}

bool ZzThemeSnapshot::reducedMotion() const noexcept
{
    return reducedMotion_;
}

} // namespace ZzFluentUI
```

- [ ] **Step 5: 把完整源码加入 Foundation target**

Replace the `zz_fluent_foundation_sources` list and its library block in `ZzFluentUI/CMakeLists.txt` with:

```cmake
set(zz_fluent_foundation_sources
    foundation/src/private/ZzFluentVersion.cpp
    foundation/src/ZzThemePalette.cpp
    foundation/src/ZzThemeSnapshot.cpp
)
set(zz_fluent_foundation_moc_headers)
add_library(ZzFluentFoundation ${zz_fluent_foundation_sources})
add_library(Zz::FluentFoundation ALIAS ZzFluentFoundation)

target_link_libraries(ZzFluentFoundation PUBLIC
    Zz::Core
    Qt6::Core
    Qt6::Gui
)

zz_configure_library_target(
    ZzFluentFoundation
    EXPORT_NAME FluentFoundation
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/foundation/include"
    EXPORT_HEADER_SUBDIR ZzFluentUI
    EXPORT_HEADER_NAME ZzFluentFoundationExport.h
    EXPORT_MACRO_NAME ZZ_FLUENT_FOUNDATION_EXPORT
    SOURCES ${zz_fluent_foundation_sources}
    MOC_HEADERS ${zz_fluent_foundation_moc_headers}
)
```

Preserve the existing `ZzFluentUI` target and test branch below this block. 后续 Task 3-4 新增 Foundation 翻译单元时，必须把路径加入 `zz_fluent_foundation_sources` 的 `set(...)`，位置保持在 `add_library()` 和 `zz_configure_library_target()` 之前。

- [ ] **Step 6: 构建并运行绿灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzThemeSnapshotTest
ctest --preset linux-gcc-debug -R fluent.theme-snapshot
```

Expected: build 返回 0；CTest 报告 `fluent.theme-snapshot ... Passed`。

- [ ] **Step 7: 提交 token 与快照**

```bash
git add ZzFluentUI
git commit -m "主题：实现定长令牌与不可变快照" \
    -m "增加 Light、Dark 和 HighContrast 调色板及颜色、尺寸、排版、动效令牌。" \
    -m "绘制读取使用边界受控的定长数组，快照不依赖字符串查找、锁或运行期堆分配。"
```

## Task 3: 实现动画、DPI、图标键和可访问性基础

**Files:**
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzAnimationPolicy.h`
- Create: `ZzFluentUI/foundation/src/ZzAnimationPolicy.cpp`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzDpiScale.h`
- Create: `ZzFluentUI/foundation/src/ZzDpiScale.cpp`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzIconDescriptor.h`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzIconCacheKey.h`
- Create: `ZzFluentUI/foundation/src/ZzIconCacheKey.cpp`
- Create: `ZzFluentUI/tests/ZzAnimationDpiTest.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Modify: `ZzFluentUI/CMakeLists.txt`

- [ ] **Step 1: 写动效、DPR 和完整缓存键红灯测试**

Create `ZzFluentUI/tests/ZzAnimationDpiTest.cpp` with:

```cpp
#include <QtCore/QSet>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzIconCacheKey.h>

class ZzAnimationDpiTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void appliesReducedMotion()
    {
        QCOMPARE(ZzFluentUI::ZzAnimationPolicy::adjustedDuration(167, false, false), 167);
        QCOMPARE(ZzFluentUI::ZzAnimationPolicy::adjustedDuration(167, true, false), 0);
        QCOMPARE(ZzFluentUI::ZzAnimationPolicy::adjustedDuration(167, true, true), 50);
    }

    void quantizesDprWithoutZeroSizes()
    {
        QCOMPARE(ZzFluentUI::ZzDpiScale::bucket(1.25), quint16{125});
        QCOMPARE(ZzFluentUI::ZzDpiScale::physicalPixels(1.0, 1.25), 2);
        QCOMPARE(ZzFluentUI::ZzDpiScale::bucket(0.0), quint16{100});
    }

    void distinguishesEveryIconInput()
    {
        const ZzFluentUI::ZzIconCacheKey first(
            QStringLiteral(":/icons/first.svg"), false,
            QSize(16, 16), 125, 0xff0067c0U, 4);
        const ZzFluentUI::ZzIconCacheKey differentDpr(
            QStringLiteral(":/icons/first.svg"), false,
            QSize(16, 16), 150, 0xff0067c0U, 4);
        const ZzFluentUI::ZzIconCacheKey differentResource(
            QStringLiteral(":/icons/second.svg"), false,
            QSize(16, 16), 125, 0xff0067c0U, 4);
        const ZzFluentUI::ZzIconCacheKey mirrored(
            QStringLiteral(":/icons/first.svg"), true,
            QSize(16, 16), 125, 0xff0067c0U, 4);
        QVERIFY(first != differentDpr);
        QVERIFY(first != differentResource);
        QVERIFY(first != mirrored);
        QSet<ZzFluentUI::ZzIconCacheKey> keys;
        keys.insert(first);
        keys.insert(differentDpr);
        keys.insert(differentResource);
        keys.insert(mirrored);
        QCOMPARE(keys.size(), 4);
    }
};

QTEST_GUILESS_MAIN(ZzAnimationDpiTest)

#include "ZzAnimationDpiTest.moc"
```

- [ ] **Step 2: 注册测试并确认缺失类型**

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzAnimationDpiTest ZzAnimationDpiTest.cpp)
target_link_libraries(ZzAnimationDpiTest PRIVATE Qt6::Test Zz::FluentFoundation)
set_target_properties(ZzAnimationDpiTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzAnimationDpiTest)
zz_enable_sanitizers(ZzAnimationDpiTest)
add_test(NAME fluent.animation-dpi COMMAND ZzAnimationDpiTest)
set_tests_properties(fluent.animation-dpi PROPERTIES LABELS "unit;fluent;foundation")
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzAnimationDpiTest
```

Expected: compile FAIL，指出 `ZzAnimationPolicy.h` 不存在。

- [ ] **Step 3: 创建动画策略完整实现**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzAnimationPolicy.h` with:

```cpp
#pragma once

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 将主题动效偏好转换为有界动画时长。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzAnimationPolicy final
{
public:
    ZzAnimationPolicy() = delete;

    /**
     * @brief 调整非负时长。
     * @param durationMilliseconds 原始毫秒数，负值按零处理。
     * @param reducedMotion 是否减少非必要动效。
     * @param essential 是否为表达即时状态反馈所必需的过渡。
     * @return 0 到 10000 毫秒；减少动效时非必要动画返回 0，必要动画最多 50。
     */
    [[nodiscard]] static int adjustedDuration(
        int durationMilliseconds,
        bool reducedMotion,
        bool essential) noexcept;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/src/ZzAnimationPolicy.cpp` with:

```cpp
#include <ZzFluentUI/ZzAnimationPolicy.h>

#include <algorithm>

namespace ZzFluentUI {

int ZzAnimationPolicy::adjustedDuration(
    int durationMilliseconds,
    bool reducedMotion,
    bool essential) noexcept
{
    const int bounded = std::clamp(durationMilliseconds, 0, 10000);
    if (!reducedMotion) {
        return bounded;
    }
    return essential ? std::min(bounded, 50) : 0;
}

} // namespace ZzFluentUI
```

- [ ] **Step 4: 创建 DPI 完整实现**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzDpiScale.h` with:

```cpp
#pragma once

#include <QtCore/QtTypes>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 提供稳定的设备像素比量化和像素对齐。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzDpiScale final
{
public:
    ZzDpiScale() = delete;
    [[nodiscard]] static quint16 bucket(qreal devicePixelRatio) noexcept;
    [[nodiscard]] static int physicalPixels(qreal logicalPixels, qreal devicePixelRatio) noexcept;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/src/ZzDpiScale.cpp` with:

```cpp
#include <ZzFluentUI/ZzDpiScale.h>

#include <algorithm>
#include <cmath>

namespace ZzFluentUI {

quint16 ZzDpiScale::bucket(qreal devicePixelRatio) noexcept
{
    const qreal valid = std::isfinite(devicePixelRatio) && devicePixelRatio > 0.0
        ? devicePixelRatio : 1.0;
    return static_cast<quint16>(std::clamp(std::lround(valid * 100.0), 50L, 800L));
}

int ZzDpiScale::physicalPixels(qreal logicalPixels, qreal devicePixelRatio) noexcept
{
    if (!std::isfinite(logicalPixels) || logicalPixels <= 0.0) {
        return 0;
    }
    const qreal ratio = static_cast<qreal>(bucket(devicePixelRatio)) / 100.0;
    return std::max(1, static_cast<int>(std::ceil(logicalPixels * ratio)));
}

} // namespace ZzFluentUI
```

- [ ] **Step 5: 创建图标描述和完整缓存键**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzIconDescriptor.h` with:

```cpp
#pragma once

#include <QtCore/QString>

namespace ZzFluentUI {

/** @brief 描述可由 Widgets 或未来 Quick 前端渲染的图标资源。 */
struct ZzIconDescriptor final
{
    QString resourceId;
    bool mirroredInRightToLeft = false;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzIconCacheKey.h` with:

```cpp
#pragma once

#include <cstddef>

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QtTypes>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/** @brief 标识一个与主题和设备像素比绑定的图标缓存项。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzIconCacheKey final
{
public:
    ZzIconCacheKey(QString resourceId, bool mirrored, QSize logicalSize,
                   quint16 dprBucket, quint32 rgba, quint64 themeRevision);
    [[nodiscard]] const QString &resourceId() const noexcept;
    [[nodiscard]] bool mirrored() const noexcept;
    [[nodiscard]] QSize logicalSize() const noexcept;
    [[nodiscard]] quint16 dprBucket() const noexcept;
    [[nodiscard]] quint32 rgba() const noexcept;
    [[nodiscard]] quint64 themeRevision() const noexcept;
    friend bool operator==(const ZzIconCacheKey &, const ZzIconCacheKey &) = default;

private:
    QString resourceId_;
    bool mirrored_;
    QSize logicalSize_;
    quint16 dprBucket_;
    quint32 rgba_;
    quint64 themeRevision_;
};

[[nodiscard]] ZZ_FLUENT_FOUNDATION_EXPORT std::size_t qHash(
    const ZzIconCacheKey &key,
    std::size_t seed = 0) noexcept;

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/src/ZzIconCacheKey.cpp` with:

```cpp
#include <ZzFluentUI/ZzIconCacheKey.h>

#include <QtCore/QHashFunctions>

#include <utility>

namespace ZzFluentUI {

ZzIconCacheKey::ZzIconCacheKey(QString resourceId, bool mirrored,
    QSize logicalSize, quint16 dprBucket, quint32 rgba, quint64 themeRevision)
    : resourceId_(std::move(resourceId)), mirrored_(mirrored),
      logicalSize_(logicalSize), dprBucket_(dprBucket), rgba_(rgba),
      themeRevision_(themeRevision)
{
}

const QString &ZzIconCacheKey::resourceId() const noexcept { return resourceId_; }
bool ZzIconCacheKey::mirrored() const noexcept { return mirrored_; }
QSize ZzIconCacheKey::logicalSize() const noexcept { return logicalSize_; }
quint16 ZzIconCacheKey::dprBucket() const noexcept { return dprBucket_; }
quint32 ZzIconCacheKey::rgba() const noexcept { return rgba_; }
quint64 ZzIconCacheKey::themeRevision() const noexcept { return themeRevision_; }

std::size_t qHash(const ZzIconCacheKey &key, std::size_t seed) noexcept
{
    seed = qHashMulti(seed, key.resourceId(), key.mirrored(),
                      key.logicalSize().width(), key.logicalSize().height(),
                      key.dprBucket(), key.rgba(), key.themeRevision());
    return seed;
}

} // namespace ZzFluentUI
```

- [ ] **Step 6: 加入源码并运行绿灯**

Add these exact entries to `set(zz_fluent_foundation_sources ...)` before `add_library(ZzFluentFoundation ...)`:

```cmake
foundation/src/ZzAnimationPolicy.cpp
foundation/src/ZzDpiScale.cpp
foundation/src/ZzIconCacheKey.cpp
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzAnimationDpiTest
ctest --preset linux-gcc-debug -R fluent.animation-dpi
```

Expected: build 和测试返回 0；三个 test slot 全部通过。

- [ ] **Step 7: 提交跨设备与可访问性策略**

```bash
git add ZzFluentUI
git commit -m "主题：增加动效 DPI 与图标缓存契约" \
    -m "统一减少动效、必要反馈时长和 DPR 像素对齐规则。" \
    -m "缓存键覆盖资源、逻辑尺寸、DPR、RGBA 和主题 revision，为 Widgets 与未来 Quick 提供无反向依赖的描述层。"
```

## Task 4: 实现四文件 ZzThemeController

**Files:**
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeChangeKind.h`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeController.h`
- Create: `ZzFluentUI/foundation/src/ZzThemeController.cpp`
- Create: `ZzFluentUI/foundation/src/private/ZzThemeControllerPrivate.h`
- Create: `ZzFluentUI/foundation/src/private/ZzThemeControllerPrivate.cpp`
- Create: `ZzFluentUI/tests/ZzThemeControllerTest.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Modify: `ZzFluentUI/CMakeLists.txt`

- [ ] **Step 1: 写模式、revision 和变更分类红灯测试**

Create `ZzFluentUI/tests/ZzThemeControllerTest.cpp` with:

```cpp
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtCore/QtGlobal>
#include <QtGui/QFont>
#include <QtGui/QGuiApplication>

#include <ZzFluentUI/ZzThemeController.h>

class ZzThemeControllerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void swapsCompleteSnapshots()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(&controller, &ZzFluentUI::ZzThemeController::snapshotChanged);
        const quint64 before = controller.snapshot()->revision();

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);

        QCOMPARE(controller.mode(), ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(controller.resolvedMode(), ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(controller.snapshot()->revision(), before + 1);
        QCOMPARE(spy.count(), 1);
        const auto changes = spy.at(0).at(1).value<ZzFluentUI::ZzThemeChangeKinds>();
        QVERIFY(changes.testFlag(ZzFluentUI::ZzThemeChangeKind::Colors));
    }

    void reportsAccessibilityChanges()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(&controller, &ZzFluentUI::ZzThemeController::snapshotChanged);
        controller.setReducedMotion(true);
        QVERIFY(controller.reducedMotion());
        QCOMPARE(controller.snapshot()->duration(ZzFluentUI::ZzMotionToken::Normal), 0);
        QVERIFY(spy.at(0).at(1).value<ZzFluentUI::ZzThemeChangeKinds>().testFlag(
            ZzFluentUI::ZzThemeChangeKind::Accessibility));
    }

    void ignoresEquivalentAssignments()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(&controller, &ZzFluentUI::ZzThemeController::snapshotChanged);
        controller.setAccentColor(controller.accentColor());
        QCOMPARE(spy.count(), 0);
    }

    void reportsApplicationFontGeometryChanges()
    {
        ZzFluentUI::ZzThemeController controller;
        QSignalSpy spy(&controller, &ZzFluentUI::ZzThemeController::snapshotChanged);
        const QFont bodyBefore =
            controller.snapshot()->font(ZzFluentUI::ZzTypographyToken::Body);
        const QFont original = QGuiApplication::font();
        QFont changed = original;
        if (changed.pointSizeF() > 0.0) {
            changed.setPointSizeF(changed.pointSizeF() + 1.0);
        } else {
            changed.setPixelSize(qMax(1, changed.pixelSize()) + 1);
        }

        QGuiApplication::setFont(changed);
        QTRY_COMPARE(spy.count(), 1);
        const auto changes =
            spy.at(0).at(1).value<ZzFluentUI::ZzThemeChangeKinds>();
        QVERIFY(changes.testFlag(ZzFluentUI::ZzThemeChangeKind::Geometry));
        const QFont bodyAfter =
            controller.snapshot()->font(ZzFluentUI::ZzTypographyToken::Body);
        QVERIFY(bodyAfter != bodyBefore);

        QGuiApplication::setFont(original);
    }
};

QTEST_MAIN(ZzThemeControllerTest)

#include "ZzThemeControllerTest.moc"
```

- [ ] **Step 2: 注册测试并确认缺失控制器**

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzThemeControllerTest ZzThemeControllerTest.cpp)
target_link_libraries(ZzThemeControllerTest PRIVATE Qt6::Test Zz::FluentFoundation)
set_target_properties(ZzThemeControllerTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzThemeControllerTest)
zz_enable_sanitizers(ZzThemeControllerTest)
add_test(NAME fluent.theme-controller COMMAND ZzThemeControllerTest)
set_tests_properties(fluent.theme-controller PROPERTIES LABELS "component;fluent;foundation")
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzThemeControllerTest
```

Expected: compile FAIL，指出 `ZzThemeController.h` 不存在。

- [ ] **Step 3: 创建变更 flags 完整头**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeChangeKind.h` with:

```cpp
#pragma once

#include <cstdint>

#include <QtCore/QFlags>
#include <QtCore/QMetaType>

namespace ZzFluentUI {

/** @brief 标识消费者需要执行的主题更新类别。 */
enum class ZzThemeChangeKind : std::uint8_t
{
    None = 0,
    Colors = 1U << 0U,
    Geometry = 1U << 1U,
    Motion = 1U << 2U,
    Accessibility = 1U << 3U
};
Q_DECLARE_FLAGS(ZzThemeChangeKinds, ZzThemeChangeKind)
Q_DECLARE_OPERATORS_FOR_FLAGS(ZzThemeChangeKinds)

} // namespace ZzFluentUI

Q_DECLARE_METATYPE(ZzFluentUI::ZzThemeChangeKinds)
```

- [ ] **Step 4: 创建控制器公开头和中文 Doxygen**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzThemeController.h` with:

```cpp
#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtGui/QColor>

#include <ZzFluentUI/ZzFluentFoundationExport.h>
#include <ZzFluentUI/ZzThemeChangeKind.h>
#include <ZzFluentUI/ZzThemeMode.h>

class QEvent;

namespace ZzFluentUI {

class ZzThemeControllerPrivate;
class ZzThemeSnapshot;

/** @brief 在 GUI 线程管理应用级 Fluent 主题快照。 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzThemeController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzThemeController)

public:
    /** @brief 构造控制器；parent 可为空且不改变快照所有权，必须在 GUI 线程调用。 */
    explicit ZzThemeController(QObject *parent = nullptr);
    ~ZzThemeController() override;

    /** @brief 返回请求模式；仅可在控制器线程调用。 */
    [[nodiscard]] ZzThemeMode mode() const noexcept;
    /** @brief 返回 System 解析后的模式；仅可在控制器线程调用。 */
    [[nodiscard]] ZzThemeMode resolvedMode() const noexcept;
    /**
     * @brief 返回共享只读快照；shared_ptr 延长其生命周期且不转移可变所有权。
     * @return 可复制的不可变快照所有权；取得后可传给其他线程只读使用。
     * @note 读取控制器当前指针本身必须发生在控制器所属 GUI 线程。
     */
    [[nodiscard]] std::shared_ptr<const ZzThemeSnapshot> snapshot() const noexcept;
    /** @brief 返回当前强调色；仅可在控制器线程调用。 */
    [[nodiscard]] QColor accentColor() const;
    /** @brief 返回是否减少非必要动效；仅可在控制器线程调用。 */
    [[nodiscard]] bool reducedMotion() const noexcept;

    /** @brief 切换主题来源；等价值不发信号，必须在控制器线程调用。 */
    void setMode(ZzThemeMode mode);
    /** @brief 设置强调色；无效颜色恢复默认色，必须在控制器线程调用。 */
    void setAccentColor(const QColor &color);
    /** @brief 设置可访问性动效偏好；必须在控制器线程调用。 */
    void setReducedMotion(bool reducedMotion);

Q_SIGNALS:
    /**
     * @brief 在完整快照交换后发出。
     * @param revision 新快照版本。
     * @param changes 消费者需要执行的更新类别。
     * @note 信号在控制器所属 GUI 线程同步发出，不转移快照所有权。
     */
    void snapshotChanged(quint64 revision, ZzThemeChangeKinds changes);

protected:
    /** @brief 监听应用字体变化并重建几何令牌；调用方不得直接调用。 */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::unique_ptr<ZzThemeControllerPrivate> d_ptr;
};

} // namespace ZzFluentUI
```

- [ ] **Step 5: 创建 private 声明和 public 转发**

Create `ZzFluentUI/foundation/src/private/ZzThemeControllerPrivate.h` with:

```cpp
#pragma once

#include <memory>

#include <QtCore/QMetaObject>
#include <QtGui/QColor>

#include <ZzFluentUI/ZzThemeChangeKind.h>
#include <ZzFluentUI/ZzThemeMode.h>

namespace ZzFluentUI {

class ZzThemeController;
class ZzThemeSnapshot;

class ZzThemeControllerPrivate final
{
public:
    explicit ZzThemeControllerPrivate(ZzThemeController *q);
    void rebuild(ZzThemeChangeKinds changes);
    [[nodiscard]] ZzThemeMode resolveSystemMode() const noexcept;

    ZzThemeController *const q_ptr;
    ZzThemeMode requestedMode = ZzThemeMode::System;
    ZzThemeMode activeMode = ZzThemeMode::Light;
    QColor accent = QColor(QStringLiteral("#0067c0"));
    bool reduceMotion = false;
    quint64 revision = 0;
    std::shared_ptr<const ZzThemeSnapshot> currentSnapshot;
    QMetaObject::Connection colorSchemeConnection;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/src/ZzThemeController.cpp` with:

```cpp
#include <ZzFluentUI/ZzThemeController.h>

#include "private/ZzThemeControllerPrivate.h"

#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QGuiApplication>

#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzThemeController::ZzThemeController(QObject *parent)
    : QObject(parent), d_ptr(std::make_unique<ZzThemeControllerPrivate>(this))
{
}

ZzThemeController::~ZzThemeController() = default;
ZzThemeMode ZzThemeController::mode() const noexcept { return d_ptr->requestedMode; }
ZzThemeMode ZzThemeController::resolvedMode() const noexcept { return d_ptr->activeMode; }
std::shared_ptr<const ZzThemeSnapshot> ZzThemeController::snapshot() const noexcept { return d_ptr->currentSnapshot; }
QColor ZzThemeController::accentColor() const { return d_ptr->accent; }
bool ZzThemeController::reducedMotion() const noexcept { return d_ptr->reduceMotion; }

void ZzThemeController::setMode(ZzThemeMode mode)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (d_ptr->requestedMode == mode) return;
    d_ptr->requestedMode = mode;
    d_ptr->rebuild(ZzThemeChangeKind::Colors);
}

void ZzThemeController::setAccentColor(const QColor &color)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const QColor valid = color.isValid() ? color : QColor(QStringLiteral("#0067c0"));
    if (d_ptr->accent == valid) return;
    d_ptr->accent = valid;
    d_ptr->rebuild(ZzThemeChangeKind::Colors);
}

void ZzThemeController::setReducedMotion(bool reducedMotion)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (d_ptr->reduceMotion == reducedMotion) return;
    d_ptr->reduceMotion = reducedMotion;
    d_ptr->rebuild(ZzThemeChangeKind::Motion | ZzThemeChangeKind::Accessibility);
}

bool ZzThemeController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == QGuiApplication::instance()
        && event != nullptr
        && event->type() == QEvent::ApplicationFontChange) {
        d_ptr->rebuild(ZzThemeChangeKind::Geometry);
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ZzFluentUI
```

- [ ] **Step 6: 实现系统主题监听和单次交换**

Create `ZzFluentUI/foundation/src/private/ZzThemeControllerPrivate.cpp` with:

```cpp
#include "ZzThemeControllerPrivate.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include <QtCore/QThread>

#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzThemeControllerPrivate::ZzThemeControllerPrivate(ZzThemeController *q)
    : q_ptr(q)
{
    activeMode = resolveSystemMode();
    currentSnapshot = std::make_shared<const ZzThemeSnapshot>(
        ZzThemeSnapshot::create(activeMode, accent, revision, reduceMotion));
    if (QGuiApplication::instance() != nullptr) {
        QGuiApplication::instance()->installEventFilter(q_ptr);
    }
    if (QGuiApplication::styleHints() != nullptr) {
        colorSchemeConnection = QObject::connect(
            QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            q_ptr, [this] {
                if (requestedMode == ZzThemeMode::System) {
                    rebuild(ZzThemeChangeKind::Colors);
                }
            });
    }
}

ZzThemeMode ZzThemeControllerPrivate::resolveSystemMode() const noexcept
{
    if (requestedMode != ZzThemeMode::System) return requestedMode;
    const QStyleHints *hints = QGuiApplication::styleHints();
    return hints != nullptr && hints->colorScheme() == Qt::ColorScheme::Dark
        ? ZzThemeMode::Dark : ZzThemeMode::Light;
}

void ZzThemeControllerPrivate::rebuild(ZzThemeChangeKinds changes)
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    activeMode = resolveSystemMode();
    const quint64 nextRevision = revision + 1;
    auto next = std::make_shared<const ZzThemeSnapshot>(
        ZzThemeSnapshot::create(activeMode, accent, nextRevision, reduceMotion));
    currentSnapshot.swap(next);
    revision = nextRevision;
    Q_EMIT q_ptr->snapshotChanged(revision, changes);
}

} // namespace ZzFluentUI
```

- [ ] **Step 7: 加入源码并运行控制器测试**

Add these exact entries to `set(zz_fluent_foundation_sources ...)` before `add_library(ZzFluentFoundation ...)`:

```cmake
foundation/src/ZzThemeController.cpp
foundation/src/private/ZzThemeControllerPrivate.cpp
```

At the same location, replace the complete AUTOMOC-only list with:

```cmake
set(zz_fluent_foundation_moc_headers
    foundation/include/ZzFluentUI/ZzThemeController.h
)
```

`zz_fluent_foundation_moc_headers` is passed through the baseline helper's `MOC_HEADERS` argument. Do not place this public header in `zz_fluent_foundation_sources`; that list remains `.cpp`-only for clang-tidy.

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzThemeControllerTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R fluent.theme-controller
```

Expected: build 和测试返回 0；四次测试均通过，等值赋值不增加 revision，应用字体变化产生 `Geometry` 分类。

- [ ] **Step 8: 提交主题控制器**

```bash
git add ZzFluentUI
git commit -m "主题：实现应用级主题控制器" \
    -m "以四文件 PIMPL 管理 System、Light、Dark 和 HighContrast 模式。" \
    -m "在 GUI 线程构造完整快照后一次交换，并区分颜色、动效和可访问性更新。"
```

## Task 5: 实现无分配视觉槽和有界图标缓存

**Files:**
- Create: `ZzFluentUI/widgets/src/private/ZzStyleCache.h`
- Create: `ZzFluentUI/widgets/src/private/ZzStyleCache.cpp`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentPainter.h`
- Create: `ZzFluentUI/widgets/src/ZzFluentPainter.cpp`
- Create: `ZzFluentUI/tests/ZzFluentStyleTest.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Modify: `ZzFluentUI/CMakeLists.txt`

- [ ] **Step 1: 写缓存容量和绘制原语红灯测试**

Create `ZzFluentUI/tests/ZzFluentStyleTest.cpp` with:

```cpp
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QTest>

#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

class ZzFluentStyleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void drawsVisibleHighContrastFocusRing()
    {
        QImage image(QSize(64, 32), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        QPainter painter(&image);
        const auto snapshot = ZzFluentUI::ZzThemeSnapshot::create(
            ZzFluentUI::ZzThemeMode::HighContrast, QColor(Qt::yellow), 1, true);
        ZzFluentUI::ZzFluentPainter::drawFocusRing(
            &painter, QRectF(2, 2, 60, 28), snapshot, 1.0);
        painter.end();
        QVERIFY(image.pixelColor(2, 16) != QColor(Qt::black));
    }
};

QTEST_MAIN(ZzFluentStyleTest)

#include "ZzFluentStyleTest.moc"
```

- [ ] **Step 2: 注册测试并确认缺失 painter**

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
add_executable(ZzFluentStyleTest ZzFluentStyleTest.cpp)
target_link_libraries(ZzFluentStyleTest PRIVATE Qt6::Test Zz::FluentUI)
set_target_properties(ZzFluentStyleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentStyleTest)
zz_enable_sanitizers(ZzFluentStyleTest)
add_test(NAME fluent.style COMMAND ZzFluentStyleTest)
set_tests_properties(fluent.style PROPERTIES
    LABELS "unit;fluent;widgets"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStyleTest
```

Expected: compile FAIL，指出 `ZzFluentPainter.h` 不存在。

- [ ] **Step 3: 创建 painter 公开接口**

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentPainter.h` with:

```cpp
#pragma once

#include <QtCore/QRectF>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QPainter;

namespace ZzFluentUI {

class ZzThemeSnapshot;

/** @brief 提供不拥有状态、不读取文件的 Fluent 绘制原语。 */
class ZZ_FLUENT_UI_EXPORT ZzFluentPainter final
{
public:
    ZzFluentPainter() = delete;
    /** @brief 绘制控件背景；painter 必须非空且已激活，调用线程必须是控件线程。 */
    static void drawControlBackground(QPainter *painter, const QRectF &rect,
        const ZzThemeSnapshot &snapshot, bool hovered, bool pressed, bool enabled);
    /** @brief 绘制可见焦点环；painter 必须非空且 DPR 为正值。 */
    static void drawFocusRing(QPainter *painter, const QRectF &rect,
        const ZzThemeSnapshot &snapshot, qreal devicePixelRatio);
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/src/ZzFluentPainter.cpp` with:

```cpp
#include <ZzFluentUI/ZzFluentPainter.h>

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

void ZzFluentPainter::drawControlBackground(QPainter *painter, const QRectF &rect,
    const ZzThemeSnapshot &snapshot, bool hovered, bool pressed, bool enabled)
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    ZzColorToken token = ZzColorToken::ControlFill;
    if (!enabled) token = ZzColorToken::ControlFillDisabled;
    else if (pressed) token = ZzColorToken::ControlFillPressed;
    else if (hovered) token = ZzColorToken::ControlFillHover;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(snapshot.color(ZzColorToken::ControlStroke),
                         snapshot.metric(ZzMetricToken::StrokeThin)));
    painter->setBrush(snapshot.color(token));
    painter->drawRoundedRect(rect, snapshot.metric(ZzMetricToken::CornerRadiusMedium),
                             snapshot.metric(ZzMetricToken::CornerRadiusMedium));
    painter->restore();
}

void ZzFluentPainter::drawFocusRing(QPainter *painter, const QRectF &rect,
    const ZzThemeSnapshot &snapshot, qreal devicePixelRatio)
{
    Q_ASSERT(painter != nullptr && painter->isActive());
    const int physicalWidth = ZzDpiScale::physicalPixels(
        snapshot.metric(ZzMetricToken::FocusStrokeWidth), devicePixelRatio);
    const qreal logicalWidth = physicalWidth /
        (static_cast<qreal>(ZzDpiScale::bucket(devicePixelRatio)) / 100.0);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(snapshot.color(ZzColorToken::FocusStroke), logicalWidth));
    painter->drawRoundedRect(rect.adjusted(logicalWidth / 2.0, logicalWidth / 2.0,
                                            -logicalWidth / 2.0, -logicalWidth / 2.0),
                             snapshot.metric(ZzMetricToken::CornerRadiusMedium),
                             snapshot.metric(ZzMetricToken::CornerRadiusMedium));
    painter->restore();
}

} // namespace ZzFluentUI
```

- [ ] **Step 4: 创建私有缓存完整接口**

Create `ZzFluentUI/widgets/src/private/ZzStyleCache.h` with:

```cpp
#pragma once

#include <array>
#include <QtCore/QCache>
#include <QtGui/QBrush>
#include <QtGui/QPixmap>

#include <ZzFluentUI/ZzIconCacheKey.h>

namespace ZzFluentUI {

class ZzThemeSnapshot;

struct ZzStyleVisual final
{
    QBrush fill;
    QBrush stroke;
};

class ZzStyleCache final
{
public:
    explicit ZzStyleCache(int maximumIconBytes);
    void rebuildVisuals(const ZzThemeSnapshot &snapshot);
    [[nodiscard]] const ZzStyleVisual &visual(std::size_t stateIndex) const noexcept;
    [[nodiscard]] const QPixmap *icon(const ZzIconCacheKey &key) const noexcept;
    void insertIcon(const ZzIconCacheKey &key, QPixmap pixmap);
    void clearIcons() noexcept;
    [[nodiscard]] int iconBytes() const noexcept;

private:
    std::array<ZzStyleVisual, 4> visuals_;
    QCache<ZzIconCacheKey, QPixmap> icons_;
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/src/private/ZzStyleCache.cpp` with:

```cpp
#include "ZzStyleCache.h"

#include <QtCore/QtGlobal>
#include <QtGui/QImage>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzStyleCache::ZzStyleCache(int maximumIconBytes)
    : icons_(qMax(0, maximumIconBytes))
{
}

void ZzStyleCache::rebuildVisuals(const ZzThemeSnapshot &snapshot)
{
    visuals_[0] = {snapshot.color(ZzColorToken::ControlFill), snapshot.color(ZzColorToken::ControlStroke)};
    visuals_[1] = {snapshot.color(ZzColorToken::ControlFillHover), snapshot.color(ZzColorToken::ControlStroke)};
    visuals_[2] = {snapshot.color(ZzColorToken::ControlFillPressed), snapshot.color(ZzColorToken::ControlStroke)};
    visuals_[3] = {snapshot.color(ZzColorToken::ControlFillDisabled), snapshot.color(ZzColorToken::ControlStroke)};
}

const ZzStyleVisual &ZzStyleCache::visual(std::size_t stateIndex) const noexcept
{
    if (stateIndex >= visuals_.size()) {
        Q_ASSERT(false);
        return visuals_[0];
    }
    return visuals_[stateIndex];
}

const QPixmap *ZzStyleCache::icon(const ZzIconCacheKey &key) const noexcept
{
    return icons_.object(key);
}

void ZzStyleCache::insertIcon(const ZzIconCacheKey &key, QPixmap pixmap)
{
    const qsizetype bytes = static_cast<qsizetype>(pixmap.width())
        * static_cast<qsizetype>(pixmap.height()) * 4;
    if (bytes <= 0 || bytes > icons_.maxCost()) {
        return;
    }
    icons_.insert(key, new QPixmap(std::move(pixmap)), static_cast<int>(bytes));
}

void ZzStyleCache::clearIcons() noexcept { icons_.clear(); }
int ZzStyleCache::iconBytes() const noexcept { return icons_.totalCost(); }

} // namespace ZzFluentUI
```

- [ ] **Step 5: 加入 painter 源码并运行绿灯**

Add these exact entries to `set(zz_fluent_ui_sources ...)` before `add_library(ZzFluentUI ...)`:

```cmake
widgets/src/ZzFluentPainter.cpp
widgets/src/private/ZzStyleCache.cpp
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStyleTest
ctest --preset linux-gcc-debug -R fluent.style
```

Expected: build 和测试返回 0；高对比度焦点环在图像边缘产生非黑像素。

- [ ] **Step 6: 提交绘制和缓存基础**

```bash
git add ZzFluentUI
git commit -m "样式：实现 Fluent 绘制原语与有界缓存" \
    -m "增加 DPR 对齐的控件背景和高对比度焦点环。" \
    -m "热状态使用固定槽读取，图标缓存以字节预算淘汰并在完整主题键上命中。"
```

## Task 6: 实现四文件 ZzFluentStyle 和主题传播

**Files:**
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h`
- Create: `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`
- Modify: `ZzFluentUI/tests/ZzFluentStyleTest.cpp`
- Create: `ZzFluentUI/tests/resources/ZzFluentTestSquare.svg`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Modify: `ZzFluentUI/CMakeLists.txt`

- [ ] **Step 1: 扩展 style 行为红灯测试**

Replace `ZzFluentUI/tests/ZzFluentStyleTest.cpp` with:

```cpp
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QPushButton>

#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeController.h>

class ZzFluentStyleTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mapsMetricsAndPalette()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QCOMPARE(style.pixelMetric(QStyle::PM_ButtonMargin), 12);
        QCOMPARE(style.standardPalette().color(QPalette::Window),
                 controller.snapshot()->color(ZzFluentUI::ZzColorToken::Surface));
    }

    void invalidatesColorCacheWithoutChangingMetric()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const int metric = style.pixelMetric(QStyle::PM_ButtonMargin);
        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(style.themeRevision(), controller.snapshot()->revision());
        QCOMPARE(style.pixelMetric(QStyle::PM_ButtonMargin), metric);
        QCOMPARE(style.iconCacheBytes(), 0);
    }

    void exposesAccessibleFocusPolicy()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        QVERIFY(style.styleHint(QStyle::SH_UnderlineShortcut) >= 0);
        QVERIFY(style.pixelMetric(QStyle::PM_FocusFrameHMargin) >= 2);
    }

    void cachesTintedResourceIcons()
    {
        ZzFluentUI::ZzThemeController controller;
        ZzFluentUI::ZzFluentStyle style(&controller);
        const ZzFluentUI::ZzIconDescriptor descriptor{
            QStringLiteral(":/zzfluent/tests/ZzFluentTestSquare.svg"), true};

        const QPixmap first = style.iconPixmap(
            descriptor, QSize(16, 16), 1.25, QColor(Qt::green),
            Qt::LeftToRight);
        QVERIFY(!first.isNull());
        const int firstCost = style.iconCacheBytes();
        QVERIFY(firstCost > 0);

        const QPixmap second = style.iconPixmap(
            descriptor, QSize(16, 16), 1.25, QColor(Qt::green),
            Qt::LeftToRight);
        QCOMPARE(second.cacheKey(), first.cacheKey());
        QCOMPARE(style.iconCacheBytes(), firstCost);

        const QPixmap mirrored = style.iconPixmap(
            descriptor, QSize(16, 16), 1.25, QColor(Qt::green),
            Qt::RightToLeft);
        QVERIFY(!mirrored.isNull());
        const int mirroredCost = style.iconCacheBytes();
        QVERIFY(mirroredCost > firstCost);

        controller.setReducedMotion(true);
        QCOMPARE(style.iconCacheBytes(), mirroredCost);
        const QPixmap afterMotionChange = style.iconPixmap(
            descriptor, QSize(16, 16), 1.25, QColor(Qt::green),
            Qt::LeftToRight);
        QCOMPARE(afterMotionChange.cacheKey(), first.cacheKey());
        QCOMPARE(style.iconCacheBytes(), mirroredCost);

        controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
        QCOMPARE(style.iconCacheBytes(), 0);
    }
};

QTEST_MAIN(ZzFluentStyleTest)

#include "ZzFluentStyleTest.moc"
```

Create `ZzFluentUI/tests/resources/ZzFluentTestSquare.svg` as a repository-owned test-only asset:

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">
  <path d="M2 3h9v3h3v7H5v-3H2z" fill="#000000"/>
</svg>
```

Append to `ZzFluentUI/tests/CMakeLists.txt` after the existing `ZzFluentStyleTest` target declaration:

```cmake
qt_add_resources(ZzFluentStyleTest "zzfluent-test-icons"
    PREFIX "/zzfluent/tests"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/ZzFluentTestSquare.svg
)
```

- [ ] **Step 2: 运行并确认缺失 style**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStyleTest
```

Expected: compile FAIL，指出 `ZzFluentStyle.h` 不存在。

- [ ] **Step 3: 创建公开 QProxyStyle 接口**

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentStyle.h` with:

```cpp
#pragma once

#include <memory>

#include <QtCore/QSize>
#include <QtCore/Qt>
#include <QtGui/QColor>
#include <QtGui/QPixmap>
#include <QtWidgets/QProxyStyle>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzFluentStylePrivate;
class ZzThemeController;

/** @brief 在保留平台基础行为的同时应用 Fluent 主题令牌。 */
class ZZ_FLUENT_UI_EXPORT ZzFluentStyle final : public QProxyStyle
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzFluentStyle)

public:
    /**
     * @brief 构造应用级样式。
     * @param controller 非空、非拥有，必须与样式同属 GUI 线程并比样式长寿。
     * @param baseStyle 可为空；非空时所有权交给 QProxyStyle。
     */
    explicit ZzFluentStyle(ZzThemeController *controller, QStyle *baseStyle = nullptr);
    ~ZzFluentStyle() override;

    [[nodiscard]] quint64 themeRevision() const noexcept;
    [[nodiscard]] int iconCacheBytes() const noexcept;

    /**
     * @brief 从 Qt 资源渲染、着色并缓存指定 DPR 的图标。
     * @param descriptor resourceId 必须以 :/ 开头；按描述决定 RTL 镜像。
     * @param logicalSize 非空的设备无关尺寸。
     * @param devicePixelRatio 设备像素比；无效值按 1.0 处理并量化。
     * @param color 有效的目标颜色。
     * @param direction 当前布局方向。
     * @return 命中或新生成的隐式共享 pixmap；输入或资源无效时返回空值。
     * @note 只能在样式所属 GUI 线程调用；缓存 miss 才读取 Qt resource 和渲染。
     */
    [[nodiscard]] QPixmap iconPixmap(
        const ZzIconDescriptor &descriptor,
        QSize logicalSize,
        qreal devicePixelRatio,
        QColor color,
        Qt::LayoutDirection direction = Qt::LeftToRight);

    [[nodiscard]] int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                                  const QWidget *widget = nullptr) const override;
    [[nodiscard]] int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                                const QWidget *widget = nullptr,
                                QStyleHintReturn *returnData = nullptr) const override;
    [[nodiscard]] QPalette standardPalette() const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override;

private:
    std::unique_ptr<ZzFluentStylePrivate> d_ptr;
};

} // namespace ZzFluentUI
```

- [ ] **Step 4: 创建 style private 完整声明**

Create `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h` with:

```cpp
#pragma once

#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QSize>
#include <QtCore/Qt>
#include <QtGui/QColor>
#include <QtGui/QPixmap>

#include "ZzStyleCache.h"

#include <ZzFluentUI/ZzThemeChangeKind.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzFluentStyle;
class ZzThemeController;
class ZzThemeSnapshot;

class ZzFluentStylePrivate final
{
public:
    ZzFluentStylePrivate(ZzFluentStyle *q, ZzThemeController *controller);
    [[nodiscard]] QPixmap iconPixmap(
        const ZzIconDescriptor &descriptor,
        QSize logicalSize,
        qreal devicePixelRatio,
        QColor color,
        Qt::LayoutDirection direction);
    void applySnapshot(ZzThemeChangeKinds changes);

    ZzFluentStyle *const q_ptr;
    QPointer<ZzThemeController> controller;
    std::shared_ptr<const ZzThemeSnapshot> snapshot;
    quint64 iconRevision = 0;
    ZzStyleCache cache{4 * 1024 * 1024};
};

} // namespace ZzFluentUI
```

- [ ] **Step 5: 创建 public 转发和 override**

Create `ZzFluentUI/widgets/src/ZzFluentStyle.cpp` with:

```cpp
#include <ZzFluentUI/ZzFluentStyle.h>

#include "private/ZzFluentStylePrivate.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyleOption>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzFluentStyle::ZzFluentStyle(ZzThemeController *controller, QStyle *baseStyle)
    : QProxyStyle(baseStyle), d_ptr(std::make_unique<ZzFluentStylePrivate>(this, controller))
{
}

ZzFluentStyle::~ZzFluentStyle() = default;
quint64 ZzFluentStyle::themeRevision() const noexcept { return d_ptr->snapshot->revision(); }
int ZzFluentStyle::iconCacheBytes() const noexcept { return d_ptr->cache.iconBytes(); }

QPixmap ZzFluentStyle::iconPixmap(
    const ZzIconDescriptor &descriptor,
    QSize logicalSize,
    qreal devicePixelRatio,
    QColor color,
    Qt::LayoutDirection direction)
{
    return d_ptr->iconPixmap(
        descriptor, logicalSize, devicePixelRatio, color, direction);
}

int ZzFluentStyle::pixelMetric(PixelMetric metric, const QStyleOption *option,
    const QWidget *widget) const
{
    if (metric == PM_ButtonMargin) return qRound(d_ptr->snapshot->metric(ZzMetricToken::HorizontalPadding));
    if (metric == PM_FocusFrameHMargin || metric == PM_FocusFrameVMargin)
        return qRound(d_ptr->snapshot->metric(ZzMetricToken::FocusStrokeWidth));
    return QProxyStyle::pixelMetric(metric, option, widget);
}

int ZzFluentStyle::styleHint(StyleHint hint, const QStyleOption *option,
    const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == SH_Widget_Animate) {
        if (d_ptr->snapshot->reducedMotion()) {
            return 0;
        }
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

QPalette ZzFluentStyle::standardPalette() const
{
    QPalette palette = QProxyStyle::standardPalette();
    palette.setColor(QPalette::Window, d_ptr->snapshot->color(ZzColorToken::Surface));
    palette.setColor(QPalette::Base, d_ptr->snapshot->color(ZzColorToken::SurfaceSecondary));
    palette.setColor(QPalette::Text, d_ptr->snapshot->color(ZzColorToken::TextPrimary));
    palette.setColor(QPalette::ButtonText, d_ptr->snapshot->color(ZzColorToken::TextPrimary));
    palette.setColor(QPalette::Highlight, d_ptr->snapshot->color(ZzColorToken::Accent));
    palette.setColor(QPalette::HighlightedText, d_ptr->snapshot->color(ZzColorToken::AccentText));
    return palette;
}

void ZzFluentStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
    QPainter *painter, const QWidget *widget) const
{
    if (element == PE_FrameFocusRect && option != nullptr && painter != nullptr) {
        const qreal dpr = widget != nullptr ? widget->devicePixelRatioF() : 1.0;
        ZzFluentPainter::drawFocusRing(painter, option->rect, *d_ptr->snapshot, dpr);
        return;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

} // namespace ZzFluentUI
```

- [ ] **Step 6: 实现主题传播和缓存失效**

Create `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp` with:

```cpp
#include "ZzFluentStylePrivate.h"

#include <utility>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzDpiScale.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconCacheKey.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzFluentStylePrivate::ZzFluentStylePrivate(
    ZzFluentStyle *q,
    ZzThemeController *themeController)
    : q_ptr(q), controller(themeController)
{
    Q_ASSERT(themeController != nullptr);
    Q_ASSERT(themeController->thread() == q->thread());
    snapshot = themeController->snapshot();
    iconRevision = snapshot->revision();
    cache.rebuildVisuals(*snapshot);
    QObject::connect(themeController, &ZzThemeController::snapshotChanged,
                     q, [this](quint64, ZzThemeChangeKinds changes) {
                         applySnapshot(changes);
                     });
}

QPixmap ZzFluentStylePrivate::iconPixmap(
    const ZzIconDescriptor &descriptor,
    QSize logicalSize,
    qreal devicePixelRatio,
    QColor color,
    Qt::LayoutDirection direction)
{
    Q_ASSERT(QThread::currentThread() == q_ptr->thread());
    if (!descriptor.resourceId.startsWith(QStringLiteral(":/"))
        || logicalSize.isEmpty() || !color.isValid()) {
        return {};
    }

    const quint16 dprBucket = ZzDpiScale::bucket(devicePixelRatio);
    const qreal effectiveDpr = static_cast<qreal>(dprBucket) / 100.0;
    const bool mirrored = descriptor.mirroredInRightToLeft
        && direction == Qt::RightToLeft;
    const ZzIconCacheKey key(
        descriptor.resourceId, mirrored, logicalSize, dprBucket, color.rgba(),
        iconRevision);
    if (const QPixmap *cached = cache.icon(key); cached != nullptr) {
        return *cached;
    }

    QSvgRenderer renderer(descriptor.resourceId);
    if (!renderer.isValid()) {
        return {};
    }

    const QSize physicalSize(
        ZzDpiScale::physicalPixels(logicalSize.width(), effectiveDpr),
        ZzDpiScale::physicalPixels(logicalSize.height(), effectiveDpr));
    QImage image(physicalSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter, QRectF(
        0.0, 0.0, physicalSize.width(), physicalSize.height()));
    painter.end();
    if (mirrored) {
        image = image.mirrored(true, false);
    }
    QPainter tintPainter(&image);
    tintPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tintPainter.fillRect(image.rect(), color);
    tintPainter.end();

    QPixmap rendered = QPixmap::fromImage(std::move(image));
    rendered.setDevicePixelRatio(effectiveDpr);
    cache.insertIcon(key, rendered);
    return rendered;
}

void ZzFluentStylePrivate::applySnapshot(ZzThemeChangeKinds changes)
{
    Q_ASSERT(controller != nullptr);
    snapshot = controller->snapshot();
    const bool colorsChanged =
        changes.testFlag(ZzThemeChangeKind::Colors);
    const bool geometryChanged =
        changes.testFlag(ZzThemeChangeKind::Geometry);

    if (colorsChanged) {
        cache.rebuildVisuals(*snapshot);
        cache.clearIcons();
        iconRevision = snapshot->revision();
        QApplication::setPalette(q_ptr->standardPalette());
    }
    if (!colorsChanged && !geometryChanged) {
        return;
    }

    const auto widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        if (geometryChanged) {
            QEvent event(QEvent::StyleChange);
            QCoreApplication::sendEvent(widget, &event);
            widget->updateGeometry();
        }
        widget->update();
    }
}

} // namespace ZzFluentUI
```

- [ ] **Step 7: 加入 style 四文件并运行绿灯**

Add these exact entries to `set(zz_fluent_ui_sources ...)` before `add_library(ZzFluentUI ...)`:

```cmake
widgets/src/ZzFluentStyle.cpp
widgets/src/private/ZzFluentStylePrivate.cpp
```

At the same location, replace the complete AUTOMOC-only list with:

```cmake
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
)
```

Keep the public `Q_OBJECT` header out of `zz_fluent_ui_sources`; the baseline helper passes the separate list to AUTOMOC without treating it as a clang-tidy translation unit.

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStyleTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R fluent.style
```

Expected: build 和测试返回 0；颜色切换 revision 同步、metric 稳定、缓存清空、焦点尺寸不小于 2；几何类更新会让顶层与子控件各收到一次 `StyleChange`。

- [ ] **Step 8: 提交 Widgets style**

```bash
git add ZzFluentUI
git commit -m "样式：实现应用级 Fluent ProxyStyle" \
    -m "以四文件 PIMPL 组合主题控制器、快照和有界缓存。" \
    -m "颜色变化只刷新 palette 与绘制，几何变化才发送 StyleChange，保留平台快捷键和基础控件行为。"
```

## Task 7: 建立 500 控件性能门禁和 Linux 真实运行 smoke

**Files:**
- Create: `ZzFluentUI/tests/ZzThemeSwitchBenchmark.cpp`
- Modify: `ZzFluentUI/tests/CMakeLists.txt`
- Create: `examples/ZzFluentFoundationDemo/CMakeLists.txt`
- Create: `examples/ZzFluentFoundationDemo/main.cpp`
- Modify: `examples/CMakeLists.txt`

- [ ] **Step 1: 写固定 500 控件 benchmark**

Create `ZzFluentUI/tests/ZzThemeSwitchBenchmark.cpp` with:

```cpp
#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

#include <QtCore/QtGlobal>
#include <QtTest/QTest>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

class ZzThemeSwitchBenchmark final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void switchesFiveHundredVisibleControls()
    {
        ZzFluentUI::ZzThemeController controller;
        auto style = std::make_unique<ZzFluentUI::ZzFluentStyle>(&controller);
        QWidget window;
        window.setStyle(style.get());
        auto *layout = new QGridLayout(&window);
        for (int index = 0; index < 500; ++index)
            layout->addWidget(new QPushButton(QString::number(index)), index / 25, index % 25);
        window.show();
        QCoreApplication::processEvents();

        std::vector<double> samples;
        samples.reserve(100);
        for (int iteration = 0; iteration < 110; ++iteration) {
            const auto start = std::chrono::steady_clock::now();
            controller.setMode(iteration % 2 == 0
                ? ZzFluentUI::ZzThemeMode::Dark : ZzFluentUI::ZzThemeMode::Light);
            QCoreApplication::processEvents();
            const auto elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            if (iteration >= 10) samples.push_back(elapsed);
        }
        std::sort(samples.begin(), samples.end());
        const double p50 = samples[samples.size() / 2];
        const double p95 = samples[static_cast<std::size_t>(samples.size() * 0.95) - 1];
        const double maximum = samples.back();
        qInfo("theme-switch-ms p50=%.3f p95=%.3f max=%.3f", p50, p95, maximum);
        if (qEnvironmentVariableIntValue("ZZ_PERFORMANCE_REFERENCE") == 1) {
            QVERIFY2(p95 <= 50.0,
                     "500-control theme switch P95 exceeded 50 ms");
        }
    }
};

QTEST_MAIN(ZzThemeSwitchBenchmark)

#include "ZzThemeSwitchBenchmark.moc"
```

- [ ] **Step 2: 注册 benchmark 并先观察当前结果**

Append to `ZzFluentUI/tests/CMakeLists.txt`:

```cmake
if(ZZ_BUILD_BENCHMARKS)
    add_executable(ZzThemeSwitchBenchmark ZzThemeSwitchBenchmark.cpp)
    target_link_libraries(ZzThemeSwitchBenchmark PRIVATE
        Qt6::Test
        Zz::FluentUI
    )
    set_target_properties(ZzThemeSwitchBenchmark PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(ZzThemeSwitchBenchmark)
    add_test(
        NAME benchmark.fluent-theme-switch
        COMMAND ZzThemeSwitchBenchmark
    )
    set_tests_properties(benchmark.fluent-theme-switch PROPERTIES
        LABELS "benchmark;fluent"
        TIMEOUT 30
    )
    if(DEFINED ZZ_PERFORMANCE_REFERENCE)
        if(ZZ_PERFORMANCE_REFERENCE)
            set(zz_performance_reference_environment 1)
        else()
            set(zz_performance_reference_environment 0)
        endif()
        set_property(TEST benchmark.fluent-theme-switch APPEND PROPERTY
            ENVIRONMENT
                "ZZ_PERFORMANCE_REFERENCE=${zz_performance_reference_environment}")
    endif()
endif()
```

Run:

```bash
cmake --preset linux-gcc-release -DZZ_BUILD_BENCHMARKS=ON
cmake --build --preset linux-gcc-release --target ZzThemeSwitchBenchmark
QT_QPA_PLATFORM=xcb ctest --preset linux-gcc-release -R benchmark.fluent-theme-switch -V
```

Expected: 在真实显示会话 PASS 且日志包含 10 轮预热、100 轮正式样本的 P50、P95、max；若当前会话不是 X11，使用实际 `QT_QPA_PLATFORM` 并记录协议。普通 CI 只记录数值，不执行绝对时间断言；指定 Linux 参考机设置 `ZZ_PERFORMANCE_REFERENCE=1` 后才要求 P95 不超过 50 ms。最终 JSON、环境元数据和相对回归由性能发布计划统一接管。

- [ ] **Step 3: 创建 Linux 原生 demo**

Create `examples/ZzFluentFoundationDemo/CMakeLists.txt` with:

```cmake
add_executable(ZzFluentFoundationDemo main.cpp)
target_link_libraries(ZzFluentFoundationDemo PRIVATE Zz::FluentUI Qt6::Widgets)
zz_enable_project_warnings(ZzFluentFoundationDemo)
```

Create `examples/ZzFluentFoundationDemo/main.cpp` with:

```cpp
#include <QtGui/QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    ZzFluentUI::ZzThemeController controller;
    application.setStyle(new ZzFluentUI::ZzFluentStyle(&controller));
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *platform = new QLabel(QGuiApplication::platformName(), &window);
    auto *mode = new QComboBox(&window);
    mode->addItems({QStringLiteral("System"), QStringLiteral("Light"),
                    QStringLiteral("Dark"), QStringLiteral("HighContrast")});
    layout->addWidget(platform);
    layout->addWidget(mode);
    QObject::connect(mode, &QComboBox::currentIndexChanged, &controller,
                     [&controller](int index) {
        controller.setMode(static_cast<ZzFluentUI::ZzThemeMode>(index));
    });
    window.resize(420, 180);
    window.show();
    const int exitCode = application.exec();
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    return exitCode;
}
```

这里直接使用 `QSvgRenderer`，不经 `QIcon` 的 SVG icon plugin；因此 offscreen 测试、安装消费者和三平台静态 Zz 构建不会依赖插件是否被自动发现。`Zz::FluentUI` 必须 private 链接 `Qt6::Svg`，Foundation 仍不得依赖 Svg/Widgets。资源读取、SVG 解析和 `QImage` 分配只发生在 cache miss，命中路径只复制隐式共享 `QPixmap`。

- [ ] **Step 4: 把 demo 接入 examples 聚合**

Append to `examples/CMakeLists.txt`:

```cmake
add_subdirectory(ZzFluentFoundationDemo)
```

Run:

```bash
cmake --preset linux-gcc-release -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-release --target ZzFluentFoundationDemo
./build/linux-gcc-release/examples/ZzFluentFoundationDemo/ZzFluentFoundationDemo
```

Expected: 窗口首屏显示实际 Qt platform 名；四种模式可切换，键盘 Tab 焦点可见，文本不截断，空闲时无持续刷新。

- [ ] **Step 5: 在可用的另一 Linux 协议复测**

Run one command matching the available session:

```bash
QT_QPA_PLATFORM=wayland ./build/linux-gcc-release/examples/ZzFluentFoundationDemo/ZzFluentFoundationDemo
QT_QPA_PLATFORM=xcb ./build/linux-gcc-release/examples/ZzFluentFoundationDemo/ZzFluentFoundationDemo
```

Expected: 可用协议启动成功；不可用协议明确报告平台插件/显示服务器不可用，不把该项记录为通过。至少一个真实 Linux 显示协议必须完成运行验证。

- [ ] **Step 6: 提交性能和运行验证**

```bash
git add ZzFluentUI/tests examples
git commit -m "测试：建立主题切换性能与 Linux 运行门禁" \
    -m "以五百个可见控件记录主题切换 P50、P95 和最大耗时。" \
    -m "增加原生显示 demo，验证四种模式、键盘焦点和实际 X11 或 Wayland 平台。"
```

## Task 8: 锁定架构、安装消费和跨平台编译矩阵

**Files:**
- Create: `tests/Architecture/CheckZzFluentBoundaries.cmake`
- Modify: `tests/Architecture/CMakeLists.txt`
- Modify: `tests/InstallConsumer/main.cpp`

- [ ] **Step 1: 创建 Fluent 边界扫描脚本**

Create `tests/Architecture/CheckZzFluentBoundaries.cmake` with:

```cmake
function(zz_read_source_without_comments source_path output_variable)
    file(READ "${source_path}" source_code)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" source_code "${source_code}")
    string(REGEX REPLACE "//[^\r\n]*" "" source_code "${source_code}")
    set(${output_variable} "${source_code}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE fluent_sources
    "${ZZ_SOURCE_DIR}/ZzFluentUI/*.h"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/*.cpp"
)

foreach(source_file IN LISTS fluent_sources)
    zz_read_source_without_comments("${source_file}" source_code)
    if(source_code MATCHES "namespace[ \t]+ZzFluentUI::")
        message(FATAL_ERROR "Chained namespace in ${source_file}")
    endif()
    if(source_code MATCHES "Qt[A-Za-z0-9]*/private|[_]p\\.h")
        message(FATAL_ERROR "Qt Private API in ${source_file}")
    endif()
    if(source_code MATCHES "ZzPureTools/|ZzWindowKit/|QWK")
        message(FATAL_ERROR "Forbidden UI dependency in ${source_file}")
    endif()
    if(source_code MATCHES "#[ \t]*include[ \t]*[<\"]([^>\"]*/)?(Repository|Database|NetworkClient|DomainEntity)")
        message(FATAL_ERROR "Business include in ${source_file}")
    endif()
endforeach()

file(GLOB_RECURSE foundation_files
    "${ZZ_SOURCE_DIR}/ZzFluentUI/foundation/*.h"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/foundation/*.cpp"
)
foreach(source_file IN LISTS foundation_files)
    zz_read_source_without_comments("${source_file}" source_code)
    if(source_code MATCHES "QtWidgets/|QWidget|QProxyStyle|QtQuick|QML")
        message(FATAL_ERROR "Foundation depends on a frontend in ${source_file}")
    endif()
endforeach()
```

- [ ] **Step 2: 注册 architecture test 并运行**

Append to `tests/Architecture/CMakeLists.txt`:

```cmake
add_test(
    NAME architecture.zzfluent-boundaries
    COMMAND ${CMAKE_COMMAND}
        -DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckZzFluentBoundaries.cmake
)
set_tests_properties(architecture.zzfluent-boundaries PROPERTIES
    LABELS "architecture;fluent"
)
```

Run:

```bash
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R architecture.zzfluent-boundaries
```

Expected: build 与架构测试返回 0；Foundation 不含 Widgets/Quick，Widgets 不含领域、存储、网络、WindowKit 或 PureTools include。

- [ ] **Step 3: 扩展安装消费者的公开 API 编译**

Add these includes to `tests/InstallConsumer/main.cpp`:

```cpp
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzThemeController.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
```

Change consumer `main()` to `main(int argc, char *argv[])`, add `#include <QtWidgets/QApplication>`, and construct `QApplication application(argc, argv);` before using the theme controller. Add this block before the existing final return:

```cpp
ZzFluentUI::ZzThemeController controller;
controller.setMode(ZzFluentUI::ZzThemeMode::Dark);
auto style = std::make_unique<ZzFluentUI::ZzFluentStyle>(&controller);
if (style->themeRevision() != controller.snapshot()->revision()) {
    return 2;
}
```


Add `#include <memory>` at the top. Preserve the existing six-version checks and return 0 after this block.

- [ ] **Step 4: 验证 Linux shared 安装消费**

Run:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release -R "(fluent|install.consumer)"
```

Expected: 全部返回 0；installed consumer 只通过 `find_package(ZzPureToolsFrame)` 获得两个 Fluent targets。

- [ ] **Step 5: 验证 Linux static 安装消费和 Sanitizer**

Run:

```bash
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release -R "(fluent|install.consumer)"
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan
QT_QPA_PLATFORM=offscreen ctest --preset linux-clang-asan -L fluent
```

Expected: static consumer PASS；ASan/UBSan 下全部 Fluent 测试 PASS，无泄漏、越界或 UAF。

- [ ] **Step 6: 扫描安装树和公开头**

Run:

```bash
rg -n "QtWidgets/|QWidget|QProxyStyle|QtQuick|QML" install/linux-gcc-release/include/ZzFluentUI/ZzThemeMode.h install/linux-gcc-release/include/ZzFluentUI/ZzThemeSnapshot.h install/linux-gcc-release/include/ZzFluentUI/ZzThemeController.h
rg -n "ZzPureTools|ZzWindowKit|QWK|Qt.*/private|repository|database|network|domain" install/linux-gcc-release/include/ZzFluentUI
rg -n "/home/|/Users/|[A-Za-z]:[/\\\\]" install/linux-gcc-release/lib/cmake/ZzPureToolsFrame
```

Expected: 三条扫描均无匹配；Foundation 公开头可由非 Widgets 消费者独立包含。

- [ ] **Step 7: 运行 Windows 和 macOS 原生编译门禁**

这是阶段 9 的延后 checkpoint：首次执行本 Fluent 计划时只完成 Linux Step 4-6，不得调用尚未定义的跨平台 preset。完成 `2026-08-02-performance-platform-release-gates.md` Task 2、在 `CMakePresets.json` 创建并解析下列 preset 后，再回到本步骤执行；该依赖必须记录在实施日志中，不能把“未执行”写成通过。

Run on each native CI host:

```bash
cmake --preset windows-msvc2022-release
cmake --build --preset windows-msvc2022-release
ctest --preset windows-msvc2022-release -R "(fluent|install.consumer)"
cmake --preset windows-msvc2022-static
cmake --build --preset windows-msvc2022-static
ctest --preset windows-msvc2022-static -R "(fluent|install.consumer)"
cmake --preset windows-mingw-release
cmake --build --preset windows-mingw-release
ctest --preset windows-mingw-release -R "(fluent|install.consumer)"
cmake --preset windows-mingw-static
cmake --build --preset windows-mingw-static
ctest --preset windows-mingw-static -R "(fluent|install.consumer)"
cmake --preset macos-clang-release-arm64
cmake --build --preset macos-clang-release-arm64
ctest --preset macos-clang-release-arm64 -R "(fluent|install.consumer)"
cmake --preset macos-clang-static-arm64
cmake --build --preset macos-clang-static-arm64
ctest --preset macos-clang-static-arm64 -R "(fluent|install.consumer)"
cmake --preset macos-clang-release-x86_64
cmake --build --preset macos-clang-release-x86_64
ctest --preset macos-clang-release-x86_64 -R "(fluent|install.consumer)"
cmake --preset macos-clang-static-x86_64
cmake --build --preset macos-clang-static-x86_64
ctest --preset macos-clang-static-x86_64 -R "(fluent|install.consumer)"
```

Expected: MSVC、Qt SDK MinGW、Apple Clang 的 shared/static 编译、公共头和 install consumer 全部通过；这些结果只证明静态可编译，不宣称 Windows 高 DPI 或 macOS Retina 真机交互已经验证。

- [ ] **Step 8: 提交架构和安装门禁**

```bash
git add tests/Architecture tests/InstallConsumer
git commit -m "测试：锁定 Fluent 主题层依赖与安装边界" \
    -m "自动拒绝 Widgets 进入 Foundation、Quick 实现、Qt Private 和业务依赖。" \
    -m "验证 Linux shared/static、Sanitizer、独立安装消费及三平台原生编译矩阵。"
```

## Task 9: 最终一致性和质量核验

**Files:**
- Verify: `ZzFluentUI/foundation/include/ZzFluentUI/*.h`
- Verify: `ZzFluentUI/foundation/src/*.cpp`
- Verify: `ZzFluentUI/foundation/src/private/*`
- Verify: `ZzFluentUI/widgets/include/ZzFluentUI/*.h`
- Verify: `ZzFluentUI/widgets/src/*.cpp`
- Verify: `ZzFluentUI/widgets/src/private/*`
- Verify: `ZzFluentUI/tests/*`
- Verify: `examples/ZzFluentFoundationDemo/*`
- Verify: `tests/Architecture/CheckZzFluentBoundaries.cmake`
- Verify: `tests/InstallConsumer/main.cpp`

本任务只执行核验，不创建或修改文件。任一检查失败时，必须返回首次引入该行为的 Task 1-8，在所属任务内补充红灯、最小修复、绿灯和提交，然后从本任务 Step 1 重新执行。

- [ ] **Step 1: 运行完整 Linux Debug 测试**

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -L fluent --output-on-failure
ctest --preset linux-gcc-debug -R architecture.zzfluent-boundaries --output-on-failure
```

Expected: 所有 Fluent unit/component/architecture 测试 PASS。

- [ ] **Step 2: 运行 Release 性能与真实显示复核**

Run:

```bash
cmake --preset linux-gcc-release \
  -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_BUILD_BENCHMARKS=ON \
  -DZZ_PERFORMANCE_REFERENCE=ON
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release -R benchmark.fluent-theme-switch -V
./build/linux-gcc-release/examples/ZzFluentFoundationDemo/ZzFluentFoundationDemo
```

Expected: 500 控件 P95 不超过 50 ms；真实窗口四种模式、Tab 焦点、字体和 RTL 基础布局无重叠，运行记录写明 Qt、编译器、CPU、DPR 和 platform name。

- [ ] **Step 3: 检查命名、namespace 和公开文档**

Run:

```bash
rg -n "namespace[[:space:]]+ZzFluentUI::|class (?!Zz|Q)|struct (?!Zz|Q)|enum class (?!Zz|Q)" ZzFluentUI --pcre2
rg -L "/\\*\\*" ZzFluentUI/foundation/include/ZzFluentUI/*.h ZzFluentUI/widgets/include/ZzFluentUI/*.h
git diff --check
```

Expected: 前两条无违规输出；`git diff --check` 返回 0。

- [ ] **Step 4: 检查改动范围和构建产物**

Run:

```bash
git status --short
git diff --stat
git ls-files | rg "(^|/)(build|install|cache)/|CMakeUserPresets.json$"
```

Expected: 只包含本计划列出的源码、测试、demo、CMake 和架构脚本；最后一条无输出。

- [ ] **Step 5: 确认核验任务没有兜底修改**

Run:

```bash
git status --short
```

Expected: 与进入 Task 9 前的状态完全一致；本任务不创建“最终修复”兜底提交。若 Step 1-4 曾失败，所属修复必须已经在 Task 1-8 的对应提交中完成并通过重跑。

## 完成标准

- `Zz::FluentFoundation` 只链接 ZzCore、Qt Core/Gui；公开头不包含 Widgets、Quick、领域、存储或网络类型。
- `Zz::FluentUI` 保留平台 `QProxyStyle` 行为，只覆盖基础 palette、metric 和 focus primitive；不依赖 WindowKit 或 PureTools。
- Light、Dark、System、HighContrast 均由完整不可变快照表达，Controller 只在 GUI 线程一次交换并发出带分类的 revision。
- token 通过定长数组 O(1) 读取，越界有断言和确定性回退；paint 热路径没有字符串查找、锁、文件读取或 SVG 解析。
- 有状态导出 QObject 按四文件 PIMPL；所有自定义类型使用 `Zz` 前缀，不使用链式 C++ namespace。
- 所有公开类、枚举、方法和信号具备简体中文 Doxygen，说明线程、所有权、参数和状态前提。
- 颜色变化只触发 palette/重绘；几何变化才触发 StyleChange；减少动效关闭非必要动画并保留最多 50 ms 的必要反馈。
- DPR 量化覆盖 0.5 到 8.0；焦点环在高对比度下可见；布局方向和平台快捷键继续由 Qt 基础样式处理。
- 图标缓存键包含资源、逻辑尺寸、DPR、RGBA 和主题 revision；缓存字节上限为 4 MiB，主题切换清空旧 revision 内容。
- 500 个可见控件主题切换记录固定 10/100 轮的 Release P50/P95/最大值与实际 Linux platform；只在指定参考机强制 P95 不超过 50 ms，普通 CI 使用后续性能计划的相对回归门禁。
- Linux shared/static、ASan/UBSan、公共头、架构测试和独立安装消费通过；性能/平台计划创建 presets 后，Windows MSVC、Qt SDK MinGW、macOS Apple Clang 的 shared/static 原生编译门禁作为延后 checkpoint 通过或明确记录未执行。
- 仓库中没有 `ZzFluentQuick` 实现 target、Qt Quick/QML 源码或让 Quick 依赖 Widgets 的路径；未来前端仅能消费 `Zz::FluentFoundation`。
