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

option(STALBERG_OPTIMIZE_DEBUG_RUNTIME
    "Use debugger-friendly optimization for the game runtime" ON)

function(enable_debug_runtime_optimization target)
    if(NOT STALBERG_OPTIMIZE_DEBUG_RUNTIME)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:/O1>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:-Og>)
    endif()
endfunction()

if(TARGET raylib)
    get_target_property(raylib_is_imported raylib IMPORTED)
    if(NOT raylib_is_imported)
        enable_debug_runtime_optimization(raylib)
    endif()
endif()

add_library(stalberg_grid_generation
    src/grid/dual_grid.cpp
    src/grid/stalberg_grid.cpp
)
target_include_directories(stalberg_grid_generation PUBLIC src)
target_compile_features(stalberg_grid_generation PUBLIC cxx_std_20)
enable_project_warnings(stalberg_grid_generation)

add_library(stalberg_room_generation
    src/rooms/room_generator.cpp
    src/rooms/room_layout.cpp
)
target_include_directories(stalberg_room_generation PUBLIC src)
target_compile_features(stalberg_room_generation PUBLIC cxx_std_20)
enable_project_warnings(stalberg_room_generation)

add_library(stalberg_room_grid_adapter src/integration/room_grid_adapter.cpp)
target_include_directories(stalberg_room_grid_adapter PUBLIC src)
target_compile_features(stalberg_room_grid_adapter PUBLIC cxx_std_20)
target_link_libraries(stalberg_room_grid_adapter
    PUBLIC stalberg_grid_generation stalberg_room_generation)
enable_project_warnings(stalberg_room_grid_adapter)

add_executable(stalberg_grid
    src/main.cpp
    src/grid_renderer.cpp
)
target_link_libraries(stalberg_grid PRIVATE stalberg_room_grid_adapter raylib)
enable_project_warnings(stalberg_grid)

add_executable(stalberg_game
    src/game/arena.cpp
    src/game/collision_2d.cpp
    src/game/game_camera.cpp
    src/game/game_input.cpp
    src/game/main.cpp
    src/game/player.cpp
    src/game/projectile_pool.cpp
    src/game/prototype_renderer.cpp
    src/game/target.cpp
    src/game/weapon.cpp
)
target_compile_features(stalberg_game PRIVATE cxx_std_20)
target_link_libraries(stalberg_game PRIVATE raylib)
enable_project_warnings(stalberg_game)
enable_debug_runtime_optimization(stalberg_game)

include(CTest)
if(BUILD_TESTING)
    add_executable(stalberg_grid_tests tests/grid_tests.cpp)
    target_link_libraries(stalberg_grid_tests PRIVATE stalberg_grid_generation)
    enable_project_warnings(stalberg_grid_tests)
    add_test(NAME stalberg_grid_tests COMMAND stalberg_grid_tests)

    add_executable(stalberg_room_generation_tests tests/room_generation_tests.cpp)
    target_link_libraries(stalberg_room_generation_tests
        PRIVATE stalberg_room_grid_adapter)
    enable_project_warnings(stalberg_room_generation_tests)
    add_test(NAME stalberg_room_generation_tests COMMAND stalberg_room_generation_tests)

    add_executable(stalberg_game_tests
        tests/game_tests.cpp
        src/game/arena.cpp
        src/game/collision_2d.cpp
        src/game/projectile_pool.cpp
        src/game/weapon.cpp
    )
    target_include_directories(stalberg_game_tests PRIVATE src)
    target_compile_features(stalberg_game_tests PRIVATE cxx_std_20)
    target_link_libraries(stalberg_game_tests PRIVATE raylib)
    enable_project_warnings(stalberg_game_tests)
    add_test(NAME stalberg_game_tests COMMAND stalberg_game_tests)
endif()
