# ZzLog

ZzLog is a small C++17 logging component with a stable project-facing API. It
vendors pinned fmt and spdlog source snapshots and builds them as part of the
single `ZzLog::ZzLog` target. Configuration never downloads dependencies.

ZzLog 是一个提供稳定项目接口的 C++17 日志组件。它内置固定版本的 fmt 和
spdlog 源码快照，并将依赖直接编入唯一的 `ZzLog::ZzLog` target。配置和构建过程
不会下载任何依赖。

## Defaults / 默认行为

- Console logging is enabled at `info` level.
- File logging is disabled and no implicit file path is used.
- Enabling file logging requires an explicit path.
- Console and file levels are independent.
- Disabling both outputs creates a safe no-op logger.
- File logging uses rotation and is asynchronous by default.

- 控制台默认开启，等级为 `info`。
- 文件日志默认关闭，不会在未知目录创建文件。
- 开启文件日志时必须显式指定路径。
- 控制台和文件日志等级可独立配置。
- 两种输出都关闭时，日志调用安全地成为 no-op。
- 文件日志默认使用异步轮转。

## Source integration / 源码接入

Copy the complete `ZzLog` directory into the consuming project, then:

将完整的 `ZzLog` 目录复制到消费项目，然后：

```cmake
set(ZZLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZZLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZZLOG_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/ZzLog)
target_link_libraries(my_app PRIVATE ZzLog::ZzLog)
```

No global include or library paths are required. Do not add the bundled fmt or
spdlog directories separately.

不需要设置全局头文件目录或库目录，也不要单独添加内置的 fmt/spdlog 子目录。

## Basic use / 基本使用

```cpp
#include <ZzLog/ZzLog.h>

int main() {
    zz::log::Config config;

    const auto result = zz::log::initialize(config);
    if (!result) {
        return 1;
    }

    zz::log::info("application started, pid={}", 1234);
    zz::log::warn("request took {} ms", 87);

    zz::log::shutdown();
}
```

`initialize()` and `shutdown()` are lifecycle operations and must run while no
other thread is logging. Normal logging, `flush()`, and level changes are
thread-safe.

`initialize()` 和 `shutdown()` 属于生命周期操作，应在没有其他日志线程运行时调用。
正常日志调用、`flush()` 和等级修改支持并发使用。

## File logging / 文件日志

```cpp
zz::log::Config config;
config.console.enabled = false;
config.file.enabled = true;
config.file.path = application_log_directory / "my-app.log";
config.file.level = zz::log::Level::debug;
config.file.max_file_size = 20 * 1024 * 1024;
config.file.max_files = 10;

const auto result = zz::log::initialize(config);
if (!result) {
    // result.message contains the initialization error.
}
```

`result.file_path` and `zz::log::active_file_path()` return the normalized
absolute path used by the backend. Directory or file-open failures are
reported; ZzLog never silently switches to a different directory.

`result.file_path` 和 `zz::log::active_file_path()` 会返回实际使用的绝对路径。
目录或文件打开失败会明确报错，ZzLog 不会静默切换到其他目录。

## Runtime text / 运行时字符串

Normal logging functions use compile-time checked fmt strings. Use the `_text`
functions for an already formatted or runtime-provided string that may contain
braces:

普通日志函数使用 fmt 编译期格式检查。已经格式化或运行时提供的字符串可能包含花括号时，
使用 `_text` 函数：

```cpp
zz::log::info("user {} connected", user_id);
zz::log::info_text(runtime_message);
```

The level functions are `zz::log::trace`, `debug`, `info`, `warn`, `error`, and
`critical`, with matching `_text` functions. ZzLog intentionally exposes no
logging macros, so application logging uses one consistent lowercase,
namespaced API. Output levels remain configurable at runtime.

等级函数包括 `zz::log::trace`、`debug`、`info`、`warn`、`error` 和 `critical`，
并提供对应的 `_text` 函数。ZzLog 有意不提供日志宏，使业务代码始终使用统一的小写、
命名空间接口；输出等级仍可在运行时配置。

## CMake options

| Option | Default | Description |
| --- | --- | --- |
| `ZZLOG_BUILD_SHARED` | `OFF` | Build a shared library instead of static. |
| `ZZLOG_BUILD_TESTS` | `OFF` | Build a smoke test with no test-framework dependency. |
| `ZZLOG_BUILD_EXAMPLES` | `OFF` | Build the basic example. |
| `ZZLOG_BUILD_WARNINGS` | `OFF` | Treat strict compiler warnings as errors. |
| `ZZLOG_INSTALL` | top-level `ON` | Generate install and CMake package rules. |

See [BUILDING-MULTIPLATFORM.md](BUILDING-MULTIPLATFORM.md) for platform and
installation details. Third-party versions and source provenance are recorded
in [DEPENDENCIES.md](DEPENDENCIES.md).

ZzLog is licensed under the MIT License; see [LICENSE](LICENSE). Dependency
licenses and source provenance are preserved under `licenses/`.

ZzLog 使用 MIT 许可证，详见 [LICENSE](LICENSE)。第三方许可证和源码溯源信息完整保存
在 `licenses/` 下。
