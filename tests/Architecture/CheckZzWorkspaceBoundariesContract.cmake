cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR ZZ_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR
    NORMALIZE
    OUTPUT_VARIABLE zz_source_root)

set(zz_checker
    "${zz_source_root}/tests/Architecture/CheckZzWorkspaceBoundaries.cmake")
if(NOT EXISTS "${zz_checker}")
    message(FATAL_ERROR
        "Workspace boundary checker is missing: ${zz_checker}")
endif()

set(zz_fixture_root
    "${zz_source_root}/tests/Architecture/fixtures")

function(zz_expect_workspace_fixture fixture_name expected_result)
    set(fixture_root "${zz_fixture_root}/${fixture_name}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DZZ_WORKSPACE_PUBLIC_ROOTS=${fixture_root}/public"
            "-DZZ_WORKSPACE_SOURCE_ROOTS=${fixture_root}/source"
            "-DZZ_WORKSPACE_PRIVATE_ROOTS=${fixture_root}/private"
            -P "${zz_checker}"
        RESULT_VARIABLE fixture_result
        OUTPUT_VARIABLE fixture_stdout
        ERROR_VARIABLE fixture_stderr
    )
    if(expected_result AND NOT fixture_result EQUAL 0)
        message(FATAL_ERROR
            "good workspace fixture was rejected:\n"
            "${fixture_stdout}\n${fixture_stderr}")
    endif()
    if(NOT expected_result AND fixture_result EQUAL 0)
        message(FATAL_ERROR "bad workspace fixture was accepted: ${fixture_name}")
    endif()
    set(zz_fixture_output "${fixture_stdout}\n${fixture_stderr}"
        PARENT_SCOPE)
endfunction()

zz_expect_workspace_fixture(zzworkspace-good TRUE)

zz_expect_workspace_fixture(zzworkspace-forbidden-dependency FALSE)
if(NOT zz_fixture_output MATCHES "WORKSPACE_PRESENTATION_DEPENDENCY:")
    message(FATAL_ERROR
        "forbidden dependency fixture produced no dependency failure:\n"
        "${zz_fixture_output}")
endif()

zz_expect_workspace_fixture(zzworkspace-no-pimpl FALSE)
if(NOT zz_fixture_output MATCHES "WORKSPACE_PUBLIC_WIDGET_PIMPL:")
    message(FATAL_ERROR
        "no-PIMPL fixture produced no PIMPL failure:\n${zz_fixture_output}")
endif()

message(STATUS "Workspace boundary checker contract passed")
