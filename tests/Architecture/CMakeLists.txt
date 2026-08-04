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
