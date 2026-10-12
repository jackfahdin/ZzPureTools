cmake_minimum_required(VERSION 3.23)
project(QtTimerPrecisionDiagnostic LANGUAGES CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This diagnostic is limited to the Linux UNIX event dispatcher")
endif()
find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core CorePrivate)
set(ZZ_QTBASE_SOURCE_DIR "" CACHE PATH "Qt 6.11.1 qtbase source directory matching this SDK")
set(qt_kernel "${ZZ_QTBASE_SOURCE_DIR}/src/corelib/kernel")
if(NOT EXISTS "${qt_kernel}/qeventdispatcher_unix.cpp")
    message(FATAL_ERROR "Provide -DZZ_QTBASE_SOURCE_DIR=<matching Qt 6.11.1 qtbase sources>")
endif()
set(CMAKE_AUTOMOC ON)
add_library(dispatch-probe SHARED
    "${qt_kernel}/qeventdispatcher_unix.cpp"
    "${qt_kernel}/qeventdispatcher_unix_p.h"
    timer-wait.cpp)
target_compile_features(dispatch-probe PRIVATE cxx_std_20)
target_link_libraries(dispatch-probe PRIVATE Qt6::CorePrivate)
add_executable(timer-wait-test timer-wait-test.cpp)
target_compile_features(timer-wait-test PRIVATE cxx_std_20)
target_link_libraries(timer-wait-test PRIVATE Qt6::CorePrivate)

enable_testing()
add_test(NAME qt.timer-wait-precision-original COMMAND timer-wait-test)
add_test(NAME qt.timer-wait-precision-candidate
    COMMAND "${CMAKE_COMMAND}" -E env ZZ_DIAGNOSTIC_PRECISE_WAIT=1
        "LD_PRELOAD=$<TARGET_FILE:dispatch-probe>" "$<TARGET_FILE:timer-wait-test>")
# The original test intentionally fails on the affected SDK; do not invert it.
# This standalone diagnostic is not registered in the framework test suite.
