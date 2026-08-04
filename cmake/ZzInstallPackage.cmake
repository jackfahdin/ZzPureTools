include_guard(GLOBAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

function(zz_install_package)
    set(zz_export_name ZzPureToolsProTargets)
    set(zz_package_cmake_dir
        "${CMAKE_INSTALL_LIBDIR}/cmake/ZzPureToolsPro")
    set(zz_targets
        ZzCore
        ZzWindowKit
        ZzFluentFoundation
        ZzFluentUI
        ZzAppCore
        ZzPureTools
    )

    foreach(zz_target IN LISTS zz_targets)
        if(NOT TARGET ${zz_target})
            message(FATAL_ERROR
                "zz_install_package requires target ${zz_target}")
        endif()
    endforeach()

    install(TARGETS ${zz_targets}
        EXPORT ${zz_export_name}
        RUNTIME
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT Runtime
        LIBRARY
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT Runtime
            NAMELINK_COMPONENT Development
        ARCHIVE
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT Development
        INCLUDES
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    set(zz_public_include_roots
        "${PROJECT_SOURCE_DIR}/ZzCore/include"
        "${PROJECT_SOURCE_DIR}/ZzWindowKit/include"
        "${PROJECT_SOURCE_DIR}/ZzFluentUI/foundation/include"
        "${PROJECT_SOURCE_DIR}/ZzFluentUI/widgets/include"
        "${PROJECT_SOURCE_DIR}/ZzPureTools/appcore/include"
        "${PROJECT_SOURCE_DIR}/ZzPureTools/widgets/include"
    )
    foreach(zz_include_root IN LISTS zz_public_include_roots)
        install(DIRECTORY "${zz_include_root}/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            COMPONENT Development
            FILES_MATCHING PATTERN "*.h"
        )
    endforeach()

    foreach(zz_target IN LISTS zz_targets)
        get_target_property(zz_generated_header
            ${zz_target} ZZ_GENERATED_EXPORT_HEADER)
        get_target_property(zz_generated_subdir
            ${zz_target} ZZ_EXPORT_HEADER_INSTALL_SUBDIR)
        if(NOT zz_generated_header
           OR zz_generated_header MATCHES "-NOTFOUND$")
            message(FATAL_ERROR
                "${zz_target} has no generated export header property")
        endif()
        if(NOT zz_generated_subdir
           OR zz_generated_subdir MATCHES "-NOTFOUND$")
            message(FATAL_ERROR
                "${zz_target} has no export header install subdirectory")
        endif()

        install(FILES "${zz_generated_header}"
            DESTINATION
                "${CMAKE_INSTALL_INCLUDEDIR}/${zz_generated_subdir}"
            COMPONENT Development
        )
    endforeach()

    configure_package_config_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ZzPureToolsProConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfig.cmake"
        INSTALL_DESTINATION "${zz_package_cmake_dir}"
    )
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMinorVersion
    )

    install(EXPORT ${zz_export_name}
        FILE ZzPureToolsProTargets.cmake
        NAMESPACE Zz::
        DESTINATION "${zz_package_cmake_dir}"
        COMPONENT Development
    )
    install(FILES
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfig.cmake"
        "${PROJECT_BINARY_DIR}/ZzPureToolsProConfigVersion.cmake"
        DESTINATION "${zz_package_cmake_dir}"
        COMPONENT Development
    )

    install(FILES
        "${PROJECT_SOURCE_DIR}/docs/superpowers/specs/2026-08-02-zzpuretoolspro-architecture-design.md"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/ZzPureToolsPro"
        COMPONENT Development
    )

    if(EXISTS "${PROJECT_SOURCE_DIR}/LICENSE")
        install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/ZzPureToolsPro"
            COMPONENT Runtime
        )
    else()
        message(STATUS
            "ZzPureToolsPro LICENSE is absent; binary publication remains blocked")
    endif()
endfunction()
