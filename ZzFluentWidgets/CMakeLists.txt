cmake_minimum_required(VERSION 3.21)

project(ZzFluentWidgets LANGUAGES CXX)

set(ZZ_WIDGETS_PUBLIC_HEADERS
    include/ZzFluentWidgetsGlobal.h
    include/ZzPushButton.h
    include/ZzComboBox.h
)

set(ZZ_WIDGETS_SOURCES
    src/ZzPushButton.cpp
    src/ZzComboBox.cpp
    src/private/ZzPushButtonPrivate.h
    src/private/ZzComboBoxPrivate.h
)

if(ZZ_BUILD_SHARED_LIBS)
    add_library(ZzFluentWidgets SHARED ${ZZ_WIDGETS_PUBLIC_HEADERS} ${ZZ_WIDGETS_SOURCES})
    target_compile_definitions(ZzFluentWidgets PRIVATE ZZ_FLUENT_WIDGETS_LIBRARY)
else()
    add_library(ZzFluentWidgets STATIC ${ZZ_WIDGETS_PUBLIC_HEADERS} ${ZZ_WIDGETS_SOURCES})
endif()

add_library(Zz::FluentWidgets ALIAS ZzFluentWidgets)

target_include_directories(ZzFluentWidgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ZzFluentWidgets>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/private
)

target_link_libraries(ZzFluentWidgets
    PUBLIC
        Zz::FluentCore
        Qt6::Widgets
)

target_compile_features(ZzFluentWidgets PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(ZzFluentWidgets PRIVATE /utf-8)
endif()

if(ZZ_INSTALL)
    install(
        TARGETS ZzFluentWidgets
        EXPORT ZzFluentTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(
        FILES ${ZZ_WIDGETS_PUBLIC_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ZzFluentWidgets
    )
endif()
