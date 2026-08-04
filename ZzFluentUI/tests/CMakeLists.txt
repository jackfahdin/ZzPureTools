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

add_executable(ZzNavigationControlsTest ZzNavigationControlsTest.cpp)
target_link_libraries(ZzNavigationControlsTest PRIVATE
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    Zz::FluentUI
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
