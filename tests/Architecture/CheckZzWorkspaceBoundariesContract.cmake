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

set(zz_workspace_contract_failures)

function(zz_check_workspace_fixture fixture_name expected_result expected_rule)
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
        list(APPEND zz_workspace_contract_failures
            "good workspace fixture was rejected: ${fixture_name}\n"
            "${fixture_stdout}\n${fixture_stderr}")
    endif()
    if(NOT expected_result AND fixture_result EQUAL 0)
        list(APPEND zz_workspace_contract_failures
            "bad workspace fixture was accepted: ${fixture_name}")
    elseif(NOT expected_result
           AND NOT "${expected_rule}" STREQUAL ""
           AND NOT "${fixture_stdout}${fixture_stderr}" MATCHES
               "${expected_rule}:")
        list(APPEND zz_workspace_contract_failures
            "bad workspace fixture produced no ${expected_rule}: ${fixture_name}\n"
            "${fixture_stdout}\n${fixture_stderr}")
    endif()
    set(zz_workspace_contract_failures "${zz_workspace_contract_failures}"
        PARENT_SCOPE)
endfunction()

zz_check_workspace_fixture(zzworkspace-good TRUE "")
zz_check_workspace_fixture(zzworkspace-good-string-literal TRUE "")
zz_check_workspace_fixture(zzworkspace-good-arithmetic TRUE "")
zz_check_workspace_fixture(zzworkspace-forbidden-dependency FALSE
    WORKSPACE_PRESENTATION_DEPENDENCY)
zz_check_workspace_fixture(zzworkspace-forbidden-pointer-type FALSE
    WORKSPACE_PRESENTATION_DEPENDENCY)
zz_check_workspace_fixture(zzworkspace-forbidden-value-type FALSE
    WORKSPACE_PRESENTATION_DEPENDENCY)
zz_check_workspace_fixture(zzworkspace-forbidden-template-type FALSE
    WORKSPACE_PRESENTATION_DEPENDENCY)
zz_check_workspace_fixture(zzworkspace-no-pimpl FALSE
    WORKSPACE_PUBLIC_WIDGET_PIMPL)
zz_check_workspace_fixture(zzworkspace-project-base-no-pimpl FALSE
    WORKSPACE_PUBLIC_WIDGET_PIMPL)

if(zz_workspace_contract_failures)
    list(JOIN zz_workspace_contract_failures "\n" failure_text)
    message(FATAL_ERROR
        "Workspace boundary checker contract failures:\n${failure_text}")
endif()

message(STATUS "Workspace boundary checker contract passed")
