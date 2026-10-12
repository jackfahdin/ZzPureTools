cmake_minimum_required(VERSION 3.23)
foreach(required ZZ_SOURCE_DIR ZZ_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
find_program(bash_program bash REQUIRED)
file(MAKE_DIRECTORY "${ZZ_TEST_ROOT}/scripts/ci" "${ZZ_TEST_ROOT}/mock-bin"
    "${ZZ_TEST_ROOT}/build/linux-gcc-benchmarks/reports")
set(scripts run-linux-gates.sh run-linux-performance-gates.sh
    capture-linux-performance-reference.sh)
foreach(script IN LISTS scripts)
    file(COPY "${ZZ_SOURCE_DIR}/scripts/ci/${script}" DESTINATION "${ZZ_TEST_ROOT}/scripts/ci")
endforeach()
if(EXISTS "${ZZ_SOURCE_DIR}/scripts/ci/ZzPerformanceEnvironment.sh")
    file(COPY "${ZZ_SOURCE_DIR}/scripts/ci/ZzPerformanceEnvironment.sh"
        DESTINATION "${ZZ_TEST_ROOT}/scripts/ci")
endif()
file(WRITE "${ZZ_TEST_ROOT}/build/linux-gcc-benchmarks/CMakeCache.txt"
    "ZZ_PERFORMANCE_REFERENCE:BOOL=ON\n")
file(WRITE "${ZZ_TEST_ROOT}/mock-bin/cmake" [=[#!/usr/bin/env bash
touch command-started
exit 90
]=])
file(CHMOD "${ZZ_TEST_ROOT}/mock-bin/cmake" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(WRITE "${ZZ_TEST_ROOT}/mock-bin/git" [=[#!/usr/bin/env bash
printf '1111111111111111111111111111111111111111\n'
]=])
file(CHMOD "${ZZ_TEST_ROOT}/mock-bin/git" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

foreach(script IN LISTS scripts)
    foreach(override IN ITEMS QT_NO_GLIB=1 QT_NO_GLIB=0
            QT_XCB_NO_MITSHM=1 QT_XCB_NO_MITSHM=0 QT_XCB_NO_MITSHM=
            LD_PRELOAD=/nonexistent/diagnostic.so QT_NO_GLIB= LD_PRELOAD=)
        string(REGEX REPLACE "=.*" "" variable "${override}")
        set(expected 64)
        if(override STREQUAL "QT_NO_GLIB=" OR override STREQUAL "LD_PRELOAD=")
            set(expected 90)
        endif()
        file(REMOVE "${ZZ_TEST_ROOT}/command-started")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env
            --unset=QT_NO_GLIB --unset=QT_XCB_NO_MITSHM --unset=LD_PRELOAD
            "PATH=${ZZ_TEST_ROOT}/mock-bin:$ENV{PATH}"
            DISPLAY=:99 QT_QPA_PLATFORM=xcb ZZ_BENCHMARK_COMMIT=contract
            ZZ_RUNNER_IMAGE_DIGEST=contract ZZ_GPU_IDENTITY=contract
            QT_ROOT=contract GCC_13=contract GXX_13=contract
            GCC_13_TOOLCHAIN_ROOT=contract CLANG_17=contract CLANGXX_17=contract
            "${bash_program}" -c "export \"$1\"; contract_script=$2; shift 2; source \"$contract_script\"" contract
            "${override}" "${ZZ_TEST_ROOT}/scripts/ci/${script}"
            WORKING_DIRECTORY "${ZZ_TEST_ROOT}"
            RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(expected EQUAL 64 AND (NOT result EQUAL 64
           OR NOT "${output}${error}" MATCHES "unset ${variable}"
           OR EXISTS "${ZZ_TEST_ROOT}/command-started"))
            message(FATAL_ERROR "${script} must reject ${override} before work starts: ${result}: ${output}${error}")
        elseif(expected EQUAL 90 AND (NOT result EQUAL 90
               OR NOT EXISTS "${ZZ_TEST_ROOT}/command-started"))
            message(FATAL_ERROR "${script} must accept an empty ${variable}: ${result}: ${output}${error}")
        endif()
    endforeach()
endforeach()
message(STATUS "Formal performance entrypoints reject diagnostic runtime overrides")
