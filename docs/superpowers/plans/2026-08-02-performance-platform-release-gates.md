# 性能、跨平台与发布门禁 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立可重复的 Linux 性能基线，以及 Linux、Windows MSVC、Windows Qt 官方 MinGW、macOS arm64/x86_64 的 shared/static 编译、静态检查、安装消费和发布合规门禁。

**Architecture:** 根 `CMakeLists.txt` 继续作为构建事实源，`CMakePresets.json` 只表达平台、工具链、shared/static 和检查选项的组合。性能程序输出带环境元数据的 JSON，参考机执行绝对阈值，普通 CI 只阻止相对回归；发布模式另行阻止未解决的第三方溯源与许可问题。

**Tech Stack:** CMake 3.23/CMakePresets schema 4、Qt 6.8+、C++20、CTest、Qt Test、GCC 13+、Clang 17+、MSVC 2022 19.38+、Qt SDK MinGW-w64 13+、Apple Clang 15+、clang-tidy、ASan/UBSan、JSON 性能报告。

---

## 前置条件和验证等级

- 必须按下文的 1 至 8 顺序执行；本计划是第 8 份，不补做前七份的组件功能。
- Linux 是第一阶段动态、Sanitizer、性能和真实窗口验证平台。
- Windows/macOS 第一阶段必须在原生 runner 编译所有条件分支，完成严格警告、公开头、安装消费和二进制依赖检查。
- Windows Snap Layout/DPI/多屏/材质和 macOS 原生按钮/全屏/Retina/blur 仍是人工真机验收，禁止把“编译通过”写成“功能验收通过”。
- qwindowkit 准确上游 commit 已完成技术核对，但在具名来源审核签署前，`ZZ_RELEASE_BUILD=ON` 必须失败。
- Qt 5.15.2 派生代码的上游字节和许可证已固定，但再分发结论尚未由具名审核人签署，因此必须继续阻止正式发布。
- 仓库根项目许可证已由负责人选择 MIT 并落地；在具名项目所有者批准记录签署前，它仍是独立发布阻断项。

## 八份计划的执行顺序

| 顺序 | 实施计划 | 必须完成的退出证据 |
|---|---|---|
| 1 | `docs/superpowers/plans/2026-08-02-repository-cmake-baseline.md` | Linux shared/static 均能 configure/build/test/install，全新 consumer 能链接六个导出 target |
| 2 | `docs/superpowers/plans/2026-08-02-zzlog-cxx20-compliance.md` | ZzLog C++20 API、overflow/flush 测试、shared/static 安装消费通过 |
| 3 | `docs/superpowers/plans/2026-08-02-zzcore-foundation.md` | Result、任务/取消、路径/设置、Qt 日志桥及 Core 依赖门禁通过 |
| 4 | `docs/superpowers/plans/2026-08-02-zzwindowkit-qwindowkit-adapter.md` | QWK 只存在于 private backend，Linux 原生窗口和 shared/static consumer 通过，vendor blocker 如实记录 |
| 5 | `docs/superpowers/plans/2026-08-02-zzfluent-foundation.md` | Foundation/Widgets 依赖边界、主题、有界缓存、500 控件基准通过 |
| 6 | `docs/superpowers/plans/2026-08-02-zzfluentui-basic-controls.md` | 基础控件、无障碍、截图、10 万行模型和控件稳定性门禁通过 |
| 7 | `docs/superpowers/plans/2026-08-02-zzpuretools-app-framework.md` | 模块图、启停回滚、页面/导航/多窗口和分层安装消费通过 |
| 8 | `docs/superpowers/plans/2026-08-02-performance-platform-release-gates.md` | 本计划的性能、原生平台、可重定位、二进制和发布证据门禁逐项完成 |

执行第 8 份计划前，实施者必须在实施日志中引用前七份计划最后一次绿灯命令的输出和 commit id。任一退出证据缺失时，停在本计划的 Task 1，不用本计划重写前置组件。

## 架构规范 1-20 节覆盖矩阵

| 规范节 | 主责计划 | 本计划的复验点 |
|---|---|---|
| 1 文档目的 | 1-8 | Task 9 保证构建/平台文档不改写硬约束 |
| 2 已确认目标 | 1、2、4-8 | Task 1/2 锁定 Qt 6.8+、C++20、shared/static 和平台工具链 |
| 3 总体架构 | 1、3-7 | Task 6 检查六个导出 target 和禁止反向依赖 |
| 4 仓库与目录 | 1 | Task 6/9 扫描安装路径、private 头和文档交付路径 |
| 5 C++ 代码规范 | 1-7 | Task 1 执行 clang-tidy，Task 6 执行命名/namespace/Doxygen/PIMPL 自动扫描 |
| 6 前后端分离 | 6、7 | Task 6 扫描 UI 对 repository/database/network/domain 的禁止依赖 |
| 7 ZzCore | 3 | Task 6 验证 Core 无 Qt Gui/Widgets，Task 10 复跑 sanitizer |
| 8 ZzLog | 2 | Task 1 探针 `format/source_location`，Task 6/7 验证 shared/static 二进制和安装依赖 |
| 9 ZzWindowKit | 4 | Task 6 禁止 QWK/Qt Private 泄漏，Task 7/9 分离自动编译和人工真机状态 |
| 10 ZzFluentUI | 5、6 | Task 4/5 验证主题、动画、大模型和缓存预算 |
| 11 ZzPureTools | 7 | Task 4 验证启动/窗口生命周期，Task 6 验证 AppCore/UI 边界 |
| 12 CMake 与 Preset | 1、2、4 | Task 1/2 增加能力探针和全平台 preset，Task 6 验证可重定位消费 |
| 13 平台支持 | 4、8 | Task 2/7 在 Linux、MSVC、Qt MinGW、macOS 原生 host 真正 configure/build/test |
| 14 性能预算 | 5-8 | Task 3-5/10 生成 JSON，分离参考机绝对阈值和普通 CI 相对回归 |
| 15 测试与质量 | 1-8 | Task 1-9 每个改动均有红/绿命令，Task 10 运行完整标签矩阵 |
| 16 分阶段实施 | 1-8 | 上表锁定阶段 1-8；Task 10 不越过未完成阶段 |
| 17 文档交付 | 4、8 | Task 5/8/9 交付性能、第三方、构建、平台和人工清单 |
| 18 Git 提交 | 1-8 | Task 1-9 只暂存精确文件并使用多个 `-m`；Task 10 不提交 |
| 19 实施者验收 | 1-8 | Task 6 将可自动条目编译为 CTest，Task 9 保留必须人工执行的条目 |
| 20 后续计划拆分 | 1-8 | 本节的八份顺序是唯一执行顺序，Task 10 仅做集成验证 |

## 工具链最低版本

| 平台 | 工具链 | 最低要求 | 关键理由 |
|---|---|---|---|
| Linux | GCC + libstdc++ | GCC 13.1 | GCC 12 的 libstdc++ 没有完整 `std::format` |
| Linux | Clang + GCC libstdc++ | Clang 17、GCC 13.1+ external toolchain，且标准库通过探针 | 固定与 Qt 二进制兼容的 libstdc++，编译器版本不能代替能力检查 |
| Windows | MSVC 2022 x64 | 19.38 / VS 17.8 | 覆盖 C++20 format/source_location/stop_token |
| Windows | Qt SDK MinGW-w64 x64 | GCC 13.1，必须与当前 Qt SDK 配套 | 不接受系统随机 MinGW 或 MSYS2 ABI 混用 |
| macOS | Apple Clang | Apple Clang 15，且 macOS 12 deployment target 编译/链接探针通过 | libc++ 能力与 deployment target 同时受约束 |

Ubuntu 22.04 是运行兼容基线，不意味着使用其默认 GCC 11 构建。Linux release/LTO 产物必须在 immutable-digest Ubuntu 22.04 build image 内使用 GCC 13.1+ 与为该基线构建的 Qt 6.8+ 生成；发布包明确携带同一 GCC 的 `libstdc++.so.6`/`libgcc_s.so.1` 和许可证，并验证动态加载器实际选中它们，不得拿宿主机产物或系统默认 libstdc++ 冒充兼容结果。

## 文件边界

### 编译和 Preset

- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Modify: `CMakeUserPresets.json.example`
- Create: `cmake/ZzCompilerCapabilities.cmake`
- Modify: `cmake/ZzCompilerWarnings.cmake`
- Modify: `cmake/ZzStaticAnalysis.cmake`
- Create: `.clang-tidy`
- Create: `tests/Platform/CompilerCapabilitiesContract.cmake`
- Create: `tests/Platform/PresetMatrixContract.cmake`
- Create: `scripts/ci/run-clang-tidy.sh`
- Create: `scripts/ci/Assert-QtMinGWKit.ps1`
- Create: `scripts/ci/run-linux-gates.sh`
- Create: `scripts/ci/run-ubuntu2204-release-gates.sh`
- Create: `scripts/ci/run-windows-gates.ps1`
- Create: `scripts/ci/run-macos-gates.sh`
- Create: `scripts/ci/check-ubuntu2204-runtime.sh`

### 性能基础设施

- Create: `benchmarks/CMakeLists.txt`
- Create: `benchmarks/common/ZzBenchmarkSample.h`
- Create: `benchmarks/common/ZzBenchmarkMetadata.h`
- Create: `benchmarks/common/ZzBenchmarkMetadata.cpp`
- Create: `benchmarks/common/ZzPerformanceReporter.h`
- Create: `benchmarks/common/ZzPerformanceReporter.cpp`
- Create: `benchmarks/ZzPerformanceReporterTest.cpp`
- Create: `benchmarks/testdata/performance-valid.json`
- Create: `benchmarks/testdata/performance-invalid.json`
- Create: `benchmarks/testdata/performance-regressed.json`
- Create: `benchmarks/ZzStartupProbe/CMakeLists.txt`
- Create: `benchmarks/ZzStartupProbe/main.cpp`
- Create: `benchmarks/ZzStartupBenchmark.cpp`
- Create: `benchmarks/ZzThemeSwitchBenchmark.cpp`
- Create: `benchmarks/ZzAnimationBenchmark.cpp`
- Create: `benchmarks/ZzLargeModelBenchmark.cpp`
- Create: `benchmarks/ZzWindowLifecycleBenchmark.cpp`
- Create: `ZzWindowKit/src/private/ZzWindowKitDiagnostics.h`
- Create: `ZzWindowKit/src/private/ZzWindowKitDiagnostics.cpp`
- Modify: `ZzWindowKit/src/private/ZzQWindowKitBackend.cpp`
- Modify: `ZzWindowKit/CMakeLists.txt`
- Create: `benchmarks/ZzIdleProbe/CMakeLists.txt`
- Create: `benchmarks/ZzIdleProbe/main.cpp`
- Create: `cmake/ZzVerifyPerformanceReport.cmake`
- Create: `cmake/ZzComparePerformanceReport.cmake`
- Create: `benchmarks/testdata/performance-mismatched-environment.json`
- Create: `docs/performance/PERFORMANCE_BASELINE_ZH.md`
- Create: `docs/performance/reference/linux/startup.json`
- Create: `docs/performance/reference/linux/theme-switch.json`
- Create: `docs/performance/reference/linux/animation.json`
- Create: `docs/performance/reference/linux/large-model.json`
- Create: `docs/performance/reference/linux/window-lifecycle.json`
- Create: `docs/performance/reference/linux/idle.json`

### 平台、发布与文档

- Create: `tests/Platform/CMakeLists.txt`
- Create: `tests/Platform/ZzPlatformCompileTest.cpp`
- Create: `tests/Platform/ZzBinaryDependencyCheck.cmake`
- Create: `tests/Platform/ZzPackageRelocationTest.cmake`
- Create: `tests/Platform/ZzPlatformGateContext.cmake.in`
- Create: `tests/Platform/ZzGateScriptContract.cmake`
- Create: `tests/PublicHeaderConsumer/CMakeLists.txt`
- Create: `tests/Architecture/ZzArchitectureAudit.cmake`
- Create: `tests/Architecture/ZzDocumentationAudit.cmake`
- Modify: `tests/Architecture/CMakeLists.txt`
- Create: `tests/Release/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `cmake/ZzArchitectureChecks.cmake`
- Modify: `cmake/ZzInstallPackage.cmake`
- Create: `cmake/ZzReleaseChecks.cmake`
- Create: `cmake/ZzExpectConfigureFailure.cmake`
- Create: `cmake/ZzVerifyInstalledLicenses.cmake`
- Create: `docs/development/CODING_STANDARD_ZH.md`
- Create: `docs/development/BUILDING_ZH.md`
- Create: `docs/development/PLATFORM_SUPPORT_ZH.md`
- Modify: `docs/third-party/THIRD_PARTY_NOTICES.md`
- Modify: `docs/third-party/qwindowkit-vendor.json`
- Create: `docs/third-party/release-evidence.json`
- Create: `docs/third-party/RELEASE_BLOCKERS_ZH.md`
- Create: `docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md`
- Create: `docs/release/MANUAL_MACOS_CHECKLIST_ZH.md`
- Create: `docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`
- Verify after owner approval: `LICENSE`

## Task 1: 在配置阶段锁定 C++20 和平台能力

**Files:**
- Create: `cmake/ZzCompilerCapabilities.cmake`
- Modify: `CMakeLists.txt`
- Modify: `cmake/ZzCompilerWarnings.cmake`
- Modify: `cmake/ZzStaticAnalysis.cmake`
- Create: `.clang-tidy`
- Create: `tests/Platform/CompilerCapabilitiesContract.cmake`
- Create: `scripts/ci/run-clang-tidy.sh`

- [ ] **Step 1: 写不合格编译器的红灯契约**

Create `tests/Platform/CompilerCapabilitiesContract.cmake`:

```cmake
foreach(required ZZ_SOURCE_DIR ZZ_QT_PREFIX ZZ_REJECTED_CXX ZZ_WORK_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ZZ_WORK_DIR NORMALIZE OUTPUT_VARIABLE work_dir)
cmake_path(IS_PREFIX ZZ_SOURCE_DIR "${work_dir}" NORMALIZE work_is_in_source)
if(work_is_in_source)
    message(FATAL_ERROR "ZZ_WORK_DIR must not be inside the source tree")
endif()
file(REMOVE_RECURSE "${work_dir}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${ZZ_SOURCE_DIR}"
        -B "${work_dir}"
        -G Ninja
        "-DCMAKE_CXX_COMPILER=${ZZ_REJECTED_CXX}"
        "-DCMAKE_PREFIX_PATH=${ZZ_QT_PREFIX}"
        -DZZ_BUILD_TESTS=OFF
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_stdout
    ERROR_VARIABLE configure_stderr
)
set(configure_output "${configure_stdout}\n${configure_stderr}")
if("${configure_result}" EQUAL 0)
    message(FATAL_ERROR "Unsupported compiler was accepted")
endif()
if(NOT "${configure_output}" MATCHES "requires GCC 13\\.1 or newer")
    message(FATAL_ERROR
        "Configuration failed for the wrong reason:\n${configure_output}")
endif()
message(STATUS "Unsupported GCC was rejected by the explicit version gate")
```

Run on the provisioned Linux builder, which must contain both GCC 12 and GCC 13.1 or newer:

```bash
cmake \
  -DZZ_SOURCE_DIR="$PWD" \
  -DZZ_QT_PREFIX="$QT_ROOT" \
  -DZZ_REJECTED_CXX="$(command -v g++-12)" \
  -DZZ_WORK_DIR="/tmp/zzpuretoolspro-gcc12-contract" \
  -P tests/Platform/CompilerCapabilitiesContract.cmake
```

Expected: FAIL，且最外层错误是 `Unsupported compiler was accepted` 或 `Configuration failed for the wrong reason`；这证明尚无明确的 GCC 13.1 门禁。

- [ ] **Step 2: 创建编译器版本和标准库探针**

Create `cmake/ZzCompilerCapabilities.cmake` with:

```cmake
include_guard(GLOBAL)
include(CheckCXXSourceCompiles)

function(zz_check_compiler_capabilities)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 13.1)
            message(FATAL_ERROR "ZzPureToolsPro requires GCC 13.1 or newer")
        endif()
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 17.0)
            message(FATAL_ERROR "ZzPureToolsPro requires Clang 17 or newer")
        endif()
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
        if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 15.0)
            message(FATAL_ERROR "ZzPureToolsPro requires Apple Clang 15 or newer")
        endif()
    elseif(MSVC)
        if("${MSVC_VERSION}" LESS 1938)
            message(FATAL_ERROR "ZzPureToolsPro requires MSVC 19.38 or newer")
        endif()
    else()
        message(FATAL_ERROR
            "Unsupported C++ compiler: ${CMAKE_CXX_COMPILER_ID} "
            "${CMAKE_CXX_COMPILER_VERSION}")
    endif()

    set(saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    if(MSVC)
        string(APPEND CMAKE_REQUIRED_FLAGS " /std:c++20 /Zc:__cplusplus")
    else()
        string(APPEND CMAKE_REQUIRED_FLAGS " -std=c++20")
    endif()

    check_cxx_source_compiles([[
        #include <format>
        #include <source_location>
        #include <stop_token>
        #include <string>
        int main() {
            std::stop_source source;
            const auto location = std::source_location::current();
            const std::string text = std::format("{}:{}", location.line(), 42);
            return source.stop_requested() || text.empty();
        }
    ]] ZZ_HAS_REQUIRED_CXX20_LIBRARY)

    set(CMAKE_REQUIRED_FLAGS "${saved_required_flags}")
    if(NOT ZZ_HAS_REQUIRED_CXX20_LIBRARY)
        message(FATAL_ERROR
            "The selected C++ standard library lacks format/source_location/stop_token")
    endif()
endfunction()
```

根 `CMakeLists.txt` 在首次 `find_package(Qt6 ...)` 之前先建立模块路径，再 include 并调用该函数。Apple 分支必须使用以下完整设置，保证 `.mm` 一方源码也是 C++20 且关闭扩展：

```cmake
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(APPLE)
    enable_language(OBJCXX)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET
       OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 12.0)
        message(FATAL_ERROR "macOS deployment target must be 12.0 or newer")
    endif()
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
endif()

include(ZzCompilerCapabilities)
zz_check_compiler_capabilities()
```

- [ ] **Step 3: 扩展显式一方源码的严格警告**

保留基线中的 public `zz_enable_project_warnings()` 和显式 source list 契约，只替换其调用的 internal `zz_apply_first_party_warnings()`。完整实现如下；不要退回到从 target 隐式收集并静默忽略空列表的弱契约：

```cmake
function(zz_apply_first_party_warnings target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_WARNINGS "" "" "SOURCES")
    if(NOT ZZ_WARNINGS_SOURCES)
        message(FATAL_ERROR
            "zz_apply_first_party_warnings(${target_name}) requires SOURCES")
    endif()

    if(MSVC)
        set(zz_warning_options
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
        )
        if(ZZ_WARNINGS_AS_ERRORS)
            list(APPEND zz_warning_options /WX)
        endif()
        if(ZZ_ENABLE_MSVC_ANALYZE)
            list(APPEND zz_warning_options /analyze)
        endif()
    else()
        set(zz_warning_options
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        )
        if(ZZ_WARNINGS_AS_ERRORS)
            list(APPEND zz_warning_options -Werror)
        endif()
    endif()

    foreach(zz_source IN LISTS ZZ_WARNINGS_SOURCES)
        set_property(SOURCE "${zz_source}" APPEND PROPERTY
            COMPILE_OPTIONS ${zz_warning_options})
    endforeach()
endfunction()
```

`zz_enable_project_warnings()` 仍从已声明 source list 中筛选 `.cc/.cpp/.cxx/.mm` 后调用此 internal helper。`mocs_compilation.cpp`、RCC 输出和 vendored QWindowKit/spdlog 源不在传入的一方翻译单元列表中，因而不承受 `/WX`、`/analyze` 或 `-Werror`。任一后续 CMake 不得在调用 public helper 后再追加一方源文件。

- [ ] **Step 4: 增加正向 clang-tidy 过滤和 MSVC analyze 选项**

Add to the root option block in `CMakeLists.txt`:

```cmake
option(ZZ_ENABLE_MSVC_ANALYZE
    "Run MSVC code analysis on explicit first-party sources"
    OFF)
```

Step 3 已把 `/analyze` 限定到显式一方翻译单元；MinGW 继续使用 `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`。Create `.clang-tidy` with:

```yaml
Checks: >-
  clang-analyzer-*,
  bugprone-*,
  performance-*,
  modernize-use-nullptr,
  modernize-use-override,
  modernize-use-using,
  -bugprone-easily-swappable-parameters
WarningsAsErrors: ''
HeaderFilterRegex: '(^|.*/)((ZzCore|ZzWindowKit|ZzFluentUI|ZzPureTools)/(include|src|foundation|widgets|appcore)|(tests|benchmarks)/.*|ZzThirdParty/ZzLog/(include|src))/.*'
FormatStyle: file
```

LLVM regex 不支持 negative lookahead，因此只使用上述正向组件路径。不设置全局 `CMAKE_CXX_CLANG_TIDY`；全局属性会扫描 MOC/RCC 和第三方源。

Create `scripts/ci/run-clang-tidy.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <source-dir> <build-dir> <run-clang-tidy> <clang-tidy>" >&2
  exit 64
fi

source_dir=$(cd "$1" && pwd -P)
build_dir=$(cd "$2" && pwd -P)
runner=$3
tidy=$4
source_dir_regex=$(printf '%s\n' "$source_dir" \
  | sed 's/[][\\.^$*+?(){}|]/\\&/g')
source_regex="^${source_dir_regex}/((ZzCore|ZzWindowKit|ZzFluentUI|ZzPureTools)/.*|(tests|benchmarks)/.*|ZzThirdParty/ZzLog/src/.*)\\.(cc|cpp|cxx|mm)$"
header_regex="^${source_dir_regex}/((ZzCore|ZzWindowKit|ZzFluentUI|ZzPureTools)/(include|src|foundation|widgets|appcore)|(tests|benchmarks)/.*|ZzThirdParty/ZzLog/(include|src))/.*"

exec "$runner" \
  -p "$build_dir" \
  -clang-tidy-binary "$tidy" \
  -header-filter "$header_regex" \
  -warnings-as-errors '*' \
  "$source_regex"
```

在 `cmake/ZzStaticAnalysis.cmake` 中替换基线的 `zz_register_clang_tidy()`，继续使用唯一 canonical target `ZzClangTidy`；不得再创建大小写不同的第二套 `zz-clang-tidy`。调用者仍必须给出显式 source list 以锁定 CMake 契约，但 aggregate target 通过 compilation database 的正向路径统一覆盖组件源码、测试源码和一方 `ZzLog/src`：

```cmake
include_guard(GLOBAL)

function(zz_register_clang_tidy target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_TIDY "" "" "SOURCES")
    if(NOT ZZ_ENABLE_CLANG_TIDY)
        return()
    endif()
    if(NOT PROJECT_IS_TOP_LEVEL)
        return()
    endif()
    if(NOT ZZ_TIDY_SOURCES)
        message(FATAL_ERROR
            "zz_register_clang_tidy(${target_name}) requires SOURCES")
    endif()
    if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        message(FATAL_ERROR
            "ZZ_ENABLE_CLANG_TIDY requires CMAKE_EXPORT_COMPILE_COMMANDS=ON")
    endif()

    if(NOT TARGET ZzClangTidy)
        find_program(ZZ_BASH_EXECUTABLE NAMES bash REQUIRED)
        find_program(ZZ_RUN_CLANG_TIDY
            NAMES
                run-clang-tidy-20
                run-clang-tidy-19
                run-clang-tidy-18
                run-clang-tidy-17
                run-clang-tidy
            REQUIRED)
        find_program(ZZ_CLANG_TIDY_EXECUTABLE
            NAMES
                clang-tidy-20
                clang-tidy-19
                clang-tidy-18
                clang-tidy-17
                clang-tidy
            REQUIRED)
        add_custom_target(ZzClangTidy
            COMMAND "${ZZ_BASH_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/scripts/ci/run-clang-tidy.sh"
                "${PROJECT_SOURCE_DIR}"
                "${PROJECT_BINARY_DIR}"
                "${ZZ_RUN_CLANG_TIDY}"
                "${ZZ_CLANG_TIDY_EXECUTABLE}"
            VERBATIM
        )
    endif()

    add_dependencies(ZzClangTidy ${target_name})
endfunction()
```

- [ ] **Step 5: 验证负向与正向路径**

Run:

```bash
cmake -S . -B build/compiler-probe-gcc12 -G Ninja \
    -DCMAKE_CXX_COMPILER="$(command -v g++-12)" \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    -DZZ_BUILD_TESTS=OFF
cmake -S . -B build/compiler-probe-gcc13 -G Ninja \
    -DCMAKE_CXX_COMPILER="$GXX_13" \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    -DZZ_BUILD_TESTS=OFF
cmake --build build/compiler-probe-gcc13 --target ZzCore
cmake -S . -B build/compiler-probe-clang17-tidy -G Ninja \
    -DCMAKE_C_COMPILER="$CLANG_17" \
    -DCMAKE_CXX_COMPILER="$CLANGXX_17" \
    -DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN="$GCC_13_TOOLCHAIN_ROOT" \
    -DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN="$GCC_13_TOOLCHAIN_ROOT" \
    -DCMAKE_PREFIX_PATH="$QT_ROOT" \
    -DZZ_ENABLE_CLANG_TIDY=ON
cmake --build build/compiler-probe-clang17-tidy
cmake --build build/compiler-probe-clang17-tidy --target ZzClangTidy
cmake \
  -DZZ_SOURCE_DIR="$PWD" \
  -DZZ_QT_PREFIX="$QT_ROOT" \
  -DZZ_REJECTED_CXX="$(command -v g++-12)" \
  -DZZ_WORK_DIR="/tmp/zzpuretoolspro-gcc12-contract" \
  -P tests/Platform/CompilerCapabilitiesContract.cmake
```

Expected: GCC 12 配置 FAIL 且只由显式最低版本门禁拒绝；GCC 13.1+ 配置/构建 PASS；契约脚本自身 PASS；`ZzClangTidy` 覆盖一方组件、测试和 `ZzThirdParty/ZzLog/src`，命令中不出现 `_autogen`、`mocs_compilation.cpp`、`qrc_`、`ZzThirdParty/qwindowkit` 或 vendored spdlog 源。

- [ ] **Step 6: 提交编译能力门禁**

```bash
git add .clang-tidy \
  CMakeLists.txt \
  cmake/ZzCompilerCapabilities.cmake \
  cmake/ZzCompilerWarnings.cmake \
  cmake/ZzStaticAnalysis.cmake \
  scripts/ci/run-clang-tidy.sh \
  tests/Platform/CompilerCapabilitiesContract.cmake
git commit -m "构建：锁定 C++20 编译器与标准库" \
  -m "在配置阶段检查 format、source_location 和 stop_token。" \
  -m "隔离生成源和第三方源，并用正向路径执行 clang-tidy。"
```

## Task 2: 扩展 Windows 和 macOS CMake Preset

**Files:**
- Modify: `CMakePresets.json`
- Modify: `CMakeUserPresets.json.example`
- Create: `tests/Platform/PresetMatrixContract.cmake`
- Create: `scripts/ci/Assert-QtMinGWKit.ps1`

- [ ] **Step 1: 写完整 preset 矩阵的红灯契约**

Create `tests/Platform/PresetMatrixContract.cmake`:

```cmake
if(NOT DEFINED ZZ_PRESETS_FILE)
    message(FATAL_ERROR "Missing -DZZ_PRESETS_FILE=...")
endif()
file(READ "${ZZ_PRESETS_FILE}" presets_json)
string(JSON schema_version GET "${presets_json}" version)
if(NOT schema_version EQUAL 4)
    message(FATAL_ERROR "CMakePresets.json must use schema 4")
endif()

string(JSON preset_count LENGTH "${presets_json}" configurePresets)
math(EXPR last_preset "${preset_count} - 1")
set(actual_names)
foreach(index RANGE 0 ${last_preset})
    string(JSON name GET "${presets_json}" configurePresets ${index} name)
    list(APPEND actual_names "${name}")
endforeach()

set(required_names
    linux-gcc-debug
    linux-gcc-release
    linux-static-release
    linux-clang-release
    linux-clang-asan
    linux-gcc-release-lto
    linux-static-release-lto
    linux-clang-tidy-release
    linux-clang-tidy-static
    linux-gcc-lto-release
    linux-clang-tidy
    linux-gcc-benchmarks
    linux-gcc-reference
    linux-clang-asan-benchmarks
    windows-msvc2022-release
    windows-msvc2022-static
    windows-mingw-release
    windows-mingw-static
    macos-clang-release-arm64
    macos-clang-release-x86_64
    macos-clang-static-arm64
    macos-clang-static-x86_64
)
foreach(required_name IN LISTS required_names)
    if(NOT "${required_name}" IN_LIST actual_names)
        message(FATAL_ERROR "Missing configure preset: ${required_name}")
    endif()
endforeach()

function(zz_find_configure_preset_index output wanted_name)
    foreach(candidate_index RANGE 0 ${last_preset})
        string(JSON candidate_name GET "${presets_json}"
            configurePresets ${candidate_index} name)
        if("${candidate_name}" STREQUAL "${wanted_name}")
            set(${output} "${candidate_index}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "Missing configure preset: ${wanted_name}")
endfunction()

foreach(alias_pair IN ITEMS
        "linux-gcc-lto-release|linux-gcc-release-lto"
        "linux-clang-tidy|linux-clang-tidy-release")
    string(REPLACE "|" ";" alias_fields "${alias_pair}")
    list(GET alias_fields 0 alias_name)
    list(GET alias_fields 1 canonical_name)
    zz_find_configure_preset_index(alias_index "${alias_name}")
    string(JSON inherited GET "${presets_json}"
        configurePresets ${alias_index} inherits)
    if(NOT "${inherited}" STREQUAL "${canonical_name}")
        message(FATAL_ERROR
            "${alias_name} must inherit exactly ${canonical_name}")
    endif()
endforeach()

foreach(benchmark_pair IN ITEMS
        "linux-gcc-benchmarks|OFF"
        "linux-gcc-reference|ON")
    string(REPLACE "|" ";" benchmark_fields "${benchmark_pair}")
    list(GET benchmark_fields 0 benchmark_name)
    list(GET benchmark_fields 1 expected_reference)
    zz_find_configure_preset_index(benchmark_index "${benchmark_name}")
    string(JSON benchmark_base GET "${presets_json}"
        configurePresets ${benchmark_index} inherits)
    string(JSON benchmark_enabled GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables ZZ_BUILD_BENCHMARKS)
    string(JSON benchmark_shared GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables BUILD_SHARED_LIBS)
    string(JSON benchmark_lto GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables ZZ_ENABLE_LTO)
    string(JSON benchmark_reference GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables ZZ_PERFORMANCE_REFERENCE)
    if(NOT "${benchmark_base}" STREQUAL "linux-gcc13-base"
       OR NOT benchmark_enabled
       OR NOT benchmark_shared
       OR NOT benchmark_lto
       OR NOT "${benchmark_reference}" STREQUAL "${expected_reference}")
        message(FATAL_ERROR
            "Invalid benchmark configuration in ${benchmark_name}")
    endif()
endforeach()

foreach(base_contract IN ITEMS
        "linux-gcc13-base|$env{QT_ROOT}"
        "linux-clang17-base|$env{QT_ROOT}"
        "windows-msvc-base|$env{QT_MSVC_ROOT}"
        "windows-mingw-base|$env{QT_MINGW_ROOT}")
    string(REPLACE "|" ";" base_fields "${base_contract}")
    list(GET base_fields 0 base_name)
    list(GET base_fields 1 expected_qt_prefix)
    zz_find_configure_preset_index(base_index "${base_name}")
    string(JSON find_prefix GET "${presets_json}"
        configurePresets ${base_index} cacheVariables CMAKE_PREFIX_PATH)
    string(JSON gate_prefix GET "${presets_json}"
        configurePresets ${base_index} cacheVariables ZZ_QT_PREFIX)
    if(NOT "${find_prefix}" STREQUAL "${expected_qt_prefix}"
       OR NOT "${gate_prefix}" STREQUAL "${expected_qt_prefix}")
        message(FATAL_ERROR "Qt prefix mismatch in ${base_name}")
    endif()
endforeach()

foreach(clang_base IN ITEMS linux-clang17-base)
    zz_find_configure_preset_index(clang_base_index "${clang_base}")
    string(JSON c_external_toolchain GET "${presets_json}"
        configurePresets ${clang_base_index} cacheVariables
        CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN)
    string(JSON cxx_external_toolchain GET "${presets_json}"
        configurePresets ${clang_base_index} cacheVariables
        CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN)
    if(NOT "${c_external_toolchain}" STREQUAL
           "$env{GCC_13_TOOLCHAIN_ROOT}"
       OR NOT "${cxx_external_toolchain}" STREQUAL
           "$env{GCC_13_TOOLCHAIN_ROOT}")
        message(FATAL_ERROR
            "${clang_base} must use the GCC 13 external toolchain")
    endif()
endforeach()

foreach(windows_contract IN ITEMS
        "windows-msvc2022-release|windows-msvc-base|ON"
        "windows-msvc2022-static|windows-msvc-base|OFF"
        "windows-mingw-release|windows-mingw-base|ON"
        "windows-mingw-static|windows-mingw-base|OFF")
    string(REPLACE "|" ";" windows_fields "${windows_contract}")
    list(GET windows_fields 0 windows_name)
    list(GET windows_fields 1 expected_base)
    list(GET windows_fields 2 expected_shared)
    zz_find_configure_preset_index(windows_index "${windows_name}")
    string(JSON windows_base GET "${presets_json}"
        configurePresets ${windows_index} inherits)
    string(JSON windows_shared GET "${presets_json}"
        configurePresets ${windows_index} cacheVariables BUILD_SHARED_LIBS)
    if(NOT "${windows_base}" STREQUAL "${expected_base}"
       OR NOT "${windows_shared}" STREQUAL "${expected_shared}")
        message(FATAL_ERROR "Invalid Windows identity in ${windows_name}")
    endif()
endforeach()

string(JSON build_preset_count LENGTH "${presets_json}" buildPresets)
math(EXPR last_build_preset "${build_preset_count} - 1")
set(build_names)
foreach(index RANGE 0 ${last_build_preset})
    string(JSON name GET "${presets_json}" buildPresets ${index} name)
    list(APPEND build_names "${name}")
    if("${name}" IN_LIST required_names)
        string(JSON configured_by GET "${presets_json}"
            buildPresets ${index} configurePreset)
        if(NOT "${configured_by}" STREQUAL "${name}")
            message(FATAL_ERROR
                "build preset ${name} must configure from itself")
        endif()
        if("${name}" MATCHES "^windows-msvc")
            string(JSON build_configuration GET "${presets_json}"
                buildPresets ${index} configuration)
            if(NOT "${build_configuration}" STREQUAL "Release")
                message(FATAL_ERROR
                    "MSVC build preset ${name} must select Release")
            endif()
        endif()
    endif()
endforeach()

string(JSON test_preset_count LENGTH "${presets_json}" testPresets)
math(EXPR last_test_preset "${test_preset_count} - 1")
set(test_names)
foreach(index RANGE 0 ${last_test_preset})
    string(JSON name GET "${presets_json}" testPresets ${index} name)
    list(APPEND test_names "${name}")
    if(NOT "${name}" IN_LIST required_names)
        continue()
    endif()
    string(JSON configured_by GET "${presets_json}"
        testPresets ${index} configurePreset)
    string(JSON output_on_failure GET "${presets_json}"
        testPresets ${index} output outputOnFailure)
    string(JSON no_tests_action GET "${presets_json}"
        testPresets ${index} execution noTestsAction)
    if(NOT "${configured_by}" STREQUAL "${name}"
       OR NOT output_on_failure
       OR NOT "${no_tests_action}" STREQUAL "error")
        message(FATAL_ERROR
            "test preset ${name} has an incomplete execution contract")
    endif()
    string(JSON recorded_preset ERROR_VARIABLE preset_error GET
        "${presets_json}" testPresets ${index} environment ZZ_CMAKE_PRESET)
    if(NOT "${preset_error}" STREQUAL "NOTFOUND"
       OR NOT "${recorded_preset}" STREQUAL "${name}")
        message(FATAL_ERROR
            "test preset ${name} must set literal ZZ_CMAKE_PRESET=${name}")
    endif()
    foreach(environment_pair IN ITEMS
            "ZZ_BENCHMARK_COMMIT|$penv{ZZ_BENCHMARK_COMMIT}"
            "ZZ_RUNNER_IMAGE_DIGEST|$penv{ZZ_RUNNER_IMAGE_DIGEST}"
            "ZZ_GPU_IDENTITY|$penv{ZZ_GPU_IDENTITY}")
        string(REPLACE "|" ";" environment_fields "${environment_pair}")
        list(GET environment_fields 0 environment_name)
        list(GET environment_fields 1 expected_value)
        string(JSON recorded_value ERROR_VARIABLE environment_error GET
            "${presets_json}" testPresets ${index}
            environment "${environment_name}")
        if(NOT "${environment_error}" STREQUAL "NOTFOUND"
           OR NOT "${recorded_value}" STREQUAL "${expected_value}")
            message(FATAL_ERROR
                "test preset ${name} must pass through ${environment_name}")
        endif()
    endforeach()
    if("${name}" MATCHES "^windows-msvc")
        string(JSON test_configuration GET "${presets_json}"
            testPresets ${index} configuration)
        if(NOT "${test_configuration}" STREQUAL "Release")
            message(FATAL_ERROR
                "MSVC test preset ${name} must select Release")
        endif()
    endif()
endforeach()
foreach(required_name IN LISTS required_names)
    if(NOT "${required_name}" IN_LIST build_names)
        message(FATAL_ERROR "Missing build preset: ${required_name}")
    endif()
    if(NOT "${required_name}" IN_LIST test_names)
        message(FATAL_ERROR "Missing test preset: ${required_name}")
    endif()
endforeach()

set(macos_base_seen FALSE)
foreach(index RANGE 0 ${last_preset})
    string(JSON name GET "${presets_json}" configurePresets ${index} name)
    if("${name}" STREQUAL "macos-clang-base")
        set(macos_base_seen TRUE)
        string(JSON build_type GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_BUILD_TYPE)
        string(JSON deployment GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_OSX_DEPLOYMENT_TARGET)
        string(JSON tests GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_BUILD_TESTS)
        string(JSON warnings GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_WARNINGS_AS_ERRORS)
        string(JSON tidy GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_ENABLE_CLANG_TIDY)
        string(JSON lto GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_ENABLE_LTO)
        if(NOT "${build_type}" STREQUAL "Release"
           OR NOT "${deployment}" STREQUAL "12.0"
           OR NOT tests OR NOT warnings OR NOT tidy OR NOT lto)
            message(FATAL_ERROR "Incomplete macOS gate options in macos-clang-base")
        endif()
    elseif("${name}" MATCHES "^macos-clang-(release|static)-")
        string(JSON inherited GET "${presets_json}"
            configurePresets ${index} inherits)
        string(JSON qt_prefix GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_PREFIX_PATH)
        string(JSON gate_qt_prefix GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_QT_PREFIX)
        string(JSON architecture GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_OSX_ARCHITECTURES)
        string(JSON shared GET "${presets_json}"
            configurePresets ${index} cacheVariables BUILD_SHARED_LIBS)
        if(NOT "${inherited}" STREQUAL "macos-clang-base"
           OR NOT "${qt_prefix}" STREQUAL "${gate_qt_prefix}"
           OR NOT "${architecture}" MATCHES "^(arm64|x86_64)$")
            message(FATAL_ERROR "Invalid macOS identity fields in ${name}")
        endif()
        if("${name}" MATCHES "-release-" AND NOT shared)
            message(FATAL_ERROR "Invalid macOS linkage mode in ${name}")
        elseif("${name}" MATCHES "-static-" AND shared)
            message(FATAL_ERROR "Invalid macOS linkage mode in ${name}")
        endif()
    endif()
endforeach()
if(NOT macos_base_seen)
    message(FATAL_ERROR "Missing configure preset: macos-clang-base")
endif()
```

Run:

```bash
cmake \
  -DZZ_PRESETS_FILE="$PWD/CMakePresets.json" \
  -P tests/Platform/PresetMatrixContract.cmake
```

Expected: FAIL，首个错误是缺失 `linux-gcc-release-lto`、Windows 或 macOS configure preset。

- [ ] **Step 2: 让 Linux preset 显式选择 GCC 13.1+，并增加 LTO/tidy/benchmark 组合**

Keep the existing public preset names used by Plans 1-7, but merge every object from this valid JSON array into `configurePresets`，then change the existing Linux presets to inherit from them:

```json
[
{
  "name": "linux-gcc13-base",
  "hidden": true,
  "inherits": "linux-base",
  "cacheVariables": {
    "CMAKE_C_COMPILER": "$env{GCC_13}",
    "CMAKE_CXX_COMPILER": "$env{GXX_13}",
    "CMAKE_PREFIX_PATH": "$env{QT_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_ROOT}"
  }
},
{
  "name": "linux-clang17-base",
  "hidden": true,
  "inherits": "linux-base",
  "cacheVariables": {
    "CMAKE_C_COMPILER": "$env{CLANG_17}",
    "CMAKE_CXX_COMPILER": "$env{CLANGXX_17}",
    "CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN": "$env{GCC_13_TOOLCHAIN_ROOT}",
    "CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN": "$env{GCC_13_TOOLCHAIN_ROOT}",
    "CMAKE_PREFIX_PATH": "$env{QT_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_ROOT}"
  }
},
{
  "name": "linux-gcc-debug",
  "inherits": "linux-gcc13-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "BUILD_SHARED_LIBS": true
  }
},
{
  "name": "linux-gcc-release",
  "inherits": "linux-gcc13-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": true
  }
},
{
  "name": "linux-static-release",
  "inherits": "linux-gcc13-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": false
  }
},
{
  "name": "linux-gcc-release-lto",
  "inherits": "linux-gcc13-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": true,
    "ZZ_ENABLE_LTO": true
  }
},
{
  "name": "linux-static-release-lto",
  "inherits": "linux-gcc13-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": false,
    "ZZ_ENABLE_LTO": true
  }
},
{
  "name": "linux-clang-tidy-release",
  "inherits": "linux-clang17-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": true,
    "ZZ_ENABLE_CLANG_TIDY": true
  }
},
{
  "name": "linux-clang-tidy-static",
  "inherits": "linux-clang17-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": false,
    "ZZ_ENABLE_CLANG_TIDY": true
  }
},
{
  "name": "linux-gcc-lto-release",
  "inherits": "linux-gcc-release-lto"
},
{
  "name": "linux-clang-tidy",
  "inherits": "linux-clang-tidy-release"
},
{
  "name": "linux-gcc-benchmarks",
  "inherits": "linux-gcc13-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": true,
    "ZZ_BUILD_BENCHMARKS": true,
    "ZZ_ENABLE_LTO": true,
    "ZZ_PERFORMANCE_REFERENCE": false
  }
},
{
  "name": "linux-gcc-reference",
  "inherits": "linux-gcc13-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "BUILD_SHARED_LIBS": true,
    "ZZ_BUILD_BENCHMARKS": true,
    "ZZ_ENABLE_LTO": true,
    "ZZ_PERFORMANCE_REFERENCE": true
  }
},
{
  "name": "linux-clang-asan-benchmarks",
  "inherits": "linux-clang17-base",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "BUILD_SHARED_LIBS": true,
    "ZZ_BUILD_BENCHMARKS": true,
    "ZZ_ENABLE_ASAN": true,
    "ZZ_ENABLE_UBSAN": true
  }
}
]
```

The existing `linux-clang-release` and `linux-clang-asan` presets inherit `linux-clang17-base`; do not keep literal `g++` or `clang++` compiler cache values. `linux-gcc-lto-release` and `linux-clang-tidy` are compatibility presets required by Plans 1-7；它们分别继承新的 shared LTO/tidy preset，不能删除或产生不同 cache 语义。Add same-name build/test presets for every non-hidden object above. LTO and clang-tidy each have both shared and static build trees；`linux-gcc-benchmarks` and `linux-gcc-reference` always enable `ZZ_BUILD_BENCHMARKS` during configure.

- [ ] **Step 3: 增加 MSVC shared/static preset**

把以下有效 JSON 数组中的每个对象加入 `configurePresets`：

```json
[
{
  "name": "windows-msvc-base",
  "hidden": true,
  "generator": "Visual Studio 17 2022",
  "architecture": { "value": "x64", "strategy": "set" },
  "binaryDir": "${sourceDir}/build/${presetName}",
  "installDir": "${sourceDir}/install/${presetName}",
  "condition": {
    "type": "equals",
    "lhs": "${hostSystemName}",
    "rhs": "Windows"
  },
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "$env{QT_MSVC_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_MSVC_ROOT}",
    "ZZ_BUILD_TESTS": true,
    "ZZ_WARNINGS_AS_ERRORS": true,
    "ZZ_ENABLE_MSVC_ANALYZE": true,
    "ZZ_ENABLE_LTO": true
  }
},
{
  "name": "windows-msvc2022-release",
  "inherits": "windows-msvc-base",
  "cacheVariables": {
    "BUILD_SHARED_LIBS": true,
    "ZZ_BUILD_BENCHMARKS": false
  }
},
{
  "name": "windows-msvc2022-static",
  "inherits": "windows-msvc-base",
  "cacheVariables": {
    "BUILD_SHARED_LIBS": false,
    "ZZ_BUILD_BENCHMARKS": false
  }
}
]
```

build/test preset 使用同名 configure preset 且 `configuration` 为 `Release`。

- [ ] **Step 4: 增加 Qt 官方 MinGW shared/static preset**

隐藏 `windows-mingw-base` 使用 Ninja，且所有可执行路径都来自 Qt SDK 专用环境变量：

```json
{
  "name": "windows-mingw-base",
  "hidden": true,
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build/${presetName}",
  "installDir": "${sourceDir}/install/${presetName}",
  "condition": {
    "type": "equals",
    "lhs": "${hostSystemName}",
    "rhs": "Windows"
  },
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "CMAKE_PREFIX_PATH": "$env{QT_MINGW_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_MINGW_ROOT}",
    "CMAKE_C_COMPILER": "$env{QT_MINGW_TOOLCHAIN_ROOT}/bin/gcc.exe",
    "CMAKE_CXX_COMPILER": "$env{QT_MINGW_TOOLCHAIN_ROOT}/bin/g++.exe",
    "CMAKE_OBJDUMP": "$env{QT_MINGW_TOOLCHAIN_ROOT}/bin/objdump.exe",
    "CMAKE_MAKE_PROGRAM": "$env{NINJA_EXE}",
    "ZZ_BUILD_TESTS": true,
    "ZZ_WARNINGS_AS_ERRORS": true,
    "ZZ_ENABLE_LTO": true
  }
}
```

派生 `windows-mingw-release` (`BUILD_SHARED_LIBS=true`) 和 `windows-mingw-static` (`BUILD_SHARED_LIBS=false`)，并创建同名 build/test preset。

Create `scripts/ci/Assert-QtMinGWKit.ps1`:

```powershell
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$required = @(
    'QT_SDK_ROOT',
    'QT_MINGW_ROOT',
    'QT_MINGW_TOOLCHAIN_ROOT',
    'QT_MINGW_EXPECTED_GCC_VERSION',
    'NINJA_EXE'
)
foreach ($name in $required) {
    if ([string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name))) {
        throw "Missing environment variable $name"
    }
}

function Resolve-ExistingPath([string]$Path) {
    return (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
}
function Assert-UnderRoot([string]$Child, [string]$Root, [string]$Label) {
    $rootWithSlash = $Root.TrimEnd('\\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $Child.StartsWith($rootWithSlash,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label is outside QT_SDK_ROOT: $Child"
    }
}

$sdkRoot = Resolve-ExistingPath $env:QT_SDK_ROOT
$qtRoot = Resolve-ExistingPath $env:QT_MINGW_ROOT
$toolchainRoot = Resolve-ExistingPath $env:QT_MINGW_TOOLCHAIN_ROOT
$gxx = Resolve-ExistingPath (Join-Path $toolchainRoot 'bin/g++.exe')
$gcc = Resolve-ExistingPath (Join-Path $toolchainRoot 'bin/gcc.exe')
$objdump = Resolve-ExistingPath (Join-Path $toolchainRoot 'bin/objdump.exe')
$qmake = Resolve-ExistingPath (Join-Path $qtRoot 'bin/qmake.exe')
$ninja = Resolve-ExistingPath $env:NINJA_EXE

Assert-UnderRoot $qtRoot $sdkRoot 'Qt MinGW prefix'
Assert-UnderRoot $toolchainRoot $sdkRoot 'Qt MinGW toolchain'

$triple = (& $gxx -dumpmachine).Trim()
if ($LASTEXITCODE -ne 0 -or $triple -ne 'x86_64-w64-mingw32') {
    throw "Unexpected MinGW target triple: $triple"
}
$gccVersion = (& $gxx -dumpfullversion -dumpversion).Trim()
if ($LASTEXITCODE -ne 0 -or
    $gccVersion -ne $env:QT_MINGW_EXPECTED_GCC_VERSION) {
    throw "GCC version $gccVersion does not match the Qt kit declaration"
}
$qtPrefix = (Resolve-ExistingPath ((& $qmake -query QT_INSTALL_PREFIX).Trim()))
if ($LASTEXITCODE -ne 0 -or $qtPrefix -ne $qtRoot) {
    throw "qmake prefix does not match QT_MINGW_ROOT: $qtPrefix"
}
$xspec = (& $qmake -query QMAKE_XSPEC).Trim()
if ($LASTEXITCODE -ne 0 -or $xspec -ne 'win32-g++') {
    throw "Qt kit is not the official win32-g++ kit: $xspec"
}
$qtVersion = [Version]((& $qmake -query QT_VERSION).Trim())
if ($LASTEXITCODE -ne 0 -or $qtVersion -lt [Version]'6.8.0') {
    throw "Qt MinGW kit must be 6.8.0 or newer: $qtVersion"
}

[pscustomobject]@{
    QtPrefix = $qtRoot
    QtVersion = $qtVersion.ToString()
    Compiler = $gxx
    CCompiler = $gcc
    CompilerVersion = $gccVersion
    TargetTriple = $triple
    QMake = $qmake
    ObjDump = $objdump
    Ninja = $ninja
} | ConvertTo-Json -Depth 2
```

后续 MinGW 配置、二进制扫描和脚本只调用该脚本输出的规范化绝对路径；`PATH` 中同名的 MSYS2 工具不得参与。

- [ ] **Step 5: 增加 macOS arm64/x86_64 shared/static preset**

隐藏 `macos-clang-base` 必须是完整的 Release 静态门禁：

```json
{
  "name": "macos-clang-base",
  "hidden": true,
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build/${presetName}",
  "installDir": "${sourceDir}/install/${presetName}",
  "condition": {
    "type": "equals",
    "lhs": "${hostSystemName}",
    "rhs": "Darwin"
  },
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Release",
    "CMAKE_C_COMPILER": "$env{APPLE_CLANG}",
    "CMAKE_CXX_COMPILER": "$env{APPLE_CLANGXX}",
    "CMAKE_OBJCXX_COMPILER": "$env{APPLE_CLANGXX}",
    "CMAKE_OSX_DEPLOYMENT_TARGET": "12.0",
    "ZZ_BUILD_TESTS": true,
    "ZZ_BUILD_BENCHMARKS": false,
    "ZZ_WARNINGS_AS_ERRORS": true,
    "ZZ_ENABLE_CLANG_TIDY": true,
    "ZZ_ENABLE_LTO": true
  }
}
```

把以下有效 JSON 数组中的四个派生对象加入 `configurePresets`：

```json
[
{
  "name": "macos-clang-release-arm64",
  "inherits": "macos-clang-base",
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "$env{QT_MACOS_ARM64_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_MACOS_ARM64_ROOT}",
    "CMAKE_OSX_ARCHITECTURES": "arm64",
    "BUILD_SHARED_LIBS": true
  }
},
{
  "name": "macos-clang-release-x86_64",
  "inherits": "macos-clang-base",
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "$env{QT_MACOS_X86_64_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_MACOS_X86_64_ROOT}",
    "CMAKE_OSX_ARCHITECTURES": "x86_64",
    "BUILD_SHARED_LIBS": true
  }
},
{
  "name": "macos-clang-static-arm64",
  "inherits": "macos-clang-base",
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "$env{QT_MACOS_ARM64_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_MACOS_ARM64_ROOT}",
    "CMAKE_OSX_ARCHITECTURES": "arm64",
    "BUILD_SHARED_LIBS": false
  }
},
{
  "name": "macos-clang-static-x86_64",
  "inherits": "macos-clang-base",
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "$env{QT_MACOS_X86_64_ROOT}",
    "ZZ_QT_PREFIX": "$env{QT_MACOS_X86_64_ROOT}",
    "CMAKE_OSX_ARCHITECTURES": "x86_64",
    "BUILD_SHARED_LIBS": false
  }
}
]
```

每个同时增加同名 build/test preset，不在一个 build tree 中混合两种架构。每个构建在普通 build/test 后还必须构建 canonical `ZzClangTidy` target。

- [ ] **Step 6: 补齐 build/test preset 和用户环境示例**

`buildPresets` 和 `testPresets` 必须显式包含下列名称：

```text
linux-gcc-debug
linux-gcc-release
linux-static-release
linux-clang-release
linux-clang-asan
linux-gcc-release-lto
linux-static-release-lto
linux-clang-tidy-release
linux-clang-tidy-static
linux-gcc-lto-release
linux-clang-tidy
linux-gcc-benchmarks
linux-gcc-reference
linux-clang-asan-benchmarks
windows-msvc2022-release
windows-msvc2022-static
windows-mingw-release
windows-mingw-static
macos-clang-release-arm64
macos-clang-release-x86_64
macos-clang-static-arm64
macos-clang-static-x86_64
```

每个 build preset 的 `configurePreset` 与名称相同；每个 test preset 的 `configurePreset` 与名称相同，且包含 `output.outputOnFailure=true`、`execution.noTestsAction=error` 和与自身名称相同的字面量环境项。例：`linux-gcc-debug` 明确写 `"ZZ_CMAKE_PRESET": "linux-gcc-debug"`，`windows-mingw-static` 明确写 `"ZZ_CMAKE_PRESET": "windows-mingw-static"`；其余名称执行相同的一对一规则。不得用 `$presetName` 或外部 shell 临时赋值代替，Task 3-7 的报告和重定位测试直接读取该值。所有 test preset 另把 `ZZ_BENCHMARK_COMMIT`、`ZZ_RUNNER_IMAGE_DIGEST` 和 `ZZ_GPU_IDENTITY` 分别设为对应的 `$penv{...}`；只有 benchmark test 消费它们，benchmark 启用时任一缺失即失败。MSVC 的 build/test preset 另设 `configuration=Release`。每个非隐藏 configure preset 的有效 cache 中还必须有 `ZZ_QT_PREFIX`，值与同一 preset 的 `CMAKE_PREFIX_PATH` Qt 根一致；Linux/Windows 可从隐藏 base 继承，macOS 四个派生 preset 必须分别写入对应架构的 Qt 根。

`CMakeUserPresets.json.example` 必须通过 `$penv{...}` 示例传递以下变量，不填写开发者绝对路径：

```text
QT_ROOT, GCC_13, GXX_13, GCC_13_TOOLCHAIN_ROOT, CLANG_17, CLANGXX_17
QT_MSVC_ROOT
QT_SDK_ROOT, QT_MINGW_ROOT, QT_MINGW_TOOLCHAIN_ROOT
QT_MINGW_EXPECTED_GCC_VERSION, NINJA_EXE
QT_MACOS_ARM64_ROOT, QT_MACOS_X86_64_ROOT
APPLE_CLANG, APPLE_CLANGXX
ZZ_BENCHMARK_COMMIT, ZZ_RUNNER_IMAGE_DIGEST, ZZ_GPU_IDENTITY
```

- [ ] **Step 7: 先运行 JSON 契约和 Linux 实际 configure/build**

Run on Linux:

```bash
cmake -DZZ_PRESETS_FILE="$PWD/CMakePresets.json" \
  -P tests/Platform/PresetMatrixContract.cmake
cmake --preset linux-gcc-debug
cmake --preset linux-gcc-release-lto
cmake --preset linux-static-release-lto
cmake --preset linux-clang-tidy-release
cmake --preset linux-clang-tidy-static
cmake --preset linux-gcc-benchmarks
cmake --preset linux-clang-asan-benchmarks
cmake --build --preset linux-gcc-release-lto
cmake --build --preset linux-static-release-lto
cmake --build --preset linux-clang-tidy-release --target ZzClangTidy
cmake --build --preset linux-clang-tidy-static --target ZzClangTidy
```

Expected: 契约和所有 configure/build 返回 0；`CMakeCache.txt` 记录的 GCC 不低于 13.1；LTO 同时覆盖 shared/static；clang-tidy 同时覆盖 shared/static 且不扫描 `_autogen`。

- [ ] **Step 8: 在 Windows 原生 host 实际 configure 四个 preset**

Run from a VS 2022 x64 developer PowerShell:

```powershell
pwsh -NoProfile -File scripts/ci/Assert-QtMinGWKit.ps1
cmake --preset windows-msvc2022-release
cmake --preset windows-msvc2022-static
cmake --preset windows-mingw-release
cmake --preset windows-mingw-static
```

Expected: 四个 configure 均返回 0；MSVC cache 报告 19.38+、x64 和 MSVC Qt prefix；MinGW cache 中的 C/C++ compiler 是 `Assert-QtMinGWKit.ps1` 验证的绝对路径，Qt 是单独的 MinGW kit。

- [ ] **Step 9: 在 macOS 原生 host 实际 configure 四个 preset**

Run:

```bash
cmake --preset macos-clang-release-arm64
cmake --preset macos-clang-release-x86_64
cmake --preset macos-clang-static-arm64
cmake --preset macos-clang-static-x86_64
```

Expected: 四个 configure 均返回 0；每个 cache 都是 `Release`、deployment target `12.0`、精确单架构、测试 ON、严格警告 ON、clang-tidy ON、LTO ON。x86_64 Qt SDK 不可用时本步不得标记完成。

- [ ] **Step 10: 在三个原生 host 绿灯后提交 Preset**

```bash
git add CMakePresets.json \
  CMakeUserPresets.json.example \
  scripts/ci/Assert-QtMinGWKit.ps1 \
  tests/Platform/PresetMatrixContract.cmake
git commit -m "构建：增加三平台原生 Preset 矩阵" \
  -m "显式选择 GCC 13.1+、MSVC Qt、Qt 官方 MinGW 和 macOS 双架构 SDK。" \
  -m "为 shared/static 执行 LTO、严格警告、静态分析和真实 configure 验证。"
```

## Task 3: 建立统一性能 JSON 报告和阈值验证

**Files:**
- Create: `benchmarks/CMakeLists.txt`
- Create: `benchmarks/common/ZzBenchmarkSample.h`
- Create: `benchmarks/common/ZzBenchmarkMetadata.h`
- Create: `benchmarks/common/ZzBenchmarkMetadata.cpp`
- Create: `benchmarks/common/ZzPerformanceReporter.h`
- Create: `benchmarks/common/ZzPerformanceReporter.cpp`
- Create: `benchmarks/ZzPerformanceReporterTest.cpp`
- Create: `benchmarks/testdata/performance-valid.json`
- Create: `benchmarks/testdata/performance-invalid.json`
- Create: `cmake/ZzVerifyPerformanceReport.cmake`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先注册 reporter test target 并写红灯源码**

Add to root `CMakeLists.txt` after the component subdirectories:

```cmake
if(ZZ_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
```

Create `benchmarks/CMakeLists.txt` with the test target before creating the reporter headers:

```cmake
add_executable(ZzPerformanceReporterTest
    ZzPerformanceReporterTest.cpp
)
target_include_directories(ZzPerformanceReporterTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
)
target_link_libraries(ZzPerformanceReporterTest PRIVATE Qt6::Core Qt6::Test)
set_target_properties(ZzPerformanceReporterTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPerformanceReporterTest)
zz_enable_sanitizers(ZzPerformanceReporterTest)
add_test(NAME benchmark.reporter COMMAND ZzPerformanceReporterTest)
set_tests_properties(benchmark.reporter PROPERTIES LABELS "benchmark;unit")
```

Create `benchmarks/ZzPerformanceReporterTest.cpp`:

```cpp
#include <QtTest/QTest>

#include "ZzPerformanceReporter.h"

class ZzPerformanceReporterTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void calculatesNearestRankAndWritesSchema()
    {
        ZzBenchmarks::ZzPerformanceReporter reporter;
        reporter.setScenario(QStringLiteral("contract"));
        reporter.setWarmupIterations(10);
        for (const double value : {1.0, 2.0, 3.0, 4.0, 100.0}) {
            reporter.addSample({QStringLiteral("latency"),
                                QStringLiteral("ms"), value});
        }
        reporter.addEnvironmentMetadata(
            QStringLiteral("cpu"), QStringLiteral("test-cpu"));
        reporter.addEnvironmentMetadata(QStringLiteral("memoryBytes"), 1024);
        reporter.addEnvironmentMetadata(
            QStringLiteral("os"), QStringLiteral("test-os"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("gpu"), QStringLiteral("test-gpu/test-driver"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("windowSystem"), QStringLiteral("test-qpa"));
        reporter.addEnvironmentMetadata(QStringLiteral("dpr"), 1.0);
        reporter.addEnvironmentMetadata(QStringLiteral("refreshRateHz"), 60.0);
        reporter.addEnvironmentMetadata(
            QStringLiteral("qtVersion"), QStringLiteral("6.8.3"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("compiler"), QStringLiteral("GNU 13.2.0"));
        reporter.addEnvironmentMetadata(
            QStringLiteral("runnerImageDigest"),
            QStringLiteral("sha256:0000000000000000000000000000000000000000000000000000000000000000"));
        reporter.addBuildMetadata(
            QStringLiteral("commit"),
            QStringLiteral("0123456789abcdef0123456789abcdef01234567"));
        const QString preset = qEnvironmentVariable("ZZ_CMAKE_PRESET");
        QVERIFY(!preset.isEmpty());
        reporter.addBuildMetadata(QStringLiteral("preset"), preset);
        reporter.addBuildMetadata(
            QStringLiteral("buildType"), QStringLiteral("Release"));
        reporter.addBuildMetadata(QStringLiteral("shared"), true);
        reporter.addBuildMetadata(QStringLiteral("lto"), true);
        reporter.addBuildMetadata(
            QStringLiteral("sanitizers"), QStringLiteral("none"));

        const auto reportResult = reporter.report();
        QVERIFY(reportResult);
        const QJsonObject root = reportResult.value();
        const QJsonObject latency = root.value(QStringLiteral("metrics"))
            .toObject().value(QStringLiteral("latency")).toObject();
        QCOMPARE(latency.value(QStringLiteral("count")).toInt(), 5);
        QCOMPARE(latency.value(QStringLiteral("p50")), QJsonValue(3.0));
        QCOMPARE(latency.value(QStringLiteral("p95")), QJsonValue(100.0));
        QCOMPARE(latency.value(QStringLiteral("max")), QJsonValue(100.0));
        QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QVERIFY(root.value(QStringLiteral("environment")).isObject());
        QVERIFY(root.value(QStringLiteral("build")).isObject());
    }

    void rejectsConflictingMetricUnits()
    {
        ZzBenchmarks::ZzPerformanceReporter reporter;
        reporter.setScenario(QStringLiteral("contract"));
        reporter.addSample({QStringLiteral("latency"),
                            QStringLiteral("ms"), 1.0});
        reporter.addSample({QStringLiteral("latency"),
                            QStringLiteral("bytes"), 2.0});

        const auto result = reporter.report();
        QVERIFY(!result);
        QCOMPARE(result.error().code(), ZzCore::ZzErrorCode::InvalidArgument);
        QVERIFY(result.error().technicalMessage().contains(
            QStringLiteral("unit"), Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(ZzPerformanceReporterTest)

#include "ZzPerformanceReporterTest.moc"
```

- [ ] **Step 2: 运行红灯测试**

Run:

```bash
cmake --preset linux-gcc-benchmarks
cmake --build --preset linux-gcc-benchmarks --target ZzPerformanceReporterTest
```

Expected: compile FAIL，缺少 `ZzPerformanceReporter.h`。

- [ ] **Step 3: 实现 reporter 内部契约和固定 JSON schema**

```cpp
#pragma once

#include <QtCore/QString>

namespace ZzBenchmarks {

/**
 * @brief 保存一次 benchmark 指标采样。
 */
struct ZzBenchmarkSample final
{
    QString metric;
    QString unit;
    double value = 0.0;
};

} // namespace ZzBenchmarks
```

```cpp
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QList>
#include <QtCore/QString>

#include "ZzBenchmarkSample.h"

#include <ZzCore/ZzResult.h>

namespace ZzBenchmarks {

/**
 * @brief 聚合性能样本并生成固定 schema 的 JSON 报告。
 */
class ZzPerformanceReporter final
{
public:
    /** @brief 设置唯一场景名称。 */
    void setScenario(QString scenario);
    /** @brief 记录未进入正式统计的预热迭代数。 */
    void setWarmupIterations(qsizetype count);
    /** @brief 添加一个有限数值样本。 */
    void addSample(ZzBenchmarkSample sample);
    /** @brief 添加环境指纹字段。 */
    void addEnvironmentMetadata(QString key, QJsonValue value);
    /** @brief 添加构建指纹字段。 */
    void addBuildMetadata(QString key, QJsonValue value);
    /** @brief 校验当前状态并生成报告对象。 */
    [[nodiscard]] ZzCore::ZzResult<QJsonObject> report() const;
    /** @brief 将已校验报告原子写入指定路径。 */
    [[nodiscard]] ZzCore::ZzResult<void> write(const QString &path) const;

private:
    QString scenario_;
    qsizetype warmupIterations_ = 0;
    QList<ZzBenchmarkSample> samples_;
    QJsonObject environment_;
    QJsonObject build_;
};

} // namespace ZzBenchmarks
```

Replace the temporary `benchmarks/CMakeLists.txt` target wiring with a support library whose complete source list is declared before warnings are enabled:

```cmake
add_library(ZzBenchmarkSupport STATIC
    common/ZzBenchmarkMetadata.cpp
    common/ZzPerformanceReporter.cpp
)
target_include_directories(ZzBenchmarkSupport PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
)
target_link_libraries(ZzBenchmarkSupport PUBLIC
    Qt6::Core
    Qt6::Gui
    Zz::Core
)
set(zz_benchmark_sanitizers none)
if(ZZ_ENABLE_ASAN OR ZZ_ENABLE_UBSAN)
    set(zz_benchmark_sanitizers asan-ubsan)
endif()
target_compile_definitions(ZzBenchmarkSupport PRIVATE
    "ZZ_BENCHMARK_BUILD_TYPE=\"$<CONFIG>\""
    "ZZ_BENCHMARK_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    "ZZ_BENCHMARK_SHARED=$<BOOL:${BUILD_SHARED_LIBS}>"
    "ZZ_BENCHMARK_LTO=$<BOOL:${ZZ_ENABLE_LTO}>"
    "ZZ_BENCHMARK_SANITIZERS=\"${zz_benchmark_sanitizers}\""
)
zz_enable_project_warnings(ZzBenchmarkSupport)
zz_enable_sanitizers(ZzBenchmarkSupport)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/reports")

add_executable(ZzPerformanceReporterTest
    ZzPerformanceReporterTest.cpp
)
target_link_libraries(ZzPerformanceReporterTest PRIVATE
    ZzBenchmarkSupport Qt6::Test
)
set_target_properties(ZzPerformanceReporterTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPerformanceReporterTest)
zz_enable_sanitizers(ZzPerformanceReporterTest)
add_test(NAME benchmark.reporter COMMAND ZzPerformanceReporterTest)
set_tests_properties(benchmark.reporter PROPERTIES LABELS "benchmark;unit")
```

No `target_sources()` call may append a first-party translation unit after `zz_enable_project_warnings()`; this preserves Task 1's source-only warning contract.

`report()` 按 `metric` 分组，对样本拷贝排序，P50/P95 使用 nearest-rank `ceil(p * N) - 1`。它在 scenario 为空、无样本、同名 metric 单位冲突、数值非有限或必需 metadata 不完整时返回 `ZzErrorCode::InvalidArgument`。`write()` 先取得成功 Result，再使用 `QSaveFile` 原子写入；打开、写入或 commit 失败返回 `Io`。输出 schema 只能是：

```json
{
  "schemaVersion": 1,
  "scenario": "theme-switch",
  "warmupIterations": 10,
  "metrics": {
    "latency": {
      "unit": "ms",
      "count": 100,
      "p50": 12.5,
      "p95": 19.5,
      "max": 22.0
    }
  },
  "environment": {
    "cpu": "exact model",
    "memoryBytes": 34359738368,
    "os": "distribution and version",
    "gpu": "renderer and driver identity",
    "windowSystem": "xcb",
    "dpr": 1.0,
    "refreshRateHz": 60.0,
    "qtVersion": "6.8.3",
    "compiler": "GNU 13.2.0",
    "runnerImageDigest": "sha256:0000000000000000000000000000000000000000000000000000000000000000"
  },
  "build": {
    "commit": "0123456789abcdef0123456789abcdef01234567",
    "preset": "linux-gcc-reference",
    "buildType": "Release",
    "shared": true,
    "lto": true,
    "sanitizers": "none"
  }
}
```

`runnerImageDigest` 必须位于 `environment`，格式为 `sha256:` 后跟 64 位小写十六进制；`commit` 必须是 40 位小写十六进制；`preset` 必须等于 test preset 注入的 `ZZ_CMAKE_PRESET`。`memoryBytes` 必须是非负 JSON integer，`dpr/refreshRateHz` 必须是有限正数，`shared/lto` 必须是 JSON boolean，不能把这些值序列化为字符串。`gpu` 必须是去除首尾空白后仍非空的 renderer/驱动身份，禁止用 `unknown` 通过校验。

Create `ZzBenchmarkMetadata.h/.cpp` with a stateless `ZzBenchmarkMetadata final` class in the same `ZzBenchmarks` namespace. Its public declaration is:

```cpp
#pragma once

#include <ZzCore/ZzResult.h>

class QScreen;

namespace ZzBenchmarks {

class ZzPerformanceReporter;

/**
 * @brief 从当前 Linux 进程、显示和构建环境采集统一性能元数据。
 */
class ZzBenchmarkMetadata final
{
public:
    ZzBenchmarkMetadata() = delete;

    /**
     * @brief 校验环境并把完整元数据写入 reporter。
     * @param reporter 接收元数据且不转移所有权的报告器。
     * @param screen 当前 benchmark 所在屏幕的非拥有指针。
     * @return 成功或带具体错误码的失败结果。
     */
    [[nodiscard]] static ZzCore::ZzResult<void> populate(
        ZzPerformanceReporter &reporter,
        const QScreen *screen);
};

} // namespace ZzBenchmarks
```

两个 `.cpp` 都使用传统的 `namespace ZzBenchmarks { ... }`，不得把 benchmark support 类型留在全局命名空间，也不得使用链式 namespace。各 benchmark 以全限定名引用，或只在自己的 `.cpp` 内声明单个 type alias；禁止在头文件写 `using namespace`。

实现必须从 `/proc/cpuinfo` 的首个非空 `model name`、`/proc/meminfo` 的 `MemTotal`、`QSysInfo::prettyProductName()`、`QGuiApplication::platformName()`、传入 screen 的 DPR/刷新率以及 `qVersion()` 填充 environment；当前 benchmark gate 只在 Linux 启用，缺少 `/proc` 字段或 screen 时返回 `Io/InvalidState`，不能用 architecture 代替 CPU 型号。compiler/buildType/shared/lto/sanitizers 使用上面 CMake 注入的强类型定义。commit、preset、runner digest 和经审定的 GPU/驱动身份分别读取 `ZZ_BENCHMARK_COMMIT`、`ZZ_CMAKE_PRESET`、`ZZ_RUNNER_IMAGE_DIGEST`、`ZZ_GPU_IDENTITY`，任一格式不合法或 GPU 值为空/`unknown` 即返回 `InvalidArgument`。GPU 身份由 runner provisioning 从 `glxinfo -B`、`vulkaninfo` 或等价受审日志生成，reporter 不在计时进程内启动外部命令。每个实际 benchmark 在添加样本前调用一次 `populate()` 并传播失败，然后调用 `write()`；禁止各 benchmark 复制一套字段采集逻辑。时间场景统一预热 10 轮、正式 100 轮；启动场景是 5 次预热和 30 次独立正式进程。

- [ ] **Step 4: 实现可独立运行的 CMake 阈值检查器**

Create `cmake/ZzVerifyPerformanceReport.cmake`:

```cmake
foreach(required ZZ_REPORT ZZ_SCENARIO ZZ_METRIC)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
if(NOT DEFINED ZZ_MAX_P95 AND NOT DEFINED ZZ_MAX_VALUE)
    message(FATAL_ERROR "At least one of ZZ_MAX_P95/ZZ_MAX_VALUE is required")
endif()
if(NOT EXISTS "${ZZ_REPORT}")
    message(FATAL_ERROR "Performance report does not exist: ${ZZ_REPORT}")
endif()
file(READ "${ZZ_REPORT}" report_json)

function(zz_json_get output)
    string(JSON value ERROR_VARIABLE json_error GET "${report_json}" ${ARGN})
    if(NOT "${json_error}" STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Invalid/missing JSON path ${ARGN}: ${json_error}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

zz_json_get(schema_version schemaVersion)
zz_json_get(scenario scenario)
if(NOT schema_version EQUAL 1 OR NOT "${scenario}" STREQUAL "${ZZ_SCENARIO}")
    message(FATAL_ERROR
        "Unexpected schema/scenario: ${schema_version}/${scenario}")
endif()

function(zz_require_commit json_text label)
    string(JSON commit ERROR_VARIABLE commit_error GET
        "${json_text}" build commit)
    string(LENGTH "${commit}" commit_length)
    if(NOT "${commit_error}" STREQUAL "NOTFOUND"
       OR NOT commit_length EQUAL 40
       OR NOT "${commit}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${label} build.commit must be 40 lowercase hex")
    endif()
endfunction()
zz_require_commit("${report_json}" "Report")

if(DEFINED ZZ_FINGERPRINT_REFERENCE)
    if("${ZZ_FINGERPRINT_REFERENCE}" STREQUAL ""
       OR NOT EXISTS "${ZZ_FINGERPRINT_REFERENCE}")
        message(FATAL_ERROR
            "ZZ_FINGERPRINT_REFERENCE must name an existing report")
    endif()
    file(READ "${ZZ_FINGERPRINT_REFERENCE}" fingerprint_json)
    zz_require_commit("${fingerprint_json}" "Fingerprint reference")
    foreach(fingerprint_path IN ITEMS
        "schemaVersion"
        "environment;cpu"
        "environment;memoryBytes"
        "environment;os"
        "environment;gpu"
        "environment;runnerImageDigest"
        "environment;qtVersion"
        "environment;compiler"
        "environment;windowSystem"
        "environment;dpr"
        "environment;refreshRateHz"
        "build;preset"
        "build;buildType"
        "build;shared"
        "build;lto"
        "build;sanitizers")
        string(JSON report_value ERROR_VARIABLE report_value_error GET
            "${report_json}" ${fingerprint_path})
        string(JSON reference_value ERROR_VARIABLE reference_value_error GET
            "${fingerprint_json}" ${fingerprint_path})
        if(NOT "${report_value_error}" STREQUAL "NOTFOUND"
           OR NOT "${reference_value_error}" STREQUAL "NOTFOUND"
           OR NOT "${report_value}" STREQUAL "${reference_value}")
            message(FATAL_ERROR
                "Fingerprint mismatch at ${fingerprint_path}: "
                "report=${report_value}, reference=${reference_value}")
        endif()
    endforeach()
endif()

string(JSON metric_type ERROR_VARIABLE metric_error TYPE
    "${report_json}" metrics "${ZZ_METRIC}")
if(NOT "${metric_error}" STREQUAL "NOTFOUND"
   OR NOT "${metric_type}" STREQUAL "OBJECT")
    message(FATAL_ERROR "Missing metric object: ${ZZ_METRIC}")
endif()
foreach(field count p50 p95 max)
    string(JSON field_type ERROR_VARIABLE field_error TYPE
        "${report_json}" metrics "${ZZ_METRIC}" ${field})
    if(NOT "${field_error}" STREQUAL "NOTFOUND"
       OR NOT "${field_type}" STREQUAL "NUMBER")
        message(FATAL_ERROR "Metric ${ZZ_METRIC}.${field} must be numeric")
    endif()
endforeach()
zz_json_get(p95 metrics "${ZZ_METRIC}" p95)
zz_json_get(maximum metrics "${ZZ_METRIC}" max)

if(DEFINED ZZ_MAX_P95 AND "${p95}" GREATER "${ZZ_MAX_P95}")
    message(FATAL_ERROR "P95 ${p95} exceeds ${ZZ_MAX_P95}")
endif()
if(DEFINED ZZ_MAX_VALUE)
    if(ZZ_STRICT_MAX)
        if(NOT "${maximum}" LESS "${ZZ_MAX_VALUE}")
            message(FATAL_ERROR "Max ${maximum} must be below ${ZZ_MAX_VALUE}")
        endif()
    elseif("${maximum}" GREATER "${ZZ_MAX_VALUE}")
        message(FATAL_ERROR "Max ${maximum} exceeds ${ZZ_MAX_VALUE}")
    endif()
endif()
message(STATUS
    "${ZZ_SCENARIO}/${ZZ_METRIC}: p95=${p95}, max=${maximum}")
```

- [ ] **Step 5: 增加检查器的成功和预期失败 CTest**

Create `benchmarks/testdata/performance-valid.json` with exactly:

```json
{
  "schemaVersion": 1,
  "scenario": "contract",
  "warmupIterations": 10,
  "metrics": {
    "latency": {
      "unit": "ms",
      "count": 5,
      "p50": 3,
      "p95": 100,
      "max": 100
    }
  },
  "environment": {
    "cpu": "test-cpu",
    "memoryBytes": 1024,
    "os": "test-os",
    "gpu": "test-gpu/test-driver",
    "windowSystem": "test-qpa",
    "dpr": 1,
    "refreshRateHz": 60,
    "qtVersion": "6.8.3",
    "compiler": "GNU 13.2.0",
    "runnerImageDigest": "sha256:0000000000000000000000000000000000000000000000000000000000000000"
  },
  "build": {
    "commit": "0123456789abcdef0123456789abcdef01234567",
    "preset": "linux-gcc-reference",
    "buildType": "Release",
    "shared": true,
    "lto": true,
    "sanitizers": "none"
  }
}
```

Create `benchmarks/testdata/performance-invalid.json` with exactly:

```json
{
  "schemaVersion": 1,
  "scenario": "contract"
}
```

Append these verifier tests after the fully declared reporter targets:

```cmake
add_test(NAME benchmark.report-verifier-valid
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=100
        -DZZ_MAX_VALUE=100
        "-DZZ_FINGERPRINT_REFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
add_test(NAME benchmark.report-verifier-invalid
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-invalid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=100
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
add_test(NAME benchmark.report-verifier-rejects-threshold
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=99
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
add_test(NAME benchmark.report-verifier-rejects-fingerprint
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=100
        "-DZZ_FINGERPRINT_REFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-invalid.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
set_tests_properties(
    benchmark.report-verifier-invalid
    benchmark.report-verifier-rejects-threshold
    benchmark.report-verifier-rejects-fingerprint
    PROPERTIES
    WILL_FAIL TRUE
    LABELS "benchmark;unit"
)
set_tests_properties(benchmark.report-verifier-valid PROPERTIES
    LABELS "benchmark;unit")
```

- [ ] **Step 6: 运行 reporter 和阈值检查器绿灯**

Run:

```bash
cmake --preset linux-gcc-benchmarks
cmake --build --preset linux-gcc-benchmarks --target ZzPerformanceReporterTest
ctest --preset linux-gcc-benchmarks \
  -R '^benchmark\.(reporter|report-verifier-(valid|invalid|rejects-(threshold|fingerprint)))$' \
  --output-on-failure
```

Expected: 5/5 PASS；reporter 的 nearest-rank 结果、JSON schema、合法报告通过路径、缺字段拒绝路径、超阈值拒绝路径和指纹不完整拒绝路径均被执行。`ZZ_FINGERPRINT_REFERENCE` 对普通格式/阈值调用可省略，但一旦传入就必须逐字段完全匹配；当前与参考 `build.commit` 都只验证格式，不要求相等。

- [ ] **Step 7: 提交性能报告基础**

```bash
git add CMakeLists.txt \
  benchmarks/CMakeLists.txt \
  benchmarks/ZzPerformanceReporterTest.cpp \
  benchmarks/common/ZzBenchmarkMetadata.cpp \
  benchmarks/common/ZzBenchmarkMetadata.h \
  benchmarks/common/ZzBenchmarkSample.h \
  benchmarks/common/ZzPerformanceReporter.cpp \
  benchmarks/common/ZzPerformanceReporter.h \
  benchmarks/testdata/performance-invalid.json \
  benchmarks/testdata/performance-valid.json \
  cmake/ZzVerifyPerformanceReport.cmake
git commit -m "性能：建立统一测量与 JSON 报告" \
  -m "固定 metric、单位、P50、P95、最大值和环境元数据 schema。" \
  -m "对合法、缺字段和超阈值报告执行独立 CMake 契约。"
```

## Task 4: 实现启动、主题、大模型和窗口生命周期基准

**Files:**
- Modify: `benchmarks/CMakeLists.txt`
- Create: `benchmarks/ZzStartupProbe/CMakeLists.txt`
- Create: `benchmarks/ZzStartupProbe/main.cpp`
- Create: `benchmarks/ZzStartupBenchmark.cpp`
- Create: `benchmarks/ZzThemeSwitchBenchmark.cpp`
- Create: `benchmarks/ZzAnimationBenchmark.cpp`
- Create: `benchmarks/ZzLargeModelBenchmark.cpp`
- Create: `benchmarks/ZzWindowLifecycleBenchmark.cpp`
- Create: `ZzWindowKit/src/private/ZzWindowKitDiagnostics.h`
- Create: `ZzWindowKit/src/private/ZzWindowKitDiagnostics.cpp`
- Modify: `ZzWindowKit/src/private/ZzQWindowKitBackend.cpp`
- Modify: `ZzWindowKit/CMakeLists.txt`

- [ ] **Step 1: 先注册六个 target 得到缺源文件红灯**

After Task 3's reporter targets, append this exact target graph to `benchmarks/CMakeLists.txt`:

```cmake
add_subdirectory(ZzStartupProbe)

function(zz_add_benchmark target_name source_name test_name)
    add_executable(${target_name} "${source_name}")
    target_link_libraries(${target_name} PRIVATE
        ZzBenchmarkSupport Qt6::Core Qt6::Gui Qt6::Test Qt6::Widgets)
    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(${target_name})
    zz_enable_sanitizers(${target_name})
    add_test(NAME "${test_name}" COMMAND ${target_name}
        --report "${CMAKE_BINARY_DIR}/reports/${test_name}.json")
    set_tests_properties("${test_name}" PROPERTIES
        LABELS "benchmark"
        TIMEOUT 120)
endfunction()

zz_add_benchmark(ZzStartupBenchmark
    ZzStartupBenchmark.cpp benchmark.startup)
zz_add_benchmark(ZzThemeSwitchBenchmark
    ZzThemeSwitchBenchmark.cpp benchmark.theme-switch)
zz_add_benchmark(ZzAnimationBenchmark
    ZzAnimationBenchmark.cpp benchmark.animation)
zz_add_benchmark(ZzLargeModelBenchmark
    ZzLargeModelBenchmark.cpp benchmark.large-model)
zz_add_benchmark(ZzWindowLifecycleBenchmark
    ZzWindowLifecycleBenchmark.cpp benchmark.window-lifecycle)

target_link_libraries(ZzStartupBenchmark PRIVATE Zz::PureTools)
target_link_libraries(ZzThemeSwitchBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzAnimationBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzLargeModelBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzWindowLifecycleBenchmark PRIVATE
    Zz::PureTools Zz::WindowKit)
add_dependencies(ZzStartupBenchmark ZzStartupProbe)
set(zz_startup_probe_generated_dir
    "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>")
file(GENERATE
    OUTPUT "${zz_startup_probe_generated_dir}/ZzStartupProbePath.h"
    CONTENT
"#pragma once
inline constexpr char ZzStartupProbePath[] = R\"ZZPROBE($<TARGET_FILE:ZzStartupProbe>)ZZPROBE\";
")
target_include_directories(ZzStartupBenchmark PRIVATE
    "${zz_startup_probe_generated_dir}")
target_compile_definitions(ZzWindowLifecycleBenchmark PRIVATE
    ZZ_WINDOWKIT_DIAGNOSTICS=1)
target_include_directories(ZzWindowLifecycleBenchmark PRIVATE
    "${PROJECT_SOURCE_DIR}/ZzWindowKit/src/private")
```

Create `benchmarks/ZzStartupProbe/CMakeLists.txt` before creating `main.cpp`:

```cmake
add_executable(ZzStartupProbe main.cpp)
target_link_libraries(ZzStartupProbe PRIVATE
    Zz::PureTools Zz::WindowKit Qt6::Widgets)
zz_enable_project_warnings(ZzStartupProbe)
zz_enable_sanitizers(ZzStartupProbe)
```

Run:

```bash
cmake --preset linux-gcc-benchmarks
```

Expected: configure FAIL，第一个缺失源文件是 `benchmarks/ZzStartupProbe/main.cpp`。

- [ ] **Step 2: 实现外部进程暖启动基准**

`ZzStartupProbe` 必须包含 `ZzWindowKit/ZzWindowKitBootstrap.h`。`main()` 的执行顺序固定为：

1. 第一条可执行语句启动 `QElapsedTimer`。
2. 记录 `process-entry=0`。
3. 调用 `ZzWindowKit::ZzWindowKitBootstrap::prepare()` 并检查结果；失败时把 `error().technicalMessage()` 的 UTF-8 文本写到 stderr，返回非零，且不得继续构造应用。
4. 仅在 prepare 成功后创建最小 `ZzPureApplication`，随后记录 `qt-created`。

之后记录 `modules-started`、`page-created`、`first-paint` 等单调 marker。窗口 event filter 收到首个 `QEvent::Paint` 后用 `QTimer::singleShot(0, ...)` 检查 `isVisible()`、`isEnabled()`、`windowHandle()->isExposed()` 和 `focusWidget()!=nullptr`；任一为假则返回 2。全部为真时向 stdout 写一行 `QJsonDocument::Compact` 并退出 0。bootstrap 失败必须可由父进程捕获 stderr，不能静默退出。

`ZzStartupBenchmark.cpp` 包含生成的 `ZzStartupProbePath.h`，并使用：

```cpp
const QString probePath = QString::fromUtf8(ZzStartupProbePath);
if (!QFileInfo(probePath).isAbsolute()
    || !QFileInfo(probePath).isExecutable()) {
    return fail(QStringLiteral("invalid startup probe: %1").arg(probePath));
}
```

禁止通过 `QCoreApplication::applicationDirPath()` 猜测子目录 target 的输出位置。`file(GENERATE)` 中的 `$<TARGET_FILE:ZzStartupProbe>` 给每个 configuration 产生精确绝对路径；`$<CONFIG>` 隔离 multi-config 头文件，raw string 避免 Windows 路径反斜杠转义。循环固定为：

```cpp
for (int iteration = 0; iteration < 35; ++iteration) {
    QProcess child;
    QElapsedTimer external;
    external.start();
    child.start(probePath, {});
    if (!child.waitForStarted(1000) || !child.waitForFinished(5000)
        || child.exitStatus() != QProcess::NormalExit
        || child.exitCode() != 0) {
        return failWithChildOutput(child);
    }
    const QJsonObject markers = parseSingleJsonLine(child.readAllStandardOutput());
    validateStrictlyIncreasingMarkers(markers);
    if (iteration >= 5) {
        reporter.addSample({QStringLiteral("external-total"),
                            QStringLiteral("ms"),
                            static_cast<double>(external.elapsed())});
        addInternalMarkerSamples(reporter, markers);
    }
}
```

它必须产生 30 个 `external-total` 样本，并把外部总时间与内部 marker 同时写入 `benchmark.startup.json`。父 benchmark 创建 `QGuiApplication`，正式采样前调用 `ZzBenchmarks::ZzBenchmarkMetadata::populate(reporter, QGuiApplication::primaryScreen())`；metadata 失败或 `write()` 失败都使进程非零退出。

- [ ] **Step 3: 实现 500 可见控件主题切换基准**

创建 25x20 的固定网格，根窗口固定为 1000x800；grid contents margin 固定为 0、水平/垂直 spacing 固定为 4，每个基础控件固定为 36x36 逻辑像素并清除继承的 minimum size，因此 25 列宽度为 996、20 行高度为 796。显示并激活布局后，对 500 个控件逐个断言 `isVisibleTo(window)==true`，且把控件 geometry 映射到根窗口坐标后完整包含于 `window.rect()`，不能用 `isVisibleTo()` 代替几何可见性。测量前执行 `ensurePolished()`、layout activate、`processEvents()`，把预分配 `QImage` 清为透明后 render 一次，再扫描像素并断言至少一个像素 alpha 非零且至少存在两种 RGBA 值，防止空白或只测到背景。每轮执行：

```cpp
QSignalSpy changed(&controller, &ZzThemeController::snapshotChanged);
timer.start();
controller.setMode(nextMode);
if (changed.count() == 0 && !changed.wait(1000)) fail();
QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
QCoreApplication::processEvents(QEventLoop::AllEvents);
window.render(&image);
reporter.addSample({"latency", "ms", elapsedMilliseconds(timer)});
```

使用 10 轮预热和 100 轮正式样本；计时区间不得重建控件、重分配 `QImage` 或清空无关缓存。正式采样前用当前窗口的 `screen()` 调用一次 `ZzBenchmarks::ZzBenchmarkMetadata::populate()`，最后传播 `write()` Result。

- [ ] **Step 4: 实现 60 FPS 动画帧耗时基准**

`ZzAnimationBenchmark` 创建一个 `ZzToggleSwitch` 和一个记录 `QEvent::Paint` 时刻的 event filter。初始化后用 `findChildren<QVariantAnimation *>()` 要求恰好一个 animation 并保存非拥有指针；不得假设公开控件 API 暴露 private animation。对 10 轮预热和 100 轮正式 toggle，每轮清空时刻、发送一次点击、运行事件循环直到该 animation 的 `state()==Stopped` 或 1000 ms 超时。正式轮次至少产生 8 个 paint marker；相邻 marker 的毫秒差加入 `frame-time` metric。对象数在 100 轮前后必须不变，防止通过每帧重建 animation 伪造帧率。采样前填充统一 metadata，最后传播 `write()` Result。

- [ ] **Step 5: 实现 10 万行 Model/View 滚动基准**

`ZzLargeModelBenchmark` 的 private nested `ZzBenchmarkListModel` 只保存 `rowCount=100000`、`multiData()` 调用计数和本帧被请求行号集合；它覆写 `multiData()`，只对传入 role span 即时生成当前 index 的展示值，不建立行容器。view 必须安装生产 `ZzFluentUI::ZzFluentItemDelegate`，禁止用测试专用 delegate 替代或继承这个 final 类型；paint 次数由 viewport event filter 统计 `QEvent::Paint`，不改变 delegate 实现。`QListView` 的 viewport 固定为约 40 行，`uniformItemSizes=true`、`layoutMode=QListView::Batched`、`batchSize<=128`。每轮把 scrollbar 增加一个 `pageStep()`，处理事件后渲染到预分配 `QImage`；重置 model 与 event-filter 计数后开始计时，渲染完停止。每帧强制去重请求行数和 `multiData()` 调用数均 `<=120`、viewport paint 次数为正，且首尾可见 index 随滚动实际变化；不得再使用原始 `data()` 总次数阈值。时间加入 `frame-time` metric，调用数分别加入 `multi-data-calls`、`requested-rows` 和 `viewport-paints` metric。采样前填充统一 metadata，最后传播 `write()` Result。

- [ ] **Step 6: 实现只在测试构建可见的 WindowKit 计数器**

Create `ZzWindowKit/src/private/ZzWindowKitDiagnostics.h/.cpp` with a private, non-installed `ZzWindowKit` -> `Internal` traditional nested namespace and `ZzWindowKitDiagnostics final` class. It owns two `std::atomic<qsizetype>` counters and exposes only benchmark-build methods:

```cpp
#include <ZzWindowKit/ZzWindowKitExport.h>

/**
 * @brief 暴露 benchmark 构建中可观察的 WindowKit 生命周期计数。
 */
class ZZ_WINDOWKIT_EXPORT ZzWindowKitDiagnostics final
{
public:
    ZzWindowKitDiagnostics() = delete;
    /** @brief 记录一个 private backend 已构造。 */
    static void backendConstructed() noexcept;
    /** @brief 记录一个 private backend 已析构。 */
    static void backendDestroyed() noexcept;
    /** @brief 记录一个 QWK agent 已成功附着。 */
    static void agentAttached() noexcept;
    /** @brief 记录一个已附着 QWK agent 已分离。 */
    static void agentDetached() noexcept;
    /** @brief 返回当前存活的 backend 数。 */
    [[nodiscard]] static qsizetype liveBackendCount() noexcept;
    /** @brief 返回当前处于 attached 状态的 agent 数。 */
    [[nodiscard]] static qsizetype liveAgentCount() noexcept;
};
```

`ZzQWindowKitBackend.cpp` 在 backend 构造/析构和真实 attach 成功/attached backend 析构点更新计数；重复清理不能二次递减。不得声称 adapter 能直接统计 QWK 内部 native event filter：该安装点位于上游 private context，除非修改上游源码，否则本层无法可靠观察；本计划保持 QWK 零补丁，依靠 agent 完整析构、100 轮重建和 ASan/UBSan 检测其 filter 清理问题。

`ZzWindowKit/CMakeLists.txt` 必须在 `add_library(ZzWindowKit ...)` 和 `zz_configure_library_target()` 之前完成条件 source list：

```cmake
if(ZZ_BUILD_BENCHMARKS)
    list(APPEND zz_window_kit_sources
        src/private/ZzWindowKitDiagnostics.cpp)
endif()
```

同一条件下对 `ZzWindowKit` 定义 private `ZZ_WINDOWKIT_DIAGNOSTICS=1`。`benchmarks/CMakeLists.txt` 在创建 `ZzWindowLifecycleBenchmark` 后为它定义同名 private 宏，并加入 `${PROJECT_SOURCE_DIR}/ZzWindowKit/src/private` private include directory；不能从更早执行的 `ZzWindowKit/CMakeLists.txt` 配置尚不存在的 benchmark target。backend 的 include 与每个计数调用都必须包在 `#if defined(ZZ_WINDOWKIT_DIAGNOSTICS)` 中；正常 `ZZ_BUILD_BENCHMARKS=OFF` 构建既不编译诊断源，也不引用诊断符号。为使 shared benchmark 可读取同一组计数，诊断类在 benchmark build 中使用 `ZZ_WINDOWKIT_EXPORT`，但 private 头不进入 install file set，安装树不得包含它。

- [ ] **Step 7: 实现 100 窗口生命周期基准**

在真实 Linux display 中先用一次性 builder 完成 application build，然后记录 `QApplication::topLevelWidgets().size()`、`ZzPureApplication::windowCount()`、`liveBackendCount()` 和 `liveAgentCount()` 基线。循环 100 次：调用 `application.createWindow()`，验证 Result 并取得非拥有窗口指针，等待 exposed，`close()`，处理 queued close erase 与 `DeferredDelete`，并要求 `QPointer` 为空。结束后四个计数必须精确回到基线。每轮耗时写 `lifecycle-time`，RSS 写 `rss-bytes`；同样在采样前填充统一 metadata 并传播写入失败。Sanitizer 构建只强制计数、leak 和 UAF，不强制时间阈值。

- [ ] **Step 8: 先运行不带绝对时间阈值的最小绿灯**

Run in a real Linux display session:

```bash
export ZZ_BENCHMARK_COMMIT="$(git rev-parse --verify HEAD)"
: "${ZZ_RUNNER_IMAGE_DIGEST:?set the reviewed sha256 runner image digest}"
: "${ZZ_GPU_IDENTITY:?set the reviewed renderer and driver identity}"
cmake --preset linux-gcc-benchmarks
cmake --build --preset linux-gcc-benchmarks \
  --target ZzStartupBenchmark ZzThemeSwitchBenchmark \
           ZzAnimationBenchmark ZzLargeModelBenchmark \
           ZzWindowLifecycleBenchmark
ctest --preset linux-gcc-benchmarks -L benchmark --output-on-failure
cmake --preset linux-clang-asan-benchmarks
cmake --build --preset linux-clang-asan-benchmarks \
  --target ZzWindowLifecycleBenchmark
ctest --preset linux-clang-asan-benchmarks \
  -R '^benchmark.window-lifecycle$' --output-on-failure
```

Expected: 五个 JSON 场景文件可解析；启动有 30 个正式进程样本；模型调用不超 120；动画对象数不增长；100 窗口后四个本层可观察计数回到基线；ASan/UBSan PASS。该绿灯不在普通 runner 上宣称达到绝对时间。

- [ ] **Step 9: 在指定 Linux 参考机验证绝对阈值**

Run on the designated Linux reference machine:

```bash
export ZZ_BENCHMARK_COMMIT="$(git rev-parse --verify HEAD)"
: "${ZZ_RUNNER_IMAGE_DIGEST:?set the approved reference image digest}"
: "${ZZ_GPU_IDENTITY:?set the approved renderer and driver identity}"
cmake --preset linux-gcc-reference
cmake --build --preset linux-gcc-reference
ctest --preset linux-gcc-reference -L benchmark --output-on-failure
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.startup.json \
  -DZZ_SCENARIO=startup -DZZ_METRIC=external-total \
  -DZZ_MAX_P95=300 -DZZ_MAX_VALUE=300 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.theme-switch.json \
  -DZZ_SCENARIO=theme-switch -DZZ_METRIC=latency \
  -DZZ_MAX_P95=50 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.animation.json \
  -DZZ_SCENARIO=animation -DZZ_METRIC=frame-time \
  -DZZ_MAX_P95=16.7 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.large-model.json \
  -DZZ_SCENARIO=large-model -DZZ_METRIC=frame-time \
  -DZZ_MAX_P95=16.7 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
```

Expected: 启动 P95 和 max 均 <=300 ms，500 控件主题切换 P95<=50 ms，动画和大模型帧 P95<=16.7 ms，窗口 100 轮无泄漏/UAF，且 adapter backend/agent 计数无残留。QWK 上游 native filter 不属于本层直接计数项，必须由重复生命周期、ASan/UBSan 和真机 checklist 共同验证。参考机环境元数据任一缺失时门禁 FAIL。

- [ ] **Step 10: 提交主性能门禁**

```bash
git add benchmarks/CMakeLists.txt \
  benchmarks/ZzAnimationBenchmark.cpp \
  benchmarks/ZzLargeModelBenchmark.cpp \
  benchmarks/ZzStartupBenchmark.cpp \
  benchmarks/ZzStartupProbe/CMakeLists.txt \
  benchmarks/ZzStartupProbe/main.cpp \
  benchmarks/ZzThemeSwitchBenchmark.cpp \
  benchmarks/ZzWindowLifecycleBenchmark.cpp \
  ZzWindowKit/CMakeLists.txt \
  ZzWindowKit/src/private/ZzQWindowKitBackend.cpp \
  ZzWindowKit/src/private/ZzWindowKitDiagnostics.cpp \
  ZzWindowKit/src/private/ZzWindowKitDiagnostics.h
git commit -m "性能：建立启动与交互基准" \
  -m "测量首帧暖启动、500 控件主题、动画帧、10 万行滚动和窗口生命周期。" \
  -m "使用私有 backend/agent 计数和 Sanitizer 阻止窗口生命周期残留。"
```

## Task 5: 增加空闲 CPU、内存和相对回归基线

**Files:**
- Modify: `benchmarks/CMakeLists.txt`
- Create: `benchmarks/ZzIdleProbe/CMakeLists.txt`
- Create: `benchmarks/ZzIdleProbe/main.cpp`
- Create: `benchmarks/testdata/performance-regressed.json`
- Create: `benchmarks/testdata/performance-mismatched-environment.json`
- Create: `cmake/ZzComparePerformanceReport.cmake`
- Create: `docs/performance/PERFORMANCE_BASELINE_ZH.md`
- Create: `docs/performance/reference/linux/startup.json`
- Create: `docs/performance/reference/linux/theme-switch.json`
- Create: `docs/performance/reference/linux/animation.json`
- Create: `docs/performance/reference/linux/large-model.json`
- Create: `docs/performance/reference/linux/window-lifecycle.json`
- Create: `docs/performance/reference/linux/idle.json`

- [ ] **Step 1: 先注册 idle target 得到缺源文件红灯**

Append to `benchmarks/CMakeLists.txt` before creating the subdirectory files:

```cmake
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    add_subdirectory(ZzIdleProbe)
    add_test(NAME benchmark.idle-cpu
        COMMAND ZzIdleProbe
            --report "${CMAKE_BINARY_DIR}/reports/benchmark.idle.json")
    set_tests_properties(benchmark.idle-cpu PROPERTIES
        LABELS "benchmark;linux"
        TIMEOUT 45)
endif()
```

Create `benchmarks/ZzIdleProbe/CMakeLists.txt`:

```cmake
add_executable(ZzIdleProbe main.cpp)
target_link_libraries(ZzIdleProbe PRIVATE
    ZzBenchmarkSupport Zz::PureTools Zz::FluentUI Qt6::Widgets)
zz_enable_project_warnings(ZzIdleProbe)
zz_enable_sanitizers(ZzIdleProbe)
```

Run:

```bash
cmake --preset linux-gcc-benchmarks
```

Expected: configure FAIL 且指向 `benchmarks/ZzIdleProbe/main.cpp` 不存在。

- [ ] **Step 2: 实现无动画空闲 probe**

`ZzIdleProbe` 构建完整首窗，对应用持有的 `ZzFluentUI::ZzThemeController` 实例调用 `setReducedMotion(true)`，不注册业务模块，也不创建 `ZzCore::ZzTaskExecutor` 或后台 task；`ZzPureTools::ZzPureApplication` 没有公开全局 executor getter，因此不得编写一个不存在的 `runningTasks()` 断言。窗口 exposed 后先调用 `ZzBenchmarks::ZzBenchmarkMetadata::populate()`，预热 5 秒，读取起始 `/proc/self/stat`、`/proc/self/status` 和 `QElapsedTimer`，继续运行事件循环 30 秒，再读结束值。

`/proc/self/stat` 必须从最后一个 `)` 后分词，因为 comm 字段可以含空格；剩余字段中索引 11/12 分别是 utime/stime。CPU 公式固定为：

```text
cpuPercent =
    ((endUtime + endStime) - (startUtime + startStime))
    / sysconf(_SC_CLK_TCK)
    / wallSeconds
    * 100.0
```

禁止再除以逻辑 CPU 数；否则高核数机器会隐藏忙循环。VmRSS 从 `VmRSS:` 行读取 KiB 并乘 1024，起始值必须大于 0；先计算 `rawGrowthPercent = (endBytes - startBytes) * 100.0 / startBytes`，报告中的 `rss-growth-percent` 固定写 `std::max(0.0, rawGrowthPercent)`，同时用 `rss-start-bytes`、`rss-end-bytes` 保留真实下降信息。JSON 共写入 `average-cpu-percent`、`rss-start-bytes`、`rss-end-bytes` 和 `rss-growth-percent` 四个 metric，并传播 reporter `write()` 失败。这样相对比较器只接收非负数的契约成立；任一 `/proc` 文件、字段或 `CLK_TCK` 无效时返回非零，不使用 0 代替。

- [ ] **Step 3: 实现不依赖浮点运算的相对回归检查器**

Create `cmake/ZzComparePerformanceReport.cmake`. It accepts `ZZ_BASELINE`、`ZZ_CURRENT` and `ZZ_MAX_REGRESSION_PERCENT` (default 10), requires equal schema/scenario、`cpu`、`memoryBytes`、`os`、`gpu`、`runnerImageDigest`、Qt/compiler/window-system/DPR/refresh-rate and `buildType/shared/lto/sanitizers`, then compares every baseline metric present in the current report. Preset 必须相同，唯一允许的显式兼容对是 baseline `linux-gcc-reference` 与 current `linux-gcc-benchmarks`；这两个 preset 在 Task 2 中具有相同 Release/shared/LTO 配置，前者只额外启用绝对参考标记。commit 只用于溯源，基线与当前 commit 本来就应不同，因此校验格式但不要求相等。Convert non-negative JSON decimals to millionths before integer arithmetic:

```cmake
function(zz_decimal_to_micro output input)
    if(NOT "${input}" MATCHES "^([0-9]+)(\\.([0-9]+))?$")
        message(FATAL_ERROR "Not a non-negative decimal: ${input}")
    endif()
    set(whole "${CMAKE_MATCH_1}")
    set(fraction "${CMAKE_MATCH_3}")
    string(APPEND fraction "000000")
    string(SUBSTRING "${fraction}" 0 6 fraction)
    math(EXPR micro "${whole} * 1000000 + ${fraction}")
    set(${output} "${micro}" PARENT_SCOPE)
endfunction()

function(zz_assert_regression metric field baseline_value current_value)
    zz_decimal_to_micro(baseline_micro "${baseline_value}")
    zz_decimal_to_micro(current_micro "${current_value}")
    math(EXPR whole_increment
        "(${baseline_micro} / 100) * ${ZZ_MAX_REGRESSION_PERCENT}")
    math(EXPR remainder_increment
        "((${baseline_micro} % 100) * ${ZZ_MAX_REGRESSION_PERCENT}) / 100")
    math(EXPR allowed_micro
        "${baseline_micro} + ${whole_increment} + ${remainder_increment}")
    if("${current_micro}" GREATER "${allowed_micro}")
        message(FATAL_ERROR
            "${metric}.${field} regressed from ${baseline_value} "
            "to ${current_value}; limit is ${ZZ_MAX_REGRESSION_PERCENT}%")
    endif()
endfunction()
```

脚本在进入上述函数前必须把 `ZZ_MAX_REGRESSION_PERCENT` 默认设为 `10`，并要求它匹配非负十进制整数且不大于 `100`。增量先做除法再做乘法，禁止恢复为 `baseline_micro * (100 + percent)`；RSS 字节数转换为 millionths 后，后者会在常见的大内存机器上产生 64 位中间值溢出。`zz_decimal_to_micro()` 还必须在 `math(EXPR)` 前拒绝超过 CMake 有符号 64 位范围的输入。

The script uses `string(JSON ... MEMBER)` to enumerate `metrics`; for each metric it compares `p95` and `max`. A baseline value of zero permits only a current value of zero. Missing metrics, differing units, non-numeric fields, malformed commit/preset compatibility or an environment/build fingerprint mismatch are `FATAL_ERROR`.

Create `benchmarks/testdata/performance-regressed.json` with exactly:

```json
{
  "schemaVersion": 1,
  "scenario": "contract",
  "warmupIterations": 10,
  "metrics": {
    "latency": {
      "unit": "ms",
      "count": 5,
      "p50": 3,
      "p95": 111,
      "max": 111
    }
  },
  "environment": {
    "cpu": "test-cpu",
    "memoryBytes": 1024,
    "os": "test-os",
    "gpu": "test-gpu/test-driver",
    "windowSystem": "test-qpa",
    "dpr": 1,
    "refreshRateHz": 60,
    "qtVersion": "6.8.3",
    "compiler": "GNU 13.2.0",
    "runnerImageDigest": "sha256:0000000000000000000000000000000000000000000000000000000000000000"
  },
  "build": {
    "commit": "0123456789abcdef0123456789abcdef01234567",
    "preset": "linux-gcc-reference",
    "buildType": "Release",
    "shared": true,
    "lto": true,
    "sanitizers": "none"
  }
}
```

Create `benchmarks/testdata/performance-mismatched-environment.json` by using the same schema、scenario、build and metric values as `performance-valid.json`, but with exactly one deliberate difference:

```json
{
  "schemaVersion": 1,
  "scenario": "contract",
  "warmupIterations": 10,
  "metrics": {
    "latency": {
      "unit": "ms",
      "count": 5,
      "p50": 3,
      "p95": 100,
      "max": 100
    }
  },
  "environment": {
    "cpu": "test-cpu",
    "memoryBytes": 1024,
    "os": "test-os",
    "gpu": "different-gpu/test-driver",
    "windowSystem": "test-qpa",
    "dpr": 1,
    "refreshRateHz": 60,
    "qtVersion": "6.8.3",
    "compiler": "GNU 13.2.0",
    "runnerImageDigest": "sha256:0000000000000000000000000000000000000000000000000000000000000000"
  },
  "build": {
    "commit": "0123456789abcdef0123456789abcdef01234567",
    "preset": "linux-gcc-reference",
    "buildType": "Release",
    "shared": true,
    "lto": true,
    "sanitizers": "none"
  }
}
```

Register:

```cmake
add_test(NAME benchmark.relative-comparison-valid
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        "-DZZ_CURRENT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -DZZ_MAX_REGRESSION_PERCENT=10
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake")
add_test(NAME benchmark.relative-comparison-rejects-regression
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        "-DZZ_CURRENT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-regressed.json"
        -DZZ_MAX_REGRESSION_PERCENT=10
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake")
add_test(NAME benchmark.relative-comparison-rejects-environment-mismatch
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        "-DZZ_CURRENT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-mismatched-environment.json"
        -DZZ_MAX_REGRESSION_PERCENT=10
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake")
set_tests_properties(
    benchmark.relative-comparison-rejects-regression
    benchmark.relative-comparison-rejects-environment-mismatch
    PROPERTIES WILL_FAIL TRUE LABELS "benchmark;unit")
set_tests_properties(benchmark.relative-comparison-valid PROPERTIES
    LABELS "benchmark;unit")
```

- [ ] **Step 4: 建立参考机基线文档和实测 JSON**

`PERFORMANCE_BASELINE_ZH.md` 必须记录 CPU 型号/核数、RAM、GPU/驱动、显示刷新率、DPR、Linux/桌面/窗口协议、Qt 精确版本、GCC/libstdc++ 版本、commit、CMake preset、预热/迭代次数和原始 JSON 路径。首次数值必须来自实测，不手工伪造。

`docs/performance/reference/linux/` 下六个 JSON 只能逐字复制指定参考机上 `linux-gcc-reference` 对应的 reporter 输出，不得合并、手改数值或把同名 metric 折叠到一起。文件名固定映射为 `startup.json`、`theme-switch.json`、`animation.json`、`large-model.json`、`window-lifecycle.json`、`idle.json`；每个文件保留自己的 scenario、全部 metrics 和相同 runner image digest。普通 CI 只在 CPU、内存、OS、GPU/驱动、digest、Qt、compiler、window system、DPR、刷新率和 build 配置全部一致时做相对比较。

- [ ] **Step 5: 固定绝对与相对双门禁**

- 参考机：执行架构文档绝对阈值，空闲 CPU 30 秒平均 `<0.5%`，未解释 RSS 增长 `<=10%`。
- 普通 CI：只在环境指纹相同时逐场景对比 `docs/performance/reference/linux/*.json`，P95、max 或 RSS 回归超过 10% 失败；环境不同则 FAIL 并要求选择正确 runner，不降级为记录模式。
- 不在噪声 CI 机器上执行或宣称绝对 300/50/16.7 ms 通过。

- [ ] **Step 6: 运行空闲、内存和相对检查绿灯**

Run:

```bash
export ZZ_BENCHMARK_COMMIT="$(git rev-parse --verify HEAD)"
: "${ZZ_RUNNER_IMAGE_DIGEST:?set the reviewed sha256 runner image digest}"
: "${ZZ_GPU_IDENTITY:?set the reviewed renderer and driver identity}"
cmake --preset linux-gcc-benchmarks
cmake --build --preset linux-gcc-benchmarks --target ZzIdleProbe
ctest --preset linux-gcc-benchmarks \
  -R '^benchmark\.(idle-cpu|relative-comparison-(valid|rejects-regression|rejects-environment-mismatch))$' \
  --output-on-failure
for scenario in startup theme-switch animation large-model window-lifecycle idle; do
  cmake \
    -DZZ_BASELINE="docs/performance/reference/linux/${scenario}.json" \
    -DZZ_CURRENT="build/linux-gcc-benchmarks/reports/benchmark.${scenario}.json" \
    -DZZ_MAX_REGRESSION_PERCENT=10 \
    -P cmake/ZzComparePerformanceReport.cmake
done
cmake --preset linux-clang-asan-benchmarks
cmake --build --preset linux-clang-asan-benchmarks \
  --target ZzWindowLifecycleBenchmark
ctest --preset linux-clang-asan-benchmarks \
  -R '^benchmark.window-lifecycle$' --output-on-failure
```

Expected: idle JSON 完整；相同报告比较 PASS；11% 回归 fixture 和仅 GPU 不同的环境 fixture 均由 `WILL_FAIL` 契约正确拒绝；ASan/UBSan PASS。参考机另执行：

```bash
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.idle.json \
  -DZZ_SCENARIO=idle -DZZ_METRIC=average-cpu-percent \
  -DZZ_MAX_VALUE=0.5 -DZZ_STRICT_MAX=ON \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.idle.json \
  -DZZ_SCENARIO=idle -DZZ_METRIC=rss-growth-percent \
  -DZZ_MAX_VALUE=10 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
```

Expected on the reference machine: average CPU 严格小于 0.5%，RSS 增长不超过 10%。

- [ ] **Step 7: 提交实测基线与回归规则**

```bash
git add benchmarks/CMakeLists.txt \
  benchmarks/ZzIdleProbe/CMakeLists.txt \
  benchmarks/ZzIdleProbe/main.cpp \
  benchmarks/testdata/performance-mismatched-environment.json \
  benchmarks/testdata/performance-regressed.json \
  cmake/ZzComparePerformanceReport.cmake \
  docs/performance/PERFORMANCE_BASELINE_ZH.md \
  docs/performance/reference/linux/startup.json \
  docs/performance/reference/linux/theme-switch.json \
  docs/performance/reference/linux/animation.json \
  docs/performance/reference/linux/large-model.json \
  docs/performance/reference/linux/window-lifecycle.json \
  docs/performance/reference/linux/idle.json
git commit -m "性能：记录空闲与内存回归基线" \
  -m "以进程 CPU 时间和墙钟时间计算三十秒空闲占用，不用核数稀释。" \
  -m "只在环境指纹一致时执行参考机绝对阈值和普通 CI 相对回归。"
```

## Task 6: 建立平台编译、重定位、公开头与二进制 CTest

**Files:**
- Create: `tests/Platform/CMakeLists.txt`
- Create: `tests/Platform/ZzPlatformCompileTest.cpp`
- Create: `tests/Platform/ZzPlatformGateContext.cmake.in`
- Create: `tests/Platform/ZzPackageRelocationTest.cmake`
- Create: `tests/Platform/ZzBinaryDependencyCheck.cmake`
- Create: `tests/PublicHeaderConsumer/CMakeLists.txt`
- Modify: `cmake/ZzArchitectureChecks.cmake`
- Create: `tests/Architecture/ZzArchitectureAudit.cmake`
- Modify: `tests/Architecture/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 先注册平台 target 得到缺源文件红灯**

Append to `tests/CMakeLists.txt` without removing the existing Architecture and InstallConsumer registration:

```cmake
add_subdirectory(Platform)
```

Create `tests/Platform/CMakeLists.txt` initially with:

```cmake
add_executable(ZzPlatformCompileTest ZzPlatformCompileTest.cpp)
target_link_libraries(ZzPlatformCompileTest PRIVATE
    Zz::Core
    Zz::WindowKit
    Zz::FluentFoundation
    Zz::FluentUI
    Zz::AppCore
    Zz::PureTools
    Qt6::Core
    Qt6::Gui
)
zz_enable_project_warnings(ZzPlatformCompileTest)
zz_enable_sanitizers(ZzPlatformCompileTest)
```

Run:

```bash
cmake --preset linux-gcc-debug
```

Expected: configure FAIL，唯一新错误是 `tests/Platform/ZzPlatformCompileTest.cpp` 不存在。

- [ ] **Step 2: 实现只消费公开 API 的平台编译测试**

Create `tests/Platform/ZzPlatformCompileTest.cpp` with:

```cpp
#include <ZzCore/ZzCoreVersion.h>
#include <ZzFluentUI/ZzFluentVersion.h>
#include <ZzFluentUI/ZzFluentWidgetVersion.h>
#include <ZzPureTools/ZzAppCoreVersion.h>
#include <ZzPureTools/ZzPureToolsVersion.h>
#include <ZzWindowKit/ZzWindowKitVersion.h>

#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QSysInfo>
#include <QtGui/QGuiApplication>

#if defined(Q_OS_WIN)
#  include <windows.h>
static_assert(sizeof(void *) == 8);
#elif defined(Q_OS_MACOS)
#  include <TargetConditionals.h>
#  if !defined(__arm64__) && !defined(__x86_64__)
#    error Unsupported macOS architecture
#  endif
#elif defined(Q_OS_LINUX)
#  include <unistd.h>
#else
#  error Unsupported platform
#endif

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    const QStringList versions{
        ZzCore::ZzCoreVersion::toString(),
        ZzWindowKit::ZzWindowKitVersion::toString(),
        ZzFluentUI::ZzFluentVersion::toString(),
        ZzFluentUI::ZzFluentWidgetVersion::toString(),
        ZzPureTools::ZzAppCoreVersion::toString(),
        ZzPureTools::ZzPureToolsVersion::toString(),
    };
    for (const QString &version : versions) {
        if (version.isEmpty()) {
            return 1;
        }
    }
#if defined(Q_OS_WIN)
    if (GetModuleHandleW(nullptr) == nullptr) {
        return 2;
    }
#elif defined(Q_OS_MACOS)
    if (TARGET_OS_OSX == 0) {
        return 3;
    }
#elif defined(Q_OS_LINUX)
    if (getpid() <= 0) {
        return 4;
    }
#endif
    qInfo().noquote() << QSysInfo::prettyProductName()
                      << QSysInfo::buildCpuArchitecture()
                      << QGuiApplication::platformName();
    return 0;
}
```

Append to `tests/Platform/CMakeLists.txt`:

```cmake
add_test(NAME platform.compile COMMAND ZzPlatformCompileTest)
set_tests_properties(platform.compile PROPERTIES LABELS "platform")
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set_tests_properties(platform.compile PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
```

Run:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzPlatformCompileTest
ctest --preset linux-gcc-debug -R '^platform\.compile$' --output-on-failure
```

Expected: target build and the single CTest PASS；源文件没有 Qt Private、QWK 或第三方 include。

- [ ] **Step 3: 固定 producer/consumer 工具链上下文并逐个编译安装头**

Create `tests/Platform/ZzPlatformGateContext.cmake.in` with:

```cmake
set(ZZ_GENERATOR [==[@CMAKE_GENERATOR@]==])
set(ZZ_GENERATOR_INSTANCE [==[@CMAKE_GENERATOR_INSTANCE@]==])
set(ZZ_GENERATOR_PLATFORM [==[@CMAKE_GENERATOR_PLATFORM@]==])
set(ZZ_GENERATOR_TOOLSET [==[@CMAKE_GENERATOR_TOOLSET@]==])
set(ZZ_MAKE_PROGRAM [==[@CMAKE_MAKE_PROGRAM@]==])
set(ZZ_C_COMPILER [==[@CMAKE_C_COMPILER@]==])
set(ZZ_CXX_COMPILER [==[@CMAKE_CXX_COMPILER@]==])
set(ZZ_OBJCXX_COMPILER [==[@CMAKE_OBJCXX_COMPILER@]==])
set(ZZ_BUILD_TYPE [==[@CMAKE_BUILD_TYPE@]==])
set(ZZ_CONFIGURATION_TYPES [==[@CMAKE_CONFIGURATION_TYPES@]==])
set(ZZ_QT_PREFIX [==[@ZZ_QT_PREFIX@]==])
set(ZZ_OSX_ARCHITECTURES [==[@CMAKE_OSX_ARCHITECTURES@]==])
set(ZZ_OSX_DEPLOYMENT_TARGET [==[@CMAKE_OSX_DEPLOYMENT_TARGET@]==])
set(ZZ_OSX_SYSROOT [==[@CMAKE_OSX_SYSROOT@]==])
set(ZZ_BUILD_SHARED [==[@BUILD_SHARED_LIBS@]==])
set(ZZ_ENABLE_LTO [==[@ZZ_ENABLE_LTO@]==])
set(ZZ_CMAKE_PRESET [==[$ENV{ZZ_CMAKE_PRESET}]==])
set(ZZ_PRIMARY_BINARY_DIR [==[@CMAKE_BINARY_DIR@]==])
set(ZZ_CTEST_COMMAND [==[@CMAKE_CTEST_COMMAND@]==])
set(ZZ_CMAKE_OBJDUMP [==[@CMAKE_OBJDUMP@]==])
set(ZZ_DUMPBIN [==[$ENV{ZZ_DUMPBIN}]==])
set(ZZ_MINGW_OBJDUMP [==[$ENV{ZZ_MINGW_OBJDUMP}]==])
```

Create `tests/PublicHeaderConsumer/CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.23)
project(ZzPublicHeaderConsumer LANGUAGES CXX)

if(NOT DEFINED ZZ_PACKAGE_ROOT OR "${ZZ_PACKAGE_ROOT}" STREQUAL "")
    message(FATAL_ERROR "ZZ_PACKAGE_ROOT is required")
endif()
file(GLOB_RECURSE package_configs LIST_DIRECTORIES FALSE
    "${ZZ_PACKAGE_ROOT}/*/cmake/ZzPureToolsPro/ZzPureToolsProConfig.cmake")
list(LENGTH package_configs package_config_count)
if(NOT "${package_config_count}" EQUAL 1)
    message(FATAL_ERROR
        "Expected one ZzPureToolsProConfig.cmake below package root")
endif()
list(GET package_configs 0 package_config)
cmake_path(GET package_config PARENT_PATH package_config_dir)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)
find_package(ZzPureToolsPro 0.1 CONFIG REQUIRED
    PATHS "${package_config_dir}"
    NO_DEFAULT_PATH)

file(GLOB_RECURSE installed_headers CONFIGURE_DEPENDS
    "${ZZ_PACKAGE_ROOT}/include/*.h"
    "${ZZ_PACKAGE_ROOT}/include/*.hh"
    "${ZZ_PACKAGE_ROOT}/include/*.hpp"
    "${ZZ_PACKAGE_ROOT}/include/*.hxx")
if(NOT installed_headers)
    message(FATAL_ERROR "No installed public headers found")
endif()

add_custom_target(ZzInstalledPublicHeaders ALL)
set(installed_header_names)
foreach(header IN LISTS installed_headers)
    file(RELATIVE_PATH include_name "${ZZ_PACKAGE_ROOT}/include" "${header}")
    file(TO_CMAKE_PATH "${include_name}" include_name)
    list(APPEND installed_header_names "${include_name}")
endforeach()
list(SORT installed_header_names)

set(zz_header_owners
    Zz::Core
    Zz::WindowKit
    Zz::FluentFoundation
    Zz::FluentUI
    Zz::AppCore
    Zz::PureTools)
set(owner_manifest)
foreach(owner IN LISTS zz_header_owners)
    get_target_property(owner_headers "${owner}" ZZ_PUBLIC_HEADERS)
    if("${owner_headers}" STREQUAL "owner_headers-NOTFOUND"
       OR "${owner_headers}" STREQUAL "")
        message(FATAL_ERROR "${owner} exports no ZZ_PUBLIC_HEADERS manifest")
    endif()
    foreach(include_name IN LISTS owner_headers)
        if(IS_ABSOLUTE "${include_name}" OR "${include_name}" MATCHES "(^|/)\\.\\.(/|$)")
            message(FATAL_ERROR
                "${owner} declares unsafe public header path: ${include_name}")
        endif()
        list(FIND owner_manifest "${include_name}" duplicate_index)
        if(NOT "${duplicate_index}" EQUAL -1)
            message(FATAL_ERROR
                "Public header has more than one owner: ${include_name}")
        endif()
        list(APPEND owner_manifest "${include_name}")

        string(MAKE_C_IDENTIFIER "${include_name}" header_id)
        set(source "${CMAKE_CURRENT_BINARY_DIR}/${header_id}.cpp")
        file(GENERATE OUTPUT "${source}"
            CONTENT "#include <${include_name}>\n")
        set_source_files_properties("${source}" PROPERTIES GENERATED TRUE)
        add_library("ZzInstalledHeader_${header_id}" OBJECT "${source}")
        target_compile_features(
            "ZzInstalledHeader_${header_id}" PRIVATE cxx_std_20)
        set_target_properties("ZzInstalledHeader_${header_id}" PROPERTIES
            CXX_EXTENSIONS OFF)
        target_link_libraries("ZzInstalledHeader_${header_id}" PRIVATE "${owner}")
        add_dependencies(ZzInstalledPublicHeaders
            "ZzInstalledHeader_${header_id}")
    endforeach()
endforeach()
list(SORT owner_manifest)
if(NOT "${owner_manifest}" STREQUAL "${installed_header_names}")
    message(FATAL_ERROR
        "Installed headers and owner manifests differ.\n"
        "Installed: ${installed_header_names}\n"
        "Owned: ${owner_manifest}")
endif()
```

基线 helper 已把每个 target 的源码公共头与生成导出头记录为 exported `ZZ_PUBLIC_HEADERS` 属性。这里要求安装目录集合与六个 owner manifest 完全相等，并拒绝重复 owner；每个 generated object target 只有一个安装头翻译单元且只链接它的唯一 owner。它不接收源码/build include 目录；若某个头依赖其他组件却未由 owner 的 public interface 声明，或者泄露 QWK、Qt Private、未声明 package path，configure/build 必须失败，不能再由其余五个 target 掩盖。

- [ ] **Step 4: 实现真正删除 prefix A 的重定位测试**

Create `tests/Platform/ZzPackageRelocationTest.cmake`. It must validate `ZZ_SOURCE_DIR`、`ZZ_TEST_ROOT`、`ZZ_CONTEXT_FILE`、`ZZ_CONFIG`, reject a filesystem root or source-root target, include the context, and require the normalized test root to remain below the captured primary binary directory. Then execute this exact sequence:

```cmake
cmake_minimum_required(VERSION 3.23)
foreach(required ZZ_SOURCE_DIR ZZ_TEST_ROOT ZZ_CONTEXT_FILE ZZ_CONFIG)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
if(NOT EXISTS "${ZZ_CONTEXT_FILE}")
    message(FATAL_ERROR "Context file does not exist: ${ZZ_CONTEXT_FILE}")
endif()
include("${ZZ_CONTEXT_FILE}")

cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE source_dir)
cmake_path(ABSOLUTE_PATH ZZ_TEST_ROOT NORMALIZE OUTPUT_VARIABLE test_root)
cmake_path(GET test_root ROOT_PATH test_root_anchor)
cmake_path(IS_PREFIX ZZ_PRIMARY_BINARY_DIR "${test_root}"
    NORMALIZE test_is_below_build)
if("${test_root}" STREQUAL "${test_root_anchor}"
   OR "${test_root}" STREQUAL "${source_dir}"
   OR NOT test_is_below_build)
    message(FATAL_ERROR "Unsafe relocation test root: ${test_root}")
endif()
set(ZZ_TEST_ROOT "${test_root}")

function(zz_run label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr)
    if(NOT "${result}" EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with exit code ${result}\n"
            "stdout:\n${stdout}\n"
            "stderr:\n${stderr}")
    endif()
    message(STATUS "${label} passed")
endfunction()

set(producer "${ZZ_TEST_ROOT}/producer")
set(prefix_a "${ZZ_TEST_ROOT}/prefix-a")
set(prefix_b "${ZZ_TEST_ROOT}/prefix-b")
set(install_consumer "${ZZ_TEST_ROOT}/install-consumer")
set(header_consumer "${ZZ_TEST_ROOT}/public-header-consumer")
file(REMOVE_RECURSE
    "${producer}" "${prefix_a}" "${prefix_b}"
    "${install_consumer}" "${header_consumer}")

set(generator_args -G "${ZZ_GENERATOR}")
if(NOT "${ZZ_GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND generator_args -A "${ZZ_GENERATOR_PLATFORM}")
endif()
if(NOT "${ZZ_GENERATOR_TOOLSET}" STREQUAL "")
    list(APPEND generator_args -T "${ZZ_GENERATOR_TOOLSET}")
endif()

set(toolchain_args
    "-DCMAKE_C_COMPILER:FILEPATH=${ZZ_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${ZZ_CXX_COMPILER}")
if(NOT "${ZZ_GENERATOR_INSTANCE}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_GENERATOR_INSTANCE:PATH=${ZZ_GENERATOR_INSTANCE}")
endif()
if("${ZZ_GENERATOR}" MATCHES "Ninja"
   AND NOT "${ZZ_MAKE_PROGRAM}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=${ZZ_MAKE_PROGRAM}")
endif()
if(NOT "${ZZ_OBJCXX_COMPILER}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_OBJCXX_COMPILER:FILEPATH=${ZZ_OBJCXX_COMPILER}")
endif()
if(NOT "${ZZ_BUILD_TYPE}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_BUILD_TYPE:STRING=${ZZ_BUILD_TYPE}")
endif()
if(NOT "${ZZ_OSX_ARCHITECTURES}" STREQUAL "")
    string(REPLACE ";" "\\;" osx_architectures
        "${ZZ_OSX_ARCHITECTURES}")
    list(APPEND toolchain_args
        "-DCMAKE_OSX_ARCHITECTURES:STRING=${osx_architectures}")
endif()
if(NOT "${ZZ_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${ZZ_OSX_DEPLOYMENT_TARGET}")
endif()
if(NOT "${ZZ_OSX_SYSROOT}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_OSX_SYSROOT:PATH=${ZZ_OSX_SYSROOT}")
endif()

set(build_config_args)
set(ctest_config_args)
if(NOT "${ZZ_CONFIG}" STREQUAL "")
    list(APPEND build_config_args --config "${ZZ_CONFIG}")
    list(APPEND ctest_config_args -C "${ZZ_CONFIG}")
endif()
string(REPLACE ";" "\\;" consumer_prefix
    "${prefix_b};${ZZ_QT_PREFIX}")

zz_run("producer configure" "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}" -B "${producer}" ${generator_args}
    ${toolchain_args}
    "-DCMAKE_PREFIX_PATH:PATH=${ZZ_QT_PREFIX}"
    "-DZZ_QT_PREFIX:PATH=${ZZ_QT_PREFIX}"
    "-DCMAKE_INSTALL_PREFIX:PATH=${prefix_a}"
    "-DBUILD_SHARED_LIBS:BOOL=${ZZ_BUILD_SHARED}"
    "-DZZ_ENABLE_LTO:BOOL=${ZZ_ENABLE_LTO}"
    -DZZ_BUILD_TESTS=OFF -DZZ_BUILD_EXAMPLES=OFF
    -DZZ_BUILD_BENCHMARKS=OFF -DZZ_WARNINGS_AS_ERRORS=ON)
zz_run("producer build" "${CMAKE_COMMAND}"
    --build "${producer}" ${build_config_args})
zz_run("producer install" "${CMAKE_COMMAND}"
    --install "${producer}" --prefix "${prefix_a}" ${build_config_args})

file(MAKE_DIRECTORY "${prefix_b}")
file(COPY "${prefix_a}/" DESTINATION "${prefix_b}")
file(REMOVE_RECURSE "${prefix_a}")
if(EXISTS "${prefix_a}")
    message(FATAL_ERROR "prefix A still exists after relocation")
endif()
file(GLOB_RECURSE package_configs LIST_DIRECTORIES FALSE
    "${prefix_b}/*/cmake/ZzPureToolsPro/ZzPureToolsProConfig.cmake")
list(LENGTH package_configs package_config_count)
if(NOT "${package_config_count}" EQUAL 1)
    message(FATAL_ERROR "prefix B must contain exactly one package Config")
endif()
list(GET package_configs 0 package_config)
cmake_path(GET package_config PARENT_PATH package_config_dir)

zz_run("install consumer configure" "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}/tests/InstallConsumer"
    -B "${install_consumer}" ${generator_args} ${toolchain_args}
    "-DCMAKE_PREFIX_PATH:STRING=${consumer_prefix}"
    "-DZzPureToolsPro_DIR:PATH=${package_config_dir}"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
    -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    "-DZZ_PACKAGE_ROOT:PATH=${prefix_b}")
zz_run("install consumer build" "${CMAKE_COMMAND}"
    --build "${install_consumer}" ${build_config_args})
zz_run("install consumer test" "${ZZ_CTEST_COMMAND}"
    --test-dir "${install_consumer}" ${ctest_config_args} --output-on-failure)

zz_run("public header consumer configure" "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}/tests/PublicHeaderConsumer"
    -B "${header_consumer}" ${generator_args} ${toolchain_args}
    "-DCMAKE_PREFIX_PATH:STRING=${consumer_prefix}"
    "-DZzPureToolsPro_DIR:PATH=${package_config_dir}"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
    -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    "-DZZ_PACKAGE_ROOT:PATH=${prefix_b}")
zz_run("public header consumer build" "${CMAKE_COMMAND}"
    --build "${header_consumer}" ${build_config_args}
    --target ZzInstalledPublicHeaders)
```

Append these executable assertions after the sequence. They require exactly one package-dir cache entry in each consumer, then reject every known producer/developer path from installed CMake files:

```cmake
foreach(consumer_dir IN ITEMS "${install_consumer}" "${header_consumer}")
    file(READ "${consumer_dir}/CMakeCache.txt" consumer_cache)
    string(REGEX MATCHALL
        "ZzPureToolsPro_DIR:[^=\\r\\n]*=[^\\r\\n]*"
        package_dir_entries "${consumer_cache}")
    list(LENGTH package_dir_entries package_dir_entry_count)
    if(NOT "${package_dir_entry_count}" EQUAL 1)
        message(FATAL_ERROR
            "${consumer_dir} must contain exactly one ZzPureToolsPro_DIR")
    endif()
    list(GET package_dir_entries 0 package_dir_entry)
    file(TO_CMAKE_PATH "${package_config_dir}" expected_package_dir)
    string(REPLACE "\\\\" "/" package_dir_entry "${package_dir_entry}")
    string(FIND "${package_dir_entry}"
        "=${expected_package_dir}" package_dir_position)
    if("${package_dir_position}" EQUAL -1)
        message(FATAL_ERROR
            "${consumer_dir} did not resolve the relocated package: "
            "${package_dir_entry}")
    endif()
endforeach()

file(GLOB_RECURSE installed_cmake_files LIST_DIRECTORIES FALSE
    "${prefix_b}/*.cmake")
if(NOT installed_cmake_files)
    message(FATAL_ERROR "Relocated prefix contains no installed CMake files")
endif()
set(forbidden_paths
    "${source_dir}"
    "${ZZ_PRIMARY_BINARY_DIR}"
    "${producer}"
    "${prefix_a}"
    "${prefix_b}"
    "${ZZ_QT_PREFIX}")
foreach(cmake_file IN LISTS installed_cmake_files)
    file(READ "${cmake_file}" cmake_text)
    string(REPLACE "\\\\" "/" normalized_text "${cmake_text}")
    foreach(forbidden_path IN LISTS forbidden_paths)
        file(TO_CMAKE_PATH "${forbidden_path}" normalized_forbidden)
        string(FIND "${normalized_text}"
            "${normalized_forbidden}" forbidden_position)
        if(NOT "${forbidden_position}" EQUAL -1)
            message(FATAL_ERROR
                "Absolute path leaked into ${cmake_file}: ${normalized_forbidden}")
        endif()
    endforeach()
    string(CONCAT zz_unix_home_pattern "/ho" "me/|/Us" "ers/")
    if("${normalized_text}" MATCHES
       "(${zz_unix_home_pattern}|[A-Za-z]:/[^;$<\\\"]*)")
        message(FATAL_ERROR
            "Developer absolute path leaked into ${cmake_file}")
    endif()
endforeach()
```

`prefix_b` is allowed only as the test's runtime `ZZ_PACKAGE_ROOT`; installed Config/Targets content must remain prefix-relative and therefore the installed-file scan rejects it too.

- [ ] **Step 5: 增加集中式架构扫描和 link-direction manifest**

Extend the existing `cmake/ZzArchitectureChecks.cmake` without deleting or renaming the baseline `zz_add_public_header_probe()` and `zz_add_public_header_directory()` functions. Add reusable audit functions that accumulate `RULE_ID:path:line` findings and emit one final `FATAL_ERROR`. Create `tests/Architecture/ZzArchitectureAudit.cmake` to call them with these exact roots and rules:

| Rule id | Roots | Rejected condition |
|---|---|---|
| `TYPE_PREFIX` | first-party `include/src/foundation/widgets/appcore` | custom `class`、`struct`、`enum`、`concept` name does not start with `Zz` |
| `TYPE_FILENAME` | same roots | the primary custom type does not equal the file stem; `main.cpp` is the only filename exception |
| `CHAINED_NAMESPACE` | same roots | code after comments/strings are removed matches `namespace[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*::` |
| `PUBLIC_API_DOXYGEN` | each component `include` root | public type or public method lacks an immediately preceding `/** ... */` containing `@brief` and at least one CJK character |
| `QT_PRIVATE_OR_QWK` | all first-party roots | include contains `Qt*/private`、`*_p.h`、`QWKCore`、`QWKWidgets` or source token `QWindowKit::`; only `ZzWindowKit/src/private/ZzQWindowKitBackend.*` may use QWK |
| `PRESENTATION_BUSINESS_DEPENDENCY` | `ZzFluentUI/widgets` and `ZzPureTools/widgets` | include path matches case-insensitive `repository`、`database`、`networkclient` or `domainentity` |
| `TARGET_LINK_DIRECTION` | generated target manifest | any of the six targets links a component above it in the architecture graph, or QWindowKit appears in an interface |

The scanner must strip block/line comments and string literals for token rules, keep raw text for Doxygen, scan every finding rather than stop at the first, and fail on an empty/nonexistent root. 类型解析规则固定如下，不能用“看到第一个 class token”代替：

1. `.h` 只把带 `{` 定义体的 class/struct/enum 以及匹配 `concept ZzName =` 的 Concept 当作自定义类型，忽略 `class QEvent;`、`class ZzFooPrivate;` 等前置声明；每个自定义定义都检查 `Zz` 前缀，至少一个定义必须与文件 stem 一致并作为主类型。只有生成的 export header 或明确的纯聚合头可以没有主定义，但它们仍必须通过逐头编译。
2. `.cpp` 从去除注释/字符串后的 `ZzType::method` out-of-line 定义识别实现主类；存在实现主类时必须与文件 stem 一致。匿名 namespace 或 translation-unit private 的辅助定义仍执行 `TYPE_PREFIX`，但不参与 `TYPE_FILENAME`；没有类成员定义的纯函数/注册翻译单元必须在审计器的精确 allowlist 中登记文件路径和原因，不能全局跳过 `.cpp`。
3. 同一文件出现两个不同的 out-of-line owner 时直接报告 `TYPE_FILENAME`，除非其中一个是与 stem 相同主类的嵌套类型；`main.cpp` 是唯一无需登记的通用文件名例外。

Generate the target manifest in `tests/Architecture/CMakeLists.txt`:

```cmake
set(zz_target_manifest
    "${CMAKE_CURRENT_BINARY_DIR}/ZzTargetLinks-$<CONFIG>.txt")
file(GENERATE OUTPUT "${zz_target_manifest}" CONTENT
"ZzCore|$<TARGET_PROPERTY:ZzCore,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzCore,INTERFACE_LINK_LIBRARIES>\n
ZzWindowKit|$<TARGET_PROPERTY:ZzWindowKit,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzWindowKit,INTERFACE_LINK_LIBRARIES>\n
ZzFluentFoundation|$<TARGET_PROPERTY:ZzFluentFoundation,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzFluentFoundation,INTERFACE_LINK_LIBRARIES>\n
ZzFluentUI|$<TARGET_PROPERTY:ZzFluentUI,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzFluentUI,INTERFACE_LINK_LIBRARIES>\n
ZzAppCore|$<TARGET_PROPERTY:ZzAppCore,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzAppCore,INTERFACE_LINK_LIBRARIES>\n
ZzPureTools|$<TARGET_PROPERTY:ZzPureTools,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzPureTools,INTERFACE_LINK_LIBRARIES>\n")
add_test(NAME architecture.complete-audit
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TARGET_MANIFEST=${zz_target_manifest}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzArchitectureAudit.cmake")
set_tests_properties(architecture.complete-audit PROPERTIES
    LABELS "architecture")
```

The link allowlist is exact: Core may use ZzLog/Qt Core/Concurrent; WindowKit may use Core/Qt Core/Gui/Widgets and private QWK; FluentFoundation may use Core/Qt Core/Gui; FluentUI may use FluentFoundation/Qt Widgets/Svg; AppCore may use Core/Qt Core; PureTools may publicly use AppCore/FluentFoundation/Qt Widgets and privately use WindowKit/FluentUI. `$<LINK_ONLY:...>` may close a static link but must not add include or compile interface requirements.

- [ ] **Step 6: 实现逐平台二进制 allow/deny 检查**

In `tests/Platform/CMakeLists.txt`, generate `ZzBinaryContext-$<CONFIG>.cmake` containing `CMAKE_SYSTEM_NAME`、compiler id、`BUILD_SHARED_LIBS`、`CMAKE_BINARY_DIR`、`ZZ_QT_PREFIX`、`CMAKE_OBJDUMP`、`$ENV{ZZ_DUMPBIN}`、`$ENV{ZZ_MINGW_OBJDUMP}`、expected macOS architecture, `$<TARGET_FILE:ZzPlatformCompileTest>`、`$<TARGET_FILE:ZzCore>`、`$<TARGET_FILE:ZzWindowKit>`、`$<TARGET_FILE:ZzFluentFoundation>`、`$<TARGET_FILE:ZzFluentUI>`、`$<TARGET_FILE:ZzAppCore>` and `$<TARGET_FILE:ZzPureTools>`. Register:

```cmake
add_test(NAME platform.binary-dependencies
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_CONTEXT_FILE=${CMAKE_CURRENT_BINARY_DIR}/ZzBinaryContext-$<CONFIG>.cmake"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzBinaryDependencyCheck.cmake")
set_tests_properties(platform.binary-dependencies PROPERTIES
    LABELS "platform;binary")
```

Create `tests/Platform/ZzBinaryDependencyCheck.cmake` with these required inputs and decisions:

| Platform/tool | Required success checks | Direct-dependency allowlist | Denylist |
|---|---|---|---|
| Linux `readelf` + `ldd` | each ELF scans; executable `ldd` contains no `not found`; RPATH/RUNPATH contains neither source nor build tree | `libZz*`, `libQt6(Core|Gui|Widgets|Svg|Concurrent)`, `libstdc++`, `libgcc_s`, `libc`, `libm`, `libpthread`, `libdl`, `librt`, ELF loader | case-insensitive `qwindowkit`, an absolute build/source path |
| MSVC `dumpbin /headers /dependents` | machine is x64; every DLL name is parsed; `ZZ_DUMPBIN` exists | `Zz*.dll`, `Qt6*.dll`, `KERNEL32|USER32|GDI32|SHELL32|OLE32|OLEAUT32|ADVAPI32|COMDLG32|COMCTL32|DWMAPI|UXTHEME|SHLWAPI|IMM32|WINMM|WS2_32|VERSION|BCRYPT|SETUPAPI|USERENV|AUTHZ|NTDLL.dll`, `VCRUNTIME140*`, `MSVCP140*`, `ucrtbase`, `api-ms-win-*` | `libgcc`、`libstdc++`、`libwinpthread`、`mingw`、`qwindowkit` |
| MinGW Qt SDK `objdump -f -p` | format is `pei-x86-64`; tool path equals captured Qt kit tool; every `DLL Name` is parsed | `Zz*.dll`, `Qt6*.dll`, the same explicit Windows system-DLL set, `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` | `VCRUNTIME`、`MSVCP`、`qwindowkit` |
| macOS `otool -L` + `lipo -archs` | each Mach-O has exactly the preset architecture; install names contain neither source nor build tree | `@rpath/libZz*.dylib`, `@rpath/Qt*.framework/Versions/*/Qt*`, `@rpath/libQt6*.dylib`, `/System/Library/*`, `/usr/lib/*` | case-insensitive `qwindowkit`, any second architecture, an absolute build/source path |

实现首先根据 `BUILD_SHARED_LIBS` 构造扫描集合：shared 模式必须包含 `ZzPlatformCompileTest` 和六个 `Zz` 动态库，且每个路径都是存在的普通文件；static 模式必须确认六个库路径均为当前工具链的静态归档，但只把最终 `ZzPlatformCompileTest` 送给动态依赖工具。集合为空、目标类型与模式不符或任一捕获路径不存在都立即 `FATAL_ERROR`。依赖名统一取 basename、Windows 转小写，再用锚定规则匹配；不得用未锚定的 `string(FIND)` 把 `not-allowed-Qt6Core.dll.backup` 误判为允许项。

MinGW 分支在执行任何扫描前必须同时要求 `ZZ_CMAKE_OBJDUMP` 和 `ZZ_MINGW_OBJDUMP` 是存在的普通文件。分别用 `cmake_path(ABSOLUTE_PATH ... NORMALIZE)` 得到规范化绝对路径，再把 Windows 路径转为小写进行完全相等比较；不相等时必须打印两个规范化路径并失败。只允许执行比较后的 `ZZ_CMAKE_OBJDUMP`，禁止回退到 `find_program(objdump)`、`PATH` 或 MSYS2。同理，MSVC 只执行已捕获且规范化存在的 `ZZ_DUMPBIN`。每个工具调用都检查 exit code；输出中每一条 `NEEDED`、`DLL Name` 或 install-name 行都必须被解析并命中 allowlist，否则失败。

Static archives are not passed to a dynamic-section tool; in static presets the final `ZzPlatformCompileTest` executable is mandatory and proves the closed dependency set. Missing tools, an unparsed dependency line, an empty scan set, or a dependency outside the allowlist is `FATAL_ERROR`, never a skip.

- [ ] **Step 7: 注册 relocation CTest 并运行 Task 6 最小绿灯**

At the end of `tests/Platform/CMakeLists.txt`, configure the context and register:

```cmake
set(platform_support_dir "${CMAKE_CURRENT_BINARY_DIR}/support")
file(MAKE_DIRECTORY "${platform_support_dir}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/ZzPlatformGateContext.cmake.in"
    "${platform_support_dir}/ZzPlatformGateContext.cmake"
    @ONLY)
add_test(NAME platform.package-relocation
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/relocation/$<CONFIG>"
        "-DZZ_CONTEXT_FILE=${platform_support_dir}/ZzPlatformGateContext.cmake"
        "-DZZ_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzPackageRelocationTest.cmake")
set_tests_properties(platform.package-relocation PROPERTIES
    LABELS "platform;install;headers"
    RUN_SERIAL TRUE
    TIMEOUT 900)
```

Run on Linux before committing:

```bash
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release \
  -R '^(platform\.(compile|package-relocation|binary-dependencies)|architecture\.complete-audit)$' \
  --output-on-failure
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release \
  -R '^(platform\.(compile|package-relocation|binary-dependencies)|architecture\.complete-audit)$' \
  --output-on-failure
```

Expected: shared and static each report 4/4 PASS；prefix A is absent after relocation；both external consumers use prefix B plus the preserved Qt prefix；every installed public header has its own translation unit；architecture and binary checks return 0.

- [ ] **Step 8: 提交平台 CTest 基础**

```bash
git add cmake/ZzArchitectureChecks.cmake \
  tests/Architecture/CMakeLists.txt \
  tests/Architecture/ZzArchitectureAudit.cmake \
  tests/CMakeLists.txt \
  tests/Platform/CMakeLists.txt \
  tests/Platform/ZzBinaryDependencyCheck.cmake \
  tests/Platform/ZzPackageRelocationTest.cmake \
  tests/Platform/ZzPlatformCompileTest.cpp \
  tests/Platform/ZzPlatformGateContext.cmake.in \
  tests/PublicHeaderConsumer/CMakeLists.txt
git commit -m "测试：建立平台安装与二进制门禁" \
  -m "以独立 producer、可删除 prefix A 和 prefix B 消费验证真实重定位。" \
  -m "逐头编译安装 API，并检查架构依赖和平台二进制允许集合。"
```

## Task 7: 建立三平台 runner 与 Ubuntu 22.04 运行门禁

> **2026-08-05 发布参考环境变更（优先于本 Task 后续旧文字）：** 当前唯一可用的 Ubuntu 26.04 / Qt 6.11.1 / GCC 15.2 主机已选为活动 Linux 发布参考机，`run-linux-gates.sh` 必须在该主机直接执行 `linux-gcc-release`、`linux-static-release`、`linux-gcc-release-lto`、`linux-static-release-lto`。原 `ubuntu2204-github-ci` 环境及脚本完整保留为 `pending-user-validation`；仅当 `ZZ_UBUNTU2204_BUILD_IMAGE` 已提供合法 immutable digest 时追加执行，不再作为当前参考机发布的前置条件。两套环境记录和性能报告不得混用，未来切换活动参考机必须先提交独立验证证据。

**Files:**
- Create: `tests/Platform/ZzGateScriptContract.cmake`
- Create: `scripts/ci/run-linux-gates.sh`
- Create: `scripts/ci/run-ubuntu2204-release-gates.sh`
- Create: `scripts/ci/run-windows-gates.ps1`
- Create: `scripts/ci/run-macos-gates.sh`
- Create: `scripts/ci/check-ubuntu2204-runtime.sh`
- Modify: `CMakeLists.txt`
- Modify: `cmake/ZzInstallPackage.cmake`
- Modify: `docs/third-party/THIRD_PARTY_NOTICES.md`
- Modify: `tests/Platform/CMakeLists.txt`

- [ ] **Step 1: 注册 gate-script 契约红灯**

Create `tests/Platform/ZzGateScriptContract.cmake` to require `ZZ_SOURCE_DIR`, read the five runner/check scripts and notices document listed below, and reject a missing or empty file. It must also require these literal tokens:

```cmake
set(required_tokens
    "scripts/ci/run-linux-gates.sh|linux-gcc-debug"
    "scripts/ci/run-linux-gates.sh|linux-clang-tidy-release"
    "scripts/ci/run-linux-gates.sh|linux-clang-tidy-static"
    "scripts/ci/run-linux-gates.sh|linux-clang-asan"
    "scripts/ci/run-linux-gates.sh|linux-gcc-benchmarks"
    "scripts/ci/run-linux-gates.sh|linux-clang-asan-benchmarks"
    "scripts/ci/run-linux-gates.sh|ZzComparePerformanceReport.cmake"
    "scripts/ci/run-linux-gates.sh|ZZ_UBUNTU2204_BUILD_IMAGE"
    "scripts/ci/run-ubuntu2204-release-gates.sh|VERSION_ID"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-gcc-release"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-static-release"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-gcc-release-lto"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-static-release-lto"
    "scripts/ci/run-ubuntu2204-release-gates.sh|ZZ_BUNDLE_GNU_RUNTIME=ON"
    "scripts/ci/run-windows-gates.ps1|windows-msvc2022-release"
    "scripts/ci/run-windows-gates.ps1|windows-msvc2022-static"
    "scripts/ci/run-windows-gates.ps1|windows-mingw-release"
    "scripts/ci/run-windows-gates.ps1|windows-mingw-static"
    "scripts/ci/run-macos-gates.sh|macos-clang-release-arm64"
    "scripts/ci/run-macos-gates.sh|macos-clang-release-x86_64"
    "scripts/ci/run-macos-gates.sh|macos-clang-static-arm64"
    "scripts/ci/run-macos-gates.sh|macos-clang-static-x86_64"
    "scripts/ci/check-ubuntu2204-runtime.sh|GLIBCXX_"
    "scripts/ci/check-ubuntu2204-runtime.sh|libstdc++.so.6 =>"
    "scripts/ci/check-ubuntu2204-runtime.sh|not found"
    "docs/third-party/THIRD_PARTY_NOTICES.md|GCC Runtime Library Exception")
```

For each `path|token`, split once, verify a regular nonempty file, and use `string(FIND)` to require the token. Register `platform.gate-script-contract` with label `platform;contract`, then run:

```bash
cmake --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R '^platform\.gate-script-contract$' \
  --output-on-failure
```

Expected: FAIL，首先报告 `scripts/ci/run-linux-gates.sh` 不存在。

- [ ] **Step 2: 实现 Linux shared/static、LTO、tidy 和 sanitizer runner**

Create `scripts/ci/run-linux-gates.sh` with this executable structure:

```bash
#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"

require_env() {
  [[ -n "${!1:-}" ]] || { echo "missing environment variable: $1" >&2; exit 64; }
}
for name in QT_ROOT GCC_13 GXX_13 GCC_13_TOOLCHAIN_ROOT CLANG_17 CLANGXX_17 \
            ZZ_RUNNER_IMAGE_DIGEST ZZ_GPU_IDENTITY \
            ZZ_UBUNTU2204_BUILD_IMAGE; do
  require_env "$name"
done
export ZZ_BENCHMARK_COMMIT
ZZ_BENCHMARK_COMMIT=$(git rev-parse --verify HEAD)

cmake -DZZ_PRESETS_FILE="$source_dir/CMakePresets.json" \
  -P tests/Platform/PresetMatrixContract.cmake

run_preset() {
  local preset=$1
  cmake --preset "$preset"
  cmake --build --preset "$preset"
  ctest --preset "$preset" --output-on-failure
}

run_preset linux-gcc-debug

for preset in linux-clang-tidy-release linux-clang-tidy-static; do
  cmake --preset "$preset"
  cmake --build --preset "$preset"
  cmake --build --preset "$preset" --target ZzClangTidy
  ctest --preset "$preset" --output-on-failure
done

run_preset linux-clang-asan
run_preset linux-gcc-benchmarks
for scenario in startup theme-switch animation large-model window-lifecycle idle; do
  cmake \
    -DZZ_BASELINE="docs/performance/reference/linux/${scenario}.json" \
    -DZZ_CURRENT="build/linux-gcc-benchmarks/reports/benchmark.${scenario}.json" \
    -DZZ_MAX_REGRESSION_PERCENT=10 \
    -P cmake/ZzComparePerformanceReport.cmake
done
cmake --preset linux-clang-asan-benchmarks
cmake --build --preset linux-clang-asan-benchmarks \
  --target ZzWindowLifecycleBenchmark
ctest --preset linux-clang-asan-benchmarks \
  -R '^benchmark\.window-lifecycle$' --output-on-failure

if [[ ! "$ZZ_UBUNTU2204_BUILD_IMAGE" =~ ^[^[:space:]@]+@sha256:[0-9a-f]{64}$ ]]; then
  echo "ZZ_UBUNTU2204_BUILD_IMAGE must be an immutable image digest" >&2
  exit 64
fi
command -v docker >/dev/null || { echo "docker is required" >&2; exit 69; }
docker pull "$ZZ_UBUNTU2204_BUILD_IMAGE"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e "ZZ_RUNNER_IMAGE_DIGEST=${ZZ_UBUNTU2204_BUILD_IMAGE##*@}" \
  -v "$source_dir:/workspace" \
  -w /workspace \
  "$ZZ_UBUNTU2204_BUILD_IMAGE" \
  bash scripts/ci/run-ubuntu2204-release-gates.sh
```

Every full `ctest` includes `platform.compile`、`platform.package-relocation`、`platform.binary-dependencies`、installed public headers and architecture tests. Host runner 负责 debug、tidy、sanitizer 和真实 display benchmark；四个 release/static/LTO 组合只由末尾的 Ubuntu 22.04 digest image 内层脚本构建。`linux-gcc-benchmarks` 生成六个当前报告并逐一执行相对比较；缺少已审核 runner digest、immutable build image 或任一基线文件立即失败。Do not filter platform tests out of tidy, sanitizer, shared, static or container release runs.

- [ ] **Step 3: 在不可变 Ubuntu 22.04 镜像内构建并验证实际 GNU runtime**

CI provisioning 必须提供 `ZZ_UBUNTU2204_BUILD_IMAGE`，其值是带 registry/repository 的不可变 `@sha256:<64 lowercase hex>` 引用。该镜像以 Ubuntu 22.04 为 runtime root，预装 GCC/G++ 13.1+、CMake 3.23+、Ninja、binutils、`file`、Qt 6.8+ x86_64 SDK 于 `/opt/qt`，以及 GCC 对应 `COPYING3`/`COPYING.RUNTIME` 于 `/opt/gcc-runtime-licenses`。Qt SDK 和 GCC runtime 必须在 Ubuntu 22.04 基线上构建；禁止在门禁时从 mutable `ubuntu:22.04` tag、PPA 或滚动仓库临时拼装镜像。

Add `ZZ_BUNDLE_GNU_RUNTIME` (default OFF) and `ZZ_GNU_RUNTIME_LICENSE_DIR` cache path to root `CMakeLists.txt`. When bundling is ON, configure requires Linux、GNU compiler、shared build、Release、an absolute existing license directory containing nonempty `COPYING3` and `COPYING.RUNTIME`. Query the selected compiler with `-print-file-name=libstdc++.so.6` and `-print-file-name=libgcc_s.so.1`, resolve both real files, reject unresolved names/non-files, and pass the two exact paths to `cmake/ZzInstallPackage.cmake`. Install their resolved bytes renamed exactly `libstdc++.so.6` and `libgcc_s.so.1` below `${CMAKE_INSTALL_LIBDIR}`，并安装两份许可证到 `share/ZzPureToolsPro/licenses/gcc-runtime/`。Linux shared Zz libraries use install RPATH `$ORIGIN` so their own `DT_NEEDED` lookup selects the adjacent deployed runtime. `THIRD_PARTY_NOTICES.md` records GCC version/source、GPL-3.0-or-later with GCC Runtime Library Exception、the two installed runtime filenames and both license paths; Task 8 的 installed-license verifier 同步要求这些文件和 notice tokens。

Create `scripts/ci/run-ubuntu2204-release-gates.sh` with:

```bash
#!/usr/bin/env bash
set -euo pipefail

source /etc/os-release
[[ "$ID" == ubuntu && "$VERSION_ID" == 22.04 ]] || {
  echo "release image must be Ubuntu 22.04" >&2
  exit 1
}
export QT_ROOT=/opt/qt
export GCC_13=/usr/bin/gcc-13
export GXX_13=/usr/bin/g++-13
export GCC_13_TOOLCHAIN_ROOT=/usr
for path in "$GCC_13" "$GXX_13" "$QT_ROOT/bin/qtpaths" \
            /opt/gcc-runtime-licenses/COPYING3 \
            /opt/gcc-runtime-licenses/COPYING.RUNTIME; do
  [[ -f "$path" && -s "$path" ]] || { echo "missing image input: $path" >&2; exit 1; }
done
gcc_version=$("$GXX_13" -dumpfullversion -dumpversion)
[[ "$(printf '%s\n%s\n' 13.1 "$gcc_version" | sort -V | head -n 1)" == 13.1 ]] || {
  echo "G++ 13.1+ is required, got $gcc_version" >&2
  exit 1
}
qt_version=$("$QT_ROOT/bin/qtpaths" --qt-version)
[[ "$(printf '%s\n%s\n' 6.8 "$qt_version" | sort -V | head -n 1)" == 6.8 ]] || {
  echo "Qt 6.8+ is required, got $qt_version" >&2
  exit 1
}

run_preset() {
  local preset=$1
  shift
  local build_dir="$PWD/build/$preset"
  [[ "$build_dir" == "$PWD/build/"* ]] || { echo "unsafe build dir" >&2; exit 1; }
  cmake -E remove_directory "$build_dir"
  cmake --preset "$preset" "$@"
  cmake --build --preset "$preset"
  ctest --preset "$preset" --output-on-failure
}
run_preset linux-gcc-release
run_preset linux-static-release
run_preset linux-static-release-lto
run_preset linux-gcc-release-lto \
  -DZZ_BUNDLE_GNU_RUNTIME=ON \
  -DZZ_GNU_RUNTIME_LICENSE_DIR=/opt/gcc-runtime-licenses

install_root=$PWD/install/ubuntu2204-gcc13-release-lto
[[ "$install_root" == "$PWD/install/"* ]] || { echo "unsafe install root" >&2; exit 1; }
cmake -E remove_directory "$install_root"
cmake --install build/linux-gcc-release-lto --prefix "$install_root"
bash scripts/ci/check-ubuntu2204-runtime.sh "$install_root" "$QT_ROOT"
```

Create `scripts/ci/check-ubuntu2204-runtime.sh`. It requires exactly `<package-root> <qt-runtime-root>`，requires `file/find/ldd/readelf/realpath/sed/sort/strings`, and first rechecks `/etc/os-release` is Ubuntu 22.04. It must then implement this exact contract:

1. Find exactly one installed `libstdc++.so.6` and `libgcc_s.so.1` below the package, resolve each with `realpath`, and include their directory plus the Qt `lib` directory in `LD_LIBRARY_PATH` in that order. Never read the system `libstdc++` as the available C++ runtime.
2. Read available `GLIBC_*` from the container `/lib/x86_64-linux-gnu/libc.so.6`, but read available `GLIBCXX_*` from the deployed `libstdc++.so.6`. Missing symbol sets fail.
3. Build a deduplicated scan set from every package ELF plus the resolved Qt Core/Gui/Widgets/Svg/Concurrent shared libraries actually used by the targets. A missing Qt library, non-ELF candidate, or empty set fails.
4. For each ELF, parse its maximum required `GLIBC_*` and `GLIBCXX_*` with `readelf --version-info` and require them not newer than the two available sets using `sort -V`. This also scans the deployed GNU runtime itself against Ubuntu 22.04 GLIBC.
5. Run `ldd` with the controlled `LD_LIBRARY_PATH`; reject nonzero/empty output and literal `not found`. For every `libstdc++.so.6 =>` and `libgcc_s.so.1 =>` line, parse the selected absolute file and require `realpath` equality with the two deployed package files. Require at least one observed selection of each runtime, so a parser that silently finds no line cannot pass.

An unresolved dependency, newer GLIBC requirement, newer-than-deployed GLIBCXX requirement, system-selected GNU runtime, missing Qt runtime, mutable/non-digest outer image reference, or tool failure exits nonzero and names the binary. The release/LTO artifacts are therefore built and inspected in the same immutable Ubuntu 22.04 image, and the check targets the runtime bytes actually shipped rather than whichever `libstdc++` happens to be installed on a later host.

- [ ] **Step 4: 实现 MSVC 与 Qt 官方 MinGW 原生 runner**

Create `scripts/ci/run-windows-gates.ps1` with:

```powershell
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Native([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$File failed with exit code $LASTEXITCODE"
    }
}
function Resolve-Required([string]$Path) {
    return (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
}
function Normalize-WindowsPath([string]$Path) {
    $resolved = Resolve-Required $Path
    return [IO.Path]::GetFullPath($resolved).TrimEnd([char[]]@('\', '/'))
}
function Assert-ConfiguredPreset(
    [string]$Preset,
    [string]$ExpectedId,
    [string]$ExpectedCompiler,
    [string]$ExpectedQt) {
    $buildDir = Resolve-Required (Join-Path $sourceDir "build/$Preset")
    $compilerFiles = @(Get-ChildItem -LiteralPath (Join-Path $buildDir 'CMakeFiles') `
        -Recurse -Filter CMakeCXXCompiler.cmake -File)
    if ($compilerFiles.Count -ne 1) {
        throw "Expected one CMakeCXXCompiler.cmake for $Preset"
    }
    $compilerState = Get-Content -LiteralPath $compilerFiles[0].FullName -Raw
    $compilerIdPattern = 'set\(CMAKE_CXX_COMPILER_ID "{0}"\)' -f
        [regex]::Escape($ExpectedId)
    if ($compilerState -notmatch $compilerIdPattern) {
        throw "Unexpected compiler id for $Preset"
    }
    $cache = Get-Content -LiteralPath (Join-Path $buildDir 'CMakeCache.txt') -Raw
    $compilerPath = (Normalize-WindowsPath $ExpectedCompiler).Replace('\', '/')
    $qtPath = (Normalize-WindowsPath $ExpectedQt).Replace('\', '/')
    $normalizedCompilerState = $compilerState.Replace('\', '/')
    $normalizedCache = $cache.Replace('\', '/')
    $qtMatches =
        $normalizedCache.Contains(
            "ZZ_QT_PREFIX:UNINITIALIZED=$qtPath",
            [StringComparison]::OrdinalIgnoreCase) -or
        $normalizedCache.Contains(
            "ZZ_QT_PREFIX:PATH=$qtPath",
            [StringComparison]::OrdinalIgnoreCase) -or
        $normalizedCache.Contains(
            "ZZ_QT_PREFIX:STRING=$qtPath",
            [StringComparison]::OrdinalIgnoreCase)
    if (-not $normalizedCompilerState.Contains(
            $compilerPath, [StringComparison]::OrdinalIgnoreCase) -or
        -not $qtMatches) {
        throw "Compiler or Qt prefix mismatch for $Preset"
    }
}

$sourceDir = Resolve-Required (Join-Path $PSScriptRoot '../..')
Set-Location $sourceDir
$msvcQt = Normalize-WindowsPath $env:QT_MSVC_ROOT
$msvcQmake = Resolve-Required (Join-Path $msvcQt 'bin/qmake.exe')
$msvcPrefixRaw = (& $msvcQmake -query QT_INSTALL_PREFIX).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to query QT_INSTALL_PREFIX from QT_MSVC_ROOT'
}
$msvcPrefix = Normalize-WindowsPath $msvcPrefixRaw
$msvcXspec = (& $msvcQmake -query QMAKE_XSPEC).Trim()
if ($LASTEXITCODE -ne 0 -or
    -not $msvcPrefix.Equals(
        $msvcQt, [StringComparison]::OrdinalIgnoreCase) -or
    $msvcXspec -notmatch '^win32-msvc') {
    throw 'QT_MSVC_ROOT is not an MSVC Qt kit'
}

$dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
$env:ZZ_DUMPBIN = Resolve-Required $dumpbin
$mingwKit = pwsh -NoProfile -File scripts/ci/Assert-QtMinGWKit.ps1 |
    ConvertFrom-Json
$env:ZZ_MINGW_OBJDUMP = Resolve-Required $mingwKit.ObjDump
$mingwQt = Normalize-WindowsPath $mingwKit.QtPrefix
if ($mingwQt.Equals($msvcQt, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'MSVC and MinGW must use separate Qt prefixes'
}

$presets = @(
    'windows-msvc2022-release',
    'windows-msvc2022-static',
    'windows-mingw-release',
    'windows-mingw-static'
)
foreach ($preset in $presets) {
    Invoke-Native cmake @('--preset', $preset)
    if ($preset -like 'windows-msvc*') {
        Assert-ConfiguredPreset $preset 'MSVC' `
            (Get-Command cl.exe -ErrorAction Stop).Source $msvcQt
    } else {
        Assert-ConfiguredPreset $preset 'GNU' $mingwKit.Compiler $mingwQt
    }
    Invoke-Native cmake @('--build', '--preset', $preset)
    Invoke-Native ctest @('--preset', $preset, '--output-on-failure')
}
```

The assertion executes after every configure and before build. MSVC files must contain `set(CMAKE_CXX_COMPILER_ID "MSVC")`; MinGW files must contain `set(CMAKE_CXX_COMPILER_ID "GNU")`. Zero/multiple compiler files or a compiler/Qt path mismatch throws.

- [ ] **Step 5: 实现 macOS 双架构 shared/static runner 与 lipo 检查**

Create `scripts/ci/run-macos-gates.sh` with:

```bash
#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"
for name in QT_MACOS_ARM64_ROOT QT_MACOS_X86_64_ROOT APPLE_CLANG APPLE_CLANGXX; do
  [[ -n "${!name:-}" ]] || { echo "missing environment variable: $name" >&2; exit 64; }
done

presets=(
  macos-clang-release-arm64
  macos-clang-release-x86_64
  macos-clang-static-arm64
  macos-clang-static-x86_64
)
for preset in "${presets[@]}"; do
  expected=arm64
  [[ "$preset" == *x86_64 ]] && expected=x86_64
  qt_var=QT_MACOS_ARM64_ROOT
  [[ "$expected" == x86_64 ]] && qt_var=QT_MACOS_X86_64_ROOT
  qt_core="${!qt_var}/lib/QtCore.framework/QtCore"
  [[ -f "$qt_core" ]] || { echo "missing QtCore: $qt_core" >&2; exit 1; }
  [[ "$(lipo -archs "$qt_core")" == *"$expected"* ]] || {
    echo "$qt_var does not contain $expected" >&2; exit 1;
  }

  cmake --preset "$preset"
  cmake --build --preset "$preset"
  cmake --build --preset "$preset" --target ZzClangTidy
  ctest --preset "$preset" --output-on-failure

  probe_count=0
  probe_path=
  while IFS= read -r candidate; do
    probe_path=$candidate
    probe_count=$((probe_count + 1))
  done < <(find "build/$preset" -type f \
    -name ZzPlatformCompileTest -perm -111)
  [[ $probe_count -eq 1 ]] || {
    echo "expected one platform probe for $preset" >&2; exit 1;
  }
  [[ "$(lipo -archs "$probe_path")" == "$expected" ]] || {
    echo "wrong probe architecture for $preset" >&2; exit 1;
  }
done
```

- [ ] **Step 6: 运行契约和三个原生 host 绿灯**

First make shell scripts executable, then run on the named hosts:

```bash
chmod +x scripts/ci/run-linux-gates.sh \
  scripts/ci/run-ubuntu2204-release-gates.sh \
  scripts/ci/run-macos-gates.sh \
  scripts/ci/check-ubuntu2204-runtime.sh
cmake -DZZ_SOURCE_DIR="$PWD" -P tests/Platform/ZzGateScriptContract.cmake
```

```text
Linux GCC 13/Clang 17 host: bash scripts/ci/run-linux-gates.sh
Windows VS 2022 x64 host:   pwsh -NoProfile -File scripts/ci/run-windows-gates.ps1
macOS AppleClang 15+ host:  bash scripts/ci/run-macos-gates.sh
```

Expected: contract PASS；Linux host debug/tidy/sanitizer/six-scenario relative performance PASS，Ubuntu 22.04 immutable image 内的 GCC shared/static/LTO、relocation、binary、bundled GNU runtime selection checks PASS；Windows four toolchain/linkage combinations PASS with separate Qt kits；macOS four architecture/linkage combinations and exact `lipo` checks PASS。缺少任一原生 host、对应 Qt SDK、真实 display、审核过的 image digest/runner digest 或性能基线时 Task 7 保持未完成。

- [ ] **Step 7: 在最小绿灯后提交 runner**

Run once more on the current native host immediately before commit; its script must return 0. Then:

```bash
git add CMakeLists.txt \
  cmake/ZzInstallPackage.cmake \
  docs/third-party/THIRD_PARTY_NOTICES.md \
  scripts/ci/check-ubuntu2204-runtime.sh \
  scripts/ci/run-linux-gates.sh \
  scripts/ci/run-ubuntu2204-release-gates.sh \
  scripts/ci/run-macos-gates.sh \
  scripts/ci/run-windows-gates.ps1 \
  tests/Platform/CMakeLists.txt \
  tests/Platform/ZzGateScriptContract.cmake
git commit -m "测试：建立三平台原生 runner" \
  -m "执行 GCC、Clang、MSVC、Qt MinGW 与 AppleClang 的 shared/static 矩阵。" \
  -m "在不可变 Ubuntu 22.04 镜像内构建发布产物，并核验随包 GNU runtime。"
```

## Task 8: 建立第三方、项目许可证与发布证据门禁

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `cmake/ZzInstallPackage.cmake`
- Create: `cmake/ZzReleaseChecks.cmake`
- Create: `cmake/ZzExpectConfigureFailure.cmake`
- Create: `cmake/ZzVerifyInstalledLicenses.cmake`
- Create: `tests/Release/CMakeLists.txt`
- Create: `tests/Release/ZzReleaseChecksFixture.cmake`
- Modify: `tests/CMakeLists.txt`
- Modify: `docs/third-party/THIRD_PARTY_NOTICES.md`
- Modify: `docs/third-party/qwindowkit-vendor.json`
- Create: `docs/third-party/release-evidence.json`
- Create: `docs/third-party/RELEASE_BLOCKERS_ZH.md`
- Verify after owner approval: `LICENSE`
- Verify from the evidence bundle: `ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp`

- [ ] **Step 1: 先写“内层失败、外层通过”的 release 红灯测试**

Create `cmake/ZzExpectConfigureFailure.cmake` with required inputs `ZZ_SOURCE_DIR`、`ZZ_WORK_DIR`、`ZZ_QT_PREFIX`、`ZZ_C_COMPILER`、`ZZ_CXX_COMPILER` and `ZZ_EXPECTED_BLOCKERS`. It removes only the validated `ZZ_WORK_DIR`, executes:

```cmake
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${ZZ_SOURCE_DIR}"
        -B "${ZZ_WORK_DIR}"
        -G Ninja
        "-DCMAKE_C_COMPILER=${ZZ_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${ZZ_CXX_COMPILER}"
        "-DCMAKE_PREFIX_PATH=${ZZ_QT_PREFIX}"
        "-DZZ_QT_PREFIX=${ZZ_QT_PREFIX}"
        -DZZ_RELEASE_BUILD=ON
        "-DZZ_RELEASE_FORCED_BLOCKERS=${ZZ_EXPECTED_BLOCKERS}"
        -DZZ_BUILD_TESTS=OFF
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
set(output "${stdout}\n${stderr}")
if("${result}" EQUAL 0)
    message(FATAL_ERROR "Release configure unexpectedly succeeded")
endif()
foreach(blocker IN LISTS ZZ_EXPECTED_BLOCKERS)
    string(FIND "${output}" "${blocker}" position)
    if("${position}" EQUAL -1)
        message(FATAL_ERROR
            "Release failed without required blocker ${blocker}:\n${output}")
    endif()
endforeach()
message(STATUS "Release configure is blocked for every declared reason")
```

Create `tests/Release/CMakeLists.txt` and append `add_subdirectory(Release)` to `tests/CMakeLists.txt`:

```cmake
add_test(NAME release.blocked-without-evidence
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/blocked"
        "-DZZ_QT_PREFIX=${ZZ_QT_PREFIX}"
        "-DZZ_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DZZ_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DZZ_EXPECTED_BLOCKERS=qwindowkit.upstream-provenance;qmsetup.windeployqt-5.15.2-derived-work;project.license"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzExpectConfigureFailure.cmake")
set_tests_properties(release.blocked-without-evidence PROPERTIES
    LABELS "release;contract"
    RUN_SERIAL TRUE
    TIMEOUT 120)

add_test(NAME release.complete-fixture
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_BINARY_ROOT=${CMAKE_BINARY_DIR}"
        "-DZZ_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/complete-fixture"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzReleaseChecksFixture.cmake")
set_tests_properties(release.complete-fixture PROPERTIES
    LABELS "release;contract"
    RUN_SERIAL TRUE
    TIMEOUT 30)
```

Run:

```bash
cmake --preset linux-gcc-debug
ctest --preset linux-gcc-debug \
  -R '^release\.(blocked-without-evidence|complete-fixture)$' \
  --output-on-failure
```

Expected: `release.blocked-without-evidence` FAIL because the inner release configure still succeeds；`release.complete-fixture` 此时也因 checker/fixture script 尚未实现而失败。不要给外层测试设置 `WILL_FAIL`。

- [ ] **Step 2: 增加 fail-closed option 和三项初始 blocker**

Add `option(ZZ_RELEASE_BUILD "Enable verified release packaging" OFF)` in root `CMakeLists.txt`. When it is ON, include `ZzReleaseChecks` and call `zz_verify_release_evidence(SOURCE_ROOT "${PROJECT_SOURCE_DIR}" EVIDENCE_ROOT "${ZZ_RELEASE_EVIDENCE_ROOT}" FORCED_BLOCKERS ${ZZ_RELEASE_FORCED_BLOCKERS})` before adding component subdirectories. `ZZ_RELEASE_FORCED_BLOCKERS` 不定义或为空时不改变生产行为；它只追加失败项，没有任何值能跳过、覆盖或删除真实证据检查。

Create `docs/third-party/release-evidence.json` initially with exactly:

```json
{
  "schemaVersion": 1,
  "review": {
    "reviewer": null,
    "reviewedAt": null
  },
  "releaseBlockers": [
    {
      "id": "qwindowkit.upstream-provenance",
      "reason": "Exact upstream commit and source archive are not verified"
    },
    {
      "id": "qmsetup.windeployqt-5.15.2-derived-work",
      "reason": "Derived-work source and redistribution review are not verified"
    },
    {
      "id": "project.license",
      "reason": "The project license and owner approval are not present"
    }
  ],
  "evidence": {
    "qwindowkit": {
      "upstreamCommit": null,
      "sourceArchive": null,
      "provenanceReview": null
    },
    "windeployqtDerivedWork": {
      "upstreamProject": "Qt",
      "upstreamVersion": "5.15.2",
      "upstreamFile": "qttools/src/shared/winutils/utils.cpp",
      "localFile": "ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp",
      "upstreamSource": null,
      "upstreamLicense": null,
      "localSourceSha256": null,
      "redistributionConclusion": null,
      "reviewRecord": null
    },
    "projectLicense": {
      "spdxExpression": null,
      "licenseFile": null,
      "approvalRecord": null
    }
  }
}
```

Update `docs/third-party/qwindowkit-vendor.json` without inventing values: retain the known declared version, upstream URL, current `null` commit/hash, local patches/licenses, and exact blocker ids `qwindowkit.upstream-provenance`、`qmsetup.windeployqt-5.15.2-derived-work`。`validatedMatrix` 初始为空，只能加入带 native runner 日志路径、日期和工具链/Qt 身份的实际记录。`RELEASE_BLOCKERS_ZH.md` must identify the three blocker ids, the exact missing evidence, the responsible human reviewer role, and the rule that deleting an array item without verified files does not close it.

- [ ] **Step 3: 实现内容和 hash 校验，禁止只靠字段形状放行**

Create `cmake/ZzReleaseChecks.cmake`. Its public function uses `cmake_parse_arguments()` and requires explicit `SOURCE_ROOT`; `EVIDENCE_ROOT` points to untracked source archives/review records, `FORCED_BLOCKERS` is an optional multi-value test input. Manifest paths remain fixed below `${SOURCE_ROOT}/docs/third-party`，生产根调用必须传 `PROJECT_SOURCE_DIR`。Implement these checks and collect every failure before one `FATAL_ERROR`:

1. `release-evidence.json` schema is exactly 1 and `releaseBlockers` is an array of length 0. When nonempty, report every `id` verbatim.
2. Top-level `review.reviewer` is nonempty and `reviewedAt` matches UTC `YYYY-MM-DDTHH:MM:SSZ`.
3. `qwindowkit-vendor.json` has a 40-hex `upstreamCommit`, 64-hex `archiveSha256`, nonempty `validatedMatrix`, and an empty `releaseBlockers` array.
4. Each file object has exact shape `{ "scope": "repository|external", "path": "relative/path", "sha256": "64-hex" }`. Reject absolute paths and `..`; resolve repository scope below the source root and external scope below `ZZ_RELEASE_EVIDENCE_ROOT`; require a regular nonempty file and compare `file(SHA256 ...)` with the declared lowercase digest.
5. Verify file objects for `qwindowkit.sourceArchive`、`qwindowkit.provenanceReview`、`windeployqtDerivedWork.upstreamSource`、`windeployqtDerivedWork.upstreamLicense`、`windeployqtDerivedWork.reviewRecord`、`projectLicense.licenseFile` and `projectLicense.approvalRecord`.
6. Require qwindowkit source-archive SHA to equal the vendor `archiveSha256`; require `upstreamCommit` to equal the vendor commit.
7. Require derived-work version exactly `5.15.2`, local file exactly `ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp`, `localSourceSha256` equal that repository file's actual SHA, and `redistributionConclusion` exactly `approved` with reviewer/date recorded in the hashed review record.
8. Require project-license file exactly `LICENSE`, its actual SHA to match the file object, a nonempty SPDX expression, and owner approval/review date in the hashed approval record.
9. Append every nonempty `FORCED_BLOCKERS` item verbatim after all real checks. Forced items can only make the result fail and must never suppress, replace, mark resolved, or short-circuit a real finding.

Use `string(LENGTH)` plus `MATCHES "^[0-9a-f]+$"` for 40/64-character values rather than relying on regex interval syntax. `null`、empty strings、`UNKNOWN`、`UNVERIFIED`, malformed JSON, a missing evidence root, missing files, a digest mismatch, nonempty blockers, or missing reviewer/date all independently block release. This means a structurally valid manifest with invented text cannot pass without the exact evidence bytes and review records.

Create `tests/Release/ZzReleaseChecksFixture.cmake`. It requires `ZZ_SOURCE_DIR`、`ZZ_BINARY_ROOT`、`ZZ_WORK_DIR`，并验证 normalized work root 既不是 filesystem/source/binary root，又位于 `ZZ_BINARY_ROOT` 下，然后只删除该目录。Under `source/` it writes nonempty `LICENSE` and `ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp`; under `evidence/` it writes qwindowkit archive/provenance、Qt 5.15.2 upstream source/license、redistribution review and owner approval records. Review JSON uses reviewer `fixture-reviewer` and UTC `2026-08-02T00:00:00Z`，redistribution conclusion is `approved`。The script computes every digest with `file(SHA256)` after writing bytes, then generates these two manifests with the computed lowercase values rather than embedding fake hashes:

```text
source/docs/third-party/qwindowkit-vendor.json
  upstreamCommit = 0123456789abcdef0123456789abcdef01234567
  archiveSha256 = computed qwindowkit archive SHA
  validatedMatrix = one fixture-only Linux/GCC/Qt record
  releaseBlockers = []

source/docs/third-party/release-evidence.json
  schemaVersion = 1
  review = fixture-reviewer / 2026-08-02T00:00:00Z
  releaseBlockers = []
  qwindowkit upstreamCommit = the same 40-hex value
  all seven required file objects = exact repository/external relative paths
  windeployqt upstreamVersion = 5.15.2
  windeployqt localFile = ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp
  localSourceSha256 = computed local source SHA
  redistributionConclusion = approved
  project SPDX expression = MIT
  project license path = LICENSE
```

Use bracket-quoted templates plus `string(CONFIGURE ... @ONLY)` so JSON backslashes/semicolons are not reinterpreted. Finally include `${ZZ_SOURCE_DIR}/cmake/ZzReleaseChecks.cmake` and call `zz_verify_release_evidence(SOURCE_ROOT "${ZZ_WORK_DIR}/source" EVIDENCE_ROOT "${ZZ_WORK_DIR}/evidence")` without forced blockers. The test must pass only through the same hash/content validator used by production; it may not add a “skip validation” or “fixture accepted” branch.

- [ ] **Step 4: 安装并复验项目与第三方许可证**

Update `cmake/ZzInstallPackage.cmake` so a release install includes:

```text
share/ZzPureToolsPro/licenses/PROJECT-LICENSE
share/ZzPureToolsPro/licenses/qwindowkit/LICENSE
share/ZzPureToolsPro/licenses/qwindowkit/qmsetup-LICENSE
share/ZzPureToolsPro/licenses/qwindowkit/syscmdline-LICENSE
share/ZzPureToolsPro/licenses/ZzLog/LICENSE
share/ZzPureToolsPro/licenses/ZzLog/spdlog-LICENSE.txt
share/ZzPureToolsPro/licenses/ZzLog/fmt-LICENSE.txt
share/ZzPureToolsPro/licenses/gcc-runtime/COPYING3
share/ZzPureToolsPro/licenses/gcc-runtime/COPYING.RUNTIME
share/ZzPureToolsPro/THIRD_PARTY_NOTICES.md
share/ZzPureToolsPro/qwindowkit-vendor.json
share/ZzPureToolsPro/release-evidence.json
```

Create `cmake/ZzVerifyInstalledLicenses.cmake` to accept `ZZ_BUILD_DIR`、`ZZ_INSTALL_ROOT` and optional `ZZ_CONFIG`. It must reject an unsafe install root, remove only that test root, run `cmake --install ZZ_BUILD_DIR --prefix ZZ_INSTALL_ROOT [--config ZZ_CONFIG]`, require each exact relative file above to be regular/nonempty, and require the notices document to contain `QWindowKit`、`FramelessHelper`、`qmsetup`、`syscmdline`、`spdlog`、`fmt`、`GCC Runtime Library Exception`, version/source, SPDX conclusion, and installed license path. The two GCC license files are required when the producer cache has `ZZ_BUNDLE_GNU_RUNTIME:BOOL=ON`; otherwise they must be absent so a stale runtime cannot enter a non-bundled package. Register only for `ZZ_RELEASE_BUILD=ON`:

```cmake
add_test(NAME release.installed-licenses
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DZZ_INSTALL_ROOT=${CMAKE_BINARY_DIR}/release-install/$<CONFIG>"
        "-DZZ_CONFIG=$<CONFIG>"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyInstalledLicenses.cmake")
set_tests_properties(release.installed-licenses PROPERTIES
    LABELS "release;install"
    RUN_SERIAL TRUE
    TIMEOUT 300)
```

A missing root `LICENSE` must produce blocker `project.license` during configure rather than an install-time surprise.

- [ ] **Step 5: 验证开发绿灯与预期阻断绿灯**

Run with the qualified GCC 13 tools:

```bash
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug \
  -R '^release\.(blocked-without-evidence|complete-fixture)$' \
  --output-on-failure
cmake -S . -B build/release-audit-blocked -G Ninja \
  -DCMAKE_C_COMPILER="$GCC_13" \
  -DCMAKE_CXX_COMPILER="$GXX_13" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DZZ_QT_PREFIX="$QT_ROOT" \
  -DZZ_RELEASE_BUILD=ON \
  -DZZ_BUILD_TESTS=OFF
```

Expected: normal development build PASS；2/2 release contract CTest PASS。负例由 `ZZ_RELEASE_FORCED_BLOCKERS` 稳定触发并打印三个 ID，即使生产 manifest 日后已清零仍必须通过；完整 fixture 则用动态计算的真实 hash 走成功路径。最后 direct release configure 仍因当前生产证据缺失而 FAIL 并打印 exactly the three current blocker ids. This is the required Task 8 green state while external evidence is unavailable.

- [ ] **Step 6: 只用实际证据关闭 blocker 并验证 release 成功路径**

After the owner supplies `LICENSE`, the source archives/licenses, provenance records, legal redistribution review, and approval record, calculate SHA-256 from those files, fill the evidence objects, add reviewer/date, and remove a blocker only when its corresponding checks pass. Then enter the same approved Ubuntu 22.04 digest image used by Task 7, mount the reviewed evidence at `ZZ_RELEASE_EVIDENCE_ROOT`, set `GCC_13=/usr/bin/gcc-13`、`GXX_13=/usr/bin/g++-13`、`QT_ROOT=/opt/qt`, and run:

```bash
cmake -S . -B build/release-audit -G Ninja \
  -DCMAKE_C_COMPILER="$GCC_13" \
  -DCMAKE_CXX_COMPILER="$GXX_13" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DZZ_QT_PREFIX="$QT_ROOT" \
  -DZZ_RELEASE_EVIDENCE_ROOT="$ZZ_RELEASE_EVIDENCE_ROOT" \
  -DZZ_BUNDLE_GNU_RUNTIME=ON \
  -DZZ_GNU_RUNTIME_LICENSE_DIR=/opt/gcc-runtime-licenses \
  -DZZ_RELEASE_BUILD=ON \
  -DZZ_BUILD_TESTS=ON
cmake --build build/release-audit
ctest --test-dir build/release-audit --output-on-failure
cmake --install build/release-audit --prefix install/release-audit
cmake -DZZ_BUILD_DIR="$PWD/build/release-audit" \
  -DZZ_INSTALL_ROOT="$PWD/install/release-audit-verified" \
  -P cmake/ZzVerifyInstalledLicenses.cmake
```

Expected only after evidence closure: configure/build/CTest/install/license audit PASS，`releaseBlockers` arrays are empty, every evidence digest matches, and reviewer/date fields are present. Until then this step remains incomplete.

- [ ] **Step 7: 在当前可证明的绿灯后提交 release gate**

Run Step 5 immediately before committing. Include `LICENSE` or evidence records only if they were actually supplied and verified:

```bash
git add CMakeLists.txt \
  cmake/ZzExpectConfigureFailure.cmake \
  cmake/ZzInstallPackage.cmake \
  cmake/ZzReleaseChecks.cmake \
  cmake/ZzVerifyInstalledLicenses.cmake \
  docs/third-party/RELEASE_BLOCKERS_ZH.md \
  docs/third-party/THIRD_PARTY_NOTICES.md \
  docs/third-party/qwindowkit-vendor.json \
  docs/third-party/release-evidence.json \
  tests/CMakeLists.txt \
  tests/Release/CMakeLists.txt \
  tests/Release/ZzReleaseChecksFixture.cmake
git commit -m "发布：建立可验证的合规阻断门禁" \
  -m "核验第三方来源、衍生代码依据、项目许可证和证据文件摘要。" \
  -m "证据未齐时保持开发可用，并让预期阻断测试稳定通过。"
```

## Task 9: 写明构建、平台状态与人工验收证据

**Files:**
- Create: `docs/development/CODING_STANDARD_ZH.md`
- Create: `docs/development/BUILDING_ZH.md`
- Create: `docs/development/PLATFORM_SUPPORT_ZH.md`
- Create: `docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md`
- Create: `docs/release/MANUAL_MACOS_CHECKLIST_ZH.md`
- Create: `docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`
- Create: `tests/Architecture/ZzDocumentationAudit.cmake`
- Modify: `tests/Architecture/CMakeLists.txt`

- [ ] **Step 1: 先注册缺文档红灯**

Append to `tests/Architecture/CMakeLists.txt`:

```cmake
add_test(NAME architecture.documentation-audit
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzDocumentationAudit.cmake")
set_tests_properties(architecture.documentation-audit PROPERTIES
    LABELS "architecture;documentation")
```

Create `tests/Architecture/ZzDocumentationAudit.cmake` first with the required-file list from this task, then run:

```bash
cmake --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R '^architecture\.documentation-audit$' \
  --output-on-failure
```

Expected: FAIL and name the first absent document, `docs/development/CODING_STANDARD_ZH.md`.

- [ ] **Step 2: 写编码规范和可复现构建手册**

`docs/development/CODING_STANDARD_ZH.md` must have normative sections for C++20, `Zz` type prefix, primary type/file identity, traditional nested namespace syntax, simplified-Chinese Doxygen, PIMPL eligibility, single QObject ownership, Result/exception boundary, UI/business separation, QWK/Qt Private boundary, cache/animation rules, and multi-paragraph Chinese Git messages.

`docs/development/BUILDING_ZH.md` must list exact prerequisites and environment variables from Task 2, then exact configure/build/test/install commands for:

```text
linux-gcc-debug, linux-gcc-release, linux-static-release
linux-gcc-release-lto, linux-static-release-lto
linux-clang-tidy-release, linux-clang-tidy-static, linux-clang-asan
windows-msvc2022-release, windows-msvc2022-static
windows-mingw-release, windows-mingw-static
macos-clang-release-arm64, macos-clang-release-x86_64
macos-clang-static-arm64, macos-clang-static-x86_64
```

It must call the three Task 7 scripts, show the external InstallConsumer/PublicHeaderConsumer path through `platform.package-relocation`, identify `CMakeUserPresets.json` as untracked local configuration, and state that MSVC/MinGW objects and Qt kits cannot be mixed.

- [ ] **Step 3: 写只有三种合法状态的平台矩阵**

`docs/development/PLATFORM_SUPPORT_ZH.md` must use the exact status vocabulary:

```text
未执行
静态验证通过
真机验收通过
```

Every row records OS/version, toolchain/version, Qt version/root identifier, architecture, shared/static, configure/build/CTest result, interaction status, evidence path, date, and reviewer. Include separate rows for Windows 10 22H2/Windows 11 with MSVC and MinGW; macOS 12+ arm64/x86_64; Linux X11 KDE, X11 GNOME, Wayland KDE, Wayland GNOME, and forced Qt fallback. Initial rows are `未执行`; automation may promote only to `静态验证通过`, while a signed manual checklist is required for `真机验收通过`.

- [ ] **Step 4: 写三份可审计的真机 checklist**

Each file under `docs/release` starts with exact fields `状态: 未执行`、`测试日期:`、`测试人员:`、`OS/版本:`、`Qt/工具链:`、`设备/显示器:`、`构建产物摘要:`、`结果:`、`问题链接:`. Each check row has expected behavior, actual result, evidence screenshot/log path, and issue link.

- Windows checklist: Win10 22H2/Win11, 100/150/200% DPI, multi-monitor, four-edge/corner resize, drag, double-click maximize, system menu, Snap Layout, minimize/restore, dark/light theme, keyboard/accessibility, and separate MSVC/MinGW packages.
- macOS checklist: macOS 12+ and current release, arm64/x86_64, Retina/multi-monitor, traffic lights, full screen, dark/light theme, blur, keyboard and accessibility.
- Linux checklist: X11 KDE/GNOME, Wayland KDE/GNOME, forced Qt fallback, best-effort system menu, drag/resize, DPI, keyboard, screenshots, and reference performance run.

No item is pre-checked. A checklist may change to `真机验收通过` only with nonempty person/date/device/evidence/result fields and no unresolved issue marked release-blocking.

- [ ] **Step 5: 完成严格的文档审计脚本**

Finish `ZzDocumentationAudit.cmake` with these checks:

```cmake
set(required_docs
    docs/development/CODING_STANDARD_ZH.md
    docs/development/BUILDING_ZH.md
    docs/development/PLATFORM_SUPPORT_ZH.md
    docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md
    docs/release/MANUAL_MACOS_CHECKLIST_ZH.md
    docs/release/MANUAL_LINUX_CHECKLIST_ZH.md
    docs/performance/PERFORMANCE_BASELINE_ZH.md
    docs/third-party/RELEASE_BLOCKERS_ZH.md
    docs/third-party/THIRD_PARTY_NOTICES.md
    docs/third-party/qwindowkit-vendor.json
    docs/third-party/release-evidence.json)
set(unknown_allowlist
    "docs/third-party/RELEASE_BLOCKERS_ZH.md|qwindowkit.upstream-provenance|qmsetup.windeployqt-5.15.2-derived-work|project.license"
    "docs/third-party/qwindowkit-vendor.json|qwindowkit.upstream-provenance|qmsetup.windeployqt-5.15.2-derived-work")
string(CONCAT deferred_phrase "implement" " later")
string(CONCAT analogy_phrase "Similar" " to")
string(CONCAT unfinished_word "place" "holder")
set(unresolved_patterns
    "TO[D]O" "TB[D]" "${unfinished_word}"
    "${deferred_phrase}" "${analogy_phrase}")
set(false_claim_patterns
    "已完全支持" "全平台通过" "全部真机验收通过" "release ready")
```

For every required path, require a regular nonempty file. Search every file below `docs/development`、`docs/release`、`docs/performance` and `docs/third-party`: unresolved patterns are allowed nowhere; `UNKNOWN`/`UNVERIFIED` may occur only in an exact path entry above and only while at least one blocker id on that same entry is present in the nonempty `releaseBlockers` array; false-claim patterns have an empty allowlist and always fail. Parse every platform/checklist status and reject any value outside the three exact states. Reject `静态验证通过` without a matching native runner log reference and reject `真机验收通过` without person/date/device/evidence/result fields.

When invoked with `-DZZ_REQUIRE_REAL_DEVICE=ON`, additionally require real-device status for Windows 10/11 MSVC and MinGW, macOS arm64/x86_64, all five Linux sessions, and all three manual checklist files. This mode is reserved for Task 10 release-candidate verification.

- [ ] **Step 6: 运行文档绿灯并提交**

Run:

```bash
cmake --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R '^architecture\.documentation-audit$' \
  --output-on-failure
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Architecture/ZzDocumentationAudit.cmake
```

Expected: both commands PASS in normal documentation mode；initial `未执行` states remain honest；unresolved-marker and false-claim allowlists are enforced exactly.

```bash
git add docs/development/BUILDING_ZH.md \
  docs/development/CODING_STANDARD_ZH.md \
  docs/development/PLATFORM_SUPPORT_ZH.md \
  docs/release/MANUAL_LINUX_CHECKLIST_ZH.md \
  docs/release/MANUAL_MACOS_CHECKLIST_ZH.md \
  docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md \
  tests/Architecture/CMakeLists.txt \
  tests/Architecture/ZzDocumentationAudit.cmake
git commit -m "文档：建立构建与平台证据手册" \
  -m "记录全部原生 preset、工具链环境和可重现的安装消费命令。" \
  -m "严格区分未执行、静态验证与真机验收状态。"
```

## Task 10: 执行最终发布候选门禁

**Files:**
- Verify: no source file is edited in this task
- Evidence output outside Git: `build/gate-evidence/`

- [ ] **Step 1: 在 Linux 原生 host 执行完整矩阵**

Run with `QT_ROOT`、GCC 13.1+、Clang 17+、reviewed `ZZ_RUNNER_IMAGE_DIGEST` and reviewed `ZZ_GPU_IDENTITY` explicitly provisioned:

```bash
set -o pipefail
mkdir -p build/gate-evidence
bash scripts/ci/run-linux-gates.sh \
  2>&1 | tee build/gate-evidence/linux-native.log
```

Expected: script exits 0；host 上的 Clang tidy shared/static、ASan/UBSan、six-scenario relative performance，以及 immutable Ubuntu 22.04 image 内的 GCC normal/LTO shared/static、architecture、installed headers、A/B relocation、binary dependency、bundled GNU runtime gates all PASS. A missing compiler, Qt SDK, Docker runtime, immutable image digest, real display, runner digest, baseline or log file leaves Task 10 incomplete.

- [ ] **Step 2: 在指定 Linux 参考机执行全部绝对性能门禁**

Run on the fingerprinted reference machine:

```bash
export ZZ_BENCHMARK_COMMIT="$(git rev-parse --verify HEAD)"
: "${ZZ_RUNNER_IMAGE_DIGEST:?set the approved reference image digest}"
: "${ZZ_GPU_IDENTITY:?set the approved renderer and driver identity}"
cmake --preset linux-gcc-reference
cmake --build --preset linux-gcc-reference
ctest --preset linux-gcc-reference -L benchmark --output-on-failure
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.startup.json \
  -DZZ_SCENARIO=startup -DZZ_METRIC=external-total \
  -DZZ_MAX_P95=300 -DZZ_MAX_VALUE=300 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.theme-switch.json \
  -DZZ_SCENARIO=theme-switch -DZZ_METRIC=latency -DZZ_MAX_P95=50 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.animation.json \
  -DZZ_SCENARIO=animation -DZZ_METRIC=frame-time -DZZ_MAX_P95=16.7 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.large-model.json \
  -DZZ_SCENARIO=large-model -DZZ_METRIC=frame-time -DZZ_MAX_P95=16.7 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.idle.json \
  -DZZ_SCENARIO=idle -DZZ_METRIC=average-cpu-percent \
  -DZZ_MAX_VALUE=0.5 -DZZ_STRICT_MAX=ON \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
cmake -DZZ_REPORT=build/linux-gcc-reference/reports/benchmark.idle.json \
  -DZZ_SCENARIO=idle -DZZ_METRIC=rss-growth-percent -DZZ_MAX_VALUE=10 \
  -DZZ_FINGERPRINT_REFERENCE=docs/performance/reference/linux/startup.json \
  -P cmake/ZzVerifyPerformanceReport.cmake
```

Expected: all commands return 0；startup P95/max <=300 ms, theme P95 <=50 ms, animation/model P95 <=16.7 ms, idle CPU <0.5%, RSS growth <=10%, 100-window lifecycle has no leak/UAF or adapter backend/agent residue, and every JSON fingerprint equals the approved reference fingerprint. QWK 上游 native filter 不伪装成 adapter 可直接计数项，其清理由重复生命周期、Sanitizer 和对应真机 checklist 共同验证。

- [ ] **Step 3: 在 Windows 原生 host 执行 MSVC/MinGW 四组合**

From VS 2022 x64 Developer PowerShell with separate Qt 6.8+ kits:

```powershell
New-Item -ItemType Directory -Force build/gate-evidence | Out-Null
pwsh -NoProfile -File scripts/ci/run-windows-gates.ps1 *>&1 |
    Tee-Object build/gate-evidence/windows-native.log
if ($LASTEXITCODE -ne 0) { throw 'Windows native gates failed' }
```

Expected: MSVC shared/static and Qt official MinGW shared/static each configure/build/test/install/relocate/header/binary scan PASS. A substituted MSYS2 kit, one missing linkage mode, missing native host, or absent log is not acceptable evidence.

- [ ] **Step 4: 在 macOS 原生 host 执行双架构四组合**

Run with separate compatible Qt SDK roots:

```bash
set -o pipefail
mkdir -p build/gate-evidence
bash scripts/ci/run-macos-gates.sh \
  2>&1 | tee build/gate-evidence/macos-native.log
```

Expected: arm64/x86_64 shared/static each PASS all CTests and `lipo` reports exactly one expected architecture. Rosetta presence does not substitute for a compatible x86_64 Qt SDK.

- [ ] **Step 5: 要求真机手工证据和无虚假声明文档**

After signed checklists and platform rows are updated from actual devices, run:

```bash
cmake -DZZ_SOURCE_DIR="$PWD" -DZZ_REQUIRE_REAL_DEVICE=ON \
  -P tests/Architecture/ZzDocumentationAudit.cmake
```

Expected: PASS only when Windows 10/11 MSVC+MinGW, macOS arm64/x86_64, Linux five sessions, and the three manual checklists all contain real-device reviewer/date/device/result/evidence. An unexecuted or static-only row blocks the release candidate.

- [ ] **Step 6: 在外部证据齐全后执行 release audit**

Run the audit in the same approved Ubuntu 22.04 build image, with the reviewed evidence directory mounted read-only:

```bash
: "${ZZ_UBUNTU2204_BUILD_IMAGE:?set the approved digest image}"
: "${ZZ_RELEASE_EVIDENCE_ROOT:?set the reviewed evidence directory}"
evidence_root=$(cd "$ZZ_RELEASE_EVIDENCE_ROOT" && pwd -P)
docker pull "$ZZ_UBUNTU2204_BUILD_IMAGE"
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -v "$PWD:/workspace" \
  -v "$evidence_root:/release-evidence:ro" \
  -w /workspace \
  "$ZZ_UBUNTU2204_BUILD_IMAGE" \
  bash -lc '
    cmake -E remove_directory /workspace/build/release-audit &&
    cmake -E remove_directory /workspace/install/release-audit &&
    cmake -E remove_directory /workspace/install/release-audit-verified &&
    cmake -S . -B build/release-audit -G Ninja \
      -DCMAKE_C_COMPILER=/usr/bin/gcc-13 \
      -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/opt/qt \
      -DZZ_QT_PREFIX=/opt/qt \
      -DZZ_RELEASE_EVIDENCE_ROOT=/release-evidence \
      -DZZ_BUNDLE_GNU_RUNTIME=ON \
      -DZZ_GNU_RUNTIME_LICENSE_DIR=/opt/gcc-runtime-licenses \
      -DZZ_RELEASE_BUILD=ON \
      -DZZ_BUILD_TESTS=ON &&
    cmake --build build/release-audit &&
    ctest --test-dir build/release-audit --output-on-failure &&
    cmake --install build/release-audit --prefix install/release-audit &&
    cmake -DZZ_BUILD_DIR=/workspace/build/release-audit \
      -DZZ_INSTALL_ROOT=/workspace/install/release-audit-verified \
      -P cmake/ZzVerifyInstalledLicenses.cmake &&
    bash scripts/ci/check-ubuntu2204-runtime.sh \
      install/release-audit /opt/qt
  '
```

Expected: PASS only with empty `releaseBlockers`, matching bytes/hashes, project `LICENSE`, qwindowkit provenance, `windeployqt 5.15.2` redistribution approval, installed notices/licenses, and reviewer/date evidence. Missing external evidence is a release failure, not a documentation-only exception.

- [ ] **Step 7: 执行最终只读一致性检查**

Run:

```bash
git status --short
git diff --check
if git ls-files | rg -q '(^|/)(build|install|CMakeCache\.txt|gate-evidence)(/|$)'; then
  echo 'tracked build/install artifact found' >&2
  exit 1
fi
if rg -n '/home/zz/|/Users/|[A-Za-z]:[/\\]' \
    CMakeLists.txt CMakePresets.json cmake docs/development tests; then
  echo 'developer absolute path found' >&2
  exit 1
fi
if rg -n 'QWindowKit::|Qt.*/private|QWK' \
    install/release-audit/include install/release-audit/lib/cmake; then
  echo 'private dependency leaked into the install interface' >&2
  exit 1
fi
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Architecture/ZzDocumentationAudit.cmake
```

Expected: only intentional source/document changes are listed; whitespace check returns 0; the tracked-artifact and developer-path searches return no matches; installed public headers/Config contain no QWK/Qt Private token; normal documentation audit PASS.

- [ ] **Step 8: 不在集成门禁中制作修复提交**

Task 10 performs no `git add` or `git commit`. If any command fails, reopen the numbered task that owns that file or evidence, add a focused red/green check there, run that task's minimal green command, and use its scoped commit list. Do not declare a release candidate, tag, or cross-platform success while any native host, matching Qt SDK, reference fingerprint, manual checklist, or external release evidence is absent.

## 完成标准

- 根工程在 configure 阶段拒绝不支持所需 C++20 标准库的工具链，并保留 GCC 13.1+/Clang 17+/MSVC 19.38+/AppleClang 15+ 下限。
- Preset 矩阵为每个平台保留 Qt prefix 和字面量 preset 身份，覆盖 Linux 检查组合、Windows 两套 ABI、macOS 双架构及 shared/static。
- 性能 JSON 有固定 schema、runner fingerprint、绝对参考机门禁和同环境相对回归门禁。
- 全新 producer 安装到 prefix A，复制到 prefix B 后删除 A；InstallConsumer 与逐头 PublicHeaderConsumer 都只消费 B 和原 Qt prefix。
- 架构、公开头、二进制依赖、Ubuntu 22.04 GLIBC/GLIBCXX 和 unresolved-dependency 检查均自动失败关闭。
- Linux、Windows MSVC、Windows Qt MinGW、macOS arm64/x86_64 有各自原生脚本日志；静态验证与真机交互证据不混用。
- 项目许可证、qwindowkit provenance、`windeployqt 5.15.2` 衍生代码依据、notices、证据文件 hash、reviewer/date 任一缺失时 release configure 失败。
- Task 10 不产生兜底提交；只有全部自动门禁、参考性能、原生 host、真机 checklist 和外部合规证据齐全时才可声明发布候选通过。
