include_guard(GLOBAL)

include(ZzCompilerWarnings)
include(ZzLto)
include(ZzSanitizers)
include(ZzStaticAnalysis)

function(zz_configure_first_party_target target_name)
    cmake_parse_arguments(PARSE_ARGV 1 ZZ_TARGET "" "" "SOURCES")
    if(NOT ZZ_TARGET_SOURCES)
        message(FATAL_ERROR
            "zz_configure_first_party_target(${target_name}) requires SOURCES")
    endif()

    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        AUTOMOC ON
        AUTORCC ON
    )

    zz_enable_project_warnings(${target_name})
    zz_enable_sanitizers(${target_name})
    zz_enable_lto(${target_name})
    zz_register_clang_tidy(${target_name}
        SOURCES ${ZZ_TARGET_SOURCES})
endfunction()
