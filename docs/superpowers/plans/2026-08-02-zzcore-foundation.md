# ZzCore 基础设施 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现不依赖 QtGui/Widgets 的错误、结果、路径、设置、可取消任务和 Qt 日志桥基础设施。

**Architecture:** `ZzCore` 只链接 Qt Core/Concurrent 与私有 ZzLog。预期失败使用 `ZzResult<T>`；任务由应用拥有的 `ZzTaskExecutor` 调度到独立 `QThreadPool`，取消采用 `std::stop_token`；全局 Qt message handler 由显式拥有的 `ZzQtLogBridge` 安装和恢复。

**Tech Stack:** Qt 6.8 Core/Concurrent/Test、C++20 `std::variant`/Concept/`std::jthread`/`std::stop_token`、ZzLog、Qt Test、ASan/UBSan。

---

## 前置条件

- 完成仓库基线和 ZzLog C++20 计划。
- `Zz::Core` target 已存在并私有链接 `ZzLog::ZzLog`。
- 本计划禁止增加 QtGui、QtWidgets、QtQuick 或 Qt Private include。
- 本计划首次创建每个公共类型时即补齐简体中文 Doxygen；公开类、枚举、方法和信号必须说明用途，带参数的方法必须包含 `@param`，非 `void` 方法必须包含 `@return`，并写明所有权、线程或状态前提。

## 文件边界

### 错误与结果

- Create: `ZzCore/include/ZzCore/ZzErrorCode.h`
- Create: `ZzCore/include/ZzCore/ZzError.h`
- Create: `ZzCore/src/ZzError.cpp`
- Create: `ZzCore/src/private/ZzErrorPrivate.h`
- Create: `ZzCore/src/private/ZzErrorPrivate.cpp`
- Create: `ZzCore/include/ZzCore/ZzResult.h`
- Create: `ZzCore/tests/ZzResultTest.cpp`

### 路径与设置

- Create: `ZzCore/include/ZzCore/ZzApplicationPaths.h`
- Create: `ZzCore/src/ZzApplicationPaths.cpp`
- Create: `ZzCore/src/private/ZzApplicationPathsPrivate.h`
- Create: `ZzCore/src/private/ZzApplicationPathsPrivate.cpp`
- Create: `ZzCore/include/ZzCore/ZzSettingsStore.h`
- Create: `ZzCore/include/ZzCore/ZzQtSettingsStore.h`
- Create: `ZzCore/src/ZzQtSettingsStore.cpp`
- Create: `ZzCore/src/private/ZzQtSettingsStorePrivate.h`
- Create: `ZzCore/src/private/ZzQtSettingsStorePrivate.cpp`
- Create: `ZzCore/tests/ZzApplicationPathsTest.cpp`
- Create: `ZzCore/tests/ZzQtSettingsStoreTest.cpp`

### 任务执行

- Create: `ZzCore/include/ZzCore/ZzTaskStatus.h`
- Create: `ZzCore/include/ZzCore/ZzTaskHandle.h`
- Create: `ZzCore/include/ZzCore/ZzTaskExecutor.h`
- Create: `ZzCore/src/ZzTaskExecutor.cpp`
- Create: `ZzCore/src/private/ZzTaskExecutorPrivate.h`
- Create: `ZzCore/src/private/ZzTaskExecutorPrivate.cpp`
- Create: `ZzCore/tests/ZzTaskExecutorTest.cpp`

### Qt 日志桥

- Create: `ZzCore/include/ZzCore/ZzQtLogBridgeConfig.h`
- Create: `ZzCore/include/ZzCore/ZzQtLogBridge.h`
- Create: `ZzCore/src/ZzQtLogBridge.cpp`
- Create: `ZzCore/src/private/ZzQtLogBridgePrivate.h`
- Create: `ZzCore/src/private/ZzQtLogBridgePrivate.cpp`
- Create: `ZzCore/tests/ZzQtLogBridgeTest.cpp`

### 构建与架构测试

- Modify: `ZzCore/CMakeLists.txt`
- Create: `ZzCore/tests/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/Architecture/CheckZzCoreDependencies.cmake`
- Create: `tests/Architecture/CheckZzCoreDependenciesContract.cmake`
- Create: `tests/Architecture/fixtures/zzcore-good/ZzGoodHeader.h`
- Create: `tests/Architecture/fixtures/zzcore-bad/ZzBadHeader.h`
- Create: `tests/Architecture/ZzCorePublicHeadersTest.cpp`

## Task 1: 实现 ZzError 和 ZzResult

**Files:**
- Create: `ZzCore/include/ZzCore/ZzErrorCode.h`
- Create: `ZzCore/include/ZzCore/ZzError.h`
- Create: `ZzCore/src/ZzError.cpp`
- Create: `ZzCore/src/private/ZzErrorPrivate.h`
- Create: `ZzCore/src/private/ZzErrorPrivate.cpp`
- Create: `ZzCore/include/ZzCore/ZzResult.h`
- Create: `ZzCore/tests/ZzResultTest.cpp`
- Modify: `ZzCore/CMakeLists.txt`
- Create: `ZzCore/tests/CMakeLists.txt`

- [ ] **Step 1: 写入 Result 失败测试**

Create `ZzCore/tests/ZzResultTest.cpp`:

```cpp
#include <QtTest/QTest>

#include <memory>
#include <utility>

#include <ZzCore/ZzError.h>
#include <ZzCore/ZzResult.h>

class ZzResultTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void storesValue()
    {
        auto result = ZzCore::ZzResult<int>::success(42);
        QVERIFY(result.hasValue());
        QCOMPARE(result.value(), 42);
    }

    void storesError()
    {
        const ZzCore::ZzError error(
            ZzCore::ZzErrorCode::InvalidArgument,
            QStringLiteral("negative size"),
            QStringLiteral("size=-1"));
        auto result = ZzCore::ZzResult<int>::failure(error);

        QVERIFY(!result.hasValue());
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QCOMPARE(result.error().technicalMessage(), QStringLiteral("negative size"));
        QCOMPARE(result.error().context(), QStringLiteral("size=-1"));
    }

    void supportsVoid()
    {
        QVERIFY(ZzCore::ZzResult<void>::success());
        auto failure = ZzCore::ZzResult<void>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Cancelled,
                QStringLiteral("cancelled")));
        QVERIFY(!failure);
        QCOMPARE(failure.error().code(), ZzCore::ZzErrorCode::Cancelled);
    }

    void extractsMoveOnlyValue()
    {
        auto result = ZzCore::ZzResult<std::unique_ptr<int>>::success(
            std::make_unique<int>(7));

        auto value = std::move(result).value();

        QVERIFY(value != nullptr);
        QCOMPARE(*value, 7);
    }
};

QTEST_GUILESS_MAIN(ZzResultTest)

#include "ZzResultTest.moc"
```

- [ ] **Step 2: 注册测试并确认缺少头文件**

Create `ZzCore/tests/CMakeLists.txt`:

```cmake
add_executable(ZzResultTest ZzResultTest.cpp)
target_link_libraries(ZzResultTest PRIVATE Qt6::Test Zz::Core)
set_target_properties(ZzResultTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzResultTest)
zz_enable_sanitizers(ZzResultTest)
add_test(NAME core.result COMMAND ZzResultTest)
set_tests_properties(core.result PROPERTIES LABELS "unit;core")
```

在 `ZzCore/CMakeLists.txt` 末尾增加：

```cmake
if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzResultTest
```

Expected: compile FAIL，缺少 `ZzCore/ZzError.h`。

- [ ] **Step 3: 定义稳定错误码**

Create `ZzCore/include/ZzCore/ZzErrorCode.h`:

```cpp
#pragma once

#include <cstdint>

namespace ZzCore {

/**
 * @brief 描述跨组件通用失败类别。
 */
enum class ZzErrorCode : std::uint16_t
{
    None = 0,
    InvalidArgument,
    InvalidState,
    Cancelled,
    TimedOut,
    NotFound,
    Unsupported,
    Io,
    Backend,
    Unknown
};

} // namespace ZzCore
```

- [ ] **Step 4: 实现可复制的 PIMPL 错误值**

`ZzError` public API：

```cpp
namespace ZzCore {

class ZzErrorPrivate;

/**
 * @brief 保存技术错误信息，不直接携带最终用户文案。
 */
class ZZ_CORE_EXPORT ZzError final
{
public:
    ZzError();
    ZzError(
        ZzErrorCode code,
        QString technicalMessage,
        QString context = {});
    ZzError(const ZzError &other);
    ZzError(ZzError &&other) noexcept;
    ZzError &operator=(const ZzError &other);
    ZzError &operator=(ZzError &&other) noexcept;
    ~ZzError();

    [[nodiscard]] bool isError() const noexcept;
    [[nodiscard]] ZzErrorCode code() const noexcept;
    [[nodiscard]] QString technicalMessage() const;
    [[nodiscard]] QString context() const;

private:
    std::unique_ptr<ZzErrorPrivate> d_ptr;
};

} // namespace ZzCore
```

`ZzErrorPrivate` 是不继承 QObject/QSharedData 的 final 私有值，包含 `ZzErrorCode code`、`QString technicalMessage`、`QString context`。默认构造 code 为 `None`。copy constructor 在 `other.d_ptr` 非空时用 `std::make_unique<ZzErrorPrivate>(*other.d_ptr)` 深复制，复制 moved-from 对象时创建默认 private；copy assignment 使用 copy-and-swap 保证强异常安全。move 操作默认转移唯一所有权；moved-from 对象的 null private 由 getter 映射为 `None` 和空字符串，仍可析构、赋值和复制。构造时禁止 `code == None` 但 message 非空；Debug 使用 `Q_ASSERT`。这保持架构规定的 `unique_ptr` PIMPL，不为错误路径引入隐式共享。

此时编辑 `ZzCore/CMakeLists.txt` 文件顶部、`add_library(ZzCore ...)` 之前的源文件清单；红灯步骤之前不得引用尚未创建的源文件。必须替换为当前完整清单，使随后执行的 `zz_configure_library_target(... SOURCES ${zz_core_sources})` 能覆盖所有一方翻译单元：

```cmake
set(zz_core_sources
    src/private/ZzCoreVersion.cpp
    src/ZzError.cpp
    src/private/ZzErrorPrivate.cpp
)
```

- [ ] **Step 5: 实现 header-only ZzResult**

`ZzResult<T>` 使用私有 tag 构造，避免 `T` 与 `ZzError` 构造歧义：

```cpp
namespace ZzCore {

template<typename ZzValue>
class [[nodiscard]] ZzResult final
{
public:
    [[nodiscard]] static ZzResult success(ZzValue value)
    {
        return ZzResult(std::in_place_index<0>, std::move(value));
    }

    [[nodiscard]] static ZzResult failure(ZzError error)
    {
        Q_ASSERT(error.isError());
        if (!error.isError()) {
            std::terminate();
        }
        return ZzResult(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept
    {
        return storage_.index() == 0;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return hasValue();
    }

    [[nodiscard]] ZzValue &value() &
    {
        Q_ASSERT(hasValue());
        auto *value = std::get_if<0>(&storage_);
        if (value == nullptr) {
            std::terminate();
        }
        return *value;
    }

    [[nodiscard]] const ZzValue &value() const &
    {
        Q_ASSERT(hasValue());
        const auto *value = std::get_if<0>(&storage_);
        if (value == nullptr) {
            std::terminate();
        }
        return *value;
    }

    [[nodiscard]] ZzValue &&value() &&
    {
        Q_ASSERT(hasValue());
        auto *value = std::get_if<0>(&storage_);
        if (value == nullptr) {
            std::terminate();
        }
        return std::move(*value);
    }

    [[nodiscard]] const ZzError &error() const & noexcept
    {
        Q_ASSERT(!hasValue());
        const auto *error = std::get_if<1>(&storage_);
        if (error == nullptr) {
            std::terminate();
        }
        return *error;
    }

    [[nodiscard]] ZzError &&error() && noexcept
    {
        Q_ASSERT(!hasValue());
        auto *error = std::get_if<1>(&storage_);
        if (error == nullptr) {
            std::terminate();
        }
        return std::move(*error);
    }

private:
    template<std::size_t ZzIndex, typename ZzArgument>
    explicit ZzResult(std::in_place_index_t<ZzIndex>, ZzArgument &&argument)
        : storage_(
              std::in_place_index<ZzIndex>,
              std::forward<ZzArgument>(argument))
    {
    }

    std::variant<ZzValue, ZzError> storage_;
};

} // namespace ZzCore
```

同一文件实现以下完整 specialization，不得只实现布尔判断：

```cpp
template<>
class [[nodiscard]] ZzResult<void> final
{
public:
    [[nodiscard]] static ZzResult success() noexcept;
    [[nodiscard]] static ZzResult failure(ZzError error);
    [[nodiscard]] bool hasValue() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] const ZzError &error() const & noexcept;
    [[nodiscard]] ZzError &&error() && noexcept;

private:
    explicit ZzResult(std::optional<ZzError> error) noexcept;
    std::optional<ZzError> error_;
};
```

`success()` 保存 `std::nullopt`，`failure()` 只接受 `isError()==true` 的 error；泛型和 void specialization 都在传入 `None` 时 Debug 断言、Release terminate，禁止构造“失败但错误码为 None”的状态。两类 Result 的 `error()` 都在错误状态返回引用；成功状态属于程序员错误，Debug 断言，Release 调用 `std::terminate()`，不得从 `noexcept` API 抛出 `std::bad_optional_access`。泛型 `value()` 同样用 `std::get_if` 检查，错误状态在 Release terminate，不得泄漏 `std::bad_variant_access`；泛型 `error()` 对成功状态采用相同契约。头文件显式包含 `<cstddef>`、`<exception>`、`<optional>`、`<utility>`、`<variant>`、`<QtCore/QtGlobal>` 和 `ZzError.h`，不得依赖传递 include。泛型版本保留编译器生成的条件复制/移动操作：当 `ZzValue` 为 `std::unique_ptr` 等 move-only 类型时，`ZzResult<ZzValue>` 可移动但不可复制，并通过 `value() &&` 转移成功值。

- [ ] **Step 6: 构建并运行 Result 测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzResultTest
ctest --preset linux-gcc-debug -R '^core\.result$'
```

Expected: PASS。

- [ ] **Step 7: 提交错误模型**

```bash
git add ZzCore
git commit -m "核心：实现错误与结果模型" \
    -m "增加可复制的 ZzError 和 header-only ZzResult<T>。" \
    -m "区分预期失败与程序员错误，并为 void 结果提供无额外成功分配的专门实现。"
```

## Task 2: 实现应用路径和设置存储

**Files:**
- Create: `ZzCore/include/ZzCore/ZzApplicationPaths.h`
- Create: `ZzCore/src/ZzApplicationPaths.cpp`
- Create: `ZzCore/src/private/ZzApplicationPathsPrivate.h`
- Create: `ZzCore/src/private/ZzApplicationPathsPrivate.cpp`
- Create: `ZzCore/include/ZzCore/ZzSettingsStore.h`
- Create: `ZzCore/include/ZzCore/ZzQtSettingsStore.h`
- Create: `ZzCore/src/ZzQtSettingsStore.cpp`
- Create: `ZzCore/src/private/ZzQtSettingsStorePrivate.h`
- Create: `ZzCore/src/private/ZzQtSettingsStorePrivate.cpp`
- Create: `ZzCore/tests/ZzApplicationPathsTest.cpp`
- Create: `ZzCore/tests/ZzQtSettingsStoreTest.cpp`
- Modify: `ZzCore/CMakeLists.txt`
- Modify: `ZzCore/tests/CMakeLists.txt`

- [ ] **Step 1: 写路径测试**

`ZzApplicationPathsTest` 使用 `QStandardPaths::setTestModeEnabled(true)`，构造 `ZzApplicationPaths("ZzTests", "CorePaths")`，验证：

```cpp
QVERIFY(!paths.configDirectory().isEmpty());
QVERIFY(!paths.dataDirectory().isEmpty());
QVERIFY(!paths.cacheDirectory().isEmpty());
QCOMPARE(
    paths.logDirectory(),
    QDir(paths.dataDirectory()).filePath(QStringLiteral("logs")));
QVERIFY(paths.configDirectory().endsWith(
    QDir::cleanPath(QStringLiteral("ZzTests/CorePaths"))));
QVERIFY(paths.dataDirectory().endsWith(
    QDir::cleanPath(QStringLiteral("ZzTests/CorePaths"))));
QVERIFY(paths.cacheDirectory().endsWith(
    QDir::cleanPath(QStringLiteral("ZzTests/CorePaths"))));
QVERIFY(paths.ensureDirectories());
QVERIFY(QDir(paths.logDirectory()).exists());
```

测试结束恢复 test mode；测试进程不使用真实用户目录。

- [ ] **Step 2: 写设置存储测试**

`ZzQtSettingsStoreTest` 使用 `QTemporaryDir` 和显式 INI 文件：

```cpp
ZzCore::ZzQtSettingsStore store(temporary.filePath("settings.ini"));
QVERIFY(store.write(QStringLiteral("theme/mode"), QStringLiteral("dark")));
QVERIFY(store.sync());
auto readResult = store.read(
    QStringLiteral("theme/mode"),
    QStringLiteral("light"));
QVERIFY(readResult);
QCOMPARE(
    std::move(readResult).value(),
    QVariant(QStringLiteral("dark")));
QVERIFY(!store.write(QString(), 1));
```

同一测试再增加 `readRejectsWrongThread()`：在创建线程构造 store，通过 `std::jthread` 调用 `read()`，把 Result 移回测试线程，精确验证 `InvalidState`；worker 不得直接访问测试 QObject。向 `ZzCore/tests/CMakeLists.txt` 追加：

```cmake
add_executable(ZzApplicationPathsTest ZzApplicationPathsTest.cpp)
target_link_libraries(ZzApplicationPathsTest PRIVATE Qt6::Test Zz::Core)
set_target_properties(ZzApplicationPathsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzApplicationPathsTest)
zz_enable_sanitizers(ZzApplicationPathsTest)
add_test(NAME core.application-paths COMMAND ZzApplicationPathsTest)
set_tests_properties(core.application-paths PROPERTIES LABELS "unit;core")

add_executable(ZzQtSettingsStoreTest ZzQtSettingsStoreTest.cpp)
target_link_libraries(ZzQtSettingsStoreTest PRIVATE Qt6::Test Zz::Core)
set_target_properties(ZzQtSettingsStoreTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzQtSettingsStoreTest)
zz_enable_sanitizers(ZzQtSettingsStoreTest)
add_test(NAME core.qt-settings-store COMMAND ZzQtSettingsStoreTest)
set_tests_properties(core.qt-settings-store PROPERTIES LABELS "unit;core")
```

- [ ] **Step 3: 运行并确认两个测试因类型缺失而失败**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzApplicationPathsTest ZzQtSettingsStoreTest
```

Expected: compile FAIL，缺少对应 public headers。

- [ ] **Step 4: 实现 ZzApplicationPaths**

公开 API：

```cpp
class ZZ_CORE_EXPORT ZzApplicationPaths final
{
public:
    ZzApplicationPaths(QString organizationName, QString applicationName);
    ZzApplicationPaths(const ZzApplicationPaths &other);
    ZzApplicationPaths &operator=(const ZzApplicationPaths &other);
    ~ZzApplicationPaths();

    [[nodiscard]] QString configDirectory() const;
    [[nodiscard]] QString dataDirectory() const;
    [[nodiscard]] QString cacheDirectory() const;
    [[nodiscard]] QString logDirectory() const;
    [[nodiscard]] ZzResult<void> ensureDirectories() const;

private:
    std::unique_ptr<ZzApplicationPathsPrivate> d_ptr;
};
```

因为使用 `unique_ptr`，copy constructor/assignment 必须深复制 private 数据。private 构造时 trim 组织名和应用名，并拒绝空值、`.`、`..`、`/`、`\\` 或平台路径分隔符；构造函数不能返回 Result，因此非法输入在 Debug 断言并把 private 标为 invalid，所有目录 getter 返回空字符串，`ensureDirectories()` 在 Release 返回 `InvalidArgument`。

有效输入使用 `QStandardPaths::GenericConfigLocation`、`GenericDataLocation` 和 `GenericCacheLocation` 作为基目录，再严格按 `QDir(base).filePath(organizationName)`、`QDir(organizationDirectory).filePath(applicationName)` 两段拼接。禁止读取或修改进程级 `QCoreApplication::organizationName()`/`applicationName()`。`ensureDirectories()` 为 config/data/cache/log 分别调用 `QDir().mkpath()`，任一路径失败返回 `ZzErrorCode::Io`，context 保存失败的规范化路径。

- [ ] **Step 5: 定义设置接口和 QSettings 实现**

```cpp
class ZZ_CORE_EXPORT ZzSettingsStore
{
public:
    virtual ~ZzSettingsStore() = default;

    [[nodiscard]] virtual ZzResult<QVariant> read(
        QStringView key,
        const QVariant &defaultValue = {}) const = 0;

    [[nodiscard]] virtual ZzResult<void> write(
        QStringView key,
        const QVariant &value) = 0;

    [[nodiscard]] virtual ZzResult<void> remove(QStringView key) = 0;
    [[nodiscard]] virtual ZzResult<void> sync() = 0;
};
```

`ZzQtSettingsStore.h` 使用以下完整 API：

```cpp
class ZzQtSettingsStorePrivate;

/**
 * @brief 使用显式 INI 文件实现线程归属明确的设置存储。
 * @note 对象只能在构造线程访问；所有失败都通过 ZzResult 返回。
 */
class ZZ_CORE_EXPORT ZzQtSettingsStore final : public ZzSettingsStore
{
public:
    /**
     * @brief 创建使用指定 INI 文件的设置存储。
     * @param filePath 设置文件路径，不能为空。
     */
    explicit ZzQtSettingsStore(QString filePath);

    /** @brief 释放设置后端；必须在构造线程调用。 */
    ~ZzQtSettingsStore() override;

    ZzQtSettingsStore(const ZzQtSettingsStore &) = delete;
    ZzQtSettingsStore &operator=(const ZzQtSettingsStore &) = delete;
    ZzQtSettingsStore(ZzQtSettingsStore &&) = delete;
    ZzQtSettingsStore &operator=(ZzQtSettingsStore &&) = delete;

    /**
     * @brief 读取设置值。
     * @param key 非空设置键。
     * @param defaultValue 键不存在时返回的值。
     * @return 成功值，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<QVariant> read(
        QStringView key,
        const QVariant &defaultValue = {}) const override;

    /**
     * @brief 写入设置值。
     * @param key 非空设置键。
     * @param value 要保存的值。
     * @return 成功状态，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<void> write(
        QStringView key,
        const QVariant &value) override;

    /**
     * @brief 删除指定设置键。
     * @param key 非空设置键。
     * @return 成功状态，或参数、线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<void> remove(QStringView key) override;

    /**
     * @brief 把待处理修改同步到 INI 文件。
     * @return 成功状态，或线程与 I/O 错误。
     */
    [[nodiscard]] ZzResult<void> sync() override;

private:
    std::unique_ptr<ZzQtSettingsStorePrivate> d_ptr;
};
```

`ZzQtSettingsStore` 使用四文件 PIMPL，构造函数用 `QSettings(filePath, QSettings::IniFormat)` 创建后端并记录构造线程。每个 public 方法在触碰 QSettings 前检查 `QThread::currentThread() == ownerThread`，所有构建类型都返回 `InvalidState`；这里是公开 Result 失败契约，不得用 Debug 断言中止进程，否则上面的 wrong-thread 测试无法成立。`read()` 成功返回实际值或 defaultValue；空 key 返回 `InvalidArgument`；读写后检查 `QSettings::status()` 并映射为 `Io`。不得用 invalid QVariant 伪装线程或 I/O 错误。

再次编辑 `add_library(ZzCore ...)` 之前的 `zz_core_sources`，替换为当前累计清单；禁止在 `zz_configure_library_target()` 调用之后用 `target_sources()` 补生产源：

```cmake
set(zz_core_sources
    src/private/ZzCoreVersion.cpp
    src/ZzError.cpp
    src/private/ZzErrorPrivate.cpp
    src/ZzApplicationPaths.cpp
    src/private/ZzApplicationPathsPrivate.cpp
    src/ZzQtSettingsStore.cpp
    src/private/ZzQtSettingsStorePrivate.cpp
)
```

- [ ] **Step 6: 运行路径和设置测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzApplicationPathsTest ZzQtSettingsStoreTest
ctest --preset linux-gcc-debug -R '^core\.(application-paths|qt-settings-store)$'
```

Expected: 2/2 PASS；仓库根目录没有新设置文件。

- [ ] **Step 7: 提交路径与设置**

```bash
git add ZzCore
git commit -m "核心：实现应用路径与设置存储" \
    -m "使用 QStandardPaths 和 QDir 生成跨平台目录，禁止手工拼接分隔符。" \
    -m "增加可注入设置接口和具有线程归属、同步错误语义的 QSettings 实现。"
```

## Task 3: 实现可取消任务执行器

**Files:**
- Create: `ZzCore/include/ZzCore/ZzTaskStatus.h`
- Create: `ZzCore/include/ZzCore/ZzTaskHandle.h`
- Create: `ZzCore/include/ZzCore/ZzTaskExecutor.h`
- Create: `ZzCore/src/ZzTaskExecutor.cpp`
- Create: `ZzCore/src/private/ZzTaskExecutorPrivate.h`
- Create: `ZzCore/src/private/ZzTaskExecutorPrivate.cpp`
- Create: `ZzCore/tests/ZzTaskExecutorTest.cpp`
- Modify: `ZzCore/CMakeLists.txt`
- Modify: `ZzCore/tests/CMakeLists.txt`

- [ ] **Step 1: 写成功、异常、取消和 shutdown 测试**

`ZzTaskExecutorTest` 完整定义以下 slot；除下面代码外，再为每个 slot 补齐类声明、`QTEST_GUILESS_MAIN`、moc include 以及所需标准库/Qt include：

```cpp
void returnsValue()
{
    ZzCore::ZzTaskExecutor executor(2);
    auto handle = executor.submit<int>([](std::stop_token) {
        return ZzCore::ZzResult<int>::success(42);
    });
    handle.future().waitForFinished();
    QCOMPARE(handle.future().result().value(), 42);
}

void convertsException()
{
    ZzCore::ZzTaskExecutor executor(1);
    auto handle = executor.submit<int>([](std::stop_token)
        -> ZzCore::ZzResult<int> {
        throw std::runtime_error("failure");
    });
    handle.future().waitForFinished();
    QCOMPARE(
        handle.future().result().error().code(),
        ZzCore::ZzErrorCode::Unknown);
}

void cooperativelyCancels()
{
    ZzCore::ZzTaskExecutor executor(1);
    auto handle = executor.submit<int>([](std::stop_token token) {
        while (!token.stop_requested()) {
            QThread::yieldCurrentThread();
        }
        return ZzCore::ZzResult<int>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Cancelled,
                QStringLiteral("cancelled")));
    });
    handle.requestCancel();
    handle.future().waitForFinished();
    QCOMPARE(
        handle.future().result().error().code(),
        ZzCore::ZzErrorCode::Cancelled);
}

void rejectsSubmissionAfterShutdown()
{
    ZzCore::ZzTaskExecutor executor(1);
    QVERIFY(executor.shutdown(QDeadlineTimer(1000)));
    auto handle = executor.submit<int>([](std::stop_token) {
        return ZzCore::ZzResult<int>::success(1);
    });
    handle.future().waitForFinished();
    QCOMPARE(
        handle.future().result().error().code(),
        ZzCore::ZzErrorCode::InvalidState);
}

void shutdownCancelsRunningAndQueuedTasks()
{
    ZzCore::ZzTaskExecutor executor(1);
    QSemaphore runningTaskStarted;

    auto running = executor.submit<int>([&runningTaskStarted](
        std::stop_token token) {
        runningTaskStarted.release();
        while (!token.stop_requested()) {
            QThread::yieldCurrentThread();
        }
        return ZzCore::ZzResult<int>::failure(
            ZzCore::ZzError(
                ZzCore::ZzErrorCode::Cancelled,
                QStringLiteral("running task cancelled")));
    });

    QVERIFY(runningTaskStarted.tryAcquire(1, 1000));
    auto queued = executor.submit<int>([](std::stop_token) {
        return ZzCore::ZzResult<int>::success(99);
    });

    QVERIFY(executor.shutdown(QDeadlineTimer(2000)));
    running.future().waitForFinished();
    queued.future().waitForFinished();
    QCOMPARE(
        running.future().result().error().code(),
        ZzCore::ZzErrorCode::Cancelled);
    QCOMPARE(
        queued.future().result().error().code(),
        ZzCore::ZzErrorCode::Cancelled);
}

void returnsMoveOnlyValue()
{
    ZzCore::ZzTaskExecutor executor(1);
    auto handle = executor.submit<std::unique_ptr<int>>(
        [](std::stop_token) {
            return ZzCore::ZzResult<std::unique_ptr<int>>::success(
                std::make_unique<int>(7));
        });

    auto future = handle.future();
    future.waitForFinished();
    auto result = future.takeResult();
    auto value = std::move(result).value();
    QVERIFY(value != nullptr);
    QCOMPARE(*value, 7);
}

void lateCancelDoesNotOverwriteFinished()
{
    ZzCore::ZzTaskExecutor executor(1);
    auto handle = executor.submit<int>([](std::stop_token) {
        return ZzCore::ZzResult<int>::success(1);
    });
    handle.future().waitForFinished();
    QCOMPARE(handle.status(), ZzCore::ZzTaskStatus::Finished);
    handle.requestCancel();
    QCOMPARE(handle.status(), ZzCore::ZzTaskStatus::Finished);
}
```

另外实现 `destroyedContextDropsContinuation()`：用 `QSemaphore` 阻塞 worker，在释放前销毁作为 `QFuture::then(QObject *, ...)` context 的 QObject；完成任务并处理事件后，原子 callback counter 必须仍为 0。实现 `shutdownTimeoutKeepsTaskOwned()`：不可取消阶段由 semaphore 阻塞，第一次 `shutdown(QDeadlineTimer(10))` 精确返回 false；辅助 `std::jthread` 随后释放 semaphore，第二次 shutdown 返回 true，future 完成且没有泄漏。测试不得真的留下永久阻塞线程。

向 `ZzCore/tests/CMakeLists.txt` 追加：

```cmake
add_executable(ZzTaskExecutorTest ZzTaskExecutorTest.cpp)
target_link_libraries(ZzTaskExecutorTest PRIVATE Qt6::Test Zz::Core)
set_target_properties(ZzTaskExecutorTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzTaskExecutorTest)
zz_enable_sanitizers(ZzTaskExecutorTest)
add_test(NAME core.task-executor COMMAND ZzTaskExecutorTest)
set_tests_properties(core.task-executor PROPERTIES LABELS "unit;core")
```

- [ ] **Step 2: 运行并确认类型缺失**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzTaskExecutorTest
```

Expected: compile FAIL，缺少 `ZzTaskExecutor.h`。

- [ ] **Step 3: 定义任务状态和共享 state**

`ZzTaskStatus`：`Pending`、`Running`、`Finished`、`CancellationRequested`。

在 `ZzTaskHandle.h` 的传统内部 namespace 中定义：

```cpp
namespace ZzCore {

namespace Internal {

struct ZzTaskControl
{
    virtual ~ZzTaskControl() = default;

    std::uint64_t taskId = 0;
    std::stop_source stopSource;
    std::atomic<ZzTaskStatus> status{ZzTaskStatus::Pending};

    bool requestCancellation() noexcept
    {
        auto current = status.load(std::memory_order_acquire);
        while (current != ZzTaskStatus::Finished
               && current != ZzTaskStatus::CancellationRequested) {
            if (status.compare_exchange_weak(
                    current,
                    ZzTaskStatus::CancellationRequested,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                stopSource.request_stop();
                return true;
            }
        }
        return false;
    }
};

template<typename ZzValue>
struct ZzTaskState final : ZzTaskControl
{
    QPromise<ZzResult<ZzValue>> promise;
};

} // namespace Internal

template<typename ZzValue>
class ZzTaskHandle final
{
public:
    [[nodiscard]] QFuture<ZzResult<ZzValue>> future() const
    {
        return state_->promise.future();
    }

    void requestCancel() noexcept
    {
        static_cast<void>(state_->requestCancellation());
    }

    [[nodiscard]] ZzTaskStatus status() const noexcept
    {
        return state_->status.load(std::memory_order_acquire);
    }

private:
    friend class ZzTaskExecutor;
    explicit ZzTaskHandle(
        std::shared_ptr<Internal::ZzTaskState<ZzValue>> state)
        : state_(std::move(state))
    {
    }

    std::shared_ptr<Internal::ZzTaskState<ZzValue>> state_;
};

} // namespace ZzCore
```

- [ ] **Step 4: 定义 Executor submit Concept 和模板**

`ZzTaskExecutor` 是四文件 QObject/PIMPL，公开 `threadCount()`、`isAcceptingTasks()`、`shutdown(QDeadlineTimer)`。模板签名：

```cpp
template<typename ZzValue, typename ZzCallable>
requires std::invocable<ZzCallable &, std::stop_token>
    && std::same_as<
        std::invoke_result_t<ZzCallable &, std::stop_token>,
        ZzResult<ZzValue>>
[[nodiscard]] ZzTaskHandle<ZzValue> submit(ZzCallable &&callable)
```

模板创建 shared state 并调用 `promise.start()`。先创建捕获 typed shared state 和 decay-copy/move callable 的 runnable，再调用以下私有非模板 API；两个函数都由 public executor 转发到仍存活的 private：

```cpp
[[nodiscard]] bool enqueue(
    const std::shared_ptr<Internal::ZzTaskControl> &control,
    QRunnable *runnable);
void finishTask(std::uint64_t taskId) noexcept;
```

`enqueue()` 在同一把 mutex 下检查 `acceptingTasks`、给 `control->taskId` 分配非零递增 ID、插入注册表并调用 `threadPool.start(runnable)`，成功返回 true。失败时不取得 runnable 所有权，submit 负责 delete；成功后 QThreadPool 按默认 auto-delete 取得 runnable 所有权。runnable 使用 `control->taskId`，不捕获尚未赋值的局部 taskId。它必须：

1. 以 CAS 把 `Pending` 改为 `Running`；若已是 `CancellationRequested`，不覆盖该状态并直接产生 Cancelled Result。
2. 调用 callable；捕获 `std::exception` 和未知异常并转换为 `Unknown`。
3. `promise.addResult(std::move(result))`，再以 release store 设置 `Finished`，最后 `promise.finish()`；future 一旦观察到 finished，`status()` 必须已经返回 Finished。
4. 使用 scope guard 无条件调用 `finishTask(control->taskId)`；addResult/finish 路径不得遗漏注册表清理。

executor 已 shutdown 时 `enqueue()` 返回 false；`submit()` 立即向 promise 写入 `InvalidState`，设置 `Finished`，调用 `promise.finish()` 并 delete runnable。不得让被拒绝或尚未开始的 future 永久未完成。模板显式使用 `std::decay_t<ZzCallable>` 保存 callable，支持 move-only callable；`QFuture::result()` 只用于可复制 Result，move-only 结果必须由唯一消费者调用 `takeResult()`，公开 Doxygen 必须说明其消费语义。

- [ ] **Step 5: 实现独立 QThreadPool 生命周期**

Private 持有：

```cpp
QThreadPool threadPool;
std::mutex tasksMutex;
bool acceptingTasks = true;
std::uint64_t nextTaskId = 1;
std::unordered_map<
    std::uint64_t,
    std::shared_ptr<Internal::ZzTaskControl>> tasks;
```

构造时把 max thread count 设为参数；参数小于 1 回退到 `qMax(1, QThread::idealThreadCount())`。`shutdown()` 必须使用以下顺序：

```cpp
bool ZzTaskExecutorPrivate::shutdown(QDeadlineTimer deadline)
{
    std::vector<std::shared_ptr<Internal::ZzTaskControl>> snapshot;
    {
        std::lock_guard<std::mutex> lock(tasksMutex);
        acceptingTasks = false;
        snapshot.reserve(tasks.size());
        for (const auto &[taskId, control] : tasks) {
            static_cast<void>(taskId);
            snapshot.push_back(control);
        }
    }

    for (const auto &control : snapshot) {
        static_cast<void>(control->requestCancellation());
    }

    return threadPool.waitForDone(deadline);
}
```

禁止调用 `QThreadPool::clear()`：清除队列会让 runnable 没有机会完成对应 promise。已排队 runnable 由线程池正常取出，进入用户 callable 前检查 stop token，已取消时立即向 promise 写入 `Cancelled` 并完成。

`shutdown(deadline)` 幂等；超时只返回 `false`，不分离仍在运行的任务。`ZzTaskExecutorPrivate` 析构时再次对注册表请求停止，然后使用 `QDeadlineTimer(QDeadlineTimer::Forever)` 无限等待线程池；这保证 runnable 不会在 executor 和 private state 销毁后回调。析构前先检查 `threadPool.contains(QThread::currentThread())`：Debug 断言，Release `std::terminate()`，不得在 executor 自己的 worker 中制造自等待死锁。公开中文 Doxygen 必须说明 executor 可并发 submit，但 shutdown/析构只能由 owner 线程调用、析构可能等待不协作 callable、callable 不得销毁 executor。

再次把 `add_library(ZzCore ...)` 之前的 `zz_core_sources` 替换为当前累计清单：

```cmake
set(zz_core_sources
    src/private/ZzCoreVersion.cpp
    src/ZzError.cpp
    src/private/ZzErrorPrivate.cpp
    src/ZzApplicationPaths.cpp
    src/private/ZzApplicationPathsPrivate.cpp
    src/ZzQtSettingsStore.cpp
    src/private/ZzQtSettingsStorePrivate.cpp
    src/ZzTaskExecutor.cpp
    src/private/ZzTaskExecutorPrivate.cpp
)
```

- [ ] **Step 6: 运行任务测试和 Sanitizer**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzTaskExecutorTest
ctest --preset linux-gcc-debug -R '^core\.task-executor$' --repeat until-fail:20
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan --target ZzTaskExecutorTest
ctest --preset linux-clang-asan -R '^core\.task-executor$'
```

Expected: 普通配置连续 20 轮 PASS；ASan/UBSan PASS，无未完成 future 或泄漏。

- [ ] **Step 7: 提交任务执行器**

```bash
git add ZzCore
git commit -m "核心：实现可取消任务执行器" \
    -m "使用独立 QThreadPool、QPromise 和 stop_token 管理后台任务。" \
    -m "覆盖成功、异常、协作取消、停止后拒绝提交和有界关闭路径。"
```

## Task 4: 实现 Qt 日志桥

**Files:**
- Create: `ZzCore/include/ZzCore/ZzQtLogBridgeConfig.h`
- Create: `ZzCore/include/ZzCore/ZzQtLogBridge.h`
- Create: `ZzCore/src/ZzQtLogBridge.cpp`
- Create: `ZzCore/src/private/ZzQtLogBridgePrivate.h`
- Create: `ZzCore/src/private/ZzQtLogBridgePrivate.cpp`
- Create: `ZzCore/tests/ZzQtLogBridgeTest.cpp`
- Modify: `ZzCore/CMakeLists.txt`
- Modify: `ZzCore/tests/CMakeLists.txt`

- [ ] **Step 1: 写安装、转发、恢复和重入测试**

测试使用 `QTemporaryDir` 初始化 ZzLog 文件 sink，保存一个测试旧 handler。覆盖：

```cpp
ZzCore::ZzQtLogBridge bridge;
QVERIFY(bridge.install({.chainPreviousHandler = false}));
QVERIFY(bridge.isInstalled());
qWarning("bridge-warning");
QVERIFY(bridge.uninstall());
QVERIFY(!bridge.isInstalled());
QVERIFY(ZzLog::flushAndWait(std::chrono::seconds(5)));
```

读取日志文件验证存在 `bridge-warning`。再安装旧 handler，使用 `chainPreviousHandler=true` 验证旧 handler 计数增加。不得在测试中调用 `qFatal()`。

测试还必须实现确定性的 `concurrentUninstallWaitsForInFlightHandler()`。先安装一个不调用 Qt 日志的测试旧 handler；该 handler 释放 `previousEntered` semaphore 后阻塞在 `allowPreviousReturn`。bridge 以 `chainPreviousHandler=true` 安装，writer `std::jthread` 只写入一次唯一 warning；主线程等待 `previousEntered`，此刻 bridge 已增加 `inFlight` 且 RAII guard 尚未析构。再启动 uninstaller `std::jthread`：先释放 `uninstallStarted`，随后调用 `bridge.uninstall()`，返回时释放 `uninstallReturned`。主线程等待 started 后，断言 `uninstallReturned.tryAcquire(1, 50)` 为 false，再释放 `allowPreviousReturn`；join 两线程并断言 uninstall 成功、returned semaphore 到达、旧 handler 已恢复。最后用 `qInstallMessageHandler(originalHandler)` 恢复测试进程原 handler。该流程重复 100 次，所有 semaphore/atomic 状态每轮重建；不得用固定 sleep 或概率性日志循环代替同步。向 `ZzCore/tests/CMakeLists.txt` 追加：

```cmake
add_executable(ZzQtLogBridgeTest ZzQtLogBridgeTest.cpp)
target_link_libraries(ZzQtLogBridgeTest PRIVATE Qt6::Test Zz::Core)
set_target_properties(ZzQtLogBridgeTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzQtLogBridgeTest)
zz_enable_sanitizers(ZzQtLogBridgeTest)
add_test(NAME core.qt-log-bridge COMMAND ZzQtLogBridgeTest)
set_tests_properties(core.qt-log-bridge PROPERTIES LABELS "unit;core")
```

- [ ] **Step 2: 运行并确认缺少桥类型**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzQtLogBridgeTest
```

Expected: compile FAIL，缺少 `ZzQtLogBridge.h`。

- [ ] **Step 3: 定义配置和公开状态 API**

```cpp
struct ZzQtLogBridgeConfig final
{
    bool chainPreviousHandler = false;
};
```

```cpp
class ZZ_CORE_EXPORT ZzQtLogBridge final
{
public:
    ZzQtLogBridge();
    ~ZzQtLogBridge();

    ZzQtLogBridge(const ZzQtLogBridge &) = delete;
    ZzQtLogBridge &operator=(const ZzQtLogBridge &) = delete;

    [[nodiscard]] ZzResult<void> install(
        ZzQtLogBridgeConfig config = {});
    [[nodiscard]] ZzResult<void> uninstall();
    [[nodiscard]] bool isInstalled() const noexcept;

private:
    std::unique_ptr<ZzQtLogBridgePrivate> d_ptr;
};
```

- [ ] **Step 4: 实现全局 handler 的安全边界**

不得用 atomic 裸指针单独承担 private 生命周期。`ZzQtLogBridgePrivate.cpp` 定义进程级 `ZzQtLogBridgeGlobalState`，包含 `std::mutex lifecycleMutex`、`std::condition_variable noInFlight`、非拥有 `active`、`QtMessageHandler previousHandler`、`std::size_t inFlight` 和 `bool uninstalling`。它是 translation-unit private 静态对象，不导出到公共头。

install/uninstall/handler 的关键算法固定为：

```text
install:
  lock lifecycleMutex
  若 active != null 或 uninstalling，返回 InvalidState
  在持锁状态调用 qInstallMessageHandler(bridgeHandler)，保存返回的 previous
  写入 active=this、previousHandler=previous、installed=true
  unlock

bridgeHandler:
  若 thread_local handling 已为 true，直接返回，禁止递归调用旧 handler
  设置 handling=true，并用 scope guard 保证所有正常/异常返回都恢复 false
  lock lifecycleMutex
  若 active == null：复制 previousHandler 后 unlock；若 previous 非空则调用并返回
  否则复制 active/previous/config，++inFlight，unlock
  用 RAII guard 保证所有返回和 catch 路径都会 lock、--inFlight、在零时 notify_all
  在锁外 qFormatLogMessage、UTF-8 转换和 ZzLog::writeText
  捕获所有异常；需要 chain 时在锁外调用复制出的 previous

uninstall:
  lock lifecycleMutex，验证 active == this
  uninstalling=true，active=null
  在持锁状态恢复 previous handler，阻止新 bridge 回调越过状态切换
  wait(noInFlight, inFlight == 0)
  installed=false，uninstalling=false；保留 previousHandler 供已进入但尚未取状态的旧回调使用
  unlock
```

第二个 bridge install 返回 `InvalidState`。handler 内不调用任何 Qt 日志函数、不发 QObject signal；映射 Debug/Info/Warning/Critical/Fatal 到对应 ZzLog level。析构若仍安装，必须以 `static_cast<void>(uninstall())` 调用并且不抛异常。公开 Doxygen 说明 bridge 是进程级唯一资源，install/uninstall 可与日志写入并发但不可彼此并发，外部代码在 bridge 安装期间不得替换全局 Qt handler。

再次把 `add_library(ZzCore ...)` 之前的 `zz_core_sources` 替换为最终累计清单：

```cmake
set(zz_core_sources
    src/private/ZzCoreVersion.cpp
    src/ZzError.cpp
    src/private/ZzErrorPrivate.cpp
    src/ZzApplicationPaths.cpp
    src/private/ZzApplicationPathsPrivate.cpp
    src/ZzQtSettingsStore.cpp
    src/private/ZzQtSettingsStorePrivate.cpp
    src/ZzTaskExecutor.cpp
    src/private/ZzTaskExecutorPrivate.cpp
    src/ZzQtLogBridge.cpp
    src/private/ZzQtLogBridgePrivate.cpp
)
```

- [ ] **Step 5: 运行日志桥测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzQtLogBridgeTest
ctest --preset linux-gcc-debug -R '^core\.qt-log-bridge$' --repeat until-fail:20
```

Expected: 连续 20 轮 PASS，无递归、hang 或 handler 泄漏。

- [ ] **Step 6: 提交日志桥**

```bash
git add ZzCore
git commit -m "核心：实现 Qt 到 ZzLog 的安全桥接" \
    -m "显式安装并恢复 Qt message handler，增加唯一实例和线程重入保护。" \
    -m "覆盖旧 handler 链式调用、文件写入和析构恢复行为。"
```

## Task 5: 建立 Core 依赖和公共头门禁

**Files:**
- Create: `tests/Architecture/CheckZzCoreDependencies.cmake`
- Create: `tests/Architecture/CheckZzCoreDependenciesContract.cmake`
- Create: `tests/Architecture/fixtures/zzcore-good/ZzGoodHeader.h`
- Create: `tests/Architecture/fixtures/zzcore-bad/ZzBadHeader.h`
- Create: `tests/Architecture/ZzCorePublicHeadersTest.cpp`
- Modify: `tests/Architecture/CMakeLists.txt`
- Modify: `ZzCore/CMakeLists.txt`

- [ ] **Step 1: 先写扫描器负例并确认红灯**

`ZzGoodHeader.h` 只包含 QtCore 并使用传统 `namespace ZzCore { namespace Internal { ... } }`；`ZzBadHeader.h` 必须同时包含 `<QGuiApplication>` 和 `namespace ZzCore::Bad {}`。`CheckZzCoreDependenciesContract.cmake` 使用 `execute_process()` 两次调用尚不存在的 `CheckZzCoreDependencies.cmake`：good 期望返回 0，bad 期望非 0，并检查错误文本同时列出文件和规则名。

Run:

```bash
cmake -DZZ_SOURCE_DIR=$PWD -P tests/Architecture/CheckZzCoreDependenciesContract.cmake
```

Expected: FAIL，明确报告缺少 `CheckZzCoreDependencies.cmake`，而不是 fixture 自身语法错误。

- [ ] **Step 2: 实现依赖、namespace、target 与公共头门禁**

`CheckZzCoreDependencies.cmake` 接收必填 `ZZ_SCAN_ROOT`，递归读取该目录的 `.h/.cpp`。它逐条报告文件、行和规则名，禁止：

```text
任何 include path 中的 QtGui、QtWidgets、QtQuick 或 */private/q*
无模块前缀的 QGuiApplication、QWindow、QWidget、QImage、QPixmap、QIcon include
namespace[空白]+任意标识符::
```

允许 `src/private/` 目录名本身，禁止的是 include 内容。扫描参数化 root，确保 fixture 与真实源码走完全相同逻辑。

在 `ZzCore/CMakeLists.txt` 中紧跟 `target_link_libraries(ZzCore ...)` 后增加 configure-time guard：读取 `LINK_LIBRARIES` 和 `INTERFACE_LINK_LIBRARIES`，逐项拒绝 `Qt6::Gui`、`Qt6::Widgets`、`Qt6::Quick`；这一步必须检查 PRIVATE link，不能只检查 installed export。同一 guard 还必须要求 `LINK_LIBRARIES` 中匹配 `ZzLog::ZzLog` 的直接依赖条目恰好一个，并拒绝 `INTERFACE_LINK_LIBRARIES` 中裸的 `ZzLog::ZzLog`；static target 为闭合最终链接而生成的 `$<LINK_ONLY:ZzLog::ZzLog>` 允许保留。

基线计划已经在 `cmake/ZzArchitectureChecks.cmake` 定义 `zz_add_public_header_probe()`/`zz_add_public_header_directory()`，并在 `tests/Architecture/CMakeLists.txt` 对 `ZzCore/include` 注册唯一的 aggregate `ZzPublicHeadersTest`。不得创建第二套 helper，也不得再次调用 `add_custom_target(ZzPublicHeadersTest)`。其 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 会在本计划重新配置时自动发现 `ZzApplicationPaths.h`、`ZzError.h`、`ZzErrorCode.h`、`ZzQtLogBridge.h`、`ZzQtLogBridgeConfig.h`、`ZzQtSettingsStore.h`、`ZzResult.h`、`ZzSettingsStore.h`、`ZzTaskExecutor.h`、`ZzTaskHandle.h` 和 `ZzTaskStatus.h`，并继续覆盖已有 `ZzCoreVersion.h`；基线的显式 probe 继续覆盖生成的 `ZzCoreExport.h`。每个 header 必须保持一个独立 translation unit。

`ZzCorePublicHeadersTest.cpp` 负责运行时值语义 smoke：构造 error、void Result、move-only Result、paths 和 task handle 可见类型；它不替代逐头编译 target。

- [ ] **Step 3: 注册正负契约、真实扫描与 public-header smoke**

```cmake
add_test(
    NAME architecture.zzcore-dependencies
    COMMAND ${CMAKE_COMMAND}
        -DZZ_SCAN_ROOT=${PROJECT_SOURCE_DIR}/ZzCore
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckZzCoreDependencies.cmake
)
set_tests_properties(architecture.zzcore-dependencies PROPERTIES
    LABELS "architecture;core"
)

add_test(
    NAME architecture.zzcore-dependencies-contract
    COMMAND ${CMAKE_COMMAND}
        -DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckZzCoreDependenciesContract.cmake
)
set_tests_properties(architecture.zzcore-dependencies-contract PROPERTIES
    LABELS "architecture;core"
)

add_executable(ZzCorePublicHeadersTest ZzCorePublicHeadersTest.cpp)
target_link_libraries(ZzCorePublicHeadersTest PRIVATE Zz::Core)
add_test(NAME headers.zzcore-smoke COMMAND ZzCorePublicHeadersTest)
set_tests_properties(headers.zzcore-smoke PROPERTIES LABELS "headers;core")
```

- [ ] **Step 4: 先运行扫描契约，再验证构建和 link interface**

Run:

```bash
cmake -DZZ_SOURCE_DIR=$PWD -P tests/Architecture/CheckZzCoreDependenciesContract.cmake
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug -L core
cmake --build --preset linux-gcc-debug --target ZzPublicHeadersTest
cmake --build --preset linux-gcc-debug --target install
rg -n "Qt6::(Gui|Widgets|Quick)" install/linux-gcc-debug/lib/cmake/ZzPureToolsFrame/ZzPureToolsFrameTargets.cmake
```

Expected: contract 中 good PASS、bad 被拒绝；Core tests 与逐头编译 PASS。Targets 文件中其他组件可以出现 Gui/Widgets，但 `Zz::Core` 对应段不得出现它们；shared interface 不含裸的 ZzLog，static interface 最多只有 CMake 生成的 `LINK_ONLY` ZzLog 链接闭包。人工读取相邻 target block 确认；configure-time guard 已单独保证 PRIVATE link 不含 UI target 且 ZzLog 没有升格为公开依赖。

- [ ] **Step 5: 运行全部 Core Sanitizer 测试**

Run:

```bash
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan
ctest --preset linux-clang-asan -L core
git diff --check
```

Expected: 所有 Core 测试 PASS；无 ASan/UBSan 报告；diff check 返回 0。

- [ ] **Step 6: 提交架构门禁**

```bash
git add tests/Architecture ZzCore
git commit -m "测试：锁定 ZzCore 非 UI 依赖边界" \
    -m "自动拒绝 QtGui、Widgets、Quick、Qt Private 和链式 namespace。" \
    -m "验证公共头、安装 target 与 Sanitizer 下的 Core 完整测试集。"
```

## 完成标准

- `Zz::Core` 只依赖 Qt Core/Concurrent 和私有 ZzLog。
- Result 的成功路径不创建错误对象，错误路径携带稳定 code/context。
- 路径不手拼分隔符，设置接口可注入且线程归属明确。
- task executor 不使用 Qt 全局线程池，不强制终止线程，不留下未完成 future。
- Qt handler 内不再次调用 Qt 日志，不直接触碰 UI QObject。
- 所有 Core 公开类、枚举、方法和信号具有简体中文 Doxygen，并通过最终 `PUBLIC_API_DOXYGEN` 架构规则。
- Core unit、architecture、shared/static install consumer 和 ASan/UBSan 全部通过。
