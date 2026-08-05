cmake_minimum_required(VERSION 3.23)
project(ZzPublicHeaderConsumer LANGUAGES CXX)

if(NOT DEFINED ZZ_PACKAGE_ROOT OR "${ZZ_PACKAGE_ROOT}" STREQUAL "")
    message(FATAL_ERROR "ZZ_PACKAGE_ROOT is required")
endif()
file(GLOB_RECURSE package_configs LIST_DIRECTORIES FALSE
    "${ZZ_PACKAGE_ROOT}/*/cmake/ZzPureToolsPro/ZzPureToolsProConfig.cmake")
list(LENGTH package_configs package_config_count)
if(NOT "${package_config_count}" EQUAL 1)
    message(FATAL_ERROR
        "Expected one ZzPureToolsProConfig.cmake below package root")
endif()
list(GET package_configs 0 package_config)
cmake_path(GET package_config PARENT_PATH package_config_dir)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)
find_package(ZzPureToolsPro 0.1 CONFIG REQUIRED
    PATHS "${package_config_dir}"
    NO_DEFAULT_PATH)

file(GLOB_RECURSE installed_headers CONFIGURE_DEPENDS
    "${ZZ_PACKAGE_ROOT}/include/*.h"
    "${ZZ_PACKAGE_ROOT}/include/*.hh"
    "${ZZ_PACKAGE_ROOT}/include/*.hpp"
    "${ZZ_PACKAGE_ROOT}/include/*.hxx")
if(NOT installed_headers)
    message(FATAL_ERROR "No installed public headers found")
endif()

add_custom_target(ZzInstalledPublicHeaders ALL)
set(installed_header_names)
foreach(header IN LISTS installed_headers)
    file(RELATIVE_PATH include_name "${ZZ_PACKAGE_ROOT}/include" "${header}")
    file(TO_CMAKE_PATH "${include_name}" include_name)
    list(APPEND installed_header_names "${include_name}")
endforeach()
list(SORT installed_header_names)

set(zz_header_owners
    ZzLog::ZzLog
    Zz::Core
    Zz::WindowKit
    Zz::FluentFoundation
    Zz::FluentUI
    Zz::AppCore
    Zz::PureTools)
set(owner_manifest)
foreach(owner IN LISTS zz_header_owners)
    get_target_property(owner_headers "${owner}" ZZ_PUBLIC_HEADERS)
    if("${owner_headers}" STREQUAL "owner_headers-NOTFOUND"
       OR "${owner_headers}" STREQUAL "")
        message(FATAL_ERROR "${owner} exports no ZZ_PUBLIC_HEADERS manifest")
    endif()
    foreach(include_name IN LISTS owner_headers)
        if(IS_ABSOLUTE "${include_name}"
           OR "${include_name}" MATCHES "(^|/)\\.\\.(/|$)")
            message(FATAL_ERROR
                "${owner} declares unsafe public header path: ${include_name}")
        endif()
        list(FIND owner_manifest "${include_name}" duplicate_index)
        if(NOT "${duplicate_index}" EQUAL -1)
            message(FATAL_ERROR
                "Public header has more than one owner: ${include_name}")
        endif()
        list(APPEND owner_manifest "${include_name}")

        string(SHA256 header_digest "${owner}|${include_name}")
        string(SUBSTRING "${header_digest}" 0 12 header_id)
        set(source "${CMAKE_CURRENT_BINARY_DIR}/${header_id}.cpp")
        file(GENERATE OUTPUT "${source}"
            CONTENT "#include <${include_name}>\n")
        set_source_files_properties("${source}" PROPERTIES GENERATED TRUE)
        add_library("ZzInstalledHeader_${header_id}" OBJECT "${source}")
        target_compile_features(
            "ZzInstalledHeader_${header_id}" PRIVATE cxx_std_20)
        set_target_properties("ZzInstalledHeader_${header_id}" PROPERTIES
            CXX_EXTENSIONS OFF)
        target_link_libraries("ZzInstalledHeader_${header_id}" PRIVATE "${owner}")
        add_dependencies(ZzInstalledPublicHeaders
            "ZzInstalledHeader_${header_id}")
    endforeach()
endforeach()
list(SORT owner_manifest)
if(NOT "${owner_manifest}" STREQUAL "${installed_header_names}")
    message(FATAL_ERROR
        "Installed headers and owner manifests differ.\n"
        "Installed: ${installed_header_names}\n"
        "Owned: ${owner_manifest}")
endif()
