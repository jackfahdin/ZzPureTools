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

function(zz_contract_metric_kind output unit)
    if(unit STREQUAL "ms" OR unit STREQUAL "us")
        set(kind statistical-duration)
    elseif(unit STREQUAL "count" OR unit STREQUAL "ratio")
        set(kind deterministic)
    elseif(unit STREQUAL "bytes" OR unit STREQUAL "percent")
        set(kind sampled-resource)
    else()
        message(FATAL_ERROR "Unsupported performance metric unit: ${unit}")
    endif()
    set(${output} "${kind}" PARENT_SCOPE)
endfunction()

function(zz_validate_formal_thresholds)
    set(threshold_file
        "${ZZ_SOURCE_DIR}/docs/performance/reference/linux/regression-thresholds.json")
    file(READ "${threshold_file}" thresholds_json)
    string(JSON threshold_schema GET "${thresholds_json}" schemaVersion)
    if(NOT threshold_schema EQUAL 2)
        message(FATAL_ERROR "Formal thresholds must use schemaVersion 2")
    endif()

    file(GLOB reporters LIST_DIRECTORIES FALSE
        "${ZZ_SOURCE_DIR}/docs/performance/reference/linux/*.json")
    list(REMOVE_ITEM reporters "${threshold_file}")
    set(expected_scenarios)
    foreach(reporter IN LISTS reporters)
        file(READ "${reporter}" reporter_json)
        string(JSON scenario GET "${reporter_json}" scenario)
        list(APPEND expected_scenarios "${scenario}")
    endforeach()
    list(SORT expected_scenarios)

    string(JSON scenario_count LENGTH "${thresholds_json}" scenarios)
    set(actual_scenarios)
    math(EXPR last_scenario "${scenario_count} - 1")
    foreach(scenario_index RANGE 0 ${last_scenario})
        string(JSON scenario MEMBER "${thresholds_json}" scenarios ${scenario_index})
        list(APPEND actual_scenarios "${scenario}")
    endforeach()
    list(SORT actual_scenarios)
    if(NOT "${actual_scenarios}" STREQUAL "${expected_scenarios}")
        message(FATAL_ERROR "Formal threshold scenarios differ from reporters")
    endif()

    foreach(reporter IN LISTS reporters)
        file(READ "${reporter}" reporter_json)
        string(JSON scenario GET "${reporter_json}" scenario)
        string(JSON reporter_metric_count LENGTH "${reporter_json}" metrics)
        string(JSON threshold_metric_count LENGTH "${thresholds_json}"
            scenarios "${scenario}" metrics)
        set(reporter_metrics)
        set(threshold_metrics)
        math(EXPR last_reporter_metric "${reporter_metric_count} - 1")
        math(EXPR last_threshold_metric "${threshold_metric_count} - 1")
        foreach(metric_index RANGE 0 ${last_reporter_metric})
            string(JSON metric MEMBER "${reporter_json}" metrics ${metric_index})
            list(APPEND reporter_metrics "${metric}")
        endforeach()
        foreach(metric_index RANGE 0 ${last_threshold_metric})
            string(JSON metric MEMBER "${thresholds_json}"
                scenarios "${scenario}" metrics ${metric_index})
            list(APPEND threshold_metrics "${metric}")
        endforeach()
        list(SORT reporter_metrics)
        list(SORT threshold_metrics)
        if(NOT "${reporter_metrics}" STREQUAL "${threshold_metrics}")
            message(FATAL_ERROR
                "Formal threshold metrics differ from reporter for ${scenario}")
        endif()

        foreach(metric IN LISTS reporter_metrics)
            string(JSON unit GET "${reporter_json}" metrics "${metric}" unit)
            zz_contract_metric_kind(expected_kind "${unit}")
            string(JSON actual_kind GET "${thresholds_json}"
                scenarios "${scenario}" metrics "${metric}" metricKind)
            if(NOT "${actual_kind}" STREQUAL "${expected_kind}")
                message(FATAL_ERROR
                    "Formal threshold kind mismatch for ${scenario}/${metric}")
            endif()
            foreach(field p95 max)
                string(JSON mode GET "${thresholds_json}" scenarios "${scenario}"
                    metrics "${metric}" "${field}" mode)
                string(JSON percent GET "${thresholds_json}" scenarios "${scenario}"
                    metrics "${metric}" "${field}" percent)
                if(expected_kind STREQUAL "statistical-duration")
                    if(field STREQUAL "max" AND NOT mode STREQUAL "observe")
                        message(FATAL_ERROR
                            "Duration max must observe for ${scenario}/${metric}")
                    endif()
                    if(field STREQUAL "p95" AND mode STREQUAL "gate"
                       AND percent GREATER 10)
                        message(FATAL_ERROR
                            "Duration P95 gate exceeds 10 percent for ${scenario}/${metric}")
                    endif()
                elseif(NOT mode STREQUAL "gate" OR percent GREATER 20)
                    message(FATAL_ERROR
                        "${expected_kind} must gate within 20 percent for ${scenario}/${metric}")
                endif()
            endforeach()
        endforeach()
    endforeach()
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
zz_validate_formal_thresholds()
string(JSON p95_regressed_json SET "${baseline_json}" metrics latency p95 111)
string(JSON max_regressed_json SET "${baseline_json}" metrics latency max 111)
file(WRITE "${ZZ_TEST_ROOT}/p95-regressed.json" "${p95_regressed_json}")
file(WRITE "${ZZ_TEST_ROOT}/max-regressed.json" "${max_regressed_json}")

set(duration_thresholds
    [=[{"schemaVersion":2,"scenarios":{"contract":{"metrics":{"latency":{"metricKind":"statistical-duration","p95":{"mode":"gate","percent":10},"max":{"mode":"observe","percent":10}}}}}}]=])
set(deterministic_thresholds
    [=[{"schemaVersion":2,"scenarios":{"contract":{"metrics":{"latency":{"metricKind":"deterministic","p95":{"mode":"gate","percent":10},"max":{"mode":"gate","percent":10}}}}}}]=])
set(resource_thresholds
    [=[{"schemaVersion":2,"scenarios":{"contract":{"metrics":{"latency":{"metricKind":"sampled-resource","p95":{"mode":"gate","percent":10},"max":{"mode":"gate","percent":10}}}}}}]=])
file(WRITE "${ZZ_TEST_ROOT}/duration.json" "${duration_thresholds}")
file(WRITE "${ZZ_TEST_ROOT}/deterministic.json" "${deterministic_thresholds}")
file(WRITE "${ZZ_TEST_ROOT}/resource.json" "${resource_thresholds}")

function(zz_run_comparison result output thresholds current)
    set(comparison_baseline "${baseline}")
    set(absolute_gate_proof "")
    if(ARGC GREATER 4)
        set(absolute_gate_proof "${ARGV4}")
    endif()
    if(ARGC GREATER 5)
        set(comparison_baseline "${ARGV5}")
    endif()
    set(compare_arguments
        "-DZZ_BASELINE=${comparison_baseline}"
        "-DZZ_CURRENT=${current}"
        "-DZZ_THRESHOLDS=${thresholds}")
    if(NOT "${absolute_gate_proof}" STREQUAL "")
        list(APPEND compare_arguments
            "-DZZ_ABSOLUTE_GATES_VERIFIED=${absolute_gate_proof}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            ${compare_arguments}
            -P "${compare_script}"
        RESULT_VARIABLE comparison_result
        OUTPUT_VARIABLE comparison_output
        ERROR_VARIABLE comparison_error)
    string(APPEND comparison_output "${comparison_error}")
    set(${result} "${comparison_result}" PARENT_SCOPE)
    set(${output} "${comparison_output}" PARENT_SCOPE)
endfunction()

function(zz_require_log output expected)
    string(FIND "${output}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "Comparison log lacks '${expected}': ${output}")
    endif()
endfunction()

zz_run_comparison(valid_result valid_output
    "${ZZ_TEST_ROOT}/duration.json" "${baseline}")
if(NOT valid_result EQUAL 0)
    message(FATAL_ERROR "Valid duration comparison failed: ${valid_output}")
endif()
zz_require_log("${valid_output}" "PASS contract/latency.p95")
zz_require_log("${valid_output}" "PASS contract/latency.max")
zz_require_log("${valid_output}" "PASS contract")

zz_run_comparison(observe_result observe_output
    "${ZZ_TEST_ROOT}/duration.json" "${ZZ_TEST_ROOT}/max-regressed.json")
if(NOT observe_result EQUAL 0)
    message(FATAL_ERROR "Duration max observation failed: ${observe_output}")
endif()
zz_require_log("${observe_output}" "OBSERVE contract/latency.max")
zz_require_log("${observe_output}" "OBSERVE contract")

zz_run_comparison(p95_result p95_output
    "${ZZ_TEST_ROOT}/duration.json" "${ZZ_TEST_ROOT}/p95-regressed.json")
if(p95_result EQUAL 0)
    message(FATAL_ERROR "Duration P95 gate accepted an 11 percent regression")
endif()
zz_require_log("${p95_output}" "FAIL contract/latency.p95")

foreach(kind IN ITEMS deterministic resource)
    zz_run_comparison(kind_result kind_output
        "${ZZ_TEST_ROOT}/${kind}.json" "${ZZ_TEST_ROOT}/max-regressed.json")
    if(kind_result EQUAL 0)
        message(FATAL_ERROR "${kind} max gate accepted an 11 percent regression")
    endif()
    zz_require_log("${kind_output}" "FAIL contract/latency.max")
endforeach()

string(JSON zero_resource_baseline SET "${baseline_json}"
    metrics latency unit [=["percent"]=])
string(JSON zero_resource_baseline SET "${zero_resource_baseline}"
    metrics latency p95 0)
string(JSON zero_resource_baseline SET "${zero_resource_baseline}"
    metrics latency max 0)
string(JSON zero_resource_current SET "${zero_resource_baseline}"
    metrics latency p95 0.033333)
string(JSON zero_resource_current SET "${zero_resource_current}"
    metrics latency max 0.033333)
file(WRITE "${ZZ_TEST_ROOT}/zero-resource-baseline.json"
    "${zero_resource_baseline}")
file(WRITE "${ZZ_TEST_ROOT}/zero-resource-current.json"
    "${zero_resource_current}")

zz_run_comparison(zero_resource_result zero_resource_output
    "${ZZ_TEST_ROOT}/resource.json"
    "${ZZ_TEST_ROOT}/zero-resource-current.json"
    "" "${ZZ_TEST_ROOT}/zero-resource-baseline.json")
if(zero_resource_result EQUAL 0)
    message(FATAL_ERROR
        "Sampled resource zero baseline accepted without absolute proof")
endif()
zz_require_log("${zero_resource_output}" "FAIL contract/latency.p95")

zz_run_comparison(zero_verified_result zero_verified_output
    "${ZZ_TEST_ROOT}/resource.json"
    "${ZZ_TEST_ROOT}/zero-resource-current.json"
    TRUE "${ZZ_TEST_ROOT}/zero-resource-baseline.json")
if(NOT zero_verified_result EQUAL 0)
    message(FATAL_ERROR
        "Sampled resource zero baseline was not accepted after absolute proof: "
        "${zero_verified_output}")
endif()
zz_require_log("${zero_verified_output}"
    "zero-baseline=absolute-budget-verified")

zz_run_comparison(zero_deterministic_result zero_deterministic_output
    "${ZZ_TEST_ROOT}/deterministic.json"
    "${ZZ_TEST_ROOT}/zero-resource-current.json"
    TRUE "${ZZ_TEST_ROOT}/zero-resource-baseline.json")
if(zero_deterministic_result EQUAL 0)
    message(FATAL_ERROR
        "Deterministic zero baseline bypassed by absolute proof")
endif()
zz_require_log("${zero_deterministic_output}" "FAIL contract/latency.p95")

function(zz_require_invalid name thresholds expected)
    file(WRITE "${ZZ_TEST_ROOT}/${name}.json" "${thresholds}")
    zz_run_comparison(invalid_result invalid_output
        "${ZZ_TEST_ROOT}/${name}.json" "${baseline}")
    if(invalid_result EQUAL 0)
        message(FATAL_ERROR "Invalid policy ${name} was accepted")
    endif()
    zz_require_log("${invalid_output}" "${expected}")
endfunction()

string(JSON invalid_schema SET "${duration_thresholds}" schemaVersion 1)
zz_require_invalid(invalid-schema "${invalid_schema}" "INVALID thresholds")
string(JSON string_schema SET "${duration_thresholds}" schemaVersion [=["2"]=])
zz_require_invalid(string-schema "${string_schema}" "INVALID thresholds")
string(JSON missing_kind REMOVE "${duration_thresholds}"
    scenarios contract metrics latency metricKind)
zz_require_invalid(missing-kind "${missing_kind}" "INVALID contract/latency")
string(JSON unknown_kind SET "${duration_thresholds}"
    scenarios contract metrics latency metricKind [=["unknown"]=])
zz_require_invalid(unknown-kind "${unknown_kind}" "INVALID contract/latency")
string(JSON duration_max_gate SET "${duration_thresholds}"
    scenarios contract metrics latency max mode [=["gate"]=])
zz_require_invalid(duration-max-gate "${duration_max_gate}" "INVALID contract/latency")
string(JSON duration_p95_percent SET "${duration_thresholds}"
    scenarios contract metrics latency p95 percent 11)
zz_require_invalid(duration-p95-percent "${duration_p95_percent}" "INVALID contract/latency")
string(JSON deterministic_observe SET "${deterministic_thresholds}"
    scenarios contract metrics latency max mode [=["observe"]=])
zz_require_invalid(deterministic-observe "${deterministic_observe}" "INVALID contract/latency")
string(JSON resource_observe SET "${resource_thresholds}"
    scenarios contract metrics latency p95 mode [=["observe"]=])
zz_require_invalid(resource-observe "${resource_observe}" "INVALID contract/latency")
string(JSON deterministic_percent SET "${deterministic_thresholds}"
    scenarios contract metrics latency max percent 21)
zz_require_invalid(deterministic-percent "${deterministic_percent}" "INVALID contract/latency")

zz_run_comparison(environment_result environment_output
    "${ZZ_TEST_ROOT}/duration.json"
    "${ZZ_SOURCE_DIR}/benchmarks/testdata/performance-mismatched-environment.json")
if(environment_result EQUAL 0)
    message(FATAL_ERROR "Environment mismatch was accepted")
endif()
zz_require_log("${environment_output}" "INVALID contract environment;gpu")

foreach(round RANGE 1 3)
    set(round_directory "${ZZ_TEST_ROOT}/round-${round}")
    file(MAKE_DIRECTORY "${round_directory}")
    math(EXPR value "90 + ${round} * 10")
    string(REPLACE "\"p95\": 100" "\"p95\": ${value}" round_json "${baseline_json}")
    string(REPLACE "\"max\": 100" "\"max\": ${value}" round_json "${round_json}")
    string(JSON round_json SET "${round_json}" metrics count
        [=[{"unit":"count","p95":100,"max":100}]=])
    string(JSON round_json SET "${round_json}" metrics bytes
        [=[{"unit":"bytes","p95":100,"max":100}]=])
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
string(JSON candidate_schema GET "${candidate_json}" schemaVersion)
string(JSON latency_kind GET "${candidate_json}"
    scenarios contract metrics latency metricKind)
string(JSON p95_mode GET "${candidate_json}"
    scenarios contract metrics latency p95 mode)
string(JSON p95_percent GET "${candidate_json}"
    scenarios contract metrics latency p95 percent)
string(JSON p95_noise GET "${candidate_json}"
    scenarios contract metrics latency p95 noisePercent)
string(JSON max_mode GET "${candidate_json}"
    scenarios contract metrics latency max mode)
string(JSON max_percent GET "${candidate_json}"
    scenarios contract metrics latency max percent)
string(JSON max_noise GET "${candidate_json}"
    scenarios contract metrics latency max noisePercent)
string(JSON count_kind GET "${candidate_json}"
    scenarios contract metrics count metricKind)
string(JSON bytes_kind GET "${candidate_json}"
    scenarios contract metrics bytes metricKind)
if(NOT candidate_schema EQUAL 2
   OR NOT latency_kind STREQUAL "statistical-duration"
   OR NOT p95_mode STREQUAL "observe"
   OR NOT p95_percent EQUAL 20
   OR NOT p95_noise EQUAL 20
   OR NOT max_mode STREQUAL "observe"
   OR NOT max_percent EQUAL 20
   OR NOT max_noise EQUAL 20
   OR NOT count_kind STREQUAL "deterministic"
   OR NOT bytes_kind STREQUAL "sampled-resource")
    message(FATAL_ERROR
        "Noise analyzer returned an unexpected schema v2 policy")
endif()

file(READ "${ZZ_TEST_ROOT}/round-3/benchmark.contract.json" noisy_count_json)
string(JSON noisy_count_json SET "${noisy_count_json}" metrics count p95 121)
string(JSON noisy_count_json SET "${noisy_count_json}" metrics count max 121)
file(WRITE "${ZZ_TEST_ROOT}/round-3/benchmark.contract.json" "${noisy_count_json}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_RUN_DIRECTORIES=${run_directories}"
        "-DZZ_OUTPUT_JSON=${ZZ_TEST_ROOT}/noisy-candidate.json"
        "-DZZ_OUTPUT_MARKDOWN=${ZZ_TEST_ROOT}/noisy-candidate.md"
        -P "${analyze_script}"
    RESULT_VARIABLE noisy_count_result
    ERROR_VARIABLE noisy_count_error)
if(noisy_count_result EQUAL 0)
    message(FATAL_ERROR
        "Noise analyzer accepted a deterministic 21 percent stability band")
endif()
string(FIND "${noisy_count_error}"
    "deterministic metric exceeds the acceptable 20 percent stability band"
    noisy_count_error_index)
if(noisy_count_error_index EQUAL -1)
    message(FATAL_ERROR "Noise analyzer reported the wrong deterministic failure")
endif()

file(READ "${ZZ_TEST_ROOT}/round-3/benchmark.contract.json" high_noise_json)
string(JSON high_noise_json SET "${high_noise_json}" metrics count p95 100)
string(JSON high_noise_json SET "${high_noise_json}" metrics count max 100)
string(JSON high_noise_json SET "${high_noise_json}" metrics latency p95 201)
file(WRITE "${ZZ_TEST_ROOT}/round-3/benchmark.contract.json" "${high_noise_json}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_RUN_DIRECTORIES=${run_directories}"
        "-DZZ_OUTPUT_JSON=${ZZ_TEST_ROOT}/high-noise-candidate.json"
        "-DZZ_OUTPUT_MARKDOWN=${ZZ_TEST_ROOT}/high-noise-candidate.md"
        -P "${analyze_script}"
    RESULT_VARIABLE high_noise_result
    ERROR_VARIABLE high_noise_error)
if(NOT high_noise_result EQUAL 0)
    message(FATAL_ERROR "Noise analyzer rejected a high-noise duration: ${high_noise_error}")
endif()
file(READ "${ZZ_TEST_ROOT}/high-noise-candidate.json" high_noise_candidate_json)
string(JSON high_noise_percent GET "${high_noise_candidate_json}"
    scenarios contract metrics latency p95 percent)
string(JSON high_noise_percent_raw GET "${high_noise_candidate_json}"
    scenarios contract metrics latency p95 noisePercent)
if(NOT high_noise_percent EQUAL 100 OR NOT high_noise_percent_raw EQUAL 101)
    message(FATAL_ERROR
        "High-noise duration must clamp percent to 100 and retain noisePercent 101")
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
    "${ZZ_SOURCE_DIR}/docs/performance/evidence/workspace-components/2026-08-26")
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
