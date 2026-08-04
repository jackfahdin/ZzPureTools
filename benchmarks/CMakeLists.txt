add_library(ZzBenchmarkSupport STATIC
    common/ZzBenchmarkMetadata.cpp
    common/ZzPerformanceReporter.cpp
)
target_include_directories(ZzBenchmarkSupport PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
)
target_link_libraries(ZzBenchmarkSupport PUBLIC
    Qt6::Core
    Qt6::Gui
    Zz::Core
)
set(zz_benchmark_sanitizers none)
if(ZZ_ENABLE_ASAN OR ZZ_ENABLE_UBSAN)
    set(zz_benchmark_sanitizers asan-ubsan)
endif()
target_compile_definitions(ZzBenchmarkSupport PRIVATE
    "ZZ_BENCHMARK_BUILD_TYPE=\"$<CONFIG>\""
    "ZZ_BENCHMARK_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    "ZZ_BENCHMARK_SHARED=$<BOOL:${BUILD_SHARED_LIBS}>"
    "ZZ_BENCHMARK_LTO=$<BOOL:${ZZ_ENABLE_LTO}>"
    "ZZ_BENCHMARK_SANITIZERS=\"${zz_benchmark_sanitizers}\""
)
zz_enable_project_warnings(ZzBenchmarkSupport)
zz_enable_sanitizers(ZzBenchmarkSupport)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/reports")

add_executable(ZzPerformanceReporterTest
    ZzPerformanceReporterTest.cpp
)
target_link_libraries(ZzPerformanceReporterTest PRIVATE
    ZzBenchmarkSupport
    Qt6::Test
)
set_target_properties(ZzPerformanceReporterTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzPerformanceReporterTest)
zz_enable_sanitizers(ZzPerformanceReporterTest)
add_test(NAME benchmark.reporter COMMAND ZzPerformanceReporterTest)
set_tests_properties(benchmark.reporter PROPERTIES LABELS "benchmark;unit")

add_test(NAME benchmark.report-verifier-valid
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=100
        -DZZ_MAX_VALUE=100
        "-DZZ_FINGERPRINT_REFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
add_test(NAME benchmark.report-verifier-invalid
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-invalid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=100
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
add_test(NAME benchmark.report-verifier-rejects-threshold
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=99
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
add_test(NAME benchmark.report-verifier-rejects-fingerprint
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_REPORT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        -DZZ_SCENARIO=contract
        -DZZ_METRIC=latency
        -DZZ_MAX_P95=100
        "-DZZ_FINGERPRINT_REFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-invalid.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake"
)
set_tests_properties(
    benchmark.report-verifier-invalid
    benchmark.report-verifier-rejects-threshold
    benchmark.report-verifier-rejects-fingerprint
    PROPERTIES
    WILL_FAIL TRUE
    LABELS "benchmark;unit"
)
set_tests_properties(benchmark.report-verifier-valid PROPERTIES
    LABELS "benchmark;unit")

add_subdirectory(ZzStartupProbe)

function(zz_add_benchmark target_name source_name test_name)
    add_executable(${target_name} "${source_name}")
    target_link_libraries(${target_name} PRIVATE
        ZzBenchmarkSupport Qt6::Core Qt6::Gui Qt6::Test Qt6::Widgets)
    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(${target_name})
    zz_enable_sanitizers(${target_name})
    add_test(NAME "${test_name}" COMMAND ${target_name}
        --report "${CMAKE_BINARY_DIR}/reports/${test_name}.json")
    set_tests_properties("${test_name}" PROPERTIES
        LABELS "benchmark"
        TIMEOUT 120)
endfunction()

zz_add_benchmark(ZzStartupBenchmark
    ZzStartupBenchmark.cpp benchmark.startup)
zz_add_benchmark(ZzThemeSwitchBenchmark
    ZzThemeSwitchBenchmark.cpp benchmark.theme-switch)
zz_add_benchmark(ZzAnimationBenchmark
    ZzAnimationBenchmark.cpp benchmark.animation)
zz_add_benchmark(ZzLargeModelBenchmark
    ZzLargeModelBenchmark.cpp benchmark.large-model)
zz_add_benchmark(ZzWindowLifecycleBenchmark
    ZzWindowLifecycleBenchmark.cpp benchmark.window-lifecycle)

target_link_libraries(ZzStartupBenchmark PRIVATE Zz::PureTools)
target_link_libraries(ZzThemeSwitchBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzAnimationBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzLargeModelBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzWindowLifecycleBenchmark PRIVATE
    Zz::PureTools Zz::WindowKit)
add_dependencies(ZzStartupBenchmark ZzStartupProbe)
set(zz_startup_probe_generated_dir
    "${CMAKE_CURRENT_BINARY_DIR}/generated/$<CONFIG>")
file(GENERATE
    OUTPUT "${zz_startup_probe_generated_dir}/ZzStartupProbePath.h"
    CONTENT
"#pragma once
inline constexpr char ZzStartupProbePath[] = R\"ZZPROBE($<TARGET_FILE:ZzStartupProbe>)ZZPROBE\";
")
target_include_directories(ZzStartupBenchmark PRIVATE
    "${zz_startup_probe_generated_dir}")
target_compile_definitions(ZzWindowLifecycleBenchmark PRIVATE
    ZZ_WINDOWKIT_DIAGNOSTICS=1)
target_include_directories(ZzWindowLifecycleBenchmark PRIVATE
    "${PROJECT_SOURCE_DIR}/ZzWindowKit/src/private")
