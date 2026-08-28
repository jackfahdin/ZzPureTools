cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR "${ZZ_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
file(REAL_PATH "${ZZ_SOURCE_DIR}" source_dir)

set(package_script "${source_dir}/scripts/package/package-macos.sh")
if(NOT EXISTS "${package_script}"
   OR IS_DIRECTORY "${package_script}"
   OR IS_SYMLINK "${package_script}")
    message(FATAL_ERROR
        "macOS package script is missing: ${package_script}")
endif()
file(SIZE "${package_script}" package_script_size)
if(package_script_size EQUAL 0)
    message(FATAL_ERROR "macOS package script is empty")
endif()

execute_process(
    COMMAND bash "${package_script}"
    RESULT_VARIABLE no_argument_result
    OUTPUT_QUIET ERROR_QUIET)
if(no_argument_result EQUAL 0)
    message(FATAL_ERROR "macOS package script accepted missing arguments")
endif()

file(READ "${package_script}" package_script_content)
set(required_script_tokens
    "arm64|x86_64"
    "macdeployqt"
    "-always-overwrite"
    "-extra-plugins="
    "lipo"
    "otool"
    "hdiutil"
    "-readonly"
    "-nobrowse"
    "QT_QPA_PLATFORM=offscreen"
    "ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1500"
    "--smoke-test"
    "StageRuntimeLicenses.cmake"
    "WriteBuildInfo.cmake"
    "macos-arm64"
    "macos-x86_64")
foreach(required_token IN LISTS required_script_tokens)
    string(FIND "${package_script_content}"
        "${required_token}" required_token_position)
    if(required_token_position EQUAL -1)
        message(FATAL_ERROR
            "macOS package script is missing token: ${required_token}")
    endif()
endforeach()

set(ordered_pipeline_tokens
    "install_component Runtime"
    "install_component ExampleRuntime"
    "invoke_macdeployqt \"\$app_bundle\""
    "audit_app_bundle \"\$app_bundle\""
    "invoke_app_smoke \"\$app_executable\""
    "stage_runtime_licenses \"\$app_bundle\""
    "create_dmg \"\$app_bundle\""
    "attach_dmg \"\$working_package\""
    "invoke_app_smoke \"\$mounted_executable\""
    "detach_dmg \"\$mount_dir\""
    "write_build_info \"\$working_package\"")
set(previous_position -1)
foreach(ordered_token IN LISTS ordered_pipeline_tokens)
    string(FIND "${package_script_content}"
        "${ordered_token}" ordered_position)
    if(ordered_position EQUAL -1 OR ordered_position LESS previous_position)
        message(FATAL_ERROR
            "macOS package pipeline order is invalid near: ${ordered_token}")
    endif()
    set(previous_position "${ordered_position}")
endforeach()

foreach(forbidden_token IN ITEMS
    "curl "
    "wget "
    "releases/download/continuous"
    "-mindepth"
    "-maxdepth"
    "rm -rf \"\$output_dir\""
    "temp_image")
    string(FIND "${package_script_content}"
        "${forbidden_token}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "macOS package script contains forbidden token: ${forbidden_token}")
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

set(macos_continuous_presets
    "macos-continuous-arm64|arm64|\$env{QT_MACOS_ARM64_ROOT}"
    "macos-continuous-x86_64|x86_64|\$env{QT_MACOS_X86_64_ROOT}")
foreach(preset_requirement IN LISTS macos_continuous_presets)
    if(NOT preset_requirement MATCHES "^([^|]+)\\|([^|]+)\\|([^|]+)$")
        message(FATAL_ERROR "Invalid macOS preset contract entry")
    endif()
    set(preset_name "${CMAKE_MATCH_1}")
    set(expected_architecture "${CMAKE_MATCH_2}")
    set(expected_qt_root "${CMAKE_MATCH_3}")
    zz_find_named_preset(
        configurePresets "${preset_name}" configure_json)
    string(JSON actual_parent GET "${configure_json}" inherits)
    if(NOT "${actual_parent}" STREQUAL "macos-clang-base")
        message(FATAL_ERROR
            "${preset_name} must inherit macos-clang-base")
    endif()

    set(required_cache_values
        CMAKE_BUILD_TYPE|Release
        CMAKE_OSX_ARCHITECTURES|${expected_architecture}
        CMAKE_PREFIX_PATH|${expected_qt_root}
        ZZ_QT_PREFIX|${expected_qt_root}
        BUILD_SHARED_LIBS|ON
        ZZ_ENABLE_LTO|ON
        ZZ_ENABLE_CLANG_TIDY|OFF
        ZZ_BUILD_TESTS|ON
        ZZ_BUILD_EXAMPLES|ON
        ZZ_BUILD_BENCHMARKS|OFF
        ZZ_RELEASE_BUILD|ON)
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
        if("${preset_array}" STREQUAL "testPresets")
            string(JSON environment_preset ERROR_VARIABLE environment_error
                GET "${consumer_json}" environment ZZ_CMAKE_PRESET)
            if(NOT "${environment_error}" STREQUAL "NOTFOUND"
               OR NOT "${environment_preset}" STREQUAL "${preset_name}")
                message(FATAL_ERROR
                    "testPresets ${preset_name} records the wrong preset")
            endif()
        endif()
    endforeach()
endforeach()

message(STATUS "PASS macOS packaging contract")
