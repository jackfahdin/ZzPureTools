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

    set(ZZ_WINDOWKIT_BUILD_QT_MAJOR "${Qt6_VERSION_MAJOR}")
    set(ZZ_WINDOWKIT_BUILD_QT_MINOR "${Qt6_VERSION_MINOR}")

    if(NOT BUILD_SHARED_LIBS)
        get_target_property(ZZ_QWK_PRIVATE_INSTALL_DIR
            ZzWindowKit ZZ_QWK_PRIVATE_INSTALL_DIR)
        get_target_property(ZZ_QWK_CORE_FILE
            ZzWindowKit ZZ_QWK_CORE_FILE)
        get_target_property(ZZ_QWK_WIDGETS_FILE
            ZzWindowKit ZZ_QWK_WIDGETS_FILE)

        foreach(zz_qwk_property IN ITEMS
            ZZ_QWK_PRIVATE_INSTALL_DIR
            ZZ_QWK_CORE_FILE
            ZZ_QWK_WIDGETS_FILE
        )
            if("${${zz_qwk_property}}" STREQUAL ""
               OR "${${zz_qwk_property}}" MATCHES "-NOTFOUND$")
                message(FATAL_ERROR
                    "static package requires ZzWindowKit property ${zz_qwk_property}")
            endif()
        endforeach()

        if(WIN32)
            set(ZZ_QWK_PLATFORM_LIBRARIES "uxtheme")
        elseif(APPLE)
            set(ZZ_QWK_PLATFORM_LIBRARIES
                "-framework Foundation;-framework Cocoa;-framework AppKit"
            )
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(ZZ_QWK_PLATFORM_LIBRARIES "${CMAKE_DL_LIBS}")
        else()
            message(FATAL_ERROR
                "ZzWindowKit supports only Windows, macOS, and Linux")
        endif()

        configure_package_config_file(
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ZzWindowKitPrivateTargets.cmake.in"
            "${PROJECT_BINARY_DIR}/ZzWindowKitPrivateTargets.cmake"
            INSTALL_DESTINATION "${zz_package_cmake_dir}"
        )
        install(FILES
            "${PROJECT_BINARY_DIR}/ZzWindowKitPrivateTargets.cmake"
            DESTINATION "${zz_package_cmake_dir}"
            COMPONENT Development
        )
    endif()

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

    if(ZZ_BUNDLE_GNU_RUNTIME)
        foreach(zz_runtime_variable IN ITEMS
            ZZ_GNU_LIBSTDCXX_PATH
            ZZ_GNU_LIBGCC_PATH)
            if(NOT DEFINED ${zz_runtime_variable}
               OR NOT EXISTS "${${zz_runtime_variable}}"
               OR IS_DIRECTORY "${${zz_runtime_variable}}")
                message(FATAL_ERROR
                    "GNU runtime input is invalid: ${zz_runtime_variable}")
            endif()
        endforeach()
        install(FILES "${ZZ_GNU_LIBSTDCXX_PATH}"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RENAME libstdc++.so.6
            COMPONENT Runtime)
        install(FILES "${ZZ_GNU_LIBGCC_PATH}"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            RENAME libgcc_s.so.1
            COMPONENT Runtime)
        install(FILES
            "${ZZ_GNU_RUNTIME_LICENSE_DIR}/COPYING3"
            "${ZZ_GNU_RUNTIME_LICENSE_DIR}/COPYING.RUNTIME"
            DESTINATION
                "${CMAKE_INSTALL_DATAROOTDIR}/ZzPureToolsPro/licenses/gcc-runtime"
            COMPONENT Runtime)
    endif()

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
