add_subdirectory(Architecture)

set(zz_install_consumer_support_dir
    "${CMAKE_CURRENT_BINARY_DIR}/InstallConsumer")
file(MAKE_DIRECTORY "${zz_install_consumer_support_dir}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/InstallConsumer/InstallConsumerContext.cmake.in"
    "${zz_install_consumer_support_dir}/InstallConsumerContext.cmake"
    @ONLY
)

add_test(
    NAME install.consumer
    COMMAND "${CMAKE_COMMAND}"
        "-DZZ_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DZZ_TEST_ROOT=${zz_install_consumer_support_dir}/$<CONFIG>"
        "-DZZ_CONTEXT_FILE=${zz_install_consumer_support_dir}/InstallConsumerContext.cmake"
        "-DZZ_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/InstallConsumer/RunInstallConsumer.cmake"
)
set_tests_properties(install.consumer PROPERTIES
    LABELS "install"
    RUN_SERIAL TRUE
    TIMEOUT 600
)
