cmake_minimum_required(VERSION 3.23)

foreach(required_variable IN ITEMS ZZ_MSVC_REDIST_DIR ZZ_STAGE_ROOT)
    if(NOT DEFINED ${required_variable}
       OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH ZZ_MSVC_REDIST_DIR
    NORMALIZE OUTPUT_VARIABLE redist_root_input)
cmake_path(ABSOLUTE_PATH ZZ_STAGE_ROOT
    NORMALIZE OUTPUT_VARIABLE stage_root)
if(NOT IS_DIRECTORY "${redist_root_input}"
   OR IS_SYMLINK "${redist_root_input}")
    message(FATAL_ERROR
        "ZZ_MSVC_REDIST_DIR must identify a regular directory")
endif()
if(NOT IS_DIRECTORY "${stage_root}" OR IS_SYMLINK "${stage_root}")
    message(FATAL_ERROR "ZZ_STAGE_ROOT must identify a regular directory")
endif()
file(REAL_PATH "${redist_root_input}" redist_root)

set(stage_bin "${stage_root}/bin")
if(NOT IS_DIRECTORY "${stage_bin}" OR IS_SYMLINK "${stage_bin}")
    message(FATAL_ERROR "Deployment stage bin directory is unavailable")
endif()

# Visual Studio 2022 的版本化 Redist 根目录应只提供一个 x64 VC CRT 集合。
file(GLOB crt_candidates LIST_DIRECTORIES true
    "${redist_root}/x64/Microsoft.VC*.CRT")
set(crt_directories)
foreach(crt_candidate IN LISTS crt_candidates)
    if(IS_DIRECTORY "${crt_candidate}" AND NOT IS_SYMLINK "${crt_candidate}")
        list(APPEND crt_directories "${crt_candidate}")
    endif()
endforeach()
list(LENGTH crt_directories crt_directory_count)
if(NOT crt_directory_count EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one x64 Microsoft VC CRT directory; found ${crt_directory_count}")
endif()
list(GET crt_directories 0 crt_directory)

file(GLOB runtime_dlls LIST_DIRECTORIES false "${crt_directory}/*.dll")
if(NOT runtime_dlls)
    message(FATAL_ERROR "MSVC CRT directory contains no runtime DLLs")
endif()
list(SORT runtime_dlls)

set(has_msvcp FALSE)
set(has_vcruntime FALSE)
foreach(runtime_dll IN LISTS runtime_dlls)
    if(IS_SYMLINK "${runtime_dll}")
        message(FATAL_ERROR "MSVC runtime input must not be a symlink: ${runtime_dll}")
    endif()
    file(SIZE "${runtime_dll}" runtime_size)
    if(runtime_size EQUAL 0)
        message(FATAL_ERROR "MSVC runtime input is empty: ${runtime_dll}")
    endif()
    get_filename_component(runtime_name "${runtime_dll}" NAME)
    string(TOLOWER "${runtime_name}" runtime_name_lower)
    if(runtime_name_lower MATCHES "^msvcp.*\\.dll$")
        set(has_msvcp TRUE)
    elseif(runtime_name_lower MATCHES "^vcruntime.*\\.dll$")
        set(has_vcruntime TRUE)
    endif()
endforeach()
if(NOT has_msvcp OR NOT has_vcruntime)
    message(FATAL_ERROR
        "MSVC CRT directory must contain both msvcp and vcruntime DLLs")
endif()

# 复制整个官方 CRT 集合，避免只补首个依赖而遗漏延迟加载的配套运行库。
foreach(runtime_dll IN LISTS runtime_dlls)
    get_filename_component(runtime_name "${runtime_dll}" NAME)
    set(destination "${stage_bin}/${runtime_name}")
    if(EXISTS "${destination}" OR IS_SYMLINK "${destination}")
        message(FATAL_ERROR
            "MSVC runtime destination already exists: ${destination}")
    endif()
    configure_file("${runtime_dll}" "${destination}" COPYONLY)
endforeach()

message(STATUS "Staged app-local MSVC runtime from ${crt_directory}")
