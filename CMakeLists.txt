cmake_minimum_required(VERSION 3.20)

project(stalberg_grid LANGUAGES CXX)

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

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(stalberg_grid_core src/stalberg_grid.cpp)
target_include_directories(stalberg_grid_core PUBLIC src)
target_compile_features(stalberg_grid_core PUBLIC cxx_std_20)
enable_project_warnings(stalberg_grid_core)

add_executable(stalberg_grid
    src/main.cpp
    src/grid_renderer.cpp
)
target_link_libraries(stalberg_grid PRIVATE stalberg_grid_core raylib)
enable_project_warnings(stalberg_grid)

include(CTest)
if(BUILD_TESTING)
    add_executable(stalberg_grid_tests tests/grid_tests.cpp)
    target_link_libraries(stalberg_grid_tests PRIVATE stalberg_grid_core)
    enable_project_warnings(stalberg_grid_tests)
    add_test(NAME stalberg_grid_tests COMMAND stalberg_grid_tests)
endif()
