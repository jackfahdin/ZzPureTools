cmake_minimum_required(VERSION 3.21)

# 从单一配置文件读取项目版本，避免多处硬编码
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ci/project.json" ZZ_PROJECT_JSON)
string(JSON ZZ_PROJECT_VERSION GET ${ZZ_PROJECT_JSON} "version")

project(ZzPureTools
    VERSION ${ZZ_PROJECT_VERSION}
    DESCRIPTION "Cross-platform Fluent UI widget library for Qt 6"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ZZ_BUILD_SHARED_LIBS "Build ZzPureTools as a shared library" ON)
option(ZZ_BUILD_EXAMPLE "Build example application" ON)
option(ZZ_INSTALL "Generate install rules" ON)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 6.8.0 REQUIRED COMPONENTS Widgets)

if(NOT WIN32)
    set(CMAKE_SKIP_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

set(ZZ_PURE_TOOLS_PUBLIC_HEADERS
    include/ZzPureTools/ZzPureTools.hpp
    include/ZzPureTools/Core/ZzApplication.hpp
    include/ZzPureTools/Core/ZzPureToolsGlobal.hpp
    include/ZzPureTools/Core/ZzPalette.hpp
    include/ZzPureTools/Core/ZzTheme.hpp
    include/ZzPureTools/Core/ZzToken.hpp
    include/ZzPureTools/Style/ZzAnimator.hpp
    include/ZzPureTools/Style/ZzStyleDelegate.hpp
    include/ZzPureTools/Widgets/ZzComboBox.hpp
    include/ZzPureTools/Widgets/ZzPushButton.hpp
)

set(ZZ_PURE_TOOLS_PRIVATE_HEADERS
    src/Style/ZzPaintPrimitives.hpp
    src/Style/ZzPushButtonDelegate.hpp
    src/Style/ZzComboBoxDelegate.hpp
)

set(ZZ_PURE_TOOLS_SOURCES
    src/Core/ZzApplication.cpp
    src/Core/ZzPalette.cpp
    src/Core/ZzTheme.cpp
    src/Style/ZzAnimator.cpp
    src/Style/ZzComboBoxDelegate.cpp
    src/Style/ZzPaintPrimitives.cpp
    src/Style/ZzPushButtonDelegate.cpp
    src/Style/ZzStyleDelegate.cpp
    src/Widgets/ZzComboBox.cpp
    src/Widgets/ZzPushButton.cpp
)

if(ZZ_BUILD_SHARED_LIBS)
    add_library(ZzPureTools SHARED
        ${ZZ_PURE_TOOLS_PUBLIC_HEADERS}
        ${ZZ_PURE_TOOLS_PRIVATE_HEADERS}
        ${ZZ_PURE_TOOLS_SOURCES}
    )
    target_compile_definitions(ZzPureTools PRIVATE ZZ_PURE_TOOLS_LIBRARY)
else()
    add_library(ZzPureTools STATIC
        ${ZZ_PURE_TOOLS_PUBLIC_HEADERS}
        ${ZZ_PURE_TOOLS_PRIVATE_HEADERS}
        ${ZZ_PURE_TOOLS_SOURCES}
    )
endif()

add_library(Zz::PureTools ALIAS ZzPureTools)

target_include_directories(ZzPureTools
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/Style
)

target_link_libraries(ZzPureTools
    PUBLIC
        Qt6::Widgets
)

target_compile_features(ZzPureTools PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(ZzPureTools PRIVATE /utf-8)
endif()

if(ZZ_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(ZZ_INSTALL)
    include(CMakePackageConfigHelpers)

    set(ZZ_CMAKE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/ZzPureTools")

    install(
        TARGETS ZzPureTools
        EXPORT ZzPureToolsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        DIRECTORY include/ZzPureTools
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        EXPORT ZzPureToolsTargets
        FILE ZzPureToolsTargets.cmake
        NAMESPACE Zz::
        DESTINATION ${ZZ_CMAKE_CONFIG_INSTALL_DIR}
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZzPureToolsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ZzPureToolsConfig.cmake"
        INSTALL_DESTINATION "${ZZ_CMAKE_CONFIG_INSTALL_DIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ZzPureToolsConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/ZzPureToolsConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/ZzPureToolsConfigVersion.cmake"
        DESTINATION "${ZZ_CMAKE_CONFIG_INSTALL_DIR}"
    )
endif()
