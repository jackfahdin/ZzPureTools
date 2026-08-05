cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_PRESETS_FILE)
    message(FATAL_ERROR "Missing -DZZ_PRESETS_FILE=...")
endif()

file(READ "${ZZ_PRESETS_FILE}" presets_json)
string(JSON schema_version GET "${presets_json}" version)
if(NOT schema_version EQUAL 4)
    message(FATAL_ERROR "CMakePresets.json must use schema 4")
endif()

string(JSON preset_count LENGTH "${presets_json}" configurePresets)
math(EXPR last_preset "${preset_count} - 1")
set(actual_names)
foreach(index RANGE 0 ${last_preset})
    string(JSON name GET "${presets_json}" configurePresets ${index} name)
    list(APPEND actual_names "${name}")
endforeach()

set(required_names
    linux-gcc-debug
    linux-gcc-release
    linux-static-release
    linux-clang-release
    linux-clang-asan
    linux-gcc-release-lto
    linux-static-release-lto
    linux-clang-tidy-release
    linux-clang-tidy-static
    linux-gcc-lto-release
    linux-clang-tidy
    linux-gcc-benchmarks
    linux-gcc-reference
    linux-clang-asan-benchmarks
    windows-msvc2022-release
    windows-msvc2022-static
    windows-mingw-release
    windows-mingw-static
    macos-clang-release-arm64
    macos-clang-release-x86_64
    macos-clang-static-arm64
    macos-clang-static-x86_64
)
foreach(required_name IN LISTS required_names)
    if(NOT "${required_name}" IN_LIST actual_names)
        message(FATAL_ERROR "Missing configure preset: ${required_name}")
    endif()
endforeach()

function(zz_find_configure_preset_index output wanted_name)
    foreach(candidate_index RANGE 0 ${last_preset})
        string(JSON candidate_name GET "${presets_json}"
            configurePresets ${candidate_index} name)
        if("${candidate_name}" STREQUAL "${wanted_name}")
            set(${output} "${candidate_index}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "Missing configure preset: ${wanted_name}")
endfunction()

foreach(alias_pair IN ITEMS
        "linux-gcc-lto-release|linux-gcc-release-lto"
        "linux-clang-tidy|linux-clang-tidy-release")
    string(REPLACE "|" ";" alias_fields "${alias_pair}")
    list(GET alias_fields 0 alias_name)
    list(GET alias_fields 1 canonical_name)
    zz_find_configure_preset_index(alias_index "${alias_name}")
    string(JSON inherited GET "${presets_json}"
        configurePresets ${alias_index} inherits)
    if(NOT "${inherited}" STREQUAL "${canonical_name}")
        message(FATAL_ERROR
            "${alias_name} must inherit exactly ${canonical_name}")
    endif()
endforeach()

foreach(benchmark_pair IN ITEMS
        "linux-gcc-benchmarks|OFF"
        "linux-gcc-reference|ON")
    string(REPLACE "|" ";" benchmark_fields "${benchmark_pair}")
    list(GET benchmark_fields 0 benchmark_name)
    list(GET benchmark_fields 1 expected_reference)
    zz_find_configure_preset_index(benchmark_index "${benchmark_name}")
    string(JSON benchmark_base GET "${presets_json}"
        configurePresets ${benchmark_index} inherits)
    string(JSON benchmark_enabled GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables ZZ_BUILD_BENCHMARKS)
    string(JSON benchmark_shared GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables BUILD_SHARED_LIBS)
    string(JSON benchmark_lto GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables ZZ_ENABLE_LTO)
    string(JSON benchmark_reference GET "${presets_json}"
        configurePresets ${benchmark_index} cacheVariables ZZ_PERFORMANCE_REFERENCE)
    if(NOT "${benchmark_base}" STREQUAL "linux-gcc13-base"
       OR NOT benchmark_enabled
       OR NOT benchmark_shared
       OR NOT benchmark_lto
       OR NOT "${benchmark_reference}" STREQUAL "${expected_reference}")
        message(FATAL_ERROR
            "Invalid benchmark configuration in ${benchmark_name}")
    endif()
endforeach()

foreach(base_contract IN ITEMS
        "linux-gcc13-base|\$env{QT_ROOT}"
        "linux-clang17-base|\$env{QT_ROOT}"
        "windows-msvc-base|\$env{QT_MSVC_ROOT}"
        "windows-mingw-base|\$env{QT_MINGW_ROOT}")
    string(REPLACE "|" ";" base_fields "${base_contract}")
    list(GET base_fields 0 base_name)
    list(GET base_fields 1 expected_qt_prefix)
    zz_find_configure_preset_index(base_index "${base_name}")
    string(JSON find_prefix GET "${presets_json}"
        configurePresets ${base_index} cacheVariables CMAKE_PREFIX_PATH)
    string(JSON gate_prefix GET "${presets_json}"
        configurePresets ${base_index} cacheVariables ZZ_QT_PREFIX)
    if(NOT "${find_prefix}" STREQUAL "${expected_qt_prefix}"
       OR NOT "${gate_prefix}" STREQUAL "${expected_qt_prefix}")
        message(FATAL_ERROR "Qt prefix mismatch in ${base_name}")
    endif()
endforeach()

zz_find_configure_preset_index(clang_base_index "linux-clang17-base")
string(JSON c_external_toolchain GET "${presets_json}"
    configurePresets ${clang_base_index} cacheVariables
    CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN)
string(JSON cxx_external_toolchain GET "${presets_json}"
    configurePresets ${clang_base_index} cacheVariables
    CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN)
if(NOT "${c_external_toolchain}" STREQUAL "\$env{GCC_13_TOOLCHAIN_ROOT}"
   OR NOT "${cxx_external_toolchain}" STREQUAL "\$env{GCC_13_TOOLCHAIN_ROOT}")
    message(FATAL_ERROR
        "linux-clang17-base must use the GCC 13 external toolchain")
endif()

foreach(windows_contract IN ITEMS
        "windows-msvc2022-release|windows-msvc-base|ON"
        "windows-msvc2022-static|windows-msvc-base|OFF"
        "windows-mingw-release|windows-mingw-base|ON"
        "windows-mingw-static|windows-mingw-base|OFF")
    string(REPLACE "|" ";" windows_fields "${windows_contract}")
    list(GET windows_fields 0 windows_name)
    list(GET windows_fields 1 expected_base)
    list(GET windows_fields 2 expected_shared)
    zz_find_configure_preset_index(windows_index "${windows_name}")
    string(JSON windows_base GET "${presets_json}"
        configurePresets ${windows_index} inherits)
    string(JSON windows_shared GET "${presets_json}"
        configurePresets ${windows_index} cacheVariables BUILD_SHARED_LIBS)
    if(NOT "${windows_base}" STREQUAL "${expected_base}"
       OR NOT "${windows_shared}" STREQUAL "${expected_shared}")
        message(FATAL_ERROR "Invalid Windows identity in ${windows_name}")
    endif()
endforeach()

string(JSON build_preset_count LENGTH "${presets_json}" buildPresets)
math(EXPR last_build_preset "${build_preset_count} - 1")
set(build_names)
foreach(index RANGE 0 ${last_build_preset})
    string(JSON name GET "${presets_json}" buildPresets ${index} name)
    list(APPEND build_names "${name}")
    if("${name}" IN_LIST required_names)
        string(JSON configured_by GET "${presets_json}"
            buildPresets ${index} configurePreset)
        if(NOT "${configured_by}" STREQUAL "${name}")
            message(FATAL_ERROR
                "build preset ${name} must configure from itself")
        endif()
        if("${name}" MATCHES "^windows-msvc")
            string(JSON build_configuration GET "${presets_json}"
                buildPresets ${index} configuration)
            if(NOT "${build_configuration}" STREQUAL "Release")
                message(FATAL_ERROR
                    "MSVC build preset ${name} must select Release")
            endif()
        endif()
    endif()
endforeach()

string(JSON test_preset_count LENGTH "${presets_json}" testPresets)
math(EXPR last_test_preset "${test_preset_count} - 1")
set(test_names)
foreach(index RANGE 0 ${last_test_preset})
    string(JSON name GET "${presets_json}" testPresets ${index} name)
    list(APPEND test_names "${name}")
    if(NOT "${name}" IN_LIST required_names)
        continue()
    endif()
    string(JSON configured_by GET "${presets_json}"
        testPresets ${index} configurePreset)
    string(JSON output_on_failure GET "${presets_json}"
        testPresets ${index} output outputOnFailure)
    string(JSON no_tests_action GET "${presets_json}"
        testPresets ${index} execution noTestsAction)
    if(NOT "${configured_by}" STREQUAL "${name}"
       OR NOT output_on_failure
       OR NOT "${no_tests_action}" STREQUAL "error")
        message(FATAL_ERROR
            "test preset ${name} has an incomplete execution contract")
    endif()
    string(JSON recorded_preset ERROR_VARIABLE preset_error GET
        "${presets_json}" testPresets ${index} environment ZZ_CMAKE_PRESET)
    if(NOT "${preset_error}" STREQUAL "NOTFOUND"
       OR NOT "${recorded_preset}" STREQUAL "${name}")
        message(FATAL_ERROR
            "test preset ${name} must set literal ZZ_CMAKE_PRESET=${name}")
    endif()
    foreach(environment_pair IN ITEMS
            "ZZ_BENCHMARK_COMMIT|\$penv{ZZ_BENCHMARK_COMMIT}"
            "ZZ_RUNNER_IMAGE_DIGEST|\$penv{ZZ_RUNNER_IMAGE_DIGEST}"
            "ZZ_GPU_IDENTITY|\$penv{ZZ_GPU_IDENTITY}")
        string(REPLACE "|" ";" environment_fields "${environment_pair}")
        list(GET environment_fields 0 environment_name)
        list(GET environment_fields 1 expected_value)
        string(JSON recorded_value ERROR_VARIABLE environment_error GET
            "${presets_json}" testPresets ${index}
            environment "${environment_name}")
        if(NOT "${environment_error}" STREQUAL "NOTFOUND"
           OR NOT "${recorded_value}" STREQUAL "${expected_value}")
            message(FATAL_ERROR
                "test preset ${name} must pass through ${environment_name}")
        endif()
    endforeach()
    if("${name}" MATCHES "^windows-msvc")
        string(JSON test_configuration GET "${presets_json}"
            testPresets ${index} configuration)
        if(NOT "${test_configuration}" STREQUAL "Release")
            message(FATAL_ERROR
                "MSVC test preset ${name} must select Release")
        endif()
    endif()
endforeach()
foreach(required_name IN LISTS required_names)
    if(NOT "${required_name}" IN_LIST build_names)
        message(FATAL_ERROR "Missing build preset: ${required_name}")
    endif()
    if(NOT "${required_name}" IN_LIST test_names)
        message(FATAL_ERROR "Missing test preset: ${required_name}")
    endif()
endforeach()

set(macos_base_seen FALSE)
foreach(index RANGE 0 ${last_preset})
    string(JSON name GET "${presets_json}" configurePresets ${index} name)
    if("${name}" STREQUAL "macos-clang-base")
        set(macos_base_seen TRUE)
        string(JSON build_type GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_BUILD_TYPE)
        string(JSON deployment GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_OSX_DEPLOYMENT_TARGET)
        string(JSON tests GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_BUILD_TESTS)
        string(JSON warnings GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_WARNINGS_AS_ERRORS)
        string(JSON tidy GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_ENABLE_CLANG_TIDY)
        string(JSON lto GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_ENABLE_LTO)
        if(NOT "${build_type}" STREQUAL "Release"
           OR NOT "${deployment}" STREQUAL "12.0"
           OR NOT tests OR NOT warnings OR NOT tidy OR NOT lto)
            message(FATAL_ERROR
                "Incomplete macOS gate options in macos-clang-base")
        endif()
    elseif("${name}" MATCHES "^macos-clang-(release|static)-")
        string(JSON inherited GET "${presets_json}"
            configurePresets ${index} inherits)
        string(JSON qt_prefix GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_PREFIX_PATH)
        string(JSON gate_qt_prefix GET "${presets_json}"
            configurePresets ${index} cacheVariables ZZ_QT_PREFIX)
        string(JSON architecture GET "${presets_json}"
            configurePresets ${index} cacheVariables CMAKE_OSX_ARCHITECTURES)
        string(JSON shared GET "${presets_json}"
            configurePresets ${index} cacheVariables BUILD_SHARED_LIBS)
        if(NOT "${inherited}" STREQUAL "macos-clang-base"
           OR NOT "${qt_prefix}" STREQUAL "${gate_qt_prefix}"
           OR NOT "${architecture}" MATCHES "^(arm64|x86_64)$")
            message(FATAL_ERROR "Invalid macOS identity fields in ${name}")
        endif()
        if("${name}" MATCHES "-release-" AND NOT shared)
            message(FATAL_ERROR "Invalid macOS linkage mode in ${name}")
        elseif("${name}" MATCHES "-static-" AND shared)
            message(FATAL_ERROR "Invalid macOS linkage mode in ${name}")
        endif()
    endif()
endforeach()
if(NOT macos_base_seen)
    message(FATAL_ERROR "Missing configure preset: macos-clang-base")
endif()

message(STATUS "CMake preset matrix contract passed")
