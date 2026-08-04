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

if(ZZ_BUILD_BENCHMARKS)
    add_executable(ZzThemeSwitchBenchmark ZzThemeSwitchBenchmark.cpp)
    target_link_libraries(ZzThemeSwitchBenchmark PRIVATE
        Qt6::Test
        Zz::FluentUI
    )
    set_target_properties(ZzThemeSwitchBenchmark PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(ZzThemeSwitchBenchmark)
    add_test(
        NAME benchmark.fluent-theme-switch
        COMMAND ZzThemeSwitchBenchmark
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
