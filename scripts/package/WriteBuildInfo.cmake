cmake_minimum_required(VERSION 3.23)

set(required_variables
    ZZ_PACKAGE_PATH
    ZZ_PLATFORM_ID
    ZZ_COMMIT
    ZZ_DIRTY
    ZZ_BUILT_AT_UTC
    ZZ_RUNNER_OS
    ZZ_ARCHITECTURE
    ZZ_QT_VERSION
    ZZ_COMPILER_ID
    ZZ_COMPILER_VERSION
    ZZ_PRESET
    ZZ_LINKAGE
    ZZ_LTO)
foreach(required_variable IN LISTS required_variables)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ZZ_PACKAGE_PATH
    NORMALIZE OUTPUT_VARIABLE package_path_input)
if(NOT EXISTS "${package_path_input}"
   OR IS_DIRECTORY "${package_path_input}"
   OR IS_SYMLINK "${package_path_input}")
    message(FATAL_ERROR
        "ZZ_PACKAGE_PATH must identify a regular non-symlink file")
endif()
file(REAL_PATH "${package_path_input}" package_path)
file(SIZE "${package_path}" package_size)
if(package_size EQUAL 0)
    message(FATAL_ERROR "ZZ_PACKAGE_PATH must not be empty")
endif()

set(supported_platforms
    linux-x86_64
    windows-msvc2022-x86_64
    windows-mingw-x86_64
    macos-arm64
    macos-x86_64)
if(NOT ZZ_PLATFORM_ID IN_LIST supported_platforms)
    message(FATAL_ERROR "Unsupported ZZ_PLATFORM_ID: ${ZZ_PLATFORM_ID}")
endif()

string(LENGTH "${ZZ_COMMIT}" commit_length)
if(NOT commit_length EQUAL 40
   OR NOT "${ZZ_COMMIT}" MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "ZZ_COMMIT must be 40 lowercase hexadecimal characters")
endif()
if(NOT "${ZZ_DIRTY}" STREQUAL "true"
   AND NOT "${ZZ_DIRTY}" STREQUAL "false")
    message(FATAL_ERROR "ZZ_DIRTY must be exactly true or false")
endif()
if(NOT "${ZZ_LTO}" STREQUAL "true"
   AND NOT "${ZZ_LTO}" STREQUAL "false")
    message(FATAL_ERROR "ZZ_LTO must be exactly true or false")
endif()
if(NOT "${ZZ_LINKAGE}" STREQUAL "shared"
   AND NOT "${ZZ_LINKAGE}" STREQUAL "static")
    message(FATAL_ERROR "ZZ_LINKAGE must be exactly shared or static")
endif()
string(LENGTH "${ZZ_BUILT_AT_UTC}" timestamp_length)
if(NOT timestamp_length EQUAL 20
   OR NOT "${ZZ_BUILT_AT_UTC}" MATCHES
       "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]Z$")
    message(FATAL_ERROR
        "ZZ_BUILT_AT_UTC must match UTC YYYY-MM-DDTHH:MM:SSZ")
endif()

function(zz_json_quote input output_variable)
    # 所有外部字段先转义，再作为 JSON 字符串写入，避免工具链信息破坏结构。
    set(value "${input}")
    string(REPLACE "\\" "\\\\" value "${value}")
    string(REPLACE "\"" "\\\"" value "${value}")
    string(REPLACE "\n" "\\n" value "${value}")
    string(REPLACE "\r" "\\r" value "${value}")
    string(REPLACE "\t" "\\t" value "${value}")
    set(${output_variable} "\"${value}\"" PARENT_SCOPE)
endfunction()

get_filename_component(package_name "${package_path}" NAME)
foreach(json_field IN ITEMS
    package_name
    ZZ_PLATFORM_ID
    ZZ_COMMIT
    ZZ_BUILT_AT_UTC
    ZZ_RUNNER_OS
    ZZ_ARCHITECTURE
    ZZ_QT_VERSION
    ZZ_COMPILER_ID
    ZZ_COMPILER_VERSION
    ZZ_PRESET
    ZZ_LINKAGE)
    zz_json_quote("${${json_field}}" "${json_field}_json")
endforeach()

file(SHA256 "${package_path}" package_sha256)
zz_json_quote("${package_sha256}" package_sha256_json)

set(build_info "{\n")
string(APPEND build_info "  \"schemaVersion\": 1,\n")
string(APPEND build_info "  \"platformId\": ${ZZ_PLATFORM_ID_json},\n")
string(APPEND build_info "  \"packageFile\": ${package_name_json},\n")
string(APPEND build_info "  \"packageSha256\": ${package_sha256_json},\n")
string(APPEND build_info "  \"commit\": ${ZZ_COMMIT_json},\n")
string(APPEND build_info "  \"dirty\": ${ZZ_DIRTY},\n")
string(APPEND build_info "  \"builtAtUtc\": ${ZZ_BUILT_AT_UTC_json},\n")
string(APPEND build_info "  \"runnerOs\": ${ZZ_RUNNER_OS_json},\n")
string(APPEND build_info "  \"architecture\": ${ZZ_ARCHITECTURE_json},\n")
string(APPEND build_info "  \"qtVersion\": ${ZZ_QT_VERSION_json},\n")
string(APPEND build_info "  \"compilerId\": ${ZZ_COMPILER_ID_json},\n")
string(APPEND build_info "  \"compilerVersion\": ${ZZ_COMPILER_VERSION_json},\n")
string(APPEND build_info "  \"preset\": ${ZZ_PRESET_json},\n")
string(APPEND build_info "  \"linkage\": ${ZZ_LINKAGE_json},\n")
string(APPEND build_info "  \"lto\": ${ZZ_LTO}\n")
string(APPEND build_info "}\n")

get_filename_component(package_dir "${package_path}" DIRECTORY)
# 摘要文件与 JSON 都放在包旁边，发布 job 可以按三文件一组下载和复核。
set(checksum_path "${package_path}.sha256")
set(build_info_path "${package_dir}/build-info.json")
foreach(output_path IN ITEMS "${checksum_path}" "${build_info_path}")
    if(IS_SYMLINK "${output_path}")
        message(FATAL_ERROR
            "Build identity output must not be a symlink: ${output_path}")
    endif()
endforeach()
file(WRITE "${checksum_path}"
    "${package_sha256}  ${package_name}\n")
file(WRITE "${build_info_path}" "${build_info}")

message(STATUS "Wrote build identity for ${package_name}")
