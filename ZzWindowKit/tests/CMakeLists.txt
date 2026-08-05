add_executable(ZzWindowAgentTest
    ZzWindowAgentTest.cpp
    private/ZzFakeWindowBackend.cpp
)
target_include_directories(ZzWindowAgentTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/private
)
target_link_libraries(ZzWindowAgentTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::WindowKit
)
set_target_properties(ZzWindowAgentTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWindowAgentTest)
zz_enable_sanitizers(ZzWindowAgentTest)
add_test(NAME windowkit.agent COMMAND ZzWindowAgentTest)
set_tests_properties(windowkit.agent PROPERTIES
    LABELS "unit;windowkit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(ZzWindowKitBootstrapTest
    ZzWindowKitBootstrapTest.cpp
)
target_link_libraries(ZzWindowKitBootstrapTest PRIVATE
    Qt6::Core
    Zz::WindowKit
)
zz_enable_project_warnings(ZzWindowKitBootstrapTest)
zz_enable_sanitizers(ZzWindowKitBootstrapTest)
add_test(NAME windowkit.bootstrap COMMAND ZzWindowKitBootstrapTest)
set_tests_properties(windowkit.bootstrap PROPERTIES
    LABELS "unit;windowkit"
)

add_executable(ZzWindowKitLifecycleTest
    ZzWindowKitLifecycleTest.cpp
)
target_link_libraries(ZzWindowKitLifecycleTest PRIVATE
    Qt6::Test
    Qt6::Widgets
    Zz::WindowKit
)
set_target_properties(ZzWindowKitLifecycleTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzWindowKitLifecycleTest)
zz_enable_sanitizers(ZzWindowKitLifecycleTest)
add_test(NAME windowkit.lifecycle COMMAND ZzWindowKitLifecycleTest)
set(zz_windowkit_lifecycle_environment "QT_QPA_PLATFORM=offscreen")
if(APPLE)
    set(zz_windowkit_lifecycle_environment "QT_QPA_PLATFORM=cocoa")
endif()
set_tests_properties(windowkit.lifecycle PROPERTIES
    LABELS "platform;windowkit"
    ENVIRONMENT "${zz_windowkit_lifecycle_environment}"
)
if(APPLE AND NOT ZZ_WINDOWKIT_FORCE_QT_CONTEXT)
    add_test(
        NAME windowkit.offscreen-rejected
        COMMAND ZzWindowKitLifecycleTest
            rejectsOffscreenBeforeCreatingNativeBackend
    )
    set_tests_properties(windowkit.offscreen-rejected PROPERTIES
        LABELS "platform;windowkit;contract"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
