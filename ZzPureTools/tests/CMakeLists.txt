add_executable(ZzModuleGraphTest ZzModuleGraphTest.cpp)
target_link_libraries(ZzModuleGraphTest PRIVATE Qt6::Test Zz::AppCore)
set_target_properties(ZzModuleGraphTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzModuleGraphTest)
zz_enable_sanitizers(ZzModuleGraphTest)
add_test(NAME appcore.module-graph COMMAND ZzModuleGraphTest)
set_tests_properties(appcore.module-graph PROPERTIES
    LABELS "unit;appcore")

add_executable(ZzApplicationRuntimeTest ZzApplicationRuntimeTest.cpp)
target_link_libraries(ZzApplicationRuntimeTest PRIVATE Qt6::Test Zz::AppCore)
set_target_properties(ZzApplicationRuntimeTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzApplicationRuntimeTest)
zz_enable_sanitizers(ZzApplicationRuntimeTest)
add_test(NAME appcore.runtime COMMAND ZzApplicationRuntimeTest)
set_tests_properties(appcore.runtime PROPERTIES LABELS "unit;appcore")

add_executable(ZzPageLifecycleTest ZzPageLifecycleTest.cpp)
target_link_libraries(ZzPageLifecycleTest PRIVATE Qt6::Test Zz::PureTools)
set_target_properties(ZzPageLifecycleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPageLifecycleTest)
zz_enable_sanitizers(ZzPageLifecycleTest)
add_test(NAME puretools.page-lifecycle COMMAND ZzPageLifecycleTest)
set_tests_properties(puretools.page-lifecycle PROPERTIES
    LABELS "component;puretools"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzNavigationControllerTest ZzNavigationControllerTest.cpp)
target_link_libraries(ZzNavigationControllerTest PRIVATE Qt6::Test Zz::PureTools)
set_target_properties(ZzNavigationControllerTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzNavigationControllerTest)
zz_enable_sanitizers(ZzNavigationControllerTest)
add_test(NAME puretools.navigation COMMAND ZzNavigationControllerTest)
set_tests_properties(puretools.navigation PROPERTIES
    LABELS "component;puretools"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzWorkspaceShellTest ZzWorkspaceShellTest.cpp)
target_link_libraries(ZzWorkspaceShellTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::PureTools
    Zz::FluentUI
)
set_target_properties(ZzWorkspaceShellTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWorkspaceShellTest)
zz_enable_sanitizers(ZzWorkspaceShellTest)
add_test(NAME puretools.workspace-shell COMMAND ZzWorkspaceShellTest)
set_tests_properties(puretools.workspace-shell PROPERTIES
    LABELS "component;puretools;workspace"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzWorkspaceLayoutStatePrivateTest
    ZzWorkspaceLayoutStatePrivateTest.cpp
    ../widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp
)
target_link_libraries(ZzWorkspaceLayoutStatePrivateTest PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Widgets
    Zz::FluentFoundation
)
set_target_properties(ZzWorkspaceLayoutStatePrivateTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWorkspaceLayoutStatePrivateTest)
zz_enable_sanitizers(ZzWorkspaceLayoutStatePrivateTest)
add_test(NAME puretools.workspace-layout-state-private
    COMMAND ZzWorkspaceLayoutStatePrivateTest)
set_tests_properties(puretools.workspace-layout-state-private PROPERTIES
    LABELS "unit;puretools;workspace"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzWorkspaceLayoutCodecPrivateTest
    ZzWorkspaceLayoutCodecPrivateTest.cpp
    ../widgets/src/private/ZzWorkspaceLayoutCodecPrivate.cpp
    ../widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp
)
target_link_libraries(ZzWorkspaceLayoutCodecPrivateTest PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Widgets
    Zz::Core
    Zz::FluentFoundation
)
set_target_properties(ZzWorkspaceLayoutCodecPrivateTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWorkspaceLayoutCodecPrivateTest)
zz_enable_sanitizers(ZzWorkspaceLayoutCodecPrivateTest)
add_test(NAME puretools.workspace-layout-codec-private
    COMMAND ZzWorkspaceLayoutCodecPrivateTest)
set_tests_properties(puretools.workspace-layout-codec-private PROPERTIES
    LABELS "unit;puretools;workspace"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzWorkspaceScreenshotTest ZzWorkspaceScreenshotTest.cpp)
target_link_libraries(ZzWorkspaceScreenshotTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::PureTools
    Zz::FluentUI
)
target_compile_definitions(ZzWorkspaceScreenshotTest PRIVATE
    "ZZ_PURETOOLS_WORKSPACE_SCREENSHOT_BASELINE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/baselines/linux\""
    "ZZ_PURETOOLS_WORKSPACE_SCREENSHOT_REPORT_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/reports/workspace-screenshots\""
    ZZ_PURETOOLS_SCREENSHOT_REFERENCE_QT_MAJOR=6
    ZZ_PURETOOLS_SCREENSHOT_REFERENCE_QT_MINOR=11
)
set_target_properties(ZzWorkspaceScreenshotTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWorkspaceScreenshotTest)
zz_enable_sanitizers(ZzWorkspaceScreenshotTest)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    function(zz_add_workspace_screenshot_test suffix scale)
        add_test(NAME "puretools.workspace-screenshot-${suffix}"
            COMMAND ZzWorkspaceScreenshotTest
                --expected-dpr "${scale}"
                --baseline-subdir "dpr-${suffix}")
        set_tests_properties("puretools.workspace-screenshot-${suffix}" PROPERTIES
            LABELS "puretools;screenshot;workspace"
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=${scale};QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough;LANG=C.UTF-8;LC_ALL=C.UTF-8"
        )
    endfunction()
    zz_add_workspace_screenshot_test(100 1.0)
    zz_add_workspace_screenshot_test(125 1.25)
    zz_add_workspace_screenshot_test(150 1.5)
    zz_add_workspace_screenshot_test(200 2.0)
    set(zz_workspace_screenshot_library_path_variable LD_LIBRARY_PATH)
    if(APPLE)
        set(zz_workspace_screenshot_library_path_variable DYLD_LIBRARY_PATH)
    elseif(WIN32)
        set(zz_workspace_screenshot_library_path_variable PATH)
    endif()
    foreach(zz_workspace_screenshot_suffix IN ITEMS 100 125 150 200)
        set_property(TEST "puretools.workspace-screenshot-${zz_workspace_screenshot_suffix}"
            APPEND PROPERTY ENVIRONMENT_MODIFICATION
                "${zz_workspace_screenshot_library_path_variable}=path_list_prepend:$<TARGET_FILE_DIR:ZzCore>"
                "${zz_workspace_screenshot_library_path_variable}=path_list_prepend:$<TARGET_FILE_DIR:ZzFluentFoundation>"
                "${zz_workspace_screenshot_library_path_variable}=path_list_prepend:$<TARGET_FILE_DIR:ZzFluentUI>"
                "${zz_workspace_screenshot_library_path_variable}=path_list_prepend:$<TARGET_FILE_DIR:ZzPureTools>"
                "${zz_workspace_screenshot_library_path_variable}=path_list_prepend:$<TARGET_FILE_DIR:ZzWindowKit>"
                "${zz_workspace_screenshot_library_path_variable}=path_list_prepend:$<TARGET_FILE_DIR:ZzLog>"
                "${zz_workspace_screenshot_library_path_variable}=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endforeach()
endif()

add_executable(ZzApplicationBuilderTest ZzApplicationBuilderTest.cpp)
target_link_libraries(ZzApplicationBuilderTest PRIVATE
    Qt6::Test
    Zz::PureTools
    Zz::WindowKit
    Zz::FluentUI
)
set_target_properties(ZzApplicationBuilderTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzApplicationBuilderTest)
zz_enable_sanitizers(ZzApplicationBuilderTest)
set(zz_puretools_window_environment "QT_QPA_PLATFORM=offscreen")
if(APPLE)
    set(zz_puretools_window_environment "QT_QPA_PLATFORM=cocoa")
endif()
set(builder_scenarios
    buildFreezesModulesAndPages
    buildRejectsDuplicateRoutesBeforeStartingModules
    buildRejectsNavigationNodeWithoutPage
    buildRejectsInvalidNavigationPresentationBeforeStartingModules
    buildRequiresRegisteredInitialRoute
    moduleStartFailureDoesNotCreateWindow
    successfulBuildCreatesOneWindowWithoutCreatingUnvisitedPages
    windowSetupCallbackValidatesAndRunsBeforeDisplay
    windowSetupFailureRollsBackInitialWindow
    subsequentWindowsReuseWindowSetupCallback
    windowFailureLeavesApplicationUnbuilt
    failedBuildCanRetryWithFreshBuilder
    successfulApplicationRejectsSecondBuilder
    applicationInstallsFluentStyleBeforeBuilding
    aboutToQuitDestroysWindowsBeforeStoppingModules
)
foreach(scenario IN LISTS builder_scenarios)
    add_test(
        NAME puretools.application-builder.${scenario}
        COMMAND ZzApplicationBuilderTest ${scenario}
    )
    set_tests_properties(puretools.application-builder.${scenario} PROPERTIES
        LABELS "component;puretools"
        ENVIRONMENT "${zz_puretools_window_environment}"
    )
endforeach()

add_executable(ZzMultiWindowIsolationTest ZzMultiWindowIsolationTest.cpp)
target_link_libraries(ZzMultiWindowIsolationTest PRIVATE
    Qt6::Test
    Zz::PureTools
    Zz::WindowKit
    Zz::FluentUI
)
set_target_properties(ZzMultiWindowIsolationTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzMultiWindowIsolationTest)
zz_enable_sanitizers(ZzMultiWindowIsolationTest)
set(multi_window_scenarios
    everyWindowOwnsDifferentWindowAgent
    everyWindowOwnsDifferentNavigationState
    titleBarIntentInvokesOnlyOwningWindow
    chromeConfigurationUsesTitleBarChildren
    nativeSystemButtonCapabilityControlsCustomButtons
    navigationIntentUsesTheOwningModelRoute
    navigationSelectionTracksRoutesAndModelResets
    footerNavigationAffectsOnlyOwningWindow
    topLevelWindowDisablesDeleteOnClose
    acceptedCloseQueuesExactlyOneApplicationErase
    metaObjectSignalWithoutAcceptedCloseCannotEraseWindow
)
foreach(scenario IN LISTS multi_window_scenarios)
    add_test(
        NAME puretools.multi-window.${scenario}
        COMMAND ZzMultiWindowIsolationTest ${scenario}
    )
    set_tests_properties(puretools.multi-window.${scenario} PROPERTIES
        LABELS "component;puretools"
        ENVIRONMENT "${zz_puretools_window_environment}"
    )
endforeach()

add_executable(ZzTranslationLifecycleTest ZzTranslationLifecycleTest.cpp)
find_package(Qt6 6.8 REQUIRED COMPONENTS LinguistTools)
qt_add_lrelease(ZzTranslationLifecycleTest
    TS_FILES ZzTranslationLifecycleTest_zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE zz_translation_test_qm_files
)
list(LENGTH zz_translation_test_qm_files zz_translation_test_qm_count)
if(NOT zz_translation_test_qm_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one translation test QM file")
endif()
list(GET zz_translation_test_qm_files 0 zz_translation_test_qm)
file(TO_CMAKE_PATH "${zz_translation_test_qm}" zz_translation_test_qm)
target_compile_definitions(ZzTranslationLifecycleTest PRIVATE
    "ZZ_TRANSLATION_TEST_QM=\"${zz_translation_test_qm}\"")
target_link_libraries(ZzTranslationLifecycleTest PRIVATE
    Qt6::Test
    Zz::PureTools
    Zz::WindowKit
    Zz::FluentUI
)
set_target_properties(ZzTranslationLifecycleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzTranslationLifecycleTest)
zz_enable_sanitizers(ZzTranslationLifecycleTest)
set(translation_scenarios
    languageChangeRefreshesStaticAndDynamicText
    failedTranslatorLoadLeavesApplicationUnbuilt
    runtimeFailureRollsBackInstalledTranslators
)
foreach(scenario IN LISTS translation_scenarios)
    add_test(
        NAME puretools.translation.${scenario}
        COMMAND ZzTranslationLifecycleTest ${scenario}
    )
    set_tests_properties(puretools.translation.${scenario} PROPERTIES
        LABELS "component;puretools"
        ENVIRONMENT "${zz_puretools_window_environment}"
    )
endforeach()
