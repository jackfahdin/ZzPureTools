cmake_minimum_required(VERSION 3.21)

project(ZzFluentCore LANGUAGES CXX)

set(ZZ_CORE_PUBLIC_HEADERS
    include/ZzFluentCoreGlobal.h
    include/ZzDesignTokens.h
    include/ZzTheme.h
    include/ZzFluentPainter.h
    include/ZzApplication.h
)

set(ZZ_CORE_SOURCES
    src/ZzTheme.cpp
    src/ZzFluentPainter.cpp
    src/ZzApplication.cpp
)

if(ZZ_BUILD_SHARED_LIBS)
    add_library(ZzFluentCore SHARED ${ZZ_CORE_PUBLIC_HEADERS} ${ZZ_CORE_SOURCES})
    target_compile_definitions(ZzFluentCore PRIVATE ZZ_FLUENT_CORE_LIBRARY)
else()
    add_library(ZzFluentCore STATIC ${ZZ_CORE_PUBLIC_HEADERS} ${ZZ_CORE_SOURCES})
endif()

add_library(Zz::FluentCore ALIAS ZzFluentCore)

target_include_directories(ZzFluentCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ZzFluentCore>
)

target_link_libraries(ZzFluentCore
    PUBLIC
        Qt6::Widgets
)

target_compile_features(ZzFluentCore PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(ZzFluentCore PRIVATE /utf-8)
endif()

if(ZZ_INSTALL)
    install(
        TARGETS ZzFluentCore
        EXPORT ZzFluentTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        FILES ${ZZ_CORE_PUBLIC_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ZzFluentCore
    )
endif()
