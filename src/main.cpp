#include "grid_renderer.hpp"
#include "grid/stalberg_grid.hpp"
#include "integration/room_grid_adapter.hpp"
#include "rooms/room_generator.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>

namespace {

constexpr int INITIAL_RADIUS = 6;
constexpr int MIN_RADIUS = 2;
constexpr int MAX_RADIUS = 14;
constexpr std::uint32_t INITIAL_SEED = 1;

const char* roomMethodName(stalberg::rooms::RoomGenerationMethod method)
{
    switch (method) {
    case stalberg::rooms::RoomGenerationMethod::BranchingShapes:
        return "branching shapes";
    case stalberg::rooms::RoomGenerationMethod::OrganicGrowth:
        return "organic growth";
    }
    return "unknown";
}

void generateRelaxedGrid(
    stalberg::StalbergGrid& grid, int radius, std::uint32_t seed)
{
    grid.generate(radius, seed);
    grid.relaxToCompletion();
}

void fitCamera(Camera2D& camera, const stalberg::StalbergGrid& grid)
{
    const stalberg::Bounds bounds = grid.getBounds();
    camera.target = Vector2 {
        (bounds.minimum.x + bounds.maximum.x) * 0.5F,
        (bounds.minimum.y + bounds.maximum.y) * 0.5F
    };

    const float gridWidth = bounds.maximum.x - bounds.minimum.x;
    const float gridHeight = bounds.maximum.y - bounds.minimum.y;
    const float horizontalZoom = (static_cast<float>(GetScreenWidth()) - 90.0F) / gridWidth;
    const float verticalZoom = (static_cast<float>(GetScreenHeight()) - 150.0F) / gridHeight;
    camera.zoom = std::clamp(std::min(horizontalZoom, verticalZoom), 0.15F, 2.5F);
}

void handleCamera(Camera2D& camera, const stalberg::StalbergGrid& grid)
{
    camera.offset = Vector2 {
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.5F
    };

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const Vector2 delta = GetMouseDelta();
        camera.target.x -= delta.x / camera.zoom;
        camera.target.y -= delta.y / camera.zoom;
    }

    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0F) {
        const Vector2 mouse = GetMousePosition();
        const Vector2 before = GetScreenToWorld2D(mouse, camera);
        camera.zoom = std::clamp(camera.zoom * std::pow(1.15F, wheel), 0.1F, 5.0F);
        const Vector2 after = GetScreenToWorld2D(mouse, camera);
        camera.target.x += before.x - after.x;
        camera.target.y += before.y - after.y;
    }

    if (IsKeyPressed(KEY_F)) {
        fitCamera(camera, grid);
    }
}

} // namespace

int main()
{
#if defined(__linux__)
    const char* display = std::getenv("DISPLAY");
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    if ((display == nullptr || display[0] == '\0')
        && (waylandDisplay == nullptr || waylandDisplay[0] == '\0')) {
        std::fprintf(stderr,
            "Unable to open the raylib window: no graphical display is available.\n"
            "DISPLAY and WAYLAND_DISPLAY are both unset.\n\n"
            "Run this program from a terminal inside a desktop session, use SSH X11\n"
            "forwarding (ssh -X), or run it in a VNC/RDP desktop. For a non-visible\n"
            "smoke test only, use: xvfb-run -a ./build/stalberg_grid\n");
        return EXIT_FAILURE;
    }
#endif

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Stalberg quad grid");
    if (!IsWindowReady()) {
        std::fprintf(stderr,
            "raylib could not create a window. Check that your display server is running\n"
            "and that DISPLAY or WAYLAND_DISPLAY points to it.\n");
        return EXIT_FAILURE;
    }
    SetTargetFPS(60);

    stalberg::StalbergGrid grid;
    int radius = INITIAL_RADIUS;
    std::uint32_t seed = INITIAL_SEED;
    std::uint32_t roomSeed = INITIAL_SEED;
    stalberg::rooms::RoomGenerationMethod roomMethod
        = stalberg::rooms::RoomGenerationMethod::BranchingShapes;
    bool drawCenters = true;
    generateRelaxedGrid(grid, radius, seed);
    stalberg::rooms::RoomGenerator roomGenerator;
    stalberg::rooms::RoomGrid roomInput = stalberg::makeRoomGrid(grid);
    stalberg::rooms::RoomLayout rooms
        = roomGenerator.generate(roomInput, roomSeed, roomMethod);

    Camera2D camera {};
    camera.offset = Vector2 { 640.0F, 400.0F };
    camera.rotation = 0.0F;
    fitCamera(camera, grid);

    while (!WindowShouldClose()) {
        bool regenerateRequested = false;
        bool refitRequested = false;

        if (IsKeyPressed(KEY_R)) {
            ++seed;
            regenerateRequested = true;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            seed = seed > INITIAL_SEED ? seed - 1 : INITIAL_SEED;
            regenerateRequested = true;
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            ++seed;
            regenerateRequested = true;
        }
        if (IsKeyPressed(KEY_UP) && radius < MAX_RADIUS) {
            ++radius;
            regenerateRequested = true;
            refitRequested = true;
        }
        if (IsKeyPressed(KEY_DOWN) && radius > MIN_RADIUS) {
            --radius;
            regenerateRequested = true;
            refitRequested = true;
        }

        if (regenerateRequested) {
            generateRelaxedGrid(grid, radius, seed);
            roomInput = stalberg::makeRoomGrid(grid);
            rooms = roomGenerator.generate(roomInput, roomSeed, roomMethod);
            if (refitRequested) {
                fitCamera(camera, grid);
            }
        }

        if (IsKeyPressed(KEY_P)) {
            drawCenters = !drawCenters;
        }
        if (IsKeyPressed(KEY_G)) {
            ++roomSeed;
            rooms = roomGenerator.generate(roomInput, roomSeed, roomMethod);
        }
        if (IsKeyPressed(KEY_M)) {
            roomMethod = roomMethod
                    == stalberg::rooms::RoomGenerationMethod::BranchingShapes
                ? stalberg::rooms::RoomGenerationMethod::OrganicGrowth
                : stalberg::rooms::RoomGenerationMethod::BranchingShapes;
            rooms = roomGenerator.generate(roomInput, roomSeed, roomMethod);
        }

        handleCamera(camera, grid);

        const Vector2 mouseScreen = GetMousePosition();
        const bool mouseOverHud = mouseScreen.x >= 14.0F && mouseScreen.x <= 454.0F
            && mouseScreen.y >= 14.0F && mouseScreen.y <= 145.0F;
        std::optional<std::size_t> hoveredCell;
        if (!mouseOverHud) {
            const Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);
            hoveredCell = stalberg::findDualCellAtPoint(
                grid, stalberg::Point { mouseWorld.x, mouseWorld.y });
        }

        BeginDrawing();
        ClearBackground(Color { 47, 121, 137, 255 });

        BeginMode2D(camera);
        stalberg::drawGrid(grid, rooms, drawCenters, camera.zoom, hoveredCell);
        EndMode2D();

        DrawRectangle(14, 14, 440, 131, Color { 8, 31, 38, 220 });
        DrawText("OSKAR STALBERG-STYLE QUAD GRID", 26, 24, 20, Color { 222, 235, 232, 255 });
        DrawText(TextFormat("radius %d   seed %u   vertices %zu   quads %zu",
                     grid.getRadius(), grid.getSeed(), grid.getVertexCount(), grid.getQuadCount()),
            26, 52, 16, Color { 150, 178, 181, 255 });
        DrawText(TextFormat("relaxation %d/%d   rooms %zu", grid.getRelaxationSteps(),
                     stalberg::MAX_RELAXATION_STEPS, rooms.getRoomCount()),
            26, 74, 16, Color { 239, 180, 74, 255 });
        DrawText(TextFormat("G/new rooms  seed %u  doors %zu  quality %.1f  pick %zu",
                     rooms.getSeed(), rooms.getDoorways().size(),
                     rooms.getQualityScore(), rooms.getSelectedCandidate()),
            26, 98, 14, Color { 196, 225, 223, 255 });
        DrawText("R/new grid  arrows/seed+size  G/new layout  M/method", 26, 120, 14,
            Color { 150, 178, 181, 255 });
        DrawText(TextFormat("method: %s", roomMethodName(rooms.getMethod())),
            470, 20, 16, Color { 226, 240, 224, 230 });
        DrawText("P/centers  F/fit  wheel/zoom  middle or right drag/pan", 20,
            GetScreenHeight() - 27, 14, Color { 25, 75, 87, 255 });

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
