cmake_minimum_required(VERSION 3.23)

foreach(required ZZ_SOURCE_DIR ZZ_TARGET_MANIFEST)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

list(PREPEND CMAKE_MODULE_PATH "${ZZ_SOURCE_DIR}/cmake")
include("${ZZ_SOURCE_DIR}/cmake/ZzArchitectureChecks.cmake")
zz_run_complete_architecture_audit(
    "${ZZ_SOURCE_DIR}" "${ZZ_TARGET_MANIFEST}")
