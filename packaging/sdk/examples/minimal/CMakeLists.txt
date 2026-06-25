cmake_minimum_required(VERSION 3.21)
project(ZzFluentMinimalConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

# 将 ZZ_FLUENT_SDK_DIR 指向 SDK 根目录（包含 include/lib/bin 的那一层）
if(NOT DEFINED ZZ_FLUENT_SDK_DIR)
    message(FATAL_ERROR "请设置 -DZZ_FLUENT_SDK_DIR=/path/to/sdk")
endif()

list(APPEND CMAKE_PREFIX_PATH "${ZZ_FLUENT_SDK_DIR}")

find_package(Qt6 6.8.0 REQUIRED COMPONENTS Widgets)
find_package(ZzFluent REQUIRED)

if(WIN32)
    add_executable(ZzFluentMinimalConsumer WIN32 main.cpp)
else()
    add_executable(ZzFluentMinimalConsumer main.cpp)
endif()
target_link_libraries(ZzFluentMinimalConsumer PRIVATE Zz::Fluent Qt6::Widgets)
