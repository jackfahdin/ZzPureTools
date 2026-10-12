add_executable(ZzStartupProbe main.cpp)
target_link_libraries(ZzStartupProbe PRIVATE
    Zz::PureTools Zz::WindowKit Qt6::Widgets)
zz_enable_project_warnings(ZzStartupProbe)
zz_enable_sanitizers(ZzStartupProbe)

add_executable(ZzStartupReadyWaiterTest ZzStartupReadyWaiterTest.cpp)
target_link_libraries(ZzStartupReadyWaiterTest PRIVATE Qt6::Widgets Qt6::Test)
set_target_properties(ZzStartupReadyWaiterTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzStartupReadyWaiterTest)
zz_enable_sanitizers(ZzStartupReadyWaiterTest)
add_test(NAME benchmark.startup-readiness COMMAND ZzStartupReadyWaiterTest)
set_tests_properties(benchmark.startup-readiness PROPERTIES
    LABELS "benchmark;unit" TIMEOUT 30)
