cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "ZZ_SOURCE_DIR 必须指向源码根目录")
endif()
file(REAL_PATH "${ZZ_SOURCE_DIR}" source_root)

set(required_docs
    README.md
    docs/development/CODING_STANDARD_ZH.md
    docs/development/BUILDING_ZH.md
    docs/development/PLATFORM_SUPPORT_ZH.md
    docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md
    docs/release/MANUAL_MACOS_CHECKLIST_ZH.md
    docs/release/MANUAL_LINUX_CHECKLIST_ZH.md
    docs/performance/PERFORMANCE_BASELINE_ZH.md
    docs/third-party/RELEASE_BLOCKERS_ZH.md
    docs/third-party/PROVENANCE_AUDIT_ZH.md
    docs/third-party/THIRD_PARTY_NOTICES.md
    docs/third-party/qwindowkit-vendor.json
    docs/third-party/release-evidence.json)
set(performance_scenarios
    startup
    theme-switch
    animation
    large-model
    window-lifecycle
    navigation-pane
    idle
    example-startup
    example-navigation
    example-theme-switch
    example-large-model
    example-idle)
foreach(scenario IN LISTS performance_scenarios)
    list(APPEND required_docs
        "docs/performance/reference/linux/${scenario}.json")
endforeach()
set(required_readme_links
    CMakeUserPresets.json.example
    LICENSE
    examples/ZzPureToolsDemo/main.cpp
    docs/development/BUILDING_ZH.md
    docs/development/CODING_STANDARD_ZH.md
    docs/development/GITHUB_ACTIONS_ZH.md
    docs/development/PLATFORM_SUPPORT_ZH.md
    docs/performance/PERFORMANCE_BASELINE_ZH.md
    docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md
    docs/release/MANUAL_MACOS_CHECKLIST_ZH.md
    docs/release/MANUAL_LINUX_CHECKLIST_ZH.md
    docs/third-party/THIRD_PARTY_NOTICES.md
    docs/third-party/RELEASE_BLOCKERS_ZH.md
    docs/superpowers/specs/2026-08-02-zzpuretoolsframe-architecture-design.md)
set(unknown_allowlist
    "docs/third-party/RELEASE_BLOCKERS_ZH.md|qwindowkit.upstream-provenance|qmsetup.windeployqt-5.15.2-derived-work|project.license"
    "docs/third-party/qwindowkit-vendor.json|qwindowkit.upstream-provenance|qmsetup.windeployqt-5.15.2-derived-work")
string(CONCAT deferred_phrase "implement" " later")
string(CONCAT analogy_phrase "Similar" " to")
string(CONCAT unfinished_word "place" "holder")
set(unresolved_patterns
    "TO[D]O" "TB[D]" "${unfinished_word}"
    "${deferred_phrase}" "${analogy_phrase}")
set(false_claim_patterns
    "已完全支持" "全平台通过" "全部真机验收通过" "release ready")
set(valid_statuses 未执行 静态验证通过 真机验收通过)

foreach(relative_path IN LISTS required_docs)
    set(document_path "${source_root}/${relative_path}")
    if(NOT EXISTS "${document_path}"
       OR IS_DIRECTORY "${document_path}"
       OR IS_SYMLINK "${document_path}")
        message(FATAL_ERROR "缺少必需的普通文档文件：${relative_path}")
    endif()
    file(SIZE "${document_path}" document_size)
    if(document_size EQUAL 0)
        message(FATAL_ERROR "必需文档为空：${relative_path}")
    endif()
endforeach()

set(performance_document_path
    "${source_root}/docs/performance/PERFORMANCE_BASELINE_ZH.md")
set(linux_runner_path
    "${source_root}/scripts/ci/run-linux-performance-gates.sh")
file(READ "${performance_document_path}" performance_document)
file(READ "${linux_runner_path}" linux_runner)
foreach(required_runner_token IN ITEMS
    "performance_scenarios=("
    "performance_scenarios[@]")
    string(FIND "${linux_runner}" "${required_runner_token}"
        runner_token_position)
    if(runner_token_position EQUAL -1)
        message(FATAL_ERROR
            "Linux 性能 runner 缺少场景数组契约：${required_runner_token}")
    endif()
endforeach()
string(FIND "${performance_document}" "十二份" report_count_position)
if(report_count_position EQUAL -1)
    message(FATAL_ERROR "性能基线文档没有声明十二份同源报告")
endif()

set(reference_commit "")
set(reference_digest "")
foreach(scenario IN LISTS performance_scenarios)
    set(report_relative_path
        "docs/performance/reference/linux/${scenario}.json")
    set(report_path "${source_root}/${report_relative_path}")
    file(READ "${report_path}" report_json)
    string(JSON schema_version ERROR_VARIABLE schema_error
        GET "${report_json}" schemaVersion)
    string(JSON report_scenario ERROR_VARIABLE scenario_error
        GET "${report_json}" scenario)
    string(JSON report_commit ERROR_VARIABLE commit_error
        GET "${report_json}" build commit)
    string(JSON report_digest ERROR_VARIABLE digest_error
        GET "${report_json}" environment runnerImageDigest)
    if(NOT "${schema_error}" STREQUAL "NOTFOUND"
       OR NOT "${scenario_error}" STREQUAL "NOTFOUND"
       OR NOT "${commit_error}" STREQUAL "NOTFOUND"
       OR NOT "${digest_error}" STREQUAL "NOTFOUND")
        message(FATAL_ERROR "性能参考报告字段不完整：${report_relative_path}")
    endif()
    if(NOT schema_version EQUAL 1
       OR NOT "${report_scenario}" STREQUAL "${scenario}")
        message(FATAL_ERROR
            "性能参考报告 schema/scenario 不匹配：${report_relative_path}")
    endif()
    string(LENGTH "${report_commit}" commit_length)
    string(LENGTH "${report_digest}" digest_length)
    if(NOT commit_length EQUAL 40
       OR NOT "${report_commit}" MATCHES "^[0-9a-f]+$"
       OR NOT digest_length EQUAL 71
       OR NOT "${report_digest}" MATCHES "^sha256:[0-9a-f]+$")
        message(FATAL_ERROR
            "性能参考报告缺少可审计 commit/digest：${report_relative_path}")
    endif()
    if("${reference_commit}" STREQUAL "")
        set(reference_commit "${report_commit}")
        set(reference_digest "${report_digest}")
    elseif(NOT "${report_commit}" STREQUAL "${reference_commit}"
           OR NOT "${report_digest}" STREQUAL "${reference_digest}")
        message(FATAL_ERROR
            "十二份性能参考报告不是同一 commit 和 runner 档案：${report_relative_path}")
    endif()

    string(FIND "${linux_runner}" "\n  ${scenario}\n"
        runner_scenario_position)
    string(FIND "${performance_document}"
        "benchmark.${scenario}.json" reporter_mapping_position)
    string(FIND "${performance_document}"
        "reference/linux/${scenario}.json" baseline_mapping_position)
    if(runner_scenario_position EQUAL -1)
        message(FATAL_ERROR
            "Linux 性能 runner 未比较场景：${scenario}")
    endif()
    if(reporter_mapping_position EQUAL -1
       OR baseline_mapping_position EQUAL -1)
        message(FATAL_ERROR
            "性能基线文档缺少报告映射：${scenario}")
    endif()
endforeach()
file(SHA256 "${source_root}/docs/performance/profiles/local-release-xvfb.json"
    profile_digest)
if(NOT "${reference_digest}" STREQUAL "sha256:${profile_digest}")
    message(FATAL_ERROR
        "性能参考报告 digest 与活动本机 runner 档案不一致")
endif()
foreach(reference_identity IN ITEMS "${reference_commit}" "${profile_digest}")
    string(FIND "${performance_document}" "${reference_identity}"
        document_identity_position)
    if(document_identity_position EQUAL -1)
        message(FATAL_ERROR
            "性能基线文档缺少报告身份：${reference_identity}")
    endif()
endforeach()
set(linux_checklist_path
    "${source_root}/docs/release/MANUAL_LINUX_CHECKLIST_ZH.md")
file(READ "${linux_checklist_path}" linux_checklist)
foreach(required_performance_count IN ITEMS
    "12 个报告生产者"
    "15 项绝对门禁")
    string(FIND "${linux_checklist}" "${required_performance_count}"
        checklist_count_position)
    if(checklist_count_position EQUAL -1)
        message(FATAL_ERROR
            "Linux 人工清单性能数量过期：${required_performance_count}")
    endif()
endforeach()

set(readme_path "${source_root}/README.md")
file(READ "${readme_path}" readme_content)
foreach(required_token IN ITEMS
    "Qt 6.8+"
    "C++20"
    "Zz::Core"
    "Zz::WindowKit"
    "Zz::FluentFoundation"
    "Zz::FluentUI"
    "Zz::AppCore"
    "Zz::PureTools"
    "cmake --preset linux-gcc-debug"
    "ctest --preset linux-gcc-debug"
    "尚无同一提交完整成功的矩阵"
    "MIT License"
    "Jackfahdin")
    string(FIND "${readme_content}" "${required_token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR "README 缺少必需说明：${required_token}")
    endif()
endforeach()
foreach(relative_path IN LISTS required_readme_links)
    string(FIND "${readme_content}" "](${relative_path})" link_position)
    if(link_position EQUAL -1)
        message(FATAL_ERROR "README 缺少入口链接：${relative_path}")
    endif()
    if(NOT EXISTS "${source_root}/${relative_path}")
        message(FATAL_ERROR "README 入口链接目标不存在：${relative_path}")
    endif()
endforeach()

function(zz_has_allowed_unverified_marker json_path out_allowed)
    set(allowed_ids ${ARGN})
    set(allowed FALSE)
    file(READ "${json_path}" json)
    string(JSON blocker_type ERROR_VARIABLE type_error
        TYPE "${json}" releaseBlockers)
    if("${type_error}" STREQUAL "NOTFOUND"
       AND "${blocker_type}" STREQUAL "ARRAY")
        string(JSON blocker_count LENGTH "${json}" releaseBlockers)
        if(blocker_count GREATER 0)
            math(EXPR last_blocker "${blocker_count} - 1")
            foreach(index RANGE 0 ${last_blocker})
                string(JSON entry_type ERROR_VARIABLE entry_error
                    TYPE "${json}" releaseBlockers ${index})
                set(blocker_id "")
                if("${entry_error}" STREQUAL "NOTFOUND"
                   AND "${entry_type}" STREQUAL "STRING")
                    string(JSON blocker_id GET
                        "${json}" releaseBlockers ${index})
                elseif("${entry_error}" STREQUAL "NOTFOUND"
                       AND "${entry_type}" STREQUAL "OBJECT")
                    string(JSON blocker_id ERROR_VARIABLE id_error GET
                        "${json}" releaseBlockers ${index} id)
                    if(NOT "${id_error}" STREQUAL "NOTFOUND")
                        set(blocker_id "")
                    endif()
                endif()
                if(NOT "${blocker_id}" STREQUAL ""
                   AND "${blocker_id}" IN_LIST allowed_ids)
                    set(allowed TRUE)
                endif()
            endforeach()
        endif()
    endif()
    set(${out_allowed} "${allowed}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE scanned_docs LIST_DIRECTORIES FALSE
    "${source_root}/docs/development/*"
    "${source_root}/docs/release/*"
    "${source_root}/docs/performance/*"
    "${source_root}/docs/third-party/*")
list(APPEND scanned_docs "${readme_path}")
foreach(document_path IN LISTS scanned_docs)
    if(IS_DIRECTORY "${document_path}")
        continue()
    endif()
    file(RELATIVE_PATH relative_path "${source_root}" "${document_path}")
    file(READ "${document_path}" content)

    foreach(pattern IN LISTS unresolved_patterns)
        if("${content}" MATCHES "${pattern}")
            message(FATAL_ERROR
                "文档包含未解决标记 ${pattern}：${relative_path}")
        endif()
    endforeach()
    foreach(pattern IN LISTS false_claim_patterns)
        if("${content}" MATCHES "${pattern}")
            message(FATAL_ERROR
                "文档包含未经证据支持的声明 ${pattern}：${relative_path}")
        endif()
    endforeach()

    if("${content}" MATCHES "UNKNOWN|UNVERIFIED")
        set(marker_allowed FALSE)
        foreach(allow_entry IN LISTS unknown_allowlist)
            if(NOT allow_entry MATCHES "^([^|]+)\\|(.*)$")
                message(FATAL_ERROR
                    "无效的 UNKNOWN/UNVERIFIED allowlist：${allow_entry}")
            endif()
            set(allowed_path "${CMAKE_MATCH_1}")
            set(allowed_ids_text "${CMAKE_MATCH_2}")
            if("${relative_path}" STREQUAL "${allowed_path}")
                string(REPLACE "|" ";" allowed_ids "${allowed_ids_text}")
                if(relative_path MATCHES "\\.json$")
                    zz_has_allowed_unverified_marker(
                        "${document_path}" marker_allowed ${allowed_ids})
                else()
                    set(vendor_manifest
                        "${source_root}/docs/third-party/qwindowkit-vendor.json")
                    zz_has_allowed_unverified_marker(
                        "${vendor_manifest}" marker_allowed ${allowed_ids})
                    if(NOT marker_allowed)
                        set(release_manifest
                            "${source_root}/docs/third-party/release-evidence.json")
                        zz_has_allowed_unverified_marker(
                            "${release_manifest}" marker_allowed ${allowed_ids})
                    endif()
                endif()
            endif()
        endforeach()
        if(NOT marker_allowed)
            message(FATAL_ERROR
                "UNKNOWN/UNVERIFIED 没有对应的非空 releaseBlockers：${relative_path}")
        endif()
    endif()

    file(STRINGS "${document_path}" explicit_status_lines
        ENCODING UTF-8
        REGEX "^状态:[ \\t]*")
    foreach(status_line IN LISTS explicit_status_lines)
        string(REGEX REPLACE "^状态:[ \\t]*" "" status "${status_line}")
        string(STRIP "${status}" status)
        if(NOT "${status}" IN_LIST valid_statuses)
            message(FATAL_ERROR
                "文档状态不属于允许词汇：${relative_path} -> ${status}")
        endif()
    endforeach()
endforeach()

function(zz_field_has_evidence value out_valid)
    string(STRIP "${value}" normalized)
    if("${normalized}" STREQUAL ""
       OR "${normalized}" STREQUAL "-"
       OR "${normalized}" STREQUAL "未执行")
        set(${out_valid} FALSE PARENT_SCOPE)
    else()
        set(${out_valid} TRUE PARENT_SCOPE)
    endif()
endfunction()

set(required_platform_rows
    windows-10-msvc-shared
    windows-10-msvc-static
    windows-10-mingw-shared
    windows-10-mingw-static
    windows-11-msvc-shared
    windows-11-msvc-static
    windows-11-mingw-shared
    windows-11-mingw-static
    macos-13-arm64-shared
    macos-13-arm64-static
    macos-13-x86_64-shared
    macos-13-x86_64-static
    linux-x11-kde
    linux-x11-gnome
    linux-wayland-kde
    linux-wayland-gnome
    linux-qt-fallback)
set(platform_path
    "${source_root}/docs/development/PLATFORM_SUPPORT_ZH.md")
file(STRINGS "${platform_path}" platform_lines ENCODING UTF-8)
set(found_platform_rows)
foreach(line IN LISTS platform_lines)
    if(NOT line MATCHES "^\\| `(windows|macos|linux)-[a-zA-Z0-9_-]+` \\|")
        continue()
    endif()
    string(REPLACE "|" ";" row_fields "${line}")
    list(LENGTH row_fields field_count)
    if(NOT field_count EQUAL 15)
        message(FATAL_ERROR "平台矩阵列数必须固定为 13：${line}")
    endif()
    foreach(index RANGE 1 13)
        list(GET row_fields ${index} field_value)
        string(STRIP "${field_value}" field_value)
        set(field_${index} "${field_value}")
    endforeach()
    string(REGEX REPLACE "^`|`$" "" row_id "${field_1}")
    if(NOT "${row_id}" IN_LIST required_platform_rows)
        message(FATAL_ERROR "平台矩阵包含未知 ID：${row_id}")
    endif()
    if("${row_id}" IN_LIST found_platform_rows)
        message(FATAL_ERROR "平台矩阵包含重复 ID：${row_id}")
    endif()
    list(APPEND found_platform_rows "${row_id}")

    set(status "${field_2}")
    if(NOT "${status}" IN_LIST valid_statuses)
        message(FATAL_ERROR "平台状态无效：${row_id} -> ${status}")
    endif()

    if(row_id MATCHES "^windows-")
        set(expected_runner_log "build/gate-evidence/windows-native.log")
        set(expected_checklist "MANUAL_WINDOWS_CHECKLIST_ZH.md")
    elseif(row_id MATCHES "^macos-")
        set(expected_runner_log "build/gate-evidence/macos-native.log")
        set(expected_checklist "MANUAL_MACOS_CHECKLIST_ZH.md")
    else()
        set(expected_runner_log "build/gate-evidence/linux-native.log")
        set(expected_checklist "MANUAL_LINUX_CHECKLIST_ZH.md")
    endif()

    if(NOT "${status}" STREQUAL "未执行")
        zz_field_has_evidence("${field_8}" build_result_valid)
        zz_field_has_evidence("${field_11}" evidence_valid)
        zz_field_has_evidence("${field_12}" date_valid)
        zz_field_has_evidence("${field_13}" reviewer_valid)
        string(FIND "${field_11}" "${expected_runner_log}" runner_log_position)
        if(NOT build_result_valid OR NOT evidence_valid
           OR NOT date_valid OR NOT reviewer_valid
           OR runner_log_position EQUAL -1)
            message(FATAL_ERROR
                "静态或真机平台状态缺少原生 runner 结果/日志/日期/审核人：${row_id}")
        endif()
    endif()
    if("${status}" STREQUAL "真机验收通过")
        zz_field_has_evidence("${field_9}" interaction_valid)
        zz_field_has_evidence("${field_10}" device_valid)
        string(FIND "${field_11}" "${expected_checklist}" checklist_position)
        if(NOT interaction_valid OR NOT device_valid
           OR checklist_position EQUAL -1)
            message(FATAL_ERROR
                "真机平台状态缺少交互结果、设备或签署清单：${row_id}")
        endif()
    endif()
    if(ZZ_REQUIRE_REAL_DEVICE
       AND NOT "${status}" STREQUAL "真机验收通过")
        message(FATAL_ERROR "发布候选仍缺少真机平台证据：${row_id}")
    endif()
endforeach()
foreach(required_row IN LISTS required_platform_rows)
    if(NOT "${required_row}" IN_LIST found_platform_rows)
        message(FATAL_ERROR "平台矩阵缺少 ID：${required_row}")
    endif()
endforeach()

function(zz_check_manual_checklist relative_path)
    set(checklist_path "${source_root}/${relative_path}")
    file(STRINGS "${checklist_path}" checklist_lines ENCODING UTF-8)
    set(expected_fields
        "状态:"
        "测试日期:"
        "测试人员:"
        "OS/版本:"
        "Qt/工具链:"
        "设备/显示器:"
        "构建产物摘要:"
        "结果:"
        "问题链接:")
    foreach(index RANGE 0 8)
        list(GET checklist_lines ${index} actual_line)
        list(GET expected_fields ${index} expected_prefix)
        string(FIND "${actual_line}" "${expected_prefix}" prefix_position)
        if(NOT prefix_position EQUAL 0)
            message(FATAL_ERROR
                "人工清单必须以固定字段顺序开始：${relative_path}")
        endif()
    endforeach()

    list(GET checklist_lines 0 status_line)
    string(REGEX REPLACE "^状态:[ \\t]*" "" status "${status_line}")
    string(STRIP "${status}" status)
    if(NOT "${status}" IN_LIST valid_statuses)
        message(FATAL_ERROR "人工清单状态无效：${relative_path}")
    endif()

    if("${status}" STREQUAL "真机验收通过")
        foreach(index RANGE 1 8)
            list(GET checklist_lines ${index} field_line)
            string(REGEX REPLACE "^[^:]+:[ \\t]*" "" field_value "${field_line}")
            zz_field_has_evidence("${field_value}" field_valid)
            if(NOT field_valid)
                message(FATAL_ERROR
                    "真机人工清单头字段不完整：${relative_path} -> ${field_line}")
            endif()
        endforeach()
    endif()

    set(check_count 0)
    foreach(line IN LISTS checklist_lines)
        if(NOT line MATCHES "^\\| \\[[ xX]\\] ")
            continue()
        endif()
        math(EXPR check_count "${check_count} + 1")
        string(REPLACE "|" ";" check_fields "${line}")
        list(LENGTH check_fields check_field_count)
        if(NOT check_field_count EQUAL 7)
            message(FATAL_ERROR "人工清单检查行列数无效：${relative_path}")
        endif()
        foreach(index RANGE 1 5)
            list(GET check_fields ${index} check_value)
            string(STRIP "${check_value}" check_value)
            set(check_${index} "${check_value}")
        endforeach()
        if("${status}" STREQUAL "真机验收通过")
            if(NOT "${check_1}" MATCHES "^\\[[xX]\\] ")
                message(FATAL_ERROR
                    "真机清单仍有未完成检查项：${relative_path} -> ${check_1}")
            endif()
            foreach(index RANGE 3 5)
                zz_field_has_evidence("${check_${index}}" check_value_valid)
                if(NOT check_value_valid)
                    message(FATAL_ERROR
                        "真机清单缺少实际结果、证据或问题记录：${relative_path}")
                endif()
            endforeach()
        elseif("${check_1}" MATCHES "^\\[[xX]\\] ")
            message(FATAL_ERROR
                "未签署的人工清单不得预先勾选：${relative_path}")
        endif()
    endforeach()
    if(check_count EQUAL 0)
        message(FATAL_ERROR "人工清单没有检查项：${relative_path}")
    endif()
    if(ZZ_REQUIRE_REAL_DEVICE
       AND NOT "${status}" STREQUAL "真机验收通过")
        message(FATAL_ERROR "发布候选仍缺少签署的人工清单：${relative_path}")
    endif()
endfunction()

foreach(checklist IN ITEMS
    docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md
    docs/release/MANUAL_MACOS_CHECKLIST_ZH.md
    docs/release/MANUAL_LINUX_CHECKLIST_ZH.md)
    zz_check_manual_checklist("${checklist}")
endforeach()

message(STATUS "构建、平台状态、人工清单与发布文档审计通过")
