foreach(required ZZ_BASELINE ZZ_CURRENT ZZ_THRESHOLDS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "Performance report does not exist: ${${required}}")
    endif()
endforeach()
if(NOT EXISTS "${ZZ_THRESHOLDS}")
    message(FATAL_ERROR "Performance threshold file does not exist: ${ZZ_THRESHOLDS}")
endif()

file(READ "${ZZ_BASELINE}" baseline_json)
file(READ "${ZZ_CURRENT}" current_json)
file(READ "${ZZ_THRESHOLDS}" thresholds_json)

function(zz_json_get output json_text label)
    string(JSON value ERROR_VARIABLE json_error GET
        "${json_text}" ${ARGN})
    if(NOT "${json_error}" STREQUAL "NOTFOUND")
        message(FATAL_ERROR
            "${label} has invalid/missing JSON path ${ARGN}: ${json_error}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(zz_json_require_type json_text label expected_type)
    string(JSON actual_type ERROR_VARIABLE json_error TYPE
        "${json_text}" ${ARGN})
    if(NOT "${json_error}" STREQUAL "NOTFOUND"
       OR NOT "${actual_type}" STREQUAL "${expected_type}")
        message(FATAL_ERROR
            "${label} JSON path ${ARGN} must be ${expected_type}")
    endif()
endfunction()

function(zz_require_commit json_text label)
    zz_json_get(commit "${json_text}" "${label}" build commit)
    string(LENGTH "${commit}" commit_length)
    if(NOT commit_length EQUAL 40
       OR NOT "${commit}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "${label} build.commit must be 40 lowercase hex characters")
    endif()
endfunction()

function(zz_decimal_to_micro output input)
    if(NOT "${input}" MATCHES "^([0-9]+)(\.([0-9]+))?$")
        message(FATAL_ERROR "Not a non-negative decimal: ${input}")
    endif()
    set(whole "${CMAKE_MATCH_1}")
    set(fraction "${CMAKE_MATCH_3}")
    string(REGEX REPLACE "^0+" "" normalized_whole "${whole}")
    if("${normalized_whole}" STREQUAL "")
        set(normalized_whole 0)
    endif()
    string(APPEND fraction "000000")
    string(SUBSTRING "${fraction}" 0 6 fraction)

    string(LENGTH "${normalized_whole}" whole_length)
    if(whole_length GREATER 13
       OR (whole_length EQUAL 13
           AND "${normalized_whole}" STRGREATER "9223372036854")
       OR ("${normalized_whole}" STREQUAL "9223372036854"
           AND "${fraction}" STRGREATER "775807"))
        message(FATAL_ERROR
            "Decimal exceeds signed 64-bit millionths: ${input}")
    endif()

    math(EXPR micro "${normalized_whole} * 1000000 + ${fraction}")
    set(${output} "${micro}" PARENT_SCOPE)
endfunction()

function(zz_assert_regression metric field baseline_value current_value mode percent)
    zz_decimal_to_micro(baseline_micro "${baseline_value}")
    zz_decimal_to_micro(current_micro "${current_value}")
    math(EXPR whole_increment
        "(${baseline_micro} / 100) * ${percent}")
    math(EXPR remainder_increment
        "((${baseline_micro} % 100) * ${percent}) / 100")
    set(maximum_signed_64 9223372036854775807)
    math(EXPR available_increment
        "${maximum_signed_64} - ${baseline_micro}")
    if("${whole_increment}" GREATER "${available_increment}")
        set(allowed_micro "${maximum_signed_64}")
    else()
        math(EXPR remaining_increment
            "${available_increment} - ${whole_increment}")
        if("${remainder_increment}" GREATER "${remaining_increment}")
            set(allowed_micro "${maximum_signed_64}")
        else()
            math(EXPR allowed_micro
                "${baseline_micro} + ${whole_increment} + ${remainder_increment}")
        endif()
    endif()
    if("${current_micro}" GREATER "${allowed_micro}")
        if("${mode}" STREQUAL "observe")
            message(WARNING
                "OBSERVE ${metric}.${field} changed from ${baseline_value} "
                "to ${current_value}; recorded band is ${percent}%")
        else()
            message(FATAL_ERROR
                "${metric}.${field} regressed from ${baseline_value} "
                "to ${current_value}; limit is ${percent}%")
        endif()
    endif()
endfunction()

zz_json_get(baseline_schema "${baseline_json}" "Baseline" schemaVersion)
zz_json_get(current_schema "${current_json}" "Current" schemaVersion)
zz_json_get(baseline_scenario "${baseline_json}" "Baseline" scenario)
zz_json_get(current_scenario "${current_json}" "Current" scenario)
zz_json_get(threshold_schema "${thresholds_json}" "Thresholds" schemaVersion)
if(NOT "${baseline_schema}" STREQUAL "1"
   OR NOT "${current_schema}" STREQUAL "${baseline_schema}"
   OR NOT "${threshold_schema}" STREQUAL "1"
   OR "${baseline_scenario}" STREQUAL ""
   OR NOT "${current_scenario}" STREQUAL "${baseline_scenario}")
    message(FATAL_ERROR
        "Baseline/current schema or scenario mismatch")
endif()

zz_require_commit("${baseline_json}" "Baseline")
zz_require_commit("${current_json}" "Current")

foreach(fingerprint_path IN ITEMS
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
    "build;buildType"
    "build;shared"
    "build;lto"
    "build;sanitizers")
    zz_json_get(baseline_value "${baseline_json}" "Baseline"
        ${fingerprint_path})
    zz_json_get(current_value "${current_json}" "Current"
        ${fingerprint_path})
    if(NOT "${current_value}" STREQUAL "${baseline_value}")
        message(FATAL_ERROR
            "Fingerprint mismatch at ${fingerprint_path}: "
            "baseline=${baseline_value}, current=${current_value}")
    endif()
endforeach()

zz_json_get(baseline_preset "${baseline_json}" "Baseline" build preset)
zz_json_get(current_preset "${current_json}" "Current" build preset)
if("${baseline_preset}" STREQUAL ""
   OR "${current_preset}" STREQUAL "")
    message(FATAL_ERROR "build.preset must not be empty")
endif()
if(NOT "${baseline_preset}" STREQUAL "${current_preset}"
   AND NOT ("${baseline_preset}" STREQUAL "linux-gcc-reference"
            AND "${current_preset}" STREQUAL "linux-gcc-benchmarks"))
    message(FATAL_ERROR
        "Incompatible presets: ${baseline_preset} -> ${current_preset}")
endif()

string(JSON metric_count ERROR_VARIABLE metric_count_error LENGTH
    "${baseline_json}" metrics)
if(NOT "${metric_count_error}" STREQUAL "NOTFOUND"
   OR metric_count LESS 1)
    message(FATAL_ERROR "Baseline metrics must be a non-empty object")
endif()
math(EXPR last_metric_index "${metric_count} - 1")
foreach(metric_index RANGE 0 ${last_metric_index})
    string(JSON metric ERROR_VARIABLE metric_error MEMBER
        "${baseline_json}" metrics ${metric_index})
    if(NOT "${metric_error}" STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Failed to enumerate baseline metrics")
    endif()
    zz_json_require_type("${baseline_json}" "Baseline" OBJECT metrics
        "${metric}")
    zz_json_require_type("${current_json}" "Current" OBJECT metrics
        "${metric}")
    zz_json_require_type("${baseline_json}" "Baseline" STRING metrics
        "${metric}" unit)
    zz_json_require_type("${current_json}" "Current" STRING metrics
        "${metric}" unit)
    zz_json_get(baseline_unit "${baseline_json}" "Baseline" metrics
        "${metric}" unit)
    zz_json_get(current_unit "${current_json}" "Current" metrics
        "${metric}" unit)
    if(NOT "${current_unit}" STREQUAL "${baseline_unit}")
        message(FATAL_ERROR
            "Metric unit mismatch for ${metric}: "
            "${baseline_unit} != ${current_unit}")
    endif()

    foreach(field p95 max)
        zz_json_require_type("${baseline_json}" "Baseline" NUMBER metrics
            "${metric}" "${field}")
        zz_json_require_type("${current_json}" "Current" NUMBER metrics
            "${metric}" "${field}")
        zz_json_get(baseline_value "${baseline_json}" "Baseline" metrics
            "${metric}" "${field}")
        zz_json_get(current_value "${current_json}" "Current" metrics
            "${metric}" "${field}")
        zz_json_require_type("${thresholds_json}" "Thresholds" OBJECT
            scenarios "${current_scenario}" metrics "${metric}" "${field}")
        zz_json_require_type("${thresholds_json}" "Thresholds" STRING
            scenarios "${current_scenario}" metrics "${metric}" "${field}" mode)
        zz_json_require_type("${thresholds_json}" "Thresholds" NUMBER
            scenarios "${current_scenario}" metrics "${metric}" "${field}" percent)
        zz_json_get(mode "${thresholds_json}" "Thresholds"
            scenarios "${current_scenario}" metrics "${metric}" "${field}" mode)
        zz_json_get(percent "${thresholds_json}" "Thresholds"
            scenarios "${current_scenario}" metrics "${metric}" "${field}" percent)
        if(NOT "${mode}" STREQUAL "gate" AND NOT "${mode}" STREQUAL "observe")
            message(FATAL_ERROR
                "Threshold mode for ${metric}.${field} must be gate or observe")
        endif()
        if(NOT "${percent}" MATCHES "^[0-9]+$" OR "${percent}" GREATER 100)
            message(FATAL_ERROR
                "Threshold percent for ${metric}.${field} must be an integer from 0 to 100")
        endif()
        if("${mode}" STREQUAL "gate" AND "${percent}" GREATER 20)
            message(FATAL_ERROR
                "Gate threshold for ${metric}.${field} must not exceed 20%")
        endif()
        zz_assert_regression(
            "${metric}" "${field}" "${baseline_value}" "${current_value}"
            "${mode}" "${percent}")
    endforeach()
endforeach()

message(STATUS
    "Performance comparison passed for ${baseline_scenario} using ${ZZ_THRESHOLDS}")
