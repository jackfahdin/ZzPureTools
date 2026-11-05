cmake_minimum_required(VERSION 3.23)
project(KissFftSdkInstall VERSION 0.1.0 LANGUAGES CXX)

# Exercise the real SDK install rules without building Qt or the full widget
# library. Each stand-in is a genuine static archive in Development.
set(BUILD_SHARED_LIBS OFF)
set(ZZ_RELEASE_BUILD OFF)
set(ZZ_BUNDLE_GNU_RUNTIME OFF)
set(Qt6_VERSION_MAJOR 6)
set(Qt6_VERSION_MINOR 8)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stub.cpp" "void fixture() {}\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/export.h" "#pragma once\n")
foreach(library IN ITEMS ZzCore ZzWindowKit ZzFluentFoundation ZzFluentUI
        ZzAppCore ZzPureTools ZzLog)
    add_library(${library} STATIC "${CMAKE_CURRENT_BINARY_DIR}/stub.cpp")
    set_target_properties(${library} PROPERTIES
        ZZ_GENERATED_EXPORT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/export.h"
        ZZ_EXPORT_HEADER_INSTALL_SUBDIR "${library}")
endforeach()
set_target_properties(ZzLog PROPERTIES VERSION 0.1.0)
set_target_properties(ZzWindowKit PROPERTIES
    ZZ_QWK_PRIVATE_INSTALL_DIR "lib/private"
    ZZ_QWK_CORE_FILE "fixture-core.a"
    ZZ_QWK_WIDGETS_FILE "fixture-widgets.a")

set(PROJECT_SOURCE_DIR "${ZZ_SOURCE_DIR}")
include("${ZZ_SOURCE_DIR}/cmake/ZzInstallPackage.cmake")
zz_install_package()
