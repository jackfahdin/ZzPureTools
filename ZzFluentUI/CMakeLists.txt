set(zz_fluent_foundation_sources
    foundation/src/private/ZzFluentVersion.cpp
)
set(zz_fluent_foundation_moc_headers)

add_library(ZzFluentFoundation ${zz_fluent_foundation_sources})
add_library(Zz::FluentFoundation ALIAS ZzFluentFoundation)

target_link_libraries(ZzFluentFoundation PUBLIC
    Zz::Core
    Qt6::Core
    Qt6::Gui
)

zz_configure_library_target(ZzFluentFoundation
    EXPORT_NAME FluentFoundation
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/foundation/include"
    EXPORT_HEADER_SUBDIR ZzFluentUI
    EXPORT_HEADER_NAME ZzFluentFoundationExport.h
    EXPORT_MACRO_NAME ZZ_FLUENT_FOUNDATION_EXPORT
    SOURCES ${zz_fluent_foundation_sources}
    MOC_HEADERS ${zz_fluent_foundation_moc_headers}
)

set(zz_fluent_ui_sources
    widgets/src/private/ZzFluentWidgetVersion.cpp
)
set(zz_fluent_ui_moc_headers)

add_library(ZzFluentUI ${zz_fluent_ui_sources})
add_library(Zz::FluentUI ALIAS ZzFluentUI)

target_link_libraries(ZzFluentUI PUBLIC
    Zz::FluentFoundation
    Qt6::Widgets
    Qt6::Svg
)

zz_configure_library_target(ZzFluentUI
    EXPORT_NAME FluentUI
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/widgets/include"
    EXPORT_HEADER_SUBDIR ZzFluentUI
    EXPORT_HEADER_NAME ZzFluentUIExport.h
    EXPORT_MACRO_NAME ZZ_FLUENT_UI_EXPORT
    SOURCES ${zz_fluent_ui_sources}
    MOC_HEADERS ${zz_fluent_ui_moc_headers}
)
