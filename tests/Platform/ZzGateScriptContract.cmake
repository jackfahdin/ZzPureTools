cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "ZZ_SOURCE_DIR must name the source directory")
endif()

set(required_tokens
    "scripts/ci/run-linux-gates.sh|linux-gcc-debug"
    "scripts/ci/run-linux-gates.sh|linux-clang-tidy-release"
    "scripts/ci/run-linux-gates.sh|linux-clang-tidy-static"
    "scripts/ci/run-linux-gates.sh|linux-clang-asan"
    "scripts/ci/run-linux-gates.sh|linux-gcc-release"
    "scripts/ci/run-linux-gates.sh|linux-static-release"
    "scripts/ci/run-linux-gates.sh|linux-gcc-release-lto"
    "scripts/ci/run-linux-gates.sh|linux-static-release-lto"
    "scripts/ci/run-linux-gates.sh|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/run-linux-gates.sh|linux-gcc-benchmarks"
    "scripts/ci/run-linux-gates.sh|linux-clang-asan-benchmarks"
    "scripts/ci/run-linux-gates.sh|ZzComparePerformanceReport.cmake"
    "scripts/ci/run-linux-gates.sh|sha256:\${profile_digest}"
    "scripts/ci/run-linux-gates.sh|ZZ_UBUNTU2204_BUILD_IMAGE"
    "scripts/ci/run-linux-gates.sh|pending-user-validation"
    "scripts/ci/run-ubuntu2204-release-gates.sh|VERSION_ID"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-gcc-release"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-static-release"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-gcc-release-lto"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-static-release-lto"
    "scripts/ci/run-ubuntu2204-release-gates.sh|ZZ_BUNDLE_GNU_RUNTIME=ON"
    "scripts/ci/run-ubuntu2204-release-gates.sh|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/run-windows-gates.ps1|windows-msvc2022-release"
    "scripts/ci/run-windows-gates.ps1|windows-msvc2022-static"
    "scripts/ci/run-windows-gates.ps1|windows-mingw-release"
    "scripts/ci/run-windows-gates.ps1|windows-mingw-static"
    "scripts/ci/run-windows-gates.ps1|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/run-macos-gates.sh|macos-clang-release-arm64"
    "scripts/ci/run-macos-gates.sh|macos-clang-release-x86_64"
    "scripts/ci/run-macos-gates.sh|macos-clang-static-arm64"
    "scripts/ci/run-macos-gates.sh|macos-clang-static-x86_64"
    "scripts/ci/run-macos-gates.sh|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/check-ubuntu2204-runtime.sh|GLIBCXX_"
    "scripts/ci/check-ubuntu2204-runtime.sh|libstdc++.so.6 =>"
    "scripts/ci/check-ubuntu2204-runtime.sh|not found"
    "docs/third-party/THIRD_PARTY_NOTICES.md|GCC Runtime Library Exception")

foreach(requirement IN LISTS required_tokens)
    if(NOT requirement MATCHES "^([^|]+)\\|(.*)$")
        message(FATAL_ERROR "Invalid runner contract entry: ${requirement}")
    endif()
    set(relative_path "${CMAKE_MATCH_1}")
    set(required_token "${CMAKE_MATCH_2}")
    set(file_path "${ZZ_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${file_path}" OR IS_DIRECTORY "${file_path}")
        message(FATAL_ERROR "Required runner file is absent: ${relative_path}")
    endif()
    file(SIZE "${file_path}" file_size)
    if(file_size EQUAL 0)
        message(FATAL_ERROR "Required runner file is empty: ${relative_path}")
    endif()
    file(READ "${file_path}" file_content)
    string(FIND "${file_content}" "${required_token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR
            "${relative_path} is missing required token: ${required_token}")
    endif()
endforeach()

message(STATUS "Native gate script contract passed")
