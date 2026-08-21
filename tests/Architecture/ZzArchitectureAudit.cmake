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

set(ZZ_WORKSPACE_PUBLIC_ROOTS
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets/include"
    "${ZZ_SOURCE_DIR}/ZzPureTools/widgets/include")
set(ZZ_WORKSPACE_SOURCE_ROOTS
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets/src"
    "${ZZ_SOURCE_DIR}/ZzPureTools/widgets/src")
set(ZZ_WORKSPACE_PRIVATE_ROOTS
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets/src/private"
    "${ZZ_SOURCE_DIR}/ZzPureTools/widgets/src/private")
include("${ZZ_SOURCE_DIR}/tests/Architecture/CheckZzWorkspaceBoundaries.cmake")
