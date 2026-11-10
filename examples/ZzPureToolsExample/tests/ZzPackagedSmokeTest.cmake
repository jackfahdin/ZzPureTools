cmake_minimum_required(VERSION 3.23)

if(NOT EXISTS "${ZZ_EXECUTABLE}" OR NOT EXISTS "${ZZ_XVFB_RUN}")
    message(FATAL_ERROR "Example executable and xvfb-run are required")
endif()

# Reproduce the deployed AppImage invocation, including its legacy auto-close
# variable. The explicit smoke must finish every route under the native X11 QPA.
execute_process(
    COMMAND "${ZZ_XVFB_RUN}" -a "${CMAKE_COMMAND}" -E env
        QT_QPA_PLATFORM=xcb
        ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1500
        ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO=
        "${ZZ_EXECUTABLE}" --smoke-test
    RESULT_VARIABLE smoke_result
    OUTPUT_VARIABLE smoke_stdout
    ERROR_VARIABLE smoke_stderr
    TIMEOUT 40)
if(NOT "${smoke_result}" STREQUAL "0")
    message(FATAL_ERROR
        "Packaged smoke failed (${smoke_result})\n${smoke_stdout}\n${smoke_stderr}")
endif()
string(FIND "${smoke_stdout}\n${smoke_stderr}"
    "ZzPureToolsExample route smoke passed" completion_position)
if(completion_position EQUAL -1)
    message(FATAL_ERROR "Packaged smoke exited before completing the route checks")
endif()
message(STATUS "PASS packaged smoke completed every route")
