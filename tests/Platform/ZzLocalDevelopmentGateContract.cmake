cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "ZZ_SOURCE_DIR 必须指向源码根目录")
endif()

set(bash_relative_path scripts/ci/run-local-development-gate.sh)
set(powershell_relative_path scripts/ci/run-local-development-gate.ps1)
set(bash_path "${ZZ_SOURCE_DIR}/${bash_relative_path}")
set(powershell_path "${ZZ_SOURCE_DIR}/${powershell_relative_path}")

foreach(script_path IN ITEMS "${bash_path}" "${powershell_path}")
    if(NOT EXISTS "${script_path}"
       OR IS_DIRECTORY "${script_path}"
       OR IS_SYMLINK "${script_path}")
        message(FATAL_ERROR "本机开发门禁入口缺失或类型错误：${script_path}")
    endif()
    file(SIZE "${script_path}" script_size)
    if(script_size EQUAL 0)
        message(FATAL_ERROR "本机开发门禁入口不能为空：${script_path}")
    endif()
endforeach()

file(READ "${bash_path}" bash_content)
file(READ "${powershell_path}" powershell_content)

set(common_tokens
    "git"
    "diff"
    "--check"
    "diff --cached --check"
    "cmake"
    "ZZ_BUILD_TESTS=ON"
    "ZZ_BUILD_EXAMPLES=ON"
    "ZZ_BUILD_BENCHMARKS=OFF"
    "ZZ_WARNINGS_AS_ERRORS=ON"
    "ctest"
    "benchmark|screenshot|install|packaging|release"
    "Total Tests"
    "ZzDocumentationAudit.cmake"
    "待验证")
foreach(script_name IN ITEMS bash powershell)
    set(content "${${script_name}_content}")
    foreach(token IN LISTS common_tokens)
        string(FIND "${content}" "${token}" token_position)
        if(token_position EQUAL -1)
            message(FATAL_ERROR
                "${script_name} 本机门禁缺少行为合同：${token}")
        endif()
    endforeach()
endforeach()

foreach(token IN ITEMS
    "--preset"
    "--tests"
    "--docs-only"
    "linux-gcc-debug"
    "macos-clang-release-arm64"
    "macos-clang-release-x86_64"
    "uname -s")
    string(FIND "${bash_content}" "${token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR "Bash 本机门禁缺少合同：${token}")
    endif()
endforeach()

foreach(token IN ITEMS
    "[CmdletBinding"
    "ParameterSetName = 'Build'"
    "ParameterSetName = 'Docs'"
    "windows-msvc2022-release"
    "windows-mingw-release"
    "$LASTEXITCODE"
    "$IsWindows")
    string(FIND "${powershell_content}" "${token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR "PowerShell 本机门禁缺少合同：${token}")
    endif()
endforeach()

foreach(forbidden_token IN ITEMS
    "git config"
    "git add"
    "git commit"
    "curl "
    "wget "
    "Invoke-WebRequest"
    "Start-BitsTransfer")
    foreach(script_name IN ITEMS bash powershell)
        string(FIND "${${script_name}_content}"
            "${forbidden_token}" forbidden_position)
        if(NOT forbidden_position EQUAL -1)
            message(FATAL_ERROR
                "${script_name} 本机门禁包含禁止副作用：${forbidden_token}")
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND bash -n "${bash_path}"
    RESULT_VARIABLE bash_syntax_result
    ERROR_VARIABLE bash_syntax_error)
if(NOT bash_syntax_result EQUAL 0)
    message(FATAL_ERROR "Bash 本机门禁语法错误：${bash_syntax_error}")
endif()

execute_process(
    COMMAND bash "${bash_path}" --help
    RESULT_VARIABLE bash_help_result
    OUTPUT_VARIABLE bash_help_output
    ERROR_VARIABLE bash_help_error)
if(NOT bash_help_result EQUAL 0
   OR NOT bash_help_output MATCHES "usage:"
   OR NOT bash_help_output MATCHES "--docs-only")
    message(FATAL_ERROR
        "Bash 本机门禁帮助合同失败：${bash_help_output}${bash_help_error}")
endif()

function(zz_expect_bash_failure expected_error)
    execute_process(
        COMMAND bash "${bash_path}" ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error)
    if(command_result EQUAL 0)
        message(FATAL_ERROR "Bash 本机门禁错误接受参数：${ARGN}")
    endif()
    set(combined_output "${command_output}${command_error}")
    string(FIND "${combined_output}" "${expected_error}" error_position)
    if(error_position EQUAL -1)
        message(FATAL_ERROR
            "Bash 本机门禁错误信息不明确：期望 ${expected_error}，实际 ${combined_output}")
    endif()
endfunction()

zz_expect_bash_failure("不支持的 preset"
    --preset linux-static-release --tests platform.compile)
zz_expect_bash_failure("必须同时提供"
    --preset linux-gcc-debug)
zz_expect_bash_failure("不能与"
    --docs-only --tests platform.compile)

find_program(pwsh_program NAMES pwsh)
if(pwsh_program)
    execute_process(
        COMMAND "${pwsh_program}" -NoProfile -Command
            "$errors = $null; [System.Management.Automation.Language.Parser]::ParseFile('${powershell_path}', [ref]$null, [ref]$errors) > $null; if ($errors.Count -ne 0) { $errors | ForEach-Object { Write-Error $_ }; exit 1 }"
        RESULT_VARIABLE powershell_syntax_result
        ERROR_VARIABLE powershell_syntax_error)
    if(NOT powershell_syntax_result EQUAL 0)
        message(FATAL_ERROR
            "PowerShell 本机门禁语法错误：${powershell_syntax_error}")
    endif()
endif()

foreach(document IN ITEMS
    docs/development/BUILDING_ZH.md
    docs/development/CODING_STANDARD_ZH.md)
    file(READ "${ZZ_SOURCE_DIR}/${document}" document_content)
    foreach(token IN ITEMS
        "run-local-development-gate.sh"
        "run-local-development-gate.ps1"
        "待验证")
        string(FIND "${document_content}" "${token}" token_position)
        if(token_position EQUAL -1)
            message(FATAL_ERROR "${document} 缺少本机门禁说明：${token}")
        endif()
    endforeach()
endforeach()

message(STATUS "跨平台本机开发门禁合同通过")
