cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_REPORT_DIR ZZ_REFERENCE_DIR ZZ_SCENARIOS ZZ_COMMIT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
set(compare_script "${CMAKE_CURRENT_LIST_DIR}/../../cmake/ZzComparePerformanceReport.cmake")

# Validate the entire batch before replacing any baseline. Self-comparison also
# checks report schema, metric coverage and the formal threshold policy.
foreach(scenario IN LISTS ZZ_SCENARIOS)
    if(NOT scenario MATCHES "^[a-z][a-z-]*$")
        message(FATAL_ERROR "Invalid performance scenario: ${scenario}")
    endif()
    set(report "${ZZ_REPORT_DIR}/benchmark.${scenario}.json")
    if(NOT EXISTS "${report}")
        message(FATAL_ERROR "Missing report: ${report}")
    endif()
    if(EXISTS "${ZZ_REFERENCE_DIR}/${scenario}.json" AND NOT ZZ_FORCE)
        message(FATAL_ERROR "Baseline already exists: ${scenario}")
    endif()
    file(READ "${report}" report_json)
    string(JSON reported_scenario GET "${report_json}" scenario)
    string(JSON reported_commit GET "${report_json}" build commit)
    string(JSON reported_preset GET "${report_json}" build preset)
    if(NOT reported_scenario STREQUAL scenario
       OR NOT reported_commit STREQUAL ZZ_COMMIT
       OR NOT reported_preset STREQUAL "linux-gcc-reference")
        message(FATAL_ERROR "Unexpected report scenario/commit/preset: ${report}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${report}" "-DZZ_CURRENT=${report}"
        "-DZZ_THRESHOLDS=${ZZ_REFERENCE_DIR}/regression-thresholds.json"
        -P "${compare_script}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Invalid capture ${scenario}: ${output}${error}")
    endif()
    if(NOT DEFINED batch_reference)
        set(batch_reference "${report_json}")
    endif()
    foreach(path "environment" "build;buildType" "build;shared" "build;lto" "build;sanitizers")
        string(JSON value GET "${report_json}" ${path})
        string(JSON expected GET "${batch_reference}" ${path})
        if(NOT value STREQUAL expected)
            message(FATAL_ERROR "Capture fingerprint mismatch: ${scenario}/${path}")
        endif()
    endforeach()
endforeach()

# A partial capture must not mix a new runner with the retained old baselines.
file(GLOB retained_reports "${ZZ_REFERENCE_DIR}/*.json")
foreach(retained IN LISTS retained_reports)
    get_filename_component(scenario "${retained}" NAME_WE)
    if(scenario STREQUAL "regression-thresholds" OR scenario IN_LIST ZZ_SCENARIOS)
        continue()
    endif()
    file(READ "${retained}" retained_json)
    foreach(path "environment" "build;buildType" "build;shared" "build;lto" "build;sanitizers")
        string(JSON value GET "${retained_json}" ${path})
        string(JSON expected GET "${batch_reference}" ${path})
        if(NOT value STREQUAL expected)
            message(FATAL_ERROR
                "Retained baseline fingerprint mismatch: ${scenario}/${path}; "
                "do not mix runner profiles")
        endif()
    endforeach()
endforeach()
foreach(scenario IN LISTS ZZ_SCENARIOS)
    configure_file("${ZZ_REPORT_DIR}/benchmark.${scenario}.json"
        "${ZZ_REFERENCE_DIR}/${scenario}.json" COPYONLY)
    message(STATUS "Captured baseline: ${ZZ_REFERENCE_DIR}/${scenario}.json")
endforeach()
