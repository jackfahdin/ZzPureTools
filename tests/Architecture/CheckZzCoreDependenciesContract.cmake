cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR)
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()

set(zz_scanner
    "${ZZ_SOURCE_DIR}/tests/Architecture/CheckZzCoreDependencies.cmake")
set(zz_good_fixture
    "${ZZ_SOURCE_DIR}/tests/Architecture/fixtures/zzcore-good")
set(zz_bad_fixture
    "${ZZ_SOURCE_DIR}/tests/Architecture/fixtures/zzcore-bad")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SCAN_ROOT=${zz_good_fixture}"
        -P "${zz_scanner}"
    RESULT_VARIABLE zz_good_result
    OUTPUT_VARIABLE zz_good_stdout
    ERROR_VARIABLE zz_good_stderr
)
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SCAN_ROOT=${zz_bad_fixture}"
        -P "${zz_scanner}"
    RESULT_VARIABLE zz_bad_result
    OUTPUT_VARIABLE zz_bad_stdout
    ERROR_VARIABLE zz_bad_stderr
)

if(NOT EXISTS "${zz_scanner}")
    message(FATAL_ERROR
        "ZzCore dependency scanner is missing: ${zz_scanner}\n"
        "good invocation: ${zz_good_stderr}\n"
        "bad invocation: ${zz_bad_stderr}")
endif()

if(NOT zz_good_result EQUAL 0)
    message(FATAL_ERROR
        "good ZzCore dependency fixture was rejected:\n"
        "${zz_good_stdout}${zz_good_stderr}")
endif()
if(zz_bad_result EQUAL 0)
    message(FATAL_ERROR "bad ZzCore dependency fixture was accepted")
endif()

set(zz_bad_output "${zz_bad_stdout}${zz_bad_stderr}")
foreach(zz_expected IN ITEMS
    "ZzBadHeader.h"
    "ZZCORE_FORBIDDEN_QT_MODULE"
    "ZZCORE_QT_PRIVATE_INCLUDE"
    "ZZCORE_UNQUALIFIED_UI_INCLUDE"
    "ZZCORE_CHAINED_NAMESPACE"
)
    if(NOT zz_bad_output MATCHES "${zz_expected}")
        message(FATAL_ERROR
            "bad fixture report lacks ${zz_expected}:\n${zz_bad_output}")
    endif()
endforeach()
