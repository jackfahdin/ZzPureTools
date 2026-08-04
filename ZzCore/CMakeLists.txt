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
)

add_library(ZzCore ${zz_core_sources})
add_library(Zz::Core ALIAS ZzCore)

target_link_libraries(ZzCore PUBLIC
    Qt6::Core
    Qt6::Concurrent
)
target_link_libraries(ZzCore PRIVATE ZzLog::ZzLog)

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
