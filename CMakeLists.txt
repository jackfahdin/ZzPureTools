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
