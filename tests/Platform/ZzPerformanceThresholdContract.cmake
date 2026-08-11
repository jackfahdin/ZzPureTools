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

message(STATUS "Performance threshold contract passed")
