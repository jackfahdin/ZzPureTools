# ZzPureToolsFrame 构建手册

本手册描述从源码配置、编译、测试到安装消费的完整流程。所有命令均在仓库根目录执行；`build/` 和 `install/` 下的目录是可删除的本机构建产物，不应提交。

## 快速导航

| 目标 | 推荐入口 | 适用场景 |
|---|---|---|
| Linux 日常开发 | `linux-gcc-debug` | GCC Debug、完整单元测试和示例调试 |
| Linux 发布候选 | `linux-gcc-release`、`linux-static-release` | GCC shared/static Release 和安装消费 |
| Linux 持续发布包 | `linux-continuous-release` | Ubuntu 22.04 AppImage、shared、LTO 和部署 smoke |
| Linux 质量检查 | `linux-clang-tidy-release`、`linux-clang-asan` | clang-tidy、ASan/UBSan 和边界检查 |
| Windows 动态库 | `windows-msvc2022-release`、`windows-mingw-release` | MSVC 或 Qt 官方 MinGW shared 构建 |
| Windows 静态库 | `windows-msvc2022-static`、`windows-mingw-static` | 对应 ABI 的 static 构建 |
| Windows 持续发布包 | `windows-msvc2022-continuous`、`windows-mingw-continuous` | 两种 ABI 各自部署和生成 ZIP |
| macOS | `macos-clang-release-*`、`macos-clang-static-*` | Apple Clang 的 arm64/x86_64 构建 |
| macOS 持续发布包 | `macos-continuous-arm64`、`macos-continuous-x86_64` | 两种原生架构各自生成 DMG |

构建并安装后，外部项目使用 `find_package(ZzPureToolsFrame 0.1 CONFIG REQUIRED)`，链接目标仍为 `Zz::Core`、`Zz::WindowKit`、`Zz::FluentUI` 和 `Zz::PureTools`。

## 构建事实源

根 `CMakeLists.txt` 定义项目选项、依赖和安装规则，`CMakePresets.json` 定义受支持的平台矩阵。`CMakeUserPresets.json.example` 只展示如何从父进程环境传值；本机可将其内容用于 `CMakeUserPresets.json`，后者已由 `.gitignore` 排除，不得提交本机 SDK 绝对路径。

除明确写入性能参考档案的受审环境外，构建文档、preset 和已安装 CMake 文件都不得保存开发者绝对路径。

如果某个 preset 曾在没有设置编译器或 Qt 变量时配置失败，不要只在同一个目录中补变量后
重复配置。CMake 会保留失败探测结果，例如 `CMAKE_EXECUTABLE_FORMAT=Unknown`，随后
Ninja 可能在生成安装规则时报告无法修改 RPATH。此时应删除或移动该 preset 对应的
精确 `build/<preset>` 目录，再从干净目录重新配置；不要删除整个 `build/` 根目录。

## 先决条件

所有平台需要：

- CMake 3.23 或更高版本。
- Qt 6.8 或更高版本，包含 Core、Gui、Widgets、Svg、Concurrent；测试还需要 Qt Test。
- 能执行 C++20 `<format>` 和 `<source_location>` 的标准库；协作取消统一使用 ZzCore 的 `ZzStopToken`，避免 Apple libc++ 尚未提供标准停止令牌时破坏跨平台构建。
- Git 和对应生成器；Ninja preset 需要 Ninja。
- shared 与 static 必须使用同一平台和同一 ABI 的 Qt 开发包。

编译器下限：

| 平台 | 编译器下限 | 额外要求 |
|---|---|---|
| Linux GCC | GCC/G++ 13.1 | 发布参考档案可使用更高版本，但必须记录精确身份 |
| Linux Clang | Clang/Clang++ 17 | `GCC_13_TOOLCHAIN_ROOT` 必须提供兼容的 libstdc++ 工具链 |
| Windows MSVC | Visual Studio 2022，MSVC 19.38 | 从 x64 Developer PowerShell 执行 |
| Windows MinGW | Qt SDK 随附的 MinGW-w64 GCC 13+ | Qt kit、工具链和 Ninja 必须来自同一官方 Qt SDK |
| macOS | Apple Clang 15 | deployment target 最低为 macOS 13.3，以满足 Apple libc++ 的 C++20 format 运行库要求 |

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
  cmake --preset "$preset" \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF
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

ASan/UBSan 运行时使用 preset 已写入的 `ASAN_OPTIONS`、`LSAN_OPTIONS` 与
`UBSAN_OPTIONS`。LeakSanitizer 保持 `detect_leaks=1`，仅通过
`cmake/ZzLeakSanitizer.supp` 排除 Qt 注册应用字体时由系统 Fontconfig 保留的
进程级配置缓存。LSan 单独使用 `symbolize=0`，避免 LLVM 20 在匹配该系统库堆栈时
阻塞；ASan/UBSan 的符号化保持启用。不得扩展为项目命名空间或关闭 leak 检查后把
结果登记为同一门禁。

### 综合示例本地预览素材

`ZzPureToolsExample` 可在本机临时加载不进入仓库的首页和卡片预览图。目录必须包含
`home.png`、`card-performance.png`、`card-windowing.png` 和 `card-data.png`，配置时
显式传入：

```bash
cmake --preset linux-gcc-release \
  -DZZ_EXAMPLE_LOCAL_ASSET_DIR="$PWD/build/local-assets/ZzPureToolsExample"
cmake --build --preset linux-gcc-release --target ZzPureToolsExample
```

该目录应放在已忽略的 `build/` 下。提供目录时四个文件缺少任意一个都会配置失败；
未提供时示例继续使用确定性 palette 预览，不影响其他开发者、CI 或发布构建。临时
上述临时首页与卡片 PNG 不得提交，替换为经过来源审核的正式原创文件后，必须重新
生成综合示例视觉基线并记录逐文件 SHA-256。已经批准进入生产资源包的字体和 SVG
不受该临时 PNG 限制，其所有权、固定摘要和变更规则见
[`ICON_ASSETS_ZH.md`](ICON_ASSETS_ZH.md)。

## Windows MSVC preset

在 Visual Studio 2022 x64 Developer PowerShell 中设置 `QT_MSVC_ROOT`：

```powershell
$env:QT_MSVC_ROOT = '<qt-msvc-root>'

foreach ($preset in @(
    'windows-msvc2022-release',
    'windows-msvc2022-static')) {
    cmake --preset $preset
    cmake --build --preset $preset
    ctest --preset $preset -C Release --output-on-failure
    cmake --install "build/$preset" --config Release --prefix "install/$preset"
}
```

只编译示例而跳过测试时，在配置命令追加
`-DZZ_BUILD_TESTS=OFF -DZZ_BUILD_EXAMPLES=ON`，然后将构建目标改为
`cmake --build --preset windows-msvc2022-release --target ZzPureToolsExample`。
MSVC 工程必须在 x64 Developer PowerShell 中执行，源码和 Qt SDK 的字符集统一为
UTF-8；不要把 MSVC 生成的库与 MinGW 目录混用。

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
    ctest --preset $preset -C Release --output-on-failure
    cmake --install "build/$preset" --prefix "install/$preset"
}
```

`Assert-QtMinGWKit.ps1` 会验证 target triple、GCC 精确版本、qmake prefix、`win32-g++` xspec、Ninja 和受控 SDK 根。验证失败不得绕过。
MinGW 静态构建仍需要 Qt kit 提供与编译器匹配的静态库；若 SDK 只有 shared Qt，配置阶段应明确失败。

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
  ctest --preset "$preset" --output-on-failure
  cmake --install "build/$preset" --prefix "install/$preset"
done
```

`ZzClangTidy` 只在启用 clang-tidy 的 preset 中执行：

```bash
cmake --build --preset macos-clang-release-arm64 --target ZzClangTidy
```

每个 macOS preset 的 `CMAKE_OSX_ARCHITECTURES` 必须与 Qt SDK 架构一致；使用
`file` 或 `lipo -archs` 检查最终库和示例的架构，不能用 Rosetta 结果替代原生验证。

## 原生平台 runner

三平台聚合入口如下。命令输出应保存到 `build/gate-evidence/`，该目录不进入 Git：

```bash
bash scripts/ci/run-linux-gates.sh
bash scripts/ci/run-macos-gates.sh
```

```powershell
pwsh -NoProfile -File scripts/ci/run-windows-gates.ps1
```

当前 Linux runner 直接在活动本机参考环境运行 GCC shared/static/LTO、Clang 检查、sanitizer、四示例编译与 offscreen 冒烟，以及性能比较。Windows 和 macOS runner 在每个 shared/static 组合中编译四个示例，但不将自动构建记录为真机交互结果。只有设置合法 `ZZ_UBUNTU2204_BUILD_IMAGE` 时才追加 `scripts/ci/run-ubuntu2204-release-gates.sh`；原 Ubuntu 22.04 档案与本机档案不得混用。

Linux runner 的编译器下限负向合同仅在主机存在 `g++-12` 时执行，并验证配置明确
拒绝旧编译器。主机没有 `g++-12` 时输出 `compiler capabilities contract not
executed`，结论是“跳过”，不是“通过”。同理，未设置 `ZZ_UBUNTU2204_BUILD_IMAGE`
时 runner 可以完成活动本机档案，但 `ubuntu2204-github-ci` 必须继续登记为
`pending-user-validation`，不能由本机 Ubuntu 26.04 结果替代。

## Continuous Build 本地复现

固定下载页是
<https://github.com/jackfahdin/ZzPureTools/releases/tag/continuous-build>。以下命令与远端
五个平台通道使用相同 preset 和打包脚本。各 `output-dir` 必须事先存在且为空，
`commit` 必须是 40 位小写提交，`built-at-utc` 必须是 UTC 时间。打包结果固定为
一个包、一个相邻 `SHA-256` 文件和一个 `build-info.json`。

发布证据目录需要先用公共脚本准备。每个平台应使用自己的空目录：

```bash
export ZZ_RELEASE_EVIDENCE_ROOT="$PWD/build/local-continuous/evidence"
mkdir -p "$ZZ_RELEASE_EVIDENCE_ROOT"
cmake "-DZZ_OUTPUT_DIR=$ZZ_RELEASE_EVIDENCE_ROOT" \
  -P scripts/package/PrepareReleaseEvidence.cmake
```

### Ubuntu 22.04 x86_64 AppImage

AppImage 脚本只接受 Ubuntu 22.04。除项目编译环境外，还需要经过固定摘要校验的
`linuxdeploy`、`linuxdeploy-plugin-qt`、`appimagetool`，以及包含 `COPYING3` 和
`COPYING.RUNTIME` 的 GNU runtime 许可证目录：

```bash
export ZZ_GNU_RUNTIME_LICENSE_DIR=/path/to/reviewed-gcc-licenses
export ZZ_APPIMAGE_TOOLS=/path/to/verified-appimage-tools
export ZZ_ARTIFACT_DIR="$PWD/build/local-continuous/linux-artifacts"
export ZZ_COMMIT="$(git rev-parse HEAD)"
export ZZ_BUILT_AT_UTC="$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
mkdir -p "$ZZ_ARTIFACT_DIR"

cmake --preset linux-continuous-release \
  "-DZZ_RELEASE_EVIDENCE_ROOT=$ZZ_RELEASE_EVIDENCE_ROOT" \
  "-DZZ_GNU_RUNTIME_LICENSE_DIR=$ZZ_GNU_RUNTIME_LICENSE_DIR"
cmake --build --preset linux-continuous-release --parallel 2
ctest --preset linux-continuous-release --output-on-failure

scripts/package/package-linux-appimage.sh \
  --build-dir "$PWD/build/linux-continuous-release" \
  --qt-root "$QT_ROOT" \
  --evidence-root "$ZZ_RELEASE_EVIDENCE_ROOT" \
  --gnu-license-dir "$ZZ_GNU_RUNTIME_LICENSE_DIR" \
  --linuxdeploy "$ZZ_APPIMAGE_TOOLS/linuxdeploy-x86_64.AppImage" \
  --qt-plugin "$ZZ_APPIMAGE_TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage" \
  --appimagetool "$ZZ_APPIMAGE_TOOLS/appimagetool-x86_64.AppImage" \
  --output-dir "$ZZ_ARTIFACT_DIR" \
  --commit "$ZZ_COMMIT" \
  --built-at-utc "$ZZ_BUILT_AT_UTC"
```

### Windows MSVC 2022 x86_64 ZIP

在 Visual Studio 2022 x64 Developer PowerShell 中执行：

```powershell
$preset = 'windows-msvc2022-continuous'
$evidenceRoot = "$PWD/build/local-continuous/msvc-evidence"
$artifactDir = "$PWD/build/local-continuous/msvc-artifacts"
$commit = (git rev-parse HEAD).Trim()
$builtAtUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
New-Item -ItemType Directory -Force -Path $evidenceRoot, $artifactDir |
    Out-Null
cmake "-DZZ_OUTPUT_DIR=$evidenceRoot" `
    -P scripts/package/PrepareReleaseEvidence.cmake

cmake --preset $preset "-DZZ_RELEASE_EVIDENCE_ROOT=$evidenceRoot"
cmake --build --preset $preset --parallel 2
ctest --preset $preset --output-on-failure
pwsh -NoProfile -File scripts/package/package-windows.ps1 `
    -Mode msvc `
    -BuildDir "$PWD/build/$preset" `
    -QtRoot $env:QT_MSVC_ROOT `
    -EvidenceRoot $evidenceRoot `
    -OutputDir $artifactDir `
    -Commit $commit `
    -BuiltAtUtc $builtAtUtc `
    -DumpBin (Get-Command dumpbin.exe -ErrorAction Stop).Source
```

### Windows Qt MinGW x86_64 ZIP

先按“Windows MinGW preset”一节设置同一 Qt SDK 的环境变量并运行 kit 检查：

```powershell
$preset = 'windows-mingw-continuous'
$evidenceRoot = "$PWD/build/local-continuous/mingw-evidence"
$artifactDir = "$PWD/build/local-continuous/mingw-artifacts"
$commit = (git rev-parse HEAD).Trim()
$builtAtUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
New-Item -ItemType Directory -Force -Path $evidenceRoot, $artifactDir |
    Out-Null
cmake "-DZZ_OUTPUT_DIR=$evidenceRoot" `
    -P scripts/package/PrepareReleaseEvidence.cmake
pwsh -NoProfile -File scripts/ci/Assert-QtMinGWKit.ps1

cmake --preset $preset "-DZZ_RELEASE_EVIDENCE_ROOT=$evidenceRoot"
cmake --build --preset $preset --parallel 2
ctest --preset $preset --output-on-failure
pwsh -NoProfile -File scripts/package/package-windows.ps1 `
    -Mode mingw `
    -BuildDir "$PWD/build/$preset" `
    -QtRoot $env:QT_MINGW_ROOT `
    -EvidenceRoot $evidenceRoot `
    -OutputDir $artifactDir `
    -Commit $commit `
    -BuiltAtUtc $builtAtUtc `
    -ObjDump "$env:QT_MINGW_TOOLCHAIN_ROOT/bin/objdump.exe"
```

### macOS arm64 与 x86_64 DMG

在对应原生架构主机上设置匹配的 Qt SDK。下面的函数每次只处理一个架构：

```bash
package_macos() {
  architecture=$1
  qt_root=$2
  preset="macos-continuous-$architecture"
  evidence_root="$PWD/build/local-continuous/macos-$architecture-evidence"
  artifact_dir="$PWD/build/local-continuous/macos-$architecture-artifacts"
  commit=$(git rev-parse HEAD)
  built_at_utc=$(date -u +'%Y-%m-%dT%H:%M:%SZ')
  mkdir -p "$evidence_root" "$artifact_dir"
  cmake "-DZZ_OUTPUT_DIR=$evidence_root" \
    -P scripts/package/PrepareReleaseEvidence.cmake

  cmake --preset "$preset" \
    "-DZZ_RELEASE_EVIDENCE_ROOT=$evidence_root"
  cmake --build --preset "$preset" --parallel 2
  ctest --preset "$preset" --output-on-failure
  scripts/package/package-macos.sh \
    --build-dir "$PWD/build/$preset" \
    --qt-root "$qt_root" \
    --evidence-root "$evidence_root" \
    --output-dir "$artifact_dir" \
    --commit "$commit" \
    --built-at-utc "$built_at_utc" \
    --architecture "$architecture"
}

package_macos arm64 "$QT_MACOS_ARM64_ROOT"
package_macos x86_64 "$QT_MACOS_X86_64_ROOT"
```

上述命令用于复现原生构建和部署 smoke。CI smoke 不等于真机验收；DMG 挂载、ZIP
离线启动或 AppImage 的 Xvfb 启动成功，都不能替代目标桌面的人工交互清单。

## GitHub Actions CI/CD

`.github/workflows/ci.yml` 只为桌面应用运行五个实际发布 preset：
`linux-continuous-release`、`windows-msvc2022-continuous`、
`windows-mingw-continuous`、`macos-continuous-arm64` 和
`macos-continuous-x86_64`。全部通过后，唯一发布 job 才更新固定 Pre-release；PR 只
验证，不发布。它不执行本机性能基线，也不替代真机验收。工作流结构、Action 固定
摘要、发布事务和远端处理流程见 `docs/development/GITHUB_ACTIONS_ZH.md`。

Qt 采用集中升级规则：五个平台统一读取 workflow 顶层 `QT_VERSION`，不得在 job 中
分别写版本。升级时只修改该值，同时审查 Qt 可用架构、MinGW 工具链、截图跨 minor
容差和许可证，再让完整五平台 workflow 重新通过；不得只验证一个平台后发布。

上传 GitHub 前可在任意具备 CMake 3.23+ 的环境运行静态契约：

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzGitHubActionsContract.cmake
```

该命令只验证工作流结构和关键门禁是否存在，不会模拟 GitHub runner。在同一提交的
完整远端矩阵成功前，Windows、macOS 和 GitHub Ubuntu 的平台状态仍保持“未执行”。

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

### 工作区组件质量门禁

工作区组件使用 `linux-gcc-benchmarks` 的 Release/shared/LTO 档位。下列命令覆盖
公开安装消费、完整架构审计、四档 DPR 截图与 observe 基准：

```bash
cmake --build --preset linux-gcc-benchmarks --target \
  ZzFluentTitleBarTest ZzCommandPaletteTest ZzDockPanelTest \
  ZzTabControlsTest ZzWorkspaceShellTest --parallel 2
ctest --preset linux-gcc-benchmarks \
  -R 'fluent\.(tab-controls|command-palette|dock-panel|title-bar)|puretools\.workspace-shell' \
  --output-on-failure
ctest --preset linux-gcc-benchmarks \
  -R '^fluent\.screenshot-(100|125|150|200)$|^benchmark\.workspace-components$|^architecture\.complete-audit$|^platform\.package-relocation$' \
  --output-on-failure
```

`ZzFluentInstallConsumer` 会从安装前缀构造标题栏、Activity/Side、Explorer、
Command Palette、Dock、Tab 和最小 `ZzWorkspaceShell`。`PublicHeaderConsumer`
逐个编译全部安装头，并显式要求 14 个工作区公开头存在。外部消费者与其 Qt
依赖必须只从 relocation 后的 prefix B 解析。

六入口工作区、右侧空态、组件导航、逐窗口设置与对象预算可用以下定向命令复验：

```bash
cmake --preset linux-gcc-benchmarks \
  -DZZ_BUILD_EXAMPLES=ON -DZZ_BUILD_BENCHMARKS=ON \
  -DZZ_PERFORMANCE_REFERENCE:BOOL=ON
cmake --build --preset linux-gcc-benchmarks --parallel 2 --target \
  ZzWorkspaceScreenshotTest ZzWorkspaceComponentsBenchmark \
  ZzExampleWorkspaceSmokeTest ZzPureToolsExample
ctest --preset linux-gcc-benchmarks -j1 --output-on-failure \
  -R '^puretools\.workspace-screenshot-(100|125|150|200)$|^example\.workspace-smoke$|^benchmark\.workspace-components$'
cmake --preset linux-gcc-debug -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2 --target ZzPureToolsExample
ctest --preset linux-gcc-debug --parallel 4 --output-on-failure \
  -R '^example\.puretools-screenshot-(100|125|150|200)$'
```

截图比较必须关闭 `ZZ_UPDATE_SCREENSHOTS` 和 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS`；
只有在已审参考机上重建基线时才临时启用对应变量。`workspace-components` 报告中的
`activity-row-widgets`、`fixed-action-steady-object-growth` 和
`right-empty-layout-width` 必须为 0，`single-side-visible-panels` 不得大于 1。

## 常见问题

### Qt 版本或私有头不匹配

项目要求配置时发现的 Qt 主版本和次版本与构建目标一致。`ZzWindowKit` 的静态
构建还需要同一 Qt SDK 提供 `Qt6::GuiPrivate`；缺少对应开发文件时应安装与 Qt
版本完全匹配的私有开发包，不能把另一个 Qt 版本的头目录临时加入
`CMAKE_PREFIX_PATH`。

### 包名或安装路径不正确

安装结果应包含：

```text
<prefix>/lib/cmake/ZzPureToolsFrame/
  ZzPureToolsFrameConfig.cmake
  ZzPureToolsFrameConfigVersion.cmake
  ZzPureToolsFrameTargets.cmake
```

消费者只设置 `CMAKE_PREFIX_PATH=<prefix>`，不要直接引用构建目录中的生成文件。
如果同时存在多个 Qt 或多个安装前缀，先清理消费者的 CMakeCache，再使用
`cmake --debug-find-pkg=ZzPureToolsFrame` 检查实际命中的配置文件。

### Windows 编译器与字符集

MSVC 构建出现 fmt 的 UTF-8 静态断言时，确认 CMake 使用了 `/utf-8` 编译选项，并
从 Visual Studio 2022 x64 Developer PowerShell 重新配置。MinGW 构建出现 kit
检查失败时，重新运行 `Assert-QtMinGWKit.ps1`，不要把 MSYS2 的 Qt 或 Ninja 混入
官方 Qt SDK。

### macOS 架构不一致

`lipo -archs` 显示的架构必须与 preset 后缀一致。删除该 preset 的 build 目录后
重新配置，不能在同一目录中切换 arm64 与 x86_64，也不能用 Rosetta 运行结果代替
另一架构的构建验证。

## 正式发布配置

`ZZ_RELEASE_BUILD=ON` 是失败关闭的正式发布模式。仓库根 `LICENSE`、Jackfahdin 所有者批准记录和两个 manifest 已完成审核；除普通工具链变量外，仍必须让 `ZZ_RELEASE_EVIDENCE_ROOT` 指向逐字节匹配的外部来源证据。Linux 捆绑 GNU runtime 时还需要：

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
