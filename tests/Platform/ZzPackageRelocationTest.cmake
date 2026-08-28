cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_SOURCE_DIR ZZ_TEST_ROOT ZZ_CONTEXT_FILE ZZ_CONFIG)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()
if(NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "Source directory does not exist: ${ZZ_SOURCE_DIR}")
endif()
if(NOT EXISTS "${ZZ_CONTEXT_FILE}")
    message(FATAL_ERROR "Context file does not exist: ${ZZ_CONTEXT_FILE}")
endif()
include("${ZZ_CONTEXT_FILE}")

cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE source_dir)
cmake_path(ABSOLUTE_PATH ZZ_TEST_ROOT NORMALIZE OUTPUT_VARIABLE test_root)
cmake_path(GET test_root ROOT_PATH test_root_anchor)
cmake_path(IS_PREFIX ZZ_PRIMARY_BINARY_DIR "${test_root}"
    NORMALIZE test_is_below_build)
if("${test_root}" STREQUAL "${test_root_anchor}"
   OR "${test_root}" STREQUAL "${source_dir}"
   OR NOT test_is_below_build)
    message(FATAL_ERROR "Unsafe relocation test root: ${test_root}")
endif()
if("${ZZ_GENERATOR}" STREQUAL ""
   OR "${ZZ_CXX_COMPILER}" STREQUAL ""
   OR "${ZZ_QT_PREFIX}" STREQUAL ""
   OR "${ZZ_CTEST_COMMAND}" STREQUAL "")
    message(FATAL_ERROR "Relocation context is incomplete")
endif()
if(NOT "${ZZ_CONFIGURATION_TYPES}" STREQUAL ""
   AND "${ZZ_CONFIG}" STREQUAL "")
    message(FATAL_ERROR "Multi-config generators require ZZ_CONFIG")
endif()
set(ZZ_TEST_ROOT "${test_root}")

function(zz_run label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr)
    if(NOT "${result}" EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with exit code ${result}\n"
            "stdout:\n${stdout}\n"
            "stderr:\n${stderr}")
    endif()
    message(STATUS "${label} passed")
endfunction()

set(producer "${ZZ_TEST_ROOT}/producer")
set(prefix_a "${ZZ_TEST_ROOT}/prefix-a")
set(prefix_b "${ZZ_TEST_ROOT}/prefix-b")
set(install_consumer "${ZZ_TEST_ROOT}/install-consumer")
set(header_consumer "${ZZ_TEST_ROOT}/public-header-consumer")
file(REMOVE_RECURSE
    "${producer}" "${prefix_a}" "${prefix_b}"
    "${install_consumer}" "${header_consumer}")

set(generator_args -G "${ZZ_GENERATOR}")
if(NOT "${ZZ_GENERATOR_PLATFORM}" STREQUAL "")
    list(APPEND generator_args -A "${ZZ_GENERATOR_PLATFORM}")
endif()
if(NOT "${ZZ_GENERATOR_TOOLSET}" STREQUAL "")
    list(APPEND generator_args -T "${ZZ_GENERATOR_TOOLSET}")
endif()

set(toolchain_args
    "-DCMAKE_CXX_COMPILER:FILEPATH=${ZZ_CXX_COMPILER}")
if(NOT "${ZZ_C_COMPILER}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_C_COMPILER:FILEPATH=${ZZ_C_COMPILER}")
endif()
if(NOT "${ZZ_GENERATOR_INSTANCE}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_GENERATOR_INSTANCE:PATH=${ZZ_GENERATOR_INSTANCE}")
endif()
if("${ZZ_GENERATOR}" MATCHES "Ninja"
   AND NOT "${ZZ_MAKE_PROGRAM}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=${ZZ_MAKE_PROGRAM}")
endif()
if(NOT "${ZZ_OBJCXX_COMPILER}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_OBJCXX_COMPILER:FILEPATH=${ZZ_OBJCXX_COMPILER}")
endif()
if(NOT "${ZZ_BUILD_TYPE}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_BUILD_TYPE:STRING=${ZZ_BUILD_TYPE}")
endif()
if(NOT "${ZZ_OSX_ARCHITECTURES}" STREQUAL "")
    string(REPLACE ";" "\\;" osx_architectures
        "${ZZ_OSX_ARCHITECTURES}")
    list(APPEND toolchain_args
        "-DCMAKE_OSX_ARCHITECTURES:STRING=${osx_architectures}")
endif()
if(NOT "${ZZ_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${ZZ_OSX_DEPLOYMENT_TARGET}")
endif()
if(NOT "${ZZ_OSX_SYSROOT}" STREQUAL "")
    list(APPEND toolchain_args
        "-DCMAKE_OSX_SYSROOT:PATH=${ZZ_OSX_SYSROOT}")
endif()
if(NOT "${ZZ_XKB_INCLUDE_DIR}" STREQUAL "")
    list(APPEND toolchain_args
        "-DXKB_INCLUDE_DIR:PATH=${ZZ_XKB_INCLUDE_DIR}")
endif()
if(NOT "${ZZ_XKB_LIBRARY}" STREQUAL "")
    list(APPEND toolchain_args
        "-DXKB_LIBRARY:FILEPATH=${ZZ_XKB_LIBRARY}")
endif()

set(build_config_args)
set(ctest_config_args)
set(zz_nested_build_parallelism 2)
if(NOT "${ZZ_CONFIG}" STREQUAL "")
    list(APPEND build_config_args --config "${ZZ_CONFIG}")
    list(APPEND ctest_config_args -C "${ZZ_CONFIG}")
endif()
string(REPLACE ";" "\\;" consumer_prefix
    "${prefix_b};${ZZ_QT_PREFIX}")

zz_run("producer configure" "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}" -B "${producer}" ${generator_args}
    ${toolchain_args}
    "-DCMAKE_PREFIX_PATH:PATH=${ZZ_QT_PREFIX}"
    "-DZZ_QT_PREFIX:PATH=${ZZ_QT_PREFIX}"
    "-DCMAKE_INSTALL_PREFIX:PATH=${prefix_a}"
    "-DBUILD_SHARED_LIBS:BOOL=${ZZ_BUILD_SHARED}"
    "-DZZ_ENABLE_LTO:BOOL=${ZZ_ENABLE_LTO}"
    -DZZ_BUILD_TESTS=OFF -DZZ_BUILD_EXAMPLES=OFF
    -DZZ_BUILD_BENCHMARKS=OFF -DZZ_WARNINGS_AS_ERRORS=ON)
zz_run("producer build" "${CMAKE_COMMAND}"
    --build "${producer}" ${build_config_args}
    --parallel "${zz_nested_build_parallelism}")
zz_run("producer install" "${CMAKE_COMMAND}"
    --install "${producer}" --prefix "${prefix_a}" ${build_config_args})

file(MAKE_DIRECTORY "${prefix_b}")
file(COPY "${prefix_a}/" DESTINATION "${prefix_b}")
file(REMOVE_RECURSE "${prefix_a}")
if(EXISTS "${prefix_a}")
    message(FATAL_ERROR "prefix A still exists after relocation")
endif()
file(GLOB_RECURSE package_configs LIST_DIRECTORIES FALSE
    "${prefix_b}/*/cmake/ZzPureToolsFrame/ZzPureToolsFrameConfig.cmake")
list(LENGTH package_configs package_config_count)
if(NOT "${package_config_count}" EQUAL 1)
    message(FATAL_ERROR "prefix B must contain exactly one package Config")
endif()
list(GET package_configs 0 package_config)
cmake_path(GET package_config PARENT_PATH package_config_dir)

zz_run("install consumer configure" "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}/tests/InstallConsumer"
    -B "${install_consumer}" ${generator_args} ${toolchain_args}
    "-DCMAKE_PREFIX_PATH:STRING=${consumer_prefix}"
    "-DZzPureToolsFrame_DIR:PATH=${package_config_dir}"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
    -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    "-DZZ_PACKAGE_ROOT:PATH=${prefix_b}")
zz_run("install consumer build" "${CMAKE_COMMAND}"
    --build "${install_consumer}" ${build_config_args}
    --parallel "${zz_nested_build_parallelism}")
zz_run("install consumer test" "${ZZ_CTEST_COMMAND}"
    --test-dir "${install_consumer}" ${ctest_config_args}
    --output-on-failure)

zz_run("public header consumer configure" "${CMAKE_COMMAND}"
    -S "${ZZ_SOURCE_DIR}/tests/PublicHeaderConsumer"
    -B "${header_consumer}" ${generator_args} ${toolchain_args}
    "-DCMAKE_PREFIX_PATH:STRING=${consumer_prefix}"
    "-DZzPureToolsFrame_DIR:PATH=${package_config_dir}"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
    -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
    "-DZZ_PACKAGE_ROOT:PATH=${prefix_b}")
zz_run("public header consumer build" "${CMAKE_COMMAND}"
    --build "${header_consumer}" ${build_config_args}
    --target ZzInstalledPublicHeaders
    --parallel "${zz_nested_build_parallelism}")

foreach(consumer_dir IN ITEMS "${install_consumer}" "${header_consumer}")
    file(STRINGS "${consumer_dir}/CMakeCache.txt" package_dir_entries
        REGEX "^ZzPureToolsFrame_DIR:[^=]*=")
    list(LENGTH package_dir_entries package_dir_entry_count)
    if(NOT "${package_dir_entry_count}" EQUAL 1)
        message(FATAL_ERROR
            "${consumer_dir} must contain exactly one ZzPureToolsFrame_DIR")
    endif()
    list(GET package_dir_entries 0 package_dir_entry)
    string(REGEX REPLACE "^ZzPureToolsFrame_DIR:[^=]*=" ""
        resolved_package_dir "${package_dir_entry}")
    file(TO_CMAKE_PATH "${package_config_dir}" expected_package_dir)
    file(TO_CMAKE_PATH "${resolved_package_dir}" resolved_package_dir)
    if(NOT "${resolved_package_dir}" STREQUAL "${expected_package_dir}")
        message(FATAL_ERROR
            "${consumer_dir} did not resolve the relocated package: "
            "${resolved_package_dir}")
    endif()
endforeach()

file(GLOB_RECURSE installed_cmake_files LIST_DIRECTORIES FALSE
    "${prefix_b}/*.cmake")
if(NOT installed_cmake_files)
    message(FATAL_ERROR "Relocated prefix contains no installed CMake files")
endif()
set(forbidden_paths
    "${source_dir}"
    "${ZZ_PRIMARY_BINARY_DIR}"
    "${producer}"
    "${prefix_a}"
    "${prefix_b}"
    "${ZZ_QT_PREFIX}")
foreach(cmake_file IN LISTS installed_cmake_files)
    file(READ "${cmake_file}" cmake_text)
    string(REPLACE "\\" "/" normalized_text "${cmake_text}")
    foreach(forbidden_path IN LISTS forbidden_paths)
        file(TO_CMAKE_PATH "${forbidden_path}" normalized_forbidden)
        string(FIND "${normalized_text}"
            "${normalized_forbidden}" forbidden_position)
        if(NOT "${forbidden_position}" EQUAL -1)
            message(FATAL_ERROR
                "Absolute path leaked into ${cmake_file}: ${normalized_forbidden}")
        endif()
    endforeach()
    string(CONCAT zz_unix_home_pattern "/ho" "me/|/Us" "ers/")
    if("${normalized_text}" MATCHES
       "(${zz_unix_home_pattern}|[A-Za-z]:/[^;$<\\\"]*)")
        message(FATAL_ERROR
            "Developer absolute path leaked into ${cmake_file}")
    endif()
endforeach()

message(STATUS
    "Package relocation and installed public-header checks passed")
