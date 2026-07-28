cmake_minimum_required(VERSION 3.20)

project(stalberg_grid LANGUAGES CXX)

if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "raylib target platform" FORCE)
endif()

if(NOT EMSCRIPTEN)
    find_package(raylib 5.0 QUIET)
endif()

if(EMSCRIPTEN OR NOT raylib_FOUND)
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

if(NOT EMSCRIPTEN)
    add_executable(stalberg_grid
        src/main.cpp
        src/grid_renderer.cpp
    )
    target_link_libraries(stalberg_grid PRIVATE stalberg_room_grid_adapter raylib)
    enable_project_warnings(stalberg_grid)
endif()

add_library(stalberg_game_core
    src/game/arena.cpp
    src/game/collision_2d.cpp
    src/game/combat.cpp
    src/game/encounter.cpp
    src/game/enemy.cpp
    src/game/enemy_collection.cpp
    src/game/generated_encounter.cpp
    src/game/generated_level.cpp
    src/game/level_session.cpp
    src/game/player.cpp
    src/game/projectile_pool.cpp
    src/game/target.cpp
    src/game/weapon.cpp
)
target_include_directories(stalberg_game_core PUBLIC src)
target_compile_features(stalberg_game_core PUBLIC cxx_std_20)
target_link_libraries(stalberg_game_core
    PUBLIC stalberg_room_grid_adapter raylib)
enable_project_warnings(stalberg_game_core)
enable_debug_runtime_optimization(stalberg_game_core)

add_executable(stalberg_game
    src/game/combat_audio.cpp
    src/game/game_camera.cpp
    src/game/game_input.cpp
    src/game/main.cpp
    src/game/prototype_renderer.cpp
)
target_link_libraries(stalberg_game PRIVATE stalberg_game_core)
enable_project_warnings(stalberg_game)
enable_debug_runtime_optimization(stalberg_game)

if(EMSCRIPTEN)
    target_compile_definitions(stalberg_game PRIVATE PLATFORM_WEB)
    set_target_properties(stalberg_game PROPERTIES SUFFIX ".html")
    target_link_options(stalberg_game PRIVATE
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sSTACK_SIZE=4MB"
        "-sEXIT_RUNTIME=0"
    )
    add_custom_command(TARGET stalberg_game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/web/favicon.ico
            $<TARGET_FILE_DIR:stalberg_game>/favicon.ico
    )
endif()

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_executable(stalberg_grid_tests tests/grid_tests.cpp)
    target_link_libraries(stalberg_grid_tests PRIVATE stalberg_grid_generation)
    enable_project_warnings(stalberg_grid_tests)
    add_test(NAME stalberg_grid_tests COMMAND stalberg_grid_tests)

    add_executable(stalberg_room_generation_tests tests/room_generation_tests.cpp)
    target_link_libraries(stalberg_room_generation_tests
        PRIVATE stalberg_room_grid_adapter)
    enable_project_warnings(stalberg_room_generation_tests)
    add_test(NAME stalberg_room_generation_tests COMMAND stalberg_room_generation_tests)

    add_executable(stalberg_grid_renderer_cache_tests
        tests/grid_renderer_cache_tests.cpp
        src/grid_renderer.cpp
    )
    target_link_libraries(stalberg_grid_renderer_cache_tests
        PRIVATE stalberg_room_grid_adapter raylib)
    enable_project_warnings(stalberg_grid_renderer_cache_tests)
    add_test(NAME stalberg_grid_renderer_cache_tests
        COMMAND stalberg_grid_renderer_cache_tests)

    add_executable(stalberg_generated_level_tests
        tests/generated_level_tests.cpp)
    target_link_libraries(stalberg_generated_level_tests
        PRIVATE stalberg_game_core)
    enable_project_warnings(stalberg_generated_level_tests)
    add_test(NAME stalberg_generated_level_tests
        COMMAND stalberg_generated_level_tests)

    add_executable(stalberg_game_tests tests/game_tests.cpp)
    target_link_libraries(stalberg_game_tests PRIVATE stalberg_game_core)
    enable_project_warnings(stalberg_game_tests)
    add_test(NAME stalberg_game_tests COMMAND stalberg_game_tests)
endif()
