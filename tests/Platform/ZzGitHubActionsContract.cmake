cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "ZZ_SOURCE_DIR must name the source directory")
endif()

set(workflow_path "${ZZ_SOURCE_DIR}/.github/workflows/ci.yml")
if(NOT EXISTS "${workflow_path}" OR IS_DIRECTORY "${workflow_path}")
    message(FATAL_ERROR "GitHub Actions workflow is absent: ${workflow_path}")
endif()
file(SIZE "${workflow_path}" workflow_size)
if(workflow_size EQUAL 0)
    message(FATAL_ERROR "GitHub Actions workflow is empty")
endif()
file(READ "${workflow_path}" workflow)

set(required_tokens
    "permissions:"
    "contents: read"
    "pull_request:"
    "workflow_dispatch:"
    "ubuntu-24.04"
    "windows-2022"
    "macos-15"
    "macos-15-intel"
    "QT_VERSION: '6.8.3'"
    "QT_QPA_PLATFORM: offscreen"
    "modules: qtsvg qttools"
    "actions/checkout@11d5960a326750d5838078e36cf38b85af677262"
    "actions/setup-python@ece7cb06caefa5fff74198d8649806c4678c61a1"
    "jurplel/install-qt-action/action@48d3ad6db93f3627c8ee7a0454bc6f3744f7e730"
    "actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02"
    "ilammy/msvc-dev-cmd@0b201ec74fa43914dc39ae48a89fd1d8cb592756"
    "linux-gcc-debug"
    "linux-gcc-release"
    "linux-static-release"
    "linux-gcc-release-lto"
    "linux-static-release-lto"
    "linux-clang-tidy-release"
    "linux-clang-tidy-static"
    "linux-clang-asan"
    "windows-msvc2022-release"
    "windows-msvc2022-static"
    "windows-mingw-release"
    "windows-mingw-static"
    "Assert-QtMinGWKit.ps1"
    "macos-clang-release-arm64"
    "macos-clang-static-arm64"
    "macos-clang-release-x86_64"
    "macos-clang-static-x86_64"
    "lipo -archs"
    "if: failure()")
foreach(required_token IN LISTS required_tokens)
    string(FIND "${workflow}" "${required_token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR
            "GitHub Actions workflow is missing: ${required_token}")
    endif()
endforeach()

set(forbidden_tokens
    "pull_request_target:"
    "ubuntu-latest"
    "windows-latest"
    "macos-latest"
    "continue-on-error: true"
    "ZZ_PERFORMANCE_REFERENCE=ON"
    "ZZ_RELEASE_BUILD=ON"
    "actions/checkout@v"
    "actions/setup-python@v"
    "actions/upload-artifact@v"
    "jurplel/install-qt-action@v"
    "ilammy/msvc-dev-cmd@v")
foreach(forbidden_token IN LISTS forbidden_tokens)
    string(FIND "${workflow}" "${forbidden_token}" token_position)
    if(NOT token_position EQUAL -1)
        message(FATAL_ERROR
            "GitHub Actions workflow contains forbidden token: ${forbidden_token}")
    endif()
endforeach()

message(STATUS "GitHub Actions workflow contract passed")
