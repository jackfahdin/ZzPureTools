# ZzLog C++20 合规升级 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把现有 ZzLog 升级为符合项目命名、中文文档、C++20 格式化、非阻塞异步日志和 shared/static 安装要求的单一日志运行时。

**Architecture:** ZzLog 继续封装 vendored spdlog，但公共 API 只暴露标准库类型。应用入口独占初始化和关闭；普通日志写入保持线程安全、`noexcept`，异步 sink 的丢弃计数和有界等待可以观测。

**Tech Stack:** C++20 `std::format`、`std::source_location`、`std::filesystem`、spdlog 2.0.0 snapshot、fmt 12.1.0（仅 private）、CMake、CTest。

---

## 前置条件

- 完成 `2026-08-02-repository-cmake-baseline.md`。
- 工作目录：`/home/zz/Jackfahdin/github/ZzPureToolsFrame/ZzPureToolsFrame`。
- 保留现有轮转文件、控制台等级、线程安全写入和独立 package 能力。
- API 允许破坏性改名，不增加旧 `zz::log` compatibility namespace。
- 生命周期契约固定为：`initialize()` 与 `shutdown()` 只能由应用生命周期线程调用，且调用期间其他线程不得进入任何日志 API；普通写入、等级修改、`flush()`、`flushAndWait()` 和观测 API 彼此可并发。
- 所有测试必须在 `NDEBUG` 已定义的 Release 构建中仍执行检查；禁止用会被编译掉的裸 `assert()` 作为 CTest 验收条件。

## 文件边界

- Modify: `ZzThirdParty/ZzLog/CMakeLists.txt`
- Modify: `ZzThirdParty/ZzLog/include/ZzLog/ZzLog.h`
- Modify: `ZzThirdParty/ZzLog/include/ZzLog/Version.h`
- Modify: `ZzThirdParty/ZzLog/src/ZzLog.cpp`
- Delete: `ZzThirdParty/ZzLog/tests/smoke.cpp`
- Create: `ZzThirdParty/ZzLog/tests/smoke/main.cpp`
- Create: `ZzThirdParty/ZzLog/tests/format_contract/main.cpp`
- Create: `ZzThirdParty/ZzLog/tests/overflow/main.cpp`
- Modify: `ZzThirdParty/ZzLog/README.md`
- Modify: `ZzThirdParty/ZzLog/BUILDING-MULTIPLATFORM.md`
- Modify: `CMakeLists.txt`
- Modify: `cmake/ZzPureToolsFrameConfig.cmake.in`
- Modify: `ZzCore/CMakeLists.txt`

## Task 1: 建立新 API 的编译失败测试

**Files:**
- Create: `ZzThirdParty/ZzLog/tests/format_contract/main.cpp`
- Modify: `ZzThirdParty/ZzLog/CMakeLists.txt`

- [ ] **Step 1: 写入 C++20 API 契约测试**

Create `ZzThirdParty/ZzLog/tests/format_contract/main.cpp`:

```cpp
#include <concepts>
#include <cstdint>
#include <format>
#include <string_view>
#include <type_traits>

#include <ZzLog/ZzLog.h>

static_assert(std::same_as<
    std::underlying_type_t<ZzLog::ZzLogLevel>,
    std::uint8_t>);

static_assert(std::same_as<
    std::underlying_type_t<ZzLog::ZzLogOverflowPolicy>,
    std::uint8_t>);

int main()
{
    ZzLog::ZzLogConfig config;
    config.console.enabled = false;

    const auto result = ZzLog::initialize(config);
    if (!result) {
        return 1;
    }

    ZzLog::write(
        ZzLog::ZzLogLevel::Info,
        std::source_location::current(),
        std::format_string<int>{"value={}"},
        42);
    ZzLog::shutdown();
    return 0;
}
```

- [ ] **Step 2: 注册格式契约 target**

在 `ZzThirdParty/ZzLog/CMakeLists.txt` 的 `ZZLOG_BUILD_TESTS` 分支增加：

```cmake
add_executable(zzlog-format-contract
    tests/format_contract/main.cpp
)
target_link_libraries(zzlog-format-contract PRIVATE ZzLog::ZzLog)
target_compile_features(zzlog-format-contract PRIVATE cxx_std_20)
add_test(NAME zzlog.format-contract COMMAND zzlog-format-contract)
set_tests_properties(zzlog.format-contract PROPERTIES LABELS "unit;zzlog")
```

- [ ] **Step 3: 运行测试并确认因新类型不存在而失败**

Run:

```bash
cmake --preset linux-gcc-debug -DZZLOG_BUILD_TESTS=ON
cmake --build --preset linux-gcc-debug --target zzlog-format-contract
```

Expected: build FAIL，至少包含 `ZzLog has not been declared` 或 `ZzLogLevel is not a member`。

- [ ] **Step 4: 不提交只会失败的中间状态**

继续 Task 2；红灯证据记录在实施日志中，本步骤不创建提交。

## Task 2: 重命名公共类型和 namespace

**Files:**
- Modify: `ZzThirdParty/ZzLog/include/ZzLog/ZzLog.h`
- Modify: `ZzThirdParty/ZzLog/src/ZzLog.cpp`
- Modify: `ZzThirdParty/ZzLog/include/ZzLog/Version.h`
- Delete: `ZzThirdParty/ZzLog/tests/smoke.cpp`
- Create: `ZzThirdParty/ZzLog/tests/smoke/main.cpp`

- [ ] **Step 1: 用以下类型替换旧公共声明**

`ZzLog.h` 必须使用传统 namespace：

```cpp
namespace ZzLog {

enum class ZzLogLevel : std::uint8_t
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

enum class ZzLogOverflowPolicy : std::uint8_t
{
    Block,
    OverrunOldest,
    DiscardNew
};

enum class ZzLogInitError : std::uint8_t
{
    None,
    AlreadyInitialized,
    InvalidConfiguration,
    FilesystemError,
    BackendError
};

struct ZzLogConsoleOptions final
{
    bool enabled = true;
    ZzLogLevel level = ZzLogLevel::Info;
    std::string pattern = "[%T] [%^%l%$] %v";
};

struct ZzLogFileOptions final
{
    bool enabled = false;
    std::filesystem::path path;
    ZzLogLevel level = ZzLogLevel::Info;
    std::string pattern = "[%Y-%m-%d %T.%e] [%l] [%t] %v";
    bool async = true;
    std::size_t queueSize = 8192;
    ZzLogOverflowPolicy overflowPolicy = ZzLogOverflowPolicy::OverrunOldest;
    std::size_t maxFileSize = 10 * 1024 * 1024;
    std::size_t maxFiles = 5;
    bool rotateOnOpen = false;
};

using ZzLogErrorHandler = std::function<void(std::string_view)>;

struct ZzLogConfig final
{
    std::string loggerName = "ZzLog";
    ZzLogConsoleOptions console;
    ZzLogFileOptions file;
    ZzLogLevel flushOn = ZzLogLevel::Error;
    ZzLogErrorHandler errorHandler;
};

struct ZzLogInitResult final
{
    ZzLogInitError error = ZzLogInitError::None;
    std::string message;
    std::filesystem::path filePath;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ZzLogInitError::None;
    }
};

} // namespace ZzLog
```

在这些声明前保留标准库 include 和 `ZZLOG_API` 定义。删除 public `#include "fmt/base.h"`、`namespace detail`、`fmt::format_string` 和所有旧的无前缀类型。

- [ ] **Step 2: 声明新的生命周期与文本 API**

在同一 namespace 中增加：

```cpp
[[nodiscard]] ZZLOG_API ZzLogInitResult initialize(ZzLogConfig config = {});
ZZLOG_API void shutdown() noexcept;
ZZLOG_API void flush() noexcept;

[[nodiscard]] ZZLOG_API bool flushAndWait(
    std::chrono::milliseconds timeout) noexcept;

[[nodiscard]] ZZLOG_API bool isInitialized() noexcept;
[[nodiscard]] ZZLOG_API bool shouldLog(ZzLogLevel level) noexcept;
[[nodiscard]] ZZLOG_API std::filesystem::path activeFilePath();
[[nodiscard]] ZZLOG_API std::uint64_t droppedMessageCount() noexcept;

[[nodiscard]] ZZLOG_API bool setConsoleLevel(ZzLogLevel level) noexcept;
[[nodiscard]] ZZLOG_API bool setFileLevel(ZzLogLevel level) noexcept;

ZZLOG_API void writeText(
    ZzLogLevel level,
    std::string_view message,
    std::source_location location = std::source_location::current()) noexcept;
```

头文件增加 `<chrono>`、`<format>` 和 `<source_location>`；不得包含 fmt 或 spdlog。

- [ ] **Step 3: 机械更新实现名称但不改变控制流**

在 `src/ZzLog.cpp` 中执行以下一致映射：

| Old | New |
|---|---|
| `namespace zz::log` | `namespace ZzLog` |
| `Level` | `ZzLogLevel` |
| `OverflowPolicy` | `ZzLogOverflowPolicy` |
| `InitError` | `ZzLogInitError` |
| `Config` | `ZzLogConfig` |
| `InitResult` | `ZzLogInitResult` |
| `ErrorHandler` | `ZzLogErrorHandler` |
| `is_initialized` | `isInitialized` |
| `should_log` | `shouldLog` |
| `active_file_path` | `activeFilePath` |
| `set_console_level` | `setConsoleLevel` |
| `set_file_level` | `setFileLevel` |
| `log_text` | `writeText` |

枚举项同步改为 PascalCase，配置字段同步改为 camelCase。完成后运行：

```bash
rg -n "namespace zz::log|\bLevel\b|\bConfig\b|is_initialized|log_text" ZzThirdParty/ZzLog/include ZzThirdParty/ZzLog/src
```

Expected: 无旧 API 匹配；spdlog 内部 `backend::level` 不属于旧公共 API，可保留。

- [ ] **Step 4: 迁移 smoke test 到 main.cpp**

把现有 `tests/smoke.cpp` 移到 `tests/smoke/main.cpp`，逐项使用新 namespace、类型、枚举和 camelCase 字段。先在文件顶部定义 Release 仍生效的检查宏：

```cpp
#include <cstdio>

#define ZZ_TEST_CHECK(condition)                                              \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(stderr, "check failed at line %d: %s\n",           \
                         __LINE__, #condition);                               \
            return __LINE__;                                                  \
        }                                                                     \
    } while (false)
```

随后保留以下检查，但全部使用 `ZZ_TEST_CHECK`，不得使用 `<cassert>`：

```cpp
namespace log = ZzLog;

const log::ZzLogConfig defaults;
ZZ_TEST_CHECK(defaults.console.enabled);
ZZ_TEST_CHECK(defaults.console.level == log::ZzLogLevel::Info);
ZZ_TEST_CHECK(defaults.file.overflowPolicy ==
              log::ZzLogOverflowPolicy::OverrunOldest);
ZZ_TEST_CHECK(!defaults.file.enabled);
```

文件日志断言必须继续验证格式化文本、纯文本、四线程共 400 条消息、运行时等级变化和重复 shutdown。

- [ ] **Step 5: 构建并确认格式测试只剩 write 模板缺失**

Run:

```bash
cmake --build --preset linux-gcc-debug --target zzlog-format-contract
```

Expected: FAIL 只指向 `ZzLog::write` 尚未声明；新类型和生命周期 API 已成功编译。

- [ ] **Step 6: 保持红灯工作区并继续 Task 3**

此时 `zzlog-format-contract` 仍按预期失败，不创建提交。保留命名迁移改动，立即继续 Task 3；只有格式 target 恢复绿灯后，才允许把两个任务作为一个可构建边界提交。

## Task 3: 增加标准 C++20 格式化与来源位置

**Files:**
- Modify: `ZzThirdParty/ZzLog/include/ZzLog/ZzLog.h`
- Modify: `ZzThirdParty/ZzLog/src/ZzLog.cpp`
- Modify: `ZzThirdParty/ZzLog/tests/format_contract/main.cpp`

- [ ] **Step 1: 在 public header 中增加无 fmt 依赖的 write 模板**

```cpp
template<typename... ZzArgs>
void write(
    ZzLogLevel level,
    std::source_location location,
    std::format_string<ZzArgs...> format,
    ZzArgs &&...args) noexcept
{
    if (!shouldLog(level)) {
        return;
    }

    try {
        writeText(
            level,
            std::format(format, std::forward<ZzArgs>(args)...),
            location);
    } catch (...) {
        writeText(
            ZzLogLevel::Error,
            "ZzLog formatting failed",
            location);
    }
}
```

头文件增加 `<utility>`。模板参数使用 `ZzArgs`，符合自定义 Concept/类型前缀扫描规则。

- [ ] **Step 2: 增加捕获调用点的便利宏**

```cpp
#define ZZ_LOG_TRACE(format, ...) \
    ::ZzLog::write(::ZzLog::ZzLogLevel::Trace, std::source_location::current(), format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_DEBUG(format, ...) \
    ::ZzLog::write(::ZzLog::ZzLogLevel::Debug, std::source_location::current(), format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_INFO(format, ...) \
    ::ZzLog::write(::ZzLog::ZzLogLevel::Info, std::source_location::current(), format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_WARNING(format, ...) \
    ::ZzLog::write(::ZzLog::ZzLogLevel::Warning, std::source_location::current(), format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_ERROR(format, ...) \
    ::ZzLog::write(::ZzLog::ZzLogLevel::Error, std::source_location::current(), format __VA_OPT__(,) __VA_ARGS__)
#define ZZ_LOG_CRITICAL(format, ...) \
    ::ZzLog::write(::ZzLog::ZzLogLevel::Critical, std::source_location::current(), format __VA_OPT__(,) __VA_ARGS__)
```

不得增加会遮蔽 Qt `qDebug` 等名称的宏。

- [ ] **Step 3: 私有实现把 source location 传给 spdlog**

在匿名 namespace 增加不分配内存的 basename helper：

```cpp
const char *baseName(const char *path) noexcept
{
    const char *result = path;
    for (const char *cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            result = cursor + 1;
        }
    }
    return result;
}
```

`writeText()` 使用：

```cpp
logger->log(
    backend::source_loc{
        baseName(location.file_name()),
        static_cast<int>(location.line()),
        location.function_name()
    },
    toBackendLevel(level),
    backend::string_view_t(message.data(), message.size()));
```

不得在该函数中创建 `std::filesystem::path`。由于公开函数声明为 `noexcept`，完整实现必须把 `logger->log(...)` 包在 `try/catch (...)` 中；异常路径调用私有 error-handler helper（统一命名为 `emitInternalError()`），且用户 error handler 自身抛出的异常也必须被吞掉。`flush()`、`setConsoleLevel()` 和 `setFileLevel()` 中所有可能进入 backend 的调用采用相同边界，禁止异常越过 `noexcept` 导致 `std::terminate()`。

- [ ] **Step 4: 验证有效格式可编译运行**

Run:

```bash
cmake --build --preset linux-gcc-debug --target zzlog-format-contract
ctest --preset linux-gcc-debug -R zzlog.format-contract
```

Expected: build 和 test PASS。

- [ ] **Step 5: 验证错误格式在编译期失败**

临时把测试调用改为：

```cpp
ZzLog::write(
    ZzLog::ZzLogLevel::Info,
    std::source_location::current(),
    std::format_string<int>{"value={:s}"},
    42);
```

Run:

```bash
cmake --build --preset linux-gcc-debug --target zzlog-format-contract
```

Expected: compile FAIL，错误来自 `std::format_string` 的非法 `:s` 整数格式。随后恢复合法 `value={}` 调用并再次构建至 PASS。

- [ ] **Step 6: 提交命名迁移与格式 API 的统一绿灯边界**

```bash
git add ZzThirdParty/ZzLog
git commit -m "日志：增加 C++20 类型安全格式化" \
    -m "统一 ZzLog 公共类型、传统命名空间，并使用 std::format_string 在编译期检查日志参数。" \
    -m "通过 source_location 记录调用位置，在写入后端前移除绝对源码目录，并保证提交时格式契约可构建运行。"
```

## Task 4: 暴露异步丢弃计数和有界 flush

**Files:**
- Modify: `ZzThirdParty/ZzLog/src/ZzLog.cpp`
- Modify: `ZzThirdParty/ZzLog/tests/smoke/main.cpp`
- Create: `ZzThirdParty/ZzLog/tests/overflow/main.cpp`
- Create: `ZzThirdParty/ZzLog/tests/async_barrier/main.cpp`
- Modify: `ZzThirdParty/ZzLog/CMakeLists.txt`
- Modify: `ZzThirdParty/ZzLog/third_party/spdlog/include/spdlog/details/async_log_msg.h`
- Modify: `ZzThirdParty/ZzLog/third_party/spdlog/include/spdlog/details/mpmc_blocking_q.h`
- Modify: `ZzThirdParty/ZzLog/third_party/spdlog/include/spdlog/sinks/async_sink.h`
- Modify: `ZzThirdParty/ZzLog/third_party/spdlog/src/sinks/async_sink.cpp`

- [ ] **Step 1: 先为正常队列增加有界 flush 断言**

`tests/smoke/main.cpp` 继续使用默认 `queueSize=8192` 和 `OverrunOldest`，并保留“四线程共 400 条消息全部落盘”的固定行数断言。在线程 join 后、读取文件前增加：

```cpp
ZZ_TEST_CHECK(log::flushAndWait(std::chrono::seconds(5)));
ZZ_TEST_CHECK(log::droppedMessageCount() == 0);
```

在原有重复 shutdown 之后增加未初始化断言：

```cpp
log::shutdown();
ZZ_TEST_CHECK(log::droppedMessageCount() == 0);
ZZ_TEST_CHECK(log::flushAndWait(std::chrono::milliseconds(10)));
```

- [ ] **Step 2: 创建独立的队列溢出失败测试**

Create `ZzThirdParty/ZzLog/tests/overflow/main.cpp` with:

```cpp
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <ZzLog/ZzLog.h>

#define ZZ_TEST_CHECK(condition)                                              \
    do {                                                                      \
        if (!(condition)) {                                                   \
            std::fprintf(stderr, "check failed at line %d: %s\n",           \
                         __LINE__, #condition);                               \
            return __LINE__;                                                  \
        }                                                                     \
    } while (false)

int main()
{
    namespace log = ZzLog;

    const auto path =
        std::filesystem::current_path() / "zzlog-overflow.log";
    std::error_code error;
    std::filesystem::remove(path, error);

    log::ZzLogConfig config;
    config.console.enabled = false;
    config.file.enabled = true;
    config.file.path = path;
    config.file.level = log::ZzLogLevel::Info;
    config.file.pattern = "%v";
    config.file.async = true;
    config.file.queueSize = 1;
    config.file.overflowPolicy = log::ZzLogOverflowPolicy::DiscardNew;

    const auto initialized = log::initialize(config);
    ZZ_TEST_CHECK(initialized);

    constexpr int threadCount = 8;
    constexpr int messagesPerThread = 4096;
    const std::string payload(4096, 'x');
    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int thread = 0; thread < threadCount; ++thread) {
        threads.emplace_back([thread, &payload] {
            for (int message = 0; message < messagesPerThread; ++message) {
                log::write(
                    log::ZzLogLevel::Info,
                    std::source_location::current(),
                    "thread={} message={} {}",
                    thread,
                    message,
                    payload);
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    const auto droppedBeforeFlush = log::droppedMessageCount();
    ZZ_TEST_CHECK(droppedBeforeFlush > 0);
    ZZ_TEST_CHECK(droppedBeforeFlush <=
        static_cast<std::uint64_t>(threadCount * messagesPerThread));
    ZZ_TEST_CHECK(log::flushAndWait(std::chrono::seconds(10)));
    ZZ_TEST_CHECK(log::droppedMessageCount() == droppedBeforeFlush);

    log::shutdown();
    std::filesystem::remove(path, error);
    return 0;
}
```

在 `ZzThirdParty/ZzLog/CMakeLists.txt` 的测试分支增加：

```cmake
add_executable(zzlog-overflow
    tests/overflow/main.cpp
)
target_link_libraries(zzlog-overflow PRIVATE ZzLog::ZzLog)
target_compile_features(zzlog-overflow PRIVATE cxx_std_20)
add_test(NAME zzlog.overflow COMMAND zzlog-overflow)
set_tests_properties(zzlog.overflow PROPERTIES
    LABELS "component;zzlog"
    TIMEOUT 30
)
```

- [ ] **Step 3: 运行并确认链接失败**

Run:

```bash
cmake --build --preset linux-gcc-debug --target zzlog-smoke zzlog-overflow
```

Expected: 两个 target 至少一个 link FAIL，缺少 `flushAndWait` 和 `droppedMessageCount` 定义。

- [ ] **Step 4: 先在 vendored async sink 建立不可丢弃的 flush barrier**

不得把 `wait_all()` 当成 flush 完成条件；当前实现只轮询 `q_->size()`，worker 已出队但尚未执行 `backend_flush_()` 时会错误返回 true。对 vendored spdlog 的修改限制在上面列出的四个 async 文件，并增加以下最小能力：

1. `async_log_msg` 的 flush 消息可以携带一个共享完成状态；普通 log/terminate 不分配该状态。
2. `mpmc_blocking_queue` 增加 control enqueue 路径。control 满队列时在 deadline 内等待空位，不应用 `block`、`overrun_oldest` 或 `discard_new` 数据策略；存在 control waiter 时，后续数据消息不得抢占为 control 保留的下一个空位。
3. `overrun_oldest` 数据入队发现队首是 control 消息时必须丢弃新数据并增加 data drop 计数，绝不能覆盖 control；`discard_new` 同样只能统计被丢弃的数据消息。flush/terminate control 永远不进入 overrun/discard 计数。
4. `async_sink` 增加 `[[nodiscard]] bool flush_and_wait(std::chrono::milliseconds timeout)`。它用单一 `steady_clock` deadline 约束“等待 control 入队 + 等待完成”总时长；负 timeout 立即 false。
5. worker 收到 barrier 后先完整调用所有 backend sink 的 `flush()`，`backend_flush_()` 返回后才完成共享状态并唤醒 waiter。worker 提前终止、promise broken、backend 控制流异常或 deadline 到期都返回 false，不能仅因队列为空返回 true。
6. 原有 `flush()` 改为投递无 deadline、不可丢弃的 control；它仍可立即返回，但不得走 `enqueue_message_()` 的 overflow 分支。terminate 使用相同 control 通道，确保析构不因 `DiscardNew` 丢失终止消息。

完成状态可直接使用 `std::promise<void>`/`std::shared_future<void>`，不新增公开类型。队列 helper 必须以 predicate 识别 control，保持通用 queue 模板；不得让通用 queue include async message 头形成反向依赖。为避免上游改动扩散，ZzLog 以外的代码不得调用新增 API。

Create `tests/async_barrier/main.cpp`，直接包含 vendored private `async_sink`，以 `ZzBlockingSink` 覆盖两个确定性场景：backend sink 的 `flush()` 被 latch 阻塞时，`flush_and_wait(20ms)` 返回 false；释放 latch 后再次投递 barrier 返回 true。另用 `queue_size=1 + discard_new + 阻塞 backend log` 人为填满队列，确认 barrier 等待空位而不是增加 discard counter，释放 backend 后 barrier 完成且 backend flush 计数精确增加一次。测试不得依赖 sleep 猜测 worker 状态，必须由 mutex/condition variable 明确握手。

在测试分支注册 `zzlog-async-barrier`，链接 `ZzLog::ZzLog`，并只为该测试增加 `third_party/spdlog/include` private include path及与库一致的 `SPDLOG_NAMESPACE=zzlog_spdlog`、`SPDLOG_DISABLE_GLOBAL_LOGGER` private definition；注册 CTest 名 `zzlog.async-barrier`，timeout 30 秒。vendored 头不得进入 `ZzLog` 的安装 interface。

- [ ] **Step 5: 保存 async sink 强类型指针并接入 barrier**

在 `RuntimeState` 增加：

```cpp
std::shared_ptr<backend::sinks::async_sink> asyncFileSink;
std::vector<std::shared_ptr<backend::sinks::sink>> synchronousSinks;
```

初始化 async file sink 时先保存在局部强类型 `shared_ptr`，只有 logger、全部 sink 和配置均构造成功后，才与其他 runtime 字段一起提交给 `state.asyncFileSink`，避免初始化异常留下半状态。`RuntimeState` 同时保留非 async sink 的强类型集合，以便 `flushAndWait()` 不通过 logger 重复向 async sink 投递普通 flush。

`shutdown()` 在锁内先以 release store 清空 `activeLogger`，再把 logger、非 async sink 和 `asyncFileSink` 全部移动到局部变量并重置全局配置；锁外按“逐个 flush 非 async sink -> async sink `flush_and_wait(5s)` -> logger/sink 释放”的顺序清理。不得使用“logger flush -> `wait_all()`”，因为这既可能丢失 flush control，也不能证明 backend flush 已完成。backend 抛出时写 stderr，但 `shutdown()` 保持幂等且不抛异常。

- [ ] **Step 6: 实现两个观测 API**

```cpp
bool flushAndWait(std::chrono::milliseconds timeout) noexcept
{
    if (timeout < std::chrono::milliseconds::zero()) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::vector<std::shared_ptr<backend::sinks::sink>> synchronousSinks;
    std::shared_ptr<backend::sinks::async_sink> asyncSink;
    {
        auto &state = runtime();
        std::lock_guard<std::mutex> lock(state.lifecycleMutex);
        synchronousSinks = state.synchronousSinks;
        asyncSink = state.asyncFileSink;
    }

    try {
        for (const auto &sink : synchronousSinks) {
            sink->flush();
        }
        if (!asyncSink) {
            return true;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto remaining = now < deadline
            ? std::chrono::duration_cast<std::chrono::milliseconds>(
                  deadline - now)
            : std::chrono::milliseconds::zero();
        return asyncSink->flush_and_wait(
            remaining);
    } catch (...) {
        return false;
    }
}

std::uint64_t droppedMessageCount() noexcept
{
    std::shared_ptr<backend::sinks::async_sink> sink;
    {
        auto &state = runtime();
        std::lock_guard<std::mutex> lock(state.lifecycleMutex);
        sink = state.asyncFileSink;
    }

    if (!sink) {
        return 0;
    }
    return static_cast<std::uint64_t>(sink->get_overrun_counter())
        + static_cast<std::uint64_t>(sink->get_discard_counter());
}
```

实现使用 shared_ptr 快照，不能在持有 lifecycle mutex 时等待 worker。`timeout == 0` 保留一次非阻塞 enqueue/完成检查；传给 barrier 的剩余时间不得大于调用者 timeout。`droppedMessageCount()` 的两个计数读取同样放在 `try/catch (...)` 中，异常时返回 0。smoke test 在 runtime 尚未 shutdown 时写入唯一 sentinel，`flushAndWait()` 返回后立即从文件读取并找到 sentinel，随后才调用 `shutdown()`；这条检查锁定“返回 true 表示调用前已接收消息已完成 backend flush”的公开语义。

- [ ] **Step 7: 分别运行不丢失 smoke 与独立 overflow 测试**

Run:

```bash
cmake --build --preset linux-gcc-debug --target \
    zzlog-smoke zzlog-overflow zzlog-async-barrier
ctest --preset linux-gcc-debug -R '^zzlog\.async-barrier$' --output-on-failure
ctest --preset linux-gcc-debug -R '^zzlog.smoke$' --repeat until-fail:20 --output-on-failure
ctest --preset linux-gcc-debug -R '^zzlog.overflow$' --repeat until-fail:5 --output-on-failure
```

Expected: smoke 连续 20 轮 PASS 且每轮仍有精确 403 行；overflow 连续 5 轮 PASS 且每轮 `droppedMessageCount() > 0`，并且 barrier 前后 dropped 计数完全相等；vendored async sink 的 latch 测试证明返回 true 时 backend flush 已完成。所有场景均无 hang。

- [ ] **Step 8: 提交异步可观测性**

```bash
git add ZzThirdParty/ZzLog
git commit -m "日志：增加异步队列观测与有界等待" \
    -m "暴露丢弃消息计数，并允许调用方在指定时限内等待异步 sink 排空。" \
    -m "等待过程不持有生命周期互斥，避免关闭阶段阻塞其他状态读取。"
```

## Task 5: 统一 CMake 模式并接入顶层安装包

**Files:**
- Modify: `ZzThirdParty/ZzLog/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `ZzCore/CMakeLists.txt`
- Modify: `cmake/ZzPureToolsFrameConfig.cmake.in`

- [ ] **Step 1: 将 ZzLog target 改为遵循 BUILD_SHARED_LIBS**

删除 `ZZLOG_BUILD_SHARED` option 和 `if(ZZLOG_BUILD_SHARED)` target 分支，使用：

```cmake
add_library(ZzLog ${ZZLOG_SOURCES})
add_library(ZzLog::ZzLog ALIAS ZzLog)

target_compile_definitions(ZzLog PRIVATE ZZLOG_BUILDING_LIBRARY)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(ZzLog PUBLIC ZZLOG_SHARED)
endif()

target_compile_features(ZzLog PUBLIC cxx_std_20)
```

保留 `SPDLOG_DISABLE_GLOBAL_LOGGER`、重命名的 backend namespace、Threads 和安装规则；调用根工程的 `zz_enable_project_warnings(ZzLog)` 与 `zz_enable_sanitizers(ZzLog)`，但不得把一方 `-Werror` 施加到 vendored fmt/spdlog 源码。实现方式是把 vendored 源码建成未安装的 `ZzLogBackend` OBJECT target并关闭 warnings-as-errors，`ZzLog` target 只编译 `src/ZzLog.cpp` 并链接这些 object；两个 target 都显式要求 C++20 和关闭 extensions。

- [ ] **Step 2: 停止安装 fmt 公共头**

删除：

```cmake
install(DIRECTORY third_party/fmt/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
```

把 fmt include directory 从 `PUBLIC` 移到 `PRIVATE`。安装后的 `ZzLog.h` 必须只依赖标准库。

- [ ] **Step 3: 顶层明确安装 ZzLog package**

在根 `add_subdirectory(ZzThirdParty/ZzLog ...)` 前设置普通变量而非 FORCE cache：

```cmake
set(ZZLOG_INSTALL ON)
set(ZZLOG_BUILD_TESTS ${ZZ_BUILD_TESTS})
add_subdirectory(ZzThirdParty/ZzLog EXCLUDE_FROM_ALL)
```

删除原来的重复 `add_subdirectory` 行。

在 `ZzCore/CMakeLists.txt` 增加：

```cmake
target_link_libraries(ZzCore PRIVATE ZzLog::ZzLog)
```

在 `ZzPureToolsFrameConfig.cmake.in` 的 Qt dependency 后增加：

```cmake
find_dependency(ZzLog 0.1 CONFIG REQUIRED)
```

同时把 `ZzLogConfigVersion.cmake` 的兼容模式从 `SameMajorVersion` 改为 `SameMinorVersion`；0.x 阶段不承诺跨 minor ABI。`ZzLogConfig.cmake` 保留 `find_dependency(Threads REQUIRED)`，独立安装消费者必须只通过安装前缀解析该依赖。

- [ ] **Step 4: 验证 shared 模式只有一个 ZzLog 动态库**

Run:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release -L zzlog
cmake --build --preset linux-gcc-release --target install
find install/linux-gcc-release -type f -name '*ZzLog*'
```

Expected: 测试 PASS；安装树只有一个 ZzLog shared runtime 和一套 ZzLog CMake package，不在各组件目录复制静态实现。

- [ ] **Step 5: 验证静态消费传递 ZzLog target**

Run:

```bash
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release -R install.consumer
```

Expected: PASS；consumer 通过 `find_dependency(ZzLog)` 解析 Core 的 link-only 静态依赖。

- [ ] **Step 6: 提交构建集成**

```bash
git add CMakeLists.txt cmake/ZzPureToolsFrameConfig.cmake.in ZzCore/CMakeLists.txt ZzThirdParty/ZzLog
git commit -m "构建：统一 ZzLog 的共享与静态模式" \
    -m "让日志组件遵循顶层 BUILD_SHARED_LIBS，并从公共安装接口移除 fmt。" \
    -m "顶层包显式发现 ZzLog，保证共享运行时唯一且静态消费者依赖完整。"
```

## Task 6: 补齐中文 Doxygen 和使用文档

**Files:**
- Modify: `ZzThirdParty/ZzLog/include/ZzLog/ZzLog.h`
- Modify: `ZzThirdParty/ZzLog/include/ZzLog/Version.h`
- Modify: `ZzThirdParty/ZzLog/README.md`
- Modify: `ZzThirdParty/ZzLog/BUILDING-MULTIPLATFORM.md`

- [ ] **Step 1: 为所有公开类型和方法补充中文 Doxygen**

每个配置字段至少说明单位和边界。例如：

```cpp
/**
 * @brief 文件日志配置。
 */
struct ZzLogFileOptions final
{
    /** @brief 是否启用文件日志。 */
    bool enabled = false;

    /** @brief 日志文件路径；启用文件日志时不能为空。 */
    std::filesystem::path path;

    /** @brief 异步队列容量，合法范围为 1 到 250000。 */
    std::size_t queueSize = 8192;
};
```

`initialize()` 注释必须写明：只能由应用生命周期线程调用，调用时其他线程不得记录日志。`shutdown()` 注释必须说明幂等性和相同限制。

- [ ] **Step 2: 更新 README 示例**

示例必须使用：

```cpp
ZzLog::ZzLogConfig config;
config.file.enabled = true;
config.file.path = "logs/application.log";

const auto result = ZzLog::initialize(config);
if (!result) {
    return 1;
}

ZZ_LOG_INFO("application started, version={}", 1);
ZzLog::shutdown();
```

文档明确 shared/static 均由 `BUILD_SHARED_LIBS` 控制，正式 Windows MinGW 只承诺 Qt SDK 配套工具链。

- [ ] **Step 3: 执行最终验证**

Run:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release -L zzlog
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release -L zzlog
git diff --check
```

Expected: shared/static 的 ZzLog 测试全部 PASS；diff check 返回 0。

额外以 Release 显式运行 `zzlog-smoke` 与 `zzlog-overflow`，故意把一个 `ZZ_TEST_CHECK(true)` 临时改成 `ZZ_TEST_CHECK(false)`，确认两个配置都会由 CTest 报告失败；恢复后再运行至 PASS。该步骤只验证测试不会因 `NDEBUG` 失效，不提交临时改动。

- [ ] **Step 4: 扫描公共依赖和旧 API**

Run:

```bash
rg -n "fmt/|spdlog/|namespace zz::log|\bConfig\b|\bLevel\b" ZzThirdParty/ZzLog/include
```

Expected: 无匹配。

- [ ] **Step 5: 提交文档与注释**

```bash
git add ZzThirdParty/ZzLog
git commit -m "文档：补充 ZzLog 中文接口契约" \
    -m "说明配置边界、线程安全、初始化关闭顺序和异步队列行为。" \
    -m "更新 C++20 格式化示例及三平台 shared/static 构建说明。"
```

## 完成标准

- ZzLog public header 不包含 fmt/spdlog。
- 所有自定义类型使用 `Zz` 前缀，namespace 使用传统写法。
- C++20 有效格式编译通过，错误格式编译失败。
- 普通写入线程安全且 `noexcept`。
- async sink 可报告 overrun/discard 总数并支持有界等待。
- shared 构建只加载一个 ZzLog runtime。
- static install consumer 能解析 ZzLog link-only 依赖。
- 生命周期和全部公开 API 有简体中文 Doxygen。
