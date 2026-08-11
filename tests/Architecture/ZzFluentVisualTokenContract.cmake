cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_SOURCE_DIR ZZ_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

set(runner "${ZZ_SOURCE_DIR}/tests/Architecture/RunFluentVisualTokenFixture.cmake")
file(REMOVE_RECURSE "${ZZ_TEST_ROOT}")
set(widget_root "${ZZ_TEST_ROOT}/ZzFluentUI/widgets/src")
file(MAKE_DIRECTORY "${widget_root}")
file(WRITE "${widget_root}/ZzGood.cpp" [=[
// QColor(1, 2, 3) and setFixedWidth(42) are documentation examples.
/*
 * setStyleSheet(QString()) and QColor ignored(1, 2, 3) are also examples.
 */
const char *text = "setStyleSheet QColor(1, 2, 3) 0x112233";
widget->setFixedWidth(controlWidth);
painter->drawRoundedRect(rect, radius, radius);
]=])
file(WRITE "${ZZ_TEST_ROOT}/empty.txt" "")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${ZZ_SOURCE_DIR}"
        "-DZZ_SCAN_ROOT=${ZZ_TEST_ROOT}"
        "-DZZ_ALLOWLIST=${ZZ_TEST_ROOT}/empty.txt"
        -P "${runner}"
    RESULT_VARIABLE good_result
    ERROR_VARIABLE good_error)
if(NOT good_result EQUAL 0)
    message(FATAL_ERROR "Good visual token fixture was rejected: ${good_error}")
endif()

file(WRITE "${widget_root}/ZzBad.cpp" [=[
widget->setStyleSheet(QString());
const QColor color(1, 2, 3);
widget->setFixedWidth(42);
painter->drawRoundedRect(rect, 4, 4);
]=])
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${ZZ_SOURCE_DIR}"
        "-DZZ_SCAN_ROOT=${ZZ_TEST_ROOT}"
        "-DZZ_ALLOWLIST=${ZZ_TEST_ROOT}/empty.txt"
        -P "${runner}"
    RESULT_VARIABLE bad_result
    OUTPUT_VARIABLE bad_output
    ERROR_VARIABLE bad_error)
set(bad_log "${bad_output}${bad_error}")
if(bad_result EQUAL 0
   OR NOT bad_log MATCHES "FLUENT_RAW_COLOR"
   OR NOT bad_log MATCHES "FLUENT_DIMENSION_MAGIC")
    message(FATAL_ERROR "Bad visual token fixture was accepted: ${bad_log}")
endif()

file(WRITE "${ZZ_TEST_ROOT}/stale.txt"
    "FLUENT_DIMENSION_MAGIC|ZzFluentUI/widgets/src/ZzCarouselView.cpp|99|setFixedWidth\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${ZZ_SOURCE_DIR}"
        "-DZZ_SCAN_ROOT=${ZZ_TEST_ROOT}"
        "-DZZ_ALLOWLIST=${ZZ_TEST_ROOT}/stale.txt"
        -P "${runner}"
    RESULT_VARIABLE stale_result
    OUTPUT_VARIABLE stale_output
    ERROR_VARIABLE stale_error)
if(stale_result EQUAL 0
   OR NOT "${stale_output}${stale_error}" MATCHES
      "FLUENT_VISUAL_ALLOWLIST_STALE")
    message(FATAL_ERROR "Stale visual allowlist entry was accepted")
endif()

message(STATUS "Fluent visual token contract passed")
