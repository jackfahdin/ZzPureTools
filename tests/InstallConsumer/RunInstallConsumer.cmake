cmake_minimum_required(VERSION 3.23)

foreach(zz_required IN ITEMS
    ZZ_SOURCE_DIR
    ZZ_TEST_ROOT
    ZZ_CONTEXT_FILE
    ZZ_CONFIG
)
    if(NOT DEFINED ${zz_required})
        message(FATAL_ERROR "${zz_required} is required")
    endif()
endforeach()
if(NOT EXISTS "${ZZ_CONTEXT_FILE}")
    message(FATAL_ERROR
        "install consumer context does not exist: ${ZZ_CONTEXT_FILE}")
endif()

include("${ZZ_CONTEXT_FILE}")

if(NOT ZZ_PRIMARY_BINARY_DIR)
    message(FATAL_ERROR "primary binary directory was not captured")
endif()
file(TO_CMAKE_PATH "${ZZ_TEST_ROOT}" zz_test_root_normalized)
file(TO_CMAKE_PATH "${ZZ_PRIMARY_BINARY_DIR}" zz_primary_binary_normalized)
string(FIND "${zz_test_root_normalized}"
    "${zz_primary_binary_normalized}/" zz_test_root_prefix)
if(ZZ_TEST_ROOT STREQUAL ""
   OR ZZ_TEST_ROOT STREQUAL "/"
   OR ZZ_TEST_ROOT STREQUAL ZZ_SOURCE_DIR
   OR NOT zz_test_root_prefix EQUAL 0)
    message(FATAL_ERROR "unsafe install consumer test root: ${ZZ_TEST_ROOT}")
endif()
if(NOT ZZ_GENERATOR)
    message(FATAL_ERROR "generator was not captured from the parent build")
endif()
if(NOT ZZ_CXX_COMPILER)
    message(FATAL_ERROR "C++ compiler was not captured from the parent build")
endif()
if(NOT ZZ_CTEST_COMMAND)
    message(FATAL_ERROR "CTest executable was not captured from the parent build")
endif()
if(NOT ZZ_CONFIGURATION_TYPES STREQUAL "" AND ZZ_CONFIG STREQUAL "")
    message(FATAL_ERROR
        "a multi-config generator requires a concrete test configuration")
endif()

set(zz_a_dir "${ZZ_TEST_ROOT}/A")
set(zz_b_dir "${ZZ_TEST_ROOT}/B")
set(zz_consumer_dir "${ZZ_TEST_ROOT}/consumer")
file(REMOVE_RECURSE
    "${zz_a_dir}"
    "${zz_b_dir}"
    "${zz_consumer_dir}"
)
file(MAKE_DIRECTORY "${ZZ_TEST_ROOT}")

function(zz_run_process zz_step)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE zz_result
        OUTPUT_VARIABLE zz_stdout
        ERROR_VARIABLE zz_stderr
    )
    if(NOT zz_result EQUAL 0)
        message(FATAL_ERROR
            "${zz_step} failed with exit code ${zz_result}\n"
            "stdout:\n${zz_stdout}\n"
            "stderr:\n${zz_stderr}")
    endif()
    message(STATUS "${zz_step} passed")
endfunction()

set(zz_generator_args -G "${ZZ_GENERATOR}")
if(NOT ZZ_GENERATOR_PLATFORM STREQUAL "")
    list(APPEND zz_generator_args -A "${ZZ_GENERATOR_PLATFORM}")
endif()
if(NOT ZZ_GENERATOR_TOOLSET STREQUAL "")
    list(APPEND zz_generator_args -T "${ZZ_GENERATOR_TOOLSET}")
endif()

set(zz_common_cache_args
    "-DCMAKE_CXX_COMPILER:FILEPATH=${ZZ_CXX_COMPILER}"
)
if(NOT ZZ_BUILD_TYPE STREQUAL "")
    list(APPEND zz_common_cache_args
        "-DCMAKE_BUILD_TYPE:STRING=${ZZ_BUILD_TYPE}")
endif()
if(NOT ZZ_CMAKE_PREFIX_PATH STREQUAL "")
    string(REPLACE ";" "\\;" zz_prefix_path_escaped
        "${ZZ_CMAKE_PREFIX_PATH}")
    list(APPEND zz_common_cache_args
        "-DCMAKE_PREFIX_PATH:STRING=${zz_prefix_path_escaped}")
endif()
if(NOT ZZ_OSX_ARCHITECTURES STREQUAL "")
    string(REPLACE ";" "\\;" zz_osx_architectures_escaped
        "${ZZ_OSX_ARCHITECTURES}")
    list(APPEND zz_common_cache_args
        "-DCMAKE_OSX_ARCHITECTURES:STRING=${zz_osx_architectures_escaped}")
endif()
if(NOT ZZ_OSX_DEPLOYMENT_TARGET STREQUAL "")
    list(APPEND zz_common_cache_args
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${ZZ_OSX_DEPLOYMENT_TARGET}")
endif()
if(NOT ZZ_OSX_SYSROOT STREQUAL "")
    list(APPEND zz_common_cache_args
        "-DCMAKE_OSX_SYSROOT:PATH=${ZZ_OSX_SYSROOT}")
endif()

set(zz_config_args)
set(zz_ctest_config_args)
if(NOT ZZ_CONFIG STREQUAL "")
    list(APPEND zz_config_args --config "${ZZ_CONFIG}")
    list(APPEND zz_ctest_config_args -C "${ZZ_CONFIG}")
endif()

zz_run_process("fresh producer configure"
    "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}"
    -B "${zz_a_dir}"
    ${zz_generator_args}
    ${zz_common_cache_args}
    "-DBUILD_SHARED_LIBS:BOOL=${ZZ_BUILD_SHARED}"
    "-DZZ_BUILD_TESTS:BOOL=OFF"
    "-DZZ_BUILD_EXAMPLES:BOOL=OFF"
    "-DZZ_BUILD_BENCHMARKS:BOOL=OFF"
    "-DZZ_WARNINGS_AS_ERRORS:BOOL=ON"
    "-DZZ_ENABLE_CLANG_TIDY:BOOL=OFF"
    "-DZZ_ENABLE_ASAN:BOOL=OFF"
    "-DZZ_ENABLE_UBSAN:BOOL=OFF"
    "-DZZ_ENABLE_LTO:BOOL=${ZZ_ENABLE_LTO}"
)

zz_run_process("fresh producer build"
    "${CMAKE_COMMAND}" --build "${zz_a_dir}" ${zz_config_args})

zz_run_process("fresh producer install"
    "${CMAKE_COMMAND}" --install "${zz_a_dir}"
    --prefix "${zz_b_dir}" ${zz_config_args})

file(GLOB_RECURSE zz_installed_configs
    LIST_DIRECTORIES FALSE
    "${zz_b_dir}/ZzPureToolsProConfig.cmake"
)
list(LENGTH zz_installed_configs zz_config_count)
if(NOT zz_config_count EQUAL 1)
    message(FATAL_ERROR
        "expected one installed Config file in B, found ${zz_config_count}")
endif()

set(zz_consumer_prefix_path "${zz_b_dir}")
if(NOT ZZ_CMAKE_PREFIX_PATH STREQUAL "")
    list(APPEND zz_consumer_prefix_path ${ZZ_CMAKE_PREFIX_PATH})
endif()
string(REPLACE ";" "\\;" zz_consumer_prefix_escaped
    "${zz_consumer_prefix_path}")

set(zz_consumer_cache_args ${zz_common_cache_args})
list(FILTER zz_consumer_cache_args EXCLUDE
    REGEX "^-DCMAKE_PREFIX_PATH:")
list(APPEND zz_consumer_cache_args
    "-DCMAKE_PREFIX_PATH:STRING=${zz_consumer_prefix_escaped}"
    "-DZZ_PACKAGE_ROOT:PATH=${zz_b_dir}"
)

zz_run_process("fresh consumer configure"
    "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}/tests/InstallConsumer"
    -B "${zz_consumer_dir}"
    ${zz_generator_args}
    ${zz_consumer_cache_args}
)

zz_run_process("fresh consumer build"
    "${CMAKE_COMMAND}" --build "${zz_consumer_dir}" ${zz_config_args})

zz_run_process("fresh consumer test"
    "${ZZ_CTEST_COMMAND}"
    --test-dir "${zz_consumer_dir}"
    ${zz_ctest_config_args}
    --output-on-failure
)

file(GLOB_RECURSE zz_installed_cmake_files
    LIST_DIRECTORIES FALSE
    "${zz_b_dir}/*.cmake"
)
set(zz_forbidden_paths
    "${ZZ_SOURCE_DIR}"
    "${ZZ_PRIMARY_BINARY_DIR}"
    "${zz_a_dir}"
)
if(NOT ZZ_CMAKE_PREFIX_PATH STREQUAL "")
    list(APPEND zz_forbidden_paths ${ZZ_CMAKE_PREFIX_PATH})
endif()

foreach(zz_cmake_file IN LISTS zz_installed_cmake_files)
    file(READ "${zz_cmake_file}" zz_cmake_content)
    foreach(zz_forbidden_path IN LISTS zz_forbidden_paths)
        if(zz_forbidden_path STREQUAL "")
            continue()
        endif()
        file(TO_CMAKE_PATH "${zz_forbidden_path}" zz_forbidden_normalized)
        string(FIND "${zz_cmake_content}"
            "${zz_forbidden_normalized}" zz_forbidden_position)
        if(NOT zz_forbidden_position EQUAL -1)
            message(FATAL_ERROR
                "installed CMake file leaks ${zz_forbidden_normalized}: ${zz_cmake_file}")
        endif()
    endforeach()
endforeach()

message(STATUS
    "fresh A/B/consumer install test passed for BUILD_SHARED_LIBS=${ZZ_BUILD_SHARED}")
