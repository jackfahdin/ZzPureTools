cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR ZZ_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR
    NORMALIZE
    OUTPUT_VARIABLE zz_source_root)
set(zz_fluent_root "${zz_source_root}/ZzFluentUI")
if(NOT IS_DIRECTORY "${zz_fluent_root}")
    message(FATAL_ERROR
        "ZzFluentUI source directory is missing: ${zz_fluent_root}")
endif()

function(zz_read_source_without_comments zz_source_path zz_output_variable)
    file(READ "${zz_source_path}" zz_source_code)
    string(REGEX REPLACE
        "/\\*([^*]|\\*+[^*/])*\\*+/" "" zz_source_code "${zz_source_code}")
    string(REGEX REPLACE
        "//[^\r\n]*" "" zz_source_code "${zz_source_code}")
    set(${zz_output_variable} "${zz_source_code}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE zz_fluent_sources
    LIST_DIRECTORIES FALSE
    "${zz_fluent_root}/*.h"
    "${zz_fluent_root}/*.cpp"
)
if(NOT zz_fluent_sources)
    message(FATAL_ERROR "ZzFluentUI contains no C++ sources")
endif()

foreach(zz_source_file IN LISTS zz_fluent_sources)
    zz_read_source_without_comments("${zz_source_file}" zz_source_code)
    if(zz_source_code MATCHES
       "namespace[ \t\r\n]+ZzFluentUI[ \t]*::")
        message(FATAL_ERROR
            "chained namespace in ${zz_source_file}")
    endif()
    if(zz_source_code MATCHES
       "#[ \t]*include[ \t]*[<\"](Qt[^>\"]*/private/|private/q[^>\"]*_p\\.h|q[^>\"]*_p\\.h)")
        message(FATAL_ERROR
            "Qt Private API in ${zz_source_file}")
    endif()
    if(zz_source_code MATCHES
       "#[ \t]*include[ \t]*[<\"](ZzPureTools/|ZzWindowKit/|QWK)")
        message(FATAL_ERROR
            "forbidden component dependency in ${zz_source_file}")
    endif()
    if(zz_source_code MATCHES
       "#[ \t]*include[ \t]*[<\"]([^>\"]*/)?(Repository|Database|NetworkClient|DomainEntity)")
        message(FATAL_ERROR
            "business include in ${zz_source_file}")
    endif()
endforeach()

file(GLOB_RECURSE zz_foundation_files
    LIST_DIRECTORIES FALSE
    "${zz_fluent_root}/foundation/*.h"
    "${zz_fluent_root}/foundation/*.cpp"
)
if(NOT zz_foundation_files)
    message(FATAL_ERROR "ZzFluentUI foundation contains no C++ sources")
endif()

foreach(zz_source_file IN LISTS zz_foundation_files)
    zz_read_source_without_comments("${zz_source_file}" zz_source_code)
    if(zz_source_code MATCHES
       "#[ \t]*include[ \t]*[<\"]Qt(Widgets|Quick)/"
       OR zz_source_code MATCHES
          "(^|[^A-Za-z0-9_])(QApplication|QWidget|QProxyStyle)([^A-Za-z0-9_]|$)"
       OR zz_source_code MATCHES
          "(^|[^A-Za-z0-9_])(QML|QtQuick)([^A-Za-z0-9_]|$)")
        message(FATAL_ERROR
            "Foundation depends on a frontend in ${zz_source_file}")
    endif()
endforeach()

message(STATUS "ZzFluentUI boundary scan passed")
