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
