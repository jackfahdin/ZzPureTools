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

set(checkout_action
    "actions/checkout@11d5960a326750d5838078e36cf38b85af677262")
set(setup_python_action
    "actions/setup-python@ece7cb06caefa5fff74198d8649806c4678c61a1")
set(install_qt_action
    "jurplel/install-qt-action/action@48d3ad6db93f3627c8ee7a0454bc6f3744f7e730")
set(upload_action
    "actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02")
set(download_action
    "actions/download-artifact@d3f86a106a0bac45b974a628896c90dbdf5c8093")
set(msvc_action
    "ilammy/msvc-dev-cmd@0b201ec74fa43914dc39ae48a89fd1d8cb592756")

set(required_tokens
    "permissions:"
    "contents: read"
    "contents: write"
    "pull_request:"
    "workflow_dispatch:"
    "ubuntu-22.04"
    "windows-2022"
    "macos-15"
    "macos-15-intel"
    "QT_VERSION: '6.8.3'"
    "QT_QPA_PLATFORM: offscreen"
    "ppa:ubuntu-toolchain-r/test"
    "gcc-13"
    "g++-13"
    "fonts-noto-cjk"
    "fc-match ':lang=zh-cn'"
    "NotoSansCJK-Regular.ttc"
    "${checkout_action}"
    "${setup_python_action}"
    "${install_qt_action}"
    "${upload_action}"
    "${download_action}"
    "${msvc_action}"
    "linux-continuous-release"
    "windows-msvc2022-continuous"
    "windows-mingw-continuous"
    "macos-continuous-arm64"
    "macos-continuous-x86_64"
    "Assert-QtMinGWKit.ps1"
    "PrepareReleaseEvidence.cmake"
    "ZZ_TEST_ROOT=\"\$PWD/build/contracts/release-packaging-support\""
    "package-linux-appimage.sh"
    "package-windows.ps1"
    "package-macos.sh"
    "publish-continuous-build.sh"
    "publish-continuous-build:"
    "needs: [linux, windows-msvc, windows-mingw, macos]"
    "github.event_name != 'pull_request'"
    "github.ref == 'refs/heads/master'"
    "cancel-in-progress: false"
    "continuous-linux-x86_64"
    "continuous-windows-msvc-x86_64"
    "continuous-windows-mingw-x86_64"
    "continuous-macos-arm64"
    "continuous-macos-x86_64"
    "build/linux-continuous-release/ZzFluentUI/tests/reports/fluent-screenshots"
    "build/linux-continuous-release/ZzPureTools/tests/reports/workspace-screenshots"
    "build/linux-continuous-release/examples/ZzPureToolsExample/reports/fluent-screenshots"
    "github.repository"
    "github.sha"
    "github.run_id"
    "if-no-files-found: error"
    "linuxdeploy/releases/download/1-alpha-20251107-1"
    "c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
    "linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1"
    "15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"
    "AppImage/appimagetool/releases/download/1.9.1"
    "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"
    "gcc-mirror/gcc/releases/gcc-13.3.0/COPYING3"
    "8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903"
    "gcc-mirror/gcc/releases/gcc-13.3.0/COPYING.RUNTIME"
    "9d6b43ce4d8de0c878bf16b54d8e7a10d9bd42b75178153e3af6a815bdc90f74")
foreach(required_token IN LISTS required_tokens)
    string(FIND "${workflow}" "${required_token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR
            "GitHub Actions workflow is missing: ${required_token}")
    endif()
endforeach()

set(forbidden_tokens
    "pull_request_target:"
    "ubuntu-24.04"
    "ubuntu-latest"
    "windows-latest"
    "macos-latest"
    "continue-on-error: true"
    "cancel-in-progress: true"
    "ZZ_PERFORMANCE_REFERENCE=ON"
    "ZZ_RELEASE_BUILD=ON"
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
    "macos-clang-release-arm64"
    "macos-clang-static-arm64"
    "macos-clang-release-x86_64"
    "macos-clang-static-x86_64"
    "matrix.presets"
    "actions/checkout@v"
    "actions/setup-python@v"
    "actions/upload-artifact@v"
    "actions/download-artifact@v"
    "jurplel/install-qt-action@v"
    "ilammy/msvc-dev-cmd@v")
foreach(forbidden_token IN LISTS forbidden_tokens)
    string(FIND "${workflow}" "${forbidden_token}" token_position)
    if(NOT token_position EQUAL -1)
        message(FATAL_ERROR
            "GitHub Actions workflow contains forbidden token: ${forbidden_token}")
    endif()
endforeach()

string(REGEX MATCH "uses:[ \t]+[^@\r\n]+@v[0-9]+" floating_action
    "${workflow}")
if(NOT floating_action STREQUAL "")
    message(FATAL_ERROR
        "GitHub Actions workflow contains a floating action: ${floating_action}")
endif()

string(REGEX MATCH "runs-on:[ \t]+[^\r\n]*-latest" floating_runner
    "${workflow}")
if(NOT floating_runner STREQUAL "")
    message(FATAL_ERROR
        "GitHub Actions workflow contains a floating runner: ${floating_runner}")
endif()

string(REGEX MATCHALL "contents:[ \t]+write" write_permissions "${workflow}")
list(LENGTH write_permissions write_permission_count)
if(NOT write_permission_count EQUAL 1)
    message(FATAL_ERROR
        "Exactly one publish job must receive contents: write")
endif()

string(FIND "${workflow}" "publish-continuous-build:" publish_job_position)
string(FIND "${workflow}" "contents: write" write_permission_position)
string(FIND "${workflow}" "publish-continuous-build.sh" publish_call_position)
if(publish_job_position EQUAL -1
   OR write_permission_position LESS publish_job_position
   OR publish_call_position LESS write_permission_position)
    message(FATAL_ERROR
        "Release write permission and publish call must stay inside the publish job")
endif()

string(REGEX MATCHALL
    "cmake --preset[ \t]+\"?linux-continuous-release\"?"
    linux_configure_commands "${workflow}")
list(LENGTH linux_configure_commands linux_configure_count)
if(NOT linux_configure_count EQUAL 1)
    message(FATAL_ERROR
        "Workflow must configure linux-continuous-release exactly once")
endif()

string(REGEX MATCH
    "modules:[^\r\n]*(qtsvg|qttools)"
    base_component_module_declaration
    "${workflow}")
if(NOT base_component_module_declaration STREQUAL "")
    message(FATAL_ERROR
        "QtSvg and QtTools belong to the desktop base kit, not aqt modules")
endif()

message(STATUS "GitHub Actions continuous workflow contract passed")
