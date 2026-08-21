cmake_minimum_required(VERSION 3.23)

foreach(required_variable IN ITEMS
    ZZ_WORKSPACE_PUBLIC_ROOTS
    ZZ_WORKSPACE_SOURCE_ROOTS
    ZZ_WORKSPACE_PRIVATE_ROOTS)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

function(zz_collect_workspace_files output_variable)
    set(result)
    foreach(root IN LISTS ARGN)
        if(NOT IS_DIRECTORY "${root}")
            message(FATAL_ERROR "workspace scan root is missing: ${root}")
        endif()
        file(GLOB_RECURSE root_files
            LIST_DIRECTORIES FALSE
            "${root}/*.h"
            "${root}/*.hpp"
            "${root}/*.cpp")
        list(APPEND result ${root_files})
    endforeach()
    list(REMOVE_DUPLICATES result)
    set(${output_variable} "${result}" PARENT_SCOPE)
endfunction()

function(zz_workspace_fail rule source_file)
    message(FATAL_ERROR "${rule}: ${source_file}")
endfunction()

zz_collect_workspace_files(
    zz_workspace_public_files ${ZZ_WORKSPACE_PUBLIC_ROOTS})
zz_collect_workspace_files(
    zz_workspace_implementation_files
    ${ZZ_WORKSPACE_SOURCE_ROOTS}
    ${ZZ_WORKSPACE_PRIVATE_ROOTS})

foreach(source_file IN LISTS
    zz_workspace_public_files
    zz_workspace_implementation_files)
    file(READ "${source_file}" source_content)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" source_code
        "${source_content}")
    string(REGEX REPLACE "//[^\r\n]*" "" source_code "${source_code}")
    string(TOLOWER "${source_code}" source_code_lower)
    if(source_code_lower MATCHES
       "#[ \t]*include[ \t]*[<\"][^>\"]*(ssh|sftp|network|setting|repository|database|domain)[^>\"]*[>\"]")
        zz_workspace_fail(WORKSPACE_PRESENTATION_DEPENDENCY "${source_file}")
    endif()
endforeach()

foreach(public_header IN LISTS zz_workspace_public_files)
    file(READ "${public_header}" public_content)
    string(REGEX MATCHALL
        "class[ \t\r\n]+(ZZ_[A-Z0-9_]+_EXPORT[ \t\r\n]+)?Zz[A-Za-z0-9_]+[ \t\r\n]+(final[ \t\r\n]+)?(:[ \t\r\n]*public[ \t\r\n]+(QWidget|QFrame|QToolBar|QTabWidget|QDockWidget|QDialog|QAbstractScrollArea))"
        widget_declarations
        "${public_content}")
    foreach(widget_declaration IN LISTS widget_declarations)
        string(REGEX MATCH "Zz[A-Za-z0-9_]+" widget_name
            "${widget_declaration}")
        if(NOT public_content MATCHES
           "class[ \t\r\n]+${widget_name}Private[ \t\r\n]*;")
            zz_workspace_fail(WORKSPACE_PUBLIC_WIDGET_PIMPL "${public_header}")
        endif()
        if(NOT public_content MATCHES
           "std::unique_ptr[ \t\r\n]*<[ \t\r\n]*${widget_name}Private[ \t\r\n]*>[ \t\r\n]+d_ptr[ \t\r\n]*;")
            zz_workspace_fail(WORKSPACE_PUBLIC_WIDGET_PIMPL "${public_header}")
        endif()
    endforeach()
endforeach()

message(STATUS "Workspace boundary scan passed")
