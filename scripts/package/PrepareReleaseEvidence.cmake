cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_OUTPUT_DIR OR "${ZZ_OUTPUT_DIR}" STREQUAL "")
    message(FATAL_ERROR "ZZ_OUTPUT_DIR is required")
endif()

cmake_path(ABSOLUTE_PATH ZZ_OUTPUT_DIR
    NORMALIZE OUTPUT_VARIABLE output_dir_input)
if(NOT IS_DIRECTORY "${output_dir_input}"
   OR IS_SYMLINK "${output_dir_input}")
    message(FATAL_ERROR
        "ZZ_OUTPUT_DIR must identify an existing regular directory")
endif()
file(REAL_PATH "${output_dir_input}" output_dir)
cmake_path(GET output_dir ROOT_PATH output_anchor)
file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../.." source_root)
set(source_build_root "${source_root}/build")
cmake_path(IS_PREFIX source_root "${output_dir}"
    NORMALIZE output_is_in_source)
cmake_path(IS_PREFIX source_build_root "${output_dir}"
    NORMALIZE output_is_in_build)
if("${output_dir}" STREQUAL "${output_anchor}"
   OR "${output_dir}" STREQUAL "${source_root}"
   OR (output_is_in_source AND NOT output_is_in_build))
    message(FATAL_ERROR
        "Refusing unsafe release evidence output directory: ${output_dir}")
endif()
file(GLOB output_entries
    LIST_DIRECTORIES true
    "${output_dir}/*"
    "${output_dir}/.[!.]*"
    "${output_dir}/..?*")
if(output_entries)
    message(FATAL_ERROR "ZZ_OUTPUT_DIR must be empty")
endif()

set(manifest_path
    "${source_root}/docs/third-party/release-evidence.json")
if(NOT EXISTS "${manifest_path}"
   OR IS_DIRECTORY "${manifest_path}"
   OR IS_SYMLINK "${manifest_path}")
    message(FATAL_ERROR "Release evidence manifest is unavailable")
endif()
file(READ "${manifest_path}" manifest_json)
string(JSON manifest_type ERROR_VARIABLE manifest_error
    TYPE "${manifest_json}")
if(NOT "${manifest_error}" STREQUAL "NOTFOUND"
   OR NOT "${manifest_type}" STREQUAL "OBJECT")
    message(FATAL_ERROR "Release evidence manifest is invalid JSON")
endif()

function(zz_download_manifest_evidence relative_path url)
    # URL 固定在本脚本，摘要只以已审核 manifest 为唯一真源。
    set(json_path ${ARGN})
    string(JSON expected_sha ERROR_VARIABLE hash_error
        GET "${manifest_json}" ${json_path} sha256)
    string(LENGTH "${expected_sha}" hash_length)
    if(NOT "${hash_error}" STREQUAL "NOTFOUND"
       OR NOT hash_length EQUAL 64
       OR NOT "${expected_sha}" MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "Manifest has no valid SHA-256 for ${relative_path}")
    endif()
    string(JSON manifest_relative_path ERROR_VARIABLE path_error
        GET "${manifest_json}" ${json_path} path)
    if(NOT "${path_error}" STREQUAL "NOTFOUND"
       OR NOT "${manifest_relative_path}" STREQUAL "${relative_path}")
        message(FATAL_ERROR
            "Manifest path mismatch for ${relative_path}")
    endif()

    set(destination "${output_dir}/${relative_path}")
    get_filename_component(destination_dir "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${destination_dir}")
    file(DOWNLOAD
        "${url}"
        "${destination}"
        EXPECTED_HASH "SHA256=${expected_sha}"
        STATUS download_status
        TIMEOUT 300
        INACTIVITY_TIMEOUT 60
        TLS_VERIFY ON)
    list(GET download_status 0 download_code)
    list(GET download_status 1 download_message)
    if(NOT download_code EQUAL 0)
        message(FATAL_ERROR
            "Failed to download ${relative_path}: ${download_message}")
    endif()
endfunction()

set(qwindowkit_commit 2813c1f810cb3fb1999a14ad524124562081f2c2)
zz_download_manifest_evidence(
    "qwindowkit/qwindowkit-${qwindowkit_commit}.tar.gz"
    "https://github.com/stdware/qwindowkit/archive/${qwindowkit_commit}.tar.gz"
    evidence qwindowkit sourceArchive)
string(JSON qt_runtime_version ERROR_VARIABLE qt_runtime_version_error
    GET "${manifest_json}" evidence qtRuntimeLicenses upstreamVersion)
if(NOT "${qt_runtime_version_error}" STREQUAL "NOTFOUND"
   OR NOT "${qt_runtime_version}" MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "Manifest has no valid Qt runtime license version")
endif()
foreach(qt_license_name IN ITEMS
    GPL-2.0-only.txt
    GPL-3.0-only.txt
    LGPL-3.0-only.txt
    LicenseRef-Qt-Commercial.txt
    Qt-GPL-exception-1.0.txt)
    zz_download_manifest_evidence(
        "qt-${qt_runtime_version}/LICENSES/${qt_license_name}"
        "https://raw.githubusercontent.com/qt/qt5/v${qt_runtime_version}/LICENSES/${qt_license_name}"
        evidence qtRuntimeLicenses files "${qt_license_name}")
endforeach()
zz_download_manifest_evidence(
    "qt-5.15.2/qttools-src-shared-winutils-utils.cpp"
    "https://raw.githubusercontent.com/qt/qttools/v5.15.2/src/shared/winutils/utils.cpp"
    evidence windeployqtDerivedWork upstreamSource)
zz_download_manifest_evidence(
    "qt-5.15.2/LICENSE.GPL3-EXCEPT"
    "https://raw.githubusercontent.com/qt/qttools/v5.15.2/LICENSE.GPL3-EXCEPT"
    evidence windeployqtDerivedWork upstreamLicense)

message(STATUS "Prepared release evidence with manifest-locked SHA-256")
