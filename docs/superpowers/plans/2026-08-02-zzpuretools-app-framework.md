# ZzAppCore 与 ZzPureTools 应用框架 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立无 Widgets 依赖的模块生命周期内核，以及严格分离业务与 UI 的页面、导航、多窗口和应用装配框架。

**Architecture:** `Zz::AppCore` 只依赖 `Zz::Core` 和 Qt Core，负责模块图、拓扑排序、启动回滚与逆序停止。`Zz::PureTools` 提供 Widgets 外壳，页面 factory 只在 composition root 中组合 Presenter、ViewModel 和 View；每个窗口独立拥有 WindowAgent、导航、历史、页面和动画状态。

**Tech Stack:** Qt 6.8+ Core/Gui/Widgets/Test、C++20、`ZzResult`、`std::unique_ptr`、`std::span`、Qt Model/View、ZzWindowKit、ZzFluentUI、CMake 3.23、CTest。

---

## 前置条件与硬边界

- 工作目录固定为 `/home/zz/Jackfahdin/github/ZzPureToolsFrame/ZzPureToolsFrame`。
- 先完成工程基线、ZzLog、ZzCore、ZzWindowKit、FluentFoundation 和基础控件计划。
- `Zz::AppCore` 禁止 include/link Qt Gui、Widgets、Quick、ZzWindowKit 和 ZzFluentUI。
- `main.cpp` 是唯一了解具体业务模块与页面 factory 的 composition root。
- 不增加 Service Locator、字符串 EventBus、动态插件加载、进程级导航或进程级撤销栈。
- 公开有状态 QObject/QWidget 使用四文件 PIMPL；所有公开 API 补齐简体中文 Doxygen、所有权和线程前提。

## 文件边界

### AppCore 值类型与模块图

- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzModuleId.h`
- Create: `ZzPureTools/appcore/src/ZzModuleId.cpp`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzRouteId.h`
- Create: `ZzPureTools/appcore/src/ZzRouteId.cpp`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzModuleDescriptor.h`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzApplicationModule.h`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzModuleGraphBuilder.h`
- Create: `ZzPureTools/appcore/src/ZzModuleGraphBuilder.cpp`
- Create: `ZzPureTools/appcore/src/private/ZzModuleGraphBuilderPrivate.h`
- Create: `ZzPureTools/appcore/src/private/ZzModuleGraphBuilderPrivate.cpp`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzApplicationRuntime.h`
- Create: `ZzPureTools/appcore/src/ZzApplicationRuntime.cpp`
- Create: `ZzPureTools/appcore/src/private/ZzApplicationRuntimePrivate.h`
- Create: `ZzPureTools/appcore/src/private/ZzApplicationRuntimePrivate.cpp`
- Create: `ZzPureTools/tests/ZzModuleGraphTest.cpp`
- Create: `ZzPureTools/tests/ZzApplicationRuntimeTest.cpp`

### Widgets 页面与导航

- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageLifetimePolicy.h`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageInstance.h`
- Create: `ZzPureTools/widgets/src/ZzPageInstance.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzPageInstancePrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzPageInstancePrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageRegistration.h`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageHost.h`
- Create: `ZzPureTools/widgets/src/ZzPageHost.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzPageHostPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzPageHostPrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzNavigationNode.h`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzNavigationModel.h`
- Create: `ZzPureTools/widgets/src/ZzNavigationModel.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationModelPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationModelPrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzNavigationController.h`
- Create: `ZzPureTools/widgets/src/ZzNavigationController.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationControllerPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationControllerPrivate.cpp`
- Create: `ZzPureTools/tests/ZzPageLifecycleTest.cpp`
- Create: `ZzPureTools/tests/ZzNavigationControllerTest.cpp`

### 应用、窗口与装配

- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPureApplication.h`
- Create: `ZzPureTools/widgets/src/ZzPureApplication.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzApplicationBuilder.h`
- Create: `ZzPureTools/widgets/src/ZzApplicationBuilder.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationBuilderPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationBuilderPrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzApplicationWindow.h`
- Create: `ZzPureTools/widgets/src/ZzApplicationWindow.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp`
- Create: `ZzPureTools/tests/ZzApplicationBuilderTest.cpp`
- Create: `ZzPureTools/tests/ZzMultiWindowIsolationTest.cpp`
- Create: `ZzPureTools/tests/ZzTranslationLifecycleTest.cpp`

### 示例、构建和门禁

- Modify: `ZzPureTools/CMakeLists.txt`
- Create: `ZzPureTools/tests/CMakeLists.txt`
- Create: `examples/ZzPureToolsDemo/CMakeLists.txt`
- Create: `examples/ZzPureToolsDemo/main.cpp`
- Create: `examples/ZzPureToolsDemo/ZzDemoModule.h`
- Create: `examples/ZzPureToolsDemo/ZzDemoModule.cpp`
- Create: `examples/ZzPureToolsDemo/ZzDemoPageFactory.h`
- Create: `examples/ZzPureToolsDemo/ZzDemoPageFactory.cpp`
- Modify: `examples/CMakeLists.txt`
- Create: `tests/Architecture/CheckZzPureToolsBoundaries.cmake`
- Create: `tests/Architecture/CheckZzPureToolsBoundariesContract.cmake`
- Create: `tests/Architecture/fixtures/zzpuretools-good/appcore/ZzGoodAppCore.h`
- Create: `tests/Architecture/fixtures/zzpuretools-good/widgets/ZzGoodWidget.h`
- Create: `tests/Architecture/fixtures/zzpuretools-bad/appcore/ZzBadAppCore.h`
- Create: `tests/Architecture/fixtures/zzpuretools-bad/widgets/ZzBadWidget.h`
- Create: `tests/Architecture/fixtures/zzpuretools-bad/widgets/ZzAllowedComposition.cpp`
- Modify: `tests/Architecture/CMakeLists.txt`
- Modify: `tests/InstallConsumer/main.cpp`

## Task 1: 定义强类型 ID 和模块契约

**Files:**
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzModuleId.h`
- Create: `ZzPureTools/appcore/src/ZzModuleId.cpp`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzRouteId.h`
- Create: `ZzPureTools/appcore/src/ZzRouteId.cpp`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzModuleDescriptor.h`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzApplicationModule.h`
- Create: `ZzPureTools/tests/ZzModuleGraphTest.cpp`
- Modify: `ZzPureTools/CMakeLists.txt`
- Create: `ZzPureTools/tests/CMakeLists.txt`

- [ ] **Step 1: 写值语义和模块契约失败测试**

Create `ZzPureTools/tests/ZzModuleGraphTest.cpp` with the first slots:

```cpp
#include <QtTest/QTest>

#include <type_traits>

#include <ZzPureTools/ZzApplicationModule.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzRouteId.h>

class ZzModuleGraphTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void idsAreOwningAndStronglyTyped()
    {
        const ZzPureTools::ZzModuleId module(QStringLiteral("settings"));
        const ZzPureTools::ZzRouteId route(QStringLiteral("settings"));
        QCOMPARE(module.value(), QStringLiteral("settings"));
        QCOMPARE(route.value(), QStringLiteral("settings"));
        QVERIFY(module.isValid());
        QVERIFY(route.isValid());
        static_assert(!std::is_same_v<
            ZzPureTools::ZzModuleId,
            ZzPureTools::ZzRouteId>);
    }

    void emptyIdsAreInvalid()
    {
        QVERIFY(!ZzPureTools::ZzModuleId().isValid());
        QVERIFY(!ZzPureTools::ZzRouteId(QStringLiteral("   ")).isValid());
    }
};

QTEST_GUILESS_MAIN(ZzModuleGraphTest)

#include "ZzModuleGraphTest.moc"
```

- [ ] **Step 2: 注册测试并确认缺少类型**

Create `ZzPureTools/tests/CMakeLists.txt` with:

```cmake
add_executable(ZzModuleGraphTest ZzModuleGraphTest.cpp)
target_link_libraries(ZzModuleGraphTest PRIVATE Qt6::Test Zz::AppCore)
set_target_properties(ZzModuleGraphTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzModuleGraphTest)
zz_enable_sanitizers(ZzModuleGraphTest)
add_test(NAME appcore.module-graph COMMAND ZzModuleGraphTest)
set_tests_properties(appcore.module-graph PROPERTIES LABELS "unit;appcore")
```

在 `ZzPureTools/CMakeLists.txt` 末尾加入测试目录；此时不得提前引用尚未创建的生产 `.cpp`：

```cmake
if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzModuleGraphTest
```

Expected: compile FAIL，缺少 `ZzModuleId.h` 或 `ZzRouteId.h`。

- [ ] **Step 3: 实现两个互不转换的 ID 值类型**

`ZzModuleId.h` 完整公开契约：

```cpp
#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QTypeInfo>

#include <ZzPureTools/ZzAppCoreExport.h>

namespace ZzPureTools {

/**
 * @brief 保存稳定、拥有字符串的模块标识。
 */
class ZZ_APP_CORE_EXPORT ZzModuleId final
{
public:
    ZzModuleId() = default;
    explicit ZzModuleId(QString value);

    /** @brief 判断标识是否为非空、非纯空白值。 */
    [[nodiscard]] bool isValid() const noexcept;
    /** @brief 返回所有的标识文本。 */
    [[nodiscard]] const QString &value() const noexcept;

    friend bool operator==(const ZzModuleId &, const ZzModuleId &) = default;

private:
    QString value_;
};

ZZ_APP_CORE_EXPORT size_t qHash(
    const ZzModuleId &id,
    size_t seed = 0) noexcept;

} // namespace ZzPureTools

Q_DECLARE_TYPEINFO(ZzPureTools::ZzModuleId, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzPureTools::ZzModuleId)
```

Create `ZzModuleId.cpp` with:

```cpp
#include <ZzPureTools/ZzModuleId.h>

namespace ZzPureTools {

ZzModuleId::ZzModuleId(QString value)
    : value_(value.trimmed())
{
}

bool ZzModuleId::isValid() const noexcept
{
    return !value_.isEmpty();
}

const QString &ZzModuleId::value() const noexcept
{
    return value_;
}

size_t qHash(const ZzModuleId &id, size_t seed) noexcept
{
    return ::qHash(id.value(), seed);
}

} // namespace ZzPureTools
```

Create `ZzRouteId.h` with:

```cpp
#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QTypeInfo>

#include <ZzPureTools/ZzAppCoreExport.h>

namespace ZzPureTools {

/**
 * @brief 保存稳定、拥有字符串的页面路由标识。
 */
class ZZ_APP_CORE_EXPORT ZzRouteId final
{
public:
    ZzRouteId() = default;
    explicit ZzRouteId(QString value);
    /** @brief 判断标识是否为非空、非纯空白值。 */
    [[nodiscard]] bool isValid() const noexcept;
    /** @brief 返回所有的标识文本。 */
    [[nodiscard]] const QString &value() const noexcept;
    friend bool operator==(const ZzRouteId &, const ZzRouteId &) = default;

private:
    QString value_;
};

ZZ_APP_CORE_EXPORT size_t qHash(
    const ZzRouteId &id,
    size_t seed = 0) noexcept;

} // namespace ZzPureTools

Q_DECLARE_TYPEINFO(ZzPureTools::ZzRouteId, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(ZzPureTools::ZzRouteId)
```

Create `ZzRouteId.cpp` with:

```cpp
#include <ZzPureTools/ZzRouteId.h>

namespace ZzPureTools {

ZzRouteId::ZzRouteId(QString value)
    : value_(value.trimmed())
{
}

bool ZzRouteId::isValid() const noexcept
{
    return !value_.isEmpty();
}

const QString &ZzRouteId::value() const noexcept
{
    return value_;
}

size_t qHash(const ZzRouteId &id, size_t seed) noexcept
{
    return ::qHash(id.value(), seed);
}

} // namespace ZzPureTools
```

两种 ID 之间不增加构造或转换操作。

完成两个 `.cpp` 后，编辑 `ZzPureTools/CMakeLists.txt` 中 `add_library(ZzAppCore ...)` 之前的源文件清单，替换为当前完整内容。禁止在 `zz_configure_library_target(... SOURCES ${zz_app_core_sources})` 调用之后追加生产翻译单元：

```cmake
set(zz_app_core_sources
    appcore/src/private/ZzAppCoreVersion.cpp
    appcore/src/ZzModuleId.cpp
    appcore/src/ZzRouteId.cpp
)
```

- [ ] **Step 4: 定义 descriptor 和无 UI 模块接口**

Create `ZzModuleDescriptor.h` with:

```cpp
#pragma once

#include <QtCore/QList>
#include <QtCore/QString>

#include <ZzPureTools/ZzModuleId.h>

namespace ZzPureTools {

/**
 * @brief 描述模块的稳定身份、版本和直接依赖。
 */
struct ZZ_APP_CORE_EXPORT ZzModuleDescriptor final
{
    ZzModuleId id;
    QString version;
    QList<ZzModuleId> dependencies;
};

} // namespace ZzPureTools
```

Create `ZzApplicationModule.h` with:

```cpp
#pragma once

#include <ZzCore/ZzResult.h>
#include <ZzPureTools/ZzModuleDescriptor.h>

namespace ZzPureTools {

/**
 * @brief 定义一个由 composition root 显式构造的应用模块。
 *
 * 所有方法均在应用主线程调用。requestStop() 只发出协作停止请求，
 * stop() 必须 noexcept 并完成最终资源回收。
 */
class ZZ_APP_CORE_EXPORT ZzApplicationModule
{
public:
    virtual ~ZzApplicationModule() = default;
    [[nodiscard]] virtual ZzModuleDescriptor descriptor() const = 0;
    [[nodiscard]] virtual ZzCore::ZzResult<void> start() = 0;
    virtual void requestStop() noexcept = 0;
    virtual void stop() noexcept = 0;
};

} // namespace ZzPureTools
```

- [ ] **Step 5: 运行 ID 契约测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzModuleGraphTest
ctest --preset linux-gcc-debug -R '^appcore\.module-graph$' --output-on-failure
```

Expected: PASS，强类型 ID 持有自己的文本，空白 ID 不合法。

- [ ] **Step 6: 提交模块基础契约**

```bash
git add ZzPureTools/appcore ZzPureTools/tests ZzPureTools/CMakeLists.txt
git commit -m "框架：定义模块与路由强类型契约" \
    -m "增加拥有字符串的模块和路由标识。" \
    -m "定义不依赖 Widgets 的模块描述、启动和停止接口。"
```

## Task 2: 实现拓扑排序、启动回滚与逆序停止

**Files:**
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzModuleGraphBuilder.h`
- Create: `ZzPureTools/appcore/src/ZzModuleGraphBuilder.cpp`
- Create: `ZzPureTools/appcore/src/private/ZzModuleGraphBuilderPrivate.h`
- Create: `ZzPureTools/appcore/src/private/ZzModuleGraphBuilderPrivate.cpp`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzApplicationRuntime.h`
- Create: `ZzPureTools/appcore/src/ZzApplicationRuntime.cpp`
- Create: `ZzPureTools/appcore/src/private/ZzApplicationRuntimePrivate.h`
- Create: `ZzPureTools/appcore/src/private/ZzApplicationRuntimePrivate.cpp`
- Modify: `ZzPureTools/tests/ZzModuleGraphTest.cpp`
- Create: `ZzPureTools/tests/ZzApplicationRuntimeTest.cpp`
- Modify: `ZzPureTools/CMakeLists.txt`
- Modify: `ZzPureTools/tests/CMakeLists.txt`

- [ ] **Step 1: 写重复、缺失、成环和稳定顺序失败测试**

在测试内定义 `ZzRecordingModule final : public ZzApplicationModule`，构造函数接收 descriptor、`QStringList *events` 和可预设的 start result。`start/requestStop/stop` 分别追加 `start:<id>`、`request:<id>`、`stop:<id>`。增加以下精确 slot：

```cpp
void rejectsDuplicateIds();
void rejectsMissingDependency();
void rejectsCycle();
void ordersIndependentModulesByRegistrationOrder();
void convertsThrownDescriptorToErrorWithoutInventingModuleId();
void rollsBackOnlyStartedModules();
void requestsAndStopsInReverseOrder();
```

`rollsBackOnlyStartedModules()` 使用 `A -> B -> C`，让 B 的 `start()` 返回 `Backend`，最终 events 必须精确为：

```cpp
QStringList{
    QStringLiteral("start:A"),
    QStringLiteral("start:B"),
    QStringLiteral("request:A"),
    QStringLiteral("stop:A")
}
```

`convertsThrownDescriptorToErrorWithoutInventingModuleId()` 让注册序号 1 的测试模块在 `descriptor()` 抛 `std::runtime_error`，断言 builder 返回 `Unknown`，context 包含 `registrationIndex=1` 和测试模块运行时类型名，但不得虚构尚未取得的 module ID。`ZzApplicationRuntimeTest.cpp` 还必须完整实现 `convertsThrownStartToErrorAndRollsBack()`：B 的 `start()` 抛 `std::runtime_error`，断言返回 `Unknown`，事件仍严格为 `start:A,start:B,request:A,stop:A`。向 tests CMake 追加：

```cmake
add_executable(ZzApplicationRuntimeTest ZzApplicationRuntimeTest.cpp)
target_link_libraries(ZzApplicationRuntimeTest PRIVATE Qt6::Test Zz::AppCore)
set_target_properties(ZzApplicationRuntimeTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzApplicationRuntimeTest)
zz_enable_sanitizers(ZzApplicationRuntimeTest)
add_test(NAME appcore.runtime COMMAND ZzApplicationRuntimeTest)
set_tests_properties(appcore.runtime PROPERTIES LABELS "unit;appcore")
```

- [ ] **Step 2: 运行并确认 builder/runtime 缺失**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzModuleGraphTest ZzApplicationRuntimeTest
```

Expected: compile FAIL，缺少 `ZzModuleGraphBuilder.h` 和 `ZzApplicationRuntime.h`。

- [ ] **Step 3: 声明一次性 builder 与 runtime**

`ZzModuleGraphBuilder` 公开 API：

```cpp
class ZZ_APP_CORE_EXPORT ZzModuleGraphBuilder final
{
public:
    ZzModuleGraphBuilder();
    ~ZzModuleGraphBuilder();
    ZzModuleGraphBuilder(ZzModuleGraphBuilder &&) noexcept;
    ZzModuleGraphBuilder &operator=(ZzModuleGraphBuilder &&) noexcept;

    [[nodiscard]] ZzCore::ZzResult<void> addModule(
        std::unique_ptr<ZzApplicationModule> module);
    [[nodiscard]] ZzCore::ZzResult<
        std::unique_ptr<ZzApplicationRuntime>> build();
    [[nodiscard]] bool isFrozen() const noexcept;

private:
    std::unique_ptr<ZzModuleGraphBuilderPrivate> d_ptr;
};
```

`ZzApplicationRuntime` 公开 API：

```cpp
class ZZ_APP_CORE_EXPORT ZzApplicationRuntime final
{
public:
    ~ZzApplicationRuntime();
    ZzApplicationRuntime(ZzApplicationRuntime &&) noexcept;
    ZzApplicationRuntime &operator=(ZzApplicationRuntime &&) noexcept;

    [[nodiscard]] ZzCore::ZzResult<void> start();
    void requestStop() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool isRunning() const noexcept;
    [[nodiscard]] qsizetype moduleCount() const noexcept;

private:
    friend class ZzModuleGraphBuilderPrivate;
    explicit ZzApplicationRuntime(
        std::vector<std::unique_ptr<ZzApplicationModule>> modules);
    std::unique_ptr<ZzApplicationRuntimePrivate> d_ptr;
};
```

builder 成功或失败执行 `build()` 后都进入 frozen，后续 `addModule/build` 返回 `InvalidState`。

- [ ] **Step 4: 实现稳定 Kahn 拓扑排序**

Private 使用 `QHash<ZzModuleId, qsizetype>` 建立 ID 到注册顺序的映射，并使用以下步骤：

1. 先验证非空 module、有效 ID、非空 version、无重复 ID 和无自依赖。
2. 再验证所有 dependency ID 存在，并为每个节点计算入度。
3. 按模块注册顺序建立 adjacency，因此每个 dependency 的 dependent 列表天然稳定；把初始入度为 0 的节点按注册顺序放入 `std::deque<qsizetype>`，之后每个刚变为 0 的 dependent 按 adjacency 顺序 push_back。使用 FIFO，不在每轮重新寻找全局最小节点。
4. 输出数量小于模块数时返回 `InvalidArgument`；再按原注册顺序线性遍历一次，把仍有入度的 ID 写入 context，不另行排序。
5. 按拓扑结果 move `unique_ptr`进 runtime，builder 不再保留模块。

该稳定规则保证相同注册输入得到相同输出，复杂度严格为 `O(V + E)`；它不承诺每一步都选择当前 ready 集合中的最小注册序。禁止使用每轮全表扫描，也不得一边声称 `O(V + E)` 一边使用 `std::set`/heap。

- [ ] **Step 5: 实现 runtime 状态和失败回滚**

Private 状态严格为 `Built -> Starting -> Running -> StopRequested -> Stopped`，启动失败从 `Starting` 进入 `Stopped`。builder 对 `descriptor()` 的调用和 runtime 对 `start()` 的调用均包在 `try/catch` 中，`std::exception` 与未知异常转换为 `Unknown`。`descriptor()` 抛异常时尚未取得模块 ID，context 只能记录稳定注册序号和 `typeid(*module).name()`，不得读取或虚构 ID；descriptor 成功后及 `start()` 失败时才附真实模块 ID。`start()` 对每个模块顺序调用；某个返回失败或抛异常时，不对失败模块调用 stop，只对已成功列表逆序调用 `requestStop()` 和 `stop()`。`requestStop()` 幂等，`stop()` 幂等且逆序停止。runtime 析构时若仍在运行，依次执行 request/stop；move assignment 在覆盖自身 private 前也必须先完成同样关闭，不能静默销毁正在运行的模块。

实现完成后再次替换 `add_library(ZzAppCore ...)` 之前的 `zz_app_core_sources`，使用当前累计清单：

```cmake
set(zz_app_core_sources
    appcore/src/private/ZzAppCoreVersion.cpp
    appcore/src/ZzModuleId.cpp
    appcore/src/ZzRouteId.cpp
    appcore/src/ZzModuleGraphBuilder.cpp
    appcore/src/private/ZzModuleGraphBuilderPrivate.cpp
    appcore/src/ZzApplicationRuntime.cpp
    appcore/src/private/ZzApplicationRuntimePrivate.cpp
)
```

- [ ] **Step 6: 运行全部模块测试和 Sanitizer**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzModuleGraphTest ZzApplicationRuntimeTest
ctest --preset linux-gcc-debug -R '^appcore\.(module-graph|runtime)$' --repeat until-fail:20 --output-on-failure
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan --target ZzApplicationRuntimeTest
ctest --preset linux-clang-asan -R '^appcore\.runtime$' --output-on-failure
```

Expected: 连续 20 轮 PASS；ASan/UBSan 无泄漏、重复 stop 或越界访问。

- [ ] **Step 7: 提交模块图与 runtime**

```bash
git add ZzPureTools/appcore ZzPureTools/tests ZzPureTools/CMakeLists.txt
git commit -m "框架：实现模块图与生命周期" \
    -m "检测重复、缺失依赖和环，并生成稳定拓扑顺序。" \
    -m "按顺序启动模块，在失败时只回滚已成功模块，关闭时逆序停止。"
```

## Task 3: 实现单一所有权的页面实例与宿主

**Files:**
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageLifetimePolicy.h`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageInstance.h`
- Create: `ZzPureTools/widgets/src/ZzPageInstance.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzPageInstancePrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzPageInstancePrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageRegistration.h`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPageHost.h`
- Create: `ZzPureTools/widgets/src/ZzPageHost.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzPageHostPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzPageHostPrivate.cpp`
- Create: `ZzPureTools/tests/ZzPageLifecycleTest.cpp`
- Modify: `ZzPureTools/CMakeLists.txt`
- Modify: `ZzPureTools/tests/CMakeLists.txt`

- [ ] **Step 1: 写页面创建、销毁顺序与策略失败测试**

测试定义会在析构时追加事件的 `ZzPageProbeObject : public QObject` 和 `ZzPageProbeWidget : public QWidget`，覆盖：

```cpp
void rejectsViewWithDifferentPageParent();
void destroysViewBeforePresenterAndViewModel();
void cancelsTasksBeforeDestroyingPresentationObjects();
void cancellationExceptionDoesNotSkipDestruction();
void persistentPageIsReused();
void whileActivePageIsDestroyedOnLeave();
void recreatableCacheNeverExceedsCapacity();
void failedFactoryLeavesNoHalfInitializedWidget();
void thrownFactoryBecomesUnknownAndPreservesCurrentPage();
void successfulNullFactoryBecomesInvalidStateAndPreservesCurrentPage();
```

销毁事件的精确期望为 `cancel, disconnect, view, presenter, view-model`。

向 tests CMake 追加：

```cmake
add_executable(ZzPageLifecycleTest ZzPageLifecycleTest.cpp)
target_link_libraries(ZzPageLifecycleTest PRIVATE Qt6::Test Zz::PureTools)
set_target_properties(ZzPageLifecycleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPageLifecycleTest)
zz_enable_sanitizers(ZzPageLifecycleTest)
add_test(NAME puretools.page-lifecycle COMMAND ZzPageLifecycleTest)
set_tests_properties(puretools.page-lifecycle PROPERTIES
    LABELS "component;puretools"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 2: 运行红灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzPageLifecycleTest
```

Expected: compile FAIL，缺少 `ZzPageInstance.h` 和 `ZzPageHost.h`。

- [ ] **Step 3: 定义页面策略、factory 和注册项**

```cpp
enum class ZzPageLifetimePolicy : std::uint8_t
{
    Persistent,
    WhileActive,
    Recreatable
};

using ZzPageFactory = std::function<ZzCore::ZzResult<
    std::unique_ptr<ZzPageInstance>>(QWidget *pageParent)>;

struct ZzPageRegistration final
{
    ZzRouteId routeId;
    ZzPageLifetimePolicy lifetime = ZzPageLifetimePolicy::Recreatable;
    ZzPageFactory factory;
};
```

factory 只允许使用传入 `pageParent` 作为 View 的 QObject parent。失败返回前 factory 必须销毁局部 View/Presenter/ViewModel，不得把半初始化对象留给 host。

- [ ] **Step 4: 实现 ZzPageInstance 四文件所有权**

Public API：

```cpp
class ZZ_PURE_TOOLS_EXPORT ZzPageInstance final
{
public:
    using ZzCancelCallback = std::function<void()>;

    [[nodiscard]] static ZzCore::ZzResult<std::unique_ptr<ZzPageInstance>>
    create(
        QWidget *pageParent,
        QWidget *view,
        std::unique_ptr<QObject> viewModel,
        std::unique_ptr<QObject> presenter);
    ~ZzPageInstance();

    [[nodiscard]] QWidget *view() const noexcept;
    void addCancellation(ZzCancelCallback callback);
    void prepareForDestruction() noexcept;

private:
    explicit ZzPageInstance(
        QWidget *view,
        std::unique_ptr<QObject> viewModel,
        std::unique_ptr<QObject> presenter);
    std::unique_ptr<ZzPageInstancePrivate> d_ptr;
};
```

`create()` 要求 `pageParent/view/viewModel/presenter` 均非空、`view->parentWidget() == pageParent`，并要求 Presenter/ViewModel 的 QObject parent 为 null。失败时它必须在返回前 delete view；两个 unique_ptr 由参数析构自动释放，调用方不得再次删除。成功后的所有权固定为：View 只由 `pageParent` 的 Qt parent 树拥有，PageInstance 仅保存 `QPointer<QWidget>`；Presenter 和 ViewModel 分别由 `unique_ptr<QObject>` 拥有。公开 Doxygen 明确 `pageParent` 必须比 `ZzPageInstance` 存活更久，host 析构时通过先 reset private 保证该顺序。

`prepareForDestruction()` 幂等执行：禁用 View；逐个调用 cancel callback，每次调用都包在 `try/catch` 中并只记录异常，不能跳过后续清理；追加一次 `disconnect` 事件后调用 `QObject::disconnect`；`delete view`；`presenter.reset()`；`viewModel.reset()`。析构函数只调用该方法且保持 noexcept。`addCancellation()` 在已 prepare 后立即执行并捕获传入 callback，而不是把它留在永不执行的列表中。

- [ ] **Step 5: 实现带上限的 PageHost**

`ZzPageHost.h` 使用以下完整 API；所有方法只允许 GUI/owner 线程调用：

```cpp
class ZZ_PURE_TOOLS_EXPORT ZzPageHost final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPageHost)

public:
    explicit ZzPageHost(QWidget *parent = nullptr);
    ~ZzPageHost() override;

    [[nodiscard]] ZzCore::ZzResult<void> activate(
        const ZzPageRegistration &registration);
    void deactivateCurrent() noexcept;
    [[nodiscard]] ZzCore::ZzResult<void> showFrameworkError(
        ZzRouteId failedRoute);
    [[nodiscard]] ZzRouteId currentRoute() const;
    [[nodiscard]] ZzCore::ZzResult<void> setRecreatableCapacity(
        qsizetype capacity);

private:
    std::unique_ptr<ZzPageHostPrivate> d_ptr;
};
```

Private 定义仅在 private header 可见的 entry，不把 move-only 值放入 Qt 隐式共享容器：

```cpp
struct ZzPageEntry final
{
    ZzPageLifetimePolicy policy = ZzPageLifetimePolicy::Recreatable;
    std::unique_ptr<ZzPageInstance> instance;
};

std::map<QString, ZzPageEntry> pages;
std::list<QString> recreatableLru;
```

Private 还保存 QObject-parented `QStackedWidget *stack`、QObject-parented framework error widget、当前 route 和默认 capacity=3。规则：

- `Persistent` 一直保留到 host 销毁。
- `WhileActive` 离开后立即 `prepareForDestruction()` 并从 map 移除。
- `Recreatable` 离开后进 LRU，默认容量 3；超限时销毁最旧的非当前页。
- factory 返回失败时保持原当前页，返回原始 `ZzError`。
- factory 调用前记录 `pageParent` 当前直接 QObject children。调用整体包在 `try/catch(std::exception)` 和 `catch(...)` 中：异常转换为 `Unknown`；成功 Result 内含空 `unique_ptr` 转换为 `InvalidState`。返回失败、抛异常或空成功时，删除本次调用新挂到 `pageParent` 的直接 child，且不得修改当前可见页、pages map、LRU 或 current route；异常绝不能逃入 Qt signal/slot 调用栈。
- `showFrameworkError(failedRoute)` 先按当前 entry policy 执行与正常离开相同的 deactivate/LRU/销毁流程，再显示仅含通用可翻译文案的 error widget，并把 current route 设为 failedRoute；它不接收或显示技术错误。
- `setRecreatableCapacity()` 拒绝负数；缩容立即从 LRU 最旧端逐项驱逐，capacity=0 表示离开即回收。

切换必须是事务式：先查找/创建目标 entry；factory 成功后才 deactivate 旧页并切换 stack。创建失败不修改 map、LRU、current route 或当前可见 widget。PageHost 析构函数体先 reset d_ptr，使 PageInstance 在 QWidget 基类删除 child tree 前按规定顺序删除 View。

实现完成后编辑 `add_library(ZzPureTools ...)` 之前的 `zz_pure_tools_sources`，替换为当前完整清单；不得在 target helper 调用之后使用 `target_sources()`：

```cmake
set(zz_pure_tools_sources
    widgets/src/private/ZzPureToolsVersion.cpp
    widgets/src/ZzPageInstance.cpp
    widgets/src/private/ZzPageInstancePrivate.cpp
    widgets/src/ZzPageHost.cpp
    widgets/src/private/ZzPageHostPrivate.cpp
)
set(zz_pure_tools_moc_headers
    widgets/include/ZzPureTools/ZzPageHost.h
)
```

- [ ] **Step 6: 运行页面生命周期测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzPageLifecycleTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R '^puretools\.page-lifecycle$' --repeat until-fail:20 --output-on-failure
```

Expected: PASS，缓存永远不超过 3，所有销毁序列精确匹配。

- [ ] **Step 7: 提交页面所有权模型**

```bash
git add ZzPureTools/widgets ZzPureTools/tests ZzPureTools/CMakeLists.txt
git commit -m "框架：实现页面所有权与有界缓存" \
    -m "统一 View、Presenter、ViewModel 和页面任务的销毁顺序。" \
    -m "实现 Persistent、WhileActive 和容量受限的 Recreatable 页面策略。"
```

## Task 4: 实现模型驱动导航与有界历史

**Files:**
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzNavigationNode.h`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzNavigationModel.h`
- Create: `ZzPureTools/widgets/src/ZzNavigationModel.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationModelPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationModelPrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzNavigationController.h`
- Create: `ZzPureTools/widgets/src/ZzNavigationController.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationControllerPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzNavigationControllerPrivate.cpp`
- Create: `ZzPureTools/tests/ZzNavigationControllerTest.cpp`
- Modify: `ZzPureTools/CMakeLists.txt`
- Modify: `ZzPureTools/tests/CMakeLists.txt`

- [ ] **Step 1: 写路由、后退、失败页和历史上限测试**

测试覆盖：

```cpp
void modelExposesDisplayAndRouteRoles();
void navigatesByRouteIdInsteadOfRow();
void backUsesWindowLocalHistory();
void historyIsCappedAtOneHundredEntries();
void duplicateRouteRegistrationFails();
void failedPageCreationShowsFrameworkErrorPage();
void retrySameRouteAfterFailureAttemptsFactoryAgain();
void returningFromFrameworkErrorDoesNotCreateSelfHistory();
void secondNavigationCancelsFirstTransition();
void failedNavigationKeepsHistoryConsistent();
void zeroHistoryCapacityDisablesBackNavigation();
```

向 tests CMake 追加：

```cmake
add_executable(ZzNavigationControllerTest ZzNavigationControllerTest.cpp)
target_link_libraries(ZzNavigationControllerTest PRIVATE Qt6::Test Zz::PureTools)
set_target_properties(ZzNavigationControllerTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzNavigationControllerTest)
zz_enable_sanitizers(ZzNavigationControllerTest)
add_test(NAME puretools.navigation COMMAND ZzNavigationControllerTest)
set_tests_properties(puretools.navigation PROPERTIES
    LABELS "component;puretools"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
```

- [ ] **Step 2: 运行红灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzNavigationControllerTest
```

Expected: compile FAIL，缺少导航模型与 controller。

- [ ] **Step 3: 定义只读导航节点和 model**

```cpp
struct ZzNavigationNode final
{
    ZzRouteId routeId;
    QString titleTranslationContext;
    QString titleSourceText;
    ZzFluentUI::ZzIconDescriptor icon;
};
```

title context/source 必须非空，不能把安装 translator 后的一次性翻译结果永久存入 node。`ZzNavigationModel : public QAbstractListModel` 使用四文件 PIMPL，完整 public API 为：

```cpp
enum class ZzNavigationRole : int
{
    Route = Qt::UserRole + 1,
    Icon
};

[[nodiscard]] ZzCore::ZzResult<void> setNodes(
    QList<ZzNavigationNode> nodes);
[[nodiscard]] ZzCore::ZzResult<ZzNavigationNode> nodeAt(
    qsizetype row) const;
void refreshTranslations();
```

model 同时完整 override `rowCount()`、`data()`、`roleNames()`。setNodes 在 reset 前验证 route 有效、context/source 非空且无重复；private 缓存每个 node 的已翻译 title。`refreshTranslations()` 重新调用 `QCoreApplication::translate()` 填充缓存，并只对非空模型发出 DisplayRole 的 `dataChanged()`。模型不保存页面实例或业务对象。

- [ ] **Step 4: 实现窗口级 NavigationController**

Public API：

```cpp
class ZZ_PURE_TOOLS_EXPORT ZzNavigationController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzNavigationController)

public:
    ZzNavigationController(
        ZzNavigationModel *model,
        ZzPageHost *pageHost,
        QObject *parent = nullptr);
    ~ZzNavigationController() override;

    [[nodiscard]] ZzCore::ZzResult<void> setRegistrations(
        QList<ZzPageRegistration> registrations);
    [[nodiscard]] ZzCore::ZzResult<void> navigate(ZzRouteId routeId);
    [[nodiscard]] ZzCore::ZzResult<void> goBack();
    [[nodiscard]] bool canGoBack() const noexcept;
    [[nodiscard]] ZzRouteId currentRoute() const;
    [[nodiscard]] ZzCore::ZzResult<void> setHistoryCapacity(
        qsizetype capacity);

Q_SIGNALS:
    void currentRouteChanged(const ZzRouteId &routeId);
    void navigationFailed(const ZzCore::ZzError &error);

private:
    std::unique_ptr<ZzNavigationControllerPrivate> d_ptr;
};
```

构造参数 `model/pageHost` 是必须非空的非拥有观察值，二者生命周期必须覆盖 controller；controller、model 如由 `unique_ptr` 拥有，QObject parent 必须为 null，PageHost 则只由窗口 Qt child tree 拥有。全部 API 只在 GUI 线程调用，构造和每次调用都在 Debug 验证线程；Release 下无效构造状态使 Result API 返回 `InvalidState`。

Private 默认 history capacity=100，使用 `QList<ZzRouteId>` 存 route ID，不存 row/index，并保存 `bool showingFrameworkError=false`。`setRegistrations()` 只允许调用一次；检查有效 ID、非空 factory 和重复。capacity 小于 0 返回 `InvalidArgument`，0 清空并禁用历史；缩容立即丢弃最旧项。唯一 `QParallelAnimationGroup` 以 controller 为 QObject parent，快速连续导航只 stop、重设 target 并复用它。

`navigate(route)` 的顺序固定为：验证 registration；仅当 route 等于 current route 且 `showingFrameworkError=false` 时返回成功 no-op；否则停止旧动画、记录 old route/error flag，再调用 host.activate。成功且旧状态不是 error、route 不同、old route 有效、history capacity 大于 0 时才把 old route 追加历史并裁剪上限；若旧状态是 error 且 history 尾部等于成功目标 route，则先 pop 该尾项，再清除 error flag、启动动画并 emit currentRouteChanged。factory 失败时，仅在旧状态不是 error、old route 有效且 history 启用时追加它，再调用 `host.showFrameworkError(route)` 并置 error flag；只把技术错误写日志并 emit navigationFailed/currentRouteChanged，最后把原始失败 Result 返回调用方。由此在错误页再次导航同一 route 会重试 factory，但错误 route 本身不会重复进入历史；`A -> B 创建失败 -> A` 后 history 为空且 `canGoBack()==false`，而从错误页成功进入其他 route 时仍保留 A。`goBack()` 先 peek 历史末项，内部激活成功后才 pop 并清除 error flag，且不把 error page/current route 再压回历史；失败时保留该历史项以便重试。

再次替换 `add_library(ZzPureTools ...)` 之前的 `zz_pure_tools_sources`，使用当前累计清单：

```cmake
set(zz_pure_tools_sources
    widgets/src/private/ZzPureToolsVersion.cpp
    widgets/src/ZzPageInstance.cpp
    widgets/src/private/ZzPageInstancePrivate.cpp
    widgets/src/ZzPageHost.cpp
    widgets/src/private/ZzPageHostPrivate.cpp
    widgets/src/ZzNavigationModel.cpp
    widgets/src/private/ZzNavigationModelPrivate.cpp
    widgets/src/ZzNavigationController.cpp
    widgets/src/private/ZzNavigationControllerPrivate.cpp
)
set(zz_pure_tools_moc_headers
    widgets/include/ZzPureTools/ZzPageHost.h
    widgets/include/ZzPureTools/ZzNavigationController.h
)
```

- [ ] **Step 5: 运行导航测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzNavigationControllerTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R '^puretools\.navigation$' --repeat until-fail:20 --output-on-failure
```

Expected: PASS，历史上限恒为最新 100 项，页面失败不留半初始化对象。

- [ ] **Step 6: 提交导航模型与 controller**

```bash
git add ZzPureTools/widgets ZzPureTools/tests ZzPureTools/CMakeLists.txt
git commit -m "框架：实现强类型页面导航" \
    -m "分离导航展示模型、路由注册和页面实例。" \
    -m "增加窗口级有界历史、可取消过渡和页面失败回退。"
```

## Task 5: 实现应用 Builder 和冻结后运行时（与 Task 6 同一交付）

**Files:**
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPureApplication.h`
- Create: `ZzPureTools/widgets/src/ZzPureApplication.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzApplicationBuilder.h`
- Create: `ZzPureTools/widgets/src/ZzApplicationBuilder.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationBuilderPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationBuilderPrivate.cpp`
- Create: `ZzPureTools/tests/ZzApplicationBuilderTest.cpp`
- Modify: `ZzPureTools/CMakeLists.txt`
- Modify: `ZzPureTools/tests/CMakeLists.txt`

Task 5 会调用下一 Task 才定义的窗口 factory，因此 Task 5 与 Task 6 是一个不可拆分的 red-green 交付：本 Task 写完后必须保留明确编译红灯，不得运行绿色 CTest 或提交；Task 6 补齐窗口后统一验证并提交。

- [ ] **Step 1: 写 build 冻结、启动失败与关闭顺序测试**

测试 slot：

```cpp
void buildFreezesModulesAndPages();
void buildRejectsDuplicateRoutesBeforeStartingModules();
void buildRejectsNavigationNodeWithoutPage();
void buildRequiresRegisteredInitialRoute();
void moduleStartFailureDoesNotCreateWindow();
void successfulBuildCreatesOneWindowWithoutCreatingUnvisitedPages();
void windowFailureLeavesApplicationUnbuilt();
void failedBuildCanRetryWithFreshBuilder();
void successfulApplicationRejectsSecondBuilder();
void applicationInstallsFluentStyleBeforeBuilding();
void aboutToQuitDestroysWindowsBeforeStoppingModules();
```

向 tests CMake 追加：

```cmake
add_executable(ZzApplicationBuilderTest ZzApplicationBuilderTest.cpp)
target_link_libraries(ZzApplicationBuilderTest PRIVATE
    Qt6::Test
    Zz::PureTools
    Zz::WindowKit
    Zz::FluentUI
)
set_target_properties(ZzApplicationBuilderTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzApplicationBuilderTest)
zz_enable_sanitizers(ZzApplicationBuilderTest)

set(builder_scenarios
    buildFreezesModulesAndPages
    buildRejectsDuplicateRoutesBeforeStartingModules
    buildRejectsNavigationNodeWithoutPage
    buildRequiresRegisteredInitialRoute
    moduleStartFailureDoesNotCreateWindow
    successfulBuildCreatesOneWindowWithoutCreatingUnvisitedPages
    windowFailureLeavesApplicationUnbuilt
    failedBuildCanRetryWithFreshBuilder
    successfulApplicationRejectsSecondBuilder
    applicationInstallsFluentStyleBeforeBuilding
    aboutToQuitDestroysWindowsBeforeStoppingModules
)
foreach(scenario IN LISTS builder_scenarios)
    add_test(
        NAME puretools.application-builder.${scenario}
        COMMAND ZzApplicationBuilderTest ${scenario}
    )
    set_tests_properties(puretools.application-builder.${scenario} PROPERTIES
        LABELS "component;puretools"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endforeach()
```

测试 executable 使用自定义 `main()`：先调用 `ZzWindowKitBootstrap::prepare()`，成功后构造唯一的 `ZzPureApplication`，再调用 `QTest::qExec()`；不得由 `QTEST_MAIN` 先构造第二个 QApplication。CTest 每次只传入一个 slot 名，因此每个 scenario 都在全新进程和全新 application 中运行。`failedBuildCanRetryWithFreshBuilder()` 让第一个一次性 builder 在 module start 失败，随后以全新 builder 对同一未 shutdown application 成功 build；`successfulApplicationRejectsSecondBuilder()` 在成功 commit 后用另一个 builder 得到 `InvalidState`，调用 `beginShutdown()` 后仍不得再次 build。slot 结束调用 `beginShutdown()`。

- [ ] **Step 2: 运行红灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzApplicationBuilderTest
```

Expected: compile FAIL，缺少 `ZzPureApplication.h` 和 `ZzApplicationBuilder.h`。

- [ ] **Step 3: 声明应用和 Builder 公开 API**

```cpp
class ZZ_PURE_TOOLS_EXPORT ZzPureApplication final : public QApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPureApplication)

public:
    ZzPureApplication(int &argc, char **argv);
    ~ZzPureApplication() override;
    [[nodiscard]] ZzCore::ZzResult<ZzApplicationWindow *> createWindow();
    [[nodiscard]] qsizetype windowCount() const noexcept;
    [[nodiscard]] ZzFluentUI::ZzThemeController *themeController()
        const noexcept;
    void beginShutdown() noexcept;

private:
    friend class ZzApplicationBuilderPrivate;
    std::unique_ptr<ZzPureApplicationPrivate> d_ptr;
};

class ZZ_PURE_TOOLS_EXPORT ZzApplicationBuilder final
{
public:
    ZzApplicationBuilder();
    ~ZzApplicationBuilder();
    ZzApplicationBuilder(ZzApplicationBuilder &&) noexcept;
    ZzApplicationBuilder &operator=(ZzApplicationBuilder &&) noexcept;

    [[nodiscard]] ZzCore::ZzResult<void> addModule(
        std::unique_ptr<ZzApplicationModule> module);
    [[nodiscard]] ZzCore::ZzResult<void> addPage(
        ZzPageRegistration registration);
    [[nodiscard]] ZzCore::ZzResult<void> addNavigationNode(
        ZzNavigationNode node);
    [[nodiscard]] ZzCore::ZzResult<void> setInitialRoute(
        ZzRouteId routeId);
    [[nodiscard]] ZzCore::ZzResult<void> addTranslatorResource(
        QString resourcePath);
    [[nodiscard]] ZzCore::ZzResult<void> build(
        ZzPureApplication &application);
    [[nodiscard]] bool isFrozen() const noexcept;

private:
    std::unique_ptr<ZzApplicationBuilderPrivate> d_ptr;
};
```

- [ ] **Step 4: 实现两阶段 build 和单一应用所有权**

`ZzPureApplicationPrivate` 在 application 构造时创建 parent=null 的 `unique_ptr<ZzThemeController>`，application 是其唯一 owner；窗口只观察该对象。`ZzPureApplication.cpp` 在 `d_ptr` 构造完成后紧接着以 `new ZzFluentStyle(themeController())` 调用 `setStyle()`，样式所有权按 Qt 契约交给 application。`applicationInstallsFluentStyleBeforeBuilding()` 在不调用 Builder 的情况下断言 `qobject_cast<ZzFluentStyle *>(application.style())` 非空且 `themeController()` 非空，因此所有首窗之前已安装 Fluent style。Private 的 committed 状态包含 runtime、不可变 registrations、navigation nodes、initial route、`std::vector<std::unique_ptr<QTranslator>>`、`std::vector<std::unique_ptr<ZzApplicationWindow>>` 和 built/shuttingDown/hasEverBuilt 标志。

`build()` 使用局部 staging 变量严格按以下顺序执行，任一步失败都返回 `ZzResult<void>`，且 application private 除构造期已安装的 theme controller/Fluent style 外保持未构建：

1. 立即冻结 outer builder。验证 application 未 build/未 shutdown、page route 有效且无重复、factory 非空、node route 有效且无重复、每个 node 指向已注册 page、initial route 有对应 page、translator resource 非空且存在。允许不出现在 navigation model 的深链接 page。
2. 调用 AppCore builder 生成局部 runtime；用 `auto runtimeResult` 和 `std::move(runtimeResult).value()` 提取 move-only 值，不调用复制型 API。
3. 在局部 `std::vector<std::unique_ptr<QTranslator>>` 中逐个 load translator；全部 load 成功后再逐个 install。记录已安装数量，任一 install 失败按逆序 remove 已安装项。
4. 启动局部 runtime；失败时 runtime 已按自身契约回滚，随后逆序卸载本次 translator，不创建窗口。
5. 调用 Task 6 定义的 `ZzApplicationWindow::create(registrations, nodes, initialRoute, application.themeController())` 创建局部首窗。用局部 `auto windowResult` 检查失败，再以 `std::move(windowResult).value()` 取出 `std::unique_ptr<ZzApplicationWindow>`。在局部 `stagedWindows` 上先 `reserve(1)`、push 首窗并通过 `connectWindowCloseProtocol()` 建立 Task 6 的队列化关闭连接；捕获分配异常、检查 connection 有效。此时仍不得写 committed application，任一失败都销毁 staging、runtime requestStop/stop 并逆序卸载 translator。
6. 调用 private `commitBuild(...) noexcept`，一次性 move runtime、registrations、nodes、initial route、translators 和整个 `stagedWindows` 进 application；该函数只能执行已知 noexcept 的 move/swap 和标志写入，不得 connect、reserve、push 或 show。此点是唯一把 built 设 true 的位置；返回后再 show 首窗。

`createWindow()` 仅在 built 且非 shuttingDown 时工作，使用 committed immutable 配置调用同一 window factory。`adoptWindow()` 在修改 vector 前先 `reserve(size + 1)`，再用同一个 `connectWindowCloseProtocol()` 连接关闭协议；任何分配/连接失败都返回 `Unknown` 并让局部 `unique_ptr` 销毁。容量和 connection 就绪后，push `unique_ptr` 不再分配，然后 show 并返回非拥有观察指针。所有顶层窗口 QObject parent 均为 null，并显式设置 `Qt::WA_DeleteOnClose=false`；窗口只能由 application 的 `unique_ptr` 销毁。

`beginShutdown()` 幂等：先置 shuttingDown、禁止新窗口；runtime requestStop；清空窗口 vector，从而取消页面任务并销毁展示对象；runtime stop；逆序 remove translator；清空 translator/runtime/config。生产中 aboutToQuit 只连接一次到该方法。单个 Builder 无论成功失败都在首次 `build()` 后冻结；application 只有在前次尝试尚未 commit、仍未 shutdown 时才允许一个全新 Builder 重试。`hasEverBuilt` 在首次成功 commit 后永久为 true，此后包括 shutdown 后都拒绝再次构建。`ZzPureApplication` 析构函数先调用 `beginShutdown()`，再像 FluentFoundation demo 一样用 `QStyleFactory::create(QStringLiteral("Fusion"))` 替换全局样式，确保 QApplication 在销毁 `themeController` 前先销毁仍观察它的 `ZzFluentStyle`；禁止把这个恢复步骤留给 QApplication 基类析构。

再次替换 `add_library(ZzPureTools ...)` 之前的 `zz_pure_tools_sources`，使用当前累计清单：

```cmake
set(zz_pure_tools_sources
    widgets/src/private/ZzPureToolsVersion.cpp
    widgets/src/ZzPageInstance.cpp
    widgets/src/private/ZzPageInstancePrivate.cpp
    widgets/src/ZzPageHost.cpp
    widgets/src/private/ZzPageHostPrivate.cpp
    widgets/src/ZzNavigationModel.cpp
    widgets/src/private/ZzNavigationModelPrivate.cpp
    widgets/src/ZzNavigationController.cpp
    widgets/src/private/ZzNavigationControllerPrivate.cpp
    widgets/src/ZzPureApplication.cpp
    widgets/src/private/ZzPureApplicationPrivate.cpp
    widgets/src/ZzApplicationBuilder.cpp
    widgets/src/private/ZzApplicationBuilderPrivate.cpp
)
set(zz_pure_tools_moc_headers
    widgets/include/ZzPureTools/ZzPageHost.h
    widgets/include/ZzPureTools/ZzNavigationController.h
    widgets/include/ZzPureTools/ZzPureApplication.h
)
```

公开 Doxygen 明确该 application 拥有应用级 style，其存活期内外部代码不得再调用 `QApplication::setStyle()` 替换它。

- [ ] **Step 5: 确认唯一剩余红灯是窗口 factory**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzApplicationBuilderTest
```

Expected: compile FAIL，错误只指向尚未创建的 `ZzApplicationWindow.h`/`ZzApplicationWindow::create`。若更早因 Builder/Application API、CMake source 或测试 main 失败，先修正到这个单一红灯。

- [ ] **Step 6: 不提交并直接继续 Task 6**

此时工作树有意保持未提交；Task 6 的 factory、close ownership 和窗口组合完成后，才允许执行完整绿色验证和一次原子提交。

## Task 6: 完成 WindowKit、FluentTitleBar 与应用运行时交付

**Files:**
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzApplicationWindow.h`
- Create: `ZzPureTools/widgets/src/ZzApplicationWindow.cpp`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.h`
- Create: `ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp`
- Create: `ZzPureTools/tests/ZzMultiWindowIsolationTest.cpp`
- Create: `ZzPureTools/tests/ZzTranslationLifecycleTest.cpp`
- Create: `ZzPureTools/tests/ZzTranslationLifecycleTest_zh_CN.ts`
- Modify: `ZzPureTools/CMakeLists.txt`
- Modify: `ZzPureTools/tests/CMakeLists.txt`

- [ ] **Step 1: 写窗口组合和隔离失败测试**

测试覆盖：

```cpp
void everyWindowOwnsDifferentWindowAgent();
void everyWindowOwnsDifferentNavigationState();
void titleBarIntentInvokesOnlyOwningWindow();
void chromeConfigurationUsesTitleBarChildren();
void nativeSystemButtonCapabilityControlsCustomButtons();
void navigationIntentUsesTheOwningModelRoute();
void topLevelWindowDisablesDeleteOnClose();
void acceptedCloseQueuesExactlyOneApplicationErase();
void metaObjectSignalWithoutAcceptedCloseCannotEraseWindow();
void languageChangeRefreshesStaticAndDynamicText();
void failedTranslatorLoadLeavesApplicationUnbuilt();
void runtimeFailureRollsBackInstalledTranslators();
```

前九项放入 `ZzMultiWindowIsolationTest.cpp`，后三项放入 `ZzTranslationLifecycleTest.cpp`。多窗口测试通过 Task 5 的 Builder 创建首窗，再调用一次 `application.createWindow()`；它不得直接访问 private factory。`ZzTranslationLifecycleTest_zh_CN.ts` 为 context `ZzTranslationLifecycleTest`、source `Owned marker` 提供 translation `Owned translated`；测试 CMake 用 Qt LinguistTools 的 `qt_add_lrelease()` 生成 `.qm`，并以私有编译定义把规范化生成路径传给测试，不提交工具生成的二进制。向 `ZzPureTools/tests/CMakeLists.txt` 追加完整 target：

```cmake
add_executable(ZzMultiWindowIsolationTest ZzMultiWindowIsolationTest.cpp)
target_link_libraries(ZzMultiWindowIsolationTest PRIVATE
    Qt6::Test
    Zz::PureTools
    Zz::WindowKit
    Zz::FluentUI
)
set_target_properties(ZzMultiWindowIsolationTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzMultiWindowIsolationTest)
zz_enable_sanitizers(ZzMultiWindowIsolationTest)
set(multi_window_scenarios
    everyWindowOwnsDifferentWindowAgent
    everyWindowOwnsDifferentNavigationState
    titleBarIntentInvokesOnlyOwningWindow
    chromeConfigurationUsesTitleBarChildren
    nativeSystemButtonCapabilityControlsCustomButtons
    navigationIntentUsesTheOwningModelRoute
    topLevelWindowDisablesDeleteOnClose
    acceptedCloseQueuesExactlyOneApplicationErase
    metaObjectSignalWithoutAcceptedCloseCannotEraseWindow
)
foreach(scenario IN LISTS multi_window_scenarios)
    add_test(
        NAME puretools.multi-window.${scenario}
        COMMAND ZzMultiWindowIsolationTest ${scenario}
    )
    set_tests_properties(puretools.multi-window.${scenario} PROPERTIES
        LABELS "component;puretools"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endforeach()

add_executable(ZzTranslationLifecycleTest ZzTranslationLifecycleTest.cpp)
find_package(Qt6 6.8 REQUIRED COMPONENTS LinguistTools)
qt_add_lrelease(ZzTranslationLifecycleTest
    TS_FILES ZzTranslationLifecycleTest_zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE zz_translation_test_qm_files)
list(LENGTH zz_translation_test_qm_files zz_translation_test_qm_count)
if(NOT zz_translation_test_qm_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one translation test QM file")
endif()
list(GET zz_translation_test_qm_files 0 zz_translation_test_qm)
file(TO_CMAKE_PATH "${zz_translation_test_qm}" zz_translation_test_qm)
target_compile_definitions(ZzTranslationLifecycleTest PRIVATE
    "ZZ_TRANSLATION_TEST_QM=\"${zz_translation_test_qm}\"")
target_link_libraries(ZzTranslationLifecycleTest PRIVATE
    Qt6::Test
    Zz::PureTools
    Zz::WindowKit
)
set_target_properties(ZzTranslationLifecycleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzTranslationLifecycleTest)
zz_enable_sanitizers(ZzTranslationLifecycleTest)
set(translation_scenarios
    languageChangeRefreshesStaticAndDynamicText
    failedTranslatorLoadLeavesApplicationUnbuilt
    runtimeFailureRollsBackInstalledTranslators
)
foreach(scenario IN LISTS translation_scenarios)
    add_test(
        NAME puretools.translation.${scenario}
        COMMAND ZzTranslationLifecycleTest ${scenario}
    )
    set_tests_properties(puretools.translation.${scenario} PROPERTIES
        LABELS "component;puretools"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endforeach()
```

两个测试 executable 都使用与 Builder 测试相同的自定义 `main()`：bootstrap 成功后构造唯一 `ZzPureApplication`，CTest 每次只传一个 slot 名。每个 scenario 因此处于新进程，不会在同一 application 上二次 build；禁止使用 `QTEST_MAIN`。

窗口测试用 `window->findChild<ZzFluentUI::ZzFluentTitleBar *>()` 找到每窗唯一 titlebar，再通过其 public button getters 发送 click；不得给生产窗口增加只为测试使用的 public getter。`chromeConfigurationUsesTitleBarChildren()` 同时断言 agent state 为 Configured，且 titlebar/icon/buttons/interactive widgets 都属于当前窗口的 QObject child tree。capability 测试读取 `windowAgent()->capabilities()`：含 `NativeSystemButtons` 时 custom buttons 全隐藏，否则保持可见；它验证当前后端报告与 UI 决策一致，不伪造平台能力。

`acceptedCloseQueuesExactlyOneApplicationErase()` 保存 `QPointer<ZzApplicationWindow>`，调用 `close()` 后先断言窗口没有被同步删除，再用 `QTRY_COMPARE(application.windowCount(), 1)` 验证 queued erase；重复处理事件后计数仍为 1 且 `QPointer` 为空。`metaObjectSignalWithoutAcceptedCloseCannotEraseWindow()` 在未调用 `close()` 时以 `QMetaObject::invokeMethod(window, "closeAccepted", Qt::DirectConnection)` 模拟外部 metaobject 调用，处理队列后窗口数量和指针必须不变，随后真实 accepted close 仍只删除一次。这样同时锁定 `unique_ptr` owner、close event 时序和伪 signal 防护。

- [ ] **Step 2: 运行红灯测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzMultiWindowIsolationTest ZzTranslationLifecycleTest
```

Expected: compile FAIL，缺少 `ZzApplicationWindow.h`。

- [ ] **Step 3: 声明应用窗口公开 API**

```cpp
class ZZ_PURE_TOOLS_EXPORT ZzApplicationWindow final : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzApplicationWindow)

public:
    ~ZzApplicationWindow() override;
    [[nodiscard]] ZzNavigationController *navigationController() const noexcept;
    [[nodiscard]] ZzNavigationModel *navigationModel() const noexcept;
    [[nodiscard]] ZzPageHost *pageHost() const noexcept;
    [[nodiscard]] ZzWindowKit::ZzWindowAgent *windowAgent() const noexcept;

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Q_SIGNAL void closeAccepted();
    friend class ZzApplicationBuilderPrivate;
    friend class ZzPureApplicationPrivate;
    ZzApplicationWindow();
    [[nodiscard]] static ZzCore::ZzResult<
        std::unique_ptr<ZzApplicationWindow>> create(
        const QList<ZzPageRegistration> &registrations,
        const QList<ZzNavigationNode> &nodes,
        ZzRouteId initialRoute,
        ZzFluentUI::ZzThemeController *themeController);
    [[nodiscard]] ZzCore::ZzResult<void> initialize(
        const QList<ZzPageRegistration> &registrations,
        const QList<ZzNavigationNode> &nodes,
        ZzRouteId initialRoute,
        ZzFluentUI::ZzThemeController *themeController);
    [[nodiscard]] bool consumeAcceptedClose() noexcept;
    std::unique_ptr<ZzApplicationWindowPrivate> d_ptr;
};
```

`create()` 是两个 application 装配 private friend 可调用的唯一构造入口，签名必须与 Task 5 的两处调用完全一致。它用 private 无参构造创建局部 `unique_ptr`，调用 `initialize()`，只在成功后返回所有权；禁止再保留一份接收 registrations/theme 的构造函数。`closeAccepted` 是 private Qt signal，不属于公开 C++ API，外部代码不得调用；Qt metaobject 仍可按名称调用它，因此 application 的 friend 回调还必须通过 private `consumeAcceptedClose()` 原子式读取并清除真实 closeEvent 设置的 pending 标志。`initialize()` 和全部观察 getter 只允许 GUI 线程调用，`themeController` 必须非空且其生命周期覆盖所有窗口。返回指针均为非拥有观察值，有效期不超过窗口。公开类、方法、参数、Result 错误和线程前提补齐简体中文 Doxygen。

- [ ] **Step 4: 在 PureTools 层完成唯一窗口装配**

Private 每窗口独立创建：

```text
unique_ptr<ZzWindowAgent>
ZzFluentTitleBar Qt child
ZzNavigationView Qt child
unique_ptr<ZzNavigationModel>
unique_ptr<ZzNavigationController>
ZzPageHost Qt child
QList<ZzRouteId> history（由 controller private 拥有）
QParallelAnimationGroup（由 controller private 拥有）
```

`ZzApplicationWindow` 构造时立即 `setAttribute(Qt::WA_DeleteOnClose, false)`，且 QObject parent 保持 null。`initialize()` 的顺序固定为：

1. 验证 theme、registrations、nodes 和 initial route；构造 central layout、titlebar、navigation view、page host，以及 parent=null 的 model/controller/agent。
2. `model.setNodes(nodes)`、`controller.setRegistrations(registrations)`，并让 navigation view 观察 model。`navigationRequested(index)` 的 lambda 先保存 `auto nodeResult = model.nodeAt(index.row())`；失败时记录 `nodeResult.error()` 并立即返回。成功时以 `std::move(nodeResult).value().routeId` 提取强类型 route，再调用当前窗口的 controller；导航失败同样只写日志，技术错误不进入 UI 文本。禁止直接对 `ZzResult<ZzNavigationNode>` 访问 `.routeId`。
3. 调用 `windowAgent.attach(this)` 后读取稳定 capability 快照，再从 titlebar 子控件构造完整 `ZzWindowChromeConfiguration`。若包含 `NativeSystemButtons`，隐藏 Fluent minimize/maximize/close 控件，并明确把 configuration 的 `minimizeButton`、`maximizeButton`、`closeButton` 全部设为 `nullptr`，同时不把这三个控件放入 `interactiveWidgets`；否则显示并登记三个自定义按钮。最后只调用一次 `configureChrome()`。
4. 连接 minimize/maximize/close 意图到当前窗口；maximize/restore 每次读取当前 `isMaximized()` 决定动作，并在 `QEvent::WindowStateChange` 时调用 `titleBar->setMaximized(isMaximized())`，覆盖窗口管理器触发的状态变化。最后调用 `controller.navigate(initialRoute)`；首次导航只创建 initial page，不创建其他注册页。

任一步失败都原样返回 `ZzError`；`create()` 销毁整个局部窗口，不 show、不发 `closeAccepted`、不留下 QObject child。Windows/Linux 显示并向 WindowKit 登记 Fluent 系统按钮；`NativeSystemButtons` capability 存在时隐藏自定义系统按钮并传入三个空系统按钮指针，仍保留标题和其他可交互区域。禁止只隐藏控件却继续向 QWK 登记它们，否则 macOS 原生按钮与不可见自定义 hit target 会同时存在。`ZzFluentTitleBar` 与 `ZzNavigationView` 不 include WindowKit，WindowKit 不 include FluentUI，只有 `ZzApplicationWindowPrivate.cpp` 同时 include WindowKit 与 FluentUI 公开头。

- [ ] **Step 5: 固定 close event 与 application 所有权协议**

`closeEvent()` 先调用 `QMainWindow::closeEvent(event)`；仅当 event 最终 accepted 且 private pending 标志尚未置位时，置位并发出一次 `closeAccepted()`。它不得 `delete this`，也不得直接修改 application 的 vector。`consumeAcceptedClose()` 仅在 pending 为 true 时清零并返回 true，否则返回 false；全部调用都在 GUI 线程，不需要原子变量或锁。

Task 5 的 `adoptWindow()` 以 `ZzPureApplication` 为 connection context 连接该信号，connection 固定为 `Qt::QueuedConnection`。lambda 捕获 `QPointer<ZzApplicationWindow>`；执行时若 application 正在 shutdown、pointer 已空或 `consumeAcceptedClose()` 返回 false 则返回，否则按地址在 `std::vector<std::unique_ptr<ZzApplicationWindow>>` 中找到并 erase 一次。这样删除发生在 `closeEvent()` 栈退出后，单独伪造 metaobject signal 也不能删除活动窗口。`beginShutdown()` 可以直接清空 vector；对象销毁会自动移除尚未投递的 sender connection，queued lambda 仍以空 `QPointer` 安全返回。

- [ ] **Step 6: 实现语言变更边界**

`changeEvent()` 对 `QEvent::LanguageChange` 刷新窗口标题、titlebar 静态文本并调用 `navigationModel()->refreshTranslations()`；对 `QEvent::WindowStateChange` 只同步 titlebar 的 maximize/restore 状态，其余事件交给基类。页面动态文本由 ViewModel 观察应用语言服务或 translator 变更通知并自行更新，窗口不得反向读取业务对象。

`ZzTranslationLifecycleTest` 使用测试 `QTranslator` 安装成功后验证两个窗口和 model 都刷新。测试页的 ViewModel 以非拥有观察方式安装为 QApplication 全局 event filter，收到任一页面的 `LanguageChange` 后重新翻译动态文本；它在 PageInstance 取消/析构路径移除 filter，不依赖窗口反向读取业务对象，也不为测试增加公开语言 API。加载不存在的 Builder resource 时，验证 build 返回失败、application 仍未 built、没有窗口，且此前已安装的外部 translator 与可观察文本保持不变。

`runtimeFailureRollsBackInstalledTranslators()` 先安装一个只翻译独立 `External marker` 的外部 tracking translator，再让 Builder 加载生成的有效 `.qm`。测试模块的 `start()` 必须先断言 `QCoreApplication::translate("ZzTranslationLifecycleTest", "Owned marker") == "Owned translated"`，由此证明 Builder translator 已成功 load/install，然后返回预设 `Backend` 失败。build 返回后断言 owned marker 恢复为源文本、external marker 仍由原 translator 翻译、application 未 built、windowCount 为 0，且外部 translator 没有被 remove；最后只由测试自身移除外部 translator。该场景锁定 runtime/window 后续失败时对本次 translator 的逆序卸载，语言资源仍只走 Task 5 的 staging/commit，不增加半提交的运行期切换 API。

- [ ] **Step 7: 将窗口实现加入目标并运行测试**

把 `add_library(ZzPureTools ...)` 之前的 `zz_pure_tools_sources` 替换为最终累计清单：

```cmake
set(zz_pure_tools_sources
    widgets/src/private/ZzPureToolsVersion.cpp
    widgets/src/ZzPageInstance.cpp
    widgets/src/private/ZzPageInstancePrivate.cpp
    widgets/src/ZzPageHost.cpp
    widgets/src/private/ZzPageHostPrivate.cpp
    widgets/src/ZzNavigationModel.cpp
    widgets/src/private/ZzNavigationModelPrivate.cpp
    widgets/src/ZzNavigationController.cpp
    widgets/src/private/ZzNavigationControllerPrivate.cpp
    widgets/src/ZzPureApplication.cpp
    widgets/src/private/ZzPureApplicationPrivate.cpp
    widgets/src/ZzApplicationBuilder.cpp
    widgets/src/private/ZzApplicationBuilderPrivate.cpp
    widgets/src/ZzApplicationWindow.cpp
    widgets/src/private/ZzApplicationWindowPrivate.cpp
)
set(zz_pure_tools_moc_headers
    widgets/include/ZzPureTools/ZzPageHost.h
    widgets/include/ZzPureTools/ZzNavigationController.h
    widgets/include/ZzPureTools/ZzPureApplication.h
    widgets/include/ZzPureTools/ZzApplicationWindow.h
)
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzMultiWindowIsolationTest ZzTranslationLifecycleTest
cmake --build --preset linux-gcc-debug --target ZzApplicationBuilderTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-debug -R '^puretools\.(application-builder|multi-window|translation)\.' --repeat until-fail:20 --output-on-failure
cmake --build --preset linux-clang-asan --target ZzMultiWindowIsolationTest
QT_QPA_PLATFORM=offscreen ctest --preset linux-clang-asan -R '^puretools\.multi-window\.' --output-on-failure
```

Expected: Builder 的十一个独立进程 scenario、multi-window 和 translation 全部 PASS；失败 build 不创建窗口且可由全新 Builder 重试，成功 build 后注册 API 和 application 二次 build 返回 `InvalidState`；所有窗口创建前已安装 Fluent style，两窗口的 agent、controller、model、host 地址全部不同；ASan 无重复所有权。

- [ ] **Step 8: 原子提交应用运行时和窗口组合**

```bash
git add ZzPureTools/widgets ZzPureTools/tests ZzPureTools/CMakeLists.txt
git commit -m "框架：实现应用装配与 Fluent 多窗口" \
    -m "以两阶段 Builder 冻结模块、页面和翻译注册。" \
    -m "组合 WindowKit 与 FluentTitleBar，并隔离每个窗口的导航和页面状态。"
```

## Task 7: 创建无业务逻辑的完整示例

**Files:**
- Create: `examples/ZzPureToolsDemo/CMakeLists.txt`
- Create: `examples/ZzPureToolsDemo/main.cpp`
- Create: `examples/ZzPureToolsDemo/ZzDemoModule.h`
- Create: `examples/ZzPureToolsDemo/ZzDemoModule.cpp`
- Create: `examples/ZzPureToolsDemo/ZzDemoPageFactory.h`
- Create: `examples/ZzPureToolsDemo/ZzDemoPageFactory.cpp`
- Modify: `examples/CMakeLists.txt`

- [ ] **Step 1: 配置示例并确认 target 尚不存在**

Run:

```bash
cmake --preset linux-gcc-release -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-release --target ZzPureToolsDemo
```

Expected: FAIL，明确报告 `ZzPureToolsDemo` target 不存在；不得以关闭 `ZZ_BUILD_EXAMPLES` 得到的伪成功替代红灯。

- [ ] **Step 2: 写示例模块、两页 factory 与构建 target**

`ZzDemoModule` 只记录 start/requestStop/stop，不 include QWidget。`ZzDemoPageFactory` 声明 `createHome(QWidget *pageParent)` 和 `createDetails(QWidget *pageParent)`；两个函数都创建一个以 `pageParent` 为 Qt parent 的 QWidget、一个 parent=null 的 QObject ViewModel 和 Presenter，并且必须调用：

```cpp
return ZzPureTools::ZzPageInstance::create(
    pageParent, view, std::move(viewModel), std::move(presenter));
```

factory 局部对象在任一构造步骤失败时按 Task 3 契约释放。两个页面分别显示可翻译的 “Home” 和 “Details” 静态文本及只读展示状态，不读文件、数据库、网络或领域服务。

Create `examples/ZzPureToolsDemo/CMakeLists.txt` with:

```cmake
add_executable(ZzPureToolsDemo
    main.cpp
    ZzDemoModule.cpp
    ZzDemoPageFactory.cpp
)
target_link_libraries(ZzPureToolsDemo PRIVATE
    Zz::PureTools
    Zz::WindowKit
)
zz_enable_project_warnings(ZzPureToolsDemo)
zz_enable_sanitizers(ZzPureToolsDemo)
```

- [ ] **Step 3: 写两页、两节点和双窗口 composition root**

Create `examples/ZzPureToolsDemo/main.cpp` with:

```cpp
#include <cstdlib>
#include <memory>
#include <utility>

#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzPageRegistration.h>
#include <ZzPureTools/ZzPureApplication.h>
#include <ZzWindowKit/ZzWindowKitBootstrap.h>

#include "ZzDemoModule.h"
#include "ZzDemoPageFactory.h"

int main(int argc, char *argv[])
{
    const auto bootstrap = ZzWindowKit::ZzWindowKitBootstrap::prepare();
    if (!bootstrap) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzPureApplication application(argc, argv);
    ZzPureTools::ZzApplicationBuilder builder;

    auto moduleResult = builder.addModule(
        std::make_unique<ZzDemoModule>());
    if (!moduleResult) {
        return EXIT_FAILURE;
    }

    const ZzPureTools::ZzRouteId homeRoute(
        QStringLiteral("home"));
    const ZzPureTools::ZzRouteId detailsRoute(
        QStringLiteral("details"));

    ZzPureTools::ZzPageRegistration home;
    home.routeId = homeRoute;
    home.lifetime = ZzPureTools::ZzPageLifetimePolicy::Persistent;
    home.factory = &ZzDemoPageFactory::createHome;
    if (!builder.addPage(std::move(home))) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzPageRegistration details;
    details.routeId = detailsRoute;
    details.lifetime = ZzPureTools::ZzPageLifetimePolicy::Recreatable;
    details.factory = &ZzDemoPageFactory::createDetails;
    if (!builder.addPage(std::move(details))) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzNavigationNode homeNode{
        homeRoute,
        QStringLiteral("ZzPureToolsDemo"),
        QStringLiteral("Home"),
        {}};
    if (!builder.addNavigationNode(std::move(homeNode))) {
        return EXIT_FAILURE;
    }

    ZzPureTools::ZzNavigationNode detailsNode{
        detailsRoute,
        QStringLiteral("ZzPureToolsDemo"),
        QStringLiteral("Details"),
        {}};
    if (!builder.addNavigationNode(std::move(detailsNode))) {
        return EXIT_FAILURE;
    }

    if (!builder.setInitialRoute(homeRoute)) {
        return EXIT_FAILURE;
    }

    if (!builder.build(application)) {
        return EXIT_FAILURE;
    }

    auto secondWindowResult = application.createWindow();
    if (!secondWindowResult) {
        application.beginShutdown();
        return EXIT_FAILURE;
    }

    return application.exec();
}
```

首窗由 `build()` 显示，`createWindow()` 成功后显示第二窗口；返回的 raw pointer 只是观察值，不由 demo 删除。两个窗口使用同一不可变 registrations/nodes，但导航、页面实例、历史和 WindowAgent 必须互相独立。

- [ ] **Step 4: 接入 examples 聚合并运行绿色构建**

根 `CMakeLists.txt` 已由基线计划在 components/tests 之后通过 `ZZ_BUILD_EXAMPLES` 条件加入整个 `examples/` 目录，不得重复添加第二个 `add_subdirectory(examples)`。只向 `examples/CMakeLists.txt` 追加：

```cmake
add_subdirectory(ZzPureToolsDemo)
```

Run:

```bash
cmake --preset linux-gcc-release -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-release --target ZzPureToolsDemo
```

Expected: PASS；target 通过 `ZZ_BUILD_EXAMPLES` 从根工程可达，关闭该选项时不配置 demo。

- [ ] **Step 5: 在 Linux 真实显示会话运行**

Run:

```bash
./build/linux-gcc-release/examples/ZzPureToolsDemo/ZzPureToolsDemo
```

Expected: 两个窗口都显示 Fluent 主题，且可分别导航 Home/Details；首次访问 Details 时才创建对应页面。分别验证拖动、缩放、系统按钮和关闭一个窗口不影响另一个窗口；关闭最后窗口后无残留进程。报告记录 Qt 版本、`platformName()`、WM/compositor 和实际功能覆盖。

- [ ] **Step 6: 提交首个可用示例**

```bash
git add examples
git commit -m "示例：增加 PureTools 完整应用流程" \
    -m "演示窗口提前准备、显式模块依赖、页面注册与 Builder 冻结。" \
    -m "示例不包含业务服务、存储或网络访问。"
```

## Task 8: 锁定分层、安装消费和最终质量门禁

**Files:**
- Create: `tests/Architecture/CheckZzPureToolsBoundaries.cmake`
- Create: `tests/Architecture/CheckZzPureToolsBoundariesContract.cmake`
- Create: `tests/Architecture/fixtures/zzpuretools-good/appcore/ZzGoodAppCore.h`
- Create: `tests/Architecture/fixtures/zzpuretools-good/widgets/ZzGoodWidget.h`
- Create: `tests/Architecture/fixtures/zzpuretools-bad/appcore/ZzBadAppCore.h`
- Create: `tests/Architecture/fixtures/zzpuretools-bad/widgets/ZzBadWidget.h`
- Create: `tests/Architecture/fixtures/zzpuretools-bad/widgets/ZzAllowedComposition.cpp`
- Modify: `tests/Architecture/CMakeLists.txt`
- Modify: `tests/InstallConsumer/main.cpp`
- Modify: `tests/InstallConsumer/CMakeLists.txt`
- Modify: `ZzPureTools/CMakeLists.txt`

- [ ] **Step 1: 先写边界扫描器正负契约并确认红灯**

good fixture 只 include QtCore，使用 `namespace ZzPureTools { namespace Internal { ... } }`，其公开类型和方法带简体中文 `/** ... */` Doxygen。bad AppCore fixture include `<QtGui/QGuiApplication>` 并使用任意名称的 `namespace Sample::Bad`；bad Widgets fixture 同时包含业务 repository、Qt private 或 QWK token、WindowKit 与 FluentUI include，并留一个只有英文注释的公开方法。另外创建只含 `static_assert(true);` 的 bad `ZzAllowedComposition.cpp`；contract 对 bad roots 把该文件作为 allowed path、设 `ZZ_REQUIRE_COMPOSITION=OFF`，因此同时 include WindowKit/FluentUI 的 `ZzBadWidget.h` 必定触发 `COMPOSITION_UNIQUENESS`，而不会被误当成允许文件。good roots 以已存在的 `ZzGoodWidget.h` 作为 allowed path，同样设 `ZZ_REQUIRE_COMPOSITION=OFF`。

`CheckZzPureToolsBoundariesContract.cmake` 用 `execute_process()` 调用同一 `CheckZzPureToolsBoundaries.cmake`：good roots 必须返回 0，bad roots 必须非 0。bad stderr 必须同时包含文件路径以及以下稳定 rule id：

```text
APP_CORE_UI_DEPENDENCY
PRESENTATION_BUSINESS_DEPENDENCY
QT_PRIVATE_OR_QWK
CHAINED_NAMESPACE
COMPOSITION_UNIQUENESS
PUBLIC_API_DOXYGEN
```

扫描器尚未创建时先运行：

```bash
cmake -DZZ_SOURCE_DIR=$PWD \
    -P tests/Architecture/CheckZzPureToolsBoundariesContract.cmake
```

Expected: FAIL，明确报告缺少 `CheckZzPureToolsBoundaries.cmake`；不得因 fixture 路径或 CMake 语法错误提前失败。

- [ ] **Step 2: 实现可复用的 AppCore/Widgets 边界扫描**

`CheckZzPureToolsBoundaries.cmake` 接收必填的 `ZZ_APPCORE_ROOT`、`ZZ_WIDGETS_ROOT`、`ZZ_APPCORE_PUBLIC_ROOT`、`ZZ_WIDGETS_PUBLIC_ROOT` 和规范化绝对路径 `ZZ_ALLOWED_COMPOSITION_FILE`，以及布尔值 `ZZ_REQUIRE_COMPOSITION`。脚本递归扫描传入 roots，而不是硬编码真实源码树；contract 与真实测试因此走完全相同的逻辑。

脚本同时保留 raw source 和移除块/行注释后的 source。它逐行累计全部违规，最后一次 `message(FATAL_ERROR ...)`，每条格式固定为 `RULE_ID:path:line`：

- `APP_CORE_UI_DEPENDENCY`：AppCore source 的 include/type token 出现 Qt Gui、Widgets、Quick、QWidget/QWindow、WindowKit、FluentUI 或 QWK；target link 由 Step 3 的 configure-time guard 独立检查。
- `PRESENTATION_BUSINESS_DEPENDENCY`：Widgets include path 出现不区分大小写的 Repository、Database、NetworkClient 或 DomainEntity。
- `QT_PRIVATE_OR_QWK`：一方 Widgets 源码 include Qt private、`*_p.h` 或出现 QWK token。
- `CHAINED_NAMESPACE`：任意 AppCore/Widgets 源码匹配 `namespace[空白]+标识符[空白]*::`，不限定 namespace 名称。
- `COMPOSITION_UNIQUENESS`：同时直接 include `ZzWindowKit/` 与 `ZzFluentUI/` 的文件必须恰好是 `ZZ_ALLOWED_COMPOSITION_FILE`；真实源码在 `ZZ_REQUIRE_COMPOSITION=ON` 时还要求该文件恰好出现一次。
- `PUBLIC_API_DOXYGEN`：两个 public roots 下每个公开 class/struct/enum、`public:` 方法和 `Q_SIGNALS` 声明，除显然的 defaulted/deleted special member 与无新增契约的 override 外，紧邻的 `/** ... */` 必须含 `@brief` 和至少一个 CJK 字符。解析器把跨行声明累积到 `;`，不能只检查每个 header 是否有任意一段注释。

comment 中的禁用 token 不触发依赖规则；Doxygen 检查则只读取 raw source 中紧邻声明的 comment。文件不存在、root 为空或 allowed composition path 不在 Widgets root 都是独立配置错误。

- [ ] **Step 3: 锁定 target link scope 并复用逐头编译 helper**

把 baseline 中 `ZzPureTools` 的 link scope 收窄为：

```cmake
target_link_libraries(ZzPureTools
    PUBLIC
        Zz::AppCore
        Zz::FluentFoundation
        Qt6::Widgets
    PRIVATE
        Zz::WindowKit
        Zz::FluentUI
)
```

public header 只 include Foundation 的 `ZzIconDescriptor`/`ZzThemeController`，并 forward declare `ZzWindowAgent`。`ZzPureApplication.cpp` 为安装应用级样式可以只 direct include `ZzFluentStyle.h`；只有 private window composition 同时 direct include Widgets 层的 FluentUI 与 WindowKit。紧跟两个 `target_link_libraries` 增加 configure-time guard：

1. 同时读取 `ZzAppCore` 的 `LINK_LIBRARIES` 和 `INTERFACE_LINK_LIBRARIES`，拒绝 Qt Gui/Widgets/Quick、ZzWindowKit 和 ZzFluentUI，保证 PRIVATE 误链也会失败。
2. 要求 `ZzPureTools` 的 `LINK_LIBRARIES` 含 WindowKit/FluentUI，且 `INTERFACE_LINK_LIBRARIES` 不含裸的 `Zz::WindowKit`/`Zz::FluentUI`。静态库为闭合最终链接而生成的 `$<LINK_ONLY:...>` 可以保留，但不得进入 include/compile interface；`Zz::FluentFoundation` 和 `Qt6::Widgets` 必须是 public。

基线计划已经在 `tests/Architecture/CMakeLists.txt` 分别对 `ZzPureTools/appcore/include` 和 `ZzPureTools/widgets/include` 调用 `zz_add_public_header_directory()`，并创建唯一的 aggregate `ZzPublicHeadersTest`；不得创建第二套 helper、重复 aggregate 或手写另一份头文件清单。重新配置时 `CONFIGURE_DEPENDS` 必须自动发现本计划新增的全部 AppCore/PureTools 公共头，每个 header 仍生成独立 translation unit；基线的显式 probe 继续覆盖生成的 `ZzAppCoreExport.h` 与 `ZzPureToolsExport.h`。若 aggregate 不存在，视为第 1 份计划未完成并立即停止，不得在本计划补建替代 target。

- [ ] **Step 4: 注册 contract 与真实源码扫描**

向 `tests/Architecture/CMakeLists.txt` 追加：

```cmake
add_test(
    NAME architecture.puretools-boundaries-contract
    COMMAND ${CMAKE_COMMAND}
        -DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckZzPureToolsBoundariesContract.cmake
)
set_tests_properties(architecture.puretools-boundaries-contract PROPERTIES
    LABELS "architecture;puretools"
)

add_test(
    NAME architecture.puretools-boundaries
    COMMAND ${CMAKE_COMMAND}
        -DZZ_APPCORE_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/appcore
        -DZZ_WIDGETS_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/widgets
        -DZZ_APPCORE_PUBLIC_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/appcore/include
        -DZZ_WIDGETS_PUBLIC_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/include
        -DZZ_ALLOWED_COMPOSITION_FILE=${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp
        -DZZ_REQUIRE_COMPOSITION=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckZzPureToolsBoundaries.cmake
)
set_tests_properties(architecture.puretools-boundaries PROPERTIES
    LABELS "architecture;appcore;puretools"
)
```

先重新运行 Step 1 命令。Expected: good PASS、bad 被拒绝，且六个 rule id 全部出现。

- [ ] **Step 5: 扩展安装消费测试**

完整替换 `tests/InstallConsumer/main.cpp`，不要在上一阶段的 Fluent GUI consumer 上增量增加 include。最终文件为无 GUI 的应用框架安装消费检查：

```cpp
#include <ZzPureTools/ZzApplicationBuilder.h>
#include <ZzPureTools/ZzModuleDescriptor.h>
#include <ZzPureTools/ZzModuleGraphBuilder.h>
#include <ZzPureTools/ZzModuleId.h>
#include <ZzPureTools/ZzNavigationNode.h>
#include <ZzPureTools/ZzRouteId.h>

int main()
{
    ZzPureTools::ZzModuleId moduleId(QStringLiteral("install.consumer"));
    ZzPureTools::ZzRouteId routeId(QStringLiteral("home"));
    ZzPureTools::ZzModuleDescriptor descriptor{
        moduleId,
        QStringLiteral("1.0.0"),
        {}};
    ZzPureTools::ZzNavigationNode navigationNode{
        routeId,
        QStringLiteral("ZzInstallConsumer"),
        QStringLiteral("Home"),
        {}};
    ZzPureTools::ZzModuleGraphBuilder moduleGraphBuilder;
    ZzPureTools::ZzApplicationBuilder applicationBuilder;

    if (!moduleId.isValid() || !routeId.isValid()
        || descriptor.id != moduleId
        || navigationNode.routeId != routeId
        || moduleGraphBuilder.isFrozen()
        || applicationBuilder.isFrozen()) {
        return 1;
    }
    return 0;
}
```

消费者只构造 ID、descriptor、navigation node 和两个 builder，不启动 GUI，不构造 `QApplication`、`QWidget`、style 或窗口。把 `tests/InstallConsumer/CMakeLists.txt` 中上阶段的 link block 精确替换为：

```cmake
target_link_libraries(ZzInstallConsumer PRIVATE
    Zz::AppCore
    Zz::PureTools
)
```

只通过 `find_package(ZzPureToolsFrame)` 获取 target，不增加源码树或 build tree include path；`Zz::PureTools` 的安装 interface 必须传递其公开 `ZzNavigationNode` 所需的 FluentFoundation 和 Qt 头依赖。WindowKit 与 FluentUI Widgets 实现依赖只允许以 static 闭合所需的 link-only 形式出现，不得向 consumer 暴露 private include path。

- [ ] **Step 6: 运行边界、AppCore link 和 public-header 检查**

Run:

```bash
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug \
    -R '^architecture.puretools-boundaries(-contract)?$' \
    --output-on-failure
ctest --preset linux-gcc-debug -L 'appcore|puretools|architecture' \
    --output-on-failure
cmake --build --preset linux-gcc-debug --target ZzPublicHeadersTest
```

Expected: PASS；contract 确实拒绝 bad fixtures；AppCore 的 direct/private/interface link 都不含 UI target；每个 AppCore/PureTools 安装头以 C++20 独立编译。`ZzApplicationWindowPrivate.cpp` 是唯一同时直接 include WindowKit 与 FluentUI 的文件。

- [ ] **Step 7: 运行 shared/static 安装消费**

Run:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release -R '^install\.consumer$' --output-on-failure
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release -R '^install\.consumer$' --output-on-failure
```

Expected: 两种模式 PASS；consumer 只通过 `find_package(ZzPureToolsFrame)` 获取导出 target。

- [ ] **Step 8: 执行最终一致性检查**

Run:

```bash
rg -n 'namespace[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*::|Qt.*/private|QWK' ZzPureTools
rg -ni 'repository|database|network[ _-]*client|domain[ _-]*entity' ZzPureTools/widgets --pcre2
rg -n 'QtGui|QtWidgets|QWidget|ZzWindowKit|ZzFluentUI' ZzPureTools/appcore
comm -12 \
    <(rg -l '#[[:space:]]*include.*ZzWindowKit/' ZzPureTools/widgets | sort) \
    <(rg -l '#[[:space:]]*include.*ZzFluentUI/' ZzPureTools/widgets | sort)
rg -n -A20 'set_target_properties\(Zz::PureTools PROPERTIES' \
    install/linux-gcc-release/lib/cmake/ZzPureToolsFrame/ZzPureToolsFrameTargets.cmake
rg -n -A20 'set_target_properties\(Zz::PureTools PROPERTIES' \
    install/linux-static-release/lib/cmake/ZzPureToolsFrame/ZzPureToolsFrameTargets.cmake
git diff --check
```

Expected: 前三条无匹配；`comm` 只输出 `ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp`；人工读取两段 `Zz::PureTools` properties，shared interface 不含裸 WindowKit/FluentUI，static 最多出现 CMake 生成的 `LINK_ONLY` link closure；`git diff --check` 返回 0。

- [ ] **Step 9: 提交框架边界与安装门禁**

```bash
git add ZzPureTools tests/Architecture tests/InstallConsumer
git commit -m "测试：锁定应用框架分层边界" \
    -m "自动检查 AppCore 无 UI 依赖、Widgets 无业务访问和公开头独立编译。" \
    -m "验证 shared/static 安装包均可由独立工程消费。"
```

## 完成标准

- `Zz::AppCore` 仅链接 ZzCore/Qt Core，拓扑复杂度为 `O(V + E)`。
- 重复 ID、缺失依赖、环、启动失败和重复停止都有自动测试。
- 页面任务、View、Presenter、ViewModel 的销毁顺序固定，不存在 QObject parent 与 `unique_ptr` 双重所有权。
- 页面与导航缓存有上限，首次访问时才创建页面。
- `ZzPureApplication` 在任一窗口之前安装应用级 `ZzFluentStyle`，析构时先替换样式再销毁 theme controller。
- 每窗口独立拥有 agent、model、controller、host、history 和动画。
- 顶层窗口关闭后由 application queued erase；`WA_DeleteOnClose` 不与 `unique_ptr` 竞争所有权。
- 生产 Widgets 源码中只有 `ZzApplicationWindowPrivate.cpp` 同时组合 WindowKit 与 FluentTitleBar，两个底层组件不互相依赖。
- Linux 示例注册至少两个页面和节点，可完成双窗口独立导航与关闭；shared/static install consumer 通过。
