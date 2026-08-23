set(zz_app_core_sources
    appcore/src/private/ZzAppCoreVersion.cpp
    appcore/src/ZzModuleId.cpp
    appcore/src/ZzRouteId.cpp
    appcore/src/ZzModuleGraphBuilder.cpp
    appcore/src/private/ZzModuleGraphBuilderPrivate.cpp
    appcore/src/ZzApplicationRuntime.cpp
    appcore/src/private/ZzApplicationRuntimePrivate.cpp
)
set(zz_app_core_moc_headers)

add_library(ZzAppCore ${zz_app_core_sources})
add_library(Zz::AppCore ALIAS ZzAppCore)

target_link_libraries(ZzAppCore PUBLIC
    Zz::Core
    Qt6::Core
)

foreach(zz_app_core_link_property IN ITEMS
    LINK_LIBRARIES
    INTERFACE_LINK_LIBRARIES
)
    get_target_property(zz_app_core_links
        ZzAppCore ${zz_app_core_link_property})
    if(zz_app_core_links MATCHES "-NOTFOUND$")
        set(zz_app_core_links)
    endif()
    if("${zz_app_core_links}" MATCHES
       "Qt6::(Gui|Widgets|Quick)|Zz::WindowKit|Zz::Fluent(UI|Foundation)")
        message(FATAL_ERROR
            "ZzAppCore ${zz_app_core_link_property} contains a UI dependency: ${zz_app_core_links}")
    endif()
endforeach()

zz_configure_library_target(ZzAppCore
    EXPORT_NAME AppCore
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/appcore/include"
    EXPORT_HEADER_SUBDIR ZzPureTools
    EXPORT_HEADER_NAME ZzAppCoreExport.h
    EXPORT_MACRO_NAME ZZ_APP_CORE_EXPORT
    SOURCES ${zz_app_core_sources}
    MOC_HEADERS ${zz_app_core_moc_headers}
)

set(zz_pure_tools_sources
    widgets/src/private/ZzPureToolsVersion.cpp
    widgets/src/ZzPageInstance.cpp
    widgets/src/private/ZzPageInstancePrivate.cpp
    widgets/src/ZzPageHost.cpp
    widgets/src/private/ZzPageHostPrivate.cpp
    widgets/src/ZzNavigationModel.cpp
    widgets/src/private/ZzNavigationModelPrivate.cpp
    widgets/src/ZzNavigationController.cpp
    widgets/src/private/ZzNavigationControllerPrivate.cpp
    widgets/src/ZzPureApplication.cpp
    widgets/src/private/ZzPureApplicationPrivate.cpp
    widgets/src/ZzApplicationBuilder.cpp
    widgets/src/private/ZzApplicationBuilderPrivate.cpp
    widgets/src/ZzApplicationWindow.cpp
    widgets/src/private/ZzApplicationWindowPrivate.cpp
    widgets/src/ZzWorkspacePanelId.cpp
    widgets/src/ZzWorkspaceShell.cpp
    widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp
    widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp
    widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp
    widgets/src/private/ZzWorkspaceShellPrivate.cpp
)
set(zz_pure_tools_moc_headers
    widgets/include/ZzPureTools/ZzPageHost.h
    widgets/include/ZzPureTools/ZzNavigationController.h
    widgets/include/ZzPureTools/ZzPureApplication.h
    widgets/include/ZzPureTools/ZzApplicationWindow.h
)

add_library(ZzPureTools ${zz_pure_tools_sources})
add_library(Zz::PureTools ALIAS ZzPureTools)

target_link_libraries(ZzPureTools
    PUBLIC
        Zz::AppCore
        Zz::FluentFoundation
        Qt6::Widgets
    PRIVATE
        Zz::WindowKit
        Zz::FluentUI
)

get_target_property(zz_pure_tools_direct_links
    ZzPureTools LINK_LIBRARIES)
foreach(zz_required_private_link IN ITEMS Zz::WindowKit Zz::FluentUI)
    list(FIND zz_pure_tools_direct_links
        "${zz_required_private_link}" zz_private_link_index)
    if(zz_private_link_index EQUAL -1)
        message(FATAL_ERROR
            "ZzPureTools must directly link ${zz_required_private_link}")
    endif()
endforeach()

get_target_property(zz_pure_tools_interface_links
    ZzPureTools INTERFACE_LINK_LIBRARIES)
foreach(zz_required_public_link IN ITEMS
    Zz::AppCore
    Zz::FluentFoundation
    Qt6::Widgets
)
    list(FIND zz_pure_tools_interface_links
        "${zz_required_public_link}" zz_public_link_index)
    if(zz_public_link_index EQUAL -1)
        message(FATAL_ERROR
            "ZzPureTools must publicly link ${zz_required_public_link}")
    endif()
endforeach()
foreach(zz_private_interface_link IN ITEMS Zz::WindowKit Zz::FluentUI)
    list(FIND zz_pure_tools_interface_links
        "${zz_private_interface_link}" zz_leaked_link_index)
    if(NOT zz_leaked_link_index EQUAL -1)
        message(FATAL_ERROR
            "ZzPureTools leaks private dependency ${zz_private_interface_link}")
    endif()
endforeach()

zz_configure_library_target(ZzPureTools
    EXPORT_NAME PureTools
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/widgets/include"
    EXPORT_HEADER_SUBDIR ZzPureTools
    EXPORT_HEADER_NAME ZzPureToolsExport.h
    EXPORT_MACRO_NAME ZZ_PURE_TOOLS_EXPORT
    SOURCES ${zz_pure_tools_sources}
    MOC_HEADERS ${zz_pure_tools_moc_headers}
)

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
