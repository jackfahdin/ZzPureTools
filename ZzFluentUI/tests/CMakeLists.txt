include_directories("${PROJECT_SOURCE_DIR}/tests/support")

add_executable(ZzThemeSnapshotTest
    ZzThemeSnapshotTest.cpp
)
target_link_libraries(ZzThemeSnapshotTest PRIVATE
    Qt6::Test
    Zz::FluentFoundation
)
set_target_properties(ZzThemeSnapshotTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzThemeSnapshotTest)
zz_enable_sanitizers(ZzThemeSnapshotTest)
add_test(NAME fluent.theme-snapshot COMMAND ZzThemeSnapshotTest)
set_tests_properties(fluent.theme-snapshot PROPERTIES
    LABELS "unit;fluent;foundation"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzAnimationDpiTest ZzAnimationDpiTest.cpp)
target_link_libraries(ZzAnimationDpiTest PRIVATE
    Qt6::Test
    Zz::FluentFoundation
)
set_target_properties(ZzAnimationDpiTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzAnimationDpiTest)
zz_enable_sanitizers(ZzAnimationDpiTest)
add_test(NAME fluent.animation-dpi COMMAND ZzAnimationDpiTest)
set_tests_properties(fluent.animation-dpi PROPERTIES
    LABELS "unit;fluent;foundation")

add_executable(ZzIconFontTest ZzIconFontTest.cpp)
target_link_libraries(ZzIconFontTest PRIVATE
    Qt6::Test
    Zz::FluentFoundation
)
set_target_properties(ZzIconFontTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzIconFontTest)
zz_enable_sanitizers(ZzIconFontTest)
add_test(NAME fluent.icon-font COMMAND ZzIconFontTest)
set_tests_properties(fluent.icon-font PROPERTIES
    LABELS "unit;fluent;foundation"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(ZzThemeControllerTest ZzThemeControllerTest.cpp)
target_link_libraries(ZzThemeControllerTest PRIVATE
    Qt6::Test
    Zz::FluentFoundation
)
set_target_properties(ZzThemeControllerTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzThemeControllerTest)
zz_enable_sanitizers(ZzThemeControllerTest)
add_test(NAME fluent.theme-controller COMMAND ZzThemeControllerTest)
set_tests_properties(fluent.theme-controller PROPERTIES
    LABELS "component;fluent;foundation"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(ZzFluentStyleTest ZzFluentStyleTest.cpp)
qt_add_resources(ZzFluentStyleTest "zzfluent-test-icons"
    PREFIX "/zzfluent/tests"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/ZzFluentTestSquare.svg
)
target_link_libraries(ZzFluentStyleTest PRIVATE
    Qt6::Test
    Zz::FluentUI
)
set_target_properties(ZzFluentStyleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentStyleTest)
zz_enable_sanitizers(ZzFluentStyleTest)
add_test(NAME fluent.style COMMAND ZzFluentStyleTest)
set_tests_properties(fluent.style PROPERTIES
    LABELS "unit;fluent;widgets"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFluentStandardControlsTest
    ZzFluentStandardControlsTest.cpp
)
target_link_libraries(ZzFluentStandardControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentStandardControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentStandardControlsTest)
zz_enable_sanitizers(ZzFluentStandardControlsTest)
add_test(NAME fluent.standard-controls COMMAND ZzFluentStandardControlsTest)
set_tests_properties(fluent.standard-controls PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzCommandStatusSurfacesTest
    ZzCommandStatusSurfacesTest.cpp
)
target_link_libraries(ZzCommandStatusSurfacesTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzCommandStatusSurfacesTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzCommandStatusSurfacesTest)
zz_enable_sanitizers(ZzCommandStatusSurfacesTest)
add_test(
    NAME fluent.command-status-surfaces
    COMMAND ZzCommandStatusSurfacesTest
)
set_tests_properties(fluent.command-status-surfaces PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzCommandBarTest ZzCommandBarTest.cpp)
target_link_libraries(ZzCommandBarTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzCommandBarTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzCommandBarTest)
zz_enable_sanitizers(ZzCommandBarTest)
add_test(NAME fluent.command-bar COMMAND ZzCommandBarTest)
set_tests_properties(fluent.command-bar PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
if(UNIX AND NOT APPLE)
    set_tests_properties(fluent.command-bar PROPERTIES
        ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:ZzFluentUI>;LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:ZzCore>;LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

add_executable(ZzPopupSurfacesTest ZzPopupSurfacesTest.cpp)
target_link_libraries(ZzPopupSurfacesTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzPopupSurfacesTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPopupSurfacesTest)
zz_enable_sanitizers(ZzPopupSurfacesTest)
add_test(NAME fluent.popup-surfaces COMMAND ZzPopupSurfacesTest)
set_tests_properties(fluent.popup-surfaces PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzTextInputControlsTest ZzTextInputControlsTest.cpp)
target_link_libraries(ZzTextInputControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzTextInputControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzTextInputControlsTest)
zz_enable_sanitizers(ZzTextInputControlsTest)
add_test(NAME fluent.text-input-controls COMMAND ZzTextInputControlsTest)
set_tests_properties(fluent.text-input-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzComboBoxControlsTest ZzComboBoxControlsTest.cpp)
target_link_libraries(ZzComboBoxControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzComboBoxControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzComboBoxControlsTest)
zz_enable_sanitizers(ZzComboBoxControlsTest)
add_test(NAME fluent.combo-box-controls COMMAND ZzComboBoxControlsTest)
set_tests_properties(fluent.combo-box-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzSuggestBoxTest ZzSuggestBoxTest.cpp)
target_link_libraries(ZzSuggestBoxTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzSuggestBoxTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzSuggestBoxTest)
zz_enable_sanitizers(ZzSuggestBoxTest)
add_test(NAME fluent.suggest-box COMMAND ZzSuggestBoxTest)
set_tests_properties(fluent.suggest-box PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzPasswordBoxTest ZzPasswordBoxTest.cpp)
target_link_libraries(ZzPasswordBoxTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzPasswordBoxTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPasswordBoxTest)
zz_enable_sanitizers(ZzPasswordBoxTest)
add_test(NAME fluent.password-box COMMAND ZzPasswordBoxTest)
set_tests_properties(fluent.password-box PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzSplitButtonTest ZzSplitButtonTest.cpp)
target_link_libraries(ZzSplitButtonTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzSplitButtonTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzSplitButtonTest)
zz_enable_sanitizers(ZzSplitButtonTest)
add_test(NAME fluent.split-button COMMAND ZzSplitButtonTest)
set_tests_properties(fluent.split-button PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzRatingControlTest ZzRatingControlTest.cpp)
target_link_libraries(ZzRatingControlTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzRatingControlTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzRatingControlTest)
zz_enable_sanitizers(ZzRatingControlTest)
add_test(NAME fluent.rating-control COMMAND ZzRatingControlTest)
set_tests_properties(fluent.rating-control PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzKeyBinderTest ZzKeyBinderTest.cpp)
target_link_libraries(ZzKeyBinderTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzKeyBinderTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzKeyBinderTest)
zz_enable_sanitizers(ZzKeyBinderTest)
add_test(NAME fluent.key-binder COMMAND ZzKeyBinderTest)
set_tests_properties(fluent.key-binder PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzColorPickerTest ZzColorPickerTest.cpp)
target_link_libraries(ZzColorPickerTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzColorPickerTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzColorPickerTest)
zz_enable_sanitizers(ZzColorPickerTest)
add_test(NAME fluent.color-picker COMMAND ZzColorPickerTest)
set_tests_properties(fluent.color-picker PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzMultiSelectComboBoxTest ZzMultiSelectComboBoxTest.cpp)
target_link_libraries(ZzMultiSelectComboBoxTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzMultiSelectComboBoxTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzMultiSelectComboBoxTest)
zz_enable_sanitizers(ZzMultiSelectComboBoxTest)
add_test(
    NAME fluent.multi-select-combo-box
    COMMAND ZzMultiSelectComboBoxTest
)
set_tests_properties(fluent.multi-select-combo-box PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzRollerControlsTest ZzRollerControlsTest.cpp)
target_link_libraries(ZzRollerControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzRollerControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzRollerControlsTest)
zz_enable_sanitizers(ZzRollerControlsTest)
add_test(NAME fluent.roller-controls COMMAND ZzRollerControlsTest)
set_tests_properties(fluent.roller-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFlowLayoutTest ZzFlowLayoutTest.cpp)
target_link_libraries(ZzFlowLayoutTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFlowLayoutTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFlowLayoutTest)
zz_enable_sanitizers(ZzFlowLayoutTest)
add_test(NAME fluent.flow-layout COMMAND ZzFlowLayoutTest)
set_tests_properties(fluent.flow-layout PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzButtonControlsTest ZzButtonControlsTest.cpp)
qt_add_resources(ZzButtonControlsTest "zzfluent-button-icons"
    PREFIX "/zzfluent/buttons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/ZzFluentTestSquare.svg
)
target_link_libraries(ZzButtonControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzButtonControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzButtonControlsTest)
zz_enable_sanitizers(ZzButtonControlsTest)
add_test(NAME fluent.buttons COMMAND ZzButtonControlsTest)
set_tests_properties(fluent.buttons PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzToggleSwitchTest ZzToggleSwitchTest.cpp)
target_link_libraries(ZzToggleSwitchTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzToggleSwitchTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzToggleSwitchTest)
zz_enable_sanitizers(ZzToggleSwitchTest)
add_test(NAME fluent.toggle-switch COMMAND ZzToggleSwitchTest)
set_tests_properties(fluent.toggle-switch PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzProgressRingTest ZzProgressRingTest.cpp)
target_link_libraries(ZzProgressRingTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzProgressRingTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzProgressRingTest)
zz_enable_sanitizers(ZzProgressRingTest)
add_test(NAME fluent.progress-ring COMMAND ZzProgressRingTest)
set_tests_properties(fluent.progress-ring PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzScrollControlsTest ZzScrollControlsTest.cpp)
target_link_libraries(ZzScrollControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzScrollControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzScrollControlsTest)
zz_enable_sanitizers(ZzScrollControlsTest)
add_test(NAME fluent.scroll-controls COMMAND ZzScrollControlsTest)
set_tests_properties(fluent.scroll-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
if(UNIX AND NOT APPLE)
    set_tests_properties(fluent.scroll-controls PROPERTIES
        ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:ZzFluentUI>;LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:ZzCore>;LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

add_executable(ZzAnnotatedScrollBarTest ZzAnnotatedScrollBarTest.cpp)
target_link_libraries(ZzAnnotatedScrollBarTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzAnnotatedScrollBarTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzAnnotatedScrollBarTest)
zz_enable_sanitizers(ZzAnnotatedScrollBarTest)
add_test(NAME fluent.annotated-scroll-bar COMMAND ZzAnnotatedScrollBarTest)
set_tests_properties(fluent.annotated-scroll-bar PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
if(UNIX AND NOT APPLE)
    set_tests_properties(fluent.annotated-scroll-bar PROPERTIES
        ENVIRONMENT_MODIFICATION
            "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:ZzFluentUI>;LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:ZzCore>;LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

add_executable(ZzSpinBoxControlsTest ZzSpinBoxControlsTest.cpp)
target_link_libraries(ZzSpinBoxControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzSpinBoxControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzSpinBoxControlsTest)
zz_enable_sanitizers(ZzSpinBoxControlsTest)
add_test(NAME fluent.spin-box-controls COMMAND ZzSpinBoxControlsTest)
set_tests_properties(fluent.spin-box-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzMessageBarTest ZzMessageBarTest.cpp)
target_link_libraries(ZzMessageBarTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzMessageBarTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzMessageBarTest)
zz_enable_sanitizers(ZzMessageBarTest)
add_test(NAME fluent.message-bar COMMAND ZzMessageBarTest)
set_tests_properties(fluent.message-bar PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzInfoBadgeTest ZzInfoBadgeTest.cpp)
target_link_libraries(ZzInfoBadgeTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzInfoBadgeTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzInfoBadgeTest)
zz_enable_sanitizers(ZzInfoBadgeTest)
add_test(NAME fluent.info-badge COMMAND ZzInfoBadgeTest)
set_tests_properties(fluent.info-badge PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzContentDialogTest ZzContentDialogTest.cpp)
target_link_libraries(ZzContentDialogTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzContentDialogTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzContentDialogTest)
zz_enable_sanitizers(ZzContentDialogTest)
add_test(NAME fluent.content-dialog COMMAND ZzContentDialogTest)
set_tests_properties(fluent.content-dialog PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzTeachingTipTest ZzTeachingTipTest.cpp)
target_link_libraries(ZzTeachingTipTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzTeachingTipTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzTeachingTipTest)
zz_enable_sanitizers(ZzTeachingTipTest)
add_test(NAME fluent.teaching-tip COMMAND ZzTeachingTipTest)
set_tests_properties(fluent.teaching-tip PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzExpanderTest ZzExpanderTest.cpp)
target_link_libraries(ZzExpanderTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzExpanderTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzExpanderTest)
zz_enable_sanitizers(ZzExpanderTest)
add_test(NAME fluent.expander COMMAND ZzExpanderTest)
set_tests_properties(fluent.expander PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzPivotTest ZzPivotTest.cpp)
target_link_libraries(ZzPivotTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzPivotTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPivotTest)
zz_enable_sanitizers(ZzPivotTest)
add_test(NAME fluent.pivot COMMAND ZzPivotTest)
set_tests_properties(fluent.pivot PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzDrawerTest ZzDrawerTest.cpp)
target_link_libraries(ZzDrawerTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzDrawerTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzDrawerTest)
zz_enable_sanitizers(ZzDrawerTest)
add_test(NAME fluent.drawer COMMAND ZzDrawerTest)
set_tests_properties(fluent.drawer PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzCalendarControlsTest ZzCalendarControlsTest.cpp)
target_link_libraries(ZzCalendarControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzCalendarControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzCalendarControlsTest)
zz_enable_sanitizers(ZzCalendarControlsTest)
add_test(NAME fluent.calendar-controls COMMAND ZzCalendarControlsTest)
set_tests_properties(fluent.calendar-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzCardControlsTest ZzCardControlsTest.cpp)
target_link_libraries(ZzCardControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzCardControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzCardControlsTest)
zz_enable_sanitizers(ZzCardControlsTest)
add_test(NAME fluent.card-controls COMMAND ZzCardControlsTest)
set_tests_properties(fluent.card-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzCarouselViewTest ZzCarouselViewTest.cpp)
target_link_libraries(ZzCarouselViewTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzCarouselViewTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzCarouselViewTest)
zz_enable_sanitizers(ZzCarouselViewTest)
add_test(NAME fluent.carousel-view COMMAND ZzCarouselViewTest)
set_tests_properties(fluent.carousel-view PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzTabControlsTest ZzTabControlsTest.cpp)
target_link_libraries(ZzTabControlsTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzTabControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzTabControlsTest)
zz_enable_sanitizers(ZzTabControlsTest)
add_test(NAME fluent.tab-controls COMMAND ZzTabControlsTest)
set_tests_properties(fluent.tab-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzNavigationControlsTest ZzNavigationControlsTest.cpp)
qt_add_resources(ZzNavigationControlsTest "zzfluent-navigation-icons"
    PREFIX "/zzfluent/navigation"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/ZzFluentTestSquare.svg
)
target_link_libraries(ZzNavigationControlsTest PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)

add_executable(ZzNavigationPaneTest ZzNavigationPaneTest.cpp)
target_link_libraries(ZzNavigationPaneTest PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzNavigationPaneTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzNavigationPaneTest)
zz_enable_sanitizers(ZzNavigationPaneTest)
add_test(NAME fluent.navigation-pane COMMAND ZzNavigationPaneTest)
set_tests_properties(fluent.navigation-pane PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzActivityBarTest ZzActivityBarTest.cpp)
target_link_libraries(ZzActivityBarTest PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzActivityBarTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzActivityBarTest)
zz_enable_sanitizers(ZzActivityBarTest)
add_test(NAME fluent.activity-bar COMMAND ZzActivityBarTest)
set_tests_properties(fluent.activity-bar PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzSidePaneTest ZzSidePaneTest.cpp)
target_link_libraries(ZzSidePaneTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzSidePaneTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzSidePaneTest)
zz_enable_sanitizers(ZzSidePaneTest)
add_test(NAME fluent.side-pane COMMAND ZzSidePaneTest)
set_tests_properties(fluent.side-pane PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzBottomPaneTest ZzBottomPaneTest.cpp)
target_link_libraries(ZzBottomPaneTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzBottomPaneTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzBottomPaneTest)
zz_enable_sanitizers(ZzBottomPaneTest)
add_test(NAME fluent.bottom-pane COMMAND ZzBottomPaneTest)
set_tests_properties(fluent.bottom-pane PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzPanelStackTest ZzPanelStackTest.cpp)
target_link_libraries(ZzPanelStackTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzPanelStackTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPanelStackTest)
zz_enable_sanitizers(ZzPanelStackTest)
add_test(NAME fluent.panel-stack COMMAND ZzPanelStackTest)
set_tests_properties(fluent.panel-stack PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzExplorerPaneTest ZzExplorerPaneTest.cpp)
target_link_libraries(ZzExplorerPaneTest PRIVATE Qt6::Test Qt6::Widgets Zz::FluentUI)
set_target_properties(ZzExplorerPaneTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzExplorerPaneTest)
zz_enable_sanitizers(ZzExplorerPaneTest)
add_test(NAME fluent.explorer COMMAND ZzExplorerPaneTest)
set_tests_properties(fluent.explorer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(ZzCommandPaletteTest ZzCommandPaletteTest.cpp)
target_link_libraries(ZzCommandPaletteTest PRIVATE Qt6::Test Qt6::Widgets Zz::FluentUI)
set_target_properties(ZzCommandPaletteTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzCommandPaletteTest)
zz_enable_sanitizers(ZzCommandPaletteTest)
add_test(NAME fluent.command-palette COMMAND ZzCommandPaletteTest)
set_tests_properties(fluent.command-palette PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(ZzDockPanelTest ZzDockPanelTest.cpp)
target_link_libraries(ZzDockPanelTest PRIVATE Qt6::Test Qt6::Widgets Zz::FluentUI)
set_target_properties(ZzDockPanelTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzDockPanelTest)
zz_enable_sanitizers(ZzDockPanelTest)
add_test(NAME fluent.dock-panel COMMAND ZzDockPanelTest)
set_tests_properties(fluent.dock-panel PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

set_target_properties(ZzNavigationControlsTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzNavigationControlsTest)
zz_enable_sanitizers(ZzNavigationControlsTest)
add_test(NAME fluent.navigation-controls COMMAND ZzNavigationControlsTest)
set_tests_properties(fluent.navigation-controls PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFluentItemDelegateTest ZzFluentItemDelegateTest.cpp)
target_link_libraries(ZzFluentItemDelegateTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentItemDelegateTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentItemDelegateTest)
zz_enable_sanitizers(ZzFluentItemDelegateTest)
add_test(NAME fluent.item-delegate COMMAND ZzFluentItemDelegateTest)
set_tests_properties(fluent.item-delegate PROPERTIES
    LABELS "fluent;unit;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFluentTitleBarTest ZzFluentTitleBarTest.cpp)
target_link_libraries(ZzFluentTitleBarTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentTitleBarTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentTitleBarTest)
zz_enable_sanitizers(ZzFluentTitleBarTest)
add_test(NAME fluent.title-bar COMMAND ZzFluentTitleBarTest)
set_tests_properties(fluent.title-bar PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFluentAccessibilityTest ZzFluentAccessibilityTest.cpp)
target_link_libraries(ZzFluentAccessibilityTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentAccessibilityTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentAccessibilityTest)
zz_enable_sanitizers(ZzFluentAccessibilityTest)
add_test(NAME fluent.accessibility COMMAND ZzFluentAccessibilityTest)
set_tests_properties(fluent.accessibility PROPERTIES
    LABELS "fluent;accessibility;component"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzFluentScreenshotTest ZzFluentScreenshotTest.cpp)
qt_add_resources(ZzFluentScreenshotTest "zzfluent-screenshot-icons"
    PREFIX "/zzfluent/screenshots"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/ZzFluentTestSquare.svg
)
target_link_libraries(ZzFluentScreenshotTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
target_compile_definitions(ZzFluentScreenshotTest PRIVATE
    "ZZ_FLUENT_SCREENSHOT_BASELINE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/baselines/linux\""
    "ZZ_FLUENT_SCREENSHOT_REPORT_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/reports/fluent-screenshots\""
    ZZ_FLUENT_SCREENSHOT_REFERENCE_QT_MAJOR=6
    ZZ_FLUENT_SCREENSHOT_REFERENCE_QT_MINOR=11
)
set_target_properties(ZzFluentScreenshotTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentScreenshotTest)
zz_enable_sanitizers(ZzFluentScreenshotTest)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    function(zz_add_fluent_screenshot_test suffix scale)
        add_test(NAME "fluent.screenshot-${suffix}"
            COMMAND ZzFluentScreenshotTest
                --expected-dpr "${scale}"
                --baseline-subdir "dpr-${suffix}")
        set_tests_properties("fluent.screenshot-${suffix}" PROPERTIES
            LABELS "fluent;screenshot;component"
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=${scale};QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough;LANG=C.UTF-8;LC_ALL=C.UTF-8"
        )
    endfunction()
    zz_add_fluent_screenshot_test(100 1.0)
    zz_add_fluent_screenshot_test(125 1.25)
    zz_add_fluent_screenshot_test(150 1.5)
    zz_add_fluent_screenshot_test(200 2.0)
endif()

add_executable(ZzSplitWorkspaceTest ZzSplitWorkspaceTest.cpp)
target_link_libraries(ZzSplitWorkspaceTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzSplitWorkspaceTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzSplitWorkspaceTest)
zz_enable_sanitizers(ZzSplitWorkspaceTest)
add_test(NAME fluent.split-workspace COMMAND ZzSplitWorkspaceTest)
set_tests_properties(fluent.split-workspace PROPERTIES
    LABELS "fluent;unit;component;accessibility"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

if(ZZ_BUILD_BENCHMARKS)
    add_executable(ZzBasicControlsBenchmark ZzBasicControlsBenchmark.cpp)
    qt_add_resources(ZzBasicControlsBenchmark "zzfluent-benchmark-icons"
        PREFIX "/zzfluent/benchmarks"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        FILES
            resources/ZzFluentTestSquare.svg
    )
    target_link_libraries(ZzBasicControlsBenchmark PRIVATE
        Qt6::Test
        Qt6::Widgets
        Zz::FluentUI
    )
    set_target_properties(ZzBasicControlsBenchmark PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(ZzBasicControlsBenchmark)
    zz_enable_sanitizers(ZzBasicControlsBenchmark)
    add_test(
        NAME benchmark.fluent-basic-controls
        COMMAND ZzBasicControlsBenchmark
    )
    set_tests_properties(benchmark.fluent-basic-controls PROPERTIES
        LABELS "benchmark;fluent"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
    )
    if(DEFINED ZZ_PERFORMANCE_REFERENCE)
        if(ZZ_PERFORMANCE_REFERENCE)
            set(zz_basic_controls_reference_environment 1)
        else()
            set(zz_basic_controls_reference_environment 0)
        endif()
        set_property(TEST benchmark.fluent-basic-controls APPEND PROPERTY
            ENVIRONMENT
                "ZZ_PERFORMANCE_REFERENCE=${zz_basic_controls_reference_environment}"
        )
    endif()

    add_executable(ZzFluentThemeSwitchBenchmark
        ZzThemeSwitchBenchmark.cpp)
    target_link_libraries(ZzFluentThemeSwitchBenchmark PRIVATE
        Qt6::Test
        Zz::FluentUI
    )
    set_target_properties(ZzFluentThemeSwitchBenchmark PROPERTIES
        AUTOMOC ON)
    zz_enable_project_warnings(ZzFluentThemeSwitchBenchmark)
    zz_enable_sanitizers(ZzFluentThemeSwitchBenchmark)
    add_test(
        NAME benchmark.fluent-theme-switch
        COMMAND ZzFluentThemeSwitchBenchmark
    )
    set_tests_properties(benchmark.fluent-theme-switch PROPERTIES
        LABELS "benchmark;fluent"
        TIMEOUT 30
    )
    if(DEFINED ZZ_PERFORMANCE_REFERENCE)
        if(ZZ_PERFORMANCE_REFERENCE)
            set(zz_performance_reference_environment 1)
        else()
            set(zz_performance_reference_environment 0)
        endif()
        set_property(TEST benchmark.fluent-theme-switch APPEND PROPERTY
            ENVIRONMENT
                "ZZ_PERFORMANCE_REFERENCE=${zz_performance_reference_environment}")
    endif()
endif()
