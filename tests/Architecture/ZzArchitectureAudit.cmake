cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_SOURCE_DIR ZZ_TARGET_MANIFEST)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

list(PREPEND CMAKE_MODULE_PATH "${ZZ_SOURCE_DIR}/cmake")
include("${ZZ_SOURCE_DIR}/cmake/ZzArchitectureChecks.cmake")
zz_run_complete_architecture_audit(
    "${ZZ_SOURCE_DIR}" "${ZZ_TARGET_MANIFEST}")

set(zz_workspace_sources
    ZzFluentUI/widgets/src/ZzActivityBar.cpp
    ZzFluentUI/widgets/src/ZzCommandPalette.cpp
    ZzFluentUI/widgets/src/ZzDockPanel.cpp
    ZzFluentUI/widgets/src/ZzExplorerPane.cpp
    ZzFluentUI/widgets/src/ZzFluentTitleBar.cpp
    ZzFluentUI/widgets/src/ZzSidePane.cpp
    ZzPureTools/widgets/src/ZzWorkspaceShell.cpp
)
foreach(zz_workspace_source IN LISTS zz_workspace_sources)
    set(zz_workspace_path "${ZZ_SOURCE_DIR}/${zz_workspace_source}")
    if(NOT EXISTS "${zz_workspace_path}")
        message(FATAL_ERROR
            "Workspace architecture source is missing: ${zz_workspace_source}")
    endif()
    file(READ "${zz_workspace_path}" zz_workspace_content)
    zz_architecture_strip_tokens(
        "${zz_workspace_content}" zz_workspace_code)
    string(TOLOWER "${zz_workspace_code}" zz_workspace_code)
    if(zz_workspace_code MATCHES
       "#[ \t]*include[ \t]*[<\"][^>\"]*(ssh|sftp|network|setting|repository|database|domain)[^>\"]*[>\"]")
        message(FATAL_ERROR
            "Workspace presentation source has a forbidden business dependency: ${zz_workspace_source}")
    endif()
endforeach()
