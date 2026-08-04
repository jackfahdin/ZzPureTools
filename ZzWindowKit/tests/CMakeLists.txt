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
