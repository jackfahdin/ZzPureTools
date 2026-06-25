cmake_minimum_required(VERSION 3.21)

project(ZzFluentExample LANGUAGES CXX)

set(ZZ_EXAMPLE_SOURCES
    main.cpp
    MainWindow.h
    MainWindow.cpp
    backend/ZzDemoViewModel.h
    backend/ZzDemoViewModel.cpp
)

add_executable(ZzFluentExample ${ZZ_EXAMPLE_SOURCES})

target_link_libraries(ZzFluentExample
    PRIVATE
        Zz::FluentWidgets
        Qt6::Widgets
)

target_compile_features(ZzFluentExample PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ZzFluentExample PRIVATE /utf-8)
endif()

set_target_properties(ZzFluentExample PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
)
