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
option(ZZ_ENABLE_MSVC_ANALYZE
    "Run MSVC code analysis on explicit first-party sources"
    OFF)
option(ZZ_WARNINGS_AS_ERRORS "Treat first-party warnings as errors" OFF)
option(ZZ_ENABLE_LTO "Enable interprocedural optimization" OFF)
option(ZZ_BUILD_FLUENT_QUICK "Build the future Qt Quick frontend" OFF)
option(ZZ_RELEASE_BUILD "Enable verified release packaging" OFF)
option(ZZ_BUNDLE_GNU_RUNTIME
    "Bundle the selected GNU C++ runtime in Linux release packages"
    OFF)
set(ZZ_GNU_RUNTIME_LICENSE_DIR "" CACHE PATH
    "Directory containing COPYING3 and COPYING.RUNTIME for GNU runtime bundling")
set(ZZ_QT_PREFIX "" CACHE PATH
    "Exact Qt SDK prefix recorded by platform and release gates")
set(ZZ_RELEASE_EVIDENCE_ROOT "" CACHE PATH
    "Directory containing external reviewed release evidence")
set(ZZ_RELEASE_FORCED_BLOCKERS "" CACHE STRING
    "Additional fail-only blocker ids used by release contract tests")
option(
    ZZ_WINDOWKIT_FORCE_QT_CONTEXT
    "Force QWindowKit to use the portable Qt window context"
    OFF
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # 统一 GNU 与 Clang 驱动行为，避免未使用的私有 Qt 依赖进入 ELF DT_NEEDED。
    add_link_options("LINKER:--as-needed")
endif()

if(ZZ_BUNDLE_GNU_RUNTIME)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
       OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
       OR NOT BUILD_SHARED_LIBS
       OR NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR
            "ZZ_BUNDLE_GNU_RUNTIME requires Linux, GNU, Release, and shared libraries")
    endif()
    if(NOT IS_ABSOLUTE "${ZZ_GNU_RUNTIME_LICENSE_DIR}"
       OR NOT IS_DIRECTORY "${ZZ_GNU_RUNTIME_LICENSE_DIR}")
        message(FATAL_ERROR
            "ZZ_GNU_RUNTIME_LICENSE_DIR must be an absolute existing directory")
    endif()
    foreach(zz_runtime_license IN ITEMS COPYING3 COPYING.RUNTIME)
        set(zz_runtime_license_path
            "${ZZ_GNU_RUNTIME_LICENSE_DIR}/${zz_runtime_license}")
        if(NOT EXISTS "${zz_runtime_license_path}"
           OR IS_DIRECTORY "${zz_runtime_license_path}")
            message(FATAL_ERROR
                "GNU runtime license is absent: ${zz_runtime_license_path}")
        endif()
        file(SIZE "${zz_runtime_license_path}" zz_runtime_license_size)
        if(zz_runtime_license_size EQUAL 0)
            message(FATAL_ERROR
                "GNU runtime license is empty: ${zz_runtime_license_path}")
        endif()
    endforeach()

    foreach(zz_runtime_name IN ITEMS libstdc++.so.6 libgcc_s.so.1)
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}"
                "-print-file-name=${zz_runtime_name}"
            RESULT_VARIABLE zz_runtime_result
            OUTPUT_VARIABLE zz_runtime_candidate
            ERROR_VARIABLE zz_runtime_error
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT zz_runtime_result EQUAL 0
           OR "${zz_runtime_candidate}" STREQUAL ""
           OR "${zz_runtime_candidate}" STREQUAL "${zz_runtime_name}"
           OR NOT EXISTS "${zz_runtime_candidate}"
           OR IS_DIRECTORY "${zz_runtime_candidate}")
            message(FATAL_ERROR
                "Unable to resolve ${zz_runtime_name} from ${CMAKE_CXX_COMPILER}: ${zz_runtime_error}")
        endif()
        file(REAL_PATH "${zz_runtime_candidate}" zz_runtime_real_path)
        if(NOT EXISTS "${zz_runtime_real_path}"
           OR IS_DIRECTORY "${zz_runtime_real_path}")
            message(FATAL_ERROR
                "Resolved GNU runtime is not a regular file: ${zz_runtime_real_path}")
        endif()
        if(zz_runtime_name STREQUAL "libstdc++.so.6")
            set(ZZ_GNU_LIBSTDCXX_PATH "${zz_runtime_real_path}")
        else()
            set(ZZ_GNU_LIBGCC_PATH "${zz_runtime_real_path}")
        endif()
    endforeach()
endif()

if(ZZ_RELEASE_BUILD)
    include(ZzReleaseChecks)
    zz_verify_release_evidence(
        SOURCE_ROOT "${PROJECT_SOURCE_DIR}"
        EVIDENCE_ROOT "${ZZ_RELEASE_EVIDENCE_ROOT}"
        FORCED_BLOCKERS ${ZZ_RELEASE_FORCED_BLOCKERS})
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

include(ZzFirstPartyTarget)
include(ZzLibraryTarget)

set(ZZLOG_INSTALL ON)
set(ZZLOG_BUILD_TESTS ${ZZ_BUILD_TESTS})
set(ZZLOG_BUILD_EXAMPLES OFF)
set(ZZLOG_BUILD_WARNINGS OFF)

add_subdirectory(ZzThirdParty/ZzLog)
add_subdirectory(ZzCore)
add_subdirectory(ZzWindowKit)
add_subdirectory(ZzFluentUI)
add_subdirectory(ZzPureTools)

if(ZZ_BUNDLE_GNU_RUNTIME)
    foreach(zz_runtime_target IN ITEMS
        ZzLog
        ZzCore
        ZzWindowKit
        ZzFluentFoundation
        ZzFluentUI
        ZzAppCore
        ZzPureTools)
        set_property(TARGET ${zz_runtime_target} PROPERTY INSTALL_RPATH "$ORIGIN")
    endforeach()
endif()

if(ZZ_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(ZZ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(ZzInstallPackage)
zz_install_package()
