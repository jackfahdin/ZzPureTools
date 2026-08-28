cmake_minimum_required(VERSION 3.23)

foreach(required_variable IN ITEMS ZZ_SOURCE_DIR ZZ_TEST_ROOT)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(REAL_PATH "${ZZ_SOURCE_DIR}" source_dir)
cmake_path(ABSOLUTE_PATH ZZ_TEST_ROOT NORMALIZE OUTPUT_VARIABLE test_root)
set(build_root "${source_dir}/build")
cmake_path(IS_PREFIX build_root "${test_root}"
    NORMALIZE test_root_is_below_build)
if(NOT test_root_is_below_build OR "${test_root}" STREQUAL "${build_root}")
    message(FATAL_ERROR "ZZ_TEST_ROOT must be a child of the repository build directory")
endif()

set(package_script_dir "${source_dir}/scripts/package")
set(verify_script "${package_script_dir}/VerifyArtifactSet.cmake")
if(NOT EXISTS "${verify_script}")
    message(FATAL_ERROR
        "Missing release packaging script: ${verify_script}")
endif()
set(write_info_script "${package_script_dir}/WriteBuildInfo.cmake")
set(stage_licenses_script "${package_script_dir}/StageRuntimeLicenses.cmake")
set(stage_msvc_runtime_script
    "${package_script_dir}/StageMsvcRuntime.cmake")
set(prepare_evidence_script "${package_script_dir}/PrepareReleaseEvidence.cmake")
foreach(script_path IN ITEMS
    "${write_info_script}"
    "${stage_licenses_script}"
    "${stage_msvc_runtime_script}"
    "${prepare_evidence_script}")
    if(NOT EXISTS "${script_path}")
        message(FATAL_ERROR
            "Missing release packaging script: ${script_path}")
    endif()
endforeach()

file(REMOVE_RECURSE "${test_root}")
file(MAKE_DIRECTORY "${test_root}")

set(msvc_redist_root "${test_root}/msvc-redist")
set(msvc_crt_dir "${msvc_redist_root}/x64/Microsoft.VC143.CRT")
set(msvc_stage "${test_root}/msvc-stage")
file(MAKE_DIRECTORY "${msvc_crt_dir}" "${msvc_stage}/bin")
foreach(runtime_name IN ITEMS
        concrt140.dll
        msvcp140.dll
        vcruntime140.dll)
    file(WRITE "${msvc_crt_dir}/${runtime_name}"
        "${runtime_name} fixture\n")
endforeach()
file(WRITE "${msvc_crt_dir}/vc_redist.x64.exe"
    "bootstrapper fixture must not be staged\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_MSVC_REDIST_DIR=${msvc_redist_root}"
        "-DZZ_STAGE_ROOT=${msvc_stage}"
        -P "${stage_msvc_runtime_script}"
    RESULT_VARIABLE msvc_stage_result
    OUTPUT_VARIABLE msvc_stage_stdout
    ERROR_VARIABLE msvc_stage_stderr)
if(NOT msvc_stage_result EQUAL 0)
    message(FATAL_ERROR
        "StageMsvcRuntime rejected a valid VS 2022 CRT fixture\n"
        "stdout:\n${msvc_stage_stdout}\nstderr:\n${msvc_stage_stderr}")
endif()
foreach(runtime_name IN ITEMS
        concrt140.dll
        msvcp140.dll
        vcruntime140.dll)
    set(staged_runtime "${msvc_stage}/bin/${runtime_name}")
    if(NOT EXISTS "${staged_runtime}" OR IS_DIRECTORY "${staged_runtime}")
        message(FATAL_ERROR
            "StageMsvcRuntime omitted CRT file: ${runtime_name}")
    endif()
    file(SIZE "${staged_runtime}" staged_runtime_size)
    if(staged_runtime_size EQUAL 0)
        message(FATAL_ERROR
            "StageMsvcRuntime produced an empty CRT file: ${runtime_name}")
    endif()
endforeach()
if(EXISTS "${msvc_stage}/bin/vc_redist.x64.exe")
    message(FATAL_ERROR
        "StageMsvcRuntime copied the installer instead of app-local CRT DLLs")
endif()

set(incomplete_msvc_redist_root "${test_root}/incomplete-msvc-redist")
set(incomplete_msvc_crt_dir
    "${incomplete_msvc_redist_root}/x64/Microsoft.VC143.CRT")
set(incomplete_msvc_stage "${test_root}/incomplete-msvc-stage")
file(MAKE_DIRECTORY
    "${incomplete_msvc_crt_dir}" "${incomplete_msvc_stage}/bin")
file(WRITE "${incomplete_msvc_crt_dir}/vcruntime140.dll"
    "vcruntime fixture\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_MSVC_REDIST_DIR=${incomplete_msvc_redist_root}"
        "-DZZ_STAGE_ROOT=${incomplete_msvc_stage}"
        -P "${stage_msvc_runtime_script}"
    RESULT_VARIABLE incomplete_msvc_stage_result
    OUTPUT_QUIET ERROR_QUIET)
if(incomplete_msvc_stage_result EQUAL 0)
    message(FATAL_ERROR
        "StageMsvcRuntime accepted an incomplete CRT directory")
endif()

set(expected_commit 0123456789abcdef0123456789abcdef01234567)
string(SUBSTRING "${expected_commit}" 0 12 short_commit)
set(valid_root "${test_root}/valid-artifacts")

function(zz_create_artifact platform_id extension architecture)
    set(artifact_dir "${valid_root}/${platform_id}")
    file(MAKE_DIRECTORY "${artifact_dir}")
    set(package_name
        "ZzPureToolsExample-continuous-${platform_id}-${short_commit}.${extension}")
    set(package_path "${artifact_dir}/${package_name}")
    file(WRITE "${package_path}"
        "fixture package bytes for ${platform_id}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DZZ_PACKAGE_PATH=${package_path}"
            "-DZZ_PLATFORM_ID=${platform_id}"
            "-DZZ_COMMIT=${expected_commit}"
            -DZZ_DIRTY=false
            -DZZ_BUILT_AT_UTC=2026-08-28T00:00:00Z
            -DZZ_RUNNER_OS=fixture-os
            "-DZZ_ARCHITECTURE=${architecture}"
            -DZZ_QT_VERSION=6.8.3
            -DZZ_COMPILER_ID=fixture-compiler
            -DZZ_COMPILER_VERSION=1.0.0
            -DZZ_PRESET=fixture-continuous
            -DZZ_LINKAGE=shared
            -DZZ_LTO=true
            -P "${write_info_script}"
        RESULT_VARIABLE write_result
        OUTPUT_VARIABLE write_stdout
        ERROR_VARIABLE write_stderr)
    if(NOT write_result EQUAL 0)
        message(FATAL_ERROR
            "WriteBuildInfo rejected valid fixture for ${platform_id}\n"
            "stdout:\n${write_stdout}\nstderr:\n${write_stderr}")
    endif()
endfunction()

zz_create_artifact(linux-x86_64 AppImage x86_64)
zz_create_artifact(windows-msvc2022-x86_64 zip x86_64)
zz_create_artifact(windows-mingw-x86_64 zip x86_64)
zz_create_artifact(macos-arm64 dmg arm64)
zz_create_artifact(macos-x86_64 dmg x86_64)

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_ARTIFACT_ROOT=${valid_root}"
        "-DZZ_EXPECTED_COMMIT=${expected_commit}"
        -P "${verify_script}"
    RESULT_VARIABLE valid_result
    OUTPUT_VARIABLE valid_stdout
    ERROR_VARIABLE valid_stderr)
if(NOT valid_result EQUAL 0)
    message(FATAL_ERROR
        "Valid continuous artifact set was rejected\n"
        "stdout:\n${valid_stdout}\nstderr:\n${valid_stderr}")
endif()

set(sample_info "${valid_root}/linux-x86_64/build-info.json")
file(READ "${sample_info}" sample_json)
string(JSON sample_type ERROR_VARIABLE sample_json_error TYPE "${sample_json}")
if(NOT "${sample_json_error}" STREQUAL "NOTFOUND"
   OR NOT "${sample_type}" STREQUAL "OBJECT")
    message(FATAL_ERROR "WriteBuildInfo did not produce a valid JSON object")
endif()
string(JSON sample_sha GET "${sample_json}" packageSha256)
set(sample_package
    "${valid_root}/linux-x86_64/ZzPureToolsExample-continuous-linux-x86_64-${short_commit}.AppImage")
file(SHA256 "${sample_package}" expected_sample_sha)
if(NOT "${sample_sha}" STREQUAL "${expected_sample_sha}")
    message(FATAL_ERROR "WriteBuildInfo package digest is incorrect")
endif()
set(sample_checksum "${sample_package}.sha256")
file(READ "${sample_checksum}" sample_checksum_content)
string(STRIP "${sample_checksum_content}" sample_checksum_content)
get_filename_component(sample_package_name "${sample_package}" NAME)
if(NOT "${sample_checksum_content}" STREQUAL
   "${expected_sample_sha}  ${sample_package_name}")
    message(FATAL_ERROR "WriteBuildInfo checksum file has an invalid format")
endif()

set(mismatch_root "${test_root}/commit-mismatch")
file(MAKE_DIRECTORY "${mismatch_root}")
file(COPY "${valid_root}/" DESTINATION "${mismatch_root}")
set(mismatch_info "${mismatch_root}/macos-arm64/build-info.json")
file(READ "${mismatch_info}" mismatch_json)
string(REPLACE "${expected_commit}"
    "1123456789abcdef0123456789abcdef01234567"
    mismatch_json "${mismatch_json}")
file(WRITE "${mismatch_info}" "${mismatch_json}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_ARTIFACT_ROOT=${mismatch_root}"
        "-DZZ_EXPECTED_COMMIT=${expected_commit}"
        -P "${verify_script}"
    RESULT_VARIABLE mismatch_result
    OUTPUT_QUIET ERROR_QUIET)
if(mismatch_result EQUAL 0)
    message(FATAL_ERROR "Artifact verifier accepted a cross-commit set")
endif()

set(missing_root "${test_root}/missing-platform")
file(MAKE_DIRECTORY "${missing_root}")
file(COPY "${valid_root}/" DESTINATION "${missing_root}")
file(REMOVE_RECURSE "${missing_root}/macos-x86_64")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_ARTIFACT_ROOT=${missing_root}"
        "-DZZ_EXPECTED_COMMIT=${expected_commit}"
        -P "${verify_script}"
    RESULT_VARIABLE missing_result
    OUTPUT_QUIET ERROR_QUIET)
if(missing_result EQUAL 0)
    message(FATAL_ERROR "Artifact verifier accepted a missing macOS package")
endif()

set(tampered_root "${test_root}/tampered-package")
file(MAKE_DIRECTORY "${tampered_root}")
file(COPY "${valid_root}/" DESTINATION "${tampered_root}")
set(tampered_package
    "${tampered_root}/windows-mingw-x86_64/ZzPureToolsExample-continuous-windows-mingw-x86_64-${short_commit}.zip")
file(APPEND "${tampered_package}" "tampered bytes\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_ARTIFACT_ROOT=${tampered_root}"
        "-DZZ_EXPECTED_COMMIT=${expected_commit}"
        -P "${verify_script}"
    RESULT_VARIABLE tampered_result
    OUTPUT_QUIET ERROR_QUIET)
if(tampered_result EQUAL 0)
    message(FATAL_ERROR "Artifact verifier accepted modified package bytes")
endif()

set(qt_license_dir "${test_root}/qt-licenses")
file(MAKE_DIRECTORY "${qt_license_dir}")
file(WRITE "${qt_license_dir}/LGPL-3.0-only.txt"
    "GNU LESSER GENERAL PUBLIC LICENSE Version 3 fixture\n")
file(WRITE "${qt_license_dir}/GPL-3.0-only.txt"
    "GNU GENERAL PUBLIC LICENSE Version 3 fixture\n")
set(stage_root "${test_root}/valid-stage")
file(MAKE_DIRECTORY "${stage_root}/bin")
file(WRITE "${stage_root}/bin/Qt6Core.dll" "Qt runtime fixture\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_STAGE_ROOT=${stage_root}"
        "-DZZ_QT_LICENSE_DIR=${qt_license_dir}"
        -P "${stage_licenses_script}"
    RESULT_VARIABLE stage_result
    OUTPUT_VARIABLE stage_stdout
    ERROR_VARIABLE stage_stderr)
if(NOT stage_result EQUAL 0)
    message(FATAL_ERROR
        "StageRuntimeLicenses rejected a valid fixture\n"
        "stdout:\n${stage_stdout}\nstderr:\n${stage_stderr}")
endif()
foreach(staged_path IN ITEMS
    licenses/ZzPureToolsFrame/LICENSE
    licenses/QWindowKit/LICENSE
    licenses/ZzLog/spdlog-LICENSE.txt
    licenses/Qt/LGPL-3.0-only.txt
    licenses/Qt/GPL-3.0-only.txt
    licenses/Qt/DEPLOYED_MODULES.txt
    THIRD_PARTY_NOTICES.md)
    set(full_staged_path "${stage_root}/${staged_path}")
    if(NOT EXISTS "${full_staged_path}"
       OR IS_DIRECTORY "${full_staged_path}")
        message(FATAL_ERROR "Staged license file is missing: ${staged_path}")
    endif()
    file(SIZE "${full_staged_path}" staged_size)
    if(staged_size EQUAL 0)
        message(FATAL_ERROR "Staged license file is empty: ${staged_path}")
    endif()
endforeach()

set(incomplete_qt_license_dir "${test_root}/qt-licenses-without-lgpl")
file(MAKE_DIRECTORY "${incomplete_qt_license_dir}")
file(WRITE "${incomplete_qt_license_dir}/GPL-3.0-only.txt"
    "GNU GENERAL PUBLIC LICENSE Version 3 fixture\n")
set(incomplete_stage "${test_root}/stage-without-lgpl")
file(MAKE_DIRECTORY "${incomplete_stage}/bin")
file(WRITE "${incomplete_stage}/bin/Qt6Core.dll" "Qt runtime fixture\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_STAGE_ROOT=${incomplete_stage}"
        "-DZZ_QT_LICENSE_DIR=${incomplete_qt_license_dir}"
        -P "${stage_licenses_script}"
    RESULT_VARIABLE incomplete_stage_result
    OUTPUT_QUIET ERROR_QUIET)
if(incomplete_stage_result EQUAL 0)
    message(FATAL_ERROR "StageRuntimeLicenses accepted a missing Qt LGPL text")
endif()

set(missing_qt_license_stage "${test_root}/stage-without-qt-license-input")
file(MAKE_DIRECTORY "${missing_qt_license_stage}/bin")
file(WRITE "${missing_qt_license_stage}/bin/Qt6Core.dll"
    "Qt runtime fixture\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_STAGE_ROOT=${missing_qt_license_stage}"
        -P "${stage_licenses_script}"
    RESULT_VARIABLE missing_qt_license_result
    OUTPUT_QUIET ERROR_QUIET)
if(missing_qt_license_result EQUAL 0)
    message(FATAL_ERROR
        "StageRuntimeLicenses accepted a missing Qt license input")
endif()

set(gnu_license_dir "${test_root}/gnu-licenses")
file(MAKE_DIRECTORY "${gnu_license_dir}")
file(WRITE "${gnu_license_dir}/COPYING3" "GPLv3 fixture\n")
file(WRITE "${gnu_license_dir}/COPYING.RUNTIME"
    "GCC Runtime Library Exception fixture\n")
set(gnu_stage "${test_root}/gnu-stage")
file(MAKE_DIRECTORY "${gnu_stage}/lib")
file(WRITE "${gnu_stage}/lib/libQt6Core.so.6" "Qt runtime fixture\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_STAGE_ROOT=${gnu_stage}"
        "-DZZ_QT_LICENSE_DIR=${qt_license_dir}"
        "-DZZ_GNU_RUNTIME_LICENSE_DIR=${gnu_license_dir}"
        -P "${stage_licenses_script}"
    RESULT_VARIABLE gnu_stage_result
    OUTPUT_VARIABLE gnu_stage_stdout
    ERROR_VARIABLE gnu_stage_stderr)
if(NOT gnu_stage_result EQUAL 0)
    message(FATAL_ERROR
        "StageRuntimeLicenses rejected valid GNU runtime licenses\n"
        "stdout:\n${gnu_stage_stdout}\nstderr:\n${gnu_stage_stderr}")
endif()
foreach(gnu_license IN ITEMS COPYING3 COPYING.RUNTIME)
    if(NOT EXISTS "${gnu_stage}/licenses/GNU-runtime/${gnu_license}")
        message(FATAL_ERROR "GNU runtime license was not staged: ${gnu_license}")
    endif()
endforeach()

if(UNIX)
    set(outside_stage_file "${test_root}/outside-stage-file")
    file(WRITE "${outside_stage_file}" "outside fixture\n")
    set(symlink_stage "${test_root}/symlink-stage")
    file(MAKE_DIRECTORY "${symlink_stage}/bin")
    file(WRITE "${symlink_stage}/bin/libQt6Core.so.6"
        "Qt runtime fixture\n")
    file(CREATE_LINK
        "${outside_stage_file}"
        "${symlink_stage}/outside-link"
        SYMBOLIC
        RESULT symlink_result)
    if(NOT "${symlink_result}" STREQUAL "0")
        message(FATAL_ERROR
            "Unable to create the symlink escape fixture: ${symlink_result}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DZZ_STAGE_ROOT=${symlink_stage}"
            "-DZZ_QT_LICENSE_DIR=${qt_license_dir}"
            -P "${stage_licenses_script}"
        RESULT_VARIABLE symlink_stage_result
        OUTPUT_QUIET ERROR_QUIET)
    if(symlink_stage_result EQUAL 0)
        message(FATAL_ERROR
            "StageRuntimeLicenses accepted a symlink escaping the stage root")
    endif()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_OUTPUT_DIR=${source_dir}"
        -P "${prepare_evidence_script}"
    RESULT_VARIABLE unsafe_evidence_result
    OUTPUT_QUIET ERROR_QUIET)
if(unsafe_evidence_result EQUAL 0)
    message(FATAL_ERROR "PrepareReleaseEvidence accepted the source root as output")
endif()

set(nonempty_evidence_dir "${test_root}/nonempty-evidence")
file(MAKE_DIRECTORY "${nonempty_evidence_dir}")
file(WRITE "${nonempty_evidence_dir}/existing-file" "existing fixture\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_OUTPUT_DIR=${nonempty_evidence_dir}"
        -P "${prepare_evidence_script}"
    RESULT_VARIABLE nonempty_evidence_result
    OUTPUT_QUIET ERROR_QUIET)
if(nonempty_evidence_result EQUAL 0)
    message(FATAL_ERROR
        "PrepareReleaseEvidence accepted a nonempty output directory")
endif()

message(STATUS "PASS release packaging support contract")
