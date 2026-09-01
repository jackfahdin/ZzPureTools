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

if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_test(
        NAME architecture.generated-code-flags
        COMMAND "${CMAKE_COMMAND}"
            "-DZZ_COMPILE_COMMANDS=${CMAKE_BINARY_DIR}/compile_commands.json"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckGeneratedCodeFlags.cmake"
    )
    set_tests_properties(architecture.generated-code-flags PROPERTIES
        LABELS "architecture"
    )
endif()

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

include(ZzArchitectureChecks)

zz_add_public_header_directory(
    OWNER Zz::Core
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzCore/include"
)
zz_add_public_header_directory(
    OWNER Zz::WindowKit
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzWindowKit/include"
)
zz_add_public_header_directory(
    OWNER Zz::FluentFoundation
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzFluentUI/foundation/include"
)
zz_add_public_header_directory(
    OWNER Zz::FluentUI
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzFluentUI/widgets/include"
)
zz_add_public_header_directory(
    OWNER Zz::AppCore
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzPureTools/appcore/include"
)
zz_add_public_header_directory(
    OWNER Zz::PureTools
    DIRECTORY "${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/include"
)

zz_add_public_header_probe(
    OWNER Zz::Core
    HEADER ZzCore/ZzCoreExport.h
)
zz_add_public_header_probe(
    OWNER Zz::WindowKit
    HEADER ZzWindowKit/ZzWindowKitExport.h
)
zz_add_public_header_probe(
    OWNER Zz::FluentFoundation
    HEADER ZzFluentUI/ZzFluentFoundationExport.h
)
zz_add_public_header_probe(
    OWNER Zz::FluentUI
    HEADER ZzFluentUI/ZzFluentUIExport.h
)
zz_add_public_header_probe(
    OWNER Zz::AppCore
    HEADER ZzPureTools/ZzAppCoreExport.h
)
zz_add_public_header_probe(
    OWNER Zz::PureTools
    HEADER ZzPureTools/ZzPureToolsExport.h
)

add_test(
    NAME architecture.public-headers
    COMMAND "${CMAKE_COMMAND}"
        --build "${CMAKE_BINARY_DIR}"
        --target ZzPublicHeadersTest
        --config "$<CONFIG>"
)
set_tests_properties(architecture.public-headers PROPERTIES
    LABELS "architecture;headers"
    RUN_SERIAL TRUE
)

add_test(
    NAME architecture.boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/RunArchitectureChecks.cmake"
)
set_tests_properties(architecture.boundaries PROPERTIES
    LABELS "architecture"
)

set(zz_target_manifest
    "${CMAKE_CURRENT_BINARY_DIR}/ZzTargetLinks-$<CONFIG>.txt")
file(GENERATE OUTPUT "${zz_target_manifest}" CONTENT
"ZzCore|$<TARGET_PROPERTY:ZzCore,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzCore,INTERFACE_LINK_LIBRARIES>\nZzWindowKit|$<TARGET_PROPERTY:ZzWindowKit,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzWindowKit,INTERFACE_LINK_LIBRARIES>\nZzFluentFoundation|$<TARGET_PROPERTY:ZzFluentFoundation,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzFluentFoundation,INTERFACE_LINK_LIBRARIES>\nZzFluentUI|$<TARGET_PROPERTY:ZzFluentUI,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzFluentUI,INTERFACE_LINK_LIBRARIES>\nZzAppCore|$<TARGET_PROPERTY:ZzAppCore,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzAppCore,INTERFACE_LINK_LIBRARIES>\nZzPureTools|$<TARGET_PROPERTY:ZzPureTools,LINK_LIBRARIES>|$<TARGET_PROPERTY:ZzPureTools,INTERFACE_LINK_LIBRARIES>\n")
add_test(NAME architecture.complete-audit
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TARGET_MANIFEST=${zz_target_manifest}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzArchitectureAudit.cmake")
set_tests_properties(architecture.complete-audit PROPERTIES
    LABELS "architecture")

add_test(
    NAME architecture.workspace-boundaries-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzWorkspaceBoundariesContract.cmake"
)
set_tests_properties(architecture.workspace-boundaries-contract PROPERTIES
    LABELS "architecture;contract;workspace")

add_test(NAME architecture.fluent-visual-token-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fluent-visual-token-contract"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzFluentVisualTokenContract.cmake")
set_tests_properties(architecture.fluent-visual-token-contract PROPERTIES
    LABELS "architecture;contract;fluent")

add_test(
    NAME architecture.zzcore-dependencies
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SCAN_ROOT=${PROJECT_SOURCE_DIR}/ZzCore"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzCoreDependencies.cmake"
)
set_tests_properties(architecture.zzcore-dependencies PROPERTIES
    LABELS "architecture;core"
)

add_test(
    NAME architecture.zzcore-dependencies-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzCoreDependenciesContract.cmake"
)
set_tests_properties(architecture.zzcore-dependencies-contract PROPERTIES
    LABELS "architecture;core"
)

add_test(NAME architecture.zzwindowkit-boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzWindowKitBoundaries.cmake")
set_tests_properties(architecture.zzwindowkit-boundaries PROPERTIES
    LABELS "architecture;windowkit")

add_test(NAME architecture.zzfluent-boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzFluentBoundaries.cmake")
set_tests_properties(architecture.zzfluent-boundaries PROPERTIES
    LABELS "architecture;fluent")

add_test(NAME architecture.zzfluentui-boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzFluentUIBoundaries.cmake")
set_tests_properties(architecture.zzfluentui-boundaries PROPERTIES
    LABELS "architecture;fluent;widgets")

add_test(
    NAME architecture.puretools-boundaries-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzPureToolsBoundariesContract.cmake"
)
set_tests_properties(architecture.puretools-boundaries-contract PROPERTIES
    LABELS "architecture;puretools"
)

add_test(
    NAME architecture.puretools-boundaries
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_APPCORE_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/appcore"
        "-DZZ_WIDGETS_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/widgets"
        "-DZZ_APPCORE_PUBLIC_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/appcore/include"
        "-DZZ_WIDGETS_PUBLIC_ROOT=${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/include"
        "-DZZ_ALLOWED_COMPOSITION_FILE=${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/src/private/ZzApplicationWindowPrivate.cpp|${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp"
        -DZZ_REQUIRE_COMPOSITION=ON
        -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckZzPureToolsBoundaries.cmake"
)
set_tests_properties(architecture.puretools-boundaries PROPERTIES
    LABELS "architecture;appcore;puretools"
)

add_test(NAME architecture.documentation-audit
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzDocumentationAudit.cmake")
set_tests_properties(architecture.documentation-audit PROPERTIES
    LABELS "architecture;documentation")

add_executable(ZzCorePublicHeadersTest ZzCorePublicHeadersTest.cpp)
target_link_libraries(ZzCorePublicHeadersTest PRIVATE Zz::Core)
zz_configure_first_party_target(ZzCorePublicHeadersTest
    SOURCES ZzCorePublicHeadersTest.cpp)
add_test(NAME headers.zzcore-smoke COMMAND ZzCorePublicHeadersTest)
set_tests_properties(headers.zzcore-smoke PROPERTIES
    LABELS "headers;core"
)
