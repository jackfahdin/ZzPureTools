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
    string(REGEX MATCHALL
        "#[ \t]*include[ \t]*[<\"][^>\"]+[>\"]"
        include_directives
        "${source_code}")
    foreach(include_directive IN LISTS include_directives)
        string(TOLOWER "${include_directive}" include_directive_lower)
        if(include_directive_lower MATCHES
           "#[ \t]*include[ \t]*[<\"][ \t]*(ssh|sftp|network|setting|repository|database|domain)"
           OR include_directive_lower MATCHES
           "/(ssh|sftp|network|setting|repository|database|domain)")
            zz_workspace_fail(WORKSPACE_PRESENTATION_DEPENDENCY "${source_file}")
        endif()
    endforeach()
    set(source_code_without_literals "${source_code}")
    string(REGEX REPLACE
        "\\\\[^\r\n]" ""
        source_code_without_literals "${source_code_without_literals}")
    string(REGEX REPLACE
        "\"[^\"\r\n]*\"" "\"\""
        source_code_without_literals "${source_code_without_literals}")
    string(REGEX REPLACE
        "'[^'\r\n]*'" "''"
        source_code_without_literals "${source_code_without_literals}")

    set(zz_business_type
        "(Ssh|SSH|Sftp|SFTP|Network|Domain)[A-Za-z0-9_]*")
    if(source_code_without_literals MATCHES
       "(^|[^A-Za-z0-9_])${zz_business_type}[ \t\r\n]*[*&]"
       OR source_code_without_literals MATCHES
       "(^|[^A-Za-z0-9_])${zz_business_type}[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*(;|=|,|\\)|\\(|\\{|\\[)"
       OR source_code_without_literals MATCHES
       "(<|,)[ \t\r\n]*${zz_business_type}[ \t\r\n]*(>|,)"
       OR source_code_without_literals MATCHES
       "(^|[^A-Za-z0-9_])${zz_business_type}[ \t\r\n]*::"
       OR source_code_without_literals MATCHES
       "(^|[^A-Za-z0-9_])(ssh|sftp|network|domain)[A-Za-z0-9_]*[ \t\r\n]*::")
        zz_workspace_fail(WORKSPACE_PRESENTATION_DEPENDENCY "${source_file}")
    endif()
endforeach()

foreach(public_header IN LISTS zz_workspace_public_files)
    file(READ "${public_header}" public_content)
    string(REGEX MATCHALL
        "class[ \t\r\n]+(ZZ_[A-Z0-9_]+_EXPORT[ \t\r\n]+)?Zz[A-Za-z0-9_]+[ \t\r\n]+(final[ \t\r\n]+)?(:[ \t\r\n]*public[ \t\r\n]+(QWidget|QFrame|QToolBar|QTabWidget|QDockWidget|QDialog|QAbstractScrollArea|QScrollBar|ZzScrollBar))"
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
