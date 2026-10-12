cmake_minimum_required(VERSION 3.23)
project(QtGlibPrecisionDiagnostic LANGUAGES CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This diagnostic requires Linux and matching Qt 6.11.1 sources")
endif()
find_package(Qt6 6.11.1 EXACT REQUIRED COMPONENTS Core CorePrivate)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.36)
set(ZZ_QTBASE_SOURCE_DIR "" CACHE PATH "Matching Qt 6.11.1 qtbase sources")
set(qt_kernel "${ZZ_QTBASE_SOURCE_DIR}/src/corelib/kernel")
if(NOT EXISTS "${qt_kernel}/qeventdispatcher_glib.cpp")
    message(FATAL_ERROR "Provide -DZZ_QTBASE_SOURCE_DIR=<Qt 6.11.1 qtbase sources>")
endif()
set(CMAKE_AUTOMOC ON)
file(READ glib-source-test.cpp test_body)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/glib-source-test.cpp")
find_package(Git REQUIRED)
set(candidate_root "${CMAKE_CURRENT_BINARY_DIR}/candidate-qtbase")
set(candidate_kernel "${candidate_root}/src/corelib/kernel")
file(MAKE_DIRECTORY "${candidate_kernel}")
foreach(name qeventdispatcher_glib.cpp qtimerinfo_unix.cpp qeventdispatcher_wasm.cpp)
    configure_file("${qt_kernel}/${name}" "${candidate_kernel}/${name}" COPYONLY)
endforeach()
foreach(patch "${CMAKE_CURRENT_SOURCE_DIR}/../../qt-timer-precision/2026-10-12/qtbase-6.11.1-timer-wait.patch"
        "${CMAKE_CURRENT_SOURCE_DIR}/qtbase-6.11.1-glib-ready-time.patch")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patch}")
    execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${patch}"
        WORKING_DIRECTORY "${candidate_root}" COMMAND_ERROR_IS_FATAL ANY)
endforeach()
enable_testing()
foreach(mode original candidate)
    set(kernel "${qt_kernel}")
    if(mode STREQUAL "candidate")
        set(kernel "${candidate_kernel}")
    endif()
    file(READ "${kernel}/qeventdispatcher_glib.cpp" source)
    # The SDK hides QThreadData::current()'s helpers. Both probe variants resolve
    # the same current thread through an exported API; the Qt patch doesn't change it.
    string(REPLACE "QThreadData::current()" "QThreadData::get2(QThread::currentThread())"
        source "${source}")
    # Marker is diagnostic-only and verifies actual entry through the replacement.
    string(REPLACE "static gboolean timerSourcePrepare(GSource *source, gint *timeout)\n{"
        "static gboolean timerSourcePrepare(GSource *source, gint *timeout)\n{\n    static const bool marker = [] { qWarning(\"diagnostic GLib timer prepare active: ${mode}\"); return true; }();\n    (void)marker;"
        source "${source}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${mode}-dispatcher.cpp" "${source}")
    # Tests exercise the real static callbacks without adding production test hooks.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${mode}-test.cpp" "${source}\n${test_body}")
    add_library(glib-dispatch-${mode} SHARED "${CMAKE_CURRENT_BINARY_DIR}/${mode}-dispatcher.cpp"
        "${qt_kernel}/qeventdispatcher_glib_p.h" "${kernel}/qtimerinfo_unix.cpp")
    add_executable(glib-source-${mode} "${CMAKE_CURRENT_BINARY_DIR}/${mode}-test.cpp"
        "${qt_kernel}/qeventdispatcher_glib_p.h" "${kernel}/qtimerinfo_unix.cpp")
    foreach(target glib-dispatch-${mode} glib-source-${mode})
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_include_directories(${target} PRIVATE "${qt_kernel}")
        target_link_libraries(${target} PRIVATE Qt6::CorePrivate PkgConfig::GLIB)
    endforeach()
    add_test(NAME qt.glib-source-${mode} COMMAND glib-source-${mode})
    add_executable(glib-runtime-${mode} glib-runtime-test.cpp)
    target_compile_features(glib-runtime-${mode} PRIVATE cxx_std_20)
    target_link_libraries(glib-runtime-${mode} PRIVATE glib-dispatch-${mode} Qt6::CorePrivate PkgConfig::GLIB)
    add_test(NAME qt.glib-runtime-${mode} COMMAND glib-runtime-${mode})
    set_tests_properties(qt.glib-runtime-${mode} PROPERTIES TIMEOUT 10)
endforeach()
# Original precision checks intentionally fail. Do not invert or add to formal gates.
