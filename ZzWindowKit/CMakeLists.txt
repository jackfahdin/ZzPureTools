set(zz_window_kit_sources
    src/private/ZzWindowKitVersion.cpp
    src/ZzWindowAgent.cpp
    src/private/ZzWindowAgentPrivate.cpp
    src/ZzWindowKitBootstrap.cpp
)
set(zz_window_kit_moc_headers
    include/ZzWindowKit/ZzWindowAgent.h
)

add_library(ZzWindowKit ${zz_window_kit_sources})
add_library(Zz::WindowKit ALIAS ZzWindowKit)

target_link_libraries(ZzWindowKit PUBLIC
    Zz::Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

zz_configure_library_target(ZzWindowKit
    EXPORT_NAME WindowKit
    PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include"
    EXPORT_HEADER_SUBDIR ZzWindowKit
    EXPORT_HEADER_NAME ZzWindowKitExport.h
    EXPORT_MACRO_NAME ZZ_WINDOWKIT_EXPORT
    SOURCES ${zz_window_kit_sources}
    MOC_HEADERS ${zz_window_kit_moc_headers}
)

if(ZZ_BUILD_TESTS)
    add_subdirectory(tests)
endif()
