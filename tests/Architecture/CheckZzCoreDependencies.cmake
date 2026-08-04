cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SCAN_ROOT)
    message(FATAL_ERROR "ZZ_SCAN_ROOT is required")
endif()
if(NOT IS_DIRECTORY "${ZZ_SCAN_ROOT}")
    message(FATAL_ERROR "ZZ_SCAN_ROOT is not a directory: ${ZZ_SCAN_ROOT}")
endif()

function(zz_append_core_violation zz_rule zz_file zz_line zz_detail)
    string(APPEND zz_core_violations
        "${zz_file}:${zz_line}: [${zz_rule}] ${zz_detail}\n")
    set(zz_core_violations "${zz_core_violations}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE zz_core_scan_files
    LIST_DIRECTORIES FALSE
    "${ZZ_SCAN_ROOT}/*.h"
    "${ZZ_SCAN_ROOT}/*.hh"
    "${ZZ_SCAN_ROOT}/*.hpp"
    "${ZZ_SCAN_ROOT}/*.hxx"
    "${ZZ_SCAN_ROOT}/*.cpp"
    "${ZZ_SCAN_ROOT}/*.cc"
    "${ZZ_SCAN_ROOT}/*.cxx"
)
if(NOT zz_core_scan_files)
    message(FATAL_ERROR "ZZ_SCAN_ROOT contains no C++ files: ${ZZ_SCAN_ROOT}")
endif()

set(zz_core_violations "")
foreach(zz_file IN LISTS zz_core_scan_files)
    file(STRINGS "${zz_file}" zz_lines)
    set(zz_line_number 0)
    foreach(zz_line IN LISTS zz_lines)
        math(EXPR zz_line_number "${zz_line_number} + 1")

        if(zz_line MATCHES
           "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
            set(zz_include_path "${CMAKE_MATCH_1}")
            if(zz_include_path MATCHES
               "(^|/)(QtGui|QtWidgets|QtQuick)(/|$)")
                zz_append_core_violation(
                    "ZZCORE_FORBIDDEN_QT_MODULE"
                    "${zz_file}"
                    "${zz_line_number}"
                    "forbidden UI module include: ${zz_include_path}")
            endif()
            if(zz_include_path MATCHES "(^|/)private/q[^/]*")
                zz_append_core_violation(
                    "ZZCORE_QT_PRIVATE_INCLUDE"
                    "${zz_file}"
                    "${zz_line_number}"
                    "forbidden Qt private include: ${zz_include_path}")
            endif()
            if(zz_include_path MATCHES
               "^(QGuiApplication|QWindow|QWidget|QImage|QPixmap|QIcon)(\\.h)?$")
                zz_append_core_violation(
                    "ZZCORE_UNQUALIFIED_UI_INCLUDE"
                    "${zz_file}"
                    "${zz_line_number}"
                    "unqualified Qt UI include: ${zz_include_path}")
            endif()
        endif()

        if(zz_line MATCHES
           "^[ \t]*namespace[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*::")
            zz_append_core_violation(
                "ZZCORE_CHAINED_NAMESPACE"
                "${zz_file}"
                "${zz_line_number}"
                "chained namespace declaration is forbidden")
        endif()
    endforeach()
endforeach()

if(zz_core_violations)
    message(FATAL_ERROR
        "ZzCore dependency violations:\n${zz_core_violations}")
endif()
