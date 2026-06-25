cmake_minimum_required(VERSION 3.21)

project(ZzFluent
    VERSION 0.1.0
    DESCRIPTION "Cross-platform Fluent UI widget library for Qt 6"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ZZ_BUILD_SHARED_LIBS "Build ZzFluent libraries as shared objects" ON)
option(ZZ_BUILD_EXAMPLE "Build ZzFluentExample" ON)
option(ZZ_INSTALL "Generate install rules" ON)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets)

if(NOT WIN32)
    set(CMAKE_SKIP_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

add_subdirectory(ZzFluentCore)
add_subdirectory(ZzFluentWidgets)

if(ZZ_BUILD_EXAMPLE)
    add_subdirectory(ZzFluentExample)
endif()

if(ZZ_INSTALL)
    include(CMakePackageConfigHelpers)

    set(ZZ_CMAKE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/ZzFluent")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ZzFluentConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ZzFluentConfig.cmake"
        INSTALL_DESTINATION "${ZZ_CMAKE_CONFIG_INSTALL_DIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ZzFluentConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/ZzFluentConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/ZzFluentConfigVersion.cmake"
        DESTINATION "${ZZ_CMAKE_CONFIG_INSTALL_DIR}"
    )

    install(
        EXPORT ZzFluentTargets
        FILE ZzFluentTargets.cmake
        NAMESPACE Zz::
        DESTINATION "${ZZ_CMAKE_CONFIG_INSTALL_DIR}"
    )
endif()
