# ZzLog Multi-platform Build / ZzLog 多平台构建

ZzLog is distributed primarily as source and is intended to be built by the
consuming CMake project. This avoids maintaining incompatible binary packages
for every compiler ABI, architecture, runtime, and build configuration.

ZzLog 主要以源码组件形式分发，由消费项目的 CMake 构建。这样无需为不同编译器 ABI、
架构、运行库和构建配置维护互不兼容的二进制包。

## Requirements / 要求

- CMake 3.23 or newer
- A C++17 compiler
- A working thread library
- Android builds require the NDK toolchain and platform `log` library

fmt 12.1.0 and spdlog 2.0.0 development sources are already included. The
build performs no network access and does not use Catch2.

fmt 12.1.0 和 spdlog 2.0.0 开发版源码已经包含在组件中。构建不会访问网络，也不使用
Catch2。

## Build as a standalone project / 独立构建

All build directories should remain outside the source tree or under `.build`:

所有构建目录应位于源码目录之外，或者统一放在 `.build`：

```console
cmake -S . -B .build/linux-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DZZLOG_BUILD_TESTS=OFF \
  -DZZLOG_BUILD_EXAMPLES=OFF
cmake --build .build/linux-release --parallel
```

Static linkage is the default. To build a shared library:

默认构建静态库。动态库构建方式：

```console
cmake -S . -B .build/linux-shared \
  -DCMAKE_BUILD_TYPE=Release \
  -DZZLOG_BUILD_SHARED=ON
cmake --build .build/linux-shared --parallel
```

## Android

Use the official NDK CMake toolchain and provide an ABI and API level:

使用官方 NDK CMake 工具链，并明确 ABI 与 API Level：

```console
cmake -S . -B .build/android-arm64-v8a \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DZZLOG_BUILD_TESTS=OFF
cmake --build .build/android-arm64-v8a --parallel
```

On Android, enabled console logging is routed to logcat. The application must
provide a writable sandbox path when file logging is enabled.

Android 上启用控制台日志时会输出到 logcat。启用文件日志时，应用必须提供可写的沙箱路径。

## Windows and GUI applications / Windows 与 GUI 应用

MSVC and MinGW outputs are ABI-incompatible and must not be mixed. ZzLog uses
the consuming project's runtime and build configuration. GUI applications can
disable console output through `config.console.enabled = false`.

MSVC 与 MinGW 产物 ABI 不兼容，不能混用。ZzLog 跟随消费项目的运行库和构建配置。
GUI 应用可通过 `config.console.enabled = false` 关闭控制台输出。

## Install and find_package / 安装与 find_package

```console
cmake -S . -B .build/install -DZZLOG_INSTALL=ON
cmake --build .build/install --config Release
cmake --install .build/install --prefix /path/to/zzlog-prefix
```

Consumer:

```cmake
find_package(ZzLog 0.1 CONFIG REQUIRED
    PATHS "/path/to/zzlog-prefix"
    NO_DEFAULT_PATH)
target_link_libraries(my_app PRIVATE ZzLog::ZzLog)
```

The installed package contains the public ZzLog and fmt headers, the ZzLog
library, CMake target metadata, documentation, and dependency licenses. spdlog
headers are private implementation details and are not installed.

安装包包含 ZzLog 与 fmt 公共头文件、ZzLog 库、CMake target 元数据、文档及第三方许可。
spdlog 头文件属于私有实现，不会安装。

## Tests / 测试

The optional smoke test has no external test framework and never downloads
dependencies:

可选 smoke 测试不使用外部测试框架，也不会下载依赖：

```console
cmake -S . -B .build/test -DZZLOG_BUILD_TESTS=ON
cmake --build .build/test --parallel
ctest --test-dir .build/test --output-on-failure
```

Cross-compiled tests should be built but not run on the host unless an emulator
or target runner has been configured.

交叉编译的测试只应构建；除非已经配置模拟器或目标运行器，否则不要在宿主机执行。

## Verification status / 验证状态

The implementation contains platform branches for Linux, Windows, macOS, and
Android. Only platforms explicitly listed in a delivery report as compiled and
run should be considered verified; source-level support is not equivalent to
target-platform validation.

实现包含 Linux、Windows、macOS 和 Android 分支。只有交付报告明确记录为实际编译并运行的
平台才能视为已验证；源码层支持不等于目标平台验证。

Verified on 2026-08-01 / 2026-08-01 已验证：

- Linux x86_64, glibc 2.43
- GCC 15.2.0, CMake 4.3.3
- Debug static build with strict warnings, smoke test, and example
- Release shared build with strict warnings and smoke test
- Static and shared installation followed by an independent
  `find_package(ZzLog)` build and run
- Source `add_subdirectory` consumption when the host already defines
  `fmt::fmt` and `spdlog::spdlog` targets
- ASAN and UBSAN smoke run; LeakSanitizer was disabled because the execution
  environment runs under ptrace

Not target-platform verified / 尚未在目标平台实际验证：

- Windows MSVC and MinGW
- macOS Intel and Apple Silicon
- Android ABIs
- Linux architectures and libc versions other than the host listed above
