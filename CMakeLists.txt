cmake_minimum_required(VERSION 3.10)
project(PhysWikiScan)

# 设置 CMAKE_BUILD_TYPE (默认 Debug)
# Debug 会打开调试选项，Release 会启用各种优化
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "编译模式：Debug")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "编译模式：Release")
elseif (NOT CMAKE_BUILD_TYPE) # 未定义
    message(STATUS "编译模式：Debug （CMAKE_BUILD_TYPE 未设置，默认）")
    set(CMAKE_BUILD_TYPE "Debug")
else()
    message(FATAL_ERROR "CMAKE_BUILD_TYPE 必须是 Debug 或 Release 之一。")
endif()

set(OPT_ASAN "OFF" CACHE STRING "使用 Address Sanitizer")
set(OPT_STATIC "ON" CACHE STRING "使用静态库编译")

# 编译器设置
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-fmax-errors=5)

include_directories(${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/include)

# choose the suitable OS
# link_directories(${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-x64-ubuntu18.04/)
# link_directories(${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-x64-ubuntu20.04/)
link_directories(${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-x64-ubuntu22.04/)
# link_directories(${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-arm64-mac/)

# Address Sanitizer
if(OPT_ASAN AND UNIX AND NOT APPLE AND ${CMAKE_CXX_COMPILER} STREQUAL "g++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address -pthread")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -static-libasan")
endif()

# SQLite
# list(APPEND LIBS sqlite3)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug
    add_compile_options(-g -ftrapv)
else()
    # Release
    add_compile_options(-D NDEBUG)
    add_compile_options(-O3)
    add_compile_options(-Wall -Wno-reorder -Wno-misleading-indentation)
endif()

add_executable(PhysWikiScan main.cpp)

target_link_libraries(PhysWikiScan SQLiteCpp SQLiteCpp-sqlite3 dl)
