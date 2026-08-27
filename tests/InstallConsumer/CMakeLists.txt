cmake_minimum_required(VERSION 3.23)

project(ZzInstallConsumer LANGUAGES CXX)

include(GNUInstallDirs)
enable_testing()

find_package(ZzPureToolsPro 0.1 CONFIG REQUIRED)

set(zz_installed_header_specs
    "Zz::Core|ZzCore/ZzCoreVersion.h"
    "Zz::Core|ZzCore/ZzCoreExport.h"
    "Zz::WindowKit|ZzWindowKit/ZzWindowKitVersion.h"
    "Zz::WindowKit|ZzWindowKit/ZzWindowKitExport.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzFluentVersion.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzFluentFoundationExport.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzNavigationDisplayMode.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzNavigationItemRole.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzNavigationPlacement.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzTitleBarMenuDisplayMode.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzActivityArea.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzActivityItemRole.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzSidePaneEdge.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzCommandItemRole.h"
    "Zz::FluentFoundation|ZzFluentUI/ZzTabGroupId.h"
    "Zz::FluentUI|ZzFluentUI/ZzFluentWidgetVersion.h"
    "Zz::FluentUI|ZzFluentUI/ZzFluentUIExport.h"
    "Zz::FluentUI|ZzFluentUI/ZzPushButton.h"
    "Zz::FluentUI|ZzFluentUI/ZzCalendar.h"
    "Zz::FluentUI|ZzFluentUI/ZzCalendarPicker.h"
    "Zz::FluentUI|ZzFluentUI/ZzProgressRing.h"
    "Zz::FluentUI|ZzFluentUI/ZzScrollArea.h"
    "Zz::FluentUI|ZzFluentUI/ZzScrollBar.h"
    "Zz::FluentUI|ZzFluentUI/ZzAnnotatedScrollBar.h"
    "Zz::FluentUI|ZzFluentUI/ZzFlowLayout.h"
    "Zz::FluentUI|ZzFluentUI/ZzSpinBox.h"
    "Zz::FluentUI|ZzFluentUI/ZzDoubleSpinBox.h"
    "Zz::FluentUI|ZzFluentUI/ZzMultiSelectComboBox.h"
    "Zz::FluentUI|ZzFluentUI/ZzNavigationPane.h"
    "Zz::FluentUI|ZzFluentUI/ZzRoller.h"
    "Zz::FluentUI|ZzFluentUI/ZzRollerPicker.h"
    "Zz::FluentUI|ZzFluentUI/ZzTabBar.h"
    "Zz::FluentUI|ZzFluentUI/ZzTabWidget.h"
    "Zz::FluentUI|ZzFluentUI/ZzSplitWorkspace.h"
    "Zz::FluentUI|ZzFluentUI/ZzFluentTitleBar.h"
    "Zz::FluentUI|ZzFluentUI/ZzActivityBar.h"
    "Zz::FluentUI|ZzFluentUI/ZzPanelStack.h"
    "Zz::FluentUI|ZzFluentUI/ZzSidePane.h"
    "Zz::FluentUI|ZzFluentUI/ZzBottomPane.h"
    "Zz::FluentUI|ZzFluentUI/ZzExplorerPane.h"
    "Zz::FluentUI|ZzFluentUI/ZzCommandPalette.h"
    "Zz::FluentUI|ZzFluentUI/ZzCommandBar.h"
    "Zz::FluentUI|ZzFluentUI/ZzDockPanel.h"
    "Zz::FluentUI|ZzFluentUI/ZzPivot.h"
    "Zz::FluentUI|ZzFluentUI/ZzSplitButton.h"
    "Zz::AppCore|ZzPureTools/ZzAppCoreVersion.h"
    "Zz::AppCore|ZzPureTools/ZzAppCoreExport.h"
    "Zz::PureTools|ZzPureTools/ZzPureToolsVersion.h"
    "Zz::PureTools|ZzPureTools/ZzPureToolsExport.h"
    "Zz::PureTools|ZzPureTools/ZzWorkspaceActivityId.h"
    "Zz::PureTools|ZzPureTools/ZzWorkspacePanelId.h"
    "Zz::PureTools|ZzPureTools/ZzWorkspaceTitleMode.h"
    "Zz::PureTools|ZzPureTools/ZzWorkspaceShell.h"
)

add_custom_target(ZzInstalledPublicHeadersTest ALL)
foreach(zz_header_spec IN LISTS zz_installed_header_specs)
    string(REPLACE "|" ";" zz_header_parts "${zz_header_spec}")
    list(GET zz_header_parts 0 zz_header_owner)
    list(GET zz_header_parts 1 zz_header_include)
    string(SHA256 zz_header_digest
        "${zz_header_owner}|${zz_header_include}")
    string(SUBSTRING "${zz_header_digest}" 0 12 zz_header_id)
    set(zz_header_target "ZzInstalledHeader_${zz_header_id}")
    set(zz_header_source
        "${CMAKE_CURRENT_BINARY_DIR}/public-headers/${zz_header_id}.cpp")

    file(GENERATE
        OUTPUT "${zz_header_source}"
        CONTENT "#include <${zz_header_include}>\n")
    set_source_files_properties("${zz_header_source}" PROPERTIES
        GENERATED TRUE)
    add_library(${zz_header_target} OBJECT "${zz_header_source}")
    target_compile_features(${zz_header_target} PRIVATE cxx_std_20)
    set_target_properties(${zz_header_target} PROPERTIES
        CXX_EXTENSIONS OFF)
    target_link_libraries(${zz_header_target} PRIVATE ${zz_header_owner})
    add_dependencies(ZzInstalledPublicHeadersTest ${zz_header_target})
endforeach()

add_executable(ZzInstallConsumer main.cpp)
target_compile_features(ZzInstallConsumer PRIVATE cxx_std_20)
set_target_properties(ZzInstallConsumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(ZzInstallConsumer PRIVATE
    Zz::AppCore
    Zz::PureTools
)

add_test(NAME zz.install-consumer.run COMMAND ZzInstallConsumer)

add_executable(ZzFluentInstallConsumer Gui/main.cpp)
target_compile_features(ZzFluentInstallConsumer PRIVATE cxx_std_20)
set_target_properties(ZzFluentInstallConsumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(ZzFluentInstallConsumer PRIVATE
    Zz::FluentUI
    Zz::PureTools
)
add_test(
    NAME zz.fluent-install-consumer.run
    COMMAND ZzFluentInstallConsumer
)
set_tests_properties(zz.fluent-install-consumer.run PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

if(NOT DEFINED ZZ_PACKAGE_ROOT)
    message(FATAL_ERROR
        "ZZ_PACKAGE_ROOT is required to locate installed runtime libraries")
endif()
if(WIN32)
    set_tests_properties(
        zz.install-consumer.run
        zz.fluent-install-consumer.run
        PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${ZZ_PACKAGE_ROOT}/${CMAKE_INSTALL_BINDIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
elseif(APPLE)
    set_tests_properties(
        zz.install-consumer.run
        zz.fluent-install-consumer.run
        PROPERTIES
        ENVIRONMENT_MODIFICATION
            "DYLD_LIBRARY_PATH=path_list_prepend:${ZZ_PACKAGE_ROOT}/${CMAKE_INSTALL_LIBDIR};DYLD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
elseif(UNIX)
    set_tests_properties(
        zz.install-consumer.run
        zz.fluent-install-consumer.run
        PROPERTIES
        ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:${ZZ_PACKAGE_ROOT}/${CMAKE_INSTALL_LIBDIR};LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()
