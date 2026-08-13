cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "ZZ_SOURCE_DIR must name the source directory")
endif()

set(script
    "${ZZ_SOURCE_DIR}/scripts/release/run-linux-desktop-acceptance.sh")
set(checklist
    "${ZZ_SOURCE_DIR}/docs/release/MANUAL_LINUX_CHECKLIST_ZH.md")
foreach(required_file IN ITEMS "${script}" "${checklist}")
    if(NOT EXISTS "${required_file}" OR IS_DIRECTORY "${required_file}")
        message(FATAL_ERROR "Linux desktop acceptance file is absent: ${required_file}")
    endif()
endforeach()

file(READ "${script}" script_content)
file(READ "${checklist}" checklist_content)
set(required_tokens
    "linux-x11-kde"
    "linux-x11-gnome"
    "linux-wayland-kde"
    "linux-wayland-gnome"
    "linux-qt-fallback"
    "ZZ_WINDOWKIT_FORCE_QT_CONTEXT:BOOL="
    "CMAKE_BUILD_TYPE:STRING="
    "BUILD_SHARED_LIBS:BOOL="
    "Qt6_DIR:PATH="
    "git status --porcelain"
    "[[ \"\$status_line\" == \"?? temp_image/\" ]]"
    "source.status=tracked-clean"
    "source.localUntrackedInputs=\$local_untracked_inputs"
    "cmake --build"
    "ZzPureToolsExample"
    "sha256sum"
    "QT_QPA_PLATFORM"
    "require_local_desktop_session"
    "loginctl show-session \"\$XDG_SESSION_ID\" -p Remote --value"
    "login_remote\" == no && \"\$login_active\" == yes"
    "login_type\" == \"\$session_type"
    "session.loginId=\$XDG_SESSION_ID"
    "require_x11_output"
    "xrandr --listmonitors"
    "require_wayland_output"
    "interface: 'wl_output'"
    "linux-native.log"
    "RESULT_ZH.md")
foreach(required_token IN LISTS required_tokens)
    string(FIND "${script_content}" "${required_token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR
            "Linux desktop script is missing token: ${required_token}")
    endif()
endforeach()
string(FIND "${checklist_content}"
    "run-linux-desktop-acceptance.sh" checklist_script_position)
if(checklist_script_position EQUAL -1)
    message(FATAL_ERROR "Linux checklist does not reference the acceptance script")
endif()
foreach(required_checklist_token IN ITEMS
    "build/linux-qt-fallback-release"
    "-DZZ_WINDOWKIT_FORCE_QT_CONTEXT=ON"
    "-DXKB_INCLUDE_DIR=\"$PWD/build/dependencies/xkbcommon/root/usr/include\""
    "-DXKB_LIBRARY=/usr/lib/x86_64-linux-gnu/libxkbcommon.so.0")
    string(FIND "${checklist_content}" "${required_checklist_token}"
        checklist_token_position)
    if(checklist_token_position EQUAL -1)
        message(FATAL_ERROR
            "Linux checklist is missing token: ${required_checklist_token}")
    endif()
endforeach()

execute_process(
    COMMAND bash -n "${script}"
    RESULT_VARIABLE syntax_result
    ERROR_VARIABLE syntax_error)
if(NOT syntax_result EQUAL 0)
    message(FATAL_ERROR "Linux desktop script syntax failed: ${syntax_error}")
endif()

execute_process(
    COMMAND bash "${script}" --help
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error)
if(NOT help_result EQUAL 0 OR NOT help_output MATCHES "usage:")
    message(FATAL_ERROR
        "Linux desktop script help failed: ${help_output}${help_error}")
endif()
foreach(required_help_token IN ITEMS
    "tracked source tree must be clean"
    "exact top-level temp_image/ directory"
    "never reads")
    string(FIND "${help_output}" "${required_help_token}" help_token_position)
    if(help_token_position EQUAL -1)
        message(FATAL_ERROR
            "Linux desktop script help is missing token: ${required_help_token}")
    endif()
endforeach()

execute_process(
    COMMAND bash "${script}" --session invalid --build-dir build/invalid
    RESULT_VARIABLE invalid_result
    OUTPUT_QUIET
    ERROR_QUIET)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "Linux desktop script accepted an invalid session id")
endif()

message(STATUS "Linux desktop acceptance script contract passed")
