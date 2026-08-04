cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR ZZ_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR
    NORMALIZE
    OUTPUT_VARIABLE zz_source_root)
set(zz_widgets_root "${zz_source_root}/ZzFluentUI/widgets")
set(zz_public_root "${zz_widgets_root}/include")
if(NOT IS_DIRECTORY "${zz_widgets_root}")
    message(FATAL_ERROR
        "ZzFluentUI widgets directory is missing: ${zz_widgets_root}")
endif()
if(NOT IS_DIRECTORY "${zz_public_root}")
    message(FATAL_ERROR
        "ZzFluentUI public include directory is missing: ${zz_public_root}")
endif()

# 先剥离注释，避免文档中的依赖名称或 namespace 示例触发误报。
function(zz_read_source_without_comments source_path output_variable)
    file(READ "${source_path}" source_code)
    string(REGEX REPLACE
        "/\\*([^*]|\\*+[^*/])*\\*+/" "" source_code "${source_code}")
    string(REGEX REPLACE
        "//[^\r\n]*" "" source_code "${source_code}")
    set(${output_variable} "${source_code}" PARENT_SCOPE)
endfunction()

macro(zz_record_violation rule source_path)
    file(RELATIVE_PATH relative_path "${zz_source_root}" "${source_path}")
    list(APPEND zz_violations "[${rule}] ${relative_path}")
endmacro()

file(GLOB_RECURSE zz_widget_files
    LIST_DIRECTORIES FALSE
    "${zz_widgets_root}/*.h"
    "${zz_widgets_root}/*.cpp"
)
if(NOT zz_widget_files)
    message(FATAL_ERROR "ZzFluentUI widgets contains no C++ sources")
endif()
list(SORT zz_widget_files)

set(zz_violations)
foreach(source_file IN LISTS zz_widget_files)
    zz_read_source_without_comments("${source_file}" source_code)
    if(source_code MATCHES
       "#[ \t]*include[ \t]*[<\"]([^>\"]*/)?(Repository|Database|NetworkClient|DomainEntity)[^>\"]*[>\"]")
        zz_record_violation("business-include" "${source_file}")
    endif()
    if(source_code MATCHES
       "#[ \t]*include[ \t]*[<\"](Qt[^>\"]*/private/|private/q[^>\"]*_p\\.h|q[^>\"]*_p\\.h)")
        zz_record_violation("qt-private" "${source_file}")
    endif()
    if(source_code MATCHES
       "#[ \t]*include[ \t]*[<\"](ZzWindowKit/|ZzPureTools/|QWK)")
        zz_record_violation("forbidden-component" "${source_file}")
    endif()
    if(source_code MATCHES
       "namespace[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[ \t]*::")
        zz_record_violation("chained-namespace" "${source_file}")
    endif()
endforeach()

file(GLOB_RECURSE zz_public_headers
    LIST_DIRECTORIES FALSE
    "${zz_public_root}/*.h"
)
if(NOT zz_public_headers)
    message(FATAL_ERROR "ZzFluentUI public include directory is empty")
endif()
list(SORT zz_public_headers)

foreach(public_header IN LISTS zz_public_headers)
    zz_read_source_without_comments("${public_header}" source_code)
    if(source_code MATCHES
       "#[ \t]*include[ \t]*[<\"][^>\"]*(src/private|/private/|q[^>\"]*_p\\.h)")
        zz_record_violation("public-private-include" "${public_header}")
    endif()
    if(source_code MATCHES
       "#[ \t]*include[ \t]*[<\"](ZzWindowKit/|ZzPureTools/|QWK)")
        zz_record_violation("public-forbidden-component" "${public_header}")
    endif()
    if(source_code MATCHES
       "#[ \t]*include[ \t]*[<\"]([^>\"]*/)?(Repository|Database|NetworkClient|DomainEntity)[^>\"]*[>\"]")
        zz_record_violation("public-business-include" "${public_header}")
    endif()
endforeach()

if(zz_violations)
    list(REMOVE_DUPLICATES zz_violations)
    list(SORT zz_violations)
    list(JOIN zz_violations "\n  " violation_details)
    message(FATAL_ERROR
        "ZzFluentUI widgets boundary violations:\n  ${violation_details}")
endif()

list(LENGTH zz_widget_files widget_file_count)
list(LENGTH zz_public_headers public_header_count)
message(STATUS
    "ZzFluentUI widgets boundary scan passed: files=${widget_file_count}, public=${public_header_count}")
