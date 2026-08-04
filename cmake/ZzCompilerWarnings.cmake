include_guard(GLOBAL)

function(zz_apply_first_party_warnings target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_WARNINGS "" "" "SOURCES")
    if(NOT ZZ_WARNINGS_SOURCES)
        message(FATAL_ERROR
            "zz_apply_first_party_warnings(${target_name}) requires SOURCES")
    endif()

    if(MSVC)
        set(zz_warning_options
            /W4
            /permissive-
            /Zc:__cplusplus
            /utf-8
        )
        if(ZZ_WARNINGS_AS_ERRORS)
            list(APPEND zz_warning_options /WX)
        endif()
    else()
        set(zz_warning_options
            -Wall
            -Wextra
            -Wpedantic
        )
        if(ZZ_WARNINGS_AS_ERRORS)
            list(APPEND zz_warning_options -Werror)
        endif()
    endif()

    foreach(zz_source IN LISTS ZZ_WARNINGS_SOURCES)
        set_property(SOURCE "${zz_source}" APPEND PROPERTY
            COMPILE_OPTIONS ${zz_warning_options})
    endforeach()
endfunction()

function(zz_enable_project_warnings target_name)
    get_target_property(zz_target_sources ${target_name} SOURCES)
    if(NOT zz_target_sources)
        message(FATAL_ERROR
            "zz_enable_project_warnings(${target_name}) requires sources")
    endif()

    set(zz_first_party_translation_units)
    foreach(zz_source IN LISTS zz_target_sources)
        if("${zz_source}" MATCHES "\\.(cc|cpp|cxx|mm)$")
            list(APPEND zz_first_party_translation_units "${zz_source}")
        endif()
    endforeach()
    if(NOT zz_first_party_translation_units)
        message(FATAL_ERROR
            "${target_name} has no explicit first-party translation unit")
    endif()

    zz_apply_first_party_warnings(${target_name}
        SOURCES ${zz_first_party_translation_units})
endfunction()
