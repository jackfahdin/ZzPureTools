include_guard(GLOBAL)

include(ZzCompilerWarnings)

function(zz_add_public_header_probe)
    cmake_parse_arguments(PARSE_ARGV 0 ZZ_HEADER
        "" "OWNER;HEADER" "")
    if(NOT ZZ_HEADER_OWNER OR NOT ZZ_HEADER_HEADER)
        message(FATAL_ERROR
            "zz_add_public_header_probe requires OWNER and HEADER")
    endif()
    if(NOT TARGET ${ZZ_HEADER_OWNER})
        message(FATAL_ERROR
            "public header owner does not exist: ${ZZ_HEADER_OWNER}")
    endif()

    if(NOT TARGET ZzPublicHeadersTest)
        add_custom_target(ZzPublicHeadersTest)
    endif()

    string(MAKE_C_IDENTIFIER
        "${ZZ_HEADER_OWNER}_${ZZ_HEADER_HEADER}" zz_header_id)
    set(zz_probe_target "ZzPublicHeader_${zz_header_id}")
    set(zz_probe_source
        "${CMAKE_CURRENT_BINARY_DIR}/public-headers/${zz_header_id}.cpp")

    file(GENERATE
        OUTPUT "${zz_probe_source}"
        CONTENT "#include <${ZZ_HEADER_HEADER}>\n")
    set_source_files_properties("${zz_probe_source}" PROPERTIES
        GENERATED TRUE)

    add_library(${zz_probe_target} OBJECT "${zz_probe_source}")
    set_target_properties(${zz_probe_target} PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    target_link_libraries(${zz_probe_target} PRIVATE ${ZZ_HEADER_OWNER})
    zz_apply_first_party_warnings(${zz_probe_target}
        SOURCES "${zz_probe_source}")
    add_dependencies(ZzPublicHeadersTest ${zz_probe_target})
endfunction()

function(zz_add_public_header_directory)
    cmake_parse_arguments(PARSE_ARGV 0 ZZ_DIRECTORY
        "" "OWNER;DIRECTORY" "")
    if(NOT ZZ_DIRECTORY_OWNER OR NOT ZZ_DIRECTORY_DIRECTORY)
        message(FATAL_ERROR
            "zz_add_public_header_directory requires OWNER and DIRECTORY")
    endif()
    if(NOT IS_DIRECTORY "${ZZ_DIRECTORY_DIRECTORY}")
        message(FATAL_ERROR
            "public header directory does not exist: ${ZZ_DIRECTORY_DIRECTORY}")
    endif()

    file(GLOB_RECURSE zz_public_headers
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES FALSE
        "${ZZ_DIRECTORY_DIRECTORY}/*.h"
    )
    if(NOT zz_public_headers)
        message(FATAL_ERROR
            "public header directory is empty: ${ZZ_DIRECTORY_DIRECTORY}")
    endif()

    foreach(zz_public_header IN LISTS zz_public_headers)
        file(RELATIVE_PATH zz_public_include
            "${ZZ_DIRECTORY_DIRECTORY}" "${zz_public_header}")
        zz_add_public_header_probe(
            OWNER ${ZZ_DIRECTORY_OWNER}
            HEADER "${zz_public_include}"
        )
    endforeach()
endfunction()
