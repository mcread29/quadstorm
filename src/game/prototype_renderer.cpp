#include "prototype_renderer.hpp"

#include "arena.hpp"
#include "directional_shader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

constexpr float WALL_HEIGHT = 2.0F;
constexpr float WALL_THICKNESS = 0.18F;
constexpr Color GENERATED_BACKGROUND { 15, 23, 31, 255 };
constexpr Color ARENA_BACKGROUND { 18, 27, 34, 255 };

Color roomRoleColor(stalberg::rooms::RoomRole role)
{
    switch (role) {
    case stalberg::rooms::RoomRole::Start:
        return Color { 72, 125, 116, 255 };
    case stalberg::rooms::RoomRole::Combat:
        return Color { 76, 99, 128, 255 };
    case stalberg::rooms::RoomRole::Connector:
        return Color { 64, 89, 104, 255 };
    case stalberg::rooms::RoomRole::Hub:
        return Color { 75, 116, 123, 255 };
    case stalberg::rooms::RoomRole::Reward:
        return Color { 128, 111, 73, 255 };
    case stalberg::rooms::RoomRole::Exit:
        return Color { 91, 125, 91, 255 };
    }
    return Color { 70, 95, 105, 255 };
}

const char* roomRoleName(stalberg::rooms::RoomRole role)
{
    switch (role) {
    case stalberg::rooms::RoomRole::Start:
        return "START";
    case stalberg::rooms::RoomRole::Combat:
        return "COMBAT";
    case stalberg::rooms::RoomRole::Connector:
        return "PASSAGE";
    case stalberg::rooms::RoomRole::Hub:
        return "HUB";
    case stalberg::rooms::RoomRole::Reward:
        return "REWARD";
    case stalberg::rooms::RoomRole::Exit:
        return "EXIT";
    }
    return "ROOM";
}

const stalberg::rooms::GeneratedRoom* findRoom(
    const GeneratedLevel& level, int region)
{
    const auto rooms = level.roomLayout().getRooms();
    const auto room = std::ranges::find(rooms, region,
        &stalberg::rooms::GeneratedRoom::id);
    return room == rooms.end() ? nullptr : &*room;
}

Color generatedFloorColor(const GeneratedLevel& level,
    int region, stalberg::rooms::CellIndex cell)
{
    const auto* room = findRoom(level, region);
    const Color base = room != nullptr
        ? roomRoleColor(room->role)
        : Color { 70, 95, 105, 255 };
    std::uint32_t hash = static_cast<std::uint32_t>(cell) * 747796405U
        + 2891336453U;
    hash ^= hash >> 16U;
    const float variation = 0.95F
        + static_cast<float>(hash & 255U) / 255.0F * 0.10F;
    return Color {
        static_cast<unsigned char>(std::clamp(base.r * variation, 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(base.g * variation, 0.0F, 255.0F)),
        static_cast<unsigned char>(std::clamp(base.b * variation, 0.0F, 255.0F)),
        255
    };
}

Mesh makeGeneratedFloorMesh(const GeneratedLevel& level)
{
    const auto triangles = level.floorTriangles();
    Mesh mesh {};
    mesh.triangleCount = static_cast<int>(triangles.size());
    mesh.vertexCount = mesh.triangleCount * 3;
    mesh.vertices = static_cast<float*>(MemAlloc(
        static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(
        static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(
        static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(
        static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(unsigned char))));

    std::size_t vertex = 0;
    for (const FloorTriangle& triangle : triangles) {
        const Vector2 points[] { triangle.first, triangle.second, triangle.third };
        const Color color = generatedFloorColor(
            level, triangle.region, triangle.cell);
        for (const Vector2 point : points) {
            const std::size_t positionOffset = vertex * 3;
            mesh.vertices[positionOffset] = point.x;
            mesh.vertices[positionOffset + 1] = 0.0F;
            mesh.vertices[positionOffset + 2] = point.y;
            mesh.normals[positionOffset] = 0.0F;
            mesh.normals[positionOffset + 1] = 1.0F;
            mesh.normals[positionOffset + 2] = 0.0F;

            const std::size_t textureOffset = vertex * 2;
            mesh.texcoords[textureOffset] = point.x * 0.1F;
            mesh.texcoords[textureOffset + 1] = point.y * 0.1F;

            const std::size_t colorOffset = vertex * 4;
            mesh.colors[colorOffset] = color.r;
            mesh.colors[colorOffset + 1] = color.g;
            mesh.colors[colorOffset + 2] = color.b;
            mesh.colors[colorOffset + 3] = color.a;
            ++vertex;
        }
    }

    UploadMesh(&mesh, false);
    return mesh;
}

Mesh makeWallMesh(std::span<const Segment2D> walls)
{
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    vertices.reserve(walls.size() * 36U * 3U);
    normals.reserve(walls.size() * 36U * 3U);
    texcoords.reserve(walls.size() * 36U * 2U);
    colors.reserve(walls.size() * 36U * 4U);

    const auto appendVertex = [&](Vector3 point, Vector3 normal, Color color) {
        vertices.insert(vertices.end(), { point.x, point.y, point.z });
        normals.insert(normals.end(), { normal.x, normal.y, normal.z });
        texcoords.insert(texcoords.end(), { 0.0F, 0.0F });
        colors.insert(colors.end(), { color.r, color.g, color.b, color.a });
    };
    const auto appendQuad = [&](Vector3 first, Vector3 second,
                                Vector3 third, Vector3 fourth,
                                Vector3 normal, Color color) {
        appendVertex(first, normal, color);
        appendVertex(second, normal, color);
        appendVertex(third, normal, color);
        appendVertex(first, normal, color);
        appendVertex(third, normal, color);
        appendVertex(fourth, normal, color);
    };

    constexpr Color topColor { 116, 133, 137, 255 };
    constexpr Color sideColor { 76, 91, 98, 255 };
    for (const Segment2D& wall : walls) {
        const float x = wall.end.x - wall.start.x;
        const float z = wall.end.y - wall.start.y;
        const float length = std::sqrt(x * x + z * z);
        if (length <= 0.0001F) {
            continue;
        }
        const Vector3 along { x / length, 0.0F, z / length };
        const Vector3 side {
            -along.z * WALL_THICKNESS * 0.5F,
            0.0F,
            along.x * WALL_THICKNESS * 0.5F
        };
        const Vector3 a0 { wall.start.x + side.x, 0.0F,
            wall.start.y + side.z };
        const Vector3 b0 { wall.end.x + side.x, 0.0F,
            wall.end.y + side.z };
        const Vector3 c0 { wall.end.x - side.x, 0.0F,
            wall.end.y - side.z };
        const Vector3 d0 { wall.start.x - side.x, 0.0F,
            wall.start.y - side.z };
        const Vector3 a1 { a0.x, WALL_HEIGHT, a0.z };
        const Vector3 b1 { b0.x, WALL_HEIGHT, b0.z };
        const Vector3 c1 { c0.x, WALL_HEIGHT, c0.z };
        const Vector3 d1 { d0.x, WALL_HEIGHT, d0.z };
        const Vector3 leftNormal { side.x / (WALL_THICKNESS * 0.5F),
            0.0F, side.z / (WALL_THICKNESS * 0.5F) };
        const Vector3 rightNormal {
            -leftNormal.x, 0.0F, -leftNormal.z };

        appendQuad(a1, b1, c1, d1, Vector3 { 0.0F, 1.0F, 0.0F }, topColor);
        appendQuad(d0, c0, b0, a0, Vector3 { 0.0F, -1.0F, 0.0F }, sideColor);
        appendQuad(a0, b0, b1, a1, leftNormal, sideColor);
        appendQuad(d0, d1, c1, c0, rightNormal, sideColor);
        appendQuad(b0, c0, c1, b1, along, sideColor);
        appendQuad(d0, a0, a1, d1,
            Vector3 { -along.x, 0.0F, -along.z }, sideColor);
    }

    Mesh mesh {};
    mesh.vertexCount = static_cast<int>(vertices.size() / 3U);
    mesh.triangleCount = mesh.vertexCount / 3;
    mesh.vertices = static_cast<float*>(MemAlloc(
        static_cast<unsigned int>(vertices.size() * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(
        static_cast<unsigned int>(normals.size() * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(
        static_cast<unsigned int>(texcoords.size() * sizeof(float))));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(
        static_cast<unsigned int>(colors.size() * sizeof(unsigned char))));
    std::ranges::copy(vertices, mesh.vertices);
    std::ranges::copy(normals, mesh.normals);
    std::ranges::copy(texcoords, mesh.texcoords);
    std::ranges::copy(colors, mesh.colors);
    UploadMesh(&mesh, false);
    return mesh;
}

Texture2D makeProjectileGlow()
{
    Image image = GenImageGradientRadial(
        32, 32, 0.12F, WHITE, Color { 255, 255, 255, 0 });
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

Shader loadDirectionalShader()
{
    Shader shader = LoadShaderFromMemory(
        LIGHTING_VERTEX_SHADER, LIGHTING_FRAGMENT_SHADER);
    shader.locs[SHADER_LOC_MATRIX_MODEL]
        = GetShaderLocation(shader, "matModel");
    const int lightDirectionLocation = GetShaderLocation(shader, "lightDirection");
    const int lightColorLocation = GetShaderLocation(shader, "lightColor");
    const int groundAmbientLocation = GetShaderLocation(shader, "groundAmbient");
    const int skyAmbientLocation = GetShaderLocation(shader, "skyAmbient");
    const float lightDirection[3] {
        DIRECTIONAL_LIGHT.x,
        DIRECTIONAL_LIGHT.y,
        DIRECTIONAL_LIGHT.z
    };
    const float lightColor[3] { 0.72F, 0.66F, 0.56F };
    const float groundAmbient[3] { 0.11F, 0.14F, 0.17F };
    const float skyAmbient[3] { 0.28F, 0.33F, 0.37F };
    SetShaderValue(shader, lightDirectionLocation,
        lightDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, lightColorLocation,
        lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, groundAmbientLocation,
        groundAmbient, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, skyAmbientLocation,
        skyAmbient, SHADER_UNIFORM_VEC3);
    return shader;
}

struct OverviewTransform {
    Vector2 worldCenter {};
    Vector2 screenCenter {};
    float scale = 1.0F;
};

OverviewTransform makeOverviewTransform(
    const GeneratedLevel& level, Rectangle bounds)
{
    Vector2 minimum {
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity()
    };
    Vector2 maximum {
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };
    for (const FloorTriangle& triangle : level.floorTriangles()) {
        for (const Vector2 point
            : { triangle.first, triangle.second, triangle.third }) {
            minimum.x = std::min(minimum.x, point.x);
            minimum.y = std::min(minimum.y, point.y);
            maximum.x = std::max(maximum.x, point.x);
            maximum.y = std::max(maximum.y, point.y);
        }
    }

    const float worldWidth = std::max(maximum.x - minimum.x, 0.001F);
    const float worldHeight = std::max(maximum.y - minimum.y, 0.001F);
    return OverviewTransform {
        Vector2 {
            (minimum.x + maximum.x) * 0.5F,
            (minimum.y + maximum.y) * 0.5F
        },
        Vector2 {
            bounds.x + bounds.width * 0.5F,
            bounds.y + bounds.height * 0.5F
        },
        std::min((bounds.width - 80.0F) / worldWidth,
            (bounds.height - 80.0F) / worldHeight)
    };
}

Vector2 overviewPoint(Vector2 point, const OverviewTransform& transform)
{
    return Vector2 {
        transform.screenCenter.x
            + (point.x - transform.worldCenter.x) * transform.scale,
        transform.screenCenter.y
            - (point.y - transform.worldCenter.y) * transform.scale
    };
}

std::vector<Vector2> roomCenters(const GeneratedLevel& level)
{
    std::vector<Vector2> centers(level.roomLayout().getRoomCount());
    std::vector<std::size_t> counts(centers.size());
    const auto assignments = level.roomLayout().getCellAssignments();
    for (stalberg::rooms::CellIndex cell = 0; cell < assignments.size(); ++cell) {
        const int room = assignments[cell];
        if (room == stalberg::rooms::EMPTY_CELL) {
            continue;
        }
        const stalberg::Point center = level.dualGrid().cells[cell].center;
        centers[static_cast<std::size_t>(room)].x
            += center.x * level.worldScale();
        centers[static_cast<std::size_t>(room)].y
            += center.y * level.worldScale();
        ++counts[static_cast<std::size_t>(room)];
    }
    for (std::size_t room = 0; room < centers.size(); ++room) {
        if (counts[room] == 0) {
            continue;
        }
        centers[room].x /= static_cast<float>(counts[room]);
        centers[room].y /= static_cast<float>(counts[room]);
    }
    return centers;
}

const char* baselineGeometryLabel(stalberg::rooms::RoomRole role)
{
    return role == stalberg::rooms::RoomRole::Connector
        ? "ROUTED"
        : "BASE COMPACT";
}

const char* baselineTopologyLabel(const GeneratedLevel& level)
{
    const std::size_t rooms = level.roomLayout().getRoomCount();
    const std::size_t doors = level.roomLayout().getDoorways().size();
    return doors >= rooms ? "SPATIAL TREE + LOOP" : "SPATIAL TREE";
}

void drawOverviewLandmark(Vector2 center,
    stalberg::rooms::RoomRole role, Color color)
{
    int sides = 0;
    float radius = 0.0F;
    float rotation = 0.0F;
    switch (role) {
    case stalberg::rooms::RoomRole::Start:
        sides = 3;
        radius = 13.0F;
        rotation = -90.0F;
        break;
    case stalberg::rooms::RoomRole::Hub:
        sides = 6;
        radius = 14.0F;
        break;
    case stalberg::rooms::RoomRole::Reward:
        sides = 4;
        radius = 12.0F;
        rotation = 45.0F;
        break;
    case stalberg::rooms::RoomRole::Exit:
        sides = 8;
        radius = 15.0F;
        break;
    case stalberg::rooms::RoomRole::Combat:
    case stalberg::rooms::RoomRole::Connector:
        return;
    }
    DrawPoly(center, sides, radius, rotation, Color { 7, 17, 24, 230 });
    DrawPolyLines(center, sides, radius, rotation, color);
}

} // namespace

PrototypeRenderer::PrototypeRenderer(const GeneratedLevel& level)
    : lightingShader(loadDirectionalShader())
    , groundModel(LoadModelFromMesh(GenMeshPlane(80.0F, 80.0F, 1, 1)))
    , generatedFloorModel(LoadModelFromMesh(makeGeneratedFloorMesh(level)))
    , generatedWallModel(LoadModelFromMesh(makeWallMesh(level.walls())))
    , wallModel(LoadModelFromMesh(
          GenMeshCube(1.0F, WALL_HEIGHT, WALL_THICKNESS)))
    , playerModel(LoadModelFromMesh(GenMeshSphere(PLAYER_RADIUS, 8, 12)))
    , targetModel(LoadModelFromMesh(GenMeshSphere(TARGET_RADIUS, 8, 12)))
    , enemyModel(LoadModelFromMesh(GenMeshSphere(ENEMY_RADIUS, 7, 10)))
    , shadowModel(LoadModelFromMesh(
          GenMeshCylinder(PLAYER_RADIUS * 1.05F, 0.01F, 24)))
    , projectileGlow(makeProjectileGlow())
    , cameraPositionLocation(GetShaderLocation(
          lightingShader, "cameraPosition"))
    , cameraTargetLocation(GetShaderLocation(lightingShader, "cameraTarget"))
    , fogColorLocation(GetShaderLocation(lightingShader, "fogColor"))
{
    groundModel.materials[0].shader = lightingShader;
    generatedFloorModel.materials[0].shader = lightingShader;
    generatedWallModel.materials[0].shader = lightingShader;
    wallModel.materials[0].shader = lightingShader;
    playerModel.materials[0].shader = lightingShader;
    targetModel.materials[0].shader = lightingShader;
    enemyModel.materials[0].shader = lightingShader;
}

PrototypeRenderer::~PrototypeRenderer()
{
    UnloadTexture(projectileGlow);
    UnloadModel(shadowModel);
    UnloadModel(enemyModel);
    UnloadModel(targetModel);
    UnloadModel(playerModel);
    UnloadModel(wallModel);
    UnloadModel(generatedWallModel);
    UnloadModel(generatedFloorModel);
    UnloadModel(groundModel);
    UnloadShader(lightingShader);
}

void PrototypeRenderer::drawGenerated(const Camera3D& camera,
    const Player& player, Vector3 aimPoint, const GeneratedLevel& level,
    const LevelSession& session, const ProjectilePool& playerProjectiles,
    const GeneratedCombatState* combat, bool floorComplete,
    float interpolationAmount, bool showDebug) const
{
    BeginDrawing();
    ClearBackground(GENERATED_BACKGROUND);
    updateLighting(camera, GENERATED_BACKGROUND);

    BeginMode3D(camera);
    DrawModel(generatedFloorModel, Vector3 { 0.0F, -0.01F, 0.0F }, 1.0F,
        WHITE);
    DrawModel(generatedWallModel, Vector3 {}, 1.0F, WHITE);
    drawLockedDoorways(level, session);
    DrawSphere(Vector3 { aimPoint.x, 0.06F, aimPoint.z }, 0.12F,
        Color { 205, 242, 236, 210 });
    drawProjectiles(camera, playerProjectiles,
        interpolationAmount, PLAYER_RADIUS,
        Color { 92, 225, 255, 255 }, Color { 92, 225, 255, 155 });
    if (combat != nullptr) {
        for (const StableEnemy& entry : combat->enemies.entries()) {
            drawEnemy(entry.enemy, interpolationAmount);
        }
        drawProjectiles(camera, combat->enemyProjectiles,
            interpolationAmount, PLAYER_RADIUS,
            Color { 255, 93, 55, 255 }, Color { 255, 153, 70, 175 });
    }
    drawPlayer(player);
    EndMode3D();

    const auto currentCell = level.cellAtWorldPoint(
        Vector2 { player.position.x, player.position.z });
    const int currentRegion = currentCell.has_value()
        ? level.roomLayout().getCellAssignment(*currentCell)
        : stalberg::rooms::EMPTY_CELL;
    const auto* currentRoom = findRoom(level, currentRegion);

    drawPlayerHud(player);
    if (currentRoom != nullptr) {
        const char* label = TextFormat("%s  %02i",
            roomRoleName(currentRoom->role), currentRoom->id + 1);
        const int width = MeasureText(label, 18) + 30;
        DrawRectangleRounded(Rectangle {
                                 static_cast<float>(GetScreenWidth() - width - 18),
                                 18.0F, static_cast<float>(width), 38.0F },
            0.35F, 8, Color { 8, 18, 25, 220 });
        DrawText(label, GetScreenWidth() - width - 3, 28, 18,
            roomRoleColor(currentRoom->role));
    }

    if (showDebug) {
        DrawRectangleRounded(Rectangle { 16.0F, 82.0F, 620.0F, 118.0F },
            0.08F, 6, Color { 7, 17, 24, 225 });
        DrawText("F3  HIDE DEBUG", 28, 94, 18,
            Color { 151, 193, 190, 255 });
        DrawText("WASD move | SPACE dash | LMB fire | R reset | F1 arena | F2 overview",
            28, 123, 16, Color { 180, 203, 200, 255 });
        DrawText(TextFormat("rooms %i  doors %i  walls %i  room %i",
                     static_cast<int>(level.roomLayout().getRoomCount()),
                     static_cast<int>(level.roomLayout().getDoorways().size()),
                     static_cast<int>(session.activeWalls().size()), currentRegion),
            28, 150, 16, Color { 224, 211, 158, 255 });
        DrawText(TextFormat("grid %u  layout %u  quality %.1f  shots %i/%i",
                     level.grid().getSeed(), level.roomLayout().getSeed(),
                     level.roomLayout().getQualityScore(),
                     static_cast<int>(playerProjectiles.activeCount()),
                     combat != nullptr
                         ? static_cast<int>(combat->enemyProjectiles.activeCount())
                         : 0),
            28, 177, 16, Color { 180, 203, 200, 255 });
        DrawFPS(GetScreenWidth() - 96, 70);
    }

    const char* statusMessage = nullptr;
    Color statusColor { 255, 174, 92, 255 };
    if (!isPlayerAlive(player)) {
        statusMessage = "DEFEATED  -  press R to restart";
    } else if (floorComplete) {
        statusMessage = "EXIT REACHED  -  floor complete";
        statusColor = Color { 151, 231, 190, 255 };
    }
    if (statusMessage != nullptr) {
        constexpr int fontSize = 30;
        const int messageWidth = MeasureText(statusMessage, fontSize);
        DrawRectangleRounded(Rectangle {
                                 static_cast<float>(GetScreenWidth() / 2
                                     - messageWidth / 2 - 24),
                                 static_cast<float>(GetScreenHeight() / 2 - 34),
                                 static_cast<float>(messageWidth + 48), 68.0F },
            0.2F, 8, Color { 7, 17, 24, 235 });
        DrawText(statusMessage, GetScreenWidth() / 2 - messageWidth / 2,
            GetScreenHeight() / 2 - fontSize / 2, fontSize, statusColor);
    }

    EndDrawing();
}

void PrototypeRenderer::drawGeneratedOverview(const GeneratedLevel& level,
    const LevelSession* session, std::size_t selectedConfiguration,
    std::size_t configurationCount) const
{
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const Rectangle mapBounds {
        18.0F, 66.0F, std::max(screenWidth - 390.0F, 220.0F),
        std::max(screenHeight - 86.0F, 220.0F)
    };
    const Rectangle detailsBounds {
        mapBounds.x + mapBounds.width + 14.0F, 66.0F,
        std::max(screenWidth - mapBounds.x - mapBounds.width - 32.0F, 320.0F),
        mapBounds.height
    };
    const OverviewTransform transform
        = makeOverviewTransform(level, mapBounds);
    const std::vector<Vector2> centers = roomCenters(level);

    BeginDrawing();
    ClearBackground(GENERATED_BACKGROUND);
    DrawText("GENERATED LEVEL OVERVIEW", 20, 17, 24,
        Color { 205, 229, 224, 255 });
    DrawText("F2 return  |  LEFT / RIGHT browse  |  HOME active layout",
        410, 22, 17, Color { 151, 193, 190, 255 });
    DrawRectangleRounded(mapBounds, 0.015F, 6, Color { 7, 17, 24, 255 });

    for (const FloorTriangle& triangle : level.floorTriangles()) {
        DrawTriangle(
            overviewPoint(triangle.first, transform),
            overviewPoint(triangle.third, transform),
            overviewPoint(triangle.second, transform),
            generatedFloorColor(level, triangle.region, triangle.cell));
    }

    for (const stalberg::rooms::Doorway& doorway
        : level.roomLayout().getDoorways()) {
        const Vector2 first = overviewPoint(
            centers[static_cast<std::size_t>(doorway.firstRegion)], transform);
        const Vector2 second = overviewPoint(
            centers[static_cast<std::size_t>(doorway.secondRegion)], transform);
        DrawLineEx(first, second, 2.0F, Color { 198, 182, 119, 150 });
    }

    for (const Segment2D& wall : level.walls()) {
        DrawLineEx(overviewPoint(wall.start, transform),
            overviewPoint(wall.end, transform), 1.35F,
            Color { 18, 32, 40, 235 });
    }

    std::size_t lockedDoorways = 0;
    for (const DoorwayThreshold& threshold : level.doorwayThresholds()) {
        const bool locked = session != nullptr
            && session->doorwayIsLocked(threshold.doorway);
        lockedDoorways += locked ? 1U : 0U;
        DrawLineEx(overviewPoint(threshold.segment.start, transform),
            overviewPoint(threshold.segment.end, transform), 5.0F,
            locked ? Color { 234, 105, 80, 255 }
                   : Color { 105, 226, 178, 255 });
    }

    for (const stalberg::rooms::GeneratedRoom& room
        : level.roomLayout().getRooms()) {
        const Vector2 center = overviewPoint(
            centers[static_cast<std::size_t>(room.id)], transform);
        const Color roleColor = roomRoleColor(room.role);
        const Color labelColor {
            static_cast<unsigned char>(std::min(
                static_cast<int>(roleColor.r) + 85, 255)),
            static_cast<unsigned char>(std::min(
                static_cast<int>(roleColor.g) + 85, 255)),
            static_cast<unsigned char>(std::min(
                static_cast<int>(roleColor.b) + 85, 255)),
            255
        };
        drawOverviewLandmark(
            Vector2 { center.x, center.y - 22.0F }, room.role, labelColor);
        const char* roomLabel = TextFormat("%s %02i",
            roomRoleName(room.role), room.id + 1);
        const int labelWidth = MeasureText(roomLabel, 13);
        const char* geometryLabel = baselineGeometryLabel(room.role);
        const int geometryWidth = MeasureText(geometryLabel, 9);
        const int badgeWidth = std::max(labelWidth, geometryWidth) + 10;
        DrawRectangleRounded(Rectangle {
                                 center.x - static_cast<float>(badgeWidth) * 0.5F,
                                 center.y - 10.0F,
                                 static_cast<float>(badgeWidth), 31.0F },
            0.18F, 5, Color { 7, 17, 24, 205 });
        DrawText(roomLabel, static_cast<int>(center.x) - labelWidth / 2,
            static_cast<int>(center.y) - 7, 13, labelColor);
        DrawText(geometryLabel,
            static_cast<int>(center.x) - geometryWidth / 2,
            static_cast<int>(center.y) + 8, 9,
            Color { 207, 218, 213, 235 });
    }

    if (session != nullptr) {
        const Vector2 player = overviewPoint(Vector2 {
            session->player().position.x, session->player().position.z },
            transform);
        DrawCircleV(player, 6.0F, Color { 255, 211, 91, 255 });
        DrawCircleLines(static_cast<int>(player.x), static_cast<int>(player.y),
            9.0F, Color { 255, 245, 210, 255 });
    }

    DrawRectangleRounded(detailsBounds, 0.025F, 6,
        Color { 7, 17, 24, 245 });
    const int detailsX = static_cast<int>(detailsBounds.x + 18.0F);
    int detailsY = static_cast<int>(detailsBounds.y + 17.0F);
    const Color heading { 151, 193, 190, 255 };
    const Color primary { 224, 225, 207, 255 };
    const Color secondary { 173, 194, 191, 255 };

    DrawText(session != nullptr ? "ACTIVE SESSION" : "READ-ONLY PREVIEW",
        detailsX, detailsY, 18,
        session != nullptr ? Color { 255, 211, 91, 255 }
                           : Color { 151, 193, 190, 255 });
    detailsY += 34;
    DrawText(TextFormat("CONFIGURATION  %02i / %02i",
                 static_cast<int>(selectedConfiguration + 1),
                 static_cast<int>(configurationCount)),
        detailsX, detailsY, 16, heading);
    detailsY += 27;
    DrawText(TextFormat("radius       %i", level.grid().getRadius()),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    DrawText(TextFormat("grid seed    %u", level.grid().getSeed()),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    DrawText(TextFormat("room seed    %u", level.roomLayout().getSeed()),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    DrawText(TextFormat("candidate    %i",
                 static_cast<int>(level.roomLayout().getSelectedCandidate())),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    DrawText(TextFormat("quality      %.2f",
                 level.roomLayout().getQualityScore()),
        detailsX, detailsY, 16, primary);

    detailsY += 38;
    DrawText("BASELINE STRUCTURE", detailsX, detailsY, 16, heading);
    detailsY += 27;
    DrawText(baselineTopologyLabel(level), detailsX, detailsY, 17, primary);
    detailsY += 25;
    const std::size_t roomCount = level.roomLayout().getRoomCount();
    const std::size_t doorwayCount = level.roomLayout().getDoorways().size();
    const std::size_t cycleRank
        = doorwayCount >= roomCount ? doorwayCount - roomCount + 1U : 0U;
    DrawText(TextFormat("rooms %i   doors %i   cycles %i",
                 static_cast<int>(roomCount), static_cast<int>(doorwayCount),
                 static_cast<int>(cycleRank)),
        detailsX, detailsY, 15, secondary);
    detailsY += 22;
    DrawText(TextFormat("locked %i   open %i",
                 static_cast<int>(lockedDoorways),
                 static_cast<int>(doorwayCount - lockedDoorways)),
        detailsX, detailsY, 15, secondary);
    detailsY += 22;
    DrawText("shape grammar: pre-identity baseline", detailsX, detailsY,
        14, Color { 226, 166, 102, 255 });

    detailsY += 39;
    DrawText("MAP KEY", detailsX, detailsY, 16, heading);
    detailsY += 29;
    DrawLineEx(Vector2 { static_cast<float>(detailsX),
                   static_cast<float>(detailsY + 6) },
        Vector2 { static_cast<float>(detailsX + 30),
            static_cast<float>(detailsY + 6) },
        5.0F, Color { 105, 226, 178, 255 });
    DrawText("open threshold", detailsX + 42, detailsY, 15, secondary);
    detailsY += 25;
    DrawLineEx(Vector2 { static_cast<float>(detailsX),
                   static_cast<float>(detailsY + 6) },
        Vector2 { static_cast<float>(detailsX + 30),
            static_cast<float>(detailsY + 6) },
        5.0F, Color { 234, 105, 80, 255 });
    DrawText("locked threshold", detailsX + 42, detailsY, 15, secondary);
    detailsY += 25;
    DrawLineEx(Vector2 { static_cast<float>(detailsX),
                   static_cast<float>(detailsY + 6) },
        Vector2 { static_cast<float>(detailsX + 30),
            static_cast<float>(detailsY + 6) },
        2.0F, Color { 198, 182, 119, 200 });
    DrawText("published room graph", detailsX + 42, detailsY, 15, secondary);
    detailsY += 25;
    DrawCircleV(Vector2 { static_cast<float>(detailsX + 6),
                    static_cast<float>(detailsY + 6) },
        6.0F, Color { 255, 211, 91, 255 });
    DrawText("active player", detailsX + 42, detailsY, 15, secondary);

    DrawText("Role markers: triangle Start · hex Hub · diamond Reward · octagon Exit",
        static_cast<int>(mapBounds.x + 14.0F),
        static_cast<int>(mapBounds.y + mapBounds.height - 24.0F),
        13, Color { 185, 205, 200, 230 });
    EndDrawing();
}

void PrototypeRenderer::drawCombat(const Camera3D& camera,
    const Player& player, Vector3 aimPoint,
    const ProjectilePool& playerProjectiles,
    const Target& target, const Enemy& enemy,
    const ProjectilePool& enemyProjectiles,
    float interpolationAmount, bool showDebug) const
{
    BeginDrawing();
    ClearBackground(ARENA_BACKGROUND);
    updateLighting(camera, ARENA_BACKGROUND);

    BeginMode3D(camera);
    DrawModel(groundModel, Vector3 { 0.0F, -0.015F, 0.0F }, 1.0F,
        Color { 62, 95, 96, 255 });
    drawArena();
    DrawSphere(Vector3 { aimPoint.x, 0.06F, aimPoint.z }, 0.12F,
        Color { 205, 242, 236, 210 });
    drawTarget(target);
    drawEnemy(enemy, interpolationAmount);
    drawProjectiles(camera, playerProjectiles, interpolationAmount,
        PLAYER_RADIUS, Color { 92, 225, 255, 255 },
        Color { 92, 225, 255, 155 });
    drawProjectiles(camera, enemyProjectiles, interpolationAmount,
        PLAYER_RADIUS, Color { 255, 93, 55, 255 },
        Color { 255, 153, 70, 175 });
    drawPlayer(player);
    EndMode3D();

    drawPlayerHud(player);
    DrawRectangleRounded(Rectangle {
                             static_cast<float>(GetScreenWidth() - 218),
                             18.0F, 200.0F, 38.0F },
        0.35F, 8, Color { 8, 18, 25, 220 });
    DrawText("COMBAT TRAINING", GetScreenWidth() - 198, 28, 18,
        Color { 180, 162, 220, 255 });

    if (showDebug) {
        DrawRectangleRounded(Rectangle { 16.0F, 82.0F, 570.0F, 118.0F },
            0.08F, 6, Color { 7, 17, 24, 225 });
        DrawText("F3  HIDE DEBUG", 28, 94, 18,
            Color { 151, 193, 190, 255 });
        DrawText("WASD move | SPACE dash | LMB fire | F1 level | F2 overview",
            28, 123, 16, Color { 180, 203, 200, 255 });
        DrawText(TextFormat("target %i/%i  player shots %i  hostile shots %i",
                     target.health, TARGET_MAX_HEALTH,
                     static_cast<int>(playerProjectiles.activeCount()),
                     static_cast<int>(enemyProjectiles.activeCount())),
            28, 150, 16, Color { 224, 211, 158, 255 });
        DrawText(isEnemyAlive(enemy) ? "enemy active" : "enemy defeated",
            28, 177, 16, Color { 180, 162, 220, 255 });
        DrawFPS(GetScreenWidth() - 96, 70);
    }

    const char* encounterMessage = nullptr;
    Color encounterMessageColor { 255, 174, 92, 255 };
    if (!isPlayerAlive(player)) {
        encounterMessage = "DEFEATED  -  press R to restart";
    } else if (!isEnemyAlive(enemy)) {
        encounterMessage = "ENEMY DEFEATED  -  press R to restart";
        encounterMessageColor = Color { 151, 231, 190, 255 };
    }
    if (encounterMessage != nullptr) {
        const int fontSize = 30;
        const int messageWidth = MeasureText(encounterMessage, fontSize);
        DrawRectangleRounded(Rectangle {
                                 static_cast<float>(GetScreenWidth() / 2
                                     - messageWidth / 2 - 24),
                                 static_cast<float>(GetScreenHeight() / 2 - 34),
                                 static_cast<float>(messageWidth + 48), 68.0F },
            0.2F, 8, Color { 7, 17, 24, 235 });
        DrawText(encounterMessage, GetScreenWidth() / 2 - messageWidth / 2,
            GetScreenHeight() / 2 - fontSize / 2, fontSize,
            encounterMessageColor);
    }

    EndDrawing();
}

void PrototypeRenderer::drawArena() const
{
    constexpr Color wallColor { 94, 111, 118, 255 };

    for (const WallSegment& wall : ARENA_WALLS) {
        const Vector2 direction {
            wall.end.x - wall.start.x,
            wall.end.y - wall.start.y
        };
        const float wallLength = std::sqrt(
            direction.x * direction.x + direction.y * direction.y);
        if (wallLength <= 0.0001F) {
            continue;
        }

        const Vector2 outwardNormal {
            direction.y / wallLength,
            -direction.x / wallLength
        };
        const Vector3 center {
            (wall.start.x + wall.end.x) * 0.5F
                + outwardNormal.x * WALL_THICKNESS * 0.5F,
            WALL_HEIGHT * 0.5F,
            (wall.start.y + wall.end.y) * 0.5F
                + outwardNormal.y * WALL_THICKNESS * 0.5F
        };
        const float angle = -std::atan2(direction.y, direction.x) * RAD2DEG;
        DrawModelEx(wallModel, center, Vector3 { 0.0F, 1.0F, 0.0F }, angle,
            Vector3 { wallLength, 1.0F, 1.0F }, wallColor);
    }
}

void PrototypeRenderer::drawLockedDoorways(const GeneratedLevel& level,
    const LevelSession& session) const
{
    constexpr Color lockedColor { 132, 82, 72, 255 };
    for (const DoorwayThreshold& threshold : level.doorwayThresholds()) {
        if (!session.doorwayIsLocked(threshold.doorway)) {
            continue;
        }
        const Segment2D& wall = threshold.segment;
        const Vector2 direction {
            wall.end.x - wall.start.x,
            wall.end.y - wall.start.y
        };
        const float wallLength = std::sqrt(
            direction.x * direction.x + direction.y * direction.y);
        if (wallLength <= 0.0001F) {
            continue;
        }
        const Vector3 center {
            (wall.start.x + wall.end.x) * 0.5F,
            WALL_HEIGHT * 0.5F,
            (wall.start.y + wall.end.y) * 0.5F
        };
        const float angle = -std::atan2(direction.y, direction.x) * RAD2DEG;
        DrawModelEx(wallModel, center, Vector3 { 0.0F, 1.0F, 0.0F }, angle,
            Vector3 { wallLength, 1.0F, 1.0F }, lockedColor);
    }
}

void PrototypeRenderer::drawActorShadow(Vector3 position, float radius) const
{
    const float groundDistance = position.y / -DIRECTIONAL_LIGHT.y;
    const Vector3 shadowPosition {
        position.x + DIRECTIONAL_LIGHT.x * groundDistance,
        0.006F,
        position.z + DIRECTIONAL_LIGHT.z * groundDistance
    };
    const float shadowAngle = std::atan2(
        DIRECTIONAL_LIGHT.z, DIRECTIONAL_LIGHT.x) * RAD2DEG;
    const float size = radius / PLAYER_RADIUS;
    DrawModelEx(shadowModel, shadowPosition, Vector3 { 0.0F, 1.0F, 0.0F },
        shadowAngle, Vector3 { size * 1.35F, 1.0F, size * 0.78F },
        Color { 5, 10, 14, 88 });
}

void PrototypeRenderer::drawTarget(const Target& target) const
{
    const Vector3 targetCenter {
        target.position.x,
        TARGET_RADIUS,
        target.position.y
    };
    const Vector3 groundCenter {
        target.position.x,
        0.025F,
        target.position.y
    };
    drawActorShadow(targetCenter, TARGET_RADIUS);

    if (target.health <= 0) {
        const float resetProgress = 1.0F
            - target.resetRemaining / TARGET_RESET_DELAY;
        DrawCircle3D(groundCenter,
            TARGET_RADIUS * (0.35F + resetProgress * 0.9F),
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 255, 174, 92, 220 });
        DrawSphereWires(targetCenter, TARGET_RADIUS * 0.4F,
            8, 12, Color { 255, 210, 140, 150 });
        return;
    }

    const float flashAmount = target.hitFlashRemaining
        / TARGET_HIT_FLASH_DURATION;
    const float targetScale = 1.0F + flashAmount * 0.18F;
    const Color targetColor = flashAmount > 0.0F
        ? Color { 255, 238, 194, 255 }
        : Color { 207, 75, 72, 255 };

    DrawCircle3D(groundCenter, TARGET_RADIUS * 1.05F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { 91, 31, 35, 230 });
    DrawModel(targetModel, targetCenter, targetScale, targetColor);
    if (flashAmount > 0.0F) {
        DrawSphereWires(targetCenter,
            TARGET_RADIUS * (1.15F + (1.0F - flashAmount) * 0.45F),
            8, 12, Color { 255, 245, 210, 210 });
    }
}

void PrototypeRenderer::drawProjectiles(const Camera3D& camera,
    const ProjectilePool& projectiles, float interpolationAmount,
    float height, Color headColor, Color trailColor) const
{
    constexpr float trailLength = 0.62F;
    const float inverseSpeed = 1.0F / projectiles.profile().speed;

    for (const Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        const Vector2 position = interpolateProjectilePosition(
            projectile, interpolationAmount);
        const Vector3 head { position.x, height, position.y };
        const Vector3 tail {
            head.x - projectile.velocity.x * inverseSpeed * trailLength,
            height,
            head.z - projectile.velocity.y * inverseSpeed * trailLength
        };
        DrawLine3D(tail, head, trailColor);
    }

    BeginBlendMode(BLEND_ADDITIVE);
    for (const Projectile& projectile : projectiles.projectiles()) {
        if (!projectile.active) {
            continue;
        }
        const Vector2 position = interpolateProjectilePosition(
            projectile, interpolationAmount);
        const Vector3 head { position.x, height, position.y };
        DrawBillboard(camera, projectileGlow, head,
            projectiles.profile().radius * 3.4F, headColor);
    }
    EndBlendMode();
}

void PrototypeRenderer::drawEnemy(
    const Enemy& enemy, float interpolationAmount) const
{
    const Vector2 position = interpolateEnemyPosition(
        enemy, interpolationAmount);
    const Vector3 center { position.x, ENEMY_RADIUS, position.y };
    const Vector3 groundCenter { position.x, 0.025F, position.y };
    drawActorShadow(center, ENEMY_RADIUS);
    if (!isEnemyAlive(enemy)) {
        DrawCircle3D(groundCenter, ENEMY_RADIUS * 1.8F,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 151, 231, 190, 210 });
        DrawModel(enemyModel, center, 0.55F, Color { 72, 74, 81, 255 });
        DrawSphereWires(center, ENEMY_RADIUS * 1.2F,
            8, 12, Color { 186, 244, 211, 190 });
        return;
    }

    const float chargeAmount = 1.0F - std::clamp(
        enemy.shotCooldownRemaining / ENEMY_SHOT_INTERVAL, 0.0F, 1.0F);
    const Color bodyColor = enemy.hitFlashRemaining > 0.0F
        ? Color { 244, 222, 255, 255 }
        : Color { 143, 81, 184, 255 };

    DrawCircle3D(groundCenter,
        ENEMY_RADIUS * (1.15F + chargeAmount * 0.35F),
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { 80, 48, 112, 220 });
    DrawModelEx(enemyModel, center, Vector3 { 0.0F, 1.0F, 0.0F }, 0.0F,
        Vector3 { 1.0F, 1.18F, 1.0F }, bodyColor);
    if (enemy.hitFlashRemaining > 0.0F) {
        DrawSphereWires(center, ENEMY_RADIUS * 1.18F,
            8, 12, Color { 255, 238, 194, 210 });
    }
    const Vector3 aimEnd {
        center.x + enemy.facing.x * (ENEMY_RADIUS + 0.6F),
        center.y,
        center.z + enemy.facing.y * (ENEMY_RADIUS + 0.6F)
    };
    DrawCylinderEx(center, aimEnd, 0.18F, 0.07F, 8,
        Color { 255, 177, 92, 255 });
}

void PrototypeRenderer::drawPlayer(const Player& player) const
{
    constexpr Color facingColor { 255, 231, 145, 255 };
    const Color bodyColor = player.hitFlashRemaining > 0.0F
        ? Color { 255, 245, 210, 255 }
        : isPlayerAlive(player)
            ? Color { 239, 180, 74, 255 }
            : Color { 96, 75, 68, 255 };
    const float bodyScale = isPlayerAlive(player) ? 1.0F : 0.65F;

    drawActorShadow(player.position, PLAYER_RADIUS * bodyScale);
    if (isPlayerAlive(player)) {
        DrawCircle3D(Vector3 {
                         player.position.x, 0.024F, player.position.z },
            PLAYER_RADIUS * 1.08F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 133, 91, 30, 150 });
        if (player.dashRemaining > 0.0F) {
            DrawCircle3D(Vector3 {
                             player.position.x, 0.03F, player.position.z },
                PLAYER_RADIUS * 1.55F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
                Color { 92, 225, 255, 130 });
        }
    }
    DrawModel(playerModel, player.position, bodyScale, bodyColor);
    if (player.invulnerabilityRemaining > 0.0F) {
        const float shieldScale = 1.08F
            + 0.12F * (player.invulnerabilityRemaining
                / PLAYER_INVULNERABILITY_DURATION);
        DrawSphereWires(player.position, PLAYER_RADIUS * shieldScale,
            8, 12, Color { 255, 238, 194, 180 });
    }
    if (!isPlayerAlive(player)) {
        DrawCircle3D(Vector3 {
                         player.position.x, 0.025F, player.position.z },
            PLAYER_RADIUS * 1.8F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 255, 113, 74, 210 });
        return;
    }

    const Vector3 noseStart {
        player.position.x + player.facing.x * PLAYER_RADIUS * 0.55F,
        player.position.y,
        player.position.z + player.facing.y * PLAYER_RADIUS * 0.55F
    };
    const Vector3 noseEnd {
        player.position.x + player.facing.x * PLAYER_FACING_MARKER_DISTANCE,
        player.position.y,
        player.position.z + player.facing.y * PLAYER_FACING_MARKER_DISTANCE
    };
    DrawCylinderEx(noseStart, noseEnd, 0.18F, 0.05F, 8, facingColor);
}

void PrototypeRenderer::updateLighting(
    const Camera3D& camera, Color fogColor) const
{
    const float cameraPosition[3] {
        camera.position.x, camera.position.y, camera.position.z
    };
    const float cameraTarget[3] {
        camera.target.x, camera.target.y, camera.target.z
    };
    const float normalizedFogColor[3] {
        static_cast<float>(fogColor.r) / 255.0F,
        static_cast<float>(fogColor.g) / 255.0F,
        static_cast<float>(fogColor.b) / 255.0F
    };
    SetShaderValue(lightingShader, cameraPositionLocation,
        cameraPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, cameraTargetLocation,
        cameraTarget, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, fogColorLocation,
        normalizedFogColor, SHADER_UNIFORM_VEC3);
}

void PrototypeRenderer::drawPlayerHud(const Player& player) const
{
    constexpr float panelWidth = 260.0F;
    constexpr float barWidth = 158.0F;
    const float healthAmount = std::clamp(
        static_cast<float>(player.health) / PLAYER_MAX_HEALTH, 0.0F, 1.0F);
    const float dashAmount = 1.0F - std::clamp(
        player.dashCooldownRemaining / PLAYER_DASH_COOLDOWN, 0.0F, 1.0F);
    const Color healthColor = player.health <= 1
        ? Color { 241, 91, 64, 255 }
        : Color { 232, 174, 65, 255 };

    DrawRectangleRounded(Rectangle { 16.0F, 16.0F, panelWidth, 64.0F },
        0.22F, 8, Color { 7, 17, 24, 225 });
    DrawText("HEALTH", 28, 25, 17, Color { 194, 211, 208, 255 });
    DrawRectangleRounded(Rectangle { 101.0F, 27.0F, barWidth, 16.0F },
        0.5F, 8, Color { 29, 42, 47, 255 });
    if (healthAmount > 0.0F) {
        DrawRectangleRounded(Rectangle {
                                 101.0F, 27.0F,
                                 barWidth * healthAmount, 16.0F },
            0.5F, 8, healthColor);
    }
    DrawText(TextFormat("%i", std::max(player.health, 0)), 263, 29, 12,
        Color { 194, 211, 208, 255 });

    DrawText("DASH", 28, 52, 14, Color { 194, 211, 208, 255 });
    DrawRectangleRounded(Rectangle { 101.0F, 55.0F, barWidth, 9.0F },
        0.5F, 8, Color { 29, 42, 47, 255 });
    if (dashAmount > 0.0F) {
        DrawRectangleRounded(Rectangle {
                                 101.0F, 55.0F,
                                 barWidth * dashAmount, 9.0F },
            0.5F, 8, Color { 92, 225, 255, 255 });
    }
}
