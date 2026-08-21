set(zz_pure_tools_example_sources
    main.cpp
    ZzExampleActivityModel.cpp
    ZzExampleApplicationContext.cpp
    ZzExampleApplicationContextPrivate.cpp
    ZzExampleApplicationModule.cpp
    ZzExampleApplicationModulePrivate.cpp
    ZzExampleCardsPage.cpp
    ZzExampleCardsPagePrivate.cpp
    ZzExampleCardsViewModel.cpp
    ZzExampleDataPage.cpp
    ZzExampleDataPagePrivate.cpp
    ZzExampleDataPresenter.cpp
    ZzExampleDataViewModel.cpp
    ZzExampleGalleryPage.cpp
    ZzExampleGalleryPagePrivate.cpp
    ZzExampleNavigationPresenter.cpp
    ZzExamplePageFactory.cpp
    ZzExampleRouteCatalog.cpp
    ZzExampleSessionModel.cpp
    ZzExampleShowcasePage.cpp
    ZzExampleShowcasePagePrivate.cpp
    ZzExampleSmokeController.cpp
    ZzExampleSmokeControllerPrivate.cpp
    ZzExampleSystemPage.cpp
    ZzExampleSystemPagePrivate.cpp
    ZzExampleSystemPresenter.cpp
    ZzExampleSystemPresenterPrivate.cpp
    ZzExampleSystemViewModel.cpp
    ZzExampleWindowShell.cpp
    ZzExampleWindowShellPrivate.cpp
    ZzExampleWorkspaceContent.cpp
)
if(ZZ_BUILD_BENCHMARKS)
    list(APPEND zz_pure_tools_example_sources
        ZzExamplePerformanceController.cpp
        ZzExamplePerformanceControllerPrivate.cpp
    )
endif()

add_executable(ZzPureToolsExample ${zz_pure_tools_example_sources})

set(ZZ_EXAMPLE_LOCAL_ASSET_DIR "" CACHE PATH
    "Optional untracked directory containing ZzPureToolsExample preview images")
mark_as_advanced(ZZ_EXAMPLE_LOCAL_ASSET_DIR)
if(NOT "${ZZ_EXAMPLE_LOCAL_ASSET_DIR}" STREQUAL "")
    if(NOT IS_DIRECTORY "${ZZ_EXAMPLE_LOCAL_ASSET_DIR}")
        message(FATAL_ERROR
            "ZZ_EXAMPLE_LOCAL_ASSET_DIR must identify a directory")
    endif()
    file(REAL_PATH "${ZZ_EXAMPLE_LOCAL_ASSET_DIR}"
        zz_example_local_asset_dir)
    set(zz_example_local_assets)
    foreach(asset_name IN ITEMS
        home.png
        card-performance.png
        card-windowing.png
        card-data.png)
        set(asset_path "${zz_example_local_asset_dir}/${asset_name}")
        if(NOT EXISTS "${asset_path}" OR IS_DIRECTORY "${asset_path}")
            message(FATAL_ERROR
                "Missing local example preview asset: ${asset_name}")
        endif()
        list(APPEND zz_example_local_assets "${asset_path}")
    endforeach()
    qt_add_resources(ZzPureToolsExample "zz_example_local_assets"
        PREFIX "/ZzPureToolsExample/local-assets"
        BASE "${zz_example_local_asset_dir}"
        FILES ${zz_example_local_assets}
    )
    target_compile_definitions(ZzPureToolsExample PRIVATE
        ZZ_EXAMPLE_LOCAL_PREVIEW_ASSETS=1)
    message(STATUS
        "ZzPureToolsExample uses untracked preview assets from ${zz_example_local_asset_dir}")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS LinguistTools)
qt_add_lrelease(ZzPureToolsExample
    TS_FILES translations/ZzPureToolsExample_en.ts
    OPTIONS -nounfinished
    QM_FILES_OUTPUT_VARIABLE zz_pure_tools_example_qm_files
)
list(LENGTH zz_pure_tools_example_qm_files
    zz_pure_tools_example_qm_count)
if(NOT zz_pure_tools_example_qm_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one example translation QM file")
endif()
list(GET zz_pure_tools_example_qm_files 0
    zz_pure_tools_example_english_qm)
set_source_files_properties(
    "${zz_pure_tools_example_english_qm}"
    PROPERTIES QT_RESOURCE_ALIAS "ZzPureToolsExample_en.qm")
qt_add_resources(ZzPureToolsExample "zz_example_translations"
    PREFIX "/translations"
    FILES "${zz_pure_tools_example_english_qm}"
)

target_link_libraries(ZzPureToolsExample PRIVATE
    Zz::FluentUI
    Zz::PureTools
    Zz::WindowKit
    ZzLog::ZzLog
)
if(ZZ_BUILD_BENCHMARKS)
    target_link_libraries(ZzPureToolsExample PRIVATE ZzBenchmarkSupport)
    target_compile_definitions(ZzPureToolsExample PRIVATE
        ZZ_EXAMPLE_PERFORMANCE_BENCHMARKS=1)
endif()

zz_configure_first_party_target(ZzPureToolsExample
    SOURCES ${zz_pure_tools_example_sources})

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)

    add_executable(ZzExampleActivityModelTest
        tests/ZzExampleActivityModelTest.cpp
        ZzExampleActivityModel.cpp
        ZzExampleActivityModel.h
        ZzExampleActivityModelPrivate.h
    )
    target_include_directories(ZzExampleActivityModelTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(ZzExampleActivityModelTest PRIVATE Qt6::Test)
    set_target_properties(ZzExampleActivityModelTest PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(ZzExampleActivityModelTest)
    zz_enable_sanitizers(ZzExampleActivityModelTest)
    add_test(
        NAME example.puretools-activity-model
        COMMAND ZzExampleActivityModelTest
    )
    set_tests_properties(example.puretools-activity-model PROPERTIES
        LABELS "example;unit")
endif()

if(ZZ_BUILD_BENCHMARKS AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set(zz_example_executable_generated_dir
        "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>")
    file(GENERATE
        OUTPUT
            "${zz_example_executable_generated_dir}/ZzExampleExecutablePath.h"
        CONTENT
"#pragma once
inline constexpr char ZzExampleExecutablePath[] = R\"ZZEXAMPLE($<TARGET_FILE:ZzPureToolsExample>)ZZEXAMPLE\";
")

    add_executable(ZzExampleStartupBenchmark
        "${PROJECT_SOURCE_DIR}/benchmarks/ZzExampleStartupBenchmark.cpp")
    target_include_directories(ZzExampleStartupBenchmark PRIVATE
        "${zz_example_executable_generated_dir}")
    target_link_libraries(ZzExampleStartupBenchmark PRIVATE
        ZzBenchmarkSupport Qt6::Gui)
    add_dependencies(ZzExampleStartupBenchmark ZzPureToolsExample)
    zz_configure_first_party_target(ZzExampleStartupBenchmark
        SOURCES
            "${PROJECT_SOURCE_DIR}/benchmarks/ZzExampleStartupBenchmark.cpp")

    add_test(NAME benchmark.example-startup
        COMMAND ZzExampleStartupBenchmark
            --report
            "${CMAKE_BINARY_DIR}/reports/benchmark.example-startup.json")
    set_tests_properties(benchmark.example-startup PROPERTIES
        LABELS "benchmark;example"
        TIMEOUT 120)

    function(zz_add_example_performance_test suffix timeout_seconds)
        add_test(NAME "benchmark.example-${suffix}"
            COMMAND "$<TARGET_FILE:ZzPureToolsExample>")
        set_tests_properties("benchmark.example-${suffix}" PROPERTIES
            ENVIRONMENT
                "ZZ_PURETOOLS_EXAMPLE_PERFORMANCE_SCENARIO=${suffix};ZZ_PURETOOLS_EXAMPLE_PERFORMANCE_REPORT=${CMAKE_BINARY_DIR}/reports/benchmark.example-${suffix}.json"
            LABELS "benchmark;example"
            TIMEOUT "${timeout_seconds}")
    endfunction()

    zz_add_example_performance_test(navigation 120)
    zz_add_example_performance_test(theme-switch 120)
    zz_add_example_performance_test(large-model 120)
    zz_add_example_performance_test(idle 50)

    if(ZZ_PERFORMANCE_REFERENCE)
        if(NOT COMMAND zz_add_reference_gate)
            message(FATAL_ERROR
                "Integrated example requires the benchmark reference gate helper")
        endif()
        zz_add_reference_gate(example-startup benchmark.example-startup
            example-startup external-total MAX_P95 300 MAX_VALUE 300)
        zz_add_reference_gate(example-navigation benchmark.example-navigation
            example-navigation latency MAX_P95 50)
        zz_add_reference_gate(example-theme-switch benchmark.example-theme-switch
            example-theme-switch latency MAX_P95 50)
        zz_add_reference_gate(example-large-model benchmark.example-large-model
            example-large-model frame-time MAX_P95 16.7)
        zz_add_reference_gate(example-idle-cpu benchmark.example-idle
            example-idle average-cpu-percent MAX_VALUE 0.5 STRICT_MAX)
        zz_add_reference_gate(example-idle-rss benchmark.example-idle
            example-idle rss-growth-percent MAX_VALUE 10)
    endif()
endif()
