include_guard(GLOBAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# The embedded upstream install rules use Unspecified for every artifact.
# Own the component mapping here; keep upstream sources and package API intact.
function(zz_install_log_package)
    get_target_property(zz_log_version ZzLog VERSION)
    if(NOT zz_log_version OR zz_log_version MATCHES "-NOTFOUND$")
        message(FATAL_ERROR "ZzLog target must declare its package version")
    endif()
    set(zz_log_source "${PROJECT_SOURCE_DIR}/ZzThirdParty/ZzLog")
    set(zz_log_generated "${PROJECT_BINARY_DIR}/ZzLogPackage")
    set(zz_log_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/ZzLog")

    install(TARGETS ZzLog EXPORT ZzLogTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Runtime
            NAMELINK_COMPONENT Development
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(DIRECTORY "${zz_log_source}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT Development)

    configure_package_config_file(
        "${zz_log_source}/cmake/ZzLogConfig.cmake.in"
        "${zz_log_generated}/ZzLogConfig.cmake"
        INSTALL_DESTINATION "${zz_log_package_dir}")
    write_basic_package_version_file(
        "${zz_log_generated}/ZzLogConfigVersion.cmake"
        VERSION "${zz_log_version}" COMPATIBILITY SameMinorVersion)
    install(EXPORT ZzLogTargets
        FILE ZzLogTargets.cmake NAMESPACE ZzLog::
        DESTINATION "${zz_log_package_dir}" COMPONENT Development)
    install(FILES
        "${zz_log_generated}/ZzLogConfig.cmake"
        "${zz_log_generated}/ZzLogConfigVersion.cmake"
        DESTINATION "${zz_log_package_dir}" COMPONENT Development)

    # Both redistributable binaries and static SDKs need upstream notices.
    foreach(zz_log_component IN ITEMS Runtime Development)
        install(DIRECTORY "${zz_log_source}/licenses/"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/ZzLog/licenses"
            COMPONENT ${zz_log_component})
        install(FILES "${zz_log_source}/LICENSE"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/ZzLog/licenses"
            COMPONENT ${zz_log_component})
    endforeach()
    install(FILES
        "${zz_log_source}/README.md"
        "${zz_log_source}/BUILDING-MULTIPLATFORM.md"
        "${zz_log_source}/DEPENDENCIES.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/ZzLog" COMPONENT Development)
endfunction()
