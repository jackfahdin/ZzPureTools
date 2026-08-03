# 仓库基线与 CMake 工程 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立可由 Linux GCC 13.1 或更高版本配置的 Qt 6.8/C++20 工程基线，并让默认 shared、正式 static、公共头门禁、安装导出、LTO、clang-tidy 和独立消费测试都可复现。

**Architecture:** 根 `CMakeLists.txt` 是构建选项和依赖顺序的唯一事实来源；组件只声明自己的 target、源码和公开依赖。严格警告只附着到显式列出的一方源码，AUTOMOC/AUTORCC 生成源码和第三方源码不继承一方 `-Werror` 或 clang-tidy；安装测试从源码重新配置全新 `A` 构建树，安装到全新 `B` 前缀，再从全新 `consumer` 构建树消费。

**Tech Stack:** CMake 3.23、CMakePresets schema 4、Ninja、GCC 13.1+、Clang/clang-tidy、Qt 6.8+ Core/Gui/Widgets/Svg/Concurrent/Test、C++20、CTest、GNUInstallDirs、CMakePackageConfigHelpers。

---

## 执行约束

- 所有命令都从内层仓库 `/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro` 执行。
- 开始前阅读 `docs/superpowers/specs/2026-08-02-zzpuretoolspro-architecture-design.md`，不得修改 `ZzThirdParty/qwindowkit`。
- 执行机必须让 `g++-13` 可从 `PATH` 找到，且版本不低于 13.1；`QT_ROOT` 必须指向 Qt 6.8 或更高版本的当前编译器套件前缀。
- 每个任务都先执行明确的红灯命令，再写最小实现；红灯只用于证明缺口，不得在红灯状态提交。
- 每次 `git commit` 前必须重新运行该任务列出的绿灯 configure/build/test，任何 configure 失败都必须先修复。
- 本计划只建立工程和最小版本 API，不实现 ZzLog API 重构、QWindowKit 后端、主题、控件或应用框架业务行为。
- 提交命令使用两个 `-m` 形成中文标题和中文正文，不在参数中写字面换行转义。

## 文件职责

- `.gitignore`：隔离构建、安装、用户 preset 和分析产物。
- `CMakeLists.txt`：定义项目、选项、Qt 版本、子目录顺序和安装入口。
- `CMakePresets.json`：定义 GCC 13.1+、shared/static、LTO、clang-tidy 和 sanitizer 组合。
- `CMakeUserPresets.json.example`：只展示环境变量继承，不保存开发机绝对路径。
- `cmake/ZzCompilerWarnings.cmake`：只给显式一方源码设置严格警告。
- `cmake/ZzSanitizers.cmake`：给一方 target 设置 ASan/UBSan 编译和链接参数。
- `cmake/ZzLto.cmake`：检查并启用 target IPO/LTO。
- `cmake/ZzStaticAnalysis.cmake`：只把显式一方源码注册到 clang-tidy 聚合 target。
- `cmake/ZzFirstPartyTarget.cmake`：统一一方 target 的 C++20、AUTOMOC/AUTORCC 和质量策略。
- `cmake/ZzLibraryTarget.cmake`：统一六个库的导出宏、可见性、build/install include 和静态宏。
- `cmake/ZzArchitectureChecks.cmake`：定义 `ZzPublicHeadersTest` 及公共头逐文件编译规则。
- `cmake/ZzInstallPackage.cmake`：安装六个 target、全部产物、公开头、Config 和 Version 文件。
- `cmake/ZzPureToolsProConfig.cmake.in`：声明已安装包的 Qt 依赖并导入 `Zz::` target。
- `examples/CMakeLists.txt`：作为唯一示例聚合入口，由后续组件计划按执行顺序追加子目录。
- `tests/Architecture/`：版本 API、生成代码边界、公共头和架构扫描门禁。
- `tests/InstallConsumer/`：全新 A/B/consumer 安装消费驱动和真正的外部消费者。

## Task 1: 建立可成功配置的 GCC 13.1+/Qt 6.8 根工程

**Files:**
- Create: `.gitignore`
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `CMakeUserPresets.json.example`

- [ ] **Step 1: 运行 Preset 红灯**

Run:

```bash
cmake --list-presets
```

Expected: FAIL，CMake 明确报告根目录没有 `CMakePresets.json`；失败原因只能是仓库尚未定义 preset，而不是编译器或 Qt 查找失败。

- [ ] **Step 2: 写入忽略规则**

Create `.gitignore` with:

```gitignore
/build/
/install/
/package/
/CMakeUserPresets.json
/.cache/
/.clangd/
compile_commands.json
*.user
*.swp
*~
```

- [ ] **Step 3: 写入完整根工程入口**

Create `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.23)

project(ZzPureToolsPro
    VERSION 0.1.0
    DESCRIPTION "High-performance cross-platform Qt application framework"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build shared libraries by default" ON)
option(ZZ_BUILD_TESTS "Build ZzPureToolsPro tests" ${BUILD_TESTING})
option(ZZ_BUILD_EXAMPLES "Build ZzPureToolsPro examples" OFF)
option(ZZ_BUILD_BENCHMARKS "Build ZzPureToolsPro benchmarks" OFF)
option(ZZ_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ZZ_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ZZ_ENABLE_CLANG_TIDY "Run clang-tidy on explicit first-party sources" OFF)
option(ZZ_WARNINGS_AS_ERRORS "Treat first-party warnings as errors" OFF)
option(ZZ_ENABLE_LTO "Enable interprocedural optimization" OFF)
option(ZZ_BUILD_FLUENT_QUICK "Build the future Qt Quick frontend" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.1)
    message(FATAL_ERROR
        "GCC 13.1 or newer is required; found ${CMAKE_CXX_COMPILER_VERSION}")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Svg
    Concurrent
)

if(ZZ_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()
```

这里暂不引用尚未创建的 helper 或组件子目录，因此首次根配置必须成功，不能用“下一任务会补文件”解释配置失败。

- [ ] **Step 4: 写入完整 Linux Preset 矩阵**

Create `CMakePresets.json` with:

```json
{
  "version": 4,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 23,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "linux-base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "installDir": "${sourceDir}/install/${presetName}",
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      },
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "$env{QT_ROOT}",
        "CMAKE_EXPORT_COMPILE_COMMANDS": true,
        "ZZ_BUILD_TESTS": true,
        "ZZ_BUILD_EXAMPLES": false,
        "ZZ_BUILD_BENCHMARKS": false,
        "ZZ_WARNINGS_AS_ERRORS": true
      }
    },
    {
      "name": "linux-gcc-13-1-base",
      "hidden": true,
      "inherits": "linux-base",
      "cacheVariables": {
        "CMAKE_CXX_COMPILER": "g++-13"
      }
    },
    {
      "name": "linux-gcc-debug",
      "inherits": "linux-gcc-13-1-base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "linux-gcc-release",
      "inherits": "linux-gcc-13-1-base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "linux-static-release",
      "inherits": "linux-gcc-13-1-base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "BUILD_SHARED_LIBS": false
      }
    },
    {
      "name": "linux-gcc-lto-release",
      "inherits": "linux-gcc-13-1-base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "ZZ_ENABLE_LTO": true
      }
    },
    {
      "name": "linux-clang-release",
      "inherits": "linux-base",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_CXX_COMPILER": "clang++",
        "CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN": "$env{GCC_13_TOOLCHAIN_ROOT}"
      }
    },
    {
      "name": "linux-clang-tidy",
      "inherits": "linux-clang-release",
      "cacheVariables": {
        "ZZ_ENABLE_CLANG_TIDY": true
      }
    },
    {
      "name": "linux-clang-asan",
      "inherits": "linux-clang-release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "ZZ_ENABLE_ASAN": true,
        "ZZ_ENABLE_UBSAN": true
      }
    }
  ],
  "buildPresets": [
    { "name": "linux-gcc-debug", "configurePreset": "linux-gcc-debug" },
    { "name": "linux-gcc-release", "configurePreset": "linux-gcc-release" },
    { "name": "linux-static-release", "configurePreset": "linux-static-release" },
    { "name": "linux-gcc-lto-release", "configurePreset": "linux-gcc-lto-release" },
    { "name": "linux-clang-release", "configurePreset": "linux-clang-release" },
    { "name": "linux-clang-tidy", "configurePreset": "linux-clang-tidy" },
    { "name": "linux-clang-asan", "configurePreset": "linux-clang-asan" }
  ],
  "testPresets": [
    {
      "name": "linux-gcc-debug",
      "configurePreset": "linux-gcc-debug",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" }
    },
    {
      "name": "linux-gcc-release",
      "configurePreset": "linux-gcc-release",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" }
    },
    {
      "name": "linux-static-release",
      "configurePreset": "linux-static-release",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" }
    },
    {
      "name": "linux-gcc-lto-release",
      "configurePreset": "linux-gcc-lto-release",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" }
    },
    {
      "name": "linux-clang-release",
      "configurePreset": "linux-clang-release",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" }
    },
    {
      "name": "linux-clang-tidy",
      "configurePreset": "linux-clang-tidy",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" }
    },
    {
      "name": "linux-clang-asan",
      "configurePreset": "linux-clang-asan",
      "output": { "outputOnFailure": true },
      "execution": { "noTestsAction": "error" },
      "environment": {
        "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
        "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"
      }
    }
  ]
}
```

`linux-static-release` 是正式支持的静态配置，不是临时诊断 preset；未显式传入 `BUILD_SHARED_LIBS` 时，根 option 仍默认 `ON`。
Linux Clang preset 通过 `CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN` 固定使用 GCC 13.1+ 配套 libstdc++，不得静默拾取 Ubuntu 22.04 的 GCC 11 标准库。`GCC_13_TOOLCHAIN_ROOT` 是包含 GCC 13 runtime/include/lib 的 toolchain 根，不是 `g++` 可执行文件路径；后续能力探针仍负责证明 `<format>` 等功能实际可用。

- [ ] **Step 5: 提供无绝对路径的用户 Preset 示例**

Create `CMakeUserPresets.json.example` with:

```json
{
  "version": 4,
  "include": ["CMakePresets.json"],
  "configurePresets": [
    {
      "name": "local-linux-gcc-debug",
      "inherits": "linux-gcc-debug",
      "environment": {
        "QT_ROOT": "$penv{QT_ROOT}",
        "GCC_13_TOOLCHAIN_ROOT": "$penv{GCC_13_TOOLCHAIN_ROOT}"
      }
    }
  ]
}
```

- [ ] **Step 6: 运行根配置绿灯**

Run:

```bash
set -euo pipefail
test -n "${QT_ROOT}"
command -v g++-13
g++-13 -dumpfullversion
cmake --list-presets
cmake --preset linux-gcc-debug
```

Expected: `test` 和 `command` 返回 0；GCC 输出版本不低于 `13.1`；preset 列表包含七个公开 Linux configure preset；最后的 configure 返回 0，并显示找到 Qt 6.8 或更高版本。若编译器低于 13.1，必须看到根文件定义的 `GCC 13.1 or newer is required`，不能继续提交。

- [ ] **Step 7: 提交可配置根工程**

```bash
git add .gitignore CMakeLists.txt CMakePresets.json CMakeUserPresets.json.example
git commit -m "构建：建立 GCC 13.1 工程入口" \
  -m "固定 Qt 6.8、C++20 和默认共享库策略。" \
  -m "提供正式静态、LTO、clang-tidy 与 sanitizer preset，并允许根工程独立配置。"
```

## Task 2: 隔离一方警告、生成代码、LTO 与 clang-tidy

**Files:**
- Create: `cmake/ZzCompilerWarnings.cmake`
- Create: `cmake/ZzSanitizers.cmake`
- Create: `cmake/ZzLto.cmake`
- Create: `cmake/ZzStaticAnalysis.cmake`
- Create: `cmake/ZzFirstPartyTarget.cmake`
- Create: `tests/CMakeLists.txt`
- Create: `tests/Architecture/CMakeLists.txt`
- Create: `tests/Architecture/ZzGeneratedCodeProbe.h`
- Create: `tests/Architecture/ZzGeneratedCodeProbe.cpp`
- Create: `tests/Architecture/resources/ZzGeneratedCodeProbe.qrc`
- Create: `tests/Architecture/resources/data/ZzGeneratedCodeProbe.txt`
- Create: `tests/Architecture/CheckGeneratedCodeFlags.cmake`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 运行质量 target 红灯**

Run:

```bash
cmake --preset linux-gcc-lto-release
cmake --preset linux-clang-tidy
cmake --build --preset linux-gcc-lto-release --target ZzGeneratedCodeProbe
cmake --build --preset linux-clang-tidy --target ZzClangTidy
```

Expected: 两条 configure 命令返回 0，证明工程未处于配置失败状态；两条 build 命令都 FAIL，并分别报告未知 target `ZzGeneratedCodeProbe` 和 `ZzClangTidy`。失败证明 LTO 与 clang-tidy 选项尚未连接到真实一方源码。

- [ ] **Step 2: 实现只作用于显式一方源码的警告函数**

Create `cmake/ZzCompilerWarnings.cmake` with:

```cmake
include_guard(GLOBAL)

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
    else()
        set(zz_warning_options
            -Wall
            -Wextra
            -Wpedantic
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

function(zz_enable_project_warnings target_name)
    get_target_property(zz_target_sources ${target_name} SOURCES)
    if(NOT zz_target_sources)
        message(FATAL_ERROR
            "zz_enable_project_warnings(${target_name}) requires sources")
    endif()

    set(zz_first_party_translation_units)
    foreach(zz_source IN LISTS zz_target_sources)
        if("${zz_source}" MATCHES "\\.(cc|cpp|cxx|mm)$")
            list(APPEND zz_first_party_translation_units "${zz_source}")
        endif()
    endforeach()
    if(NOT zz_first_party_translation_units)
        message(FATAL_ERROR
            "${target_name} has no explicit first-party translation unit")
    endif()

    zz_apply_first_party_warnings(${target_name}
        SOURCES ${zz_first_party_translation_units})
endfunction()
```

`zz_enable_project_warnings()` 是后续所有组件和测试使用的唯一公开警告 helper。必须在该 target 的全部一方 `.cpp`/`.mm` 已进入 `SOURCES` 后调用；禁止随后追加翻译单元。禁止把这些选项改成 `target_compile_options()`：target 级 `-Werror` 会同时进入 `mocs_compilation.cpp` 和 `qrc_*.cpp`，破坏架构规定的生成代码边界。

- [ ] **Step 3: 实现 Sanitizer 和 LTO helper**

Create `cmake/ZzSanitizers.cmake` with:

```cmake
include_guard(GLOBAL)

function(zz_enable_sanitizers target_name)
    if(MSVC)
        if(ZZ_ENABLE_ASAN)
            target_compile_options(${target_name} PRIVATE /fsanitize=address)
            target_link_options(${target_name} PRIVATE /fsanitize=address)
        endif()
        if(ZZ_ENABLE_UBSAN)
            message(FATAL_ERROR "ZZ_ENABLE_UBSAN is not supported by MSVC presets")
        endif()
        return()
    endif()

    set(zz_sanitizers)
    if(ZZ_ENABLE_ASAN)
        list(APPEND zz_sanitizers address)
    endif()
    if(ZZ_ENABLE_UBSAN)
        list(APPEND zz_sanitizers undefined)
    endif()

    if(zz_sanitizers)
        list(JOIN zz_sanitizers "," zz_sanitizer_list)
        target_compile_options(${target_name} PRIVATE
            -fno-omit-frame-pointer
            "-fsanitize=${zz_sanitizer_list}"
        )
        target_link_options(${target_name} PRIVATE
            -fno-omit-frame-pointer
            "-fsanitize=${zz_sanitizer_list}"
        )
    endif()
endfunction()
```

Create `cmake/ZzLto.cmake` with:

```cmake
include_guard(GLOBAL)
include(CheckIPOSupported)

function(zz_enable_lto target_name)
    if(NOT ZZ_ENABLE_LTO)
        return()
    endif()

    check_ipo_supported(
        RESULT zz_ipo_supported
        OUTPUT zz_ipo_error
        LANGUAGES CXX
    )
    if(NOT zz_ipo_supported)
        message(FATAL_ERROR
            "ZZ_ENABLE_LTO=ON, but ${target_name} cannot enable IPO: ${zz_ipo_error}")
    endif()

    set_property(TARGET ${target_name} PROPERTY
        INTERPROCEDURAL_OPTIMIZATION TRUE)
endfunction()
```

- [ ] **Step 4: 实现只接收显式源码清单的 clang-tidy target**

Create `cmake/ZzStaticAnalysis.cmake` with:

```cmake
include_guard(GLOBAL)

function(zz_register_clang_tidy target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_TIDY "" "" "SOURCES")
    if(NOT ZZ_ENABLE_CLANG_TIDY)
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

    if(NOT ZZ_CLANG_TIDY_EXECUTABLE)
        find_program(zz_clang_tidy_program NAMES clang-tidy REQUIRED)
        set(ZZ_CLANG_TIDY_EXECUTABLE
            "${zz_clang_tidy_program}"
            CACHE FILEPATH "clang-tidy executable used by Zz targets")
    endif()

    if(NOT TARGET ZzClangTidy)
        add_custom_target(ZzClangTidy)
    endif()

    set(zz_tidy_stamps)
    foreach(zz_source IN LISTS ZZ_TIDY_SOURCES)
        get_filename_component(zz_source_absolute
            "${zz_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        string(SHA256 zz_source_hash "${zz_source_absolute}")
        string(SUBSTRING "${zz_source_hash}" 0 16 zz_source_hash_short)
        set(zz_stamp
            "${CMAKE_BINARY_DIR}/clang-tidy/${target_name}/${zz_source_hash_short}.stamp")

        add_custom_command(
            OUTPUT "${zz_stamp}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_BINARY_DIR}/clang-tidy/${target_name}"
            COMMAND "${ZZ_CLANG_TIDY_EXECUTABLE}"
                "-p=${CMAKE_BINARY_DIR}"
                "--checks=-*,clang-analyzer-*,bugprone-*,performance-*,modernize-use-nullptr,modernize-use-override"
                "--warnings-as-errors=clang-analyzer-*,bugprone-*,performance-*"
                "${zz_source_absolute}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${zz_stamp}"
            DEPENDS
                "${zz_source_absolute}"
                "${CMAKE_BINARY_DIR}/compile_commands.json"
            VERBATIM
        )
        list(APPEND zz_tidy_stamps "${zz_stamp}")
    endforeach()

    set(zz_target_tidy "${target_name}ClangTidy")
    add_custom_target(${zz_target_tidy} DEPENDS ${zz_tidy_stamps})
    add_dependencies(${zz_target_tidy} ${target_name})
    add_dependencies(ZzClangTidy ${zz_target_tidy})
endfunction()
```

`ZZ_TIDY_SOURCES` 永远只由调用者列出一方 `.cpp`；不要把 target 的 `SOURCES` 属性整体转交 clang-tidy，因为该属性会包含 CMake/Qt 生成文件。

- [ ] **Step 5: 组合一方 target 策略**

Create `cmake/ZzFirstPartyTarget.cmake` with:

```cmake
include_guard(GLOBAL)

include(ZzCompilerWarnings)
include(ZzLto)
include(ZzSanitizers)
include(ZzStaticAnalysis)

function(zz_configure_first_party_target target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_TARGET "" "" "SOURCES")
    if(NOT ZZ_TARGET_SOURCES)
        message(FATAL_ERROR
            "zz_configure_first_party_target(${target_name}) requires SOURCES")
    endif()

    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        AUTOMOC ON
        AUTORCC ON
    )

    zz_enable_project_warnings(${target_name})
    zz_enable_sanitizers(${target_name})
    zz_enable_lto(${target_name})
    zz_register_clang_tidy(${target_name}
        SOURCES ${ZZ_TARGET_SOURCES})
endfunction()
```

- [ ] **Step 6: 创建同时触发 AUTOMOC 和 AUTORCC 的探针**

Create `tests/Architecture/ZzGeneratedCodeProbe.h` with:

```cpp
#pragma once

#include <QtCore/QObject>

/**
 * @brief 触发 AUTOMOC 并为生成代码编译边界提供稳定探针。
 *
 * 该测试对象只在创建它的测试线程使用，不向外转移所有权。
 */
class ZzGeneratedCodeProbe final : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
};
```

Create `tests/Architecture/ZzGeneratedCodeProbe.cpp` with:

```cpp
#include "ZzGeneratedCodeProbe.h"

#include <QtCore/QFile>

int main()
{
    [[maybe_unused]] ZzGeneratedCodeProbe probe;
    QFile resource(QStringLiteral(":/probe/message.txt"));
    if (!resource.open(QIODevice::ReadOnly)) {
        return 1;
    }

    return resource.readAll().trimmed() == "generated-code-probe" ? 0 : 2;
}
```

Create `tests/Architecture/resources/ZzGeneratedCodeProbe.qrc` with:

```xml
<RCC>
    <qresource prefix="/probe">
        <file alias="message.txt">data/ZzGeneratedCodeProbe.txt</file>
    </qresource>
</RCC>
```

Create `tests/Architecture/resources/data/ZzGeneratedCodeProbe.txt` with:

```text
generated-code-probe
```

- [ ] **Step 7: 添加 compile_commands 边界断言**

Create `tests/Architecture/CheckGeneratedCodeFlags.cmake` with:

```cmake
cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_COMPILE_COMMANDS)
    message(FATAL_ERROR "ZZ_COMPILE_COMMANDS is required")
endif()
if(NOT EXISTS "${ZZ_COMPILE_COMMANDS}")
    message(FATAL_ERROR
        "compile_commands.json does not exist: ${ZZ_COMPILE_COMMANDS}")
endif()

file(READ "${ZZ_COMPILE_COMMANDS}" zz_compile_database)
string(JSON zz_entry_count LENGTH "${zz_compile_database}")
if(zz_entry_count EQUAL 0)
    message(FATAL_ERROR "compile_commands.json is empty")
endif()

set(zz_found_first_party FALSE)
set(zz_found_moc FALSE)
set(zz_found_rcc FALSE)
math(EXPR zz_last_entry "${zz_entry_count} - 1")

foreach(zz_index RANGE 0 ${zz_last_entry})
    string(JSON zz_file GET "${zz_compile_database}" ${zz_index} file)
    string(JSON zz_command GET "${zz_compile_database}" ${zz_index} command)
    file(TO_CMAKE_PATH "${zz_file}" zz_file_normalized)

    if(zz_file_normalized MATCHES "/ZzGeneratedCodeProbe\\.cpp$")
        set(zz_found_first_party TRUE)
        if(NOT zz_command MATCHES "(^|[ \\t])(-Werror|/WX)([ \\t]|$)")
            message(FATAL_ERROR
                "first-party probe is missing -Werror or /WX: ${zz_command}")
        endif()
    elseif(zz_file_normalized MATCHES "/mocs_compilation\\.cpp$")
        set(zz_found_moc TRUE)
        if(zz_command MATCHES "(^|[ \\t])(-Werror|/WX)([ \\t]|$)")
            message(FATAL_ERROR
                "AUTOMOC source inherited first-party warnings: ${zz_command}")
        endif()
    elseif(zz_file_normalized MATCHES "/qrc_ZzGeneratedCodeProbe\\.cpp$")
        set(zz_found_rcc TRUE)
        if(zz_command MATCHES "(^|[ \\t])(-Werror|/WX)([ \\t]|$)")
            message(FATAL_ERROR
                "AUTORCC source inherited first-party warnings: ${zz_command}")
        endif()
    endif()
endforeach()

if(NOT zz_found_first_party)
    message(FATAL_ERROR "first-party probe compile command was not found")
endif()
if(NOT zz_found_moc)
    message(FATAL_ERROR "AUTOMOC compile command was not found")
endif()
if(NOT zz_found_rcc)
    message(FATAL_ERROR "AUTORCC compile command was not found")
endif()
```

- [ ] **Step 8: 注册探针和 CTest**

Create `tests/CMakeLists.txt` with:

```cmake
add_subdirectory(Architecture)
```

Create `tests/Architecture/CMakeLists.txt` with:

```cmake
add_executable(ZzGeneratedCodeProbe
    ZzGeneratedCodeProbe.h
    ZzGeneratedCodeProbe.cpp
    resources/ZzGeneratedCodeProbe.qrc
)
target_link_libraries(ZzGeneratedCodeProbe PRIVATE Qt6::Core)
zz_configure_first_party_target(ZzGeneratedCodeProbe
    SOURCES ZzGeneratedCodeProbe.cpp)

add_test(
    NAME architecture.generated-code-probe
    COMMAND ZzGeneratedCodeProbe
)
set_tests_properties(architecture.generated-code-probe PROPERTIES
    LABELS "architecture;unit"
)

add_test(
    NAME architecture.generated-code-flags
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_COMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckGeneratedCodeFlags.cmake"
)
set_tests_properties(architecture.generated-code-flags PROPERTIES
    LABELS "architecture"
)
```

- [ ] **Step 9: 把 helper 和测试接入根工程**

Append to `CMakeLists.txt` immediately before the end of the file:

```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ZzFirstPartyTarget)

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 10: 运行普通、LTO、生成代码和 clang-tidy 绿灯**

Run:

```bash
set -euo pipefail
: "${GCC_13_TOOLCHAIN_ROOT:?set the GCC 13.1+ toolchain root used by Clang}"
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R 'architecture.generated-code-(probe|flags)'
cmake --preset linux-gcc-lto-release
cmake --build --preset linux-gcc-lto-release --target ZzGeneratedCodeProbe --verbose 2>&1 | tee build/linux-gcc-lto-release/lto-build.log
rg -- '-flto([^[:space:]]*)?' build/linux-gcc-lto-release/lto-build.log
cmake --preset linux-clang-tidy
cmake --build --preset linux-clang-tidy
cmake --build --preset linux-clang-tidy --target ZzClangTidy --verbose 2>&1 | tee build/linux-clang-tidy/clang-tidy.log
rg 'ZzGeneratedCodeProbe\\.cpp' build/linux-clang-tidy/clang-tidy.log
if rg 'mocs_compilation|qrc_ZzGeneratedCodeProbe' build/linux-clang-tidy/clang-tidy.log; then exit 1; fi
```

Expected: 所有 configure/build/CTest 返回 0；LTO 日志至少出现一次 GCC `-flto` 选项；clang-tidy 日志包含一方 `ZzGeneratedCodeProbe.cpp`，最后的负向扫描无匹配并返回 0。`architecture.generated-code-flags` 同时证明一方源码有 `-Werror`，MOC/RCC 生成源码没有 `-Werror`。

- [ ] **Step 11: 提交质量边界**

```bash
git add CMakeLists.txt cmake/ZzCompilerWarnings.cmake cmake/ZzSanitizers.cmake cmake/ZzLto.cmake cmake/ZzStaticAnalysis.cmake cmake/ZzFirstPartyTarget.cmake tests/CMakeLists.txt tests/Architecture
git commit -m "构建：隔离一方代码质量策略" \
  -m "将严格警告和 clang-tidy 限定到显式一方源码，并验证生成代码边界。" \
  -m "让 LTO、sanitizer 与 C++20 通过统一 target helper 实际生效。"
```

## Task 3: 创建六个最小导出 target 和版本 API

**Files:**
- Create: `cmake/ZzLibraryTarget.cmake`
- Create: `ZzCore/CMakeLists.txt`
- Create: `ZzCore/include/ZzCore/ZzCoreVersion.h`
- Create: `ZzCore/src/private/ZzCoreVersion.cpp`
- Create: `ZzWindowKit/CMakeLists.txt`
- Create: `ZzWindowKit/include/ZzWindowKit/ZzWindowKitVersion.h`
- Create: `ZzWindowKit/src/private/ZzWindowKitVersion.cpp`
- Create: `ZzFluentUI/CMakeLists.txt`
- Create: `ZzFluentUI/foundation/include/ZzFluentUI/ZzFluentVersion.h`
- Create: `ZzFluentUI/foundation/src/private/ZzFluentVersion.cpp`
- Create: `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentWidgetVersion.h`
- Create: `ZzFluentUI/widgets/src/private/ZzFluentWidgetVersion.cpp`
- Create: `ZzPureTools/CMakeLists.txt`
- Create: `ZzPureTools/appcore/include/ZzPureTools/ZzAppCoreVersion.h`
- Create: `ZzPureTools/appcore/src/private/ZzAppCoreVersion.cpp`
- Create: `ZzPureTools/widgets/include/ZzPureTools/ZzPureToolsVersion.h`
- Create: `ZzPureTools/widgets/src/private/ZzPureToolsVersion.cpp`
- Create: `tests/Architecture/ZzVersionApiTest.h`
- Create: `tests/Architecture/ZzVersionApiTest.cpp`
- Create: `tests/Architecture/ZzVersionApiTestMain.cpp`
- Create: `examples/CMakeLists.txt`
- Modify: `tests/Architecture/CMakeLists.txt`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先写版本 API 测试**

Create `tests/Architecture/ZzVersionApiTest.h` with:

```cpp
#pragma once

#include <QtCore/QObject>

class ZzVersionApiTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void reportsProjectVersion();
};
```

Create `tests/Architecture/ZzVersionApiTest.cpp` with:

```cpp
#include "ZzVersionApiTest.h"

#include <QtTest/QTest>

#include <ZzCore/ZzCoreVersion.h>
#include <ZzFluentUI/ZzFluentVersion.h>
#include <ZzFluentUI/ZzFluentWidgetVersion.h>
#include <ZzPureTools/ZzAppCoreVersion.h>
#include <ZzPureTools/ZzPureToolsVersion.h>
#include <ZzWindowKit/ZzWindowKitVersion.h>

void ZzVersionApiTest::reportsProjectVersion()
{
    QCOMPARE(ZzCore::ZzCoreVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzWindowKit::ZzWindowKitVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzFluentUI::ZzFluentVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzFluentUI::ZzFluentWidgetVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzPureTools::ZzAppCoreVersion::toString(), QStringLiteral("0.1.0"));
    QCOMPARE(ZzPureTools::ZzPureToolsVersion::toString(), QStringLiteral("0.1.0"));
}
```

Create `tests/Architecture/ZzVersionApiTestMain.cpp` with:

```cpp
#include "ZzVersionApiTest.h"

#include <QtTest/QTest>

QTEST_GUILESS_MAIN(ZzVersionApiTest)
```

Append this exact block to `tests/Architecture/CMakeLists.txt`:

```cmake
add_executable(ZzVersionApiTest
    ZzVersionApiTest.h
    ZzVersionApiTest.cpp
    ZzVersionApiTestMain.cpp
)
target_link_libraries(ZzVersionApiTest PRIVATE
    Qt6::Test
    Zz::Core
    Zz::WindowKit
    Zz::FluentFoundation
    Zz::FluentUI
    Zz::AppCore
    Zz::PureTools
)
zz_configure_first_party_target(ZzVersionApiTest
    SOURCES
        ZzVersionApiTest.cpp
        ZzVersionApiTestMain.cpp
)

add_test(NAME architecture.version-api COMMAND ZzVersionApiTest)
set_tests_properties(architecture.version-api PROPERTIES
    LABELS "architecture;unit"
)
```

- [ ] **Step 2: 运行缺少导出 target 的红灯**

Run:

```bash
cmake --preset linux-gcc-debug
```

Expected: FAIL at `target_link_libraries()`，明确指出 `Zz::Core` 或后续 `Zz::` target 不存在；失败原因是六个库尚未创建，不得把测试链接临时删掉来获取绿灯。

- [ ] **Step 3: 实现库 target 公共 helper**

Create `cmake/ZzLibraryTarget.cmake` with:

```cmake
include_guard(GLOBAL)

include(GenerateExportHeader)
include(ZzFirstPartyTarget)

function(zz_configure_library_target target_name)
    set(zz_one_value_args
        EXPORT_NAME
        PUBLIC_INCLUDE_DIR
        EXPORT_HEADER_SUBDIR
        EXPORT_HEADER_NAME
        EXPORT_MACRO_NAME
    )
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_LIBRARY
        "" "${zz_one_value_args}" "SOURCES;MOC_HEADERS")

    foreach(zz_required_arg IN ITEMS
        EXPORT_NAME
        PUBLIC_INCLUDE_DIR
        EXPORT_HEADER_SUBDIR
        EXPORT_HEADER_NAME
        EXPORT_MACRO_NAME
    )
        if(NOT ZZ_LIBRARY_${zz_required_arg})
            message(FATAL_ERROR
                "zz_configure_library_target(${target_name}) requires ${zz_required_arg}")
        endif()
    endforeach()
    if(NOT ZZ_LIBRARY_SOURCES)
        message(FATAL_ERROR
            "zz_configure_library_target(${target_name}) requires SOURCES")
    endif()

    if(ZZ_LIBRARY_MOC_HEADERS)
        # 公开 Q_OBJECT 头可能与实现文件分处 include/src；显式交给 AUTOMOC。
        # 它们不进入 SOURCES，因此不会被当作 clang-tidy 翻译单元。
        target_sources(${target_name} PRIVATE ${ZZ_LIBRARY_MOC_HEADERS})
    endif()

    set(zz_generated_include_dir
        "${CMAKE_CURRENT_BINARY_DIR}/generated/${target_name}/include")
    set(zz_generated_export_header
        "${zz_generated_include_dir}/${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}/${ZZ_LIBRARY_EXPORT_HEADER_NAME}")
    file(MAKE_DIRECTORY
        "${zz_generated_include_dir}/${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}")

    generate_export_header(${target_name}
        EXPORT_FILE_NAME "${zz_generated_export_header}"
        EXPORT_MACRO_NAME "${ZZ_LIBRARY_EXPORT_MACRO_NAME}"
        STATIC_DEFINE "${ZZ_LIBRARY_EXPORT_MACRO_NAME}_STATIC_DEFINE"
    )

    get_target_property(zz_library_type ${target_name} TYPE)
    if(zz_library_type STREQUAL "STATIC_LIBRARY")
        target_compile_definitions(${target_name} PUBLIC
            "${ZZ_LIBRARY_EXPORT_MACRO_NAME}_STATIC_DEFINE")
    endif()

    target_include_directories(${target_name}
        PUBLIC
            "$<BUILD_INTERFACE:${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}>"
            "$<BUILD_INTERFACE:${zz_generated_include_dir}>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    file(GLOB_RECURSE zz_source_public_headers
        CONFIGURE_DEPENDS
        RELATIVE "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.h"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.hh"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.hpp"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.hxx")
    list(APPEND zz_source_public_headers
        "${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}/${ZZ_LIBRARY_EXPORT_HEADER_NAME}")
    list(REMOVE_DUPLICATES zz_source_public_headers)
    list(SORT zz_source_public_headers)

    set_target_properties(${target_name} PROPERTIES
        EXPORT_NAME "${ZZ_LIBRARY_EXPORT_NAME}"
        VERSION "${PROJECT_VERSION}"
        SOVERSION "${PROJECT_VERSION_MAJOR}"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        ZZ_GENERATED_EXPORT_HEADER "${zz_generated_export_header}"
        ZZ_EXPORT_HEADER_INSTALL_SUBDIR
            "${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}"
        ZZ_PUBLIC_HEADERS "${zz_source_public_headers}"
        EXPORT_PROPERTIES ZZ_PUBLIC_HEADERS
    )

    zz_configure_first_party_target(${target_name}
        SOURCES ${ZZ_LIBRARY_SOURCES})
endfunction()
```

静态库把 `${EXPORT_MACRO_NAME}_STATIC_DEFINE` 作为 `PUBLIC` 使用要求导出，保证 Windows 静态消费者不会把静态符号误标为 `dllimport`；共享库不定义该宏，由 `GenerateExportHeader` 生成正确的导入/导出分支。`ZZ_PUBLIC_HEADERS` 保存相对于统一安装 include 根的精确头路径，并通过 `EXPORT_PROPERTIES` 写入 imported target；它包含源码公共头和当前 target 的生成导出头，供后续逐头消费者验证唯一 owner。新增公共头后 `CONFIGURE_DEPENDS` 会触发重新配置，但安装规则仍以显式 target/目录安装声明为事实来源。

- [ ] **Step 4: 创建 ZzCore 版本 API 和 target**

Create `ZzCore/include/ZzCore/ZzCoreVersion.h` with:

```cpp
#pragma once

#include <QtCore/QString>

#include <ZzCore/ZzCoreExport.h>

namespace ZzCore {

/**
 * @brief 提供 ZzCore 的运行时版本信息。
 */
class ZZ_CORE_EXPORT ZzCoreVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求预先初始化。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzCore
```

Create `ZzCore/src/private/ZzCoreVersion.cpp` with:

```cpp
#include <ZzCore/ZzCoreVersion.h>

namespace ZzCore {

QString ZzCoreVersion::toString()
{
    return QStringLiteral("0.1.0");
}

} // namespace ZzCore
```

Create `ZzCore/CMakeLists.txt` with:

```cmake
set(zz_core_sources
    src/private/ZzCoreVersion.cpp
)

add_library(ZzCore ${zz_core_sources})
add_library(Zz::Core ALIAS ZzCore)

target_link_libraries(ZzCore PUBLIC
    Qt6::Core
    Qt6::Concurrent
)

zz_configure_library_target(ZzCore
    EXPORT_NAME Core
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include"
    EXPORT_HEADER_SUBDIR ZzCore
    EXPORT_HEADER_NAME ZzCoreExport.h
    EXPORT_MACRO_NAME ZZ_CORE_EXPORT
    SOURCES ${zz_core_sources}
)
```

- [ ] **Step 5: 创建 ZzWindowKit 版本 API 和 target**

Create `ZzWindowKit/include/ZzWindowKit/ZzWindowKitVersion.h` with:

```cpp
#pragma once

#include <QtCore/QString>

#include <ZzWindowKit/ZzWindowKitExport.h>

namespace ZzWindowKit {

/**
 * @brief 提供 ZzWindowKit 的运行时版本信息。
 */
class ZZ_WINDOWKIT_EXPORT ZzWindowKitVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求窗口系统初始化。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzWindowKit
```

Create `ZzWindowKit/src/private/ZzWindowKitVersion.cpp` with:

```cpp
#include <ZzWindowKit/ZzWindowKitVersion.h>

namespace ZzWindowKit {

QString ZzWindowKitVersion::toString()
{
    return QStringLiteral("0.1.0");
}

} // namespace ZzWindowKit
```

Create `ZzWindowKit/CMakeLists.txt` with:

```cmake
set(zz_window_kit_sources
    src/private/ZzWindowKitVersion.cpp
)
set(zz_window_kit_moc_headers)

add_library(ZzWindowKit ${zz_window_kit_sources})
add_library(Zz::WindowKit ALIAS ZzWindowKit)

target_link_libraries(ZzWindowKit PUBLIC
    Zz::Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

zz_configure_library_target(ZzWindowKit
    EXPORT_NAME WindowKit
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include"
    EXPORT_HEADER_SUBDIR ZzWindowKit
    EXPORT_HEADER_NAME ZzWindowKitExport.h
    EXPORT_MACRO_NAME ZZ_WINDOWKIT_EXPORT
    SOURCES ${zz_window_kit_sources}
    MOC_HEADERS ${zz_window_kit_moc_headers}
)
```

- [ ] **Step 6: 创建 FluentFoundation 和 FluentUI 版本 API**

Create `ZzFluentUI/foundation/include/ZzFluentUI/ZzFluentVersion.h` with:

```cpp
#pragma once

#include <QtCore/QString>

#include <ZzFluentUI/ZzFluentFoundationExport.h>

namespace ZzFluentUI {

/**
 * @brief 提供 Fluent Foundation 的运行时版本信息。
 */
class ZZ_FLUENT_FOUNDATION_EXPORT ZzFluentVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求主题控制器存在。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/foundation/src/private/ZzFluentVersion.cpp` with:

```cpp
#include <ZzFluentUI/ZzFluentVersion.h>

namespace ZzFluentUI {

QString ZzFluentVersion::toString()
{
    return QStringLiteral("0.1.0");
}

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentWidgetVersion.h` with:

```cpp
#pragma once

#include <QtCore/QString>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

/**
 * @brief 提供 Fluent Widgets 的运行时版本信息。
 */
class ZZ_FLUENT_UI_EXPORT ZzFluentWidgetVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求 QApplication 存在。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzFluentUI
```

Create `ZzFluentUI/widgets/src/private/ZzFluentWidgetVersion.cpp` with:

```cpp
#include <ZzFluentUI/ZzFluentWidgetVersion.h>

namespace ZzFluentUI {

QString ZzFluentWidgetVersion::toString()
{
    return QStringLiteral("0.1.0");
}

} // namespace ZzFluentUI
```

Create `ZzFluentUI/CMakeLists.txt` with:

```cmake
set(zz_fluent_foundation_sources
    foundation/src/private/ZzFluentVersion.cpp
)
set(zz_fluent_foundation_moc_headers)

add_library(ZzFluentFoundation ${zz_fluent_foundation_sources})
add_library(Zz::FluentFoundation ALIAS ZzFluentFoundation)

target_link_libraries(ZzFluentFoundation PUBLIC
    Zz::Core
    Qt6::Core
    Qt6::Gui
)

zz_configure_library_target(ZzFluentFoundation
    EXPORT_NAME FluentFoundation
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/foundation/include"
    EXPORT_HEADER_SUBDIR ZzFluentUI
    EXPORT_HEADER_NAME ZzFluentFoundationExport.h
    EXPORT_MACRO_NAME ZZ_FLUENT_FOUNDATION_EXPORT
    SOURCES ${zz_fluent_foundation_sources}
    MOC_HEADERS ${zz_fluent_foundation_moc_headers}
)

set(zz_fluent_ui_sources
    widgets/src/private/ZzFluentWidgetVersion.cpp
)
set(zz_fluent_ui_moc_headers)

add_library(ZzFluentUI ${zz_fluent_ui_sources})
add_library(Zz::FluentUI ALIAS ZzFluentUI)

target_link_libraries(ZzFluentUI PUBLIC
    Zz::FluentFoundation
    Qt6::Widgets
    Qt6::Svg
)

zz_configure_library_target(ZzFluentUI
    EXPORT_NAME FluentUI
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/widgets/include"
    EXPORT_HEADER_SUBDIR ZzFluentUI
    EXPORT_HEADER_NAME ZzFluentUIExport.h
    EXPORT_MACRO_NAME ZZ_FLUENT_UI_EXPORT
    SOURCES ${zz_fluent_ui_sources}
    MOC_HEADERS ${zz_fluent_ui_moc_headers}
)
```

- [ ] **Step 7: 创建 AppCore 和 PureTools 版本 API**

Create `ZzPureTools/appcore/include/ZzPureTools/ZzAppCoreVersion.h` with:

```cpp
#pragma once

#include <QtCore/QString>

#include <ZzPureTools/ZzAppCoreExport.h>

namespace ZzPureTools {

/**
 * @brief 提供 ZzAppCore 的运行时版本信息。
 */
class ZZ_APP_CORE_EXPORT ZzAppCoreVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求应用运行时存在。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzPureTools
```

Create `ZzPureTools/appcore/src/private/ZzAppCoreVersion.cpp` with:

```cpp
#include <ZzPureTools/ZzAppCoreVersion.h>

namespace ZzPureTools {

QString ZzAppCoreVersion::toString()
{
    return QStringLiteral("0.1.0");
}

} // namespace ZzPureTools
```

Create `ZzPureTools/widgets/include/ZzPureTools/ZzPureToolsVersion.h` with:

```cpp
#pragma once

#include <QtCore/QString>

#include <ZzPureTools/ZzPureToolsExport.h>

namespace ZzPureTools {

/**
 * @brief 提供 ZzPureTools Widgets 的运行时版本信息。
 */
class ZZ_PURE_TOOLS_EXPORT ZzPureToolsVersion final
{
public:
    /**
     * @brief 返回当前组件版本。
     * @return 使用 major.minor.patch 格式的独立字符串值。
     * @note 不涉及对象所有权，可从任意线程调用，不要求 QApplication 存在。
     */
    [[nodiscard]] static QString toString();
};

} // namespace ZzPureTools
```

Create `ZzPureTools/widgets/src/private/ZzPureToolsVersion.cpp` with:

```cpp
#include <ZzPureTools/ZzPureToolsVersion.h>

namespace ZzPureTools {

QString ZzPureToolsVersion::toString()
{
    return QStringLiteral("0.1.0");
}

} // namespace ZzPureTools
```

Create `ZzPureTools/CMakeLists.txt` with:

```cmake
set(zz_app_core_sources
    appcore/src/private/ZzAppCoreVersion.cpp
)
set(zz_app_core_moc_headers)

add_library(ZzAppCore ${zz_app_core_sources})
add_library(Zz::AppCore ALIAS ZzAppCore)

target_link_libraries(ZzAppCore PUBLIC
    Zz::Core
    Qt6::Core
)

zz_configure_library_target(ZzAppCore
    EXPORT_NAME AppCore
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/appcore/include"
    EXPORT_HEADER_SUBDIR ZzPureTools
    EXPORT_HEADER_NAME ZzAppCoreExport.h
    EXPORT_MACRO_NAME ZZ_APP_CORE_EXPORT
    SOURCES ${zz_app_core_sources}
    MOC_HEADERS ${zz_app_core_moc_headers}
)

set(zz_pure_tools_sources
    widgets/src/private/ZzPureToolsVersion.cpp
)
set(zz_pure_tools_moc_headers)

add_library(ZzPureTools ${zz_pure_tools_sources})
add_library(Zz::PureTools ALIAS ZzPureTools)

target_link_libraries(ZzPureTools PUBLIC
    Zz::AppCore
    Zz::WindowKit
    Zz::FluentUI
    Qt6::Widgets
)

zz_configure_library_target(ZzPureTools
    EXPORT_NAME PureTools
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/widgets/include"
    EXPORT_HEADER_SUBDIR ZzPureTools
    EXPORT_HEADER_NAME ZzPureToolsExport.h
    EXPORT_MACRO_NAME ZZ_PURE_TOOLS_EXPORT
    SOURCES ${zz_pure_tools_sources}
    MOC_HEADERS ${zz_pure_tools_moc_headers}
)
```

- [ ] **Step 8: 按架构顺序接入组件**

Create `examples/CMakeLists.txt` with the initial aggregation boundary:

```cmake
# 各组件实施计划按既定顺序在此追加示例子目录。
```

Replace the existing module/test tail of `CMakeLists.txt` with:

```cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ZzFirstPartyTarget)
include(ZzLibraryTarget)

set(ZZLOG_BUILD_SHARED "${BUILD_SHARED_LIBS}"
    CACHE BOOL "Build ZzLog with the repository linkage mode" FORCE)
set(ZZLOG_INSTALL OFF
    CACHE BOOL "Do not install the vendored ZzLog package" FORCE)
set(ZZLOG_BUILD_TESTS "${ZZ_BUILD_TESTS}"
    CACHE BOOL "Build ZzLog tests with the repository test mode" FORCE)
set(ZZLOG_BUILD_EXAMPLES OFF
    CACHE BOOL "Do not build vendored examples" FORCE)
set(ZZLOG_BUILD_WARNINGS OFF
    CACHE BOOL "Do not inject first-party warnings into vendored code" FORCE)

add_subdirectory(ZzThirdParty/ZzLog EXCLUDE_FROM_ALL)
add_subdirectory(ZzCore)
add_subdirectory(ZzWindowKit)
add_subdirectory(ZzFluentUI)
add_subdirectory(ZzPureTools)

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(ZZ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

此阶段不把 `ZzLog` 链入 `ZzCore`：当前 ZzLog 公共 API 仍泄漏 fmt，且静态导出会要求消费者解析未安装的内部 target。下一份 ZzLog 计划必须先完成 C++20 和安装封装，再建立 `PRIVATE` 依赖。

- [ ] **Step 9: 运行六目标绿灯**

Run:

```bash
set -euo pipefail
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R architecture.version-api
cmake --preset linux-clang-tidy
cmake --build --preset linux-clang-tidy
cmake --build --preset linux-clang-tidy --target ZzClangTidy --verbose 2>&1 | tee build/linux-clang-tidy/component-tidy.log
for source in ZzCore/src/private/ZzCoreVersion.cpp ZzWindowKit/src/private/ZzWindowKitVersion.cpp ZzFluentUI/foundation/src/private/ZzFluentVersion.cpp ZzFluentUI/widgets/src/private/ZzFluentWidgetVersion.cpp ZzPureTools/appcore/src/private/ZzAppCoreVersion.cpp ZzPureTools/widgets/src/private/ZzPureToolsVersion.cpp; do
    rg -F "$source" build/linux-clang-tidy/component-tidy.log
done
if rg 'mocs_compilation|qrc_' build/linux-clang-tidy/component-tidy.log; then exit 1; fi
```

Expected: configure/build 返回 0；CTest 报告 `architecture.version-api` 通过；`ZzClangTidy` 处理探针、测试和六个版本实现，但命令中没有 `mocs_compilation.cpp` 或 `qrc_*.cpp`。

- [ ] **Step 10: 提交六个 target**

```bash
git add CMakeLists.txt cmake/ZzLibraryTarget.cmake ZzCore ZzWindowKit ZzFluentUI ZzPureTools examples/CMakeLists.txt tests/Architecture/CMakeLists.txt tests/Architecture/ZzVersionApiTest.h tests/Architecture/ZzVersionApiTest.cpp tests/Architecture/ZzVersionApiTestMain.cpp
git commit -m "构建：建立六个组件导出目标" \
  -m "增加符合 C++20 和中文公共文档约束的最小版本 API，并固定组件依赖方向。" \
  -m "让静态导出宏正确传递到 Windows 消费者。"
```

## Task 4: 建立公共安装头逐文件编译和架构扫描门禁

**Files:**
- Create: `cmake/ZzArchitectureChecks.cmake`
- Create: `tests/Architecture/RunArchitectureChecks.cmake`
- Modify: `tests/Architecture/CMakeLists.txt`

- [ ] **Step 1: 运行公共头和扫描 target 红灯**

Run:

```bash
cmake --build --preset linux-gcc-debug --target ZzPublicHeadersTest
ctest --preset linux-gcc-debug -R architecture.boundaries
```

Expected: 第一条命令 FAIL 并报告未知 target `ZzPublicHeadersTest`；第二条命令因没有匹配测试而 FAIL。两个失败分别证明公共头还没有独立编译，架构规则还没有进入 CTest。

- [ ] **Step 2: 定义每个公共头一个翻译单元的聚合 target**

Create `cmake/ZzArchitectureChecks.cmake` with:

```cmake
include_guard(GLOBAL)

include(ZzCompilerWarnings)

function(zz_add_public_header_probe)
    cmake_parse_arguments(PARSE_ARGV 0 ZZ_HEADER
        "" "OWNER;HEADER" "")
    if(NOT ZZ_HEADER_OWNER OR NOT ZZ_HEADER_HEADER)
        message(FATAL_ERROR
            "zz_add_public_header_probe requires OWNER and HEADER")
    endif()
    if(NOT TARGET ${ZZ_HEADER_OWNER})
        message(FATAL_ERROR
            "public header owner does not exist: ${ZZ_HEADER_OWNER}")
    endif()

    if(NOT TARGET ZzPublicHeadersTest)
        add_custom_target(ZzPublicHeadersTest)
    endif()

    string(MAKE_C_IDENTIFIER
        "${ZZ_HEADER_OWNER}_${ZZ_HEADER_HEADER}" zz_header_id)
    set(zz_probe_target "ZzPublicHeader_${zz_header_id}")
    set(zz_probe_source
        "${CMAKE_CURRENT_BINARY_DIR}/public-headers/${zz_header_id}.cpp")

    file(GENERATE
        OUTPUT "${zz_probe_source}"
        CONTENT "#include <${ZZ_HEADER_HEADER}>\n")
    set_source_files_properties("${zz_probe_source}" PROPERTIES
        GENERATED TRUE)

    add_library(${zz_probe_target} OBJECT "${zz_probe_source}")
    set_target_properties(${zz_probe_target} PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    target_link_libraries(${zz_probe_target} PRIVATE ${ZZ_HEADER_OWNER})
    zz_apply_first_party_warnings(${zz_probe_target}
        SOURCES "${zz_probe_source}")
    add_dependencies(ZzPublicHeadersTest ${zz_probe_target})
endfunction()

function(zz_add_public_header_directory)
    cmake_parse_arguments(PARSE_ARGV 0 ZZ_DIRECTORY
        "" "OWNER;DIRECTORY" "")
    if(NOT ZZ_DIRECTORY_OWNER OR NOT ZZ_DIRECTORY_DIRECTORY)
        message(FATAL_ERROR
            "zz_add_public_header_directory requires OWNER and DIRECTORY")
    endif()
    if(NOT IS_DIRECTORY "${ZZ_DIRECTORY_DIRECTORY}")
        message(FATAL_ERROR
            "public header directory does not exist: ${ZZ_DIRECTORY_DIRECTORY}")
    endif()

    file(GLOB_RECURSE zz_public_headers
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES FALSE
        "${ZZ_DIRECTORY_DIRECTORY}/*.h"
    )
    if(NOT zz_public_headers)
        message(FATAL_ERROR
            "public header directory is empty: ${ZZ_DIRECTORY_DIRECTORY}")
    endif()

    foreach(zz_public_header IN LISTS zz_public_headers)
        file(RELATIVE_PATH zz_public_include
            "${ZZ_DIRECTORY_DIRECTORY}" "${zz_public_header}")
        zz_add_public_header_probe(
            OWNER ${ZZ_DIRECTORY_OWNER}
            HEADER "${zz_public_include}"
        )
    endforeach()
endfunction()
```

每个 object library 只有一个 `.cpp`，且只链接该头所属的 target；因此某个 Core 头不能借用 FluentUI 或 Widgets 的 include/link 环境掩盖非法依赖。`ZzPublicHeadersTest` 只是这些独立编译 target 的聚合入口。

- [ ] **Step 3: 实现架构扫描脚本**

Create `tests/Architecture/RunArchitectureChecks.cmake` with:

```cmake
cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR)
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
if(NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "source directory does not exist: ${ZZ_SOURCE_DIR}")
endif()
file(TO_CMAKE_PATH "${ZZ_SOURCE_DIR}" zz_source_root_normalized)

set(zz_public_roots
    "${ZZ_SOURCE_DIR}/ZzCore/include"
    "${ZZ_SOURCE_DIR}/ZzWindowKit/include"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/foundation/include"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets/include"
    "${ZZ_SOURCE_DIR}/ZzPureTools/appcore/include"
    "${ZZ_SOURCE_DIR}/ZzPureTools/widgets/include"
)

foreach(zz_public_root IN LISTS zz_public_roots)
    file(GLOB_RECURSE zz_public_headers
        LIST_DIRECTORIES FALSE
        "${zz_public_root}/*.h"
    )
    foreach(zz_public_header IN LISTS zz_public_headers)
        file(READ "${zz_public_header}" zz_public_content)

        if(zz_public_content MATCHES
           "#[ \\t]*include[ \\t]*[<\"][^>\"]*(QWK|qwindowkit|Qt[^>\"]*/private|spdlog|fmt/)")
            message(FATAL_ERROR
                "forbidden dependency leaked into public header: ${zz_public_header}")
        endif()
        if(zz_public_content MATCHES
           "#[ \\t]*include[ \\t]*[<\"][^>\"]*\\.\\./")
            message(FATAL_ERROR
                "relative parent include leaked into public header: ${zz_public_header}")
        endif()

        set(zz_type_content "${zz_public_content}")
        string(REGEX REPLACE
            "ZZ_[A-Z0-9_]+_EXPORT[ \\t]+" "" zz_type_content "${zz_type_content}")
        # class/struct/enum 只匹配带定义体的类型；Concept 使用等号定义。
        # PIMPL、Qt 和关联值类型的前置声明不属于主类型。
        string(REGEX MATCHALL
            "(class|struct|enum[ \\t]+class)[ \\t\\r\\n]+(\\[\\[[^]]*\\]\\][ \\t\\r\\n]*)*[A-Za-z_][A-Za-z0-9_]*[^;{]*\\{"
            zz_type_definitions "${zz_type_content}")
        string(REGEX MATCHALL
            "concept[ \\t]+[A-Za-z_][A-Za-z0-9_]*[ \\t\\r\\n]*="
            zz_concept_definitions "${zz_type_content}")
        list(APPEND zz_type_definitions ${zz_concept_definitions})

        get_filename_component(zz_header_stem "${zz_public_header}" NAME_WE)
        set(zz_has_primary_type FALSE)
        foreach(zz_definition IN LISTS zz_type_definitions)
            string(REGEX MATCH
                "(class|struct|concept|enum[ \\t]+class)[ \\t\\r\\n]+(\\[\\[[^]]*\\]\\][ \\t\\r\\n]*)*[A-Za-z_][A-Za-z0-9_]*"
                zz_type_prefix "${zz_definition}")
            string(REGEX MATCH
                "[A-Za-z_][A-Za-z0-9_]*$" zz_type_name "${zz_type_prefix}")
            if(NOT zz_type_name MATCHES "^Zz")
                message(FATAL_ERROR
                    "public type lacks Zz prefix in ${zz_public_header}: ${zz_type_name}")
            endif()
            if(zz_type_name STREQUAL zz_header_stem)
                set(zz_has_primary_type TRUE)
            endif()
        endforeach()
        if(zz_type_definitions AND NOT zz_has_primary_type)
            message(FATAL_ERROR
                "public header has no primary type matching its file name: ${zz_public_header}")
        endif()

        if(zz_type_definitions
           AND (NOT zz_public_content MATCHES "/\\*\\*"
                OR NOT zz_public_content MATCHES "@brief"))
            message(FATAL_ERROR
                "public declaration lacks Chinese Doxygen structure: ${zz_public_header}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE zz_first_party_files
    LIST_DIRECTORIES FALSE
    "${ZZ_SOURCE_DIR}/ZzCore/*.h"
    "${ZZ_SOURCE_DIR}/ZzCore/*.cpp"
    "${ZZ_SOURCE_DIR}/ZzWindowKit/*.h"
    "${ZZ_SOURCE_DIR}/ZzWindowKit/*.cpp"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/*.h"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/*.cpp"
    "${ZZ_SOURCE_DIR}/ZzPureTools/*.h"
    "${ZZ_SOURCE_DIR}/ZzPureTools/*.cpp"
    "${ZZ_SOURCE_DIR}/tests/Architecture/*.h"
    "${ZZ_SOURCE_DIR}/tests/Architecture/*.cpp"
)

foreach(zz_source IN LISTS zz_first_party_files)
    file(READ "${zz_source}" zz_source_content)
    file(TO_CMAKE_PATH "${zz_source}" zz_source_normalized)

    if(zz_source_content MATCHES
       "namespace[ \\t\\r\\n]+[A-Za-z_][A-Za-z0-9_]*[ \\t]*::")
        message(FATAL_ERROR
            "chained namespace declaration is forbidden: ${zz_source}")
    endif()
    if(zz_source_content MATCHES
       "#[ \\t]*include[ \\t]*[<\"][^>\"]*(Qt[^>\"]*/private|Qt[^>\"]*Private)")
        message(FATAL_ERROR "Qt Private include is forbidden: ${zz_source}")
    endif()
    if(zz_source_content MATCHES
       "#[ \\t]*include[ \\t]*[<\"][^>\"]*(QWK|qwindowkit)")
        string(FIND "${zz_source_normalized}"
            "${zz_source_root_normalized}/ZzWindowKit/src/private/"
            zz_window_private_pos)
        if(NOT zz_window_private_pos EQUAL 0)
            message(FATAL_ERROR
                "QWindowKit include escaped ZzWindowKit private: ${zz_source}")
        endif()
    endif()
endforeach()

file(GLOB_RECURSE zz_core_files
    LIST_DIRECTORIES FALSE
    "${ZZ_SOURCE_DIR}/ZzCore/*.h"
    "${ZZ_SOURCE_DIR}/ZzCore/*.cpp"
)
foreach(zz_core_file IN LISTS zz_core_files)
    file(READ "${zz_core_file}" zz_core_content)
    if(zz_core_content MATCHES
       "#[ \\t]*include[ \\t]*[<\"]Qt(Gui|Widgets|Quick)")
        message(FATAL_ERROR
            "ZzCore source includes a forbidden Qt UI module: ${zz_core_file}")
    endif()
endforeach()

file(READ "${ZZ_SOURCE_DIR}/ZzCore/CMakeLists.txt" zz_core_cmake)
if(zz_core_cmake MATCHES "Qt6::(Gui|Widgets|Quick)")
    message(FATAL_ERROR "ZzCore links a forbidden Qt UI target")
endif()

set(zz_ui_roots
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets"
    "${ZZ_SOURCE_DIR}/ZzPureTools/widgets"
)
foreach(zz_ui_root IN LISTS zz_ui_roots)
    file(GLOB_RECURSE zz_ui_files
        LIST_DIRECTORIES FALSE
        "${zz_ui_root}/*.h"
        "${zz_ui_root}/*.cpp"
    )
    foreach(zz_ui_file IN LISTS zz_ui_files)
        file(READ "${zz_ui_file}" zz_ui_content)
        if(zz_ui_content MATCHES
           "#[ \\t]*include[ \\t]*[<\"][^>\"]*(Repository|Database|NetworkClient|DomainEntity)")
            message(FATAL_ERROR
                "UI source includes a forbidden business/storage type: ${zz_ui_file}")
        endif()
    endforeach()
endforeach()

message(STATUS "Zz architecture boundary scan passed")
```

这个文本门禁覆盖当前阶段可以可靠判定的规则。类型规则对 class/struct/enum 只检查带 `{` 的定义，对 Concept 检查 `concept ZzName =`，明确忽略 `class ZzFooPrivate;`、Qt 类型和同组件关联类型的前置声明；同一公共头允许定义多个以 `Zz` 开头的紧密关联类型，但必须至少有一个主类型与文件 stem 完全一致。出现误报时先缩小正则并保留规则。需要语义判断的规则在后续计划增加结构化扫描，不能直接删除门禁。

- [ ] **Step 4: 注册源头和生成导出头的逐文件编译**

Append this exact block to `tests/Architecture/CMakeLists.txt`:

```cmake
include(ZzArchitectureChecks)

zz_add_public_header_directory(
    OWNER Zz::Core
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzCore/include"
)
zz_add_public_header_directory(
    OWNER Zz::WindowKit
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzWindowKit/include"
)
zz_add_public_header_directory(
    OWNER Zz::FluentFoundation
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzFluentUI/foundation/include"
)
zz_add_public_header_directory(
    OWNER Zz::FluentUI
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzFluentUI/widgets/include"
)
zz_add_public_header_directory(
    OWNER Zz::AppCore
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzPureTools/appcore/include"
)
zz_add_public_header_directory(
    OWNER Zz::PureTools
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/include"
)

zz_add_public_header_probe(
    OWNER Zz::Core
    HEADER ZzCore/ZzCoreExport.h
)
zz_add_public_header_probe(
    OWNER Zz::WindowKit
    HEADER ZzWindowKit/ZzWindowKitExport.h
)
zz_add_public_header_probe(
    OWNER Zz::FluentFoundation
    HEADER ZzFluentUI/ZzFluentFoundationExport.h
)
zz_add_public_header_probe(
    OWNER Zz::FluentUI
    HEADER ZzFluentUI/ZzFluentUIExport.h
)
zz_add_public_header_probe(
    OWNER Zz::AppCore
    HEADER ZzPureTools/ZzAppCoreExport.h
)
zz_add_public_header_probe(
    OWNER Zz::PureTools
    HEADER ZzPureTools/ZzPureToolsExport.h
)

add_test(
    NAME architecture.public-headers
    COMMAND "${CMAKE_COMMAND}"
        --build "${CMAKE_BINARY_DIR}"
        --target ZzPublicHeadersTest
        --config "$<CONFIG>"
)
set_tests_properties(architecture.public-headers PROPERTIES
    LABELS "architecture;headers"
    RUN_SERIAL TRUE
)

add_test(
    NAME architecture.boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/RunArchitectureChecks.cmake"
)
set_tests_properties(architecture.boundaries PROPERTIES
    LABELS "architecture"
)
```

- [ ] **Step 5: 运行公共头和架构绿灯**

Run:

```bash
set -euo pipefail
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ZzPublicHeadersTest
ctest --preset linux-gcc-debug -L architecture
```

Expected: `ZzPublicHeadersTest` 分别编译六个版本头和六个生成导出头，共十二个独立翻译单元；CTest 中版本、生成代码、公共头和 `architecture.boundaries` 全部通过。

- [ ] **Step 6: 提交架构门禁**

```bash
git add cmake/ZzArchitectureChecks.cmake tests/Architecture/CMakeLists.txt tests/Architecture/RunArchitectureChecks.cmake
git commit -m "测试：建立公共头与架构门禁" \
  -m "让每个可安装公共头在所属 target 的最小依赖环境中以 C++20 独立编译。" \
  -m "扫描 QWindowKit、Qt Private、UI 反向依赖、链式命名空间和公开 API 规范。"
```

## Task 5: 实现 shared/static/Windows 完整安装包

**Files:**
- Create: `cmake/ZzInstallPackage.cmake`
- Create: `cmake/ZzPureToolsProConfig.cmake.in`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 运行缺少 Config Package 的红灯**

Run:

```bash
cmake --build --preset linux-gcc-debug
cmake --install build/linux-gcc-debug --prefix build/linux-gcc-debug/red-install
test -f build/linux-gcc-debug/red-install/lib/cmake/ZzPureToolsPro/ZzPureToolsProConfig.cmake
```

Expected: build 和空安装命令可以返回 0，但最后的 `test` 必须 FAIL，因为当前没有 ZzPureToolsPro 安装规则和 Config 文件。红灯原因是包缺失，不是现有工程配置失败。

- [ ] **Step 2: 写入包配置模板**

Create `cmake/ZzPureToolsProConfig.cmake.in` with:

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Svg
    Concurrent
)

include("${CMAKE_CURRENT_LIST_DIR}/ZzPureToolsProTargets.cmake")

check_required_components(ZzPureToolsPro)
```

模板只记录逻辑依赖，不写 Qt SDK 路径。Qt minor 精确匹配将在 QWindowKit 私有后端进入二进制包的计划中增加；当前总体约束仍是 Qt 6.8 或更高版本。

- [ ] **Step 3: 实现完整 `zz_install_package()`**

Create `cmake/ZzInstallPackage.cmake` with:

```cmake
include_guard(GLOBAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

function(zz_install_package)
    set(zz_export_name ZzPureToolsProTargets)
    set(zz_package_cmake_dir
        "${CMAKE_INSTALL_LIBDIR}/cmake/ZzPureToolsPro")
    set(zz_targets
        ZzCore
        ZzWindowKit
        ZzFluentFoundation
        ZzFluentUI
        ZzAppCore
        ZzPureTools
    )

    foreach(zz_target IN LISTS zz_targets)
        if(NOT TARGET ${zz_target})
            message(FATAL_ERROR
                "zz_install_package requires target ${zz_target}")
        endif()
    endforeach()

    install(TARGETS ${zz_targets}
        EXPORT ${zz_export_name}
        RUNTIME
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT Runtime
        LIBRARY
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT Runtime
            NAMELINK_COMPONENT Development
        ARCHIVE
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT Development
        INCLUDES
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    set(zz_public_include_roots
        "${PROJECT_SOURCE_DIR}/ZzCore/include"
        "${PROJECT_SOURCE_DIR}/ZzWindowKit/include"
        "${PROJECT_SOURCE_DIR}/ZzFluentUI/foundation/include"
        "${PROJECT_SOURCE_DIR}/ZzFluentUI/widgets/include"
        "${PROJECT_SOURCE_DIR}/ZzPureTools/appcore/include"
        "${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/include"
    )
    foreach(zz_include_root IN LISTS zz_public_include_roots)
        install(DIRECTORY "${zz_include_root}/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            COMPONENT Development
            FILES_MATCHING PATTERN "*.h"
        )
    endforeach()

    foreach(zz_target IN LISTS zz_targets)
        get_target_property(zz_generated_header
            ${zz_target} ZZ_GENERATED_EXPORT_HEADER)
        get_target_property(zz_generated_subdir
            ${zz_target} ZZ_EXPORT_HEADER_INSTALL_SUBDIR)
        if(NOT zz_generated_header
           OR zz_generated_header MATCHES "-NOTFOUND$")
            message(FATAL_ERROR
                "${zz_target} has no generated export header property")
        endif()
        if(NOT zz_generated_subdir
           OR zz_generated_subdir MATCHES "-NOTFOUND$")
            message(FATAL_ERROR
                "${zz_target} has no export header install subdirectory")
        endif()

        install(FILES "${zz_generated_header}"
            DESTINATION
                "${CMAKE_INSTALL_INCLUDEDIR}/${zz_generated_subdir}"
            COMPONENT Development
        )
    endforeach()

    configure_package_config_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ZzPureToolsProConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfig.cmake"
        INSTALL_DESTINATION "${zz_package_cmake_dir}"
    )
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
    )

    install(EXPORT ${zz_export_name}
        FILE ZzPureToolsProTargets.cmake
        NAMESPACE Zz::
        DESTINATION "${zz_package_cmake_dir}"
        COMPONENT Development
    )
    install(FILES
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfig.cmake"
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfigVersion.cmake"
        DESTINATION "${zz_package_cmake_dir}"
        COMPONENT Development
    )

    install(FILES
        "${PROJECT_SOURCE_DIR}/docs/superpowers/specs/2026-08-02-zzpuretoolspro-architecture-design.md"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/ZzPureToolsPro"
        COMPONENT Development
    )

    if(EXISTS "${PROJECT_SOURCE_DIR}/LICENSE")
        install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/ZzPureToolsPro"
            COMPONENT Runtime
        )
    else()
        message(STATUS
            "ZzPureToolsPro LICENSE is absent; binary publication remains blocked")
    endif()
endfunction()
```

`RUNTIME` 覆盖 Windows DLL，`ARCHIVE` 同时覆盖 Windows import library 和静态库，`LIBRARY` 覆盖 Linux/macOS 共享库；因此不得删减任何 artifact destination。源公共头和六个 `GenerateExportHeader` 产物分别安装，`install(EXPORT ... NAMESPACE Zz::)` 生成 shared/static 对应的 imported target。

- [ ] **Step 4: 从根工程调用安装 helper**

Append to the end of `CMakeLists.txt`:

```cmake
include(ZzInstallPackage)
zz_install_package()
```

- [ ] **Step 5: 运行安装包绿灯并检查内容**

Run:

```bash
set -euo pipefail
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --install build/linux-gcc-debug --prefix install/linux-gcc-debug
test -f install/linux-gcc-debug/lib/cmake/ZzPureToolsPro/ZzPureToolsProConfig.cmake
test -f install/linux-gcc-debug/lib/cmake/ZzPureToolsPro/ZzPureToolsProConfigVersion.cmake
test -f install/linux-gcc-debug/lib/cmake/ZzPureToolsPro/ZzPureToolsProTargets.cmake
test -f install/linux-gcc-debug/include/ZzCore/ZzCoreVersion.h
test -f install/linux-gcc-debug/include/ZzCore/ZzCoreExport.h
test -f install/linux-gcc-debug/include/ZzWindowKit/ZzWindowKitExport.h
test -f install/linux-gcc-debug/include/ZzFluentUI/ZzFluentFoundationExport.h
test -f install/linux-gcc-debug/include/ZzFluentUI/ZzFluentUIExport.h
test -f install/linux-gcc-debug/include/ZzPureTools/ZzAppCoreExport.h
test -f install/linux-gcc-debug/include/ZzPureTools/ZzPureToolsExport.h
if rg -n -F "$PWD" install/linux-gcc-debug/lib/cmake/ZzPureToolsPro; then exit 1; fi
if rg -n -F "$QT_ROOT" install/linux-gcc-debug/lib/cmake/ZzPureToolsPro; then exit 1; fi
```

Expected: 所有 `test -f` 返回 0；安装树包含共享库、公开头、六个生成导出头和三个 package 文件；两个绝对路径负向扫描都无匹配。`@PACKAGE_INIT@` 生成的 `_IMPORT_PREFIX` 相对计算允许存在。

- [ ] **Step 6: 提交安装实现**

```bash
git add CMakeLists.txt cmake/ZzInstallPackage.cmake cmake/ZzPureToolsProConfig.cmake.in
git commit -m "构建：实现可重定位安装包" \
  -m "完整安装六个导出 target 的产物、源公共头与生成导出头。" \
  -m "生成使用 Zz 命名空间的 Config 和 SameMinorVersion 版本文件。"
```

## Task 6: 从全新 A/B/consumer 树验证安装消费

**Files:**
- Create: `tests/InstallConsumer/CMakeLists.txt`
- Create: `tests/InstallConsumer/main.cpp`
- Create: `tests/InstallConsumer/InstallConsumerContext.cmake.in`
- Create: `tests/InstallConsumer/RunInstallConsumer.cmake`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 运行未注册 install consumer 的红灯**

Run:

```bash
ctest --preset linux-gcc-debug -R '^install.consumer$'
```

Expected: FAIL，并因 `noTestsAction=error` 报告没有名为 `install.consumer` 的测试；失败原因是 CTest 尚未注册外部消费流程。

- [ ] **Step 2: 创建真正的外部消费者**

Create `tests/InstallConsumer/main.cpp` with:

```cpp
#include <ZzCore/ZzCoreVersion.h>
#include <ZzFluentUI/ZzFluentVersion.h>
#include <ZzFluentUI/ZzFluentWidgetVersion.h>
#include <ZzPureTools/ZzAppCoreVersion.h>
#include <ZzPureTools/ZzPureToolsVersion.h>
#include <ZzWindowKit/ZzWindowKitVersion.h>

int main()
{
    if (ZzCore::ZzCoreVersion::toString() != QStringLiteral("0.1.0")) {
        return 1;
    }
    if (ZzWindowKit::ZzWindowKitVersion::toString() != QStringLiteral("0.1.0")) {
        return 2;
    }
    if (ZzFluentUI::ZzFluentVersion::toString() != QStringLiteral("0.1.0")) {
        return 3;
    }
    if (ZzFluentUI::ZzFluentWidgetVersion::toString() != QStringLiteral("0.1.0")) {
        return 4;
    }
    if (ZzPureTools::ZzAppCoreVersion::toString() != QStringLiteral("0.1.0")) {
        return 5;
    }
    if (ZzPureTools::ZzPureToolsVersion::toString() != QStringLiteral("0.1.0")) {
        return 6;
    }
    return 0;
}
```

Create `tests/InstallConsumer/CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.23)

project(ZzInstallConsumer LANGUAGES CXX)

include(GNUInstallDirs)
enable_testing()

find_package(ZzPureToolsPro 0.1 CONFIG REQUIRED)

set(zz_installed_header_specs
    "Zz::Core|ZzCore/ZzCoreVersion.h"
    "Zz::Core|ZzCore/ZzCoreExport.h"
    "Zz::WindowKit|ZzWindowKit/ZzWindowKitVersion.h"
    "Zz::WindowKit|ZzWindowKit/ZzWindowKitExport.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzFluentVersion.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzFluentFoundationExport.h"
    "Zz::FluentUI|ZzFluentUI/ZzFluentWidgetVersion.h"
    "Zz::FluentUI|ZzFluentUI/ZzFluentUIExport.h"
    "Zz::AppCore|ZzPureTools/ZzAppCoreVersion.h"
    "Zz::AppCore|ZzPureTools/ZzAppCoreExport.h"
    "Zz::PureTools|ZzPureTools/ZzPureToolsVersion.h"
    "Zz::PureTools|ZzPureTools/ZzPureToolsExport.h"
)

add_custom_target(ZzInstalledPublicHeadersTest ALL)
foreach(zz_header_spec IN LISTS zz_installed_header_specs)
    string(REPLACE "|" ";" zz_header_parts "${zz_header_spec}")
    list(GET zz_header_parts 0 zz_header_owner)
    list(GET zz_header_parts 1 zz_header_include)
    string(MAKE_C_IDENTIFIER
        "${zz_header_owner}_${zz_header_include}" zz_header_id)
    set(zz_header_target "ZzInstalledHeader_${zz_header_id}")
    set(zz_header_source
        "${CMAKE_CURRENT_BINARY_DIR}/public-headers/${zz_header_id}.cpp")

    file(GENERATE
        OUTPUT "${zz_header_source}"
        CONTENT "#include <${zz_header_include}>\n")
    set_source_files_properties("${zz_header_source}" PROPERTIES
        GENERATED TRUE)
    add_library(${zz_header_target} OBJECT "${zz_header_source}")
    target_compile_features(${zz_header_target} PRIVATE cxx_std_20)
    set_target_properties(${zz_header_target} PROPERTIES
        CXX_EXTENSIONS OFF)
    target_link_libraries(${zz_header_target} PRIVATE ${zz_header_owner})
    add_dependencies(ZzInstalledPublicHeadersTest ${zz_header_target})
endforeach()

add_executable(ZzInstallConsumer main.cpp)
target_compile_features(ZzInstallConsumer PRIVATE cxx_std_20)
set_target_properties(ZzInstallConsumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(ZzInstallConsumer PRIVATE
    Zz::Core
    Zz::WindowKit
    Zz::FluentFoundation
    Zz::FluentUI
    Zz::AppCore
    Zz::PureTools
)

add_test(NAME zz.install-consumer.run COMMAND ZzInstallConsumer)
if(WIN32)
    if(NOT DEFINED ZZ_PACKAGE_ROOT)
        message(FATAL_ERROR
            "ZZ_PACKAGE_ROOT is required to locate installed DLL files")
    endif()
    set_tests_properties(zz.install-consumer.run PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${ZZ_PACKAGE_ROOT}/${CMAKE_INSTALL_BINDIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
```

消费者再次为十二个已安装头生成十二个独立翻译单元，因此 build-tree 的 `ZzPublicHeadersTest` 和 install-tree 的 `ZzInstalledPublicHeadersTest` 分别验证源接口与发布接口。

- [ ] **Step 3: 生成不丢失列表值的工具链上下文**

Create `tests/InstallConsumer/InstallConsumerContext.cmake.in` with:

```cmake
set(ZZ_GENERATOR [==[@CMAKE_GENERATOR@]==])
set(ZZ_GENERATOR_PLATFORM [==[@CMAKE_GENERATOR_PLATFORM@]==])
set(ZZ_GENERATOR_TOOLSET [==[@CMAKE_GENERATOR_TOOLSET@]==])
set(ZZ_CXX_COMPILER [==[@CMAKE_CXX_COMPILER@]==])
set(ZZ_BUILD_TYPE [==[@CMAKE_BUILD_TYPE@]==])
set(ZZ_CONFIGURATION_TYPES [==[@CMAKE_CONFIGURATION_TYPES@]==])
set(ZZ_CMAKE_PREFIX_PATH [==[@CMAKE_PREFIX_PATH@]==])
set(ZZ_OSX_ARCHITECTURES [==[@CMAKE_OSX_ARCHITECTURES@]==])
set(ZZ_OSX_DEPLOYMENT_TARGET [==[@CMAKE_OSX_DEPLOYMENT_TARGET@]==])
set(ZZ_OSX_SYSROOT [==[@CMAKE_OSX_SYSROOT@]==])
set(ZZ_PRIMARY_BINARY_DIR [==[@CMAKE_BINARY_DIR@]==])
set(ZZ_CTEST_COMMAND [==[@CMAKE_CTEST_COMMAND@]==])
set(ZZ_BUILD_SHARED [==[@BUILD_SHARED_LIBS@]==])
set(ZZ_ENABLE_LTO [==[@ZZ_ENABLE_LTO@]==])
```

使用配置文件而不是把所有值直接拼进 `add_test()`，是为了保留 `CMAKE_PREFIX_PATH` 和 macOS universal architecture 中的分号列表。Qt 前缀随后同时传给全新 producer 和 consumer，不能被安装前缀覆盖。

- [ ] **Step 4: 实现全新 A/B/consumer 驱动**

Create `tests/InstallConsumer/RunInstallConsumer.cmake` with:

```cmake
cmake_minimum_required(VERSION 3.23)

foreach(zz_required IN ITEMS
    ZZ_SOURCE_DIR
    ZZ_TEST_ROOT
    ZZ_CONTEXT_FILE
    ZZ_CONFIG
)
    if(NOT DEFINED ${zz_required})
        message(FATAL_ERROR "${zz_required} is required")
    endif()
endforeach()
if(NOT EXISTS "${ZZ_CONTEXT_FILE}")
    message(FATAL_ERROR
        "install consumer context does not exist: ${ZZ_CONTEXT_FILE}")
endif()

include("${ZZ_CONTEXT_FILE}")

if(NOT ZZ_PRIMARY_BINARY_DIR)
    message(FATAL_ERROR "primary binary directory was not captured")
endif()
file(TO_CMAKE_PATH "${ZZ_TEST_ROOT}" zz_test_root_normalized)
file(TO_CMAKE_PATH "${ZZ_PRIMARY_BINARY_DIR}" zz_primary_binary_normalized)
string(FIND "${zz_test_root_normalized}"
    "${zz_primary_binary_normalized}/" zz_test_root_prefix)
if(ZZ_TEST_ROOT STREQUAL ""
   OR ZZ_TEST_ROOT STREQUAL "/"
   OR ZZ_TEST_ROOT STREQUAL ZZ_SOURCE_DIR
   OR NOT zz_test_root_prefix EQUAL 0)
    message(FATAL_ERROR "unsafe install consumer test root: ${ZZ_TEST_ROOT}")
endif()
if(NOT ZZ_GENERATOR)
    message(FATAL_ERROR "generator was not captured from the parent build")
endif()
if(NOT ZZ_CXX_COMPILER)
    message(FATAL_ERROR "C++ compiler was not captured from the parent build")
endif()
if(NOT ZZ_CTEST_COMMAND)
    message(FATAL_ERROR "CTest executable was not captured from the parent build")
endif()
if(NOT ZZ_CONFIGURATION_TYPES STREQUAL "" AND ZZ_CONFIG STREQUAL "")
    message(FATAL_ERROR
        "a multi-config generator requires a concrete test configuration")
endif()

set(zz_a_dir "${ZZ_TEST_ROOT}/A")
set(zz_b_dir "${ZZ_TEST_ROOT}/B")
set(zz_consumer_dir "${ZZ_TEST_ROOT}/consumer")
file(REMOVE_RECURSE
    "${zz_a_dir}"
    "${zz_b_dir}"
    "${zz_consumer_dir}"
)
file(MAKE_DIRECTORY "${ZZ_TEST_ROOT}")

function(zz_run_process zz_step)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE zz_result
        OUTPUT_VARIABLE zz_stdout
        ERROR_VARIABLE zz_stderr
    )
    if(NOT zz_result EQUAL 0)
        message(FATAL_ERROR
            "${zz_step} failed with exit code ${zz_result}\n"
            "stdout:\n${zz_stdout}\n"
            "stderr:\n${zz_stderr}")
    endif()
    message(STATUS "${zz_step} passed")
endfunction()

set(zz_generator_args -G "${ZZ_GENERATOR}")
if(NOT ZZ_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND zz_generator_args -A "${ZZ_GENERATOR_PLATFORM}")
endif()
if(NOT ZZ_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND zz_generator_args -T "${ZZ_GENERATOR_TOOLSET}")
endif()

set(zz_common_cache_args
    "-DCMAKE_CXX_COMPILER:FILEPATH=${ZZ_CXX_COMPILER}"
)
if(NOT ZZ_BUILD_TYPE STREQUAL "")
    list(APPEND zz_common_cache_args
        "-DCMAKE_BUILD_TYPE:STRING=${ZZ_BUILD_TYPE}")
endif()
if(NOT ZZ_CMAKE_PREFIX_PATH STREQUAL "")
    string(REPLACE ";" "\\;" zz_prefix_path_escaped
        "${ZZ_CMAKE_PREFIX_PATH}")
    list(APPEND zz_common_cache_args
        "-DCMAKE_PREFIX_PATH:STRING=${zz_prefix_path_escaped}")
endif()
if(NOT ZZ_OSX_ARCHITECTURES STREQUAL "")
    string(REPLACE ";" "\\;" zz_osx_architectures_escaped
        "${ZZ_OSX_ARCHITECTURES}")
    list(APPEND zz_common_cache_args
        "-DCMAKE_OSX_ARCHITECTURES:STRING=${zz_osx_architectures_escaped}")
endif()
if(NOT ZZ_OSX_DEPLOYMENT_TARGET STREQUAL "")
    list(APPEND zz_common_cache_args
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${ZZ_OSX_DEPLOYMENT_TARGET}")
endif()
if(NOT ZZ_OSX_SYSROOT STREQUAL "")
    list(APPEND zz_common_cache_args
        "-DCMAKE_OSX_SYSROOT:PATH=${ZZ_OSX_SYSROOT}")
endif()

set(zz_config_args)
if(NOT ZZ_CONFIG STREQUAL "")
    list(APPEND zz_config_args --config "${ZZ_CONFIG}")
endif()

zz_run_process("fresh producer configure"
    "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}"
    -B "${zz_a_dir}"
    ${zz_generator_args}
    ${zz_common_cache_args}
    "-DBUILD_SHARED_LIBS:BOOL=${ZZ_BUILD_SHARED}"
    "-DZZ_BUILD_TESTS:BOOL=OFF"
    "-DZZ_BUILD_EXAMPLES:BOOL=OFF"
    "-DZZ_BUILD_BENCHMARKS:BOOL=OFF"
    "-DZZ_WARNINGS_AS_ERRORS:BOOL=ON"
    "-DZZ_ENABLE_CLANG_TIDY:BOOL=OFF"
    "-DZZ_ENABLE_ASAN:BOOL=OFF"
    "-DZZ_ENABLE_UBSAN:BOOL=OFF"
    "-DZZ_ENABLE_LTO:BOOL=${ZZ_ENABLE_LTO}"
)

zz_run_process("fresh producer build"
    "${CMAKE_COMMAND}" --build "${zz_a_dir}" ${zz_config_args})

zz_run_process("fresh producer install"
    "${CMAKE_COMMAND}" --install "${zz_a_dir}"
    --prefix "${zz_b_dir}" ${zz_config_args})

file(GLOB_RECURSE zz_installed_configs
    LIST_DIRECTORIES FALSE
    "${zz_b_dir}/ZzPureToolsProConfig.cmake"
)
list(LENGTH zz_installed_configs zz_config_count)
if(NOT zz_config_count EQUAL 1)
    message(FATAL_ERROR
        "expected one installed Config file in B, found ${zz_config_count}")
endif()

set(zz_consumer_prefix_path "${zz_b_dir}")
if(NOT ZZ_CMAKE_PREFIX_PATH STREQUAL "")
    list(APPEND zz_consumer_prefix_path ${ZZ_CMAKE_PREFIX_PATH})
endif()
string(REPLACE ";" "\\;" zz_consumer_prefix_escaped
    "${zz_consumer_prefix_path}")

set(zz_consumer_cache_args ${zz_common_cache_args})
list(FILTER zz_consumer_cache_args EXCLUDE
    REGEX "^-DCMAKE_PREFIX_PATH:")
list(APPEND zz_consumer_cache_args
    "-DCMAKE_PREFIX_PATH:STRING=${zz_consumer_prefix_escaped}"
    "-DZZ_PACKAGE_ROOT:PATH=${zz_b_dir}"
)

zz_run_process("fresh consumer configure"
    "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}/tests/InstallConsumer"
    -B "${zz_consumer_dir}"
    ${zz_generator_args}
    ${zz_consumer_cache_args}
)

zz_run_process("fresh consumer build"
    "${CMAKE_COMMAND}" --build "${zz_consumer_dir}" ${zz_config_args})

zz_run_process("fresh consumer test"
    "${ZZ_CTEST_COMMAND}"
    --test-dir "${zz_consumer_dir}"
    ${zz_config_args}
    --output-on-failure
)

file(GLOB_RECURSE zz_installed_cmake_files
    LIST_DIRECTORIES FALSE
    "${zz_b_dir}/*.cmake"
)
set(zz_forbidden_paths
    "${ZZ_SOURCE_DIR}"
    "${ZZ_PRIMARY_BINARY_DIR}"
    "${zz_a_dir}"
)
if(NOT ZZ_CMAKE_PREFIX_PATH STREQUAL "")
    list(APPEND zz_forbidden_paths ${ZZ_CMAKE_PREFIX_PATH})
endif()

foreach(zz_cmake_file IN LISTS zz_installed_cmake_files)
    file(READ "${zz_cmake_file}" zz_cmake_content)
    foreach(zz_forbidden_path IN LISTS zz_forbidden_paths)
        if(zz_forbidden_path STREQUAL "")
            continue()
        endif()
        file(TO_CMAKE_PATH "${zz_forbidden_path}" zz_forbidden_normalized)
        string(FIND "${zz_cmake_content}"
            "${zz_forbidden_normalized}" zz_forbidden_position)
        if(NOT zz_forbidden_position EQUAL -1)
            message(FATAL_ERROR
                "installed CMake file leaks ${zz_forbidden_normalized}: ${zz_cmake_file}")
        endif()
    endforeach()
endforeach()

message(STATUS
    "fresh A/B/consumer install test passed for BUILD_SHARED_LIBS=${ZZ_BUILD_SHARED}")
```

脚本只删除 `${ZZ_TEST_ROOT}` 下明确的 `A`、`B`、`consumer` 三棵测试树。`A` 从源码重新 configure/build，`B` 只接收 `cmake --install A --prefix B` 的结果，`consumer` 只通过 `B` 和原 Qt prefix 执行 `find_package()`；主构建树不充当 producer 或 consumer。

- [ ] **Step 5: 完整注册 install consumer CTest**

Replace `tests/CMakeLists.txt` with:

```cmake
add_subdirectory(Architecture)

set(zz_install_consumer_support_dir
    "${CMAKE_CURRENT_BINARY_DIR}/InstallConsumer")
file(MAKE_DIRECTORY "${zz_install_consumer_support_dir}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/InstallConsumer/InstallConsumerContext.cmake.in"
    "${zz_install_consumer_support_dir}/InstallConsumerContext.cmake"
    @ONLY
)

add_test(
    NAME install.consumer
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TEST_ROOT=${zz_install_consumer_support_dir}/$<CONFIG>"
        "-DZZ_CONTEXT_FILE=${zz_install_consumer_support_dir}/InstallConsumerContext.cmake"
        "-DZZ_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/InstallConsumer/RunInstallConsumer.cmake"
)
set_tests_properties(install.consumer PROPERTIES
    LABELS "install"
    RUN_SERIAL TRUE
    TIMEOUT 600
)
```

上下文完整透传当前 configuration、C++ compiler、generator、generator platform、generator toolset、Qt/CMake prefix、macOS architectures、deployment target 和 sysroot。Windows 多配置生成器依靠 `$<CONFIG>` 传给 build/install/CTest，单配置生成器同时保留 `CMAKE_BUILD_TYPE`。

- [ ] **Step 6: 运行 shared 安装消费绿灯**

Run:

```bash
set -euo pipefail
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R '^install.consumer$'
```

Expected: CTest 输出 `fresh producer configure/build/install passed`、`fresh consumer configure/build/test passed`，并报告 `BUILD_SHARED_LIBS=ON`；`A`、`B` 和 `consumer` 是 `build/linux-gcc-debug/tests/InstallConsumer/Debug/` 下三个重新创建的独立目录。

- [ ] **Step 7: 运行正式 static 安装消费绿灯**

Run:

```bash
set -euo pipefail
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release -R '^(architecture.public-headers|install.consumer)$'
```

Expected: configure/build/CTest 全部返回 0；install consumer 报告 `BUILD_SHARED_LIBS=OFF`；消费者链接六个静态 imported target，且不需要 QWindowKit、ZzLog 或源码树头文件。Windows 执行同一流程时，静态宏阻止 `dllimport`；shared 流程由测试属性把 `B/bin` 和 `Qt6::Core` 的运行时目录加入 `PATH`。

- [ ] **Step 8: 提交独立消费门禁**

```bash
git add tests/CMakeLists.txt tests/InstallConsumer
git commit -m "测试：建立全新安装消费门禁" \
  -m "从源码重新创建 producer A，安装到隔离前缀 B，再由全新 consumer 查包。" \
  -m "透传生成器、编译器、平台、工具集和 macOS 参数，并逐个编译安装公共头。"
```

## Task 7: 执行最终 shared/static/LTO/tidy 验收

**Files:**
- 预计不修改源文件。

- [ ] **Step 1: 验证默认值和 GCC 版本缓存**

Run:

```bash
set -euo pipefail
cmake --preset linux-gcc-release
rg '^BUILD_SHARED_LIBS:BOOL=ON$' build/linux-gcc-release/CMakeCache.txt
rg -- '-std=c\\+\\+20' build/linux-gcc-release/compile_commands.json
rg '^set\\(CMAKE_CXX_COMPILER_VERSION "' build/linux-gcc-release/CMakeFiles/*/CMakeCXXCompiler.cmake
```

Expected: configure 返回 0；option cache、实际编译命令和 CMake 编译器识别文件分别证明默认 shared、`-std=c++20` 和 GCC 版本已生效。根配置已经拒绝低于 13.1 的 GNU 编译器。

- [ ] **Step 2: 验证 shared 全量门禁**

Run:

```bash
set -euo pipefail
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

Expected: 构建和全部 CTest 返回 0，包含 `architecture`、`headers`、`unit`、`install` 标签；`install.consumer` 使用全新 shared A/B/consumer 树。

- [ ] **Step 3: 验证 static 全量门禁**

Run:

```bash
set -euo pipefail
cmake --preset linux-static-release
cmake --build --preset linux-static-release
ctest --preset linux-static-release
```

Expected: configure/build/全部 CTest 返回 0，`install.consumer` 从静态 `B` 前缀完成链接和运行，十二个安装头各自独立编译。

- [ ] **Step 4: 再次证明 LTO 与 clang-tidy 实际执行**

Run:

```bash
set -euo pipefail
cmake --preset linux-gcc-lto-release
cmake --build --preset linux-gcc-lto-release --clean-first --verbose 2>&1 | tee build/linux-gcc-lto-release/final-lto.log
rg -- '-flto([^[:space:]]*)?' build/linux-gcc-lto-release/final-lto.log
ctest --preset linux-gcc-lto-release
cmake --preset linux-clang-tidy
cmake --build --preset linux-clang-tidy
cmake -E remove_directory build/linux-clang-tidy/clang-tidy
cmake --build --preset linux-clang-tidy --target ZzClangTidy --verbose 2>&1 | tee build/linux-clang-tidy/final-tidy.log
for source in ZzCore/src/private/ZzCoreVersion.cpp ZzWindowKit/src/private/ZzWindowKitVersion.cpp ZzFluentUI/foundation/src/private/ZzFluentVersion.cpp ZzFluentUI/widgets/src/private/ZzFluentWidgetVersion.cpp ZzPureTools/appcore/src/private/ZzAppCoreVersion.cpp ZzPureTools/widgets/src/private/ZzPureToolsVersion.cpp; do
    rg -F "$source" build/linux-clang-tidy/final-tidy.log
done
if rg 'mocs_compilation|qrc_' build/linux-clang-tidy/final-tidy.log; then exit 1; fi
ctest --preset linux-clang-tidy
```

Expected: LTO 日志含真实 `-flto` 编译或链接命令，不能只依赖 cache 中的 `ZZ_ENABLE_LTO=ON`；tidy 日志含六个组件的一方 `.cpp`，不含 MOC/RCC 生成 `.cpp`；两个 preset 的全部 CTest 均通过。

- [ ] **Step 5: 运行最终文档和工作区检查**

Run:

```bash
set -euo pipefail
git diff --check
git status --short
```

Expected: `git diff --check` 返回 0；状态中没有 `build/`、`install/`、`CMakeUserPresets.json`、clang-tidy stamp 或日志。Task 7 不修改源码，因此不创建空提交。

## 完成标准

- `cmake --list-presets` 列出 GCC 13.1+ debug/release/static/LTO 和 Clang release/tidy/ASan 组合。
- `linux-gcc-debug` 在 `g++-13` 与 Qt 6.8+ 环境中可配置、构建和测试；GCC 低于 13.1 或 Qt 低于 6.8 时有明确配置错误。
- 未显式指定时 `BUILD_SHARED_LIBS=ON`，`linux-static-release` 的 configure/build/全部 CTest 同样通过。
- 六个 target 以 `Zz::Core`、`Zz::WindowKit`、`Zz::FluentFoundation`、`Zz::FluentUI`、`Zz::AppCore`、`Zz::PureTools` 导出，依赖方向符合架构规范。
- 一方 `.cpp` 在严格 preset 下使用 `-Werror` 或 `/WX`；AUTOMOC/AUTORCC 和第三方源码不继承这些选项，clang-tidy 也不扫描生成源码。
- `ZzPublicHeadersTest` 在 build tree 中逐个编译全部源公共头和生成导出头；外部 consumer 在 install tree 中再次逐个编译十二个已安装头。
- `zz_install_package()` 安装 Windows DLL、import library、Unix/macOS shared library、static archive、源公共头、生成导出头、`ZzPureToolsProTargets.cmake`、Config 和 SameMinorVersion 文件。
- `install.consumer` 每次删除并重建隔离的 `A`、`B`、`consumer` 三棵树；它保留 Qt prefix，并透传 configuration、compiler、generator platform/toolset 与 macOS architecture/deployment target。
- 已安装 CMake 文件不包含源码树、主构建树、producer A 或本机 Qt SDK 的绝对路径。
- LTO 由 verbose 命令中的真实 `-flto` 证明；clang-tidy 由 `ZzClangTidy` 的真实组件源码命令证明。
- 每次提交前对应 configure/build/test 已恢复为绿灯；所有提交均使用中文标题和独立中文正文参数。
