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
zz_add_benchmark(ZzNavigationPaneBenchmark
    ZzNavigationPaneBenchmark.cpp benchmark.navigation-pane)
add_executable(ZzBackdropBenchmark
    ZzBackdropBenchmark.cpp
    "${PROJECT_SOURCE_DIR}/ZzWindowKit/src/private/ZzSoftwareBackdrop.cpp"
    "${PROJECT_SOURCE_DIR}/ZzWindowKit/src/private/ZzSoftwareBackdropPrivate.cpp")
target_include_directories(ZzBackdropBenchmark PRIVATE
    "${PROJECT_SOURCE_DIR}/ZzWindowKit/src/private")
target_link_libraries(ZzBackdropBenchmark PRIVATE
    ZzBenchmarkSupport Qt6::Core Qt6::Gui Qt6::Widgets)
set_target_properties(ZzBackdropBenchmark PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzBackdropBenchmark)
zz_enable_sanitizers(ZzBackdropBenchmark)
add_test(NAME benchmark.backdrop COMMAND ZzBackdropBenchmark
    --report "${CMAKE_BINARY_DIR}/reports/benchmark.backdrop.json")
set_tests_properties(benchmark.backdrop PROPERTIES
    LABELS "benchmark"
    TIMEOUT 120)

target_link_libraries(ZzStartupBenchmark PRIVATE Zz::PureTools)
target_link_libraries(ZzThemeSwitchBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzAnimationBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzLargeModelBenchmark PRIVATE Zz::FluentUI)
target_link_libraries(ZzWindowLifecycleBenchmark PRIVATE
    Zz::PureTools Zz::WindowKit)
target_link_libraries(ZzNavigationPaneBenchmark PRIVATE Zz::FluentUI)
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

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    add_subdirectory(ZzIdleProbe)
    add_test(NAME benchmark.idle-cpu
        COMMAND ZzIdleProbe
            --report "${CMAKE_BINARY_DIR}/reports/benchmark.idle.json")
    set_tests_properties(benchmark.idle-cpu PROPERTIES
        LABELS "benchmark;linux"
        TIMEOUT 45)
endif()

if(ZZ_PERFORMANCE_REFERENCE)
    function(zz_add_reference_gate gate_name producer scenario metric)
        set(options STRICT_MAX)
        set(one_value_arguments MAX_P95 MAX_VALUE)
        cmake_parse_arguments(PARSE_ARGV 4 gate
            "${options}" "${one_value_arguments}" "")
        if(gate_UNPARSED_ARGUMENTS)
            message(FATAL_ERROR
                "Unknown reference gate arguments: ${gate_UNPARSED_ARGUMENTS}")
        endif()
        if(NOT DEFINED gate_MAX_P95 AND NOT DEFINED gate_MAX_VALUE)
            message(FATAL_ERROR
                "Reference gate ${gate_name} requires a threshold")
        endif()

        set(gate_command
            "${CMAKE_COMMAND}"
            "-DZZ_REPORT=${CMAKE_BINARY_DIR}/reports/benchmark.${scenario}.json"
            "-DZZ_SCENARIO=${scenario}"
            "-DZZ_METRIC=${metric}"
            "-DZZ_FINGERPRINT_REFERENCE=${CMAKE_BINARY_DIR}/reports/benchmark.startup.json")
        if(DEFINED gate_MAX_P95)
            list(APPEND gate_command "-DZZ_MAX_P95=${gate_MAX_P95}")
        endif()
        if(DEFINED gate_MAX_VALUE)
            list(APPEND gate_command "-DZZ_MAX_VALUE=${gate_MAX_VALUE}")
        endif()
        if(gate_STRICT_MAX)
            list(APPEND gate_command -DZZ_STRICT_MAX=TRUE)
        endif()
        list(APPEND gate_command
            -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyPerformanceReport.cmake")

        add_test(NAME "benchmark.reference-gate.${gate_name}"
            COMMAND ${gate_command})
        set(gate_dependencies "${producer}")
        if(NOT "${producer}" STREQUAL "benchmark.startup")
            list(APPEND gate_dependencies benchmark.startup)
        endif()
        set_tests_properties("benchmark.reference-gate.${gate_name}" PROPERTIES
            DEPENDS "${gate_dependencies}"
            LABELS "benchmark;reference-gate"
            TIMEOUT 10)
    endfunction()

    zz_add_reference_gate(startup benchmark.startup
        startup external-total MAX_P95 300 MAX_VALUE 300)
    zz_add_reference_gate(theme-switch benchmark.theme-switch
        theme-switch latency MAX_P95 50)
    zz_add_reference_gate(animation benchmark.animation
        animation frame-time MAX_P95 16.7)
    zz_add_reference_gate(large-model benchmark.large-model
        large-model frame-time MAX_P95 16.7)
    zz_add_reference_gate(navigation-pane-frame benchmark.navigation-pane
        navigation-pane frame-time MAX_P95 12)
    zz_add_reference_gate(navigation-pane-paint benchmark.navigation-pane
        navigation-pane paint-complexity-ratio
        MAX_VALUE 1.5 STRICT_MAX)
    zz_add_reference_gate(navigation-pane-reset benchmark.navigation-pane
        navigation-pane reset-time MAX_P95 80)
    if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
        zz_add_reference_gate(idle-cpu benchmark.idle-cpu
            idle average-cpu-percent MAX_VALUE 0.5 STRICT_MAX)
        zz_add_reference_gate(idle-rss benchmark.idle-cpu
            idle rss-growth-percent MAX_VALUE 10)
    endif()
endif()

add_test(NAME benchmark.relative-comparison-valid
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        "-DZZ_CURRENT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        "-DZZ_THRESHOLDS=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-thresholds-valid.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake")
add_test(NAME benchmark.relative-comparison-rejects-regression
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        "-DZZ_CURRENT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-regressed.json"
        "-DZZ_THRESHOLDS=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-thresholds-valid.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake")
add_test(NAME benchmark.relative-comparison-rejects-environment-mismatch
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_BASELINE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-valid.json"
        "-DZZ_CURRENT=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-mismatched-environment.json"
        "-DZZ_THRESHOLDS=${CMAKE_CURRENT_SOURCE_DIR}/testdata/performance-thresholds-valid.json"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzComparePerformanceReport.cmake")
set_tests_properties(
    benchmark.relative-comparison-rejects-regression
    benchmark.relative-comparison-rejects-environment-mismatch
    PROPERTIES WILL_FAIL TRUE LABELS "benchmark;unit")
set_tests_properties(benchmark.relative-comparison-valid PROPERTIES
    LABELS "benchmark;unit")
