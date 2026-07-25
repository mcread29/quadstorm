cmake_minimum_required(VERSION 3.20)

project(stalberg_grid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(raylib 5.0 QUIET)

if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(stalberg_grid main.cpp)
target_link_libraries(stalberg_grid PRIVATE raylib)

if(MSVC)
    target_compile_options(stalberg_grid PRIVATE /W4 /permissive-)
else()
    target_compile_options(stalberg_grid PRIVATE -Wall -Wextra -Wpedantic)
endif()
