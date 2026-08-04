# ZzLog 多平台构建

ZzLog 主要以源码组件交付，由消费项目的 CMake 使用同一编译器 ABI、架构、运行库和
构建配置进行编译。组件内置 fmt 12.1.0 与 spdlog 2.0.0 开发快照，构建过程不访问
网络。

## 要求

- CMake 3.23 或更高版本。
- 完整支持 C++20 `std::format` 与 `std::source_location` 的编译器和标准库。
- 可用的线程库。
- ZzPureToolsPro 顶层工程额外要求 Qt 6.8 或更高版本。

shared/static 只由 `BUILD_SHARED_LIBS` 控制。正式支持的桌面组合为 Linux GCC 13.1+、
Windows MSVC 2022、Qt 官方 SDK 配套 MinGW，以及 macOS Apple Clang。

## Linux

```console
cmake -S . -B .build/linux-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DZZLOG_BUILD_TESTS=ON
cmake --build .build/linux-release --parallel
ctest --test-dir .build/linux-release --output-on-failure
```

静态构建只需改为 `-DBUILD_SHARED_LIBS=OFF`。不要复用已配置为另一种链接模式的
构建目录。

## Windows MSVC 2022

```console
cmake -S . -B .build/windows-msvc \
  -G "Visual Studio 17 2022" -A x64 \
  -DBUILD_SHARED_LIBS=ON \
  -DZZLOG_BUILD_TESTS=ON
cmake --build .build/windows-msvc --config Release --parallel
ctest --test-dir .build/windows-msvc -C Release --output-on-failure
```

MSVC shared 与 static、Debug 与 Release 必须使用各自独立的构建和安装目录。

## Windows Qt 官方 MinGW

正式支持范围只包括 Qt 官方 SDK 附带并与所选 Qt 套件匹配的 MinGW。不要混用系统
MinGW、MSYS2 MinGW、MSVC 产物或不同 major 版本的运行库。

```console
cmake -S . -B .build/windows-mingw -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe \
  -DBUILD_SHARED_LIBS=ON \
  -DZZLOG_BUILD_TESTS=ON
cmake --build .build/windows-mingw --parallel
ctest --test-dir .build/windows-mingw --output-on-failure
```

示例路径必须替换为当前 Qt 安装实际附带的编译器。运行 shared 测试时，ZzLog DLL
及对应 MinGW 运行库必须位于 `PATH`。

## macOS

```console
cmake -S . -B .build/macos-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DZZLOG_BUILD_TESTS=ON
cmake --build .build/macos-release --parallel
ctest --test-dir .build/macos-release --output-on-failure
```

Universal 2 构建可增加：

```console
-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

`CMAKE_OSX_DEPLOYMENT_TARGET` 必须与应用和 Qt 套件保持一致。

## 安装与消费

```console
cmake -S . -B .build/install -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DZZLOG_INSTALL=ON
cmake --build .build/install --parallel
cmake --install .build/install --prefix /path/to/zzlog-prefix
```

消费者只需要：

```cmake
find_package(ZzLog 0.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ZzLog::ZzLog)
```

安装包包含 ZzLog 公共头、库、CMake package、文档和第三方许可证。fmt/spdlog 头和
`ZzLogBackend` 均属于私有实现，不会安装。static 消费者通过导入 target 获得线程库
依赖；Windows shared 消费者还必须部署 `ZzLog.dll`。

## 测试

启用 `ZZLOG_BUILD_TESTS` 后会注册：

- `zzlog.format-contract`：C++20 类型与有效格式契约。
- `zzlog.smoke`：默认配置、线程写入、运行时等级和精确落盘数量。
- `zzlog.overflow`：小队列丢弃计数与 barrier 前后计数稳定性。
- `zzlog.async-barrier`：满队列 control 入队、backend flush 完成和 deadline 超时。

测试使用显式返回码，不依赖会在 Release 中被 `NDEBUG` 移除的 `assert()`。交叉编译时
只应构建测试；除非配置了目标运行器，否则不要在宿主机执行。

## 当前验证状态

截至 2026-08-04，本仓库已在 Linux x86_64、GCC 15.2.0、CMake 4.3.3 上实际完成：

- Debug shared 全量测试。
- Release shared ZzLog 测试与独立安装。
- Release static ZzLog 测试与全新 A/B/consumer 安装消费。
- C++20 非法格式编译失败验证。
- 异步 smoke 20 轮与 overflow 5 轮稳定性验证。

Windows MSVC 2022、Qt 官方 MinGW 和 macOS 当前只有 CMake/源码级静态检查，仍需在
对应目标平台手动编译运行后，才能标记为目标平台动态验证通过。
