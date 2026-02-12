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
include_directories(${PROJECT_SOURCE_DIR}/deps/nlohmann_json/include)

# choose a compatible prebuilt SQLiteCpp library directory
set(SQLITECPP_LIB_DIR "")
if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(SQLITECPP_LIB_DIR ${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-arm64-mac/)
    endif()
elseif(UNIX AND NOT APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(SQLITECPP_LIB_18 ${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-x64-ubuntu18.04/)
        set(SQLITECPP_LIB_20 ${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-x64-ubuntu20.04/)
        set(SQLITECPP_LIB_22 ${PROJECT_SOURCE_DIR}/deps/SQLiteCpp/lib-x64-ubuntu22.04/)

        execute_process(
            COMMAND getconf GNU_LIBC_VERSION
            OUTPUT_VARIABLE GLIBC_VERSION_STR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        string(REGEX MATCH "[0-9]+\\.[0-9]+" GLIBC_VERSION ${GLIBC_VERSION_STR})

        if(GLIBC_VERSION)
            if(GLIBC_VERSION VERSION_GREATER_EQUAL "2.35" AND EXISTS ${SQLITECPP_LIB_22})
                set(SQLITECPP_LIB_DIR ${SQLITECPP_LIB_22})
            elseif(GLIBC_VERSION VERSION_GREATER_EQUAL "2.31" AND EXISTS ${SQLITECPP_LIB_20})
                set(SQLITECPP_LIB_DIR ${SQLITECPP_LIB_20})
            elseif(EXISTS ${SQLITECPP_LIB_18})
                set(SQLITECPP_LIB_DIR ${SQLITECPP_LIB_18})
            endif()
            message(STATUS "检测到 glibc ${GLIBC_VERSION}，使用 SQLiteCpp 库目录：${SQLITECPP_LIB_DIR}")
        endif()

        if(NOT SQLITECPP_LIB_DIR)
            if(EXISTS ${SQLITECPP_LIB_18})
                set(SQLITECPP_LIB_DIR ${SQLITECPP_LIB_18})
                message(WARNING "未检测到 glibc 版本，回退到：${SQLITECPP_LIB_DIR}")
            endif()
        endif()
    endif()
endif()

if(NOT SQLITECPP_LIB_DIR)
    message(FATAL_ERROR "无法自动匹配 SQLiteCpp 预编译库，请手动设置 link_directories")
endif()

link_directories(${SQLITECPP_LIB_DIR})
find_package(Threads REQUIRED)
set(THREAD_LIBS Threads::Threads)
if(UNIX AND NOT APPLE)
    list(APPEND THREAD_LIBS pthread)
endif()

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
set_target_properties(PhysWikiScan PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

target_link_libraries(PhysWikiScan SQLiteCpp SQLiteCpp-sqlite3 ${THREAD_LIBS} dl)

# local test utilities
add_subdirectory(test)
