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
