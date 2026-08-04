include_guard(GLOBAL)

function(zz_enable_sanitizers target_name)
    if(MSVC)
        if(ZZ_ENABLE_ASAN)
            target_compile_options(${target_name} PRIVATE /fsanitize=address)
            target_link_options(${target_name} PRIVATE /fsanitize=address)
        endif()
        if(ZZ_ENABLE_UBSAN)
            message(FATAL_ERROR "ZZ_ENABLE_UBSAN is not supported by MSVC presets")
        endif()
        return()
    endif()

    set(zz_sanitizers)
    if(ZZ_ENABLE_ASAN)
        list(APPEND zz_sanitizers address)
    endif()
    if(ZZ_ENABLE_UBSAN)
        list(APPEND zz_sanitizers undefined)
    endif()

    if(zz_sanitizers)
        list(JOIN zz_sanitizers "," zz_sanitizer_list)
        target_compile_options(${target_name} PRIVATE
            -fno-omit-frame-pointer
            "-fsanitize=${zz_sanitizer_list}"
        )
        target_link_options(${target_name} PRIVATE
            -fno-omit-frame-pointer
            "-fsanitize=${zz_sanitizer_list}"
        )
    endif()
endfunction()
