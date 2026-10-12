cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_SOURCE_DIR ZZ_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
find_program(bash_program bash REQUIRED)
set(fixture "${ZZ_TEST_ROOT}/fixture")
file(MAKE_DIRECTORY "${fixture}/scripts/ci" "${fixture}/cmake"
    "${fixture}/docs/performance/reference/linux" "${fixture}/mock-bin"
    "${fixture}/input")
file(COPY "${ZZ_SOURCE_DIR}/scripts/ci/capture-linux-performance-reference.sh"
    "${ZZ_SOURCE_DIR}/scripts/ci/ZzPerformanceEnvironment.sh"
    DESTINATION "${fixture}/scripts/ci")
file(GLOB helpers "${ZZ_SOURCE_DIR}/scripts/ci/ZzCapturePerformanceReference.cmake")
if(helpers)
    file(COPY ${helpers} DESTINATION "${fixture}/scripts/ci")
endif()
file(COPY "${ZZ_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake"
    DESTINATION "${fixture}/cmake")
file(READ "${ZZ_SOURCE_DIR}/benchmarks/testdata/performance-valid.json" report)
string(JSON report SET "${report}" scenario "\"startup\"")
string(JSON report SET "${report}" build preset "\"linux-gcc-reference\"")
string(JSON commit GET "${report}" build commit)
file(WRITE "${fixture}/input/benchmark.startup.json" "${report}")
file(WRITE "${fixture}/docs/performance/reference/linux/regression-thresholds.json"
    [=[{"schemaVersion":2,"scenarios":{"startup":{"metrics":{"latency":{"metricKind":"statistical-duration","p95":{"mode":"gate","percent":10},"max":{"mode":"observe","percent":10}}}}}}]=])

# Only the expensive configure/build/benchmark process is replaced. Report
# parsing, validation and publication execute the real production scripts.
file(WRITE "${fixture}/mock-bin/cmake"
    "#!/usr/bin/env bash\nif [[ \"\$1\" == --preset || \"\$1\" == --build ]]; then exit 0; fi\nexec '${CMAKE_COMMAND}' \"\$@\"\n")
file(WRITE "${fixture}/mock-bin/taskset"
    "#!/usr/bin/env bash\nshift 2\nexec \"\$@\"\n")
file(WRITE "${fixture}/mock-bin/ctest" [=[#!/usr/bin/env bash
set -eu
[[ " $* " == *" -L benchmark "* ]] || exit 64
[[ " $* " == *" -E ^platform.performance-threshold-contract$ "* ]] || exit 64
mkdir -p build/linux-gcc-reference/reports
if [[ ${ZZ_TEST_KEEP_STALE:-0} != 1 ]]; then
    cp input/benchmark.startup.json build/linux-gcc-reference/reports/
fi
exit "${ZZ_TEST_CTEST_RESULT:-0}"
]=])
file(CHMOD "${fixture}/mock-bin/cmake" "${fixture}/mock-bin/taskset"
    "${fixture}/mock-bin/ctest"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

function(run_capture expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        --unset=QT_NO_GLIB --unset=QT_XCB_NO_MITSHM --unset=LD_PRELOAD
        "PATH=${fixture}/mock-bin:$ENV{PATH}"
        DISPLAY=:99 QT_QPA_PLATFORM=xcb
        "ZZ_BENCHMARK_COMMIT=${commit}"
        ZZ_RUNNER_IMAGE_DIGEST=contract ZZ_GPU_IDENTITY=contract
        "ZZ_TEST_KEEP_STALE=${keep_stale}"
        "ZZ_TEST_CTEST_RESULT=${ctest_result}"
        "${bash_program}" scripts/ci/capture-linux-performance-reference.sh ${ARGN}
        WORKING_DIRECTORY "${fixture}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(expected STREQUAL "pass" AND NOT result EQUAL 0)
        message(FATAL_ERROR "Capture should pass: ${output}${error}")
    elseif(expected STREQUAL "fail" AND result EQUAL 0)
        message(FATAL_ERROR "Capture should reject ${ARGN}")
    endif()
endfunction()

set(baseline "${fixture}/docs/performance/reference/linux/startup.json")
set(ctest_result 0)
file(REMOVE "${baseline}")
file(REMOVE "${fixture}/docs/performance/reference/linux/animation.json")
run_capture(pass startup)
file(READ "${baseline}" captured)
if(NOT captured STREQUAL report)
    message(FATAL_ERROR "Capture changed reporter bytes")
endif()
run_capture(fail startup)
run_capture(pass --force startup)
set(ctest_result 1)
run_capture(fail --force startup)
set(ctest_result 0)
run_capture(fail --force ../startup)
run_capture(fail --force unknown-scenario)

set(keep_stale 1)
run_capture(fail --force startup)
set(keep_stale 0)
string(JSON invalid_report SET "${report}" scenario "\"wrong\"")
file(WRITE "${fixture}/input/benchmark.startup.json" "${invalid_report}")
run_capture(fail --force startup)
string(JSON invalid_report SET "${report}" build commit
    "\"0000000000000000000000000000000000000000\"")
file(WRITE "${fixture}/input/benchmark.startup.json" "${invalid_report}")
run_capture(fail --force startup)
file(WRITE "${fixture}/input/benchmark.startup.json" "${report}")

# Failure in a later report must not publish an earlier validated report.
file(REMOVE "${baseline}")
run_capture(fail startup animation)
if(EXISTS "${baseline}")
    message(FATAL_ERROR "Capture published a partial batch")
endif()
run_capture(pass startup)

# A partial update cannot silently mix reports from different runner profiles.
string(JSON invalid_report SET "${report}" scenario "\"animation\"")
string(JSON invalid_report SET "${invalid_report}" environment gpu "\"other-gpu\"")
set(retained "${fixture}/docs/performance/reference/linux/animation.json")
file(WRITE "${retained}" "${invalid_report}")
run_capture(fail --force startup)
file(REMOVE "${retained}")
file(READ "${baseline}" unchanged)
if(NOT unchanged STREQUAL report)
    message(FATAL_ERROR "Rejected capture overwrote the baseline")
endif()
message(STATUS "Performance capture contract passed")
