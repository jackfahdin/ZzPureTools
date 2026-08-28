cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR "${ZZ_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
file(REAL_PATH "${ZZ_SOURCE_DIR}" source_dir)

set(publish_script
    "${source_dir}/scripts/release/publish-continuous-build.sh")
set(publish_test
    "${source_dir}/tests/Platform/ZzContinuousPublishTest.sh")
foreach(required_file IN ITEMS "${publish_script}" "${publish_test}")
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
