cmake_minimum_required(VERSION 3.23)

foreach(required_variable IN ITEMS ZZ_STAGE_ROOT ZZ_QT_LICENSE_DIR)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ZZ_STAGE_ROOT
    NORMALIZE OUTPUT_VARIABLE stage_root)
cmake_path(ABSOLUTE_PATH ZZ_QT_LICENSE_DIR
    NORMALIZE OUTPUT_VARIABLE qt_license_dir_input)
if(NOT IS_DIRECTORY "${stage_root}" OR IS_SYMLINK "${stage_root}")
    message(FATAL_ERROR "ZZ_STAGE_ROOT must identify a regular directory")
endif()
if(NOT IS_DIRECTORY "${qt_license_dir_input}"
   OR IS_SYMLINK "${qt_license_dir_input}")
    message(FATAL_ERROR
        "ZZ_QT_LICENSE_DIR must identify a regular directory")
endif()
file(REAL_PATH "${qt_license_dir_input}" qt_license_dir)

foreach(stale_output IN ITEMS
    "${stage_root}/licenses"
    "${stage_root}/THIRD_PARTY_NOTICES.md")
    if(EXISTS "${stale_output}" OR IS_SYMLINK "${stale_output}")
        message(FATAL_ERROR
            "License staging output must not already exist: ${stale_output}")
    endif()
endforeach()

file(GLOB_RECURSE stage_entries
    LIST_DIRECTORIES true "${stage_root}/*")
set(deployed_qt_modules)
foreach(stage_entry IN LISTS stage_entries)
    # 包内符号链接可以保留，但解析后的目标绝不能越过部署根目录。
    if(IS_SYMLINK "${stage_entry}")
        if(NOT EXISTS "${stage_entry}")
            message(FATAL_ERROR
                "Deployment stage contains a broken symlink: ${stage_entry}")
        endif()
        file(REAL_PATH "${stage_entry}" symlink_target)
        cmake_path(IS_PREFIX stage_root "${symlink_target}"
            NORMALIZE symlink_stays_in_stage)
        if(NOT symlink_stays_in_stage)
            message(FATAL_ERROR
                "Deployment stage symlink escapes its input root: ${stage_entry}")
        endif()
    endif()

    get_filename_component(stage_entry_name "${stage_entry}" NAME)
    # 从最终部署树采集模块，而不是从构建时 find_package 结果推测随包内容。
    if(stage_entry_name MATCHES "^Qt6.*\\.dll$"
       OR stage_entry_name MATCHES "^libQt6.*\\.so(\\..*)?$"
       OR stage_entry_name MATCHES "^libQt6.*\\.dylib$"
       OR stage_entry MATCHES "/Qt[^/]+\\.framework($|/)")
        file(RELATIVE_PATH relative_qt_module
            "${stage_root}" "${stage_entry}")
        list(APPEND deployed_qt_modules "${relative_qt_module}")
    endif()
endforeach()
list(REMOVE_DUPLICATES deployed_qt_modules)
list(SORT deployed_qt_modules)
if(NOT deployed_qt_modules)
    message(FATAL_ERROR
        "Deployment stage contains no recognizable Qt runtime module")
endif()

file(GLOB qt_license_files LIST_DIRECTORIES false "${qt_license_dir}/*")
if(NOT qt_license_files)
    message(FATAL_ERROR "Qt LICENSES directory is empty")
endif()

set(has_qt_lgpl FALSE)
set(has_qt_gpl FALSE)
foreach(qt_license_file IN LISTS qt_license_files)
    if(IS_SYMLINK "${qt_license_file}")
        message(FATAL_ERROR
            "Qt license inputs must not be symlinks: ${qt_license_file}")
    endif()
    file(SIZE "${qt_license_file}" qt_license_size)
    if(qt_license_size EQUAL 0)
        message(FATAL_ERROR "Qt license input is empty: ${qt_license_file}")
    endif()
    file(READ "${qt_license_file}" qt_license_content)
    string(FIND "${qt_license_content}"
        "GNU LESSER GENERAL PUBLIC LICENSE" lgpl_index)
    if(NOT lgpl_index EQUAL -1)
        set(has_qt_lgpl TRUE)
    endif()
    string(FIND "${qt_license_content}"
        "GNU GENERAL PUBLIC LICENSE" gpl_index)
    if(NOT gpl_index EQUAL -1)
        set(has_qt_gpl TRUE)
    endif()
endforeach()
if(NOT has_qt_lgpl OR NOT has_qt_gpl)
    message(FATAL_ERROR
        "Qt LICENSES must include complete LGPL and GPL license texts")
endif()

file(REAL_PATH "${CMAKE_CURRENT_LIST_DIR}/../.." source_root)

function(zz_stage_regular_file source_path relative_destination)
    # 许可证输入必须是稳定普通文件，避免复制时跟随仓库外符号链接。
    if(NOT EXISTS "${source_path}"
       OR IS_DIRECTORY "${source_path}"
       OR IS_SYMLINK "${source_path}")
        message(FATAL_ERROR
            "License input must be a regular non-symlink file: ${source_path}")
    endif()
    file(SIZE "${source_path}" source_size)
    if(source_size EQUAL 0)
        message(FATAL_ERROR "License input is empty: ${source_path}")
    endif()
    set(destination "${stage_root}/${relative_destination}")
    get_filename_component(destination_dir "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${destination_dir}")
    configure_file("${source_path}" "${destination}" COPYONLY)
endfunction()

zz_stage_regular_file(
    "${source_root}/LICENSE"
    licenses/ZzPureToolsFrame/LICENSE)
zz_stage_regular_file(
    "${source_root}/ZzThirdParty/qwindowkit/LICENSE"
    licenses/QWindowKit/LICENSE)
zz_stage_regular_file(
    "${source_root}/ZzThirdParty/qwindowkit/qmsetup/LICENSE"
    licenses/QWindowKit/qmsetup-LICENSE)
zz_stage_regular_file(
    "${source_root}/ZzThirdParty/qwindowkit/qmsetup/src/syscmdline/LICENSE"
    licenses/QWindowKit/syscmdline-LICENSE)
zz_stage_regular_file(
    "${source_root}/ZzThirdParty/ZzLog/LICENSE"
    licenses/ZzLog/LICENSE)
zz_stage_regular_file(
    "${source_root}/ZzThirdParty/ZzLog/licenses/spdlog/LICENSE.txt"
    licenses/ZzLog/spdlog-LICENSE.txt)
zz_stage_regular_file(
    "${source_root}/ZzThirdParty/ZzLog/licenses/fmt/LICENSE.txt"
    licenses/ZzLog/fmt-LICENSE.txt)
zz_stage_regular_file(
    "${source_root}/docs/third-party/THIRD_PARTY_NOTICES.md"
    THIRD_PARTY_NOTICES.md)

foreach(qt_license_file IN LISTS qt_license_files)
    get_filename_component(qt_license_name "${qt_license_file}" NAME)
    zz_stage_regular_file(
        "${qt_license_file}" "licenses/Qt/${qt_license_name}")
endforeach()
string(REPLACE ";" "\n" deployed_qt_module_text
    "${deployed_qt_modules}")
file(WRITE "${stage_root}/licenses/Qt/DEPLOYED_MODULES.txt"
    "${deployed_qt_module_text}\n")

if(DEFINED ZZ_GNU_RUNTIME_LICENSE_DIR
   AND NOT "${ZZ_GNU_RUNTIME_LICENSE_DIR}" STREQUAL "")
    cmake_path(ABSOLUTE_PATH ZZ_GNU_RUNTIME_LICENSE_DIR
        NORMALIZE OUTPUT_VARIABLE gnu_license_dir_input)
    if(NOT IS_DIRECTORY "${gnu_license_dir_input}"
       OR IS_SYMLINK "${gnu_license_dir_input}")
        message(FATAL_ERROR
            "ZZ_GNU_RUNTIME_LICENSE_DIR must be a regular directory")
    endif()
    foreach(gnu_license IN ITEMS COPYING3 COPYING.RUNTIME)
        zz_stage_regular_file(
            "${gnu_license_dir_input}/${gnu_license}"
            "licenses/GNU-runtime/${gnu_license}")
    endforeach()
endif()

message(STATUS "Staged runtime licenses and Qt module inventory")
