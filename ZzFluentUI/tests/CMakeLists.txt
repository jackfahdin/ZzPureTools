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
