cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_CONTEXT_FILE OR "${ZZ_CONTEXT_FILE}" STREQUAL "")
    message(FATAL_ERROR "ZZ_CONTEXT_FILE is required")
endif()
if(NOT EXISTS "${ZZ_CONTEXT_FILE}")
    message(FATAL_ERROR "Binary context does not exist: ${ZZ_CONTEXT_FILE}")
endif()
include("${ZZ_CONTEXT_FILE}")

foreach(required
    ZZ_SYSTEM_NAME
    ZZ_COMPILER_ID
    ZZ_BUILD_SHARED
    ZZ_SOURCE_DIR
    ZZ_BINARY_DIR
    ZZ_PLATFORM_EXECUTABLE
    ZZ_CORE_FILE
    ZZ_WINDOWKIT_FILE
    ZZ_FLUENT_FOUNDATION_FILE
    ZZ_FLUENT_UI_FILE
    ZZ_APP_CORE_FILE
    ZZ_PURE_TOOLS_FILE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Binary context is missing ${required}")
    endif()
endforeach()

set(zz_library_files
    "${ZZ_CORE_FILE}"
    "${ZZ_WINDOWKIT_FILE}"
    "${ZZ_FLUENT_FOUNDATION_FILE}"
    "${ZZ_FLUENT_UI_FILE}"
    "${ZZ_APP_CORE_FILE}"
    "${ZZ_PURE_TOOLS_FILE}")
foreach(binary IN ITEMS "${ZZ_PLATFORM_EXECUTABLE}" ${zz_library_files})
    if(NOT EXISTS "${binary}" OR IS_DIRECTORY "${binary}")
        message(FATAL_ERROR "Captured target file is absent: ${binary}")
    endif()
endforeach()

if(ZZ_BUILD_SHARED)
    set(scan_files "${ZZ_PLATFORM_EXECUTABLE}" ${zz_library_files})
    foreach(library IN LISTS zz_library_files)
        if(ZZ_SYSTEM_NAME STREQUAL "Linux"
           AND NOT "${library}" MATCHES "\\.so(\\.[0-9]+)*$")
            message(FATAL_ERROR "Expected a Linux shared library: ${library}")
        elseif(ZZ_SYSTEM_NAME STREQUAL "Windows"
               AND NOT "${library}" MATCHES "\\.[dD][lL][lL]$")
            message(FATAL_ERROR "Expected a Windows DLL: ${library}")
        elseif(ZZ_SYSTEM_NAME STREQUAL "Darwin"
               AND NOT "${library}" MATCHES "\\.dylib$")
            message(FATAL_ERROR "Expected a macOS dylib: ${library}")
        endif()
    endforeach()
else()
    set(scan_files "${ZZ_PLATFORM_EXECUTABLE}")
    foreach(library IN LISTS zz_library_files)
        if(ZZ_SYSTEM_NAME STREQUAL "Windows")
            if(NOT "${library}" MATCHES "\\.(lib|a)$")
                message(FATAL_ERROR "Expected a Windows static archive: ${library}")
            endif()
        elseif(NOT "${library}" MATCHES "\\.a$")
            message(FATAL_ERROR "Expected a static archive: ${library}")
        endif()
    endforeach()
endif()
if(NOT scan_files)
    message(FATAL_ERROR "Binary scan set is empty")
endif()

function(zz_run_tool output label)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${label} failed with exit code ${result}\n"
            "stdout:\n${stdout}\nstderr:\n${stderr}")
    endif()
    set(${output} "${stdout}\n${stderr}" PARENT_SCOPE)
endfunction()

function(zz_reject_path_leaks text binary)
    string(REPLACE "\\" "/" normalized_text "${text}")
    foreach(path IN ITEMS "${ZZ_SOURCE_DIR}" "${ZZ_BINARY_DIR}")
        file(TO_CMAKE_PATH "${path}" normalized_path)
        string(FIND "${normalized_text}" "${normalized_path}" position)
        if(NOT position EQUAL -1)
            message(FATAL_ERROR
                "Build/source path leaked from ${binary}: ${normalized_path}")
        endif()
    endforeach()
endfunction()

function(zz_check_windows_dependency dependency mode binary)
    string(TOLOWER "${dependency}" name)
    if(name MATCHES "qwindowkit")
        message(FATAL_ERROR "QWindowKit leaked into ${binary}: ${dependency}")
    endif()
    set(system_pattern
        "^(kernel32|user32|gdi32|shell32|ole32|oleaut32|advapi32|comdlg32|comctl32|dwmapi|uxtheme|shlwapi|imm32|winmm|ws2_32|version|bcrypt|setupapi|userenv|authz|ntdll|msvcrt)\\.dll$")
    if(name MATCHES "^zz[a-z0-9_]*\\.dll$"
       OR name MATCHES "^qt6[a-z0-9_]*\\.dll$"
       OR name MATCHES "^api-ms-win-[a-z0-9_.-]+\\.dll$"
       OR name MATCHES "${system_pattern}")
        return()
    endif()
    if(mode STREQUAL "MSVC")
        if(name MATCHES "^(vcruntime140[a-z0-9_]*|msvcp140[a-z0-9_]*|ucrtbase)\\.dll$")
            return()
        endif()
        if(name MATCHES "(libgcc|libstdc\\+\\+|libwinpthread|mingw)")
            message(FATAL_ERROR "MinGW runtime leaked into MSVC ${binary}: ${dependency}")
        endif()
    elseif(mode STREQUAL "MinGW")
        if(name MATCHES "^(libgcc_s_seh-1|libstdc\\+\\+-6|libwinpthread-1)\\.dll$")
            return()
        endif()
        if(name MATCHES "(vcruntime|msvcp)")
            message(FATAL_ERROR "MSVC runtime leaked into MinGW ${binary}: ${dependency}")
        endif()
    endif()
    message(FATAL_ERROR "Unexpected ${mode} dependency in ${binary}: ${dependency}")
endfunction()

if(ZZ_SYSTEM_NAME STREQUAL "Linux")
    foreach(tool_variable ZZ_READELF ZZ_LDD)
        if(NOT DEFINED ${tool_variable}
           OR NOT EXISTS "${${tool_variable}}"
           OR IS_DIRECTORY "${${tool_variable}}")
            message(FATAL_ERROR "Linux tool is unavailable: ${tool_variable}")
        endif()
    endforeach()
    foreach(binary IN LISTS scan_files)
        zz_run_tool(header "readelf header for ${binary}"
            "${ZZ_READELF}" -h "${binary}")
        if(NOT header MATCHES "Class:[ \\t]+ELF64")
            message(FATAL_ERROR "Expected ELF64 binary: ${binary}")
        endif()
        zz_run_tool(dynamic "readelf dynamic section for ${binary}"
            "${ZZ_READELF}" -d "${binary}")
        zz_reject_path_leaks("${dynamic}" "${binary}")
        string(REGEX MATCHALL
            "Shared library: \\[[^]]+\\]" dependency_lines "${dynamic}")
        foreach(line IN LISTS dependency_lines)
            string(REGEX REPLACE ".*\\[([^]]+)\\].*" "\\1" dependency "${line}")
            string(TOLOWER "${dependency}" dependency_lower)
            if(dependency_lower MATCHES "qwindowkit")
                message(FATAL_ERROR
                    "QWindowKit leaked into ${binary}: ${dependency}")
            endif()
            if(NOT dependency MATCHES
               "^(libZz[A-Za-z0-9_]*\\.so(\\.[0-9]+)*|libQt6(Core|Gui|Widgets|Svg|Concurrent)\\.so(\\.[0-9]+)*|lib(GLX|OpenGL)\\.so(\\.[0-9]+)*|libstdc\\+\\+\\.so(\\.[0-9]+)*|libgcc_s\\.so(\\.[0-9]+)*|libc\\.so(\\.[0-9]+)*|libm\\.so(\\.[0-9]+)*|libpthread\\.so(\\.[0-9]+)*|libdl\\.so(\\.[0-9]+)*|librt\\.so(\\.[0-9]+)*|ld-linux[^/]*\\.so(\\.[0-9]+)*)$")
                if((ZZ_ENABLE_ASAN OR ZZ_ENABLE_UBSAN)
                   AND dependency MATCHES
                       "^(lib(asan|ubsan)\\.so(\\.[0-9]+)*|libclang_rt\\.[A-Za-z0-9_.-]+\\.so|libresolv\\.so(\\.[0-9]+)*)$")
                    continue()
                endif()
                message(FATAL_ERROR
                    "Unexpected Linux dependency in ${binary}: ${dependency}")
            endif()
        endforeach()
        zz_run_tool(ldd_output "ldd for ${binary}" "${ZZ_LDD}" "${binary}")
        if(ldd_output MATCHES "not found")
            message(FATAL_ERROR "Unresolved Linux dependency in ${binary}:\n${ldd_output}")
        endif()
    endforeach()
elseif(ZZ_SYSTEM_NAME STREQUAL "Windows" AND ZZ_COMPILER_ID STREQUAL "MSVC")
    if(NOT EXISTS "${ZZ_DUMPBIN}" OR IS_DIRECTORY "${ZZ_DUMPBIN}")
        message(FATAL_ERROR "ZZ_DUMPBIN must name the captured dumpbin.exe")
    endif()
    foreach(binary IN LISTS scan_files)
        zz_run_tool(headers "dumpbin headers for ${binary}"
            "${ZZ_DUMPBIN}" /nologo /headers "${binary}")
        if(NOT headers MATCHES "8664 machine \\(x64\\)")
            message(FATAL_ERROR "Expected an MSVC x64 binary: ${binary}")
        endif()
        zz_run_tool(dependents "dumpbin dependents for ${binary}"
            "${ZZ_DUMPBIN}" /nologo /dependents "${binary}")
        zz_reject_path_leaks("${dependents}" "${binary}")
        string(REGEX MATCHALL
            "[A-Za-z0-9_.+-]+\\.[dD][lL][lL]" dependencies "${dependents}")
        foreach(dependency IN LISTS dependencies)
            zz_check_windows_dependency("${dependency}" MSVC "${binary}")
        endforeach()
    endforeach()
elseif(ZZ_SYSTEM_NAME STREQUAL "Windows" AND ZZ_COMPILER_ID STREQUAL "GNU")
    foreach(tool_variable ZZ_CMAKE_OBJDUMP ZZ_MINGW_OBJDUMP)
        if(NOT EXISTS "${${tool_variable}}" OR IS_DIRECTORY "${${tool_variable}}")
            message(FATAL_ERROR "MinGW tool is unavailable: ${tool_variable}")
        endif()
        cmake_path(ABSOLUTE_PATH ${tool_variable} NORMALIZE
            OUTPUT_VARIABLE ${tool_variable}_NORMALIZED)
        string(TOLOWER "${${tool_variable}_NORMALIZED}"
            ${tool_variable}_NORMALIZED)
    endforeach()
    if(NOT "${ZZ_CMAKE_OBJDUMP_NORMALIZED}"
       STREQUAL "${ZZ_MINGW_OBJDUMP_NORMALIZED}")
        message(FATAL_ERROR
            "MinGW objdump mismatch: CMake=${ZZ_CMAKE_OBJDUMP_NORMALIZED}, "
            "kit=${ZZ_MINGW_OBJDUMP_NORMALIZED}")
    endif()
    foreach(binary IN LISTS scan_files)
        get_filename_component(binary_directory "${binary}" DIRECTORY)
        get_filename_component(binary_name "${binary}" NAME)
        zz_run_tool(objdump "objdump for ${binary}"
            "${CMAKE_COMMAND}" -E chdir "${binary_directory}"
            "${ZZ_CMAKE_OBJDUMP}" -f -p "${binary_name}")
        if(NOT objdump MATCHES "file format pei-x86-64")
            message(FATAL_ERROR "Expected a MinGW x64 PE binary: ${binary}")
        endif()
        zz_reject_path_leaks("${objdump}" "${binary}")
        string(REGEX MATCHALL
            "DLL Name:[ \\t]*[A-Za-z0-9_.+-]+\\.[dD][lL][lL]"
            dependency_lines "${objdump}")
        foreach(line IN LISTS dependency_lines)
            string(REGEX REPLACE "DLL Name:[ \\t]*" "" dependency "${line}")
            string(STRIP "${dependency}" dependency)
            zz_check_windows_dependency("${dependency}" MinGW "${binary}")
        endforeach()
    endforeach()
elseif(ZZ_SYSTEM_NAME STREQUAL "Darwin")
    foreach(tool_variable ZZ_OTOOL ZZ_LIPO)
        if(NOT EXISTS "${${tool_variable}}" OR IS_DIRECTORY "${${tool_variable}}")
            message(FATAL_ERROR "macOS tool is unavailable: ${tool_variable}")
        endif()
    endforeach()
    list(LENGTH ZZ_EXPECTED_OSX_ARCHITECTURES expected_arch_count)
    if(NOT expected_arch_count EQUAL 1)
        message(FATAL_ERROR "macOS binary gate requires exactly one architecture")
    endif()
    list(GET ZZ_EXPECTED_OSX_ARCHITECTURES 0 expected_arch)
    foreach(binary IN LISTS scan_files)
        zz_run_tool(archs "lipo for ${binary}" "${ZZ_LIPO}" -archs "${binary}")
        string(STRIP "${archs}" archs)
        if(NOT "${archs}" STREQUAL "${expected_arch}")
            message(FATAL_ERROR
                "Unexpected architecture in ${binary}: ${archs}")
        endif()
        get_filename_component(binary_directory "${binary}" DIRECTORY)
        get_filename_component(binary_name "${binary}" NAME)
        zz_run_tool(links "otool for ${binary}"
            "${CMAKE_COMMAND}" -E chdir "${binary_directory}"
            "${ZZ_OTOOL}" -L "${binary_name}")
        zz_reject_path_leaks("${links}" "${binary}")
        string(REPLACE "\r\n" "\n" links "${links}")
        string(REPLACE "\n" ";" link_lines "${links}")
        foreach(line IN LISTS link_lines)
            string(STRIP "${line}" line)
            if(line STREQUAL "" OR line STREQUAL "${binary_name}:")
                continue()
            endif()
            string(REGEX REPLACE "[ \\t]+\\(compatibility version.*" "" dependency "${line}")
            if(dependency MATCHES "[Qq][Ww]indow[Kk]it")
                message(FATAL_ERROR "QWindowKit leaked into ${binary}: ${dependency}")
            endif()
            if(NOT dependency MATCHES
               "^(@rpath/libZz[A-Za-z0-9_]*(\\.[0-9]+)*\\.dylib|@rpath/Qt[^/]+\\.framework/Versions/[^/]+/Qt[^/]+|@rpath/libQt6[A-Za-z0-9_]*\\.dylib|/System/Library/|/usr/lib/)")
                message(FATAL_ERROR
                    "Unexpected macOS dependency in ${binary}: ${dependency}")
            endif()
        endforeach()
    endforeach()
else()
    message(FATAL_ERROR
        "Unsupported platform/compiler pair: ${ZZ_SYSTEM_NAME}/${ZZ_COMPILER_ID}")
endif()

message(STATUS
    "Binary dependency gate passed for ${ZZ_SYSTEM_NAME}/${ZZ_COMPILER_ID}")
