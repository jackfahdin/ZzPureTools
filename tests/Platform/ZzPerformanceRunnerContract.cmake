cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_SOURCE_DIR ZZ_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
find_program(bash_program bash REQUIRED)
find_program(tee_program tee REQUIRED)
file(MAKE_DIRECTORY "${ZZ_TEST_ROOT}")
# Each invocation owns a new fixture; earlier failed-run evidence remains intact.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef fixture_id)
set(fixture "${ZZ_TEST_ROOT}/${fixture_id}")
set(build "${fixture}/build/linux-gcc-benchmarks")
file(MAKE_DIRECTORY "${fixture}/scripts/ci" "${fixture}/mock-bin"
    "${fixture}/input" "${build}/reports")
file(COPY "${ZZ_SOURCE_DIR}/scripts/ci/run-linux-performance-gates.sh"
    DESTINATION "${fixture}/scripts/ci")
file(WRITE "${build}/CMakeCache.txt" "ZZ_PERFORMANCE_REFERENCE:BOOL=ON\n")
file(WRITE "${fixture}/mock-bin/taskset" [=[#!/usr/bin/env bash
shift 2
exec "$@"
]=])
file(WRITE "${fixture}/mock-bin/ctest" [=[#!/usr/bin/env bash
set -eu
echo producer-stdout
echo producer-stderr >&2
printf 'run\n' >> producer-calls
for report in input/*.json; do
    cp "$report" build/linux-gcc-benchmarks/reports/
done
exit "$ZZ_TEST_CTEST_RESULT"
]=])
file(WRITE "${fixture}/mock-bin/cmake"
    "#!/usr/bin/env bash\nif [[ \"\$1\" != -E ]]; then\n  printf 'compare\\n' >> compare-calls\n  exit \"\$ZZ_TEST_COMPARE_RESULT\"\nfi\nexec '${CMAKE_COMMAND}' \"\$@\"\n")
file(WRITE "${fixture}/mock-bin/tee"
    "#!/usr/bin/env bash\n'${tee_program}' \"\$@\" || exit \"\$?\"\nexit \"\$ZZ_TEST_LOG_RESULT\"\n")
file(CHMOD "${fixture}/mock-bin/taskset" "${fixture}/mock-bin/ctest"
    "${fixture}/mock-bin/cmake" "${fixture}/mock-bin/tee"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

set(ctest_result 8)
set(compare_result 0)
set(log_result 0)
function(run_runner expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=${fixture}/mock-bin:$ENV{PATH}" DISPLAY=:99 QT_QPA_PLATFORM=xcb
        ZZ_BENCHMARK_COMMIT=1111111111111111111111111111111111111111
        ZZ_RUNNER_IMAGE_DIGEST=contract ZZ_GPU_IDENTITY=contract
        "ZZ_TEST_CTEST_RESULT=${ctest_result}"
        "ZZ_TEST_COMPARE_RESULT=${compare_result}"
        "ZZ_TEST_LOG_RESULT=${log_result}"
        "${bash_program}" scripts/ci/run-linux-performance-gates.sh
        WORKING_DIRECTORY "${fixture}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL expected)
        message(FATAL_ERROR "Runner result ${result}, expected ${expected}: ${output}${error}")
    endif()
    set(runner_output "${output}${error}" PARENT_SCOPE)
    if(NOT output MATCHES "Performance evidence: ([^\n\r]+)")
        message(FATAL_ERROR "Runner did not print its evidence location")
    endif()
    set(last_run "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

file(WRITE "${fixture}/input/benchmark.startup.json" "fresh failure report\n")
file(WRITE "${build}/reports/benchmark.animation.json" "stale report\n")
run_runner(8)
file(GLOB_RECURSE saved_reports "${build}/release-rounds/benchmark.startup.json")
list(LENGTH saved_reports saved_count)
if(NOT saved_count EQUAL 1)
    message(FATAL_ERROR "Failed CTest lost its produced report: ${runner_output}")
endif()
list(GET saved_reports 0 first_report)
get_filename_component(first_round "${first_report}" DIRECTORY)
file(READ "${first_report}" saved)
file(READ "${first_round}/ctest.log" log)
file(READ "${first_round}/commands.log" commands)
if(NOT saved STREQUAL "fresh failure report\n"
   OR NOT log MATCHES "producer-stdout" OR NOT log MATCHES "producer-stderr"
   OR NOT commands MATCHES "ctest.exit=8"
   OR EXISTS "${first_round}/benchmark.animation.json"
   OR EXISTS "${fixture}/compare-calls")
    message(FATAL_ERROR "Failed round must retain exact evidence, exclude stale reports and skip comparison")
endif()

set(ctest_result 5)
file(WRITE "${fixture}/input/benchmark.startup.json" "second failure report\n")
run_runner(5)
file(GLOB_RECURSE saved_reports "${build}/release-rounds/benchmark.startup.json")
list(LENGTH saved_reports saved_count)
file(READ "${first_report}" retained)
if(NOT saved_count EQUAL 2 OR NOT retained STREQUAL saved)
    message(FATAL_ERROR "A later invocation overwrote the previous failure evidence")
endif()

# A zero CTest status without every expected producer is still a failure.
set(ctest_result 0)
run_runner(1)
if(EXISTS "${fixture}/compare-calls")
    message(FATAL_ERROR "Runner compared incomplete reports")
endif()

set(scenarios startup theme-switch animation large-model window-lifecycle
    navigation-pane workspace-components fluent-standard-surfaces backdrop
    radial-gauge-animation idle example-startup example-navigation
    example-theme-switch example-large-model example-idle)
foreach(scenario IN LISTS scenarios)
    file(WRITE "${fixture}/input/benchmark.${scenario}.json" "${scenario} current\n")
endforeach()
file(REMOVE "${fixture}/producer-calls")
run_runner(0)
file(STRINGS "${fixture}/producer-calls" producers)
file(STRINGS "${fixture}/compare-calls" comparisons)
list(LENGTH producers producer_count)
list(LENGTH comparisons comparison_count)
if(NOT producer_count EQUAL 3 OR NOT comparison_count EQUAL 48
   OR NOT runner_output MATCHES "PASS three-round")
    message(FATAL_ERROR "Successful runner must retain all three rounds and all 48 comparisons")
endif()

# A relative comparison failure must fail immediately, with all raw reports saved.
file(REMOVE "${fixture}/producer-calls" "${fixture}/compare-calls")
set(compare_result 9)
run_runner(9)
file(STRINGS "${fixture}/producer-calls" producers)
file(STRINGS "${fixture}/compare-calls" comparisons)
list(LENGTH producers producer_count)
list(LENGTH comparisons comparison_count)
if(NOT producer_count EQUAL 1 OR NOT comparison_count EQUAL 1)
    message(FATAL_ERROR "Relative failure must prevent later rounds and comparisons")
endif()
file(GLOB archived "${last_run}/round-1/benchmark.*.json")
list(LENGTH archived archived_count)
file(READ "${last_run}/round-1/commands.log" commands)
if(NOT archived_count EQUAL 16 OR NOT commands MATCHES "compare.startup.exit=9"
   OR NOT EXISTS "${last_run}/round-1/compare.startup.log")
    message(FATAL_ERROR "Relative failure did not retain all producer reports and its status")
endif()

# Logging errors cannot turn a failed command green, or replace its exit code.
set(compare_result 0)
set(log_result 21)
set(ctest_result 8)
run_runner(8)
set(ctest_result 0)
run_runner(21)
message(STATUS "Performance runner failure-evidence contract passed")
