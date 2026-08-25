cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_SOURCE_DIR ZZ_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

set(compare_script "${ZZ_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake")
set(analyze_script "${ZZ_SOURCE_DIR}/scripts/ci/ZzAnalyzePerformanceNoise.cmake")
set(baseline "${ZZ_SOURCE_DIR}/benchmarks/testdata/performance-valid.json")
file(REMOVE_RECURSE "${ZZ_TEST_ROOT}")
file(MAKE_DIRECTORY "${ZZ_TEST_ROOT}")

function(zz_contract_decimal_to_micro output value)
    if(NOT "${value}" MATCHES "^([0-9]+)(\\.([0-9]+))?$")
        set(${output} "" PARENT_SCOPE)
        return()
    endif()
    set(whole "${CMAKE_MATCH_1}")
    set(fraction "${CMAKE_MATCH_3}")
    string(APPEND fraction "000000")
    string(SUBSTRING "${fraction}" 0 6 fraction)
    math(EXPR micro "${whole} * 1000000 + ${fraction}")
    set(${output} "${micro}" PARENT_SCOPE)
endfunction()

function(zz_validate_workspace_evidence output directory)
    file(GLOB round_reports LIST_DIRECTORIES FALSE
        "${directory}/round-*.json")
    list(SORT round_reports)
    list(LENGTH round_reports round_count)
    if(NOT round_count EQUAL 3)
        set(${output} "workspace evidence requires exactly three rounds" PARENT_SCOPE)
        return()
    endif()

    set(expected_reports)
    foreach(round RANGE 1 3)
        set(report "${directory}/round-${round}.json")
        if(NOT EXISTS "${report}")
            set(${output} "missing workspace evidence ${report}" PARENT_SCOPE)
            return()
        endif()
        list(APPEND expected_reports "${report}")
    endforeach()
    if(NOT "${round_reports}" STREQUAL "${expected_reports}")
        set(${output} "workspace evidence round names are not canonical" PARENT_SCOPE)
        return()
    endif()

    set(required_metrics
        panel-toggle-time
        group-structure-time
        workspace-paint-4-groups-time
        workspace-paint-32-groups-time
        workspace-render-time
        marker-paint-20-time
        marker-paint-100000-time
        object-count
        timer-count
        animation-count
        rss-bytes)
    set(count_metrics
        object-count timer-count animation-count)
    list(GET expected_reports 0 reference_report)
    file(READ "${reference_report}" reference_json)
    foreach(report IN LISTS expected_reports)
        file(READ "${report}" report_json)
        string(JSON scenario ERROR_VARIABLE scenario_error GET
            "${report_json}" scenario)
        if(NOT "${scenario_error}" STREQUAL "NOTFOUND"
           OR NOT "${scenario}" STREQUAL "workspace-components")
            set(${output} "invalid workspace scenario in ${report}" PARENT_SCOPE)
            return()
        endif()

        foreach(fingerprint_path IN ITEMS
            "schemaVersion"
            "environment;cpu"
            "environment;memoryBytes"
            "environment;os"
            "environment;gpu"
            "environment;runnerImageDigest"
            "environment;qtVersion"
            "environment;compiler"
            "environment;windowSystem"
            "environment;dpr"
            "environment;refreshRateHz"
            "build;commit"
            "build;preset"
            "build;buildType"
            "build;shared"
            "build;lto"
            "build;sanitizers")
            string(JSON reference_value ERROR_VARIABLE reference_error GET
                "${reference_json}" ${fingerprint_path})
            string(JSON report_value ERROR_VARIABLE report_error GET
                "${report_json}" ${fingerprint_path})
            if(NOT "${reference_error}" STREQUAL "NOTFOUND"
               OR NOT "${report_error}" STREQUAL "NOTFOUND"
               OR NOT "${report_value}" STREQUAL "${reference_value}")
                set(${output}
                    "workspace fingerprint mismatch at ${fingerprint_path} in ${report}"
                    PARENT_SCOPE)
                return()
            endif()
        endforeach()

        foreach(metric IN LISTS required_metrics)
            string(JSON metric_type ERROR_VARIABLE metric_error TYPE
                "${report_json}" metrics "${metric}")
            if(NOT "${metric_error}" STREQUAL "NOTFOUND"
               OR NOT "${metric_type}" STREQUAL "OBJECT")
                set(${output} "missing workspace metric ${metric} in ${report}"
                    PARENT_SCOPE)
                return()
            endif()
            string(JSON unit ERROR_VARIABLE unit_error GET
                "${report_json}" metrics "${metric}" unit)
            list(FIND count_metrics "${metric}" count_metric_index)
            if(count_metric_index EQUAL -1)
                set(expected_unit ms)
                if(metric STREQUAL "rss-bytes")
                    set(expected_unit bytes)
                endif()
            else()
                set(expected_unit count)
            endif()
            if(NOT "${unit_error}" STREQUAL "NOTFOUND"
               OR NOT "${unit}" STREQUAL "${expected_unit}")
                set(${output} "unexpected unit for ${metric} in ${report}"
                    PARENT_SCOPE)
                return()
            endif()
            foreach(field count p50 p95 max)
                string(JSON field_type ERROR_VARIABLE field_error TYPE
                    "${report_json}" metrics "${metric}" "${field}")
                if(NOT "${field_error}" STREQUAL "NOTFOUND"
                   OR NOT "${field_type}" STREQUAL "NUMBER")
                    set(${output}
                        "workspace metric ${metric}.${field} must be numeric in ${report}"
                        PARENT_SCOPE)
                    return()
                endif()
            endforeach()
            string(JSON sample_count GET
                "${report_json}" metrics "${metric}" count)
            if(NOT sample_count EQUAL 80)
                set(${output} "workspace metric ${metric} requires 80 samples in ${report}"
                    PARENT_SCOPE)
                return()
            endif()
        endforeach()

        foreach(metric panel-toggle-time group-structure-time)
            string(JSON structure_p95 GET
                "${report_json}" metrics "${metric}" p95)
            if("${structure_p95}" GREATER 16.7)
                set(${output} "${metric} P95 exceeds 16.7 ms in ${report}"
                    PARENT_SCOPE)
                return()
            endif()
        endforeach()
        string(JSON render_p95 GET
            "${report_json}" metrics workspace-render-time p95)
        if("${render_p95}" GREATER 12)
            set(${output} "workspace-render-time P95 exceeds 12 ms in ${report}"
                PARENT_SCOPE)
            return()
        endif()

        foreach(ratio_pair IN ITEMS
            "workspace-paint-4-groups-time;workspace-paint-32-groups-time"
            "marker-paint-20-time;marker-paint-100000-time")
            list(GET ratio_pair 0 small_metric)
            list(GET ratio_pair 1 large_metric)
            string(JSON small_p95 GET
                "${report_json}" metrics "${small_metric}" p95)
            string(JSON large_p95 GET
                "${report_json}" metrics "${large_metric}" p95)
            zz_contract_decimal_to_micro(small_micro "${small_p95}")
            zz_contract_decimal_to_micro(large_micro "${large_p95}")
            if("${small_micro}" STREQUAL "" OR "${large_micro}" STREQUAL "")
                set(${output} "invalid paint ratio input in ${report}" PARENT_SCOPE)
                return()
            endif()
            math(EXPR ratio_limit "${small_micro} * 2")
            if(small_micro EQUAL 0 OR large_micro GREATER ratio_limit)
                set(${output}
                    "${large_metric}/${small_metric} P95 ratio exceeds 2.0 in ${report}"
                    PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endforeach()

    set(${output} "" PARENT_SCOPE)
endfunction()

file(READ "${baseline}" baseline_json)
set(gate_thresholds
    [=[{"schemaVersion":1,"scenarios":{"contract":{"metrics":{"latency":{"p95":{"mode":"gate","percent":10},"max":{"mode":"gate","percent":10}}}}}}]=])
set(observe_thresholds
    [=[{"schemaVersion":1,"scenarios":{"contract":{"metrics":{"latency":{"p95":{"mode":"observe","percent":10},"max":{"mode":"observe","percent":10}}}}}}]=])
file(WRITE "${ZZ_TEST_ROOT}/gate.json" "${gate_thresholds}")
file(WRITE "${ZZ_TEST_ROOT}/observe.json" "${observe_thresholds}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${baseline}"
        "-DZZ_CURRENT=${baseline}"
        "-DZZ_THRESHOLDS=${ZZ_TEST_ROOT}/gate.json"
        -P "${compare_script}"
    RESULT_VARIABLE valid_result
    ERROR_VARIABLE valid_error)
if(NOT valid_result EQUAL 0)
    message(FATAL_ERROR "Valid threshold comparison failed: ${valid_error}")
endif()

string(REPLACE "\"p95\": 100" "\"p95\": 111" regressed_json "${baseline_json}")
string(REPLACE "\"max\": 100" "\"max\": 111" regressed_json "${regressed_json}")
file(WRITE "${ZZ_TEST_ROOT}/regressed.json" "${regressed_json}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${baseline}"
        "-DZZ_CURRENT=${ZZ_TEST_ROOT}/regressed.json"
        "-DZZ_THRESHOLDS=${ZZ_TEST_ROOT}/gate.json"
        -P "${compare_script}"
    RESULT_VARIABLE gate_result)
if(gate_result EQUAL 0)
    message(FATAL_ERROR "Gate mode accepted an 11 percent regression")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${baseline}"
        "-DZZ_CURRENT=${ZZ_TEST_ROOT}/regressed.json"
        "-DZZ_THRESHOLDS=${ZZ_TEST_ROOT}/observe.json"
        -P "${compare_script}"
    RESULT_VARIABLE observe_result
    ERROR_VARIABLE observe_error)
if(NOT observe_result EQUAL 0 OR NOT observe_error MATCHES "OBSERVE")
    message(FATAL_ERROR
        "Observe mode did not report and accept the regression: ${observe_error}")
endif()

foreach(round RANGE 1 3)
    set(round_directory "${ZZ_TEST_ROOT}/round-${round}")
    file(MAKE_DIRECTORY "${round_directory}")
    math(EXPR value "90 + ${round} * 10")
    string(REPLACE "\"p95\": 100" "\"p95\": ${value}" round_json "${baseline_json}")
    string(REPLACE "\"max\": 100" "\"max\": ${value}" round_json "${round_json}")
    file(WRITE "${round_directory}/benchmark.contract.json" "${round_json}")
endforeach()
set(run_directories
    "${ZZ_TEST_ROOT}/round-1;${ZZ_TEST_ROOT}/round-2;${ZZ_TEST_ROOT}/round-3")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_RUN_DIRECTORIES=${run_directories}"
        "-DZZ_OUTPUT_JSON=${ZZ_TEST_ROOT}/candidate.json"
        "-DZZ_OUTPUT_MARKDOWN=${ZZ_TEST_ROOT}/candidate.md"
        -P "${analyze_script}"
    RESULT_VARIABLE analyze_result
    ERROR_VARIABLE analyze_error)
if(NOT analyze_result EQUAL 0)
    message(FATAL_ERROR "Noise analyzer failed: ${analyze_error}")
endif()
file(READ "${ZZ_TEST_ROOT}/candidate.json" candidate_json)
string(JSON p95_mode GET "${candidate_json}"
    scenarios contract metrics latency p95 mode)
string(JSON p95_percent GET "${candidate_json}"
    scenarios contract metrics latency p95 percent)
string(JSON p95_noise GET "${candidate_json}"
    scenarios contract metrics latency p95 noisePercent)
if(NOT p95_mode STREQUAL "gate"
   OR NOT p95_percent EQUAL 20
   OR NOT p95_noise EQUAL 20)
    message(FATAL_ERROR
        "Noise analyzer returned an unexpected 20 percent policy")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_RUN_DIRECTORIES=${ZZ_TEST_ROOT}/round-1;${ZZ_TEST_ROOT}/round-2"
        "-DZZ_OUTPUT_JSON=${ZZ_TEST_ROOT}/invalid.json"
        "-DZZ_OUTPUT_MARKDOWN=${ZZ_TEST_ROOT}/invalid.md"
        -P "${analyze_script}"
    RESULT_VARIABLE insufficient_result)
if(insufficient_result EQUAL 0)
    message(FATAL_ERROR "Noise analyzer accepted fewer than three rounds")
endif()

set(workspace_evidence_directory
    "${ZZ_SOURCE_DIR}/docs/performance/evidence/workspace-components/2026-08-22")
file(READ "${workspace_evidence_directory}/round-1.json" workspace_fixture)
string(JSON workspace_fixture SET "${workspace_fixture}"
    build preset [=["linux-gcc-reference"]=])
foreach(metric IN ITEMS
    panel-toggle-time
    group-structure-time
    workspace-paint-4-groups-time
    workspace-paint-32-groups-time
    marker-paint-20-time
    marker-paint-100000-time)
    string(JSON workspace_fixture SET "${workspace_fixture}" metrics "${metric}"
        [=[{"unit":"ms","count":80,"p50":1,"p95":1,"max":1}]=])
endforeach()

set(valid_workspace_directory "${ZZ_TEST_ROOT}/workspace-valid")
file(MAKE_DIRECTORY "${valid_workspace_directory}")
foreach(round RANGE 1 3)
    file(WRITE "${valid_workspace_directory}/round-${round}.json"
        "${workspace_fixture}\n")
endforeach()
zz_validate_workspace_evidence(valid_workspace_error
    "${valid_workspace_directory}")
if(NOT "${valid_workspace_error}" STREQUAL "")
    message(FATAL_ERROR
        "Workspace evidence validator rejected valid fixture: ${valid_workspace_error}")
endif()

set(insufficient_workspace_directory "${ZZ_TEST_ROOT}/workspace-two-rounds")
file(MAKE_DIRECTORY "${insufficient_workspace_directory}")
foreach(round RANGE 1 2)
    file(WRITE "${insufficient_workspace_directory}/round-${round}.json"
        "${workspace_fixture}\n")
endforeach()
zz_validate_workspace_evidence(insufficient_workspace_error
    "${insufficient_workspace_directory}")
if("${insufficient_workspace_error}" STREQUAL "")
    message(FATAL_ERROR "Workspace evidence validator accepted two rounds")
endif()

set(missing_metric_directory "${ZZ_TEST_ROOT}/workspace-missing-metric")
file(MAKE_DIRECTORY "${missing_metric_directory}")
foreach(round RANGE 1 3)
    set(round_fixture "${workspace_fixture}")
    if(round EQUAL 2)
        string(JSON round_fixture REMOVE "${round_fixture}"
            metrics panel-toggle-time)
    endif()
    file(WRITE "${missing_metric_directory}/round-${round}.json"
        "${round_fixture}\n")
endforeach()
zz_validate_workspace_evidence(missing_metric_error
    "${missing_metric_directory}")
if("${missing_metric_error}" STREQUAL "")
    message(FATAL_ERROR "Workspace evidence validator accepted a missing metric")
endif()

set(excessive_ratio_directory "${ZZ_TEST_ROOT}/workspace-excessive-ratio")
file(MAKE_DIRECTORY "${excessive_ratio_directory}")
foreach(round RANGE 1 3)
    set(round_fixture "${workspace_fixture}")
    string(JSON round_fixture SET "${round_fixture}"
        metrics marker-paint-100000-time p95 2.1)
    string(JSON round_fixture SET "${round_fixture}"
        metrics marker-paint-100000-time max 2.1)
    file(WRITE "${excessive_ratio_directory}/round-${round}.json"
        "${round_fixture}\n")
endforeach()
zz_validate_workspace_evidence(excessive_ratio_error
    "${excessive_ratio_directory}")
if("${excessive_ratio_error}" STREQUAL "")
    message(FATAL_ERROR "Workspace evidence validator accepted a paint ratio above 2.0")
endif()

zz_validate_workspace_evidence(workspace_evidence_error
    "${workspace_evidence_directory}")
if(NOT "${workspace_evidence_error}" STREQUAL "")
    message(FATAL_ERROR
        "Workspace reference evidence is invalid: ${workspace_evidence_error}")
endif()

message(STATUS "Performance threshold contract passed")
