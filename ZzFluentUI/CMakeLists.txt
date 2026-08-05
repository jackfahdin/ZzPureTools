set(zz_fluent_foundation_sources
    foundation/src/private/ZzFluentVersion.cpp
    foundation/src/ZzAnimationPolicy.cpp
    foundation/src/ZzDpiScale.cpp
    foundation/src/ZzIconCacheKey.cpp
    foundation/src/ZzThemeController.cpp
    foundation/src/private/ZzThemeControllerPrivate.cpp
    foundation/src/ZzThemePalette.cpp
    foundation/src/ZzThemeSnapshot.cpp
)
set(zz_fluent_foundation_moc_headers
    foundation/include/ZzFluentUI/ZzThemeController.h
)

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
    widgets/src/ZzFluentPainter.cpp
    widgets/src/ZzFluentStyle.cpp
    widgets/src/private/ZzFluentStylePrivate.cpp
    widgets/src/ZzPushButton.cpp
    widgets/src/private/ZzPushButtonPrivate.cpp
    widgets/src/ZzIconButton.cpp
    widgets/src/private/ZzIconButtonPrivate.cpp
    widgets/src/ZzToggleSwitch.cpp
    widgets/src/private/ZzToggleSwitchPrivate.cpp
    widgets/src/ZzProgressRing.cpp
    widgets/src/private/ZzProgressRingPrivate.cpp
    widgets/src/ZzMessageBar.cpp
    widgets/src/private/ZzMessageBarPrivate.cpp
    widgets/src/ZzCalendar.cpp
    widgets/src/private/ZzCalendarPrivate.cpp
    widgets/src/ZzCalendarPicker.cpp
    widgets/src/private/ZzCalendarPickerPrivate.cpp
    widgets/src/ZzActionCard.cpp
    widgets/src/private/ZzActionCardPrivate.cpp
    widgets/src/ZzImageCard.cpp
    widgets/src/private/ZzImageCardPrivate.cpp
    widgets/src/ZzTabBar.cpp
    widgets/src/private/ZzTabBarPrivate.cpp
    widgets/src/ZzTabWidget.cpp
    widgets/src/private/ZzTabWidgetPrivate.cpp
    widgets/src/ZzNavigationView.cpp
    widgets/src/private/ZzNavigationViewPrivate.cpp
    widgets/src/ZzBreadcrumbBar.cpp
    widgets/src/private/ZzBreadcrumbBarPrivate.cpp
    widgets/src/ZzFluentItemDelegate.cpp
    widgets/src/private/ZzFluentItemDelegatePrivate.cpp
    widgets/src/ZzFluentTitleBar.cpp
    widgets/src/private/ZzFluentTitleBarPrivate.cpp
    widgets/src/private/ZzStyleCache.cpp
)
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
    widgets/include/ZzFluentUI/ZzPushButton.h
    widgets/include/ZzFluentUI/ZzIconButton.h
    widgets/include/ZzFluentUI/ZzToggleSwitch.h
    widgets/include/ZzFluentUI/ZzProgressRing.h
    widgets/include/ZzFluentUI/ZzMessageBar.h
    widgets/include/ZzFluentUI/ZzCalendar.h
    widgets/include/ZzFluentUI/ZzCalendarPicker.h
    widgets/include/ZzFluentUI/ZzActionCard.h
    widgets/include/ZzFluentUI/ZzImageCard.h
    widgets/include/ZzFluentUI/ZzTabBar.h
    widgets/include/ZzFluentUI/ZzTabWidget.h
    widgets/include/ZzFluentUI/ZzNavigationView.h
    widgets/include/ZzFluentUI/ZzBreadcrumbBar.h
    widgets/include/ZzFluentUI/ZzFluentItemDelegate.h
    widgets/include/ZzFluentUI/ZzFluentTitleBar.h
)

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

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
