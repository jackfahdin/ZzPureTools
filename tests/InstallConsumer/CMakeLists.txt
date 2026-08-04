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
