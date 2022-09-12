cmake_minimum_required(VERSION 3.10)
project(PhysWikiScan)

set(CMAKE_CXX_COMPILER "/usr/bin/g++")

set(CMAKE_CXX_FLAGS "-std=c++11")

# include_directories(SLISC)

# Boost
# add_definitions(-D SLS_USE_BOOST)
# include_directories(../boost-headers)
# list(APPEND LIBS boost_system boost_filesystem)

# Address Sanitizer
add_definitions(-D SLS_USE_ASAN)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -static-libasan")
# SQLite
# add_definitions(-D SLS_USE_SQLITE)
# list(APPEND LIBS sqlite3)
# Debug
add_compile_options(-g -ftrapv)
# Release
# add_definitions(-D NDEBUG)
# add_compile_options(-O3)

add_compile_options(-Wall -Wno-reorder -Wno-misleading-indentation -fopenmp -fmax-errors=2)

add_executable(PhysWikiScan main.cpp)
