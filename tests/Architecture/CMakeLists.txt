add_executable(ZzGeneratedCodeProbe
    ZzGeneratedCodeProbe.h
    ZzGeneratedCodeProbe.cpp
    resources/ZzGeneratedCodeProbe.qrc
)
target_link_libraries(ZzGeneratedCodeProbe PRIVATE Qt6::Core)
zz_configure_first_party_target(ZzGeneratedCodeProbe
    SOURCES ZzGeneratedCodeProbe.cpp)

add_test(
    NAME architecture.generated-code-probe
    COMMAND ZzGeneratedCodeProbe
)
set_tests_properties(architecture.generated-code-probe PROPERTIES
    LABELS "architecture;unit"
)

add_test(
    NAME architecture.generated-code-flags
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_COMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckGeneratedCodeFlags.cmake"
)
set_tests_properties(architecture.generated-code-flags PROPERTIES
    LABELS "architecture"
)

add_executable(ZzVersionApiTest
    ZzVersionApiTest.h
    ZzVersionApiTest.cpp
    ZzVersionApiTestMain.cpp
)
target_link_libraries(ZzVersionApiTest PRIVATE
    Qt6::Test
    Zz::Core
    Zz::WindowKit
    Zz::FluentFoundation
    Zz::FluentUI
    Zz::AppCore
    Zz::PureTools
)
zz_configure_first_party_target(ZzVersionApiTest
    SOURCES
        ZzVersionApiTest.cpp
        ZzVersionApiTestMain.cpp
)

add_test(NAME architecture.version-api COMMAND ZzVersionApiTest)
set_tests_properties(architecture.version-api PROPERTIES
    LABELS "architecture;unit"
)
