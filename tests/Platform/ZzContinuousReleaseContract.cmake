cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR "${ZZ_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
file(REAL_PATH "${ZZ_SOURCE_DIR}" source_dir)

set(publish_script
    "${source_dir}/scripts/release/publish-continuous-build.sh")
set(publish_test
    "${source_dir}/tests/Platform/ZzContinuousPublishTest.sh")
set(workflow_file "${source_dir}/.github/workflows/ci.yml")
foreach(required_file IN ITEMS
        "${publish_script}" "${publish_test}" "${workflow_file}")
    if(NOT EXISTS "${required_file}"
       OR IS_DIRECTORY "${required_file}"
       OR IS_SYMLINK "${required_file}")
        message(FATAL_ERROR
            "Continuous release file is missing: ${required_file}")
    endif()
    file(SIZE "${required_file}" required_file_size)
    if(required_file_size EQUAL 0)
        message(FATAL_ERROR
            "Continuous release file is empty: ${required_file}")
    endif()
endforeach()

file(READ "${workflow_file}" workflow_content)
foreach(required_workflow_token IN ITEMS
        "publish-continuous-build:"
        "contents: write"
        "actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093"
        "scripts/release/publish-continuous-build.sh"
        "--artifact-root"
        "github.repository"
        "github.sha"
        "github.run_id")
    string(FIND "${workflow_content}"
        "${required_workflow_token}" workflow_token_position)
    if(workflow_token_position EQUAL -1)
        message(FATAL_ERROR
            "Continuous workflow lacks token: ${required_workflow_token}")
    endif()
endforeach()

execute_process(
    COMMAND bash "${publish_script}"
    RESULT_VARIABLE no_argument_result
    OUTPUT_QUIET ERROR_QUIET)
if(no_argument_result EQUAL 0)
    message(FATAL_ERROR "Continuous publish script accepted missing arguments")
endif()

file(READ "${publish_script}" publish_content)
set(required_publish_tokens
    "continuous-build"
    "VerifyArtifactSet.cmake"
    "release view"
    "release create"
    "release upload"
    "release edit"
    "release delete-asset"
    "api --method PATCH"
    "--prerelease"
    "--latest=false"
    "自动生成"
    "未签名"
    "build-info.json")
foreach(required_token IN LISTS required_publish_tokens)
    string(FIND "${publish_content}"
        "${required_token}" required_position)
    if(required_position EQUAL -1)
        message(FATAL_ERROR
            "Continuous publish script lacks token: ${required_token}")
    endif()
endforeach()

file(READ "${publish_test}" publish_test_content)
foreach(required_diagnostic_token IN ITEMS
        "report_failure()"
        "BASH_LINENO"
        "BASH_COMMAND"
        "stderr.log"
        "gh.log")
    string(FIND "${publish_test_content}"
        "${required_diagnostic_token}" diagnostic_token_position)
    if(diagnostic_token_position EQUAL -1)
        message(FATAL_ERROR
            "Continuous publish test lacks failure diagnostic token: "
            "${required_diagnostic_token}")
    endif()
endforeach()

set(diagnostic_test_root
    "${source_dir}/build/continuous-publish-diagnostic-contract")
file(REMOVE_RECURSE "${diagnostic_test_root}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        ZZ_CONTINUOUS_PUBLISH_TEST_DIAGNOSTIC_PROBE=1
        bash "${publish_test}" "${source_dir}" "${diagnostic_test_root}"
    RESULT_VARIABLE diagnostic_result
    OUTPUT_VARIABLE diagnostic_stdout
    ERROR_VARIABLE diagnostic_stderr)
if(diagnostic_result EQUAL 0)
    message(FATAL_ERROR
        "Continuous publish diagnostic probe unexpectedly succeeded")
endif()
foreach(expected_diagnostic IN ITEMS
        "exit=1"
        "line="
        "command=false"
        "bash-lines="
        "diagnostic-probe/stderr.log"
        "diagnostic stderr fixture"
        "diagnostic-probe/gh.log"
        "diagnostic gh fixture")
    string(FIND "${diagnostic_stderr}"
        "${expected_diagnostic}" expected_diagnostic_position)
    if(expected_diagnostic_position EQUAL -1)
        message(FATAL_ERROR
            "Continuous publish diagnostic probe lacks output: "
            "${expected_diagnostic}\nstderr:\n${diagnostic_stderr}")
    endif()
endforeach()

foreach(forbidden_token IN ITEMS
    "curl "
    "wget "
    "gh release delete continuous-build --yes"
    "temp_image")
    string(FIND "${publish_content}"
        "${forbidden_token}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "Continuous publish script contains forbidden token: ${forbidden_token}")
    endif()
endforeach()

message(STATUS "PASS continuous release contract")
