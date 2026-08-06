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

    string(SHA256 zz_header_digest
        "${ZZ_HEADER_OWNER}|${ZZ_HEADER_HEADER}")
    string(SUBSTRING "${zz_header_digest}" 0 12 zz_header_id)
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

function(zz_architecture_record rule_id source_path line_number detail)
    get_property(findings GLOBAL PROPERTY ZZ_ARCHITECTURE_FINDINGS)
    if(NOT findings)
        set(findings)
    endif()
    string(REPLACE ";" "," safe_detail "${detail}")
    list(APPEND findings
        "${rule_id}:${source_path}:${line_number}: ${safe_detail}")
    set_property(GLOBAL PROPERTY ZZ_ARCHITECTURE_FINDINGS "${findings}")
endfunction()

function(zz_architecture_line_number content token output)
    string(FIND "${content}" "${token}" position)
    if(position LESS 0)
        set(${output} 1 PARENT_SCOPE)
        return()
    endif()
    string(SUBSTRING "${content}" 0 ${position} prefix)
    string(REGEX MATCHALL "\n" newlines "${prefix}")
    list(LENGTH newlines newline_count)
    math(EXPR line_number "${newline_count} + 1")
    set(${output} "${line_number}" PARENT_SCOPE)
endfunction()

function(zz_architecture_strip_tokens input output)
    set(code "${input}")
    string(REGEX REPLACE
        "/\\*([^*]|\\*+[^*/])*\\*+/" "" code "${code}")
    string(REGEX REPLACE "//[^\r\n]*" "" code "${code}")
    string(REGEX REPLACE
        "\"([^\"\\\\]|\\\\.)*\"" "\"\"" code "${code}")
    string(REGEX REPLACE
        "'([^'\\\\]|\\\\.)*'" "''" code "${code}")
    set(${output} "${code}" PARENT_SCOPE)
endfunction()

function(zz_architecture_scan_source source_root source_file public_header)
    file(READ "${source_file}" raw_content)
    zz_architecture_strip_tokens("${raw_content}" source_content)
    file(RELATIVE_PATH relative_path "${source_root}" "${source_file}")
    file(TO_CMAKE_PATH "${relative_path}" relative_path)
    get_filename_component(file_stem "${source_file}" NAME_WE)
    get_filename_component(file_extension "${source_file}" EXT)

    string(REGEX MATCH
        "namespace[ \\t\\r\\n]+[A-Za-z_][A-Za-z0-9_]*[ \\t]*::"
        chained_namespace "${source_content}")
    if(chained_namespace)
        zz_architecture_line_number(
            "${raw_content}" "${chained_namespace}" finding_line)
        zz_architecture_record(CHAINED_NAMESPACE "${relative_path}"
            "${finding_line}" "链式命名空间声明")
    endif()

    set(qwk_allowed FALSE)
    if(relative_path STREQUAL
       "ZzWindowKit/src/private/ZzQWindowKitBackend.h"
       OR relative_path STREQUAL
          "ZzWindowKit/src/private/ZzQWindowKitBackend.cpp")
        set(qwk_allowed TRUE)
    endif()
    string(REGEX MATCH
        "#[ \\t]*include[ \\t]*[<\"][^>\"]*(Qt[^>\"]*/private|_p\\.h|QWKCore|QWKWidgets)[^>\"]*[>\"]|QWindowKit::"
        private_token "${source_content}")
    if(private_token AND NOT qwk_allowed)
        zz_architecture_line_number(
            "${raw_content}" "${private_token}" finding_line)
        zz_architecture_record(QT_PRIVATE_OR_QWK "${relative_path}"
            "${finding_line}" "Qt Private 或 QWindowKit 依赖越界")
    endif()

    string(TOLOWER "${relative_path}" relative_lower)
    if(relative_lower MATCHES
       "^zzfluentui/widgets/|^zzpuretools/widgets/")
        string(TOLOWER "${source_content}" source_lower)
        string(REGEX MATCH
            "#[ \\t]*include[ \\t]*[<\"][^>\"]*(repository|database|networkclient|domainentity)[^>\"]*[>\"]"
            business_include "${source_lower}")
        if(business_include)
            zz_architecture_line_number(
                "${raw_content}" "#include" finding_line)
            zz_architecture_record(PRESENTATION_BUSINESS_DEPENDENCY
                "${relative_path}" "${finding_line}"
                "展示层包含业务或存储依赖")
        endif()
    endif()

    string(REGEX MATCHALL
        "(class|struct|enum([ \\t\\r\\n]+class)?)[ \\t\\r\\n]+(\\[\\[[^]]*\\]\\][ \\t\\r\\n]*)*[A-Za-z_][A-Za-z0-9_]*[^;{]*\\{|concept[ \\t\\r\\n]+[A-Za-z_][A-Za-z0-9_]*[ \\t\\r\\n]*="
        type_definitions "${source_content}")
    set(has_primary_type FALSE)
    foreach(definition IN LISTS type_definitions)
        string(REGEX REPLACE
            "ZZ_[A-Z0-9_]+_EXPORT[ \\t\\r\\n]+" ""
            normalized_definition "${definition}")
        string(REGEX MATCH
            "(class|struct|concept|enum([ \\t\\r\\n]+class)?)[ \\t\\r\\n]+(\\[\\[[^]]*\\]\\][ \\t\\r\\n]*)*[A-Za-z_][A-Za-z0-9_]*"
            type_prefix "${normalized_definition}")
        string(REGEX MATCH "[A-Za-z_][A-Za-z0-9_]*$"
            type_name "${type_prefix}")
        if(type_name STREQUAL "")
            continue()
        endif()
        zz_architecture_line_number(
            "${raw_content}" "${type_name}" finding_line)
        if(NOT type_name MATCHES "^Zz")
            zz_architecture_record(TYPE_PREFIX "${relative_path}"
                "${finding_line}" "自定义类型 ${type_name} 缺少 Zz 前缀")
        endif()
        if(type_name STREQUAL file_stem)
            set(has_primary_type TRUE)
        endif()
    endforeach()

    if(file_extension MATCHES "^\\.h(h|pp|xx)?$")
        if(type_definitions AND NOT has_primary_type)
            zz_architecture_record(TYPE_FILENAME "${relative_path}" 1
                "头文件没有与文件名一致的主类型 ${file_stem}")
        endif()
        if(public_header AND type_definitions)
            if(NOT raw_content MATCHES "/\\*\\*"
               OR NOT raw_content MATCHES "@brief"
               OR NOT raw_content MATCHES "[一-龥]")
                zz_architecture_record(PUBLIC_API_DOXYGEN "${relative_path}" 1
                    "公开声明缺少含中文 @brief 的 Doxygen 注释")
            endif()
        endif()
    elseif(file_extension STREQUAL ".cpp"
           AND NOT file_stem STREQUAL "main")
        string(FIND "${source_content}" "${file_stem}::" primary_owner_position)
        if(primary_owner_position EQUAL -1)
            zz_architecture_record(TYPE_FILENAME "${relative_path}" 1
                "实现文件没有 ${file_stem} 的 out-of-line 定义")
        endif()
    endif()
endfunction()

function(zz_architecture_scan_target_manifest source_root target_manifest)
    if(NOT EXISTS "${target_manifest}")
        zz_architecture_record(TARGET_LINK_DIRECTION
            "${target_manifest}" 1 "target manifest 不存在")
        return()
    endif()
    file(STRINGS "${target_manifest}" manifest_lines)
    list(LENGTH manifest_lines manifest_count)
    if(NOT manifest_count EQUAL 6)
        zz_architecture_record(TARGET_LINK_DIRECTION
            "${target_manifest}" 1 "target manifest 必须包含六个组件")
    endif()
    set(line_number 0)
    foreach(manifest_line IN LISTS manifest_lines)
        math(EXPR line_number "${line_number} + 1")
        if(NOT manifest_line MATCHES "^([^|]+)\\|([^|]*)\\|(.*)$")
            zz_architecture_record(TARGET_LINK_DIRECTION
                "${target_manifest}" "${line_number}" "manifest 行格式无效")
            continue()
        endif()
        set(target "${CMAKE_MATCH_1}")
        set(direct_links "${CMAKE_MATCH_2}")
        set(interface_links "${CMAKE_MATCH_3}")
        set(all_links "${direct_links};${interface_links}")
        set(audited_interface_links "${interface_links}")
        if(target STREQUAL "ZzWindowKit")
            foreach(allowed_build_link IN ITEMS
                "$<LINK_ONLY:$<BUILD_INTERFACE:QWindowKit::Core>>"
                "$<LINK_ONLY:$<BUILD_INTERFACE:QWindowKit::Widgets>>")
                string(REPLACE "${allowed_build_link}" ""
                    audited_interface_links "${audited_interface_links}")
            endforeach()
        endif()
        if(audited_interface_links MATCHES "QWindowKit::|QWK(Core|Widgets)")
            zz_architecture_record(TARGET_LINK_DIRECTION
                "${target_manifest}" "${line_number}"
                "${target} 的接口泄露 QWindowKit")
        endif()
        set(forbidden_pattern)
        if(target STREQUAL "ZzCore")
            set(forbidden_pattern
                "Zz::(WindowKit|FluentFoundation|FluentUI|AppCore|PureTools)")
        elseif(target STREQUAL "ZzWindowKit")
            set(forbidden_pattern
                "Zz::(FluentFoundation|FluentUI|AppCore|PureTools)")
        elseif(target STREQUAL "ZzFluentFoundation")
            set(forbidden_pattern
                "Zz::(WindowKit|FluentUI|AppCore|PureTools)")
        elseif(target STREQUAL "ZzFluentUI")
            set(forbidden_pattern "Zz::(WindowKit|AppCore|PureTools)")
        elseif(target STREQUAL "ZzAppCore")
            set(forbidden_pattern
                "Zz::(WindowKit|FluentFoundation|FluentUI|PureTools)")
        elseif(NOT target STREQUAL "ZzPureTools")
            zz_architecture_record(TARGET_LINK_DIRECTION
                "${target_manifest}" "${line_number}"
                "未知组件 target: ${target}")
        endif()
        if(NOT "${forbidden_pattern}" STREQUAL ""
           AND all_links MATCHES "${forbidden_pattern}")
            zz_architecture_record(TARGET_LINK_DIRECTION
                "${target_manifest}" "${line_number}"
                "${target} 存在反向组件依赖: ${all_links}")
        endif()
    endforeach()
endfunction()

function(zz_run_complete_architecture_audit source_dir target_manifest)
    cmake_path(ABSOLUTE_PATH source_dir NORMALIZE OUTPUT_VARIABLE source_root)
    if(NOT IS_DIRECTORY "${source_root}")
        message(FATAL_ERROR "Architecture source root is absent: ${source_root}")
    endif()
    set_property(GLOBAL PROPERTY ZZ_ARCHITECTURE_FINDINGS "")
    set(scan_roots
        "${source_root}/ZzCore/include"
        "${source_root}/ZzCore/src"
        "${source_root}/ZzWindowKit/include"
        "${source_root}/ZzWindowKit/src"
        "${source_root}/ZzFluentUI/foundation"
        "${source_root}/ZzFluentUI/widgets"
        "${source_root}/ZzPureTools/appcore"
        "${source_root}/ZzPureTools/widgets"
        "${source_root}/examples/ZzPureToolsExample")
    set(public_roots
        "${source_root}/ZzCore/include"
        "${source_root}/ZzWindowKit/include"
        "${source_root}/ZzFluentUI/foundation/include"
        "${source_root}/ZzFluentUI/widgets/include"
        "${source_root}/ZzPureTools/appcore/include"
        "${source_root}/ZzPureTools/widgets/include")
    set(source_files)
    foreach(scan_root IN LISTS scan_roots)
        if(NOT IS_DIRECTORY "${scan_root}")
            message(FATAL_ERROR "Architecture scan root is absent: ${scan_root}")
        endif()
        file(GLOB_RECURSE root_files LIST_DIRECTORIES FALSE
            "${scan_root}/*.h" "${scan_root}/*.hh"
            "${scan_root}/*.hpp" "${scan_root}/*.hxx"
            "${scan_root}/*.cpp")
        if(NOT root_files)
            message(FATAL_ERROR "Architecture scan root is empty: ${scan_root}")
        endif()
        list(APPEND source_files ${root_files})
    endforeach()
    list(REMOVE_DUPLICATES source_files)
    list(SORT source_files)
    foreach(source_file IN LISTS source_files)
        set(is_public FALSE)
        foreach(public_root IN LISTS public_roots)
            cmake_path(IS_PREFIX public_root "${source_file}"
                NORMALIZE is_below_public_root)
            if(is_below_public_root)
                set(is_public TRUE)
                break()
            endif()
        endforeach()
        zz_architecture_scan_source(
            "${source_root}" "${source_file}" "${is_public}")
    endforeach()
    zz_architecture_scan_target_manifest(
        "${source_root}" "${target_manifest}")

    get_property(findings GLOBAL PROPERTY ZZ_ARCHITECTURE_FINDINGS)
    if(findings)
        list(SORT findings)
        list(JOIN findings "\n" finding_text)
        message(FATAL_ERROR "Architecture audit failed:\n${finding_text}")
    endif()
    message(STATUS "Complete Zz architecture audit passed")
endfunction()
