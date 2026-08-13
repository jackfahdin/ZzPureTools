set(zz_window_kit_sources
    src/private/ZzWindowKitVersion.cpp
    src/ZzWindowAgent.cpp
    src/private/ZzWindowAgentPrivate.cpp
    src/ZzWindowKitBootstrap.cpp
    src/private/ZzQWindowKitBackend.cpp
    src/private/ZzSoftwareBackdrop.cpp
    src/private/ZzSoftwareBackdropPrivate.cpp
)
set(zz_window_kit_moc_headers
    include/ZzWindowKit/ZzWindowAgent.h
)

if(ZZ_BUILD_BENCHMARKS)
    list(APPEND zz_window_kit_sources
        src/private/ZzWindowKitDiagnostics.cpp)
endif()

set(QWINDOWKIT_BUILD_STATIC ON)
set(QWINDOWKIT_BUILD_WIDGETS ON)
set(QWINDOWKIT_BUILD_QUICK OFF)
set(QWINDOWKIT_BUILD_EXAMPLES OFF)
set(QWINDOWKIT_BUILD_DOCUMENTATIONS OFF)
set(QWINDOWKIT_INSTALL OFF)
set(QWINDOWKIT_FORCE_QT_WINDOW_CONTEXT
    ${ZZ_WINDOWKIT_FORCE_QT_CONTEXT}
)
set(QWINDOWKIT_ENABLE_STYLE_AGENT OFF)

# 先在父目录加载，避免 Qt imported target 被限制在上游子目录作用域。
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6CorePrivate ${Qt6_VERSION} EXACT QUIET)
find_package(Qt6GuiPrivate ${Qt6_VERSION} EXACT QUIET)
find_package(Qt6WidgetsPrivate ${Qt6_VERSION} EXACT QUIET)

add_subdirectory(
    "${PROJECT_SOURCE_DIR}/ZzThirdParty/qwindowkit"
    "${PROJECT_BINARY_DIR}/third-party/qwindowkit"
    EXCLUDE_FROM_ALL
)

foreach(zz_qwk_target IN ITEMS
    QWindowKit::Core
    QWindowKit::Widgets
    Qt6::CorePrivate
    Qt6::GuiPrivate
    Qt6::WidgetsPrivate
)
    if(NOT TARGET ${zz_qwk_target})
        message(FATAL_ERROR
            "ZzWindowKit requires ${zz_qwk_target}; install private development files matching Qt ${Qt6_VERSION} exactly")
    endif()
endforeach()
if(NOT QWINDOWKIT_BUILD_STATIC
   OR NOT QWINDOWKIT_BUILD_WIDGETS
   OR QWINDOWKIT_BUILD_QUICK
   OR QWINDOWKIT_INSTALL)
    message(FATAL_ERROR
        "QWindowKit options conflict with ZzWindowKit; delete this preset build directory and configure again")
endif()
get_target_property(zz_qwk_core_type QWKCore TYPE)
get_target_property(zz_qwk_widgets_type QWKWidgets TYPE)
if(NOT zz_qwk_core_type STREQUAL "STATIC_LIBRARY"
   OR NOT zz_qwk_widgets_type STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR
        "ZzWindowKit requires static QWKCore and QWKWidgets; delete this preset build directory and configure again")
endif()

add_library(ZzWindowKit ${zz_window_kit_sources})
add_library(Zz::WindowKit ALIAS ZzWindowKit)

target_link_libraries(ZzWindowKit
    PUBLIC
        Zz::Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    PRIVATE
        "$<BUILD_INTERFACE:QWindowKit::Core>"
        "$<BUILD_INTERFACE:QWindowKit::Widgets>"
)
if(NOT BUILD_SHARED_LIBS)
    target_link_libraries(ZzWindowKit
        INTERFACE
            "$<INSTALL_INTERFACE:$<LINK_ONLY:ZzPrivate::WindowKitWidgetsBackend>>"
    )
endif()
target_compile_definitions(ZzWindowKit PRIVATE
    "ZZ_WINDOWKIT_FORCE_QT_CONTEXT=$<BOOL:${ZZ_WINDOWKIT_FORCE_QT_CONTEXT}>"
)
if(ZZ_BUILD_BENCHMARKS)
    target_compile_definitions(ZzWindowKit PRIVATE
        ZZ_WINDOWKIT_DIAGNOSTICS=1)
endif()

zz_configure_library_target(ZzWindowKit
    EXPORT_NAME WindowKit
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include"
    EXPORT_HEADER_SUBDIR ZzWindowKit
    EXPORT_HEADER_NAME ZzWindowKitExport.h
    EXPORT_MACRO_NAME ZZ_WINDOWKIT_EXPORT
    SOURCES ${zz_window_kit_sources}
    MOC_HEADERS ${zz_window_kit_moc_headers}
)

if(NOT BUILD_SHARED_LIBS)
    set(zz_qwk_private_install_dir
        "${CMAKE_INSTALL_LIBDIR}/zzpuretoolspro/private"
    )
    set(zz_qwk_core_file
        "ZzWindowKitBackendCore${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
    set(zz_qwk_widgets_file
        "ZzWindowKitBackendWidgets${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )

    set_target_properties(ZzWindowKit PROPERTIES
        ZZ_QWK_PRIVATE_INSTALL_DIR "${zz_qwk_private_install_dir}"
        ZZ_QWK_CORE_FILE "${zz_qwk_core_file}"
        ZZ_QWK_WIDGETS_FILE "${zz_qwk_widgets_file}"
    )

    install(FILES "$<TARGET_FILE:QWKCore>"
        DESTINATION "${zz_qwk_private_install_dir}"
        RENAME "${zz_qwk_core_file}"
        COMPONENT Development
    )
    install(FILES "$<TARGET_FILE:QWKWidgets>"
        DESTINATION "${zz_qwk_private_install_dir}"
        RENAME "${zz_qwk_widgets_file}"
        COMPONENT Development
    )
endif()

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
