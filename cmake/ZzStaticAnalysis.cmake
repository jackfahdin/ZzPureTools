include_guard(GLOBAL)

function(zz_register_clang_tidy target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_TIDY "" "" "SOURCES")
    if(NOT ZZ_ENABLE_CLANG_TIDY)
        return()
    endif()
    if(NOT ZZ_TIDY_SOURCES)
        message(FATAL_ERROR
            "zz_register_clang_tidy(${target_name}) requires SOURCES")
    endif()
    if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        message(FATAL_ERROR
            "ZZ_ENABLE_CLANG_TIDY requires CMAKE_EXPORT_COMPILE_COMMANDS=ON")
    endif()

    if(NOT ZZ_CLANG_TIDY_EXECUTABLE)
        find_program(zz_clang_tidy_program
            NAMES clang-tidy clang-tidy-21 clang-tidy-20 clang-tidy-19
            REQUIRED)
        set(ZZ_CLANG_TIDY_EXECUTABLE
            "${zz_clang_tidy_program}"
            CACHE FILEPATH "clang-tidy executable used by Zz targets")
    endif()

    if(NOT TARGET ZzClangTidy)
        add_custom_target(ZzClangTidy)
    endif()

    set(zz_tidy_stamps)
    foreach(zz_source IN LISTS ZZ_TIDY_SOURCES)
        get_filename_component(zz_source_absolute
            "${zz_source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        string(SHA256 zz_source_hash "${zz_source_absolute}")
        string(SUBSTRING "${zz_source_hash}" 0 16 zz_source_hash_short)
        set(zz_stamp
            "${CMAKE_BINARY_DIR}/clang-tidy/${target_name}/${zz_source_hash_short}.stamp")

        add_custom_command(
            OUTPUT "${zz_stamp}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_BINARY_DIR}/clang-tidy/${target_name}"
            COMMAND "${ZZ_CLANG_TIDY_EXECUTABLE}"
                "-p=${CMAKE_BINARY_DIR}"
                "--checks=-*,clang-analyzer-*,bugprone-*,performance-*,modernize-use-nullptr,modernize-use-override"
                "--warnings-as-errors=clang-analyzer-*,bugprone-*,performance-*"
                "${zz_source_absolute}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${zz_stamp}"
            DEPENDS
                "${zz_source_absolute}"
                "${CMAKE_BINARY_DIR}/compile_commands.json"
            VERBATIM
        )
        list(APPEND zz_tidy_stamps "${zz_stamp}")
    endforeach()

    set(zz_target_tidy "${target_name}ClangTidy")
    add_custom_target(${zz_target_tidy} DEPENDS ${zz_tidy_stamps})
    add_dependencies(${zz_target_tidy} ${target_name})
    add_dependencies(ZzClangTidy ${zz_target_tidy})
endfunction()
