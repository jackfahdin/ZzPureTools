cmake_minimum_required(VERSION 3.23)

foreach(zz_required IN ITEMS
    ZZ_APPCORE_ROOT
    ZZ_WIDGETS_ROOT
    ZZ_APPCORE_PUBLIC_ROOT
    ZZ_WIDGETS_PUBLIC_ROOT
    ZZ_ALLOWED_COMPOSITION_FILE
    ZZ_REQUIRE_COMPOSITION
)
    if(NOT DEFINED ${zz_required} OR "${${zz_required}}" STREQUAL "")
        message(FATAL_ERROR "${zz_required} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ZZ_APPCORE_ROOT
    NORMALIZE OUTPUT_VARIABLE zz_appcore_root)
cmake_path(ABSOLUTE_PATH ZZ_WIDGETS_ROOT
    NORMALIZE OUTPUT_VARIABLE zz_widgets_root)
cmake_path(ABSOLUTE_PATH ZZ_APPCORE_PUBLIC_ROOT
    NORMALIZE OUTPUT_VARIABLE zz_appcore_public_root)
cmake_path(ABSOLUTE_PATH ZZ_WIDGETS_PUBLIC_ROOT
    NORMALIZE OUTPUT_VARIABLE zz_widgets_public_root)
cmake_path(ABSOLUTE_PATH ZZ_ALLOWED_COMPOSITION_FILE
    NORMALIZE OUTPUT_VARIABLE zz_allowed_composition)

foreach(zz_directory IN ITEMS
    zz_appcore_root
    zz_widgets_root
    zz_appcore_public_root
    zz_widgets_public_root
)
    if(NOT IS_DIRECTORY "${${zz_directory}}")
        message(FATAL_ERROR
            "required PureTools scan directory is missing: ${${zz_directory}}")
    endif()
endforeach()
if(NOT EXISTS "${zz_allowed_composition}")
    message(FATAL_ERROR
        "allowed composition file is missing: ${zz_allowed_composition}")
endif()
cmake_path(IS_PREFIX zz_widgets_root "${zz_allowed_composition}"
    NORMALIZE zz_composition_is_in_widgets)
if(NOT zz_composition_is_in_widgets)
    message(FATAL_ERROR
        "allowed composition file must be inside ZZ_WIDGETS_ROOT")
endif()

file(GLOB_RECURSE zz_appcore_files
    LIST_DIRECTORIES FALSE
    "${zz_appcore_root}/*.h"
    "${zz_appcore_root}/*.hh"
    "${zz_appcore_root}/*.hpp"
    "${zz_appcore_root}/*.hxx"
    "${zz_appcore_root}/*.cpp"
    "${zz_appcore_root}/*.cc"
    "${zz_appcore_root}/*.cxx"
)
file(GLOB_RECURSE zz_widget_files
    LIST_DIRECTORIES FALSE
    "${zz_widgets_root}/*.h"
    "${zz_widgets_root}/*.hh"
    "${zz_widgets_root}/*.hpp"
    "${zz_widgets_root}/*.hxx"
    "${zz_widgets_root}/*.cpp"
    "${zz_widgets_root}/*.cc"
    "${zz_widgets_root}/*.cxx"
    "${zz_widgets_root}/*.mm"
)
file(GLOB_RECURSE zz_public_headers
    LIST_DIRECTORIES FALSE
    "${zz_appcore_public_root}/*.h"
    "${zz_appcore_public_root}/*.hh"
    "${zz_appcore_public_root}/*.hpp"
    "${zz_appcore_public_root}/*.hxx"
    "${zz_widgets_public_root}/*.h"
    "${zz_widgets_public_root}/*.hh"
    "${zz_widgets_public_root}/*.hpp"
    "${zz_widgets_public_root}/*.hxx"
)
if(NOT zz_appcore_files)
    message(FATAL_ERROR "ZZ_APPCORE_ROOT contains no C++ sources")
endif()
if(NOT zz_widget_files)
    message(FATAL_ERROR "ZZ_WIDGETS_ROOT contains no C++ sources")
endif()
if(NOT zz_public_headers)
    message(FATAL_ERROR "PureTools public roots contain no headers")
endif()
list(REMOVE_DUPLICATES zz_public_headers)
list(SORT zz_appcore_files)
list(SORT zz_widget_files)
list(SORT zz_public_headers)

set_property(GLOBAL PROPERTY ZZ_PURETOOLS_VIOLATIONS "")
set_property(GLOBAL PROPERTY ZZ_PURETOOLS_COMPOSITION_FILES "")

macro(zz_record_violation zz_rule zz_path zz_line)
    file(TO_CMAKE_PATH "${zz_path}" zz_violation_path)
    set_property(GLOBAL APPEND PROPERTY ZZ_PURETOOLS_VIOLATIONS
        "${zz_rule}:${zz_violation_path}:${zz_line}")
endmacro()

function(zz_read_source_lines zz_path zz_output)
    file(READ "${zz_path}" zz_source)
    string(REPLACE "\r\n" "\n" zz_source "${zz_source}")
    string(REPLACE "\r" "\n" zz_source "${zz_source}")
    string(REPLACE ";" "\\;" zz_source "${zz_source}")
    string(REPLACE "\n" ";" zz_lines "${zz_source}")
    set(${zz_output} "${zz_lines}" PARENT_SCOPE)
endfunction()

function(zz_strip_line_comments
    zz_input zz_initial_block zz_output zz_output_block)
    set(zz_remaining "${zz_input}")
    set(zz_code "")
    set(zz_in_block "${zz_initial_block}")
    set(zz_done FALSE)
    while(NOT zz_done)
        if(zz_remaining STREQUAL "")
            set(zz_done TRUE)
        elseif(zz_in_block)
            string(FIND "${zz_remaining}" "*/" zz_block_end)
            if(zz_block_end EQUAL -1)
                set(zz_done TRUE)
            else()
                math(EXPR zz_after_block "${zz_block_end} + 2")
                string(SUBSTRING "${zz_remaining}"
                    ${zz_after_block} -1 zz_remaining)
                set(zz_in_block FALSE)
            endif()
        else()
            string(FIND "${zz_remaining}" "//" zz_line_comment)
            string(FIND "${zz_remaining}" "/*" zz_block_start)
            if(zz_line_comment EQUAL -1 AND zz_block_start EQUAL -1)
                string(APPEND zz_code "${zz_remaining}")
                set(zz_done TRUE)
            elseif(NOT zz_line_comment EQUAL -1
                   AND (zz_block_start EQUAL -1
                        OR zz_line_comment LESS zz_block_start))
                string(SUBSTRING "${zz_remaining}"
                    0 ${zz_line_comment} zz_prefix)
                string(APPEND zz_code "${zz_prefix}")
                set(zz_done TRUE)
            else()
                string(SUBSTRING "${zz_remaining}"
                    0 ${zz_block_start} zz_prefix)
                string(APPEND zz_code "${zz_prefix}")
                math(EXPR zz_after_start "${zz_block_start} + 2")
                string(SUBSTRING "${zz_remaining}"
                    ${zz_after_start} -1 zz_remaining)
                set(zz_in_block TRUE)
            endif()
        endif()
    endwhile()
    set(${zz_output} "${zz_code}" PARENT_SCOPE)
    set(${zz_output_block} "${zz_in_block}" PARENT_SCOPE)
endfunction()

function(zz_scan_source_file zz_path zz_layer)
    zz_read_source_lines("${zz_path}" zz_lines)
    set(zz_in_block FALSE)
    set(zz_line_number 0)
    set(zz_has_windowkit FALSE)
    set(zz_has_fluent FALSE)
    set(zz_composition_line 1)

    foreach(zz_raw_line IN LISTS zz_lines)
        math(EXPR zz_line_number "${zz_line_number} + 1")
        zz_strip_line_comments(
            "${zz_raw_line}" "${zz_in_block}"
            zz_code zz_in_block)
        string(STRIP "${zz_code}" zz_code)
        if(zz_code STREQUAL "")
            continue()
        endif()

        if(zz_code MATCHES
           "namespace[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*::")
            zz_record_violation(
                CHAINED_NAMESPACE "${zz_path}" "${zz_line_number}")
        endif()

        if(zz_layer STREQUAL "appcore")
            if(zz_code MATCHES
               "#[ \t]*include[ \t]*[<\"](Qt(Gui|Widgets|Quick)(/|>)|ZzWindowKit/|ZzFluentUI/|QWK)"
               OR zz_code MATCHES
               "(^|[^A-Za-z0-9_])(QWidget|QWindow|QGuiApplication|QApplication|QQuick[A-Za-z0-9_]*|ZzWindowKit|ZzFluentUI|QWK)([^A-Za-z0-9_]|$)")
                zz_record_violation(
                    APP_CORE_UI_DEPENDENCY
                    "${zz_path}" "${zz_line_number}")
            endif()
        else()
            string(TOLOWER "${zz_code}" zz_lower_code)
            if(zz_lower_code MATCHES
               "#[ \t]*include[ \t]*[<\"][^>\"]*(repository|database|network[ _-]*client|domain[ _-]*entity)")
                zz_record_violation(
                    PRESENTATION_BUSINESS_DEPENDENCY
                    "${zz_path}" "${zz_line_number}")
            endif()
            if(zz_code MATCHES
               "#[ \t]*include[ \t]*[<\"](Qt[^>\"]*/private/|([^>\"]*/)?q[^>/\"]*_p\\.h)"
               OR zz_code MATCHES
               "(^|[^A-Za-z0-9_])QWK([^A-Za-z0-9_]|$)")
                zz_record_violation(
                    QT_PRIVATE_OR_QWK "${zz_path}" "${zz_line_number}")
            endif()
        endif()

        if(zz_code MATCHES
           "#[ \t]*include[ \t]*[<\"]ZzWindowKit/")
            set(zz_has_windowkit TRUE)
            set(zz_composition_line "${zz_line_number}")
        endif()
        if(zz_code MATCHES
           "#[ \t]*include[ \t]*[<\"]ZzFluentUI/")
            set(zz_has_fluent TRUE)
            set(zz_composition_line "${zz_line_number}")
        endif()
    endforeach()

    if(zz_has_windowkit AND zz_has_fluent)
        file(TO_CMAKE_PATH "${zz_path}" zz_normalized_path)
        if(zz_normalized_path STREQUAL zz_allowed_composition)
            set_property(GLOBAL APPEND PROPERTY
                ZZ_PURETOOLS_COMPOSITION_FILES "${zz_normalized_path}")
        else()
            zz_record_violation(
                COMPOSITION_UNIQUENESS
                "${zz_path}" "${zz_composition_line}")
        endif()
    endif()
endfunction()

function(zz_doxygen_is_valid zz_text zz_output)
    if(zz_text MATCHES "@brief" AND zz_text MATCHES "[一-龥]")
        set(${zz_output} TRUE PARENT_SCOPE)
    else()
        set(${zz_output} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(zz_validate_public_header zz_path)
    zz_read_source_lines("${zz_path}" zz_lines)
    set(zz_line_number 0)
    set(zz_comment_block FALSE)
    set(zz_in_doxygen FALSE)
    set(zz_doxygen_text "")
    set(zz_last_doxygen_valid FALSE)
    set(zz_public_section FALSE)
    set(zz_method_pending FALSE)
    set(zz_method_text "")
    set(zz_method_line 1)
    set(zz_method_doxygen FALSE)

    foreach(zz_raw_line IN LISTS zz_lines)
        math(EXPR zz_line_number "${zz_line_number} + 1")
        string(STRIP "${zz_raw_line}" zz_raw_trimmed)

        if(zz_in_doxygen)
            string(APPEND zz_doxygen_text "\n${zz_raw_line}")
            if(zz_raw_line MATCHES "\\*/")
                set(zz_in_doxygen FALSE)
                zz_doxygen_is_valid(
                    "${zz_doxygen_text}" zz_last_doxygen_valid)
                set(zz_doxygen_text "")
            endif()
            continue()
        endif()
        if(zz_raw_trimmed MATCHES "^/\\*\\*")
            set(zz_in_doxygen TRUE)
            set(zz_doxygen_text "${zz_raw_line}")
            if(zz_raw_line MATCHES "\\*/")
                set(zz_in_doxygen FALSE)
                zz_doxygen_is_valid(
                    "${zz_doxygen_text}" zz_last_doxygen_valid)
                set(zz_doxygen_text "")
            endif()
            continue()
        endif()

        zz_strip_line_comments(
            "${zz_raw_line}" "${zz_comment_block}"
            zz_code zz_comment_block)
        string(STRIP "${zz_code}" zz_code)
        if(zz_code STREQUAL "")
            continue()
        endif()

        if(zz_method_pending)
            string(APPEND zz_method_text " ${zz_code}")
            if(zz_code MATCHES ";" OR zz_code MATCHES "\\{")
                if(zz_method_text MATCHES "\\("
                   AND NOT zz_method_text MATCHES "=[ \t]*(default|delete)"
                   AND NOT zz_method_text MATCHES "override"
                   AND NOT zz_method_doxygen)
                    zz_record_violation(
                        PUBLIC_API_DOXYGEN
                        "${zz_path}" "${zz_method_line}")
                endif()
                set(zz_method_pending FALSE)
                set(zz_method_text "")
            endif()
            continue()
        endif()

        if(zz_code MATCHES "^(public:|Q_SIGNALS:|signals:)")
            set(zz_public_section TRUE)
            set(zz_last_doxygen_valid FALSE)
            continue()
        elseif(zz_code MATCHES "^(private:|protected:)")
            set(zz_public_section FALSE)
            set(zz_last_doxygen_valid FALSE)
            continue()
        endif()

        if(zz_code MATCHES
           "^(class|struct|enum[ \t]+class|enum[ \t]+struct)[ \t]"
           AND NOT (zz_code MATCHES ";[ \t]*$"
                    AND NOT zz_code MATCHES "\\{"))
            if(NOT zz_last_doxygen_valid)
                zz_record_violation(
                    PUBLIC_API_DOXYGEN
                    "${zz_path}" "${zz_line_number}")
            endif()
            set(zz_last_doxygen_valid FALSE)
            continue()
        endif()

        if(zz_code MATCHES "^};")
            set(zz_public_section FALSE)
            set(zz_last_doxygen_valid FALSE)
            continue()
        endif()

        if(zz_public_section
           AND (zz_code MATCHES "\\("
                OR (zz_last_doxygen_valid
                    AND NOT zz_code MATCHES "[;{}]"))
           AND NOT zz_code MATCHES
               "^(Q_OBJECT|Q_PROPERTY|Q_DISABLE_|Q_DECLARE_)")
            set(zz_method_line "${zz_line_number}")
            set(zz_method_text "${zz_code}")
            set(zz_method_doxygen "${zz_last_doxygen_valid}")
            set(zz_last_doxygen_valid FALSE)
            if(zz_code MATCHES ";" OR zz_code MATCHES "\\{")
                if(zz_method_text MATCHES "\\("
                   AND NOT zz_method_text MATCHES "=[ \t]*(default|delete)"
                   AND NOT zz_method_text MATCHES "override"
                   AND NOT zz_method_doxygen)
                    zz_record_violation(
                        PUBLIC_API_DOXYGEN
                        "${zz_path}" "${zz_method_line}")
                endif()
                set(zz_method_text "")
            else()
                set(zz_method_pending TRUE)
            endif()
            continue()
        endif()

        set(zz_last_doxygen_valid FALSE)
    endforeach()
endfunction()

foreach(zz_file IN LISTS zz_appcore_files)
    zz_scan_source_file("${zz_file}" appcore)
endforeach()
foreach(zz_file IN LISTS zz_widget_files)
    zz_scan_source_file("${zz_file}" widgets)
endforeach()
foreach(zz_header IN LISTS zz_public_headers)
    zz_validate_public_header("${zz_header}")
endforeach()

get_property(zz_composition_files GLOBAL
    PROPERTY ZZ_PURETOOLS_COMPOSITION_FILES)
if(ZZ_REQUIRE_COMPOSITION)
    if(zz_composition_files)
        list(REMOVE_DUPLICATES zz_composition_files)
    endif()
    list(LENGTH zz_composition_files zz_composition_count)
    if(NOT zz_composition_count EQUAL 1)
        zz_record_violation(
            COMPOSITION_UNIQUENESS "${zz_allowed_composition}" 1)
    endif()
endif()

get_property(zz_violations GLOBAL PROPERTY ZZ_PURETOOLS_VIOLATIONS)
if(zz_violations)
    list(REMOVE_DUPLICATES zz_violations)
    list(SORT zz_violations)
    list(JOIN zz_violations "\n" zz_violation_details)
    message(FATAL_ERROR
        "ZzPureTools boundary violations:\n${zz_violation_details}")
endif()

list(LENGTH zz_appcore_files zz_appcore_count)
list(LENGTH zz_widget_files zz_widget_count)
list(LENGTH zz_public_headers zz_public_count)
message(STATUS
    "ZzPureTools boundary scan passed: appcore=${zz_appcore_count}, widgets=${zz_widget_count}, public=${zz_public_count}")
