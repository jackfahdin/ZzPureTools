# ZzWindowKit 与 qwindowkit 适配 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立不暴露 QWK API、具有明确状态机和平台结果语义的 `Zz::WindowKit` 无边框窗口组件。

**Architecture:** `ZzWindowAgent` 使用四文件 PIMPL，private 层组合可替换的 `ZzWindowBackend`；测试注入假后端，生产实现 `ZzQWindowKitBackend` 私有组合 `QWK::WidgetWindowAgent`。所有 QWK 字符串属性、Qt Private 依赖和平台差异止于该后端。

**Tech Stack:** Qt 6.8 Core/Gui/Widgets/Test、C++20、QWindowKit 1.5.1.0、CMake、Qt Test、X11/Wayland、ASan/UBSan。

---

## 前置条件

- 完成仓库基线、ZzLog 和 ZzCore 计划。
- `ZzResult<T>`、`ZzError` 和 `ZzWindowKit` 最小 target 已可用。
- 阅读架构设计第 9 节及 qwindowkit 的 `README.zh-CN.md`。
- 不直接修改 qwindowkit 行为；确需补丁时先新增 ADR 和 `patches/` 记录。
- 本计划只使用当前源码实际提供的 `QWindowKit::Core`、`QWindowKit::Widgets` alias；实现文件 include `<QWKWidgets/widgetwindowagent.h>`，不得猜测其他 target 或头路径。
- `ZzWindowAgent` 的全部公开方法只能在其所属 GUI 线程调用。`capabilities()` 是保守能力快照，具体效果调用返回的 `Applied/Deferred/Unsupported` 或 `ZzError` 才是最终结果。

## 文件边界

### 公开 API

- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowAgentState.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowCapability.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowBackdrop.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowColorScheme.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowApplyState.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowChromeConfiguration.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowKitBootstrap.h`
- Create: `ZzWindowKit/src/ZzWindowKitBootstrap.cpp`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowAgent.h`
- Create: `ZzWindowKit/src/ZzWindowAgent.cpp`
- Create: `ZzWindowKit/src/private/ZzWindowAgentPrivate.h`
- Create: `ZzWindowKit/src/private/ZzWindowAgentPrivate.cpp`

### 私有后端

- Create: `ZzWindowKit/src/private/ZzWindowBackend.h`
- Create: `ZzWindowKit/src/private/ZzQWindowKitBackend.h`
- Create: `ZzWindowKit/src/private/ZzQWindowKitBackend.cpp`
- Create: `ZzWindowKit/src/private/ZzWindowAgentTestAccess.h`

### 测试与示例

- Create: `ZzWindowKit/tests/CMakeLists.txt`
- Create: `ZzWindowKit/tests/ZzWindowAgentTest.cpp`
- Create: `ZzWindowKit/tests/ZzWindowKitBootstrapTest.cpp`
- Create: `ZzWindowKit/tests/ZzWindowKitLifecycleTest.cpp`
- Create: `ZzWindowKit/tests/private/ZzFakeWindowBackend.h`
- Create: `ZzWindowKit/tests/private/ZzFakeWindowBackend.cpp`
- Create: `examples/ZzWindowKitDemo/CMakeLists.txt`
- Create: `examples/ZzWindowKitDemo/main.cpp`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindow.h`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindow.cpp`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindowPrivate.h`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindowPrivate.cpp`
- Modify: `examples/CMakeLists.txt`

### 构建、安装和治理

- Modify: `ZzWindowKit/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `cmake/ZzInstallPackage.cmake`
- Modify: `cmake/ZzPureToolsProConfig.cmake.in`
- Create: `cmake/ZzWindowKitPrivateTargets.cmake.in`
- Create: `tests/Architecture/CheckZzWindowKitBoundaries.cmake`
- Create: `docs/third-party/qwindowkit-vendor.json`
- Create: `docs/third-party/THIRD_PARTY_NOTICES.md`

## Task 1: 定义强类型窗口 API

**Files:**
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowAgentState.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowCapability.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowBackdrop.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowColorScheme.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowApplyState.h`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowChromeConfiguration.h`
- Create: `ZzWindowKit/src/private/ZzWindowBackend.h`
- Create: `ZzWindowKit/tests/ZzWindowAgentTest.cpp`
- Create: `ZzWindowKit/tests/private/ZzFakeWindowBackend.h`
- Create: `ZzWindowKit/tests/private/ZzFakeWindowBackend.cpp`
- Create: `ZzWindowKit/tests/CMakeLists.txt`
- Modify: `ZzWindowKit/CMakeLists.txt`

- [ ] **Step 1: 创建状态和能力类型**

`ZzWindowAgentState`：

```cpp
enum class ZzWindowAgentState : std::uint8_t
{
    Detached,
    Attached,
    Configured,
    Invalidated,
    Failed
};
```

`ZzWindowCapability`：

```cpp
enum class ZzWindowCapability : std::uint32_t
{
    None = 0,
    SystemMenu = 1U << 0U,
    Blur = 1U << 1U,
    Acrylic = 1U << 2U,
    Mica = 1U << 3U,
    MicaAlt = 1U << 4U,
    NativeSystemButtons = 1U << 5U,
    SnapLayout = 1U << 6U
};
Q_DECLARE_FLAGS(ZzWindowCapabilities, ZzWindowCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(ZzWindowCapabilities)
```

每个 enum 放在同名 `.h`，使用传统 `namespace ZzWindowKit`，添加中文 Doxygen 和必要标准 include。

- [ ] **Step 2: 创建效果类型**

```cpp
enum class ZzWindowBackdrop : std::uint8_t
{
    None,
    Blur,
    Acrylic,
    Mica,
    MicaAlt,
    Automatic
};

enum class ZzWindowColorScheme : std::uint8_t
{
    System,
    Light,
    Dark
};

enum class ZzWindowApplyState : std::uint8_t
{
    Applied,
    Deferred,
    Unsupported
};
```

后端失败使用外层 `ZzResult`，禁止向 `ZzWindowApplyState` 添加 `Failed`。

- [ ] **Step 3: 创建完整标题栏配置**

```cpp
namespace ZzWindowKit {

/**
 * @brief 描述一次完整的无边框标题栏绑定。
 *
 * 所有指针均为非拥有输入；调用完成后由代理使用 QPointer 跟踪。
 */
struct ZzWindowChromeConfiguration final
{
    QWidget *titleBar = nullptr;
    QWidget *windowIcon = nullptr;
    QWidget *minimizeButton = nullptr;
    QWidget *maximizeButton = nullptr;
    QWidget *closeButton = nullptr;
    QList<QWidget *> interactiveWidgets;
};

} // namespace ZzWindowKit
```

- [ ] **Step 4: 定义私有后端接口和假后端**

`ZzWindowBackend` 只位于 `src/private`：

```cpp
class ZzWindowBackend
{
public:
    virtual ~ZzWindowBackend() = default;

    [[nodiscard]] virtual ZzCore::ZzResult<void> attach(QWidget *window) = 0;
    [[nodiscard]] virtual ZzCore::ZzResult<void> configureChrome(
        const ZzWindowChromeConfiguration &configuration) = 0;
    [[nodiscard]] virtual ZzWindowCapabilities capabilities() const noexcept = 0;
    [[nodiscard]] virtual ZzCore::ZzResult<ZzWindowApplyState> setBackdrop(
        ZzWindowBackdrop backdrop) = 0;
    [[nodiscard]] virtual ZzCore::ZzResult<ZzWindowApplyState> setColorScheme(
        ZzWindowColorScheme colorScheme) = 0;
    [[nodiscard]] virtual ZzCore::ZzResult<void> showSystemMenu(
        const QPoint &globalPosition) = 0;
};
```

`ZzFakeWindowBackend` 保存 attach/configure 调用次数、最后配置和可预设返回值，不包含 QWK 头。假后端还保存 `QStringList calls`；每次调用追加 `attach`、`title-bar`、`icon`、`minimize`、`maximize`、`close`、`interactive:<objectName>`。测试必须精确比较调用序列，以锁定 QWK 更换 title bar 会清空既有登记这一上游行为。

- [ ] **Step 5: 写 facade 失败测试**

测试覆盖：

```cpp
void attachesOnlyOnce();
void rejectsNonWindowHost();
void rejectsChromeFromAnotherWindow();
void configuresBackendInAttachedState();
void invalidatesWhenHostIsDestroyed();
void propagatesBackendFailure();
```

核心断言示例：

```cpp
QWidget host;
auto backend = std::make_unique<ZzFakeWindowBackend>();
auto *backendPointer = backend.get();
auto agent = ZzWindowAgentTestAccess::create(std::move(backend));

QVERIFY(agent->attach(&host));
QCOMPARE(agent->state(), ZzWindowAgentState::Attached);
QCOMPARE(backendPointer->attachCalls(), 1);
QVERIFY(!agent->attach(&host));
QCOMPARE(backendPointer->attachCalls(), 1);
```

- [ ] **Step 6: 注册并运行红灯测试**

Create `ZzWindowKit/tests/CMakeLists.txt` with:

```cmake
add_executable(ZzWindowAgentTest
    ZzWindowAgentTest.cpp
    private/ZzFakeWindowBackend.cpp
)
target_include_directories(ZzWindowAgentTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/private
)
target_link_libraries(ZzWindowAgentTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::WindowKit
)
set_target_properties(ZzWindowAgentTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWindowAgentTest)
zz_enable_sanitizers(ZzWindowAgentTest)
add_test(NAME windowkit.agent COMMAND ZzWindowAgentTest)
set_tests_properties(windowkit.agent PROPERTIES
    LABELS "unit;windowkit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Append to `ZzWindowKit/CMakeLists.txt`:

```cmake
if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzWindowAgentTest
```

Expected: configure PASS；compile FAIL，诊断明确指出缺少 `ZzWindowAgent.h` 或 `ZzWindowAgentTestAccess.h`，而不是 `unknown target`。

- [ ] **Step 7: 保留红灯到实现任务**

继续 Task 2；本步骤不提交无法编译的中间状态。Task 2 的提交必须同时包含本任务创建的公开类型、假后端和测试注册。

## Task 2: 实现 ZzWindowAgent 状态机

**Files:**
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowAgent.h`
- Create: `ZzWindowKit/src/ZzWindowAgent.cpp`
- Create: `ZzWindowKit/src/private/ZzWindowAgentPrivate.h`
- Create: `ZzWindowKit/src/private/ZzWindowAgentPrivate.cpp`
- Create: `ZzWindowKit/src/private/ZzWindowAgentTestAccess.h`
- Modify: `ZzWindowKit/CMakeLists.txt`
- Modify: `ZzWindowKit/tests/CMakeLists.txt`

- [ ] **Step 1: 声明 public facade**

公开方法严格使用架构文档签名：

```cpp
class ZZ_WINDOWKIT_EXPORT ZzWindowAgent final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzWindowAgent)

public:
    ~ZzWindowAgent() override;

    [[nodiscard]] ZzCore::ZzResult<void> attach(QWidget *window);
    [[nodiscard]] ZzCore::ZzResult<void> configureChrome(
        const ZzWindowChromeConfiguration &configuration);
    [[nodiscard]] ZzWindowAgentState state() const noexcept;
    [[nodiscard]] ZzWindowCapabilities capabilities() const noexcept;
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setBackdrop(
        ZzWindowBackdrop backdrop);
    [[nodiscard]] ZzCore::ZzResult<ZzWindowApplyState> setColorScheme(
        ZzWindowColorScheme colorScheme);
    [[nodiscard]] ZzCore::ZzResult<void> showSystemMenu(
        const QPoint &globalPosition);

private:
    friend class ZzWindowAgentTestAccess;
    explicit ZzWindowAgent(
        std::unique_ptr<ZzWindowBackend> backend,
        QObject *parent);

    std::unique_ptr<ZzWindowAgentPrivate> d_ptr;
};
```

`ZzWindowAgentTestAccess.h` 使用堆所有权，不按值返回禁止移动的 QObject：

```cpp
#pragma once

#include <memory>
#include <utility>

#include <ZzWindowKit/ZzWindowAgent.h>

#include "ZzWindowBackend.h"

namespace ZzWindowKit {

class ZzWindowAgentTestAccess final
{
public:
    [[nodiscard]] static std::unique_ptr<ZzWindowAgent> create(
        std::unique_ptr<ZzWindowBackend> backend)
    {
        return std::unique_ptr<ZzWindowAgent>(
            new ZzWindowAgent(std::move(backend), nullptr));
    }
};

} // namespace ZzWindowKit
```

Public header 只前置声明 private backend，不包含其定义或 QWK。

本任务先完成可注入后端的 facade 状态机，因此暂不声明生产默认构造函数；只有 private `ZzWindowAgentTestAccess` 能创建实例。Task 4 在真实 QWK backend 与 target 同时可链接后，再原子地增加 public `explicit ZzWindowAgent(QObject *parent = nullptr)` 及其实现，避免中间提交暴露一个没有定义、消费者一调用就链接失败的公开符号。

- [ ] **Step 2: 实现统一前置校验**

Private 保存：

```cpp
ZzWindowAgent *const q_ptr;
std::unique_ptr<ZzWindowBackend> backend;
QPointer<QWidget> host;
ZzWindowAgentState state = ZzWindowAgentState::Detached;
QMetaObject::Connection hostDestroyedConnection;
```

`attach()` 校验顺序：

1. state 必须为 Detached。
2. window 非空。
3. `QThread::currentThread() == q_ptr->thread()`。
4. `window->thread() == q_ptr->thread()`。
5. `window->isWindow()`。
6. 调用 backend attach。

成功后保存 QPointer、连接 destroyed、进入 Attached；backend 失败进入 Failed。

本任务只实现测试专用构造函数，且它只在 private TestAccess 可见。Facade 与 backend 均不设置 QObject parent/智能指针双重所有权；不得用临时 null/unsupported backend 冒充生产默认行为。

- [ ] **Step 3: 实现完整 chrome 校验**

使用私有函数：

```cpp
[[nodiscard]] ZzCore::ZzResult<void> validateChrome(
    const ZzWindowChromeConfiguration &configuration) const;
```

逐项验证 titleBar 非空且为 host 后代；四个系统按钮允许为空，但非空时必须为 titleBar 后代；interactive widget 必须全部非空、为 titleBar 后代，且列表内部及与四个系统按钮之间均无重复地址。所有对象线程必须与 agent/host 一致。用 `QSet<QWidget *>` 只在配置路径分配，paint/event 热路径不调用。

backend configure 成功进入 Configured；失败进入 Failed。更换 titleBar 必须传入完整新配置。

- [ ] **Step 4: 实现 host 销毁和后续调用语义**

host destroyed lambda 只把 `host` 清空并设置 `Invalidated`，不解引用已销毁对象。所有 effect/menu/config 方法在 Invalidated/Failed/Detached 返回 `InvalidState`。

完成实现后，编辑 `ZzWindowKit/CMakeLists.txt` 文件顶部、`add_library(ZzWindowKit ...)` 之前的源文件清单，替换为当前完整内容。不得在 `zz_configure_library_target(... SOURCES ${zz_window_kit_sources})` 调用之后追加生产翻译单元：

```cmake
set(zz_window_kit_sources
    src/private/ZzWindowKitVersion.cpp
    src/ZzWindowAgent.cpp
    src/private/ZzWindowAgentPrivate.cpp
)
set(zz_window_kit_moc_headers
    include/ZzWindowKit/ZzWindowAgent.h
)
```

- [ ] **Step 5: 构建并运行 facade 测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzWindowAgentTest
ctest --preset linux-gcc-debug -R windowkit.agent --repeat until-fail:20
```

Expected: 连续 20 轮 PASS；fake backend 无 QWK 依赖。

- [ ] **Step 6: 提交 facade 与状态机**

```bash
git add ZzWindowKit
git commit -m "窗口：实现无边框代理状态机" \
    -m "增加强类型能力、效果和完整标题栏配置接口。" \
    -m "通过可注入假后端验证单次绑定、线程、控件归属、失效和后端失败语义。"
```

## Task 3: 实现应用创建前 bootstrap

**Files:**
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowKitBootstrap.h`
- Create: `ZzWindowKit/src/ZzWindowKitBootstrap.cpp`
- Create: `ZzWindowKit/tests/ZzWindowKitBootstrapTest.cpp`
- Modify: `ZzWindowKit/CMakeLists.txt`
- Modify: `ZzWindowKit/tests/CMakeLists.txt`

- [ ] **Step 1: 编写独立 main 测试**

该测试不能使用 `QTEST_MAIN`，因为必须在 QApplication 前调用：

```cpp
#include <QtCore/QCoreApplication>
#include <QtCore/Qt>

#include <ZzWindowKit/ZzWindowKitBootstrap.h>

int main(int argc, char *argv[])
{
    const auto first = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!first || !QCoreApplication::testAttribute(
            Qt::AA_DontCreateNativeWidgetSiblings)) {
        return 1;
    }

    const auto second = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!second) {
        return 2;
    }

    QCoreApplication application(argc, argv);
    const auto late = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    return late ? 3 : 0;
}
```

- [ ] **Step 2: 运行并确认缺少类型**

Append to `ZzWindowKit/tests/CMakeLists.txt` before running the red test:

```cmake
add_executable(ZzWindowKitBootstrapTest
    ZzWindowKitBootstrapTest.cpp
)
target_link_libraries(ZzWindowKitBootstrapTest PRIVATE
    Qt6::Core
    Zz::WindowKit
)
zz_enable_project_warnings(ZzWindowKitBootstrapTest)
zz_enable_sanitizers(ZzWindowKitBootstrapTest)
add_test(NAME windowkit.bootstrap COMMAND ZzWindowKitBootstrapTest)
set_tests_properties(windowkit.bootstrap PROPERTIES
    LABELS "unit;windowkit"
)
```

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzWindowKitBootstrapTest
```

Expected: configure PASS；compile FAIL，诊断明确指出 `ZzWindowKit/ZzWindowKitBootstrap.h` 不存在，而不是 `unknown target`。

- [ ] **Step 3: 实现幂等 prepare**

`ZzWindowKitBootstrap` 是无状态、不可实例化类：

```cpp
class ZZ_WINDOWKIT_EXPORT ZzWindowKitBootstrap final
{
public:
    ZzWindowKitBootstrap() = delete;

    [[nodiscard]] static ZzCore::ZzResult<void> prepare();
};
```

`prepare()` 在 `QCoreApplication::instance() != nullptr` 时返回 `InvalidState`；否则调用 `QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings)` 并返回 success。重复提前调用保持成功。

再次编辑 `add_library(ZzWindowKit ...)` 之前的 `zz_window_kit_sources`，替换为当前累计清单：

```cmake
set(zz_window_kit_sources
    src/private/ZzWindowKitVersion.cpp
    src/ZzWindowAgent.cpp
    src/private/ZzWindowAgentPrivate.cpp
    src/ZzWindowKitBootstrap.cpp
)
set(zz_window_kit_moc_headers
    include/ZzWindowKit/ZzWindowAgent.h
)
```

- [ ] **Step 4: 运行测试并提交**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzWindowKitBootstrapTest
ctest --preset linux-gcc-debug -R windowkit.bootstrap
```

Expected: PASS。

```bash
git add ZzWindowKit
git commit -m "窗口：增加应用创建前初始化入口" \
    -m "在 QApplication 创建前设置无边框窗口所需 Qt 属性。" \
    -m "重复提前调用保持幂等，过晚调用返回明确状态错误。"
```

## Task 4: 私有集成 QWindowKit backend

**Files:**
- Create: `ZzWindowKit/src/private/ZzQWindowKitBackend.h`
- Create: `ZzWindowKit/src/private/ZzQWindowKitBackend.cpp`
- Modify: `ZzWindowKit/include/ZzWindowKit/ZzWindowAgent.h`
- Modify: `ZzWindowKit/src/ZzWindowAgent.cpp`
- Modify: `ZzWindowKit/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 固定 qwindowkit 子项目选项**

先在根 `CMakeLists.txt` 的项目选项区增加：

```cmake
option(
    ZZ_WINDOWKIT_FORCE_QT_CONTEXT
    "Force QWindowKit to use the portable Qt window context"
    OFF
)
```

再在 `ZzWindowKit/CMakeLists.txt` 的 `add_library` 前设置普通变量：

```cmake
set(QWINDOWKIT_BUILD_STATIC ON)
set(QWINDOWKIT_BUILD_WIDGETS ON)
set(QWINDOWKIT_BUILD_QUICK OFF)
set(QWINDOWKIT_BUILD_EXAMPLES OFF)
set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF)
set(QWINDOWKIT_INSTALL OFF)
set(QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT
    ${ZZ_WINDOWKIT_FORCE_QT_CONTEXT}
)
set(QWINDOWKIT_ENABLE_STYLE_AGENT OFF)

add_subdirectory(
    "${PROJECT_SOURCE_DIR}/ZzThirdParty/qwindowkit"
    "${PROJECT_BINARY_DIR}/third-party/qwindowkit"
    EXCLUDE_FROM_ALL
)
```

Root 已先 `find_package(Qt6 6.8 ...)`。如果 qwindowkit option 因 cache 旧值不生效，配置必须明确失败并提示删除该 preset build directory；禁止无提示 `FORCE` 覆盖用户 cache。

`add_subdirectory()` 后立即验证 `TARGET QWindowKit::Core`、`TARGET QWindowKit::Widgets`、`QWINDOWKIT_BUILD_STATIC`、`NOT QWINDOWKIT_INSTALL` 和 `NOT QWINDOWKIT_BUILD_QUICK`，任一不满足即 `FATAL_ERROR`。官方 Qt SDK/发行版还必须提供 qwindowkit 所需的 `Qt6::CorePrivate`、`Qt6::GuiPrivate` 和 `Qt6::WidgetsPrivate`；缺少时配置错误需明确提示安装与当前 Qt 6.8 minor 完全匹配的 private development package，不允许回退到另一 Qt minor。

- [ ] **Step 2: 私有链接 QWK targets**

```cmake
target_link_libraries(ZzWindowKit
    PRIVATE
        QWindowKit::Core
        QWindowKit::Widgets
)
```

`ZzWindowKit` public include directory不得加入 qwindowkit 路径。同时给 adapter 自身增加只在 private 源码可见的 `ZZ_WINDOWKIT_FORCE_QT_CONTEXT=$<BOOL:${ZZ_WINDOWKIT_FORCE_QT_CONTEXT}>` 编译定义，供 capability 判定与测试使用；该宏不得出现在安装头或导出 target 的 interface definitions。

- [ ] **Step 3: 实现 QWK attach 和 chrome 映射**

先在 `ZzWindowAgent.h` 的 public 区增加生产构造函数，并在 `ZzWindowAgent.cpp` 中与真实后端定义一起实现；头文件仍只前置声明 private 类型：

```cpp
explicit ZzWindowAgent(QObject *parent = nullptr);
```

构造函数使用 `std::make_unique<ZzQWindowKitBackend>()` 委托给 Task 2 的 private 注入构造函数。该 public 声明、构造函数定义、backend 源码和 QWK private link 必须属于同一步、同一提交，任何中间状态都不能进入提交历史。

`ZzQWindowKitBackend` private 成员：

```cpp
std::unique_ptr<QWK::WidgetWindowAgent> agent_;
QPointer<QWidget> host_;
```

attach 创建每窗一个 agent，QObject parent 必须为 `nullptr`，由唯一 `unique_ptr` 独占；禁止同时把 agent parent 设为 host，否则 host 先销毁时会与 `unique_ptr` 形成双重删除。随后调用 `setup(host)`；false 映射 `Backend`。backend 析构先销毁 agent，再清空 host 观察指针。chrome 按以下顺序完整应用：

```cpp
agent_->setTitleBar(configuration.titleBar);
agent_->setSystemButton(QWK::WindowAgentBase::WindowIcon, configuration.windowIcon);
agent_->setSystemButton(QWK::WindowAgentBase::Minimize, configuration.minimizeButton);
agent_->setSystemButton(QWK::WindowAgentBase::Maximize, configuration.maximizeButton);
agent_->setSystemButton(QWK::WindowAgentBase::Close, configuration.closeButton);
for (QWidget *widget : configuration.interactiveWidgets) {
    agent_->setHitTestVisible(widget, true);
}
```

Facade 已完成全部指针校验，backend 不重复业务校验，但仍检查 agent 存在。`configureChrome()` 每次都先调用 `setTitleBar()`，再依次登记四个系统按钮，最后按调用者列表顺序登记 interactive widgets；包括空按钮在内也调用对应 `setSystemButton(..., nullptr)`。不得尝试增量更新，因为上游 `setTitleBar()` 会清空旧按钮和 hit-test 登记。

- [ ] **Step 4: 实现平台能力与字符串属性映射**

所有字符串只写在 `.cpp`：

| Zz value | QWK key/value |
|---|---|
| `Mica` | `"mica" = true` |
| `MicaAlt` | `"mica-alt" = true` |
| `Acrylic` | `"acrylic-material" = true` |
| Windows `Blur` | `"dwm-blur" = true` |
| macOS `Blur` | `"blur-effect" = "dark"` 或 `"light"` |
| `Light/Dark` | `"dark-mode" = false/true` |

切换 backdrop 前先关闭其他互斥材质。`None` 在三平台都受支持，并关闭当前已知材质；Linux 对 `Blur/Acrylic/Mica/MicaAlt/Automatic` 返回 `Unsupported`，且不得调用未知字符串属性。判断 native handle 只能读取 `host_->testAttribute(Qt::WA_WState_Created)` 与 `host_->windowHandle()`，不得调用会为了查询而强制创建窗口的 `winId()`。尚无 handle 时仍把受支持属性提交给 QWK 以便其在 `WinIdChange` 后重放，但返回 `Deferred`；已有 handle 且 QWK 返回 false 时，根据平台/系统版本不支持映射 `Unsupported`，只有本应支持却执行失败时映射 `Backend`。

`Automatic`：Windows 11 优先 Mica，macOS 优先 Blur，Linux 返回 Unsupported。不得把 Linux fallback 描述为已应用材质。

`setColorScheme()` 规则固定为：Windows 将 `System` 通过 `QGuiApplication::styleHints()->colorScheme()` 解析后写 `dark-mode`；macOS 仅当当前 backdrop 为 Blur 时把 `blur-effect` 更新为 `dark/light`，否则返回 `Unsupported`；Linux 返回 `Unsupported`。backend 保存最近一次成功或 deferred 的 backdrop/color scheme，native handle 重建后由 QWK 自身重放属性，Zz 层不安装第二套 native event filter。

- [ ] **Step 5: 实现保守 capability 快照**

capability 在 attach 成功后计算并保持稳定：强制 Qt fallback 时返回空集合；Windows 10+ native context 至少包含 `SystemMenu|Blur`，Windows 11+ 再包含 `Acrylic|Mica|MicaAlt|SnapLayout`；macOS native context 包含 `Blur|NativeSystemButtons`；Linux 仅当 `QGuiApplication::platformName()` 为 `xcb` 或以 `wayland` 开头时包含 `SystemMenu`。系统版本判断使用 Qt public `QOperatingSystemVersion`，禁止包含 Win32/Cocoa/X11 私有头。实际 setter 仍可因驱动、compositor 或系统策略返回 `Unsupported`。

- [ ] **Step 6: 实现 system menu 平台语义**

- macOS：返回 `Unsupported`。
- Windows/Linux：调用 `agent_->showSystemMenu(globalPosition)` 并返回 success，Doxygen 说明 Linux WM/Wayland compositor 可能忽略请求。
- 未 attach 或 host 已失效：`InvalidState`。

- [ ] **Step 7: 构建真实 backend**

在构建前把 `add_library(ZzWindowKit ...)` 之前的 `zz_window_kit_sources` 替换为最终累计清单，确保 QWK adapter 也进入逐源警告和 clang-tidy 门禁：

```cmake
set(zz_window_kit_sources
    src/private/ZzWindowKitVersion.cpp
    src/ZzWindowAgent.cpp
    src/private/ZzWindowAgentPrivate.cpp
    src/ZzWindowKitBootstrap.cpp
    src/private/ZzQWindowKitBackend.cpp
)
set(zz_window_kit_moc_headers
    include/ZzWindowKit/ZzWindowAgent.h
)
```

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzWindowKit
```

Expected: PASS；编译命令中的 QWK/Qt Private include 只出现在 qwindowkit 与 `ZzQWindowKitBackend.cpp`，公共头 compile command 不出现。

- [ ] **Step 8: 提交 QWK backend**

```bash
git add ZzWindowKit CMakeLists.txt
git commit -m "窗口：接入私有 QWindowKit 后端" \
    -m "按固定顺序映射 setup、标题栏、系统按钮和交互区域。" \
    -m "封装平台材质与系统菜单差异，QWK 类型和字符串属性不进入公共接口。"
```

## Task 5: Linux 原生窗口生命周期验证

**Files:**
- Create: `examples/ZzWindowKitDemo/CMakeLists.txt`
- Create: `examples/ZzWindowKitDemo/main.cpp`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindow.h`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindow.cpp`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindowPrivate.h`
- Create: `examples/ZzWindowKitDemo/ZzWindowKitDemoWindowPrivate.cpp`
- Create: `ZzWindowKit/tests/ZzWindowKitLifecycleTest.cpp`
- Modify: `examples/CMakeLists.txt`
- Modify: `ZzWindowKit/tests/CMakeLists.txt`

- [ ] **Step 1: 创建最小真实窗口示例**

`ZzWindowKitDemoWindow : QMainWindow` 使用四文件 PIMPL。Private 创建普通 `QWidget` title bar 和三个 `QPushButton`，完整调用 `configureChrome()`；按钮连接：

```cpp
connect(minimizeButton, &QPushButton::clicked, q_ptr, &QWidget::showMinimized);
connect(maximizeButton, &QPushButton::clicked, q_ptr, [this] {
    q_ptr->isMaximized() ? q_ptr->showNormal() : q_ptr->showMaximized();
});
connect(closeButton, &QPushButton::clicked, q_ptr, &QWidget::close);
```

示例只验证窗口能力，不引入 Fluent 视觉。

- [ ] **Step 2: 创建 100 次生命周期自动测试**

测试循环：

```cpp
for (int index = 0; index < 100; ++index) {
    auto window = std::make_unique<QWidget>();
    auto agent = std::make_unique<ZzWindowKit::ZzWindowAgent>();
    QVERIFY(agent->attach(window.get()));
    window->show();
    QCoreApplication::processEvents();
    window->close();
    QCoreApplication::processEvents();
    agent.reset();
    window.reset();
}
```

另增加相反销毁顺序：attach 后先 `window.reset()`，确认 facade 进入 `Invalidated`，再销毁 agent；两种顺序各 100 次。该测试标记 `platform;windowkit`，在 `offscreen` 下只检查 QObject 生命周期；原生事件过滤器验证必须在真实 X11/Wayland 环境运行。

- [ ] **Step 3: 在当前 Linux 显示协议运行**

Create `examples/ZzWindowKitDemo/CMakeLists.txt` with:

```cmake
add_executable(ZzWindowKitDemo
    main.cpp
    ZzWindowKitDemoWindow.cpp
    ZzWindowKitDemoWindowPrivate.cpp
)
target_link_libraries(ZzWindowKitDemo PRIVATE
    Qt6::Widgets
    Zz::WindowKit
)
zz_enable_project_warnings(ZzWindowKitDemo)
zz_enable_sanitizers(ZzWindowKitDemo)
```

Append to the baseline `examples/CMakeLists.txt`; the root `ZZ_BUILD_EXAMPLES` branch already guards the entire directory:

```cmake
add_subdirectory(ZzWindowKitDemo)
```

Append to `ZzWindowKit/tests/CMakeLists.txt`:

```cmake
add_executable(ZzWindowKitLifecycleTest
    ZzWindowKitLifecycleTest.cpp
)
target_link_libraries(ZzWindowKitLifecycleTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::WindowKit
)
set_target_properties(ZzWindowKitLifecycleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWindowKitLifecycleTest)
zz_enable_sanitizers(ZzWindowKitLifecycleTest)
add_test(NAME windowkit.lifecycle COMMAND ZzWindowKitLifecycleTest)
set_tests_properties(windowkit.lifecycle PROPERTIES
    LABELS "platform;windowkit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

Run:

```bash
cmake --preset linux-clang-asan -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-clang-asan --target ZzWindowKitDemo ZzWindowKitLifecycleTest
ctest --preset linux-clang-asan -R windowkit.lifecycle
./build/linux-clang-asan/examples/ZzWindowKitDemo/ZzWindowKitDemo
```

Expected: 自动测试 PASS；手工窗口可拖动、四边缩放、双击最大化、最小化、恢复、关闭。报告必须记录实际 `QGuiApplication::platformName()`。

- [ ] **Step 4: 分别记录 X11、Wayland 和 fallback**

在可用会话分别运行 demo。fallback 使用项目 option，不直接编辑第三方文件：

```bash
cmake --preset linux-gcc-debug -DZZ_WINDOWKIT_FORCE_QT_CONTEXT=ON
cmake --build --preset linux-gcc-debug --target ZzWindowKitDemo
./build/linux-gcc-debug/examples/ZzWindowKitDemo/ZzWindowKitDemo
cmake --preset linux-gcc-debug -DZZ_WINDOWKIT_FORCE_QT_CONTEXT=OFF
```

Expected: fallback 运行时 demo 显示且可使用 Qt 自身的 move/resize 能力；报告分别记录实际 `QGuiApplication::platformName()`、compositor/WM、系统菜单支持和已知限制。

- [ ] **Step 5: 提交示例和生命周期测试**

```bash
git add examples ZzWindowKit/tests ZzWindowKit/CMakeLists.txt
git commit -m "测试：验证 Linux 无边框窗口生命周期" \
    -m "增加真实窗口示例和一百次创建销毁测试。" \
    -m "区分 offscreen QObject smoke 与 X11、Wayland、Qt fallback 的原生交互验证。"
```

## Task 6: 隐藏 static 安装中的 QWK 依赖

**Files:**
- Modify: `ZzWindowKit/CMakeLists.txt`
- Create: `cmake/ZzWindowKitPrivateTargets.cmake.in`
- Modify: `cmake/ZzInstallPackage.cmake`
- Modify: `cmake/ZzPureToolsProConfig.cmake.in`
- Modify: `tests/InstallConsumer/RunInstallConsumer.cmake`

- [ ] **Step 1: 添加静态安装泄漏失败检查**

Modify `tests/InstallConsumer/RunInstallConsumer.cmake`。静态 install consumer 驱动在 consumer configure 前执行：

```cmake
set(zz_install_root "${zz_b_dir}")
cmake_path(ABSOLUTE_PATH zz_install_root NORMALIZE)
file(GLOB_RECURSE installed_cmake_files
    "${zz_install_root}/*.cmake")
foreach(cmake_file IN LISTS installed_cmake_files)
    file(READ "${cmake_file}" contents)
    if(contents MATCHES "QWindowKit::")
        message(FATAL_ERROR "Installed package leaks QWindowKit target: ${cmake_file}")
    endif()
endforeach()
file(GLOB_RECURSE installed_files LIST_DIRECTORIES FALSE
    "${zz_install_root}/*")
set(leaked_qwk_artifacts ${installed_files})
list(FILTER leaked_qwk_artifacts INCLUDE REGEX
    "/(QWKCore|QWKWidgets)/|/QWindowKit(Config|Targets)[^/]*\\.cmake$")
if(leaked_qwk_artifacts)
    message(FATAL_ERROR
        "Installed package leaks QWindowKit files: ${leaked_qwk_artifacts}")
endif()
```

这里必须使用基线驱动已经定义并实际接收安装结果的 `${zz_b_dir}`；不得另造未初始化的安装目录变量。先在 B 安装完成后、上述扫描前临时加入：

```cmake
file(WRITE "${zz_b_dir}/ZzQwkLeakFixture.cmake"
    "target_link_libraries(Fixture PRIVATE QWindowKit::Core)\n")
```

运行一次测试，必须由扫描器报告 `ZzQwkLeakFixture.cmake` 后失败，证明检查的是实际 B 树。随后删除这两行 fixture 注入并保留扫描器，再执行本 Step 的正式红灯；fixture 不得进入最终脚本。

Run:

```bash
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release -R install.consumer
```

Expected: FAIL，因为静态 `ZzWindowKit` 当前导出 link-only QWindowKit target。

- [ ] **Step 2: 分离 build/install link interface**

使用完整链接块，保留 ZzCore 和 Qt 公开依赖，且只在 build tree 中暴露 QWK target：

```cmake
target_link_libraries(ZzWindowKit
    PUBLIC
        Zz::Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    PRIVATE
        "$<BUILD_INTERFACE:QWindowKit::Core>"
        "$<BUILD_INTERFACE:QWindowKit::Widgets>"
)

if(NOT BUILD_SHARED_LIBS)
    target_link_libraries(ZzWindowKit
        INTERFACE
            "$<INSTALL_INTERFACE:$<LINK_ONLY:ZzPrivate::WindowKitWidgetsBackend>>"
    )
endif()
```


禁止用 `set_property(... INTERFACE_LINK_LIBRARIES ...)` 整体覆盖属性，那会丢失 `Zz::Core` 和 Qt 的 PUBLIC 依赖。配置后执行：

```bash
cmake --preset linux-static-release --trace-expand 2>&1 | tee build/linux-static-release/windowkit-configure.trace
rg 'INTERFACE_LINK_LIBRARIES' build/linux-static-release/windowkit-configure.trace
```

Expected: build interface 含 `QWindowKit::Core`/`QWindowKit::Widgets`，install interface 只含 `ZzPrivate::WindowKitWidgetsBackend`，且 Zz/Qt 公开依赖仍存在。

- [ ] **Step 3: 安装重命名内部归档**

在 `ZzWindowKit/CMakeLists.txt` 中只为 static Zz 构建增加以下完整安装规则：

```cmake
if(NOT BUILD_SHARED_LIBS)
    set(zz_qwk_private_install_dir
        "${CMAKE_INSTALL_LIBDIR}/zzpuretoolspro/private"
    )
    set(zz_qwk_core_file
        "ZzWindowKitBackendCore${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
    set(zz_qwk_widgets_file
        "ZzWindowKitBackendWidgets${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )

    set_target_properties(ZzWindowKit PROPERTIES
        ZZ_QWK_PRIVATE_INSTALL_DIR "${zz_qwk_private_install_dir}"
        ZZ_QWK_CORE_FILE "${zz_qwk_core_file}"
        ZZ_QWK_WIDGETS_FILE "${zz_qwk_widgets_file}"
    )

    install(FILES "$<TARGET_FILE:QWKCore>"
        DESTINATION "${zz_qwk_private_install_dir}"
        RENAME "${zz_qwk_core_file}"
        COMPONENT Development
    )
    install(FILES "$<TARGET_FILE:QWKWidgets>"
        DESTINATION "${zz_qwk_private_install_dir}"
        RENAME "${zz_qwk_widgets_file}"
        COMPONENT Development
    )
endif()
```

三个值必须保存为 `ZzWindowKit` target property，因为根目录调用的 `zz_install_package()` 不能读取 `ZzWindowKit` 子目录中的普通变量。`ZZ_QWK_CORE_FILE` 与 `ZZ_QWK_WIDGETS_FILE` 随后作为输入传给 Step 4 的 `configure_package_config_file()`；不得通过硬编码 `.a` 或 `.lib` 推断平台后缀。

不得安装 QWK 头、Config、qmake 或 MSBuild 文件。

- [ ] **Step 4: 生成私有 imported targets**

`ZzWindowKitPrivateTargets.cmake.in` 由 `configure_package_config_file()` 生成，使用 `@PACKAGE_INIT@` 计算 prefix，定义两个非文档化 imported target：

```cmake
if(NOT TARGET ZzPrivate::WindowKitCoreBackend)
    add_library(ZzPrivate::WindowKitCoreBackend STATIC IMPORTED)
endif()
set_target_properties(ZzPrivate::WindowKitCoreBackend PROPERTIES
    IMPORTED_LOCATION
        "${PACKAGE_PREFIX_DIR}/@ZZ_QWK_PRIVATE_INSTALL_DIR@/@ZZ_QWK_CORE_FILE@"
    INTERFACE_LINK_LIBRARIES "Qt6::Core;Qt6::Gui;${CMAKE_DL_LIBS}"
)

if(NOT TARGET ZzPrivate::WindowKitWidgetsBackend)
    add_library(ZzPrivate::WindowKitWidgetsBackend STATIC IMPORTED)
endif()
set_target_properties(ZzPrivate::WindowKitWidgetsBackend PROPERTIES
    IMPORTED_LOCATION
        "${PACKAGE_PREFIX_DIR}/@ZZ_QWK_PRIVATE_INSTALL_DIR@/@ZZ_QWK_WIDGETS_FILE@"
    INTERFACE_LINK_LIBRARIES
        "ZzPrivate::WindowKitCoreBackend;Qt6::Core;Qt6::Gui;Qt6::Widgets;@ZZ_QWK_PLATFORM_LIBRARIES@"
)
```

`ZZ_QWK_PLATFORM_LIBRARIES` 在生成文件前使用以下精确分支设置：

```cmake
if(WIN32)
    set(ZZ_QWK_PLATFORM_LIBRARIES "uxtheme")
elseif(APPLE)
    set(ZZ_QWK_PLATFORM_LIBRARIES
        "-framework Foundation;-framework Cocoa;-framework AppKit"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ZZ_QWK_PLATFORM_LIBRARIES "${CMAKE_DL_LIBS}")
else()
    message(FATAL_ERROR "ZzWindowKit supports only Windows, macOS, and Linux")
endif()
```

在 `cmake/ZzInstallPackage.cmake` 的 `zz_install_package()` 内、主 package Config 生成前加入以下完整读取、生成和安装逻辑：

```cmake
if(NOT BUILD_SHARED_LIBS)
    get_target_property(ZZ_QWK_PRIVATE_INSTALL_DIR
        ZzWindowKit ZZ_QWK_PRIVATE_INSTALL_DIR)
    get_target_property(ZZ_QWK_CORE_FILE
        ZzWindowKit ZZ_QWK_CORE_FILE)
    get_target_property(ZZ_QWK_WIDGETS_FILE
        ZzWindowKit ZZ_QWK_WIDGETS_FILE)

    foreach(zz_qwk_property IN ITEMS
        ZZ_QWK_PRIVATE_INSTALL_DIR
        ZZ_QWK_CORE_FILE
        ZZ_QWK_WIDGETS_FILE
    )
        if("${${zz_qwk_property}}" STREQUAL ""
           OR "${${zz_qwk_property}}" MATCHES "-NOTFOUND$")
            message(FATAL_ERROR
                "static package requires ZzWindowKit property ${zz_qwk_property}")
        endif()
    endforeach()

    configure_package_config_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ZzWindowKitPrivateTargets.cmake.in"
        "${PROJECT_BINARY_DIR}/ZzWindowKitPrivateTargets.cmake"
        INSTALL_DESTINATION "${zz_package_cmake_dir}"
    )
    install(FILES
        "${PROJECT_BINARY_DIR}/ZzWindowKitPrivateTargets.cmake"
        DESTINATION "${zz_package_cmake_dir}"
        COMPONENT Development
    )
endif()
```

模板中的安装路径必须使用 `@ZZ_QWK_PRIVATE_INSTALL_DIR@`，即：

```cmake
IMPORTED_LOCATION
    "${PACKAGE_PREFIX_DIR}/@ZZ_QWK_PRIVATE_INSTALL_DIR@/@ZZ_QWK_CORE_FILE@"
```

Widgets backend 同样使用该目录 property，不能再次硬编码 `@CMAKE_INSTALL_LIBDIR@/zzpuretoolspro/private`。以上 `configure_package_config_file()` 依赖前文已设置的 `ZZ_QWK_PLATFORM_LIBRARIES`；static 模式缺少任一 target property 必须在 configure 阶段失败，不能生成带 `-NOTFOUND` 的安装包。

Apple 分支禁止把 `find_library()` 得到的 Xcode SDK 绝对路径写入已安装 Config；由消费者当前 SDK 解析 framework 名称。不增加 X11/Wayland link，因为上游运行时动态加载它们。此静态封装支持“静态 Zz + Qt 官方动态 SDK”，不承诺静态 Qt SDK。

每个 preset 使用独立安装前缀，因此 private archive 只需一个 `IMPORTED_LOCATION`。安装脚本仍必须从 `$<TARGET_FILE:QWKCore>` 与 `$<TARGET_FILE:QWKWidgets>` 取得当前配置产物，不能拼接 Debug postfix；MSVC shared/static preset 的 `cmake --install ... --config Release` 必须由公共 consumer 驱动透传。

- [ ] **Step 5: 在 package Config 中校验 Qt minor**

配置时记录：

```cmake
set(ZZ_WINDOWKIT_BUILD_QT_MAJOR ${Qt6_VERSION_MAJOR})
set(ZZ_WINDOWKIT_BUILD_QT_MINOR ${Qt6_VERSION_MINOR})
```

生成 Config 后比较消费者 `Qt6_VERSION_MAJOR/MINOR`；不一致时 `set(ZzPureToolsPro_FOUND FALSE)` 并给出明确原因。静态模式先 include private targets，再 include 主 Targets。

- [ ] **Step 6: 验证 shared/static 安装消费者**

Run:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release -R install.consumer
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release -R install.consumer
```

Expected: 两种模式 PASS；安装 CMake 文件不包含 `QWindowKit::`，consumer 不需要 QWK headers/package/shared library。

- [ ] **Step 7: 提交静态封装**

```bash
git add ZzWindowKit cmake tests/InstallConsumer
git commit -m "构建：隐藏静态 WindowKit 的 QWK 依赖" \
    -m "将 QWK 归档作为重命名的包内私有实现安装。" \
    -m "消费者只解析 Zz 私有 imported target，并校验构建时 Qt 主次版本。"
```

## Task 7: 增加边界扫描和第三方治理记录

**Files:**
- Create: `tests/Architecture/CheckZzWindowKitBoundaries.cmake`
- Modify: `tests/Architecture/CMakeLists.txt`
- Create: `docs/third-party/qwindowkit-vendor.json`
- Create: `docs/third-party/THIRD_PARTY_NOTICES.md`

- [ ] **Step 1: 创建 QWK 边界扫描**

`CheckZzWindowKitBoundaries.cmake` 接收并规范化 `ZZ_SOURCE_DIR`，不存在或为空的扫描根必须失败。脚本规则：

- `ZzWindowKit/include` 中禁止 `QWK`、`QWindowKit`、`private/`。
- 全部一方源码中，只有 `ZzWindowKit/src/private/ZzQWindowKitBackend.*` 可以 include `QWKCore/` 或 `QWKWidgets/`。
- 禁止 include `qwindowkit_linux.h`、`qwindowkit_windows.h` 和任何 `*_p.h`。
- 安装树禁止 QWK header 和 `QWindowKitConfig.cmake`；该规则由 Task 6 的 `RunInstallConsumer.cmake` 在真实安装树执行，本脚本不把源码树冒充安装树。

Append to `tests/Architecture/CMakeLists.txt`:

```cmake
add_test(NAME architecture.zzwindowkit-boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzWindowKitBoundaries.cmake")
set_tests_properties(architecture.zzwindowkit-boundaries PROPERTIES
    LABELS "architecture;windowkit")
```

- [ ] **Step 2: 写 vendor manifest**

JSON 必须包含真实值：

```json
{
  "name": "QWindowKit",
  "declaredVersion": "1.5.1.0",
  "upstreamUrl": "https://github.com/stdware/qwindowkit",
  "upstreamCommit": null,
  "importDate": "2026-08-02",
  "archiveSha256": null,
  "licenses": ["Apache-2.0", "MIT"],
  "localPatches": [],
  "validatedMatrix": [],
  "releaseBlockers": [
    "qwindowkit.upstream-provenance",
    "qmsetup.windeployqt-5.15.2-derived-work"
  ]
}
```

`null` 和空 `validatedMatrix` 是已知未知/未验证信息，不得伪造 commit、校验和或平台结果。每个 blocker 使用跨文档稳定 id；只有原生 runner 证据可以向 `validatedMatrix` 增加记录，发布前必须用最终发布计划的 hash/reviewer 检查消除两个 blocker。

- [ ] **Step 3: 写 notice 清单**

记录 QWindowKit Apache-2.0、FramelessHelper MIT、qmsetup MIT、syscmdline MIT 的文件路径、版权声明和随包位置。不要复制无法确认的许可证结论。

- [ ] **Step 4: 运行全部 WindowKit 门禁**

Run:

```bash
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug -L windowkit
ctest --preset linux-gcc-debug -R architecture.zzwindowkit-boundaries
cmake --build --preset linux-clang-asan
ctest --preset linux-clang-asan -L windowkit
git diff --check
```

Expected: 全部 PASS；无 sanitizer 和 whitespace 错误。

- [ ] **Step 5: 提交边界与治理文档**

```bash
git add tests/Architecture docs/third-party
git commit -m "文档：记录 QWindowKit 来源与隔离门禁" \
    -m "登记上游版本、许可证、未知 commit 和再许可发布阻塞项。" \
    -m "增加公共头、源码引用和安装树扫描，防止 QWK 与 Qt Private API 泄漏。"
```

## 完成标准

- public headers 只包含 Zz 类型和 Qt public 类型。
- attach/configure/effect/menu 的状态、线程、所有权和失败语义有测试。
- title bar 更换始终重绑全部系统按钮和 hit-test widget。
- shared consumer 不部署 QWK library；static consumer 不查找 QWindowKit package。
- Qt major/minor 不匹配时二进制 package 明确拒绝。
- QWK QObject 仅由 `unique_ptr` 拥有，不与 host parent 树形成双重所有权；host/agent 两种销毁顺序均通过 Sanitizer。
- Linux 生命周期测试在 ASan/UBSan 下通过，原生测试报告区分 X11/Wayland/fallback。
- vendor 未知信息明确标为发布 blocker。
