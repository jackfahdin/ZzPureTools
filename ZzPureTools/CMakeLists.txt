set(zz_app_core_sources
    appcore/src/private/ZzAppCoreVersion.cpp
)
set(zz_app_core_moc_headers)

add_library(ZzAppCore ${zz_app_core_sources})
add_library(Zz::AppCore ALIAS ZzAppCore)

target_link_libraries(ZzAppCore PUBLIC
    Zz::Core
    Qt6::Core
)

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
)
set(zz_pure_tools_moc_headers)

add_library(ZzPureTools ${zz_pure_tools_sources})
add_library(Zz::PureTools ALIAS ZzPureTools)

target_link_libraries(ZzPureTools PUBLIC
    Zz::AppCore
    Zz::WindowKit
    Zz::FluentUI
    Qt6::Widgets
)

zz_configure_library_target(ZzPureTools
    EXPORT_NAME PureTools
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/widgets/include"
    EXPORT_HEADER_SUBDIR ZzPureTools
    EXPORT_HEADER_NAME ZzPureToolsExport.h
    EXPORT_MACRO_NAME ZZ_PURE_TOOLS_EXPORT
    SOURCES ${zz_pure_tools_sources}
    MOC_HEADERS ${zz_pure_tools_moc_headers}
)
