cmake_minimum_required(VERSION 3.21)

project(ZzPureToolsExample LANGUAGES CXX)

set(ZZ_EXAMPLE_SOURCES
    main.cpp
    MainWindow.hpp
    MainWindow.cpp
    backend/ZzDemoViewModel.hpp
    backend/ZzDemoViewModel.cpp
)

# WIN32：Windows 下使用 GUI 子系统，避免启动时弹出 cmd 黑窗口
if(WIN32)
    add_executable(ZzPureToolsExample WIN32 ${ZZ_EXAMPLE_SOURCES})
else()
    add_executable(ZzPureToolsExample ${ZZ_EXAMPLE_SOURCES})
endif()

target_link_libraries(ZzPureToolsExample
    PRIVATE
        Zz::PureTools
        Qt6::Widgets
)

target_compile_features(ZzPureToolsExample PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ZzPureToolsExample PRIVATE /utf-8)
endif()
