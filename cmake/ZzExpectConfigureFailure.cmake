cmake_minimum_required(VERSION 3.23)

foreach(required IN ITEMS
    ZZ_SOURCE_DIR
    ZZ_WORK_DIR
    ZZ_QT_PREFIX
    ZZ_C_COMPILER
    ZZ_CXX_COMPILER
    ZZ_EXPECTED_BLOCKERS)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "缺少 -D${required}=...")
    endif()
endforeach()
if(NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "源码目录不存在：${ZZ_SOURCE_DIR}")
endif()

cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE source_dir)
cmake_path(ABSOLUTE_PATH ZZ_WORK_DIR NORMALIZE OUTPUT_VARIABLE work_dir)
cmake_path(GET work_dir ROOT_PATH work_anchor)
cmake_path(IS_PREFIX work_dir "${source_dir}" NORMALIZE work_contains_source)
if("${work_dir}" STREQUAL "${work_anchor}"
   OR "${work_dir}" STREQUAL "${source_dir}"
   OR work_contains_source)
    message(FATAL_ERROR "拒绝不安全的发布配置测试目录：${work_dir}")
endif()

file(REMOVE_RECURSE "${work_dir}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${source_dir}"
        -B "${work_dir}"
        -G Ninja
        "-DCMAKE_C_COMPILER=${ZZ_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${ZZ_CXX_COMPILER}"
        "-DCMAKE_PREFIX_PATH=${ZZ_QT_PREFIX}"
        "-DZZ_QT_PREFIX=${ZZ_QT_PREFIX}"
        -DZZ_RELEASE_BUILD=ON
        "-DZZ_RELEASE_FORCED_BLOCKERS=${ZZ_EXPECTED_BLOCKERS}"
        -DZZ_BUILD_TESTS=OFF
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
set(output "${stdout}\n${stderr}")
if("${result}" EQUAL 0)
    message(FATAL_ERROR "正式发布配置意外成功")
endif()
foreach(blocker IN LISTS ZZ_EXPECTED_BLOCKERS)
    string(FIND "${output}" "${blocker}" position)
    if("${position}" EQUAL -1)
        message(FATAL_ERROR
            "发布失败输出中缺少阻塞项 ${blocker}：\n${output}")
    endif()
endforeach()
message(STATUS "正式发布配置已按声明的全部原因阻断")
