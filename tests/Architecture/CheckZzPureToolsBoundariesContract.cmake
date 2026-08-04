cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR ZZ_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR
    NORMALIZE
    OUTPUT_VARIABLE zz_source_root)

set(zz_checker
    "${zz_source_root}/tests/Architecture/CheckZzPureToolsBoundaries.cmake")
if(NOT EXISTS "${zz_checker}")
    message(FATAL_ERROR
        "PureTools boundary checker is missing: ${zz_checker}")
endif()

set(zz_fixture_root
    "${zz_source_root}/tests/Architecture/fixtures")
set(zz_good_root "${zz_fixture_root}/zzpuretools-good")
set(zz_bad_root "${zz_fixture_root}/zzpuretools-bad")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_APPCORE_ROOT=${zz_good_root}/appcore"
        "-DZZ_WIDGETS_ROOT=${zz_good_root}/widgets"
        "-DZZ_APPCORE_PUBLIC_ROOT=${zz_good_root}/appcore"
        "-DZZ_WIDGETS_PUBLIC_ROOT=${zz_good_root}/widgets"
        "-DZZ_ALLOWED_COMPOSITION_FILE=${zz_good_root}/widgets/ZzGoodWidget.h"
        -DZZ_REQUIRE_COMPOSITION=OFF
        -P "${zz_checker}"
    RESULT_VARIABLE zz_good_result
    OUTPUT_VARIABLE zz_good_stdout
    ERROR_VARIABLE zz_good_stderr
)
if(NOT zz_good_result EQUAL 0)
    message(FATAL_ERROR
        "good PureTools fixture was rejected:\n"
        "${zz_good_stdout}\n${zz_good_stderr}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_APPCORE_ROOT=${zz_bad_root}/appcore"
        "-DZZ_WIDGETS_ROOT=${zz_bad_root}/widgets"
        "-DZZ_APPCORE_PUBLIC_ROOT=${zz_bad_root}/appcore"
        "-DZZ_WIDGETS_PUBLIC_ROOT=${zz_bad_root}/widgets"
        "-DZZ_ALLOWED_COMPOSITION_FILE=${zz_bad_root}/widgets/ZzAllowedComposition.cpp"
        -DZZ_REQUIRE_COMPOSITION=OFF
        -P "${zz_checker}"
    RESULT_VARIABLE zz_bad_result
    OUTPUT_VARIABLE zz_bad_stdout
    ERROR_VARIABLE zz_bad_stderr
)
if(zz_bad_result EQUAL 0)
    message(FATAL_ERROR "bad PureTools fixture was accepted")
endif()
set(zz_bad_output "${zz_bad_stdout}\n${zz_bad_stderr}")

foreach(zz_rule IN ITEMS
    APP_CORE_UI_DEPENDENCY
    PRESENTATION_BUSINESS_DEPENDENCY
    QT_PRIVATE_OR_QWK
    CHAINED_NAMESPACE
    COMPOSITION_UNIQUENESS
    PUBLIC_API_DOXYGEN
)
    if(NOT zz_bad_output MATCHES "${zz_rule}:")
        message(FATAL_ERROR
            "bad fixture output is missing ${zz_rule}:\n${zz_bad_output}")
    endif()
endforeach()
foreach(zz_bad_file IN ITEMS ZzBadAppCore.h ZzBadWidget.h)
    if(NOT zz_bad_output MATCHES "${zz_bad_file}")
        message(FATAL_ERROR
            "bad fixture output is missing ${zz_bad_file}:\n${zz_bad_output}")
    endif()
endforeach()

message(STATUS "PureTools boundary checker contract passed")
