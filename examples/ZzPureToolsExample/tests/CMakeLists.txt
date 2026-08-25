add_executable(ZzExampleWorkspaceSmokeTest
    ZzExampleWorkspaceSmokeTest.cpp
    ../ZzExampleActivityModel.cpp
    ../ZzExampleActivityModel.h
    ../ZzExampleActivityModelPrivate.h
    ../ZzExampleApplicationContext.cpp
    ../ZzExampleApplicationContext.h
    ../ZzExampleApplicationContextPrivate.cpp
    ../ZzExampleApplicationContextPrivate.h
    ../ZzExampleRouteCatalog.cpp
    ../ZzExampleRouteCatalog.h
    ../ZzExampleSessionModel.cpp
    ../ZzExampleSessionModel.h
    ../ZzExampleWindowShell.cpp
    ../ZzExampleWindowShell.h
    ../ZzExampleWindowShellPrivate.cpp
    ../ZzExampleWindowShellPrivate.h
    ../ZzExampleWorkspaceContent.cpp
    ../ZzExampleWorkspaceContent.h
)

target_include_directories(ZzExampleWorkspaceSmokeTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(ZzExampleWorkspaceSmokeTest PRIVATE
    Qt6::Test
    Zz::FluentUI
    Zz::PureTools
    Zz::WindowKit
    ZzLog::ZzLog
)
set_target_properties(ZzExampleWorkspaceSmokeTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzExampleWorkspaceSmokeTest)
zz_enable_sanitizers(ZzExampleWorkspaceSmokeTest)

add_test(
    NAME example.workspace-smoke
    COMMAND ZzExampleWorkspaceSmokeTest
)
set_tests_properties(example.workspace-smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "example;smoke"
    TIMEOUT 20
)
