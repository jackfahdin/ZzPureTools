cmake_minimum_required(VERSION 3.23)

project(ZzLog VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(ZZLOG_BUILD_SHARED "Build ZzLog as a shared library" OFF)
option(ZZLOG_BUILD_TESTS "Build the dependency-free ZzLog smoke test" OFF)
option(ZZLOG_BUILD_EXAMPLES "Build ZzLog examples" OFF)
option(ZZLOG_BUILD_WARNINGS "Enable strict compiler warnings" OFF)

if(PROJECT_IS_TOP_LEVEL)
    option(ZZLOG_INSTALL "Generate ZzLog install rules" ON)
else()
    option(ZZLOG_INSTALL "Generate ZzLog install rules" OFF)
endif()

set(ZZLOG_SOURCES
    src/ZzLog.cpp
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
    list(APPEND ZZLOG_SOURCES
        third_party/spdlog/src/details/os_windows.cpp
        third_party/spdlog/src/sinks/wincolor_sink.cpp
    )
else()
    list(APPEND ZZLOG_SOURCES
        third_party/spdlog/src/details/os_unix.cpp
        third_party/spdlog/src/sinks/ansicolor_sink.cpp
    )
endif()

if(ZZLOG_BUILD_SHARED)
    add_library(ZzLog SHARED ${ZZLOG_SOURCES})
    target_compile_definitions(ZzLog PRIVATE ZZLOG_BUILDING_LIBRARY PUBLIC ZZLOG_SHARED)
else()
    add_library(ZzLog STATIC ${ZZLOG_SOURCES})
    target_compile_definitions(ZzLog PRIVATE ZZLOG_BUILDING_LIBRARY)
endif()

add_library(ZzLog::ZzLog ALIAS ZzLog)

find_package(Threads REQUIRED)

target_compile_features(ZzLog PUBLIC cxx_std_17)
target_compile_definitions(ZzLog PRIVATE
    SPDLOG_DISABLE_GLOBAL_LOGGER
    SPDLOG_NAMESPACE=zzlog_spdlog
)
target_include_directories(ZzLog
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include"
)
target_link_libraries(ZzLog PUBLIC Threads::Threads)

set_target_properties(ZzLog PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(MSVC)
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
    add_executable(zzlog-smoke tests/smoke.cpp)
    target_link_libraries(zzlog-smoke PRIVATE ZzLog::ZzLog)
    add_test(NAME zzlog-smoke COMMAND zzlog-smoke)
endif()

if(ZZLOG_BUILD_EXAMPLES)
    add_executable(zzlog-basic-example examples/basic.cpp)
    target_link_libraries(zzlog-basic-example PRIVATE ZzLog::ZzLog)
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
    install(DIRECTORY third_party/fmt/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
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
        COMPATIBILITY SameMajorVersion
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
