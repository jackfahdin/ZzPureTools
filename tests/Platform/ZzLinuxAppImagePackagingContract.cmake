cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR "${ZZ_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
file(REAL_PATH "${ZZ_SOURCE_DIR}" source_dir)

set(package_script
    "${source_dir}/scripts/package/package-linux-appimage.sh")
if(NOT EXISTS "${package_script}"
   OR IS_DIRECTORY "${package_script}"
   OR IS_SYMLINK "${package_script}")
    message(FATAL_ERROR
        "Linux AppImage package script is missing: ${package_script}")
endif()
file(SIZE "${package_script}" package_script_size)
if(package_script_size EQUAL 0)
    message(FATAL_ERROR "Linux AppImage package script is empty")
endif()

execute_process(
    COMMAND bash "${package_script}"
    RESULT_VARIABLE no_argument_result
    OUTPUT_QUIET ERROR_QUIET)
if(no_argument_result EQUAL 0)
    message(FATAL_ERROR "Linux AppImage script accepted missing arguments")
endif()

if(NOT DEFINED ZZ_TEST_ROOT OR "${ZZ_TEST_ROOT}" STREQUAL "")
    set(ZZ_TEST_ROOT
        "${source_dir}/build/contracts/linux-appimage-packaging-contract")
endif()
file(REMOVE_RECURSE "${ZZ_TEST_ROOT}")
file(MAKE_DIRECTORY
    "${ZZ_TEST_ROOT}/source/scripts/package"
    "${ZZ_TEST_ROOT}/source/build/linux-continuous-release"
    "${ZZ_TEST_ROOT}/qt/bin"
    "${ZZ_TEST_ROOT}/evidence/qt-6.8.3/LICENSES"
    "${ZZ_TEST_ROOT}/gnu-licenses"
    "${ZZ_TEST_ROOT}/other-qt"
    "${ZZ_TEST_ROOT}/tools"
    "${ZZ_TEST_ROOT}/output"
    "${ZZ_TEST_ROOT}/bin")
file(COPY_FILE "${package_script}"
    "${ZZ_TEST_ROOT}/source/scripts/package/package-linux-appimage.sh")

file(WRITE "${ZZ_TEST_ROOT}/qt/bin/qmake" [=[#!/usr/bin/env bash
set -euo pipefail
[[ ${1:-} == -query && ${2:-} == QT_VERSION ]]
printf '%s\n' 6.8.3
]=])
file(WRITE "${ZZ_TEST_ROOT}/bin/xvfb-run" [=[#!/usr/bin/env bash
exit 0
]=])
foreach(tool_name IN ITEMS linuxdeploy qt-plugin appimagetool)
    file(WRITE "${ZZ_TEST_ROOT}/tools/${tool_name}" [=[#!/usr/bin/env bash
exit 0
]=])
endforeach()
file(CHMOD
    "${ZZ_TEST_ROOT}/qt/bin/qmake"
    "${ZZ_TEST_ROOT}/bin/xvfb-run"
    "${ZZ_TEST_ROOT}/tools/linuxdeploy"
    "${ZZ_TEST_ROOT}/tools/qt-plugin"
    "${ZZ_TEST_ROOT}/tools/appimagetool"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
file(WRITE "${ZZ_TEST_ROOT}/gnu-licenses/COPYING3" "GPL test fixture\n")
file(WRITE "${ZZ_TEST_ROOT}/gnu-licenses/COPYING.RUNTIME"
    "Runtime exception test fixture\n")

foreach(true_spelling IN ITEMS ON TRUE YES 1 Y)
    file(WRITE
        "${ZZ_TEST_ROOT}/source/build/linux-continuous-release/CMakeCache.txt"
        "CMAKE_BUILD_TYPE:STRING=Release\n"
        "BUILD_SHARED_LIBS:BOOL=${true_spelling}\n"
        "ZZ_ENABLE_LTO:BOOL=${true_spelling}\n"
        "ZZ_BUILD_TESTS:BOOL=${true_spelling}\n"
        "ZZ_BUILD_EXAMPLES:BOOL=${true_spelling}\n"
        "ZZ_RELEASE_BUILD:BOOL=${true_spelling}\n"
        "ZZ_BUNDLE_GNU_RUNTIME:BOOL=${true_spelling}\n"
        "ZZ_QT_PREFIX:PATH=${ZZ_TEST_ROOT}/other-qt\n"
        "ZZ_RELEASE_EVIDENCE_ROOT:PATH=${ZZ_TEST_ROOT}/evidence\n"
        "ZZ_GNU_RUNTIME_LICENSE_DIR:PATH=${ZZ_TEST_ROOT}/gnu-licenses\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
            "PATH=${ZZ_TEST_ROOT}/bin:$ENV{PATH}"
            bash
            "${ZZ_TEST_ROOT}/source/scripts/package/package-linux-appimage.sh"
            --build-dir
            "${ZZ_TEST_ROOT}/source/build/linux-continuous-release"
            --qt-root "${ZZ_TEST_ROOT}/qt"
            --evidence-root "${ZZ_TEST_ROOT}/evidence"
            --gnu-license-dir "${ZZ_TEST_ROOT}/gnu-licenses"
            --linuxdeploy "${ZZ_TEST_ROOT}/tools/linuxdeploy"
            --qt-plugin "${ZZ_TEST_ROOT}/tools/qt-plugin"
            --appimagetool "${ZZ_TEST_ROOT}/tools/appimagetool"
            --output-dir "${ZZ_TEST_ROOT}/output"
            --commit aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
            --built-at-utc 2026-08-28T00:00:00Z
        RESULT_VARIABLE boolean_result
        OUTPUT_VARIABLE boolean_stdout
        ERROR_VARIABLE boolean_stderr)
    if(boolean_result EQUAL 0)
        message(FATAL_ERROR
            "Linux AppImage script passed the intentional path mismatch for ${true_spelling}")
    endif()
    string(FIND "${boolean_stderr}"
        "qt-root does not match configured ZZ_QT_PREFIX"
        path_mismatch_position)
    if(path_mismatch_position EQUAL -1)
        message(FATAL_ERROR
            "Linux AppImage script rejected CMake true spelling ${true_spelling} "
            "before the path-mismatch sentinel; stderr=${boolean_stderr}")
    endif()
endforeach()

file(READ "${package_script}" package_script_content)
set(required_script_tokens
    "realpath"
    "cmake --install"
    "--component Runtime"
    "--component ExampleRuntime"
    "--plugin qt"
    "QMAKE="
    "ZZ_QT_LICENSE_DIR"
    "StageRuntimeLicenses.cmake"
    "appimagetool"
    "--appimage-extract"
    "check-ubuntu2204-runtime.sh"
    "xvfb-run -a env"
    "WriteBuildInfo.cmake")
foreach(required_token IN LISTS required_script_tokens)
    string(FIND "${package_script_content}"
        "${required_token}" required_token_position)
    if(required_token_position EQUAL -1)
        message(FATAL_ERROR
            "Linux AppImage script is missing token: ${required_token}")
    endif()
endforeach()

set(ordered_script_tokens
    "--component Runtime"
    "--component ExampleRuntime"
    "--plugin qt"
    "StageRuntimeLicenses.cmake"
    "appimagetool\" \"\$appdir"
    "--appimage-extract"
    "check-ubuntu2204-runtime.sh"
    "xvfb-run -a env"
    "WriteBuildInfo.cmake")
set(previous_position -1)
foreach(ordered_token IN LISTS ordered_script_tokens)
    string(FIND "${package_script_content}"
        "${ordered_token}" ordered_position)
    if(ordered_position EQUAL -1 OR ordered_position LESS previous_position)
        message(FATAL_ERROR
            "Linux AppImage pipeline order is invalid near: ${ordered_token}")
    endif()
    set(previous_position "${ordered_position}")
endforeach()

foreach(forbidden_token IN ITEMS
    "curl "
    "wget "
    "linuxdeploy/releases/download/continuous"
    "rm -rf \"\$output_dir\""
    "temp_image")
    string(FIND "${package_script_content}"
        "${forbidden_token}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "Linux AppImage script contains forbidden token: ${forbidden_token}")
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

zz_find_named_preset(
    configurePresets linux-continuous-release continuous_configure)
string(JSON continuous_parent GET "${continuous_configure}" inherits)
if(NOT "${continuous_parent}" STREQUAL "linux-gcc13-base")
    message(FATAL_ERROR
        "linux-continuous-release must inherit linux-gcc13-base")
endif()

set(required_cache_values
    CMAKE_BUILD_TYPE|Release
    BUILD_SHARED_LIBS|ON
    ZZ_ENABLE_LTO|ON
    ZZ_BUILD_TESTS|ON
    ZZ_BUILD_EXAMPLES|ON
    ZZ_RELEASE_BUILD|ON
    ZZ_BUNDLE_GNU_RUNTIME|ON)
foreach(cache_requirement IN LISTS required_cache_values)
    if(NOT cache_requirement MATCHES "^([^|]+)\\|(.*)$")
        message(FATAL_ERROR "Invalid cache contract entry")
    endif()
    set(cache_name "${CMAKE_MATCH_1}")
    set(expected_value "${CMAKE_MATCH_2}")
    string(JSON actual_value ERROR_VARIABLE cache_error
        GET "${continuous_configure}" cacheVariables "${cache_name}")
    if(NOT "${cache_error}" STREQUAL "NOTFOUND"
       OR NOT "${actual_value}" STREQUAL "${expected_value}")
        message(FATAL_ERROR
            "linux-continuous-release ${cache_name} must equal ${expected_value}")
    endif()
endforeach()

foreach(preset_array IN ITEMS buildPresets testPresets)
    zz_find_named_preset(
        "${preset_array}" linux-continuous-release continuous_consumer)
    string(JSON configure_preset
        GET "${continuous_consumer}" configurePreset)
    if(NOT "${configure_preset}" STREQUAL "linux-continuous-release")
        message(FATAL_ERROR
            "${preset_array} linux-continuous-release targets the wrong configure preset")
    endif()
endforeach()

set(example_cmake
    "${source_dir}/examples/ZzPureToolsExample/CMakeLists.txt")
file(READ "${example_cmake}" example_cmake_content)
string(FIND "${example_cmake_content}"
    [=[INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"]=]
    example_rpath_position)
if(example_rpath_position EQUAL -1)
    message(FATAL_ERROR
        "ZzPureToolsExample lacks a relocatable Linux install RPATH")
endif()

message(STATUS "PASS Linux AppImage packaging contract")
