set(zz_core_sources
    src/private/ZzCoreVersion.cpp
    src/ZzError.cpp
    src/private/ZzErrorPrivate.cpp
    src/ZzApplicationPaths.cpp
    src/private/ZzApplicationPathsPrivate.cpp
    src/ZzQtSettingsStore.cpp
    src/private/ZzQtSettingsStorePrivate.cpp
    src/ZzTaskExecutor.cpp
    src/private/ZzTaskExecutorPrivate.cpp
    src/ZzQtLogBridge.cpp
    src/private/ZzQtLogBridgePrivate.cpp
)

add_library(ZzCore ${zz_core_sources})
add_library(Zz::Core ALIAS ZzCore)

target_link_libraries(ZzCore PUBLIC
    Qt6::Core
    Qt6::Concurrent
)
target_link_libraries(ZzCore PRIVATE ZzLog::ZzLog)

get_target_property(zz_core_direct_links ZzCore LINK_LIBRARIES)
get_target_property(zz_core_interface_links ZzCore INTERFACE_LINK_LIBRARIES)
set(zz_core_direct_log_count 0)
foreach(zz_core_link IN LISTS zz_core_direct_links)
    if(zz_core_link MATCHES "^Qt6::(Gui|Widgets|Quick)$")
        message(FATAL_ERROR
            "ZzCore must not link UI target: ${zz_core_link}")
    endif()
    if(zz_core_link STREQUAL "ZzLog::ZzLog")
        math(EXPR zz_core_direct_log_count
            "${zz_core_direct_log_count} + 1")
    endif()
endforeach()
if(NOT zz_core_direct_log_count EQUAL 1)
    message(FATAL_ERROR
        "ZzCore must directly link ZzLog::ZzLog exactly once; found ${zz_core_direct_log_count}")
endif()
foreach(zz_core_link IN LISTS zz_core_interface_links)
    if(zz_core_link MATCHES "^Qt6::(Gui|Widgets|Quick)$")
        message(FATAL_ERROR
            "ZzCore must not expose UI target: ${zz_core_link}")
    endif()
    if(zz_core_link STREQUAL "ZzLog::ZzLog")
        message(FATAL_ERROR
            "ZzCore must not expose a bare ZzLog::ZzLog dependency")
    endif()
endforeach()

zz_configure_library_target(ZzCore
    EXPORT_NAME Core
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include"
    EXPORT_HEADER_SUBDIR ZzCore
    EXPORT_HEADER_NAME ZzCoreExport.h
    EXPORT_MACRO_NAME ZZ_CORE_EXPORT
    SOURCES ${zz_core_sources}
)

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
