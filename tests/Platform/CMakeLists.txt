add_executable(ZzPlatformCompileTest ZzPlatformCompileTest.cpp)
target_link_libraries(ZzPlatformCompileTest PRIVATE
    Zz::Core
    Zz::WindowKit
    Zz::FluentFoundation
    Zz::FluentUI
    Zz::AppCore
    Zz::PureTools
    Qt6::Core
    Qt6::Gui
)
zz_configure_first_party_target(ZzPlatformCompileTest
    SOURCES ZzPlatformCompileTest.cpp)

add_test(NAME platform.compile COMMAND ZzPlatformCompileTest)
set_tests_properties(platform.compile PROPERTIES LABELS "platform")
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set_tests_properties(platform.compile PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    foreach(zz_origin_target IN ITEMS
        ZzPlatformCompileTest
        ZzCore
        ZzWindowKit
        ZzFluentFoundation
        ZzFluentUI
        ZzAppCore
        ZzPureTools
    )
        set_property(TARGET ${zz_origin_target} PROPERTY
            BUILD_RPATH_USE_ORIGIN TRUE)
    endforeach()
endif()

set(platform_support_dir "${CMAKE_CURRENT_BINARY_DIR}/support")
file(MAKE_DIRECTORY "${platform_support_dir}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/ZzPlatformGateContext.cmake.in"
    "${platform_support_dir}/ZzPlatformGateContext.cmake"
    @ONLY)

set(zz_platform_readelf)
set(zz_platform_ldd)
set(zz_platform_otool)
set(zz_platform_lipo)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    find_program(zz_platform_readelf NAMES readelf REQUIRED)
    find_program(zz_platform_ldd NAMES ldd REQUIRED)
elseif(APPLE)
    find_program(zz_platform_otool NAMES otool REQUIRED)
    find_program(zz_platform_lipo NAMES lipo REQUIRED)
endif()

set(zz_binary_context
    "${CMAKE_CURRENT_BINARY_DIR}/ZzBinaryContext-$<CONFIG>.cmake")
file(GENERATE OUTPUT "${zz_binary_context}" CONTENT
"set(ZZ_SYSTEM_NAME [==[${CMAKE_SYSTEM_NAME}]==])
set(ZZ_COMPILER_ID [==[${CMAKE_CXX_COMPILER_ID}]==])
set(ZZ_BUILD_SHARED [==[${BUILD_SHARED_LIBS}]==])
set(ZZ_ENABLE_ASAN [==[${ZZ_ENABLE_ASAN}]==])
set(ZZ_ENABLE_UBSAN [==[${ZZ_ENABLE_UBSAN}]==])
set(ZZ_SOURCE_DIR [==[${PROJECT_SOURCE_DIR}]==])
set(ZZ_BINARY_DIR [==[${CMAKE_BINARY_DIR}]==])
set(ZZ_QT_PREFIX [==[${ZZ_QT_PREFIX}]==])
set(ZZ_CMAKE_OBJDUMP [==[${CMAKE_OBJDUMP}]==])
set(ZZ_DUMPBIN [==[$ENV{ZZ_DUMPBIN}]==])
set(ZZ_MINGW_OBJDUMP [==[$ENV{ZZ_MINGW_OBJDUMP}]==])
set(ZZ_READELF [==[${zz_platform_readelf}]==])
set(ZZ_LDD [==[${zz_platform_ldd}]==])
set(ZZ_OTOOL [==[${zz_platform_otool}]==])
set(ZZ_LIPO [==[${zz_platform_lipo}]==])
set(ZZ_EXPECTED_OSX_ARCHITECTURES [==[${CMAKE_OSX_ARCHITECTURES}]==])
set(ZZ_PLATFORM_EXECUTABLE [==[$<TARGET_FILE:ZzPlatformCompileTest>]==])
set(ZZ_CORE_FILE [==[$<TARGET_FILE:ZzCore>]==])
set(ZZ_WINDOWKIT_FILE [==[$<TARGET_FILE:ZzWindowKit>]==])
set(ZZ_FLUENT_FOUNDATION_FILE [==[$<TARGET_FILE:ZzFluentFoundation>]==])
set(ZZ_FLUENT_UI_FILE [==[$<TARGET_FILE:ZzFluentUI>]==])
set(ZZ_APP_CORE_FILE [==[$<TARGET_FILE:ZzAppCore>]==])
set(ZZ_PURE_TOOLS_FILE [==[$<TARGET_FILE:ZzPureTools>]==])
")

add_test(NAME platform.binary-dependencies
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_CONTEXT_FILE=${zz_binary_context}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzBinaryDependencyCheck.cmake")
set_tests_properties(platform.binary-dependencies PROPERTIES
    DEPENDS platform.compile
    LABELS "platform;binary")

add_test(NAME platform.package-relocation
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/relocation/$<CONFIG>"
        "-DZZ_CONTEXT_FILE=${platform_support_dir}/ZzPlatformGateContext.cmake"
        "-DZZ_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzPackageRelocationTest.cmake")
set_tests_properties(platform.package-relocation PROPERTIES
    LABELS "platform;install;headers"
    RUN_SERIAL TRUE
    TIMEOUT 900)

add_test(NAME platform.gate-script-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzGateScriptContract.cmake")
set_tests_properties(platform.gate-script-contract PROPERTIES
    LABELS "platform;contract")

add_test(NAME platform.performance-threshold-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/performance-threshold-contract"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzPerformanceThresholdContract.cmake")
set_tests_properties(platform.performance-threshold-contract PROPERTIES
    LABELS "platform;contract;benchmark")

add_test(NAME platform.github-actions-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzGitHubActionsContract.cmake")
set_tests_properties(platform.github-actions-contract PROPERTIES
    LABELS "platform;contract")

add_test(NAME platform.example-application-metadata-contract
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzExampleApplicationMetadataContract.cmake")
set_tests_properties(platform.example-application-metadata-contract PROPERTIES
    LABELS "platform;contract;packaging")

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    add_test(NAME platform.governor-state
        COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/ZzGovernorStateTest.sh"
            "${PROJECT_SOURCE_DIR}")
    set_tests_properties(platform.governor-state PROPERTIES
        LABELS "platform;contract;linux")

    add_test(NAME platform.linux-desktop-acceptance-contract
        COMMAND "${CMAKE_COMMAND}"
            "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzLinuxDesktopAcceptanceContract.cmake")
    set_tests_properties(platform.linux-desktop-acceptance-contract PROPERTIES
        LABELS "platform;contract;documentation")
endif()
