include_directories("${PROJECT_SOURCE_DIR}/tests/support")

add_executable(ZzExampleWorkspaceSmokeTest
    ZzExampleWorkspaceSmokeTest.cpp
    ../ZzExampleActivityModel.cpp
    ../ZzExampleActivityModel.h
    ../ZzExampleActivityModelPrivate.h
    ../ZzExampleAboutWindow.cpp
    ../ZzExampleAboutWindow.h
    ../ZzExampleAboutWindowPrivate.cpp
    ../ZzExampleAboutWindowPrivate.h
    ../ZzExampleApplicationContext.cpp
    ../ZzExampleApplicationContext.h
    ../ZzExampleApplicationContextPrivate.cpp
    ../ZzExampleApplicationContextPrivate.h
    ../ZzExampleRouteCatalog.cpp
    ../ZzExampleRouteCatalog.h
    ../ZzExampleSessionModel.cpp
    ../ZzExampleSessionModel.h
    ../ZzExampleSettingsWindow.cpp
    ../ZzExampleSettingsWindow.h
    ../ZzExampleSettingsWindowPrivate.cpp
    ../ZzExampleSettingsWindowPrivate.h
    ../ZzExampleSystemPage.cpp
    ../ZzExampleSystemPage.h
    ../ZzExampleSystemPagePrivate.cpp
    ../ZzExampleSystemPagePrivate.h
    ../ZzExampleSystemPageKind.h
    ../ZzExampleSystemPresenter.cpp
    ../ZzExampleSystemPresenter.h
    ../ZzExampleSystemPresenterPrivate.cpp
    ../ZzExampleSystemPresenterPrivate.h
    ../ZzExampleSystemViewModel.cpp
    ../ZzExampleSystemViewModel.h
    ../ZzExampleWindowShell.cpp
    ../ZzExampleWindowShell.h
    ../ZzExampleWindowShellPrivate.cpp
    ../ZzExampleWindowShellPrivate.h
    ../ZzExampleWorkspaceContent.cpp
    ../ZzExampleWorkspaceContent.h
)

qt_add_resources(ZzExampleWorkspaceSmokeTest "zz_example_workspace_test_icons"
    PREFIX "/ZzPureToolsExample/workspace-icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../resources"
    FILES
        ../resources/Sessions.svg
        ../resources/Files.svg
        ../resources/Properties.svg
        ../resources/Tasks.svg
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
set(zz_example_workspace_environment "QT_QPA_PLATFORM=offscreen")
if(APPLE)
    set(zz_example_workspace_environment "QT_QPA_PLATFORM=cocoa")
endif()
set_tests_properties(example.workspace-smoke PROPERTIES
    ENVIRONMENT "${zz_example_workspace_environment}"
    LABELS "example;smoke"
    TIMEOUT 20
)
