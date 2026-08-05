cmake_minimum_required(VERSION 3.23)

project(ZzLog VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(ZZLOG_BUILD_TESTS "Build the dependency-free ZzLog smoke test" OFF)
option(ZZLOG_BUILD_EXAMPLES "Build ZzLog examples" OFF)
option(ZZLOG_BUILD_WARNINGS "Enable strict compiler warnings" OFF)

if(PROJECT_IS_TOP_LEVEL)
    option(ZZLOG_INSTALL "Generate ZzLog install rules" ON)
else()
    option(ZZLOG_INSTALL "Generate ZzLog install rules" OFF)
endif()

set(ZZLOG_BACKEND_SOURCES
    third_party/fmt/src/format.cc
    third_party/spdlog/src/common.cpp
    third_party/spdlog/src/logger.cpp
    third_party/spdlog/src/pattern_formatter.cpp
    third_party/spdlog/src/details/file_helper.cpp
    third_party/spdlog/src/details/os_filesystem.cpp
    third_party/spdlog/src/details/log_msg.cpp
    third_party/spdlog/src/details/async_log_msg.cpp
    third_party/spdlog/src/details/err_helper.cpp
    third_party/spdlog/src/sinks/base_sink.cpp
    third_party/spdlog/src/sinks/basic_file_sink.cpp
    third_party/spdlog/src/sinks/rotating_file_sink.cpp
    third_party/spdlog/src/sinks/stdout_sinks.cpp
    third_party/spdlog/src/sinks/async_sink.cpp
)

if(WIN32)
    list(APPEND ZZLOG_BACKEND_SOURCES
        third_party/spdlog/src/details/os_windows.cpp
        third_party/spdlog/src/sinks/wincolor_sink.cpp
    )
else()
    list(APPEND ZZLOG_BACKEND_SOURCES
        third_party/spdlog/src/details/os_unix.cpp
        third_party/spdlog/src/sinks/ansicolor_sink.cpp
    )
endif()

add_library(ZzLogBackend OBJECT ${ZZLOG_BACKEND_SOURCES})
target_compile_features(ZzLogBackend PRIVATE cxx_std_20)
target_compile_definitions(ZzLogBackend PRIVATE
    SPDLOG_DISABLE_GLOBAL_LOGGER
    SPDLOG_NAMESPACE=zzlog_spdlog
)
target_include_directories(ZzLogBackend PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include"
)
set_target_properties(ZzLogBackend PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

add_library(ZzLog
    src/ZzLog.cpp
    $<TARGET_OBJECTS:ZzLogBackend>
)
target_compile_definitions(ZzLog PRIVATE ZZLOG_BUILDING_LIBRARY)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(ZzLog PUBLIC ZZLOG_SHARED)
endif()

add_library(ZzLog::ZzLog ALIAS ZzLog)

find_package(Threads REQUIRED)

target_compile_features(ZzLog PUBLIC cxx_std_20)
target_compile_definitions(ZzLog PRIVATE
    SPDLOG_DISABLE_GLOBAL_LOGGER
    SPDLOG_NAMESPACE=zzlog_spdlog
)
target_include_directories(ZzLog
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include"
)
target_link_libraries(ZzLog PUBLIC Threads::Threads)

set_target_properties(ZzLog PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    ZZ_PUBLIC_HEADERS "ZzLog/Version.h;ZzLog/ZzLog.h"
    EXPORT_PROPERTIES ZZ_PUBLIC_HEADERS
)

if(COMMAND zz_enable_project_warnings)
    zz_enable_project_warnings(ZzLog)
    zz_enable_sanitizers(ZzLogBackend)
    zz_enable_sanitizers(ZzLog)
elseif(MSVC)
    target_compile_options(ZzLog PRIVATE /utf-8)
    if(ZZLOG_BUILD_WARNINGS)
        target_compile_options(ZzLog PRIVATE /W4 /WX)
    endif()
elseif(ZZLOG_BUILD_WARNINGS)
    target_compile_options(ZzLog PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(ANDROID)
    target_link_libraries(ZzLog PUBLIC log)
endif()

if(ZZLOG_BUILD_TESTS)
    enable_testing()
    add_executable(zzlog-smoke tests/smoke/main.cpp)
    target_link_libraries(zzlog-smoke PRIVATE ZzLog::ZzLog)
    target_compile_features(zzlog-smoke PRIVATE cxx_std_20)
    add_test(NAME zzlog.smoke COMMAND zzlog-smoke)
    set_tests_properties(zzlog.smoke PROPERTIES LABELS "component;zzlog")

    add_executable(zzlog-format-contract
        tests/format_contract/main.cpp
    )
    target_link_libraries(zzlog-format-contract PRIVATE ZzLog::ZzLog)
    target_compile_features(zzlog-format-contract PRIVATE cxx_std_20)
    set_property(TARGET zzlog-format-contract PROPERTY EXCLUDE_FROM_ALL FALSE)
    add_test(NAME zzlog.format-contract COMMAND zzlog-format-contract)
    set_tests_properties(zzlog.format-contract PROPERTIES LABELS "unit;zzlog")

    add_executable(zzlog-overflow
        tests/overflow/main.cpp
    )
    target_link_libraries(zzlog-overflow PRIVATE ZzLog::ZzLog)
    target_compile_features(zzlog-overflow PRIVATE cxx_std_20)
    set_property(TARGET zzlog-overflow PROPERTY EXCLUDE_FROM_ALL FALSE)
    add_test(NAME zzlog.overflow COMMAND zzlog-overflow)
    set_tests_properties(zzlog.overflow PROPERTIES
        LABELS "component;zzlog"
        TIMEOUT 30
    )

    add_executable(zzlog-async-barrier
        tests/async_barrier/main.cpp
        $<TARGET_OBJECTS:ZzLogBackend>
    )
    target_link_libraries(zzlog-async-barrier PRIVATE ZzLog::ZzLog)
    target_compile_features(zzlog-async-barrier PRIVATE cxx_std_20)
    target_compile_definitions(zzlog-async-barrier PRIVATE
        SPDLOG_DISABLE_GLOBAL_LOGGER
        SPDLOG_NAMESPACE=zzlog_spdlog
    )
    target_include_directories(zzlog-async-barrier PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include"
    )
    set_property(TARGET zzlog-async-barrier PROPERTY EXCLUDE_FROM_ALL FALSE)
    add_test(NAME zzlog.async-barrier COMMAND zzlog-async-barrier)
    set_tests_properties(zzlog.async-barrier PROPERTIES
        LABELS "component;zzlog"
        TIMEOUT 30
    )

    if(COMMAND zz_enable_sanitizers)
        foreach(zzlog_test_target IN ITEMS
            zzlog-smoke
            zzlog-format-contract
            zzlog-overflow
            zzlog-async-barrier
        )
            zz_enable_sanitizers(${zzlog_test_target})
        endforeach()
    endif()
endif()

if(ZZLOG_BUILD_EXAMPLES)
    add_executable(zzlog-basic-example examples/basic.cpp)
    target_link_libraries(zzlog-basic-example PRIVATE ZzLog::ZzLog)
    if(COMMAND zz_enable_sanitizers)
        zz_enable_sanitizers(zzlog-basic-example)
    endif()
endif()

if(ZZLOG_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS ZzLog
        EXPORT ZzLogTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(DIRECTORY licenses/ DESTINATION ${CMAKE_INSTALL_DATADIR}/ZzLog/licenses)
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/ZzLog/licenses)
    install(FILES README.md BUILDING-MULTIPLATFORM.md DEPENDENCIES.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/ZzLog)

    configure_package_config_file(
        cmake/ZzLogConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/ZzLogConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ZzLog"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ZzLogConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
    )

    install(EXPORT ZzLogTargets
        FILE ZzLogTargets.cmake
        NAMESPACE ZzLog::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ZzLog"
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ZzLogConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ZzLogConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ZzLog"
    )
endif()
