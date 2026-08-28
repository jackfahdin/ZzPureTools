# ZzLog

ZzLog 是面向 ZzPureToolsFrame 的 C++20 日志组件。它把固定版本的 fmt 与 spdlog
源码快照编译进唯一的 `ZzLog::ZzLog` target，但公共头只依赖 C++ 标准库。配置和
构建过程不下载依赖，也不会把 fmt/spdlog 头安装给消费者。

## 默认行为

- 控制台默认开启，最低等级为 `Info`。
- 文件日志默认关闭；开启时必须显式提供路径。
- 文件日志默认使用容量为 8192 的异步队列和轮转文件。
- 默认溢出策略为 `OverrunOldest`，可通过计数 API 观测数据丢弃。
- 控制台与文件等级可以独立修改。
- 两种输出均关闭时，普通日志调用安全地成为 no-op。

## 源码接入

```cmake
set(ZZLOG_BUILD_TESTS OFF)
set(ZZLOG_BUILD_EXAMPLES OFF)
set(ZZLOG_INSTALL OFF)

add_subdirectory(third_party/ZzLog)
target_link_libraries(my_app PRIVATE ZzLog::ZzLog)
```

ZzLog 遵循项目统一的 `BUILD_SHARED_LIBS`：`ON` 构建共享库，`OFF` 构建静态库。
不要单独添加或链接内置 fmt/spdlog。

## 基本使用

```cpp
#include <ZzLog/ZzLog.h>

int main()
{
    ZzLog::ZzLogConfig config;
    config.file.enabled = true;
    config.file.path = "logs/application.log";

    const auto result = ZzLog::initialize(config);
    if (!result) {
        return 1;
    }

    ZZ_LOG_INFO("application started, version={}", 1);
    ZzLog::shutdown();
    return 0;
}
```

`initialize()` 与 `shutdown()` 只能由应用生命周期线程调用，调用期间其他线程不得
进入任何 ZzLog API。普通写入、等级修改、`flush()`、`flushAndWait()` 与观测 API
彼此可以并发。`shutdown()` 幂等，但应用仍应在工作线程停止记录日志后再调用。

## 格式化与运行时文本

`ZZ_LOG_TRACE`、`ZZ_LOG_DEBUG`、`ZZ_LOG_INFO`、`ZZ_LOG_WARNING`、
`ZZ_LOG_ERROR` 和 `ZZ_LOG_CRITICAL` 使用 `std::format_string` 在编译期检查格式，
并通过 `std::source_location` 记录调用文件、行号和函数。

```cpp
ZZ_LOG_INFO("user {} connected", userId);
ZzLog::writeText(ZzLog::ZzLogLevel::Info, runtimeMessage);
```

已经格式化或来自配置/网络的运行时文本应交给 `writeText()`；其中的花括号不会被
再次解释。普通写入 API 均为 `noexcept`，后端或格式化异常不会传播到业务线程。

## 异步文件日志

```cpp
ZzLog::ZzLogConfig config;
config.console.enabled = false;
config.file.enabled = true;
config.file.path = "logs/application.log";
config.file.queueSize = 16384;
config.file.overflowPolicy = ZzLog::ZzLogOverflowPolicy::DiscardNew;
```

`droppedMessageCount()` 返回当前运行时 overrun 与 discard 的总数。flush 和 terminate
属于不可丢弃的控制消息，不计入该数值。`flush()` 只投递刷新请求；需要确认调用前
已经接收的消息完成后端刷新时，使用：

```cpp
if (!ZzLog::flushAndWait(std::chrono::seconds(5))) {
    // 超时或后端刷新失败。
}
```

`flushAndWait()` 使用同一个 deadline 约束控制消息入队与后端完成等待，等待期间不
持有生命周期互斥。

## 文件路径与轮转

`ZzLogInitResult::filePath` 和 `activeFilePath()` 返回后端实际使用的规范化绝对路径。
路径解析、目录创建或文件打开失败会通过初始化结果明确报告，不会静默切换目录。
`maxFileSize` 的单位是字节，必须大于 0；`maxFiles` 的合法范围是 0 到 200000。

## CMake 选项

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `BUILD_SHARED_LIBS` | CMake 默认 `OFF` | 统一控制 shared/static |
| `ZZLOG_BUILD_TESTS` | `OFF` | 构建格式、smoke、溢出与 barrier 测试 |
| `ZZLOG_BUILD_EXAMPLES` | `OFF` | 构建基础示例 |
| `ZZLOG_BUILD_WARNINGS` | `OFF` | 独立构建时启用严格警告 |
| `ZZLOG_INSTALL` | 顶层 `ON` | 生成安装与 CMake package 规则 |

三平台命令、安装消费和验证边界见
[BUILDING-MULTIPLATFORM.md](BUILDING-MULTIPLATFORM.md)。依赖版本与源码来源见
[DEPENDENCIES.md](DEPENDENCIES.md)。ZzLog 使用 MIT 许可证，第三方许可证保存在
`licenses/`。
