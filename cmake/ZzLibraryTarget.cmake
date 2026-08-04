include_guard(GLOBAL)

include(GenerateExportHeader)
include(ZzFirstPartyTarget)

function(zz_configure_library_target target_name)
    set(zz_one_value_args
        EXPORT_NAME
        PUBLIC_INCLUDE_DIR
        EXPORT_HEADER_SUBDIR
        EXPORT_HEADER_NAME
        EXPORT_MACRO_NAME
    )
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_LIBRARY
        "" "${zz_one_value_args}" "SOURCES;MOC_HEADERS")

    foreach(zz_required_arg IN ITEMS
        EXPORT_NAME
        PUBLIC_INCLUDE_DIR
        EXPORT_HEADER_SUBDIR
        EXPORT_HEADER_NAME
        EXPORT_MACRO_NAME
    )
        if(NOT ZZ_LIBRARY_${zz_required_arg})
            message(FATAL_ERROR
                "zz_configure_library_target(${target_name}) requires ${zz_required_arg}")
        endif()
    endforeach()
    if(NOT ZZ_LIBRARY_SOURCES)
        message(FATAL_ERROR
            "zz_configure_library_target(${target_name}) requires SOURCES")
    endif()

    if(ZZ_LIBRARY_MOC_HEADERS)
        target_sources(${target_name} PRIVATE ${ZZ_LIBRARY_MOC_HEADERS})
    endif()

    set(zz_generated_include_dir
        "${CMAKE_CURRENT_BINARY_DIR}/generated/${target_name}/include")
    set(zz_generated_export_header
        "${zz_generated_include_dir}/${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}/${ZZ_LIBRARY_EXPORT_HEADER_NAME}")
    file(MAKE_DIRECTORY
        "${zz_generated_include_dir}/${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}")

    generate_export_header(${target_name}
        EXPORT_FILE_NAME "${zz_generated_export_header}"
        EXPORT_MACRO_NAME "${ZZ_LIBRARY_EXPORT_MACRO_NAME}"
        STATIC_DEFINE "${ZZ_LIBRARY_EXPORT_MACRO_NAME}_STATIC_DEFINE"
    )

    get_target_property(zz_library_type ${target_name} TYPE)
    if(zz_library_type STREQUAL "STATIC_LIBRARY")
        target_compile_definitions(${target_name} PUBLIC
            "${ZZ_LIBRARY_EXPORT_MACRO_NAME}_STATIC_DEFINE")
    endif()

    target_include_directories(${target_name}
        PUBLIC
            "$<BUILD_INTERFACE:${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}>"
            "$<BUILD_INTERFACE:${zz_generated_include_dir}>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    )

    file(GLOB_RECURSE zz_source_public_headers
        CONFIGURE_DEPENDS
        RELATIVE "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.h"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.hh"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.hpp"
        "${ZZ_LIBRARY_PUBLIC_INCLUDE_DIR}/*.hxx")
    list(APPEND zz_source_public_headers
        "${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}/${ZZ_LIBRARY_EXPORT_HEADER_NAME}")
    list(REMOVE_DUPLICATES zz_source_public_headers)
    list(SORT zz_source_public_headers)

    set_target_properties(${target_name} PROPERTIES
        EXPORT_NAME "${ZZ_LIBRARY_EXPORT_NAME}"
        VERSION "${PROJECT_VERSION}"
        SOVERSION "${PROJECT_VERSION_MAJOR}"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        ZZ_GENERATED_EXPORT_HEADER "${zz_generated_export_header}"
        ZZ_EXPORT_HEADER_INSTALL_SUBDIR
            "${ZZ_LIBRARY_EXPORT_HEADER_SUBDIR}"
        ZZ_PUBLIC_HEADERS "${zz_source_public_headers}"
        EXPORT_PROPERTIES ZZ_PUBLIC_HEADERS
    )

    zz_configure_first_party_target(${target_name}
        SOURCES ${ZZ_LIBRARY_SOURCES})
endfunction()
