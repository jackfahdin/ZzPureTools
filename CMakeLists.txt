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

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(ZZ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(ZzInstallPackage)
zz_install_package()
