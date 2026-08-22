qt_add_resources(zz_fluent_icon_resources
    resources/zzfluent_icon_assets.qrc
)

set(zz_fluent_foundation_sources
    foundation/src/private/ZzFluentVersion.cpp
    foundation/src/ZzAnimationPolicy.cpp
    foundation/src/ZzDpiScale.cpp
    foundation/src/ZzIconAssets.cpp
    foundation/src/ZzIconDescriptor.cpp
    foundation/src/ZzIconFont.cpp
    foundation/src/ZzIconCacheKey.cpp
    foundation/src/ZzThemeController.cpp
    foundation/src/private/ZzThemeControllerPrivate.cpp
    foundation/src/ZzThemePalette.cpp
    foundation/src/ZzThemeSnapshot.cpp
    foundation/src/ZzTabGroupId.cpp
    ${zz_fluent_icon_resources}
)
set(zz_fluent_foundation_moc_headers
    foundation/include/ZzFluentUI/ZzThemeController.h
    foundation/include/ZzFluentUI/ZzCommandItemRole.h
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
    widgets/src/ZzSplitButton.cpp
    widgets/src/private/ZzSplitButtonPrivate.cpp
    widgets/src/ZzRatingControl.cpp
    widgets/src/private/ZzRatingControlPrivate.cpp
    widgets/src/ZzKeyBinder.cpp
    widgets/src/private/ZzKeyBinderPrivate.cpp
    widgets/src/ZzColorPicker.cpp
    widgets/src/private/ZzColorPickerPrivate.cpp
    widgets/src/ZzIconButton.cpp
    widgets/src/private/ZzIconButtonPrivate.cpp
    widgets/src/ZzToggleSwitch.cpp
    widgets/src/private/ZzToggleSwitchPrivate.cpp
    widgets/src/ZzProgressRing.cpp
    widgets/src/private/ZzProgressRingPrivate.cpp
    widgets/src/ZzScrollBar.cpp
    widgets/src/private/ZzScrollBarPrivate.cpp
    widgets/src/ZzScrollArea.cpp
    widgets/src/ZzFlowLayout.cpp
    widgets/src/private/ZzFlowLayoutPrivate.cpp
    widgets/src/ZzSpinBox.cpp
    widgets/src/ZzDoubleSpinBox.cpp
    widgets/src/ZzPasswordBox.cpp
    widgets/src/private/ZzPasswordBoxPrivate.cpp
    widgets/src/ZzSuggestBox.cpp
    widgets/src/private/ZzSuggestBoxPrivate.cpp
    widgets/src/ZzMultiSelectComboBox.cpp
    widgets/src/private/ZzMultiSelectComboBoxPrivate.cpp
    widgets/src/ZzRoller.cpp
    widgets/src/private/ZzRollerPrivate.cpp
    widgets/src/ZzRollerPicker.cpp
    widgets/src/private/ZzRollerPickerPrivate.cpp
    widgets/src/ZzMessageBar.cpp
    widgets/src/private/ZzMessageBarPrivate.cpp
    widgets/src/ZzInfoBadge.cpp
    widgets/src/private/ZzInfoBadgePrivate.cpp
    widgets/src/private/ZzWidgetTheme.cpp
    widgets/src/ZzContentDialog.cpp
    widgets/src/private/ZzContentDialogPrivate.cpp
    widgets/src/ZzTeachingTip.cpp
    widgets/src/private/ZzTeachingTipPrivate.cpp
    widgets/src/ZzExpander.cpp
    widgets/src/private/ZzExpanderPrivate.cpp
    widgets/src/ZzPivot.cpp
    widgets/src/private/ZzPivotPrivate.cpp
    widgets/src/ZzDrawer.cpp
    widgets/src/private/ZzDrawerPrivate.cpp
    widgets/src/ZzCalendar.cpp
    widgets/src/private/ZzCalendarPrivate.cpp
    widgets/src/ZzCalendarPicker.cpp
    widgets/src/private/ZzCalendarPickerPrivate.cpp
    widgets/src/ZzActionCard.cpp
    widgets/src/private/ZzActionCardPrivate.cpp
    widgets/src/ZzImageCard.cpp
    widgets/src/private/ZzImageCardPrivate.cpp
    widgets/src/ZzCarouselView.cpp
    widgets/src/private/ZzCarouselViewPrivate.cpp
    widgets/src/ZzTabBar.cpp
    widgets/src/private/ZzTabBarPrivate.cpp
    widgets/src/ZzTabWidget.cpp
    widgets/src/private/ZzTabWidgetPrivate.cpp
    widgets/src/ZzNavigationView.cpp
    widgets/src/private/ZzNavigationViewPrivate.cpp
    widgets/src/ZzNavigationPane.cpp
    widgets/src/private/ZzNavigationPanePrivate.cpp
    widgets/src/private/ZzNavigationProjectionModel.cpp
    widgets/src/ZzActivityBar.cpp
    widgets/src/private/ZzActivityBarPrivate.cpp
    widgets/src/ZzSidePane.cpp
    widgets/src/private/ZzSidePanePrivate.cpp
    widgets/src/ZzExplorerPane.cpp
    widgets/src/private/ZzExplorerPanePrivate.cpp
    widgets/src/ZzCommandPalette.cpp
    widgets/src/private/ZzCommandPalettePrivate.cpp
    widgets/src/ZzDockPanel.cpp
    widgets/src/private/ZzDockPanelPrivate.cpp
    widgets/src/ZzBreadcrumbBar.cpp
    widgets/src/private/ZzBreadcrumbBarPrivate.cpp
    widgets/src/ZzFluentItemDelegate.cpp
    widgets/src/private/ZzFluentItemDelegatePrivate.cpp
    widgets/src/private/ZzItemViewVisual.cpp
    widgets/src/private/ZzSelectionIndicatorTransition.cpp
    widgets/src/ZzFluentTitleBar.cpp
    widgets/src/private/ZzFluentTitleBarPrivate.cpp
    widgets/src/private/ZzStyleCache.cpp
)
set(zz_fluent_ui_moc_headers
    widgets/include/ZzFluentUI/ZzFluentStyle.h
    widgets/include/ZzFluentUI/ZzPushButton.h
    widgets/include/ZzFluentUI/ZzSplitButton.h
    widgets/include/ZzFluentUI/ZzRatingControl.h
    widgets/include/ZzFluentUI/ZzKeyBinder.h
    widgets/include/ZzFluentUI/ZzColorPicker.h
    widgets/include/ZzFluentUI/ZzIconButton.h
    widgets/include/ZzFluentUI/ZzToggleSwitch.h
    widgets/include/ZzFluentUI/ZzProgressRing.h
    widgets/include/ZzFluentUI/ZzScrollBar.h
    widgets/include/ZzFluentUI/ZzScrollArea.h
    widgets/include/ZzFluentUI/ZzFlowLayout.h
    widgets/include/ZzFluentUI/ZzSpinBox.h
    widgets/include/ZzFluentUI/ZzDoubleSpinBox.h
    widgets/include/ZzFluentUI/ZzPasswordBox.h
    widgets/include/ZzFluentUI/ZzSuggestBox.h
    widgets/include/ZzFluentUI/ZzMultiSelectComboBox.h
    widgets/include/ZzFluentUI/ZzRoller.h
    widgets/include/ZzFluentUI/ZzRollerPicker.h
    widgets/include/ZzFluentUI/ZzMessageBar.h
    widgets/include/ZzFluentUI/ZzInfoBadge.h
    widgets/include/ZzFluentUI/ZzContentDialog.h
    widgets/include/ZzFluentUI/ZzTeachingTip.h
    widgets/include/ZzFluentUI/ZzExpander.h
    widgets/include/ZzFluentUI/ZzPivot.h
    widgets/include/ZzFluentUI/ZzDrawer.h
    widgets/include/ZzFluentUI/ZzCalendar.h
    widgets/include/ZzFluentUI/ZzCalendarPicker.h
    widgets/include/ZzFluentUI/ZzActionCard.h
    widgets/include/ZzFluentUI/ZzImageCard.h
    widgets/include/ZzFluentUI/ZzCarouselView.h
    widgets/include/ZzFluentUI/ZzTabBar.h
    widgets/include/ZzFluentUI/ZzTabWidget.h
    widgets/include/ZzFluentUI/ZzNavigationView.h
    widgets/include/ZzFluentUI/ZzNavigationPane.h
    widgets/include/ZzFluentUI/ZzActivityBar.h
    widgets/include/ZzFluentUI/ZzSidePane.h
    widgets/include/ZzFluentUI/ZzExplorerPane.h
    widgets/include/ZzFluentUI/ZzCommandPalette.h
    widgets/include/ZzFluentUI/ZzDockPanel.h
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
