cmake_minimum_required(VERSION 3.23)
project(ZzApplicationStarter LANGUAGES CXX)

# This directory can be copied out of the framework repository and built alone.
find_package(ZzPureToolsFrame 0.1 CONFIG REQUIRED)
add_executable(ZzApplicationStarter main.cpp ZzStarterSetup.cpp ZzStarterSmoke.cpp)
target_compile_features(ZzApplicationStarter PRIVATE cxx_std_20)
set_target_properties(ZzApplicationStarter PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(ZzApplicationStarter PRIVATE
    Zz::PureTools Zz::FluentUI Zz::WindowKit)
if(MSVC)
    target_compile_options(ZzApplicationStarter PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(ZzApplicationStarter PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME starter.application COMMAND ZzApplicationStarter --smoke)
    set(starter_platform offscreen)
    set(starter_library_path LD_LIBRARY_PATH)
    if(APPLE)
        set(starter_platform cocoa)
        set(starter_library_path DYLD_LIBRARY_PATH)
    elseif(WIN32)
        set(starter_library_path PATH)
    endif()
    set_tests_properties(starter.application PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "QT_QPA_PLATFORM=${starter_platform}"
        ENVIRONMENT_MODIFICATION
            "${starter_library_path}=path_list_prepend:$<TARGET_FILE_DIR:Zz::Core>;${starter_library_path}=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endif()
