cmake_minimum_required(VERSION 3.21)

project(ZzFluentExample LANGUAGES CXX)

set(ZZ_EXAMPLE_SOURCES
    main.cpp
    MainWindow.hpp
    MainWindow.cpp
    backend/ZzDemoViewModel.hpp
    backend/ZzDemoViewModel.cpp
)

add_executable(ZzFluentExample ${ZZ_EXAMPLE_SOURCES})

target_link_libraries(ZzFluentExample
    PRIVATE
        Zz::Fluent
        Qt6::Widgets
)

target_compile_features(ZzFluentExample PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ZzFluentExample PRIVATE /utf-8)
endif()
