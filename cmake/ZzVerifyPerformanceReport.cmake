foreach(required ZZ_REPORT ZZ_SCENARIO ZZ_METRIC)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
if(NOT DEFINED ZZ_MAX_P95 AND NOT DEFINED ZZ_MAX_VALUE)
    message(FATAL_ERROR "At least one of ZZ_MAX_P95/ZZ_MAX_VALUE is required")
endif()
if(NOT EXISTS "${ZZ_REPORT}")
    message(FATAL_ERROR "Performance report does not exist: ${ZZ_REPORT}")
endif()
file(READ "${ZZ_REPORT}" report_json)

function(zz_json_get output)
    string(JSON value ERROR_VARIABLE json_error GET "${report_json}" ${ARGN})
    if(NOT "${json_error}" STREQUAL "NOTFOUND")
        message(FATAL_ERROR "Invalid/missing JSON path ${ARGN}: ${json_error}")
    endif()
    set(${output} "${value}" PARENT_SCOPE)
endfunction()

zz_json_get(schema_version schemaVersion)
zz_json_get(scenario scenario)
if(NOT schema_version EQUAL 1 OR NOT "${scenario}" STREQUAL "${ZZ_SCENARIO}")
    message(FATAL_ERROR
        "Unexpected schema/scenario: ${schema_version}/${scenario}")
endif()

function(zz_require_commit json_text label)
    string(JSON commit ERROR_VARIABLE commit_error GET
        "${json_text}" build commit)
    string(LENGTH "${commit}" commit_length)
    if(NOT "${commit_error}" STREQUAL "NOTFOUND"
       OR NOT commit_length EQUAL 40
       OR NOT "${commit}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${label} build.commit must be 40 lowercase hex")
    endif()
endfunction()
zz_require_commit("${report_json}" "Report")

if(DEFINED ZZ_FINGERPRINT_REFERENCE)
    if("${ZZ_FINGERPRINT_REFERENCE}" STREQUAL ""
       OR NOT EXISTS "${ZZ_FINGERPRINT_REFERENCE}")
        message(FATAL_ERROR
            "ZZ_FINGERPRINT_REFERENCE must name an existing report")
    endif()
    file(READ "${ZZ_FINGERPRINT_REFERENCE}" fingerprint_json)
    zz_require_commit("${fingerprint_json}" "Fingerprint reference")
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
        "build;preset"
        "build;buildType"
        "build;shared"
        "build;lto"
        "build;sanitizers")
        string(JSON report_value ERROR_VARIABLE report_value_error GET
            "${report_json}" ${fingerprint_path})
        string(JSON reference_value ERROR_VARIABLE reference_value_error GET
            "${fingerprint_json}" ${fingerprint_path})
        if(NOT "${report_value_error}" STREQUAL "NOTFOUND"
           OR NOT "${reference_value_error}" STREQUAL "NOTFOUND"
           OR NOT "${report_value}" STREQUAL "${reference_value}")
            message(FATAL_ERROR
                "Fingerprint mismatch at ${fingerprint_path}: "
                "report=${report_value}, reference=${reference_value}")
        endif()
    endforeach()
endif()

string(JSON metric_type ERROR_VARIABLE metric_error TYPE
    "${report_json}" metrics "${ZZ_METRIC}")
if(NOT "${metric_error}" STREQUAL "NOTFOUND"
   OR NOT "${metric_type}" STREQUAL "OBJECT")
    message(FATAL_ERROR "Missing metric object: ${ZZ_METRIC}")
endif()
foreach(field count p50 p95 max)
    string(JSON field_type ERROR_VARIABLE field_error TYPE
        "${report_json}" metrics "${ZZ_METRIC}" ${field})
    if(NOT "${field_error}" STREQUAL "NOTFOUND"
       OR NOT "${field_type}" STREQUAL "NUMBER")
        message(FATAL_ERROR "Metric ${ZZ_METRIC}.${field} must be numeric")
    endif()
endforeach()
zz_json_get(p95 metrics "${ZZ_METRIC}" p95)
zz_json_get(maximum metrics "${ZZ_METRIC}" max)

if(DEFINED ZZ_MAX_P95 AND "${p95}" GREATER "${ZZ_MAX_P95}")
    message(FATAL_ERROR "P95 ${p95} exceeds ${ZZ_MAX_P95}")
endif()
if(DEFINED ZZ_MAX_VALUE)
    if(ZZ_STRICT_MAX)
        if(NOT "${maximum}" LESS "${ZZ_MAX_VALUE}")
            message(FATAL_ERROR "Max ${maximum} must be below ${ZZ_MAX_VALUE}")
        endif()
    elseif("${maximum}" GREATER "${ZZ_MAX_VALUE}")
        message(FATAL_ERROR "Max ${maximum} exceeds ${ZZ_MAX_VALUE}")
    endif()
endif()
message(STATUS
    "${ZZ_SCENARIO}/${ZZ_METRIC}: p95=${p95}, max=${maximum}")
