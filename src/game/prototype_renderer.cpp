#include "prototype_renderer.hpp"

#include "arena.hpp"
#include "directional_shader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
    const LevelSession& session, const CombatState* combat,
    bool floorComplete, float interpolationAmount, bool showDebug) const
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
    if (combat != nullptr) {
        drawEnemy(combat->enemy, interpolationAmount);
        drawProjectiles(camera, combat->playerProjectiles,
            interpolationAmount, PLAYER_RADIUS,
            Color { 92, 225, 255, 255 }, Color { 92, 225, 255, 155 });
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
        DrawText("WASD move  |  mouse aim  |  hold LMB fire  |  R reset  |  F1 arena",
            28, 123, 16, Color { 180, 203, 200, 255 });
        DrawText(TextFormat("rooms %i  doors %i  walls %i  room %i",
                     static_cast<int>(level.roomLayout().getRoomCount()),
                     static_cast<int>(level.roomLayout().getDoorways().size()),
                     static_cast<int>(session.activeWalls().size()), currentRegion),
            28, 150, 16, Color { 224, 211, 158, 255 });
        DrawText(TextFormat("grid %u  layout %u  quality %.1f  shots %i/%i",
                     level.grid().getSeed(), level.roomLayout().getSeed(),
                     level.roomLayout().getQualityScore(),
                     combat != nullptr
                         ? static_cast<int>(combat->playerProjectiles.activeCount())
                         : 0,
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
        DrawText("WASD move  |  mouse aim  |  hold LMB fire  |  F1 level",
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
    const Color healthColor = player.health <= 1
        ? Color { 241, 91, 64, 255 }
        : Color { 232, 174, 65, 255 };

    DrawRectangleRounded(Rectangle { 16.0F, 16.0F, panelWidth, 50.0F },
        0.28F, 8, Color { 7, 17, 24, 225 });
    DrawText("HEALTH", 28, 27, 17, Color { 194, 211, 208, 255 });
    DrawRectangleRounded(Rectangle { 101.0F, 29.0F, barWidth, 16.0F },
        0.5F, 8, Color { 29, 42, 47, 255 });
    if (healthAmount > 0.0F) {
        DrawRectangleRounded(Rectangle {
                                 101.0F, 29.0F,
                                 barWidth * healthAmount, 16.0F },
            0.5F, 8, healthColor);
    }
    DrawText(TextFormat("%i", std::max(player.health, 0)), 229, 48, 12,
        Color { 194, 211, 208, 255 });
}
