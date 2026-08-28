cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR "${ZZ_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
file(REAL_PATH "${ZZ_SOURCE_DIR}" source_dir)

set(package_script "${source_dir}/scripts/package/package-windows.ps1")
if(NOT EXISTS "${package_script}"
   OR IS_DIRECTORY "${package_script}"
   OR IS_SYMLINK "${package_script}")
    message(FATAL_ERROR
        "Windows package script is missing: ${package_script}")
endif()
file(SIZE "${package_script}" package_script_size)
if(package_script_size EQUAL 0)
    message(FATAL_ERROR "Windows package script is empty")
endif()

file(READ "${package_script}" package_script_content)
set(required_script_tokens
    "[ValidateSet('msvc', 'mingw')]"
    "Resolve-Path -LiteralPath"
    "windeployqt.exe"
    "dumpbin"
    "objdump"
    "libgcc_s"
    "libstdc++"
    "vcruntime"
    "msvcp"
    "QT_QPA_PLATFORM"
    "ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS"
    "--smoke-test"
    "ZZ_QT_LICENSE_DIR"
    "StageRuntimeLicenses.cmake"
    "Compress-Archive"
    "WriteBuildInfo.cmake"
    "windows-msvc2022-x86_64"
    "windows-mingw-x86_64")
foreach(required_token IN LISTS required_script_tokens)
    string(FIND "${package_script_content}"
        "${required_token}" required_token_position)
    if(required_token_position EQUAL -1)
        message(FATAL_ERROR
            "Windows package script is missing token: ${required_token}")
    endif()
endforeach()

set(ordered_pipeline_tokens
    "Install-Component -Name 'Runtime'"
    "Install-Component -Name 'ExampleRuntime'"
    "Invoke-WinDeployQt -Executable"
    "Invoke-DeployedSmokeTest -Executable"
    "Assert-PeDependencies -StageRoot"
    "Invoke-StageRuntimeLicenses -StageRoot"
    "Compress-Archive -LiteralPath"
    "Invoke-WriteBuildInfo -PackagePath")
set(previous_position -1)
foreach(ordered_token IN LISTS ordered_pipeline_tokens)
    string(FIND "${package_script_content}"
        "${ordered_token}" ordered_position)
    if(ordered_position EQUAL -1 OR ordered_position LESS previous_position)
        message(FATAL_ERROR
            "Windows package pipeline order is invalid near: ${ordered_token}")
    endif()
    set(previous_position "${ordered_position}")
endforeach()

foreach(forbidden_token IN ITEMS
    "Invoke-WebRequest"
    "Start-BitsTransfer"
    "releases/download/continuous"
    "Remove-Item -LiteralPath \$OutputDir -Recurse"
    "temp_image")
    string(FIND "${package_script_content}"
        "${forbidden_token}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "Windows package script contains forbidden token: ${forbidden_token}")
    endif()
endforeach()

set(presets_path "${source_dir}/CMakePresets.json")
file(READ "${presets_path}" presets_json)
string(JSON root_type ERROR_VARIABLE root_error TYPE "${presets_json}")
if(NOT "${root_error}" STREQUAL "NOTFOUND"
   OR NOT "${root_type}" STREQUAL "OBJECT")
    message(FATAL_ERROR "CMakePresets.json is invalid JSON")
endif()

function(zz_find_named_preset array_name preset_name output_json)
    string(JSON preset_count LENGTH "${presets_json}" "${array_name}")
    set(found_json "")
    if(preset_count GREATER 0)
        math(EXPR last_preset "${preset_count} - 1")
        foreach(index RANGE 0 ${last_preset})
            string(JSON candidate_name
                GET "${presets_json}" "${array_name}" ${index} name)
            if("${candidate_name}" STREQUAL "${preset_name}")
                string(JSON found_json
                    GET "${presets_json}" "${array_name}" ${index})
                break()
            endif()
        endforeach()
    endif()
    if("${found_json}" STREQUAL "")
        message(FATAL_ERROR
            "Missing ${array_name} entry: ${preset_name}")
    endif()
    set(${output_json} "${found_json}" PARENT_SCOPE)
endfunction()

set(windows_continuous_presets
    "windows-msvc2022-continuous|windows-msvc-base"
    "windows-mingw-continuous|windows-mingw-base")
foreach(preset_requirement IN LISTS windows_continuous_presets)
    if(NOT preset_requirement MATCHES "^([^|]+)\\|([^|]+)$")
        message(FATAL_ERROR "Invalid Windows preset contract entry")
    endif()
    set(preset_name "${CMAKE_MATCH_1}")
    set(expected_parent "${CMAKE_MATCH_2}")
    zz_find_named_preset(
        configurePresets "${preset_name}" configure_json)
    string(JSON actual_parent GET "${configure_json}" inherits)
    if(NOT "${actual_parent}" STREQUAL "${expected_parent}")
        message(FATAL_ERROR
            "${preset_name} must inherit ${expected_parent}")
    endif()

    set(required_cache_values
        BUILD_SHARED_LIBS|ON
        ZZ_ENABLE_LTO|ON
        ZZ_BUILD_TESTS|ON
        ZZ_BUILD_EXAMPLES|ON
        ZZ_BUILD_BENCHMARKS|OFF
        ZZ_RELEASE_BUILD|ON)
    if("${preset_name}" STREQUAL "windows-msvc2022-continuous")
        list(APPEND required_cache_values ZZ_ENABLE_MSVC_ANALYZE|OFF)
    else()
        list(APPEND required_cache_values CMAKE_BUILD_TYPE|Release)
    endif()
    foreach(cache_requirement IN LISTS required_cache_values)
        if(NOT cache_requirement MATCHES "^([^|]+)\\|(.*)$")
            message(FATAL_ERROR "Invalid cache contract entry")
        endif()
        set(cache_name "${CMAKE_MATCH_1}")
        set(expected_value "${CMAKE_MATCH_2}")
        string(JSON actual_value ERROR_VARIABLE cache_error
            GET "${configure_json}" cacheVariables "${cache_name}")
        if(NOT "${cache_error}" STREQUAL "NOTFOUND"
           OR NOT "${actual_value}" STREQUAL "${expected_value}")
            message(FATAL_ERROR
                "${preset_name} ${cache_name} must equal ${expected_value}")
        endif()
    endforeach()

    foreach(preset_array IN ITEMS buildPresets testPresets)
        zz_find_named_preset(
            "${preset_array}" "${preset_name}" consumer_json)
        string(JSON configure_preset
            GET "${consumer_json}" configurePreset)
        if(NOT "${configure_preset}" STREQUAL "${preset_name}")
            message(FATAL_ERROR
                "${preset_array} ${preset_name} targets the wrong configure preset")
        endif()
        if("${preset_name}" STREQUAL "windows-msvc2022-continuous")
            string(JSON configuration ERROR_VARIABLE configuration_error
                GET "${consumer_json}" configuration)
            if(NOT "${configuration_error}" STREQUAL "NOTFOUND"
               OR NOT "${configuration}" STREQUAL "Release")
                message(FATAL_ERROR
                    "${preset_array} ${preset_name} must select Release")
            endif()
        endif()
        if("${preset_array}" STREQUAL "testPresets")
            string(JSON test_environment_preset ERROR_VARIABLE environment_error
                GET "${consumer_json}" environment ZZ_CMAKE_PRESET)
            string(JSON test_environment_commit ERROR_VARIABLE commit_env_error
                GET "${consumer_json}" environment ZZ_BENCHMARK_COMMIT)
            if(NOT "${environment_error}" STREQUAL "NOTFOUND"
               OR NOT "${test_environment_preset}" STREQUAL "${preset_name}"
               OR NOT "${commit_env_error}" STREQUAL "NOTFOUND"
               OR NOT "${test_environment_commit}" STREQUAL
                    [=[$penv{ZZ_BENCHMARK_COMMIT}]=])
                message(FATAL_ERROR
                    "testPresets ${preset_name} has invalid environment forwarding")
            endif()
        endif()
    endforeach()
endforeach()

message(STATUS "PASS Windows packaging contract")
