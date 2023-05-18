cmake_minimum_required(VERSION 3.10)
project(PhysWikiScan)

set(opt_debug ON CACHE BOOL "debug mode")

set(CMAKE_CXX_COMPILER "/usr/bin/g++")

set(CMAKE_CXX_FLAGS "-std=c++11")

include_directories(/home/addis/github/SLISC-libs-x64-ubuntu22.04/sqlitecpp-3.2.1/include)
link_directories(/home/addis/github/SLISC-libs-x64-ubuntu22.04/sqlitecpp-3.2.1/lib)

list(APPEND LIBS SQLiteCpp SQLiteCpp-sqlite3)

# Address Sanitizer
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address -static-libasan")
# SQLite
# list(APPEND LIBS sqlite3)
if (opt_debug)
    # Debug
    add_compile_options(-g -ftrapv)
else()
    # Release
    add_compile_options(-D NDEBUG)
    add_compile_options(-O3)
    add_compile_options(-Wall -Wno-reorder -Wno-misleading-indentation -fopenmp -fmax-errors=2)
endif()

add_executable(PhysWikiScan main.cpp)

target_link_libraries(PhysWikiScan ${LIBS})
