cmake_minimum_required(VERSION 3.23)

foreach(zz_required IN ITEMS
    ZZ_SOURCE_DIR
    ZZ_QT_PREFIX
    ZZ_REJECTED_CXX
    ZZ_WORK_DIR
)
    if(NOT DEFINED ${zz_required} OR "${${zz_required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${zz_required}=...")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR
    NORMALIZE OUTPUT_VARIABLE zz_source_dir)
cmake_path(ABSOLUTE_PATH ZZ_WORK_DIR
    NORMALIZE OUTPUT_VARIABLE zz_work_dir)
cmake_path(IS_PREFIX zz_source_dir "${zz_work_dir}"
    NORMALIZE zz_work_is_in_source)
if(zz_work_is_in_source)
    message(FATAL_ERROR "ZZ_WORK_DIR must not be inside the source tree")
endif()
file(REMOVE_RECURSE "${zz_work_dir}")

set(zz_optional_dependency_arguments)
foreach(zz_optional IN ITEMS ZZ_XKB_INCLUDE_DIR ZZ_XKB_LIBRARY)
    if(DEFINED ${zz_optional} AND NOT "${${zz_optional}}" STREQUAL "")
        string(REPLACE "ZZ_" "" zz_cache_name "${zz_optional}")
        list(APPEND zz_optional_dependency_arguments
            "-D${zz_cache_name}:PATH=${${zz_optional}}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${zz_source_dir}"
        -B "${zz_work_dir}"
        -G Ninja
        "-DCMAKE_CXX_COMPILER=${ZZ_REJECTED_CXX}"
        "-DCMAKE_PREFIX_PATH=${ZZ_QT_PREFIX}"
        -DZZ_BUILD_TESTS=OFF
        ${zz_optional_dependency_arguments}
    RESULT_VARIABLE zz_configure_result
    OUTPUT_VARIABLE zz_configure_stdout
    ERROR_VARIABLE zz_configure_stderr
)
set(zz_configure_output
    "${zz_configure_stdout}\n${zz_configure_stderr}")
if(zz_configure_result EQUAL 0)
    message(FATAL_ERROR "Unsupported compiler was accepted")
endif()
if(NOT zz_configure_output MATCHES "requires GCC 13\\.1 or newer")
    message(FATAL_ERROR
        "Configuration failed for the wrong reason:\n${zz_configure_output}")
endif()

message(STATUS "Unsupported GCC was rejected by the explicit version gate")
