cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR ZZ_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR
    NORMALIZE
    OUTPUT_VARIABLE zz_source_root)
if(NOT IS_DIRECTORY "${zz_source_root}")
    message(FATAL_ERROR
        "source directory does not exist: ${zz_source_root}")
endif()

function(zz_append_windowkit_violation zz_rule zz_file zz_line zz_detail)
    string(APPEND zz_windowkit_violations
        "${zz_file}:${zz_line}: [${zz_rule}] ${zz_detail}\n")
    set(zz_windowkit_violations
        "${zz_windowkit_violations}" PARENT_SCOPE)
endfunction()

set(zz_windowkit_violations "")
set(zz_public_root "${zz_source_root}/ZzWindowKit/include")
if(NOT IS_DIRECTORY "${zz_public_root}")
    message(FATAL_ERROR
        "ZzWindowKit public include directory is missing: ${zz_public_root}")
endif()
file(GLOB_RECURSE zz_windowkit_public_headers
    LIST_DIRECTORIES FALSE
    "${zz_public_root}/*.h"
    "${zz_public_root}/*.hh"
    "${zz_public_root}/*.hpp"
    "${zz_public_root}/*.hxx"
)
if(NOT zz_windowkit_public_headers)
    message(FATAL_ERROR
        "ZzWindowKit public include directory contains no headers")
endif()

foreach(zz_header IN LISTS zz_windowkit_public_headers)
    file(STRINGS "${zz_header}" zz_header_lines)
    set(zz_line_number 0)
    foreach(zz_line IN LISTS zz_header_lines)
        math(EXPR zz_line_number "${zz_line_number} + 1")
        if(zz_line MATCHES "QWK|QWindowKit|qwindowkit")
            zz_append_windowkit_violation(
                "ZZWINDOWKIT_PUBLIC_QWK"
                "${zz_header}"
                "${zz_line_number}"
                "public header mentions an upstream QWindowKit symbol")
        endif()
        if(zz_line MATCHES "private/")
            zz_append_windowkit_violation(
                "ZZWINDOWKIT_PUBLIC_PRIVATE_INCLUDE"
                "${zz_header}"
                "${zz_line_number}"
                "public header mentions a private include path")
        endif()
    endforeach()
endforeach()

set(zz_first_party_roots
    "${zz_source_root}/ZzCore"
    "${zz_source_root}/ZzWindowKit"
    "${zz_source_root}/ZzFluentUI"
    "${zz_source_root}/ZzPureTools"
    "${zz_source_root}/examples"
    "${zz_source_root}/tests"
)
set(zz_first_party_sources)
foreach(zz_first_party_root IN LISTS zz_first_party_roots)
    if(NOT IS_DIRECTORY "${zz_first_party_root}")
        continue()
    endif()
    file(GLOB_RECURSE zz_root_sources
        LIST_DIRECTORIES FALSE
        "${zz_first_party_root}/*.h"
        "${zz_first_party_root}/*.hh"
        "${zz_first_party_root}/*.hpp"
        "${zz_first_party_root}/*.hxx"
        "${zz_first_party_root}/*.cpp"
        "${zz_first_party_root}/*.cc"
        "${zz_first_party_root}/*.cxx"
        "${zz_first_party_root}/*.mm"
    )
    list(APPEND zz_first_party_sources ${zz_root_sources})
endforeach()
if(NOT zz_first_party_sources)
    message(FATAL_ERROR "source tree contains no first-party C++ files")
endif()

set(zz_adapter_prefix
    "${zz_source_root}/ZzWindowKit/src/private/ZzQWindowKitBackend.")
foreach(zz_source IN LISTS zz_first_party_sources)
    file(TO_CMAKE_PATH "${zz_source}" zz_source_normalized)
    if(zz_source_normalized MATCHES
       "/tests/Architecture/fixtures/")
        continue()
    endif()
    file(STRINGS "${zz_source}" zz_source_lines)
    set(zz_line_number 0)
    foreach(zz_line IN LISTS zz_source_lines)
        math(EXPR zz_line_number "${zz_line_number} + 1")
        if(NOT zz_line MATCHES
           "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
            continue()
        endif()
        set(zz_include_path "${CMAKE_MATCH_1}")

        if(zz_include_path MATCHES
           "(^|/)(qwindowkit_linux|qwindowkit_windows)\\.h$")
            zz_append_windowkit_violation(
                "ZZWINDOWKIT_PLATFORM_HEADER"
                "${zz_source_normalized}"
                "${zz_line_number}"
                "platform-specific QWindowKit header: ${zz_include_path}")
        endif()
        if(zz_include_path MATCHES "(^|/)[^/]+_p\\.h$")
            zz_append_windowkit_violation(
                "ZZWINDOWKIT_PRIVATE_HEADER"
                "${zz_source_normalized}"
                "${zz_line_number}"
                "private implementation header: ${zz_include_path}")
        endif()

        if(zz_include_path MATCHES "^(QWKCore|QWKWidgets)/")
            string(FIND "${zz_source_normalized}"
                "${zz_adapter_prefix}" zz_adapter_position)
            if(NOT zz_adapter_position EQUAL 0)
                zz_append_windowkit_violation(
                    "ZZWINDOWKIT_QWK_INCLUDE_ESCAPE"
                    "${zz_source_normalized}"
                    "${zz_line_number}"
                    "QWK include is outside the private adapter: ${zz_include_path}")
            endif()
        elseif(zz_include_path MATCHES "^(QWK|QWindowKit|qwindowkit)")
            zz_append_windowkit_violation(
                "ZZWINDOWKIT_UNKNOWN_QWK_INCLUDE"
                "${zz_source_normalized}"
                "${zz_line_number}"
                "unsupported upstream include path: ${zz_include_path}")
        endif()
    endforeach()
endforeach()

if(zz_windowkit_violations)
    message(FATAL_ERROR
        "ZzWindowKit boundary violations:\n${zz_windowkit_violations}")
endif()

message(STATUS "ZzWindowKit boundary scan passed")
