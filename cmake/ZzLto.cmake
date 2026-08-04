include_guard(GLOBAL)
include(CheckIPOSupported)

function(zz_enable_lto target_name)
    if(NOT ZZ_ENABLE_LTO)
        return()
    endif()

    check_ipo_supported(
        RESULT zz_ipo_supported
        OUTPUT zz_ipo_error
        LANGUAGES CXX
    )
    if(NOT zz_ipo_supported)
        message(FATAL_ERROR
            "ZZ_ENABLE_LTO=ON, but ${target_name} cannot enable IPO: ${zz_ipo_error}")
    endif()

    set_property(TARGET ${target_name} PROPERTY
        INTERPROCEDURAL_OPTIMIZATION TRUE)
endfunction()
