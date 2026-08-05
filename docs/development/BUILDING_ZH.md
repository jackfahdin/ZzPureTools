# ZzPureToolsPro 构建手册

## 构建事实源

根 `CMakeLists.txt` 定义项目选项、依赖和安装规则，`CMakePresets.json` 定义受支持的平台矩阵。`CMakeUserPresets.json.example` 只展示如何从父进程环境传值；本机可将其内容用于 `CMakeUserPresets.json`，后者已由 `.gitignore` 排除，不得提交本机 SDK 绝对路径。

除明确写入性能参考档案的受审环境外，构建文档、preset 和已安装 CMake 文件都不得保存开发者绝对路径。

## 先决条件

所有平台需要：

- CMake 3.23 或更高版本。
- Qt 6.8 或更高版本，包含 Core、Gui、Widgets、Svg、Concurrent；测试还需要 Qt Test。
- 能执行 C++20 `<format>`、`<source_location>` 和 `<stop_token>` 的标准库。
- Git 和对应生成器；Ninja preset 需要 Ninja。
- shared 与 static 必须使用同一平台和同一 ABI 的 Qt 开发包。

编译器下限：

| 平台 | 编译器下限 | 额外要求 |
|---|---|---|
| Linux GCC | GCC/G++ 13.1 | 发布参考档案可使用更高版本，但必须记录精确身份 |
| Linux Clang | Clang/Clang++ 17 | `GCC_13_TOOLCHAIN_ROOT` 必须提供兼容的 libstdc++ 工具链 |
| Windows MSVC | Visual Studio 2022，MSVC 19.38 | 从 x64 Developer PowerShell 执行 |
| Windows MinGW | Qt SDK 随附的 MinGW-w64 GCC 13+ | Qt kit、工具链和 Ninja 必须来自同一官方 Qt SDK |
| macOS | Apple Clang 15 | deployment target 最低为 macOS 12.0 |

Linux 完整 runner 还需要 Bash、Xvfb、`xdpyinfo`、`taskset`、`sha256sum` 和常规 binutils。Ubuntu 22.04 兼容镜像审计需要 Docker、`file`、`ldd`、`readelf`、`strings` 与受审 GNU runtime 许可证。Windows runner 需要 PowerShell 7、`dumpbin.exe` 和 Qt MinGW 的 `objdump.exe`。macOS runner 需要 `lipo` 和两个架构匹配的 Qt SDK。

如果系统 xkbcommon 只有运行库而缺少开发头，可在本机 user preset 或命令行设置 `XKB_INCLUDE_DIR` 与 `XKB_LIBRARY`；路径不得写入共享 preset。

## 环境变量

| 变量 | 平台 | 含义 |
|---|---|---|
| `QT_ROOT` | Linux | Qt 6.8+ SDK 根目录 |
| `GCC_13` / `GXX_13` | Linux | GCC/G++ 13.1+ 可执行文件 |
| `GCC_13_TOOLCHAIN_ROOT` | Linux Clang | Clang 使用的 GCC 标准库工具链根 |
| `CLANG_17` / `CLANGXX_17` | Linux | Clang/Clang++ 17+ 可执行文件 |
| `QT_MSVC_ROOT` | Windows | MSVC ABI 的 Qt 根目录 |
| `QT_SDK_ROOT` | Windows | 包含 Qt MinGW kit、工具链和 Ninja 的受控 Qt SDK 根 |
| `QT_MINGW_ROOT` | Windows | `QMAKE_XSPEC=win32-g++` 的 Qt 根目录 |
| `QT_MINGW_TOOLCHAIN_ROOT` | Windows | 与 MinGW Qt kit 完全匹配的工具链根 |
| `QT_MINGW_EXPECTED_GCC_VERSION` | Windows | Qt kit 声明的精确 GCC 版本 |
| `NINJA_EXE` | Windows | 同一 Qt SDK 中的 Ninja 可执行文件 |
| `QT_MACOS_ARM64_ROOT` | macOS | arm64 Qt SDK 根目录 |
| `QT_MACOS_X86_64_ROOT` | macOS | x86_64 Qt SDK 根目录 |
| `APPLE_CLANG` / `APPLE_CLANGXX` | macOS | Apple Clang C/C++ 可执行文件 |
| `ZZ_BENCHMARK_COMMIT` | 性能 | 当前干净工作树的 40 位 Git commit |
| `ZZ_RUNNER_IMAGE_DIGEST` | 性能 | `sha256:<64位小写摘要>` runner 身份 |
| `ZZ_GPU_IDENTITY` | 性能 | 经审核的 renderer 与驱动身份 |
| `ZZ_UBUNTU2204_BUILD_IMAGE` | Linux 可选兼容门禁 | 带 registry/repository 和 immutable `@sha256:` 的镜像引用 |
| `ZZ_RELEASE_EVIDENCE_ROOT` | 正式发布 | 仓库外受审核证据目录的绝对路径 |

MSVC 目标文件、库和 Qt kit 不得与 MinGW 目标文件、库或 Qt kit 混用。两个 ABI 使用独立 build 目录、独立 Qt prefix 和独立二进制检查工具。MSYS2 Qt 不能替代本矩阵指定的 Qt 官方 MinGW kit。

## Linux preset

先在 shell 中设置 Linux 环境变量，然后对每个 preset 执行配置、构建、CTest 和安装：

```bash
export QT_ROOT=/path/to/qt
export GCC_13=/path/to/gcc
export GXX_13=/path/to/g++
export GCC_13_TOOLCHAIN_ROOT=/path/to/gcc-toolchain
export CLANG_17=/path/to/clang
export CLANGXX_17=/path/to/clang++

for preset in \
  linux-gcc-debug \
  linux-gcc-release \
  linux-static-release \
  linux-gcc-release-lto \
  linux-static-release-lto \
  linux-clang-tidy-release \
  linux-clang-tidy-static \
  linux-clang-asan; do
  cmake --preset "$preset"
  cmake --build --preset "$preset"
  ctest --preset "$preset" --output-on-failure
  cmake --install "build/$preset" --prefix "install/$preset"
done
```

两个 clang-tidy preset 还必须运行聚合目标：

```bash
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy
cmake --build --preset linux-clang-tidy-static --target ZzClangTidy
```

ASan/UBSan 运行时使用 preset 已写入的 `ASAN_OPTIONS` 与 `UBSAN_OPTIONS`。不得关闭 leak 检查后把结果登记为同一门禁。

## Windows MSVC preset

在 Visual Studio 2022 x64 Developer PowerShell 中设置 `QT_MSVC_ROOT`：

```powershell
$env:QT_MSVC_ROOT = '<qt-msvc-root>'

foreach ($preset in @(
    'windows-msvc2022-release',
    'windows-msvc2022-static')) {
    cmake --preset $preset
    cmake --build --preset $preset
    ctest --preset $preset --output-on-failure
    cmake --install "build/$preset" --config Release --prefix "install/$preset"
}
```

## Windows MinGW preset

在 PowerShell 7 中设置同一 Qt SDK 的五个变量：

```powershell
$env:QT_SDK_ROOT = '<qt-sdk-root>'
$env:QT_MINGW_ROOT = '<qt-mingw-root>'
$env:QT_MINGW_TOOLCHAIN_ROOT = '<matching-mingw-root>'
$env:QT_MINGW_EXPECTED_GCC_VERSION = 'the-kit-version'
$env:NINJA_EXE = '<qt-sdk-root>/Tools/Ninja/ninja.exe'

pwsh -NoProfile -File scripts/ci/Assert-QtMinGWKit.ps1
foreach ($preset in @('windows-mingw-release', 'windows-mingw-static')) {
    cmake --preset $preset
    cmake --build --preset $preset
    ctest --preset $preset --output-on-failure
    cmake --install "build/$preset" --prefix "install/$preset"
}
```

`Assert-QtMinGWKit.ps1` 会验证 target triple、GCC 精确版本、qmake prefix、`win32-g++` xspec、Ninja 和受控 SDK 根。验证失败不得绕过。

## macOS preset

分别设置两个架构的 Qt SDK；Rosetta 不会把 arm64 Qt framework 变成可链接的 x86_64 SDK：

```bash
export QT_MACOS_ARM64_ROOT=/path/to/qt-arm64
export QT_MACOS_X86_64_ROOT=/path/to/qt-x86_64
export APPLE_CLANG=/usr/bin/clang
export APPLE_CLANGXX=/usr/bin/clang++

for preset in \
  macos-clang-release-arm64 \
  macos-clang-release-x86_64 \
  macos-clang-static-arm64 \
  macos-clang-static-x86_64; do
  cmake --preset "$preset"
  cmake --build --preset "$preset"
  cmake --build --preset "$preset" --target ZzClangTidy
  ctest --preset "$preset" --output-on-failure
  cmake --install "build/$preset" --prefix "install/$preset"
done
```

## 原生平台 runner

三平台聚合入口如下。命令输出应保存到 `build/gate-evidence/`，该目录不进入 Git：

```bash
bash scripts/ci/run-linux-gates.sh
bash scripts/ci/run-macos-gates.sh
```

```powershell
pwsh -NoProfile -File scripts/ci/run-windows-gates.ps1
```

当前 Linux runner 直接在活动本机参考环境运行 GCC shared/static/LTO、Clang 检查、sanitizer 和性能比较。只有设置合法 `ZZ_UBUNTU2204_BUILD_IMAGE` 时才追加 `scripts/ci/run-ubuntu2204-release-gates.sh`；原 Ubuntu 22.04 档案与本机档案不得混用。

## 安装消费与重定位

每个完整 CTest 都包含外部消费路径。定向执行：

```bash
ctest --test-dir build/linux-gcc-release \
  -R '^platform\.package-relocation$' --output-on-failure
```

该测试执行以下独立步骤：

1. producer 配置、构建并安装到 prefix A。
2. 将 prefix A 复制到 prefix B 后删除 prefix A。
3. `tests/InstallConsumer` 只从 prefix B 与原 Qt prefix 配置、构建和运行。
4. `tests/PublicHeaderConsumer` 逐个编译所有已安装公开头。
5. 扫描已安装 CMake 文件，拒绝源码、构建、prefix A/B、Qt SDK 或开发者目录泄漏。

普通 `install.consumer` 也会构建外部消费者，但发布和平台门禁以同时覆盖 InstallConsumer/PublicHeaderConsumer 的 `platform.package-relocation` 为准。

## 正式发布配置

`ZZ_RELEASE_BUILD=ON` 是失败关闭的正式发布模式。仓库根 `LICENSE` 已固定为 MIT；除普通工具链变量外，还必须提供具名项目所有者批准记录、清空并通过审核的两个 manifest，以及 `ZZ_RELEASE_EVIDENCE_ROOT` 中逐字节匹配的外部证据。Linux 捆绑 GNU runtime 时还需要：

```bash
cmake -S . -B build/release-audit -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DZZ_RELEASE_BUILD=ON \
  -DZZ_RELEASE_EVIDENCE_ROOT="$ZZ_RELEASE_EVIDENCE_ROOT" \
  -DZZ_BUNDLE_GNU_RUNTIME=ON \
  -DZZ_GNU_RUNTIME_LICENSE_DIR=/path/to/reviewed-gcc-licenses
```

证据不足时配置失败是正确结果。开发构建不要启用该选项，也不得用 `ZZ_RELEASE_FORCED_BLOCKERS` 处理生产构建；该变量只允许发布契约测试追加失败项。
