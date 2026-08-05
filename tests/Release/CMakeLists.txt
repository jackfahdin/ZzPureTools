string(REPLACE ";" "\\;" zz_release_osx_architectures
    "${CMAKE_OSX_ARCHITECTURES}")

add_test(NAME release.blocked-without-evidence
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/blocked"
        "-DZZ_QT_PREFIX=${ZZ_QT_PREFIX}"
        "-DZZ_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DZZ_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DZZ_OSX_ARCHITECTURES=${zz_release_osx_architectures}"
        "-DZZ_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        "-DZZ_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
        "-DZZ_EXPECTED_BLOCKERS=qwindowkit.upstream-provenance;qmsetup.windeployqt-5.15.2-derived-work;project.license"
        -P "${PROJECT_SOURCE_DIR}/cmake/ZzExpectConfigureFailure.cmake")
set_tests_properties(release.blocked-without-evidence PROPERTIES
    LABELS "release;contract"
    RUN_SERIAL TRUE
    TIMEOUT 120)

add_test(NAME release.complete-fixture
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_BINARY_ROOT=${CMAKE_BINARY_DIR}"
        "-DZZ_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/complete-fixture"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ZzReleaseChecksFixture.cmake")
set_tests_properties(release.complete-fixture PROPERTIES
    LABELS "release;contract"
    RUN_SERIAL TRUE
    TIMEOUT 30)

if(ZZ_RELEASE_BUILD)
    add_test(NAME release.installed-licenses
        COMMAND "${CMAKE_COMMAND}"
            "-DZZ_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DZZ_INSTALL_ROOT=${CMAKE_BINARY_DIR}/release-install/$<CONFIG>"
            "-DZZ_CONFIG=$<CONFIG>"
            -P "${PROJECT_SOURCE_DIR}/cmake/ZzVerifyInstalledLicenses.cmake")
    set_tests_properties(release.installed-licenses PROPERTIES
        LABELS "release;install"
        RUN_SERIAL TRUE
        TIMEOUT 300)
endif()
