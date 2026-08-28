cmake_minimum_required(VERSION 3.23)

foreach(required_variable IN ITEMS ZZ_ARTIFACT_ROOT ZZ_EXPECTED_COMMIT)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ZZ_ARTIFACT_ROOT
    NORMALIZE OUTPUT_VARIABLE artifact_root)
if(NOT IS_DIRECTORY "${artifact_root}" OR IS_SYMLINK "${artifact_root}")
    message(FATAL_ERROR
        "ZZ_ARTIFACT_ROOT must identify a regular directory")
endif()
string(LENGTH "${ZZ_EXPECTED_COMMIT}" expected_commit_length)
if(NOT expected_commit_length EQUAL 40
   OR NOT "${ZZ_EXPECTED_COMMIT}" MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR
        "ZZ_EXPECTED_COMMIT must be 40 lowercase hexadecimal characters")
endif()
string(SUBSTRING "${ZZ_EXPECTED_COMMIT}" 0 12 expected_short_commit)

set(expected_platforms
    linux-x86_64
    windows-msvc2022-x86_64
    windows-mingw-x86_64
    macos-arm64
    macos-x86_64)

file(GLOB_RECURSE artifact_files
    LIST_DIRECTORIES false "${artifact_root}/*")
list(LENGTH artifact_files artifact_file_count)
if(NOT artifact_file_count EQUAL 15)
    message(FATAL_ERROR
        "Continuous artifact root must contain exactly 15 files; found ${artifact_file_count}")
endif()
foreach(artifact_file IN LISTS artifact_files)
    if(IS_SYMLINK "${artifact_file}")
        message(FATAL_ERROR "Artifact files must not be symlinks: ${artifact_file}")
    endif()
endforeach()

file(GLOB_RECURSE build_info_files
    LIST_DIRECTORIES false "${artifact_root}/*/build-info.json")
list(LENGTH build_info_files build_info_count)
if(NOT build_info_count EQUAL 5)
    message(FATAL_ERROR
        "Continuous artifact root must contain five build-info.json files")
endif()

macro(zz_json_string field output_variable)
    # 宏在当前 build-info 作用域中提取必需字符串并立即失败关闭。
    string(JSON zz_field_type ERROR_VARIABLE zz_field_error
        TYPE "${build_info_json}" "${field}")
    if(NOT "${zz_field_error}" STREQUAL "NOTFOUND"
       OR NOT "${zz_field_type}" STREQUAL "STRING")
        message(FATAL_ERROR
            "${build_info_file}: ${field} must be a JSON string")
    endif()
    string(JSON ${output_variable} GET "${build_info_json}" "${field}")
    if("${${output_variable}}" STREQUAL "")
        message(FATAL_ERROR
            "${build_info_file}: ${field} must not be empty")
    endif()
endmacro()

set(seen_platforms)
foreach(build_info_file IN LISTS build_info_files)
    file(SIZE "${build_info_file}" build_info_size)
    if(build_info_size EQUAL 0)
        message(FATAL_ERROR "Build info is empty: ${build_info_file}")
    endif()
    file(READ "${build_info_file}" build_info_json)
    string(JSON root_type ERROR_VARIABLE root_error TYPE "${build_info_json}")
    if(NOT "${root_error}" STREQUAL "NOTFOUND"
       OR NOT "${root_type}" STREQUAL "OBJECT")
        message(FATAL_ERROR "Invalid build-info JSON: ${build_info_file}")
    endif()

    string(JSON schema_type ERROR_VARIABLE schema_error
        TYPE "${build_info_json}" schemaVersion)
    string(JSON schema_version ERROR_VARIABLE schema_value_error
        GET "${build_info_json}" schemaVersion)
    if(NOT "${schema_error}" STREQUAL "NOTFOUND"
       OR NOT "${schema_value_error}" STREQUAL "NOTFOUND"
       OR NOT "${schema_type}" STREQUAL "NUMBER"
       OR NOT schema_version EQUAL 1)
        message(FATAL_ERROR
            "${build_info_file}: schemaVersion must equal 1")
    endif()

    foreach(field IN ITEMS
        platformId packageFile packageSha256 commit builtAtUtc runnerOs
        architecture qtVersion compilerId compilerVersion preset linkage)
        zz_json_string("${field}" "${field}")
    endforeach()

    foreach(boolean_field IN ITEMS dirty lto)
        string(JSON boolean_type ERROR_VARIABLE boolean_error
            TYPE "${build_info_json}" "${boolean_field}")
        string(JSON ${boolean_field} ERROR_VARIABLE boolean_value_error
            GET "${build_info_json}" "${boolean_field}")
        if(NOT "${boolean_error}" STREQUAL "NOTFOUND"
           OR NOT "${boolean_value_error}" STREQUAL "NOTFOUND"
           OR NOT "${boolean_type}" STREQUAL "BOOLEAN")
            message(FATAL_ERROR
                "${build_info_file}: ${boolean_field} must be a JSON boolean")
        endif()
    endforeach()

    if(NOT platformId IN_LIST expected_platforms)
        message(FATAL_ERROR "Unsupported platformId: ${platformId}")
    endif()
    if(platformId IN_LIST seen_platforms)
        message(FATAL_ERROR "Duplicate platformId: ${platformId}")
    endif()
    list(APPEND seen_platforms "${platformId}")
    if(NOT "${commit}" STREQUAL "${ZZ_EXPECTED_COMMIT}")
        message(FATAL_ERROR
            "Artifact commit mismatch for ${platformId}: ${commit}")
    endif()
    if(dirty)
        message(FATAL_ERROR "Release artifact is marked dirty: ${platformId}")
    endif()
    if(NOT lto)
        message(FATAL_ERROR "Release artifact is not LTO-enabled: ${platformId}")
    endif()
    if(NOT "${linkage}" STREQUAL "shared")
        message(FATAL_ERROR "Release artifact is not shared: ${platformId}")
    endif()

    if("${platformId}" STREQUAL "linux-x86_64")
        # 平台 id 同时决定固定包名、扩展名和唯一允许的目标架构。
        set(expected_extension AppImage)
        set(expected_architecture x86_64)
    elseif("${platformId}" STREQUAL "windows-msvc2022-x86_64"
           OR "${platformId}" STREQUAL "windows-mingw-x86_64")
        set(expected_extension zip)
        set(expected_architecture x86_64)
    elseif("${platformId}" STREQUAL "macos-arm64")
        set(expected_extension dmg)
        set(expected_architecture arm64)
    else()
        set(expected_extension dmg)
        set(expected_architecture x86_64)
    endif()
    set(expected_package_file
        "ZzPureToolsExample-continuous-${platformId}-${expected_short_commit}.${expected_extension}")
    if(NOT "${packageFile}" STREQUAL "${expected_package_file}")
        message(FATAL_ERROR
            "Unexpected package filename for ${platformId}: ${packageFile}")
    endif()
    if(NOT "${architecture}" STREQUAL "${expected_architecture}")
        message(FATAL_ERROR
            "Unexpected architecture for ${platformId}: ${architecture}")
    endif()
    string(LENGTH "${packageSha256}" declared_sha_length)
    if(NOT declared_sha_length EQUAL 64
       OR NOT "${packageSha256}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "Invalid packageSha256 for ${platformId}")
    endif()

    get_filename_component(build_info_dir "${build_info_file}" DIRECTORY)
    set(package_path "${build_info_dir}/${packageFile}")
    set(checksum_path "${package_path}.sha256")
    foreach(required_path IN ITEMS "${package_path}" "${checksum_path}")
        if(NOT EXISTS "${required_path}"
           OR IS_DIRECTORY "${required_path}"
           OR IS_SYMLINK "${required_path}")
            message(FATAL_ERROR
                "Missing regular artifact file for ${platformId}: ${required_path}")
        endif()
        file(SIZE "${required_path}" required_path_size)
        if(required_path_size EQUAL 0)
            message(FATAL_ERROR "Artifact file is empty: ${required_path}")
        endif()
    endforeach()
    file(SHA256 "${package_path}" actual_package_sha256)
    if(NOT "${actual_package_sha256}" STREQUAL "${packageSha256}")
        message(FATAL_ERROR
            "Package digest mismatch for ${platformId}")
    endif()
    file(READ "${checksum_path}" checksum_content)
    string(STRIP "${checksum_content}" checksum_content)
    if(NOT "${checksum_content}" STREQUAL
       "${actual_package_sha256}  ${packageFile}")
        message(FATAL_ERROR
            "Checksum manifest mismatch for ${platformId}")
    endif()
endforeach()

list(SORT seen_platforms)
list(SORT expected_platforms)
if(NOT "${seen_platforms}" STREQUAL "${expected_platforms}")
    message(FATAL_ERROR "Continuous artifact platform set is incomplete")
endif()

message(STATUS
    "PASS five-platform artifact set for ${ZZ_EXPECTED_COMMIT}")
