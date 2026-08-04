include_guard(GLOBAL)

function(zz_collect_buildsystem_targets source_directory output_variable)
    get_property(zz_directory_targets
        DIRECTORY "${source_directory}"
        PROPERTY BUILDSYSTEM_TARGETS)
    get_property(zz_subdirectories
        DIRECTORY "${source_directory}"
        PROPERTY SUBDIRECTORIES)

    set(zz_collected_targets ${zz_directory_targets})
    foreach(zz_subdirectory IN LISTS zz_subdirectories)
        zz_collect_buildsystem_targets(
            "${zz_subdirectory}"
            zz_subdirectory_targets)
        list(APPEND zz_collected_targets ${zz_subdirectory_targets})
    endforeach()

    set(${output_variable} "${zz_collected_targets}" PARENT_SCOPE)
endfunction()

function(zz_finalize_clang_tidy_dependencies)
    if(NOT TARGET ZzClangTidy)
        return()
    endif()

    zz_collect_buildsystem_targets(
        "${PROJECT_SOURCE_DIR}"
        zz_buildsystem_targets)
    foreach(zz_candidate IN LISTS zz_buildsystem_targets)
        if(zz_candidate STREQUAL "ZzClangTidy")
            continue()
        endif()

        get_target_property(zz_candidate_type ${zz_candidate} TYPE)
        if(zz_candidate_type MATCHES
           "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            add_dependencies(ZzClangTidy ${zz_candidate})
        endif()
    endforeach()
endfunction()

function(zz_register_clang_tidy target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_TIDY "" "" "SOURCES")
    if(NOT ZZ_ENABLE_CLANG_TIDY)
        return()
    endif()
    if(NOT PROJECT_IS_TOP_LEVEL)
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

    if(NOT TARGET ZzClangTidy)
        find_program(ZZ_BASH_EXECUTABLE NAMES bash REQUIRED)
        find_program(ZZ_RUN_CLANG_TIDY
            NAMES
                run-clang-tidy-20
                run-clang-tidy-19
                run-clang-tidy-18
                run-clang-tidy-17
                run-clang-tidy
            REQUIRED)
        find_program(ZZ_CLANG_TIDY_EXECUTABLE
            NAMES
                clang-tidy-20
                clang-tidy-19
                clang-tidy-18
                clang-tidy-17
                clang-tidy
            REQUIRED)
        add_custom_target(ZzClangTidy
            COMMAND "${ZZ_BASH_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/scripts/ci/run-clang-tidy.sh"
                "${PROJECT_SOURCE_DIR}"
                "${PROJECT_BINARY_DIR}"
                "${ZZ_RUN_CLANG_TIDY}"
                "${ZZ_CLANG_TIDY_EXECUTABLE}"
            VERBATIM
        )
        cmake_language(DEFER
            DIRECTORY "${PROJECT_SOURCE_DIR}"
            CALL zz_finalize_clang_tidy_dependencies)
    endif()

    add_dependencies(ZzClangTidy ${target_name})
endfunction()
