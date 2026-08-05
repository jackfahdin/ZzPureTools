cmake_minimum_required(VERSION 3.23)

foreach(required IN ITEMS ZZ_BUILD_DIR ZZ_INSTALL_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "缺少 -D${required}=...")
    endif()
endforeach()
if(NOT EXISTS "${ZZ_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "构建目录缺少 CMakeCache.txt：${ZZ_BUILD_DIR}")
endif()

cmake_path(ABSOLUTE_PATH ZZ_BUILD_DIR NORMALIZE OUTPUT_VARIABLE build_dir)
cmake_path(ABSOLUTE_PATH ZZ_INSTALL_ROOT NORMALIZE OUTPUT_VARIABLE install_root)
cmake_path(GET install_root ROOT_PATH install_anchor)
cmake_path(IS_PREFIX build_dir "${install_root}"
    NORMALIZE install_is_below_build)
if("${install_root}" STREQUAL "${install_anchor}"
   OR "${install_root}" STREQUAL "${build_dir}"
   OR NOT install_is_below_build)
    message(FATAL_ERROR "拒绝不安全的许可证审计安装目录：${install_root}")
endif()

file(STRINGS "${build_dir}/CMakeCache.txt" release_cache
    REGEX "^ZZ_RELEASE_BUILD:BOOL=ON$")
if(NOT release_cache)
    message(FATAL_ERROR "许可证安装审计只接受 ZZ_RELEASE_BUILD=ON 的构建")
endif()
file(STRINGS "${build_dir}/CMakeCache.txt" runtime_cache
    REGEX "^ZZ_BUNDLE_GNU_RUNTIME:BOOL=ON$")

file(REMOVE_RECURSE "${install_root}")
set(install_command
    "${CMAKE_COMMAND}" --install "${build_dir}" --prefix "${install_root}")
if(DEFINED ZZ_CONFIG AND NOT "${ZZ_CONFIG}" STREQUAL "")
    list(APPEND install_command --config "${ZZ_CONFIG}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_stdout
    ERROR_VARIABLE install_stderr)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "发布安装失败，退出码 ${install_result}\n"
        "stdout:\n${install_stdout}\n"
        "stderr:\n${install_stderr}")
endif()

set(required_files
    share/ZzPureToolsPro/licenses/PROJECT-LICENSE
    share/ZzPureToolsPro/licenses/qwindowkit/LICENSE
    share/ZzPureToolsPro/licenses/qwindowkit/qmsetup-LICENSE
    share/ZzPureToolsPro/licenses/qwindowkit/syscmdline-LICENSE
    share/ZzPureToolsPro/licenses/ZzLog/LICENSE
    share/ZzPureToolsPro/licenses/ZzLog/spdlog-LICENSE.txt
    share/ZzPureToolsPro/licenses/ZzLog/fmt-LICENSE.txt
    share/ZzPureToolsPro/THIRD_PARTY_NOTICES.md
    share/ZzPureToolsPro/qwindowkit-vendor.json
    share/ZzPureToolsPro/release-evidence.json
    share/ZzPureToolsPro/reviews/project-license-approval.json
    share/ZzPureToolsPro/reviews/qwindowkit-provenance-review.json
    share/ZzPureToolsPro/reviews/windeployqt-redistribution-review.json)
if(runtime_cache)
    list(APPEND required_files
        share/ZzPureToolsPro/licenses/gcc-runtime/COPYING3
        share/ZzPureToolsPro/licenses/gcc-runtime/COPYING.RUNTIME)
endif()

foreach(relative_file IN LISTS required_files)
    set(installed_file "${install_root}/${relative_file}")
    if(NOT EXISTS "${installed_file}"
       OR IS_DIRECTORY "${installed_file}"
       OR IS_SYMLINK "${installed_file}")
        message(FATAL_ERROR "发布包缺少普通文件：${relative_file}")
    endif()
    file(SIZE "${installed_file}" installed_size)
    if(installed_size EQUAL 0)
        message(FATAL_ERROR "发布包文件为空：${relative_file}")
    endif()
endforeach()

set(runtime_license_root
    "${install_root}/share/ZzPureToolsPro/licenses/gcc-runtime")
if(NOT runtime_cache
   AND (EXISTS "${runtime_license_root}/COPYING3"
        OR EXISTS "${runtime_license_root}/COPYING.RUNTIME"))
    message(FATAL_ERROR "未捆绑 GNU runtime 的发布包含有陈旧运行库许可证")
endif()

foreach(forbidden_tool IN ITEMS bin/qmcorecmd bin/qmcorecmd.exe)
    if(EXISTS "${install_root}/${forbidden_tool}")
        message(FATAL_ERROR
            "Qt 派生构建工具不得进入 ZzPureToolsPro 安装包：${forbidden_tool}")
    endif()
endforeach()

set(notices_path
    "${install_root}/share/ZzPureToolsPro/THIRD_PARTY_NOTICES.md")
file(READ "${notices_path}" notices)
set(required_notice_tokens
    QWindowKit
    FramelessHelper
    qmsetup
    syscmdline
    spdlog
    fmt
    "GCC Runtime Library Exception"
    "GPL-3.0-only WITH Qt-GPL-exception-1.0"
    Jackfahdin
    "1.5.1.0"
    "https://github.com/stdware/qwindowkit"
    "d24088deaa441a79267df8ae3dbc567fbe2a5e03"
    "12.1.0"
    "SPDX"
    "share/ZzPureToolsPro/licenses/PROJECT-LICENSE"
    "share/ZzPureToolsPro/licenses/qwindowkit/LICENSE"
    "share/ZzPureToolsPro/licenses/qwindowkit/qmsetup-LICENSE"
    "share/ZzPureToolsPro/licenses/qwindowkit/syscmdline-LICENSE"
    "share/ZzPureToolsPro/licenses/ZzLog/LICENSE"
    "share/ZzPureToolsPro/licenses/ZzLog/spdlog-LICENSE.txt"
    "share/ZzPureToolsPro/licenses/ZzLog/fmt-LICENSE.txt"
    "share/ZzPureToolsPro/licenses/gcc-runtime/COPYING3"
    "share/ZzPureToolsPro/licenses/gcc-runtime/COPYING.RUNTIME"
    "share/ZzPureToolsPro/reviews/project-license-approval.json"
    "share/ZzPureToolsPro/reviews/qwindowkit-provenance-review.json"
    "share/ZzPureToolsPro/reviews/windeployqt-redistribution-review.json")
foreach(token IN LISTS required_notice_tokens)
    string(FIND "${notices}" "${token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR
            "THIRD_PARTY_NOTICES.md 缺少必需信息：${token}")
    endif()
endforeach()

message(STATUS "正式发布许可证、通知和证据安装审计通过")
