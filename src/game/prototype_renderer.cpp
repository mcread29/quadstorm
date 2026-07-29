#include "prototype_renderer.hpp"

#include "arena.hpp"
#include "directional_shader.hpp"
#include "generated_level_queries.hpp"
#include "post_process_shader.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

constexpr float WALL_HEIGHT = 2.15F;
constexpr float WALL_THICKNESS = 0.24F;
constexpr Color GENERATED_BACKGROUND { 8, 14, 20, 255 };
constexpr Color ARENA_BACKGROUND { 10, 16, 22, 255 };
constexpr Color HUD_SURFACE { 6, 13, 19, 238 };
constexpr Color HUD_BORDER { 66, 96, 101, 180 };
constexpr Color ENERGY_CYAN { 76, 224, 226, 255 };
constexpr Color MACHINE_GOLD { 238, 171, 62, 255 };

Color roomRoleColor(stalberg::rooms::RoomRole role)
{
    switch (role) {
    case stalberg::rooms::RoomRole::Start:
        return Color { 50, 78, 76, 255 };
    case stalberg::rooms::RoomRole::Combat:
        return Color { 49, 62, 76, 255 };
    case stalberg::rooms::RoomRole::Connector:
        return Color { 40, 53, 62, 255 };
    case stalberg::rooms::RoomRole::Hub:
        return Color { 45, 72, 78, 255 };
    case stalberg::rooms::RoomRole::Reward:
        return Color { 78, 67, 45, 255 };
    case stalberg::rooms::RoomRole::Exit:
        return Color { 55, 78, 64, 255 };
    }
    return Color { 43, 58, 64, 255 };
}

const char* smallMapRecipeName(stalberg::rooms::SmallMapRecipe recipe)
{
    switch (recipe) {
    case stalberg::rooms::SmallMapRecipe::HubCircuit:
        return "HUB CIRCUIT";
    case stalberg::rooms::SmallMapRecipe::BrokenRing:
        return "BROKEN RING";
    case stalberg::rooms::SmallMapRecipe::TwinWings:
        return "TWIN WINGS";
    }
    return "SMALL MAP";
}

const char* roundPhaseName(RoundPhase phase)
{
    switch (phase) {
    case RoundPhase::Intermission:
        return "INTERMISSION";
    case RoundPhase::Buildup:
        return "BUILDUP";
    case RoundPhase::Peak:
        return "PEAK";
    case RoundPhase::Cleanup:
        return "CLEANUP";
    }
    return "ROUND";
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

Color generatedFloorColor(const GeneratedLevel& level,
    int region, stalberg::rooms::CellIndex cell)
{
    const auto* room = generated_level::findRoomById(level, region);
    const Color base = room != nullptr
        ? roomRoleColor(room->role)
        : Color { 43, 58, 64, 255 };
    std::uint32_t hash = static_cast<std::uint32_t>(cell) * 747796405U
        + 2891336453U;
    hash ^= hash >> 16U;
    const float variation = 0.88F
        + static_cast<float>(hash & 255U) / 255.0F * 0.12F;
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

    constexpr Color topColor { 99, 118, 120, 255 };
    constexpr Color sideColor { 47, 57, 65, 255 };
    constexpr Color capColor { 66, 87, 91, 255 };
    constexpr Color stripColor { 55, 94, 97, 255 };
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

        const Vector3 stripA { a0.x + leftNormal.x * 0.004F,
            WALL_HEIGHT * 0.72F, a0.z + leftNormal.z * 0.004F };
        const Vector3 stripB { b0.x + leftNormal.x * 0.004F,
            WALL_HEIGHT * 0.72F, b0.z + leftNormal.z * 0.004F };
        const Vector3 stripC { stripB.x, WALL_HEIGHT * 0.76F, stripB.z };
        const Vector3 stripD { stripA.x, WALL_HEIGHT * 0.76F, stripA.z };
        appendQuad(stripA, stripB, stripC, stripD, leftNormal, stripColor);

        const Vector3 capSide {
            side.x * 1.34F, 0.0F, side.z * 1.34F
        };
        const Vector3 capA { wall.start.x + capSide.x,
            WALL_HEIGHT - 0.08F, wall.start.y + capSide.z };
        const Vector3 capB { wall.end.x + capSide.x,
            WALL_HEIGHT - 0.08F, wall.end.y + capSide.z };
        const Vector3 capC { wall.end.x - capSide.x,
            WALL_HEIGHT - 0.08F, wall.end.y - capSide.z };
        const Vector3 capD { wall.start.x - capSide.x,
            WALL_HEIGHT - 0.08F, wall.start.y - capSide.z };
        const Vector3 capTopA { capA.x, WALL_HEIGHT + 0.04F, capA.z };
        const Vector3 capTopB { capB.x, WALL_HEIGHT + 0.04F, capB.z };
        const Vector3 capTopC { capC.x, WALL_HEIGHT + 0.04F, capC.z };
        const Vector3 capTopD { capD.x, WALL_HEIGHT + 0.04F, capD.z };
        appendQuad(capTopA, capTopB, capTopC, capTopD,
            Vector3 { 0.0F, 1.0F, 0.0F }, topColor);
        appendQuad(capA, capB, capTopB, capTopA, leftNormal, capColor);
        appendQuad(capD, capTopD, capTopC, capC, rightNormal, capColor);

        const float ribHalfWidth = std::min(0.14F, length * 0.18F);
        const Vector3 middle {
            (wall.start.x + wall.end.x) * 0.5F,
            0.0F,
            (wall.start.y + wall.end.y) * 0.5F
        };
        const Vector3 ribAlong {
            along.x * ribHalfWidth, 0.0F, along.z * ribHalfWidth
        };
        const Vector3 ribSide {
            leftNormal.x * WALL_THICKNESS * 0.95F,
            0.0F,
            leftNormal.z * WALL_THICKNESS * 0.95F
        };
        const Vector3 ribA {
            middle.x - ribAlong.x + ribSide.x, 0.0F,
            middle.z - ribAlong.z + ribSide.z
        };
        const Vector3 ribB {
            middle.x + ribAlong.x + ribSide.x, 0.0F,
            middle.z + ribAlong.z + ribSide.z
        };
        const Vector3 ribC {
            middle.x + ribAlong.x - ribSide.x, 0.0F,
            middle.z + ribAlong.z - ribSide.z
        };
        const Vector3 ribD {
            middle.x - ribAlong.x - ribSide.x, 0.0F,
            middle.z - ribAlong.z - ribSide.z
        };
        constexpr float ribHeight = WALL_HEIGHT * 0.82F;
        const Vector3 ribTopA { ribA.x, ribHeight, ribA.z };
        const Vector3 ribTopB { ribB.x, ribHeight, ribB.z };
        const Vector3 ribTopC { ribC.x, ribHeight, ribC.z };
        const Vector3 ribTopD { ribD.x, ribHeight, ribD.z };
        appendQuad(ribA, ribB, ribTopB, ribTopA,
            leftNormal, capColor);
        appendQuad(ribD, ribTopD, ribTopC, ribC,
            rightNormal, capColor);
        appendQuad(ribB, ribC, ribTopC, ribTopB,
            along, capColor);
        appendQuad(ribD, ribA, ribTopA, ribTopD,
            Vector3 { -along.x, 0.0F, -along.z }, capColor);
        appendQuad(ribTopA, ribTopB, ribTopC, ribTopD,
            Vector3 { 0.0F, 1.0F, 0.0F }, topColor);
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

Shader loadPostProcessShader(const char* fragmentShader)
{
    return LoadShaderFromMemory(nullptr, fragmentShader);
}

void drawRenderTexture(RenderTexture2D target, float width, float height)
{
    DrawTexturePro(target.texture,
        Rectangle { 0.0F, 0.0F,
            static_cast<float>(target.texture.width),
            -static_cast<float>(target.texture.height) },
        Rectangle { 0.0F, 0.0F, width, height },
        Vector2 {}, 0.0F, WHITE);
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
    const float lightColor[3] { 0.92F, 0.74F, 0.54F };
    const float groundAmbient[3] { 0.055F, 0.075F, 0.095F };
    const float skyAmbient[3] { 0.22F, 0.29F, 0.32F };
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
    if (!level.roomLayout().hasSmallMapRecipe()) {
        const std::size_t rooms = level.roomLayout().getRoomCount();
        const std::size_t doors = level.roomLayout().getDoorways().size();
        return doors >= rooms ? "SPATIAL TREE + LOOP" : "SPATIAL TREE";
    }
    return smallMapRecipeName(level.roomLayout().getSmallMapRecipe());
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

void drawHudPanel(Rectangle bounds, Color accent)
{
    DrawRectangleRounded(Rectangle {
                             bounds.x + 4.0F, bounds.y + 5.0F,
                             bounds.width, bounds.height },
        0.16F, 7, Color { 0, 0, 0, 105 });
    DrawRectangleRounded(bounds, 0.16F, 7, HUD_BORDER);
    DrawRectangleRounded(Rectangle {
                             bounds.x + 1.0F, bounds.y + 1.0F,
                             bounds.width - 2.0F, bounds.height - 2.0F },
        0.16F, 7, HUD_SURFACE);
    DrawRectangle(static_cast<int>(bounds.x + 1.0F),
        static_cast<int>(bounds.y + 9.0F), 3,
        static_cast<int>(bounds.height - 18.0F), accent);
}

void drawVoidGrid()
{
    constexpr int extent = 42;
    constexpr int spacing = 2;
    constexpr float height = -0.16F;
    for (int coordinate = -extent; coordinate <= extent;
         coordinate += spacing) {
        const bool major = coordinate % 10 == 0;
        const Color color = major
            ? Color { 27, 63, 70, 120 }
            : Color { 15, 34, 41, 78 };
        DrawLine3D(Vector3 { static_cast<float>(coordinate), height,
                       static_cast<float>(-extent) },
            Vector3 { static_cast<float>(coordinate), height,
                static_cast<float>(extent) }, color);
        DrawLine3D(Vector3 { static_cast<float>(-extent), height,
                       static_cast<float>(coordinate) },
            Vector3 { static_cast<float>(extent), height,
                static_cast<float>(coordinate) }, color);
    }
}

void drawRadialFloorDecal(Vector2 center, float radius,
    int spokes, Color color)
{
    const Vector3 origin { center.x, 0.045F, center.y };
    for (const float scale : { 0.55F, 0.78F, 1.0F }) {
        DrawCircle3D(origin, radius * scale,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { color.r, color.g, color.b,
                static_cast<unsigned char>(color.a * scale) });
    }
    for (int spoke = 0; spoke < spokes; ++spoke) {
        const float angle = static_cast<float>(spoke)
            / static_cast<float>(spokes) * 2.0F * PI;
        const Vector2 direction { std::cos(angle), std::sin(angle) };
        DrawLine3D(Vector3 {
                       center.x + direction.x * radius * 0.78F,
                       0.046F,
                       center.y + direction.y * radius * 0.78F },
            Vector3 {
                center.x + direction.x * radius * 1.18F,
                0.046F,
                center.y + direction.y * radius * 1.18F }, color);
    }
}

void drawWorldReticle(Vector3 aimPoint)
{
    const Vector3 center { aimPoint.x, 0.045F, aimPoint.z };
    DrawCircle3D(center, 0.28F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { ENERGY_CYAN.r, ENERGY_CYAN.g, ENERGY_CYAN.b, 185 });
    DrawCircle3D(center, 0.08F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { 214, 255, 249, 225 });
    constexpr float inner = 0.12F;
    constexpr float outer = 0.38F;
    for (const Vector2 direction
        : { Vector2 { 1.0F, 0.0F }, Vector2 { 0.0F, 1.0F } }) {
        for (const float sign : { -1.0F, 1.0F }) {
            DrawLine3D(Vector3 {
                           center.x + direction.x * inner * sign,
                           center.y,
                           center.z + direction.y * inner * sign },
                Vector3 {
                    center.x + direction.x * outer * sign,
                    center.y,
                    center.z + direction.y * outer * sign },
                Color { ENERGY_CYAN.r, ENERGY_CYAN.g,
                    ENERGY_CYAN.b, 180 });
        }
    }
}

} // namespace

PrototypeRenderer::PrototypeRenderer(const GeneratedLevel& level)
    : lightingShader(loadDirectionalShader())
    , bloomExtractShader(loadPostProcessShader(
          BLOOM_EXTRACT_FRAGMENT_SHADER))
    , bloomBlurShader(loadPostProcessShader(BLOOM_BLUR_FRAGMENT_SHADER))
    , compositeShader(loadPostProcessShader(COMPOSITE_FRAGMENT_SHADER))
    , groundModel(LoadModelFromMesh(GenMeshPlane(80.0F, 80.0F, 1, 1)))
    , generatedFloorModel(LoadModelFromMesh(makeGeneratedFloorMesh(level)))
    , generatedWallModel(LoadModelFromMesh(makeWallMesh(level.walls())))
    , wallModel(LoadModelFromMesh(
          GenMeshCube(1.0F, WALL_HEIGHT, WALL_THICKNESS)))
    , playerModel(LoadModelFromMesh(GenMeshCylinder(
          PLAYER_RADIUS * 0.72F, PLAYER_RADIUS * 1.45F, 8)))
    , targetModel(LoadModelFromMesh(GenMeshSphere(TARGET_RADIUS, 8, 12)))
    , enemyModel(LoadModelFromMesh(GenMeshCylinder(
          ENEMY_RADIUS * 0.78F, ENEMY_RADIUS * 1.25F, 7)))
    , runnerModel(LoadModelFromMesh(GenMeshCone(
          ENEMY_RADIUS * 0.72F, ENEMY_RADIUS * 1.65F, 6)))
    , casterModel(LoadModelFromMesh(GenMeshCylinder(
          ENEMY_RADIUS * 0.68F, ENEMY_RADIUS * 1.85F, 8)))
    , eliteModel(LoadModelFromMesh(GenMeshCube(
          ENEMY_RADIUS * 1.45F, ENEMY_RADIUS * 1.55F,
          ENEMY_RADIUS * 1.45F)))
    , shadowModel(LoadModelFromMesh(
          GenMeshCylinder(PLAYER_RADIUS * 1.05F, 0.01F, 24)))
    , projectileGlow(makeProjectileGlow())
    , cameraPositionLocation(GetShaderLocation(
          lightingShader, "cameraPosition"))
    , cameraTargetLocation(GetShaderLocation(lightingShader, "cameraTarget"))
    , fogColorLocation(GetShaderLocation(lightingShader, "fogColor"))
    , materialKindLocation(GetShaderLocation(lightingShader, "materialKind"))
    , pointLightPositionALocation(GetShaderLocation(
          lightingShader, "pointLightPositionA"))
    , pointLightColorALocation(GetShaderLocation(
          lightingShader, "pointLightColorA"))
    , pointLightPositionBLocation(GetShaderLocation(
          lightingShader, "pointLightPositionB"))
    , pointLightColorBLocation(GetShaderLocation(
          lightingShader, "pointLightColorB"))
    , blurDirectionLocation(GetShaderLocation(
          bloomBlurShader, "blurDirection"))
    , compositeResolutionLocation(GetShaderLocation(
          compositeShader, "resolution"))
    , compositeTimeLocation(GetShaderLocation(compositeShader, "time"))
    , compositeDamageLocation(GetShaderLocation(
          compositeShader, "damageAmount"))
    , compositeDashLocation(GetShaderLocation(
          compositeShader, "dashAmount"))
    , compositeEnergyLocation(GetShaderLocation(
          compositeShader, "energyPulse"))
{
    groundModel.materials[0].shader = lightingShader;
    generatedFloorModel.materials[0].shader = lightingShader;
    generatedWallModel.materials[0].shader = lightingShader;
    wallModel.materials[0].shader = lightingShader;
    playerModel.materials[0].shader = lightingShader;
    targetModel.materials[0].shader = lightingShader;
    enemyModel.materials[0].shader = lightingShader;
    runnerModel.materials[0].shader = lightingShader;
    casterModel.materials[0].shader = lightingShader;
    eliteModel.materials[0].shader = lightingShader;
    clearLocalLights();
    setMaterial(0.0F);
    ensurePostProcessTargets();
}

PrototypeRenderer::~PrototypeRenderer()
{
    if (sceneTarget.id != 0) {
        UnloadRenderTexture(sceneTarget);
        UnloadRenderTexture(bloomTargetA);
        UnloadRenderTexture(bloomTargetB);
    }
    UnloadTexture(projectileGlow);
    UnloadModel(shadowModel);
    UnloadModel(eliteModel);
    UnloadModel(casterModel);
    UnloadModel(runnerModel);
    UnloadModel(enemyModel);
    UnloadModel(targetModel);
    UnloadModel(playerModel);
    UnloadModel(wallModel);
    UnloadModel(generatedWallModel);
    UnloadModel(generatedFloorModel);
    UnloadModel(groundModel);
    UnloadShader(compositeShader);
    UnloadShader(bloomBlurShader);
    UnloadShader(bloomExtractShader);
    UnloadShader(lightingShader);
}

void PrototypeRenderer::drawGenerated(const Camera3D& camera,
    const Player& player, Vector3 aimPoint, const GeneratedLevel& level,
    const LevelSession& session, const HordeMatch& match,
    float interpolationAmount, bool showDebug)
{
    ensurePostProcessTargets();
    updateLighting(camera, GENERATED_BACKGROUND);
    updateGeneratedLights(match);

    BeginTextureMode(sceneTarget);
    ClearBackground(GENERATED_BACKGROUND);
    BeginMode3D(camera);
    drawVoidGrid();
    setMaterial(1.0F);
    DrawModel(generatedFloorModel, Vector3 { 0.0F, -0.01F, 0.0F }, 1.0F,
        WHITE);
    setMaterial(2.0F);
    DrawModel(generatedWallModel, Vector3 {}, 1.0F, WHITE);
    drawLockedDoorways(level, session);
    drawHordeLandmarks(level, session, match);
    drawWorldReticle(aimPoint);
    drawProjectiles(camera, match.playerProjectiles(),
        interpolationAmount, PLAYER_RADIUS,
        Color { 92, 225, 255, 255 }, Color { 92, 225, 255, 155 });
    for (const HordeEnemy& enemy : match.enemies()) {
        drawHordeEnemy(enemy, interpolationAmount);
    }
    drawProjectiles(camera, match.enemyProjectiles(),
        interpolationAmount, PLAYER_RADIUS,
        Color { 255, 93, 55, 255 }, Color { 255, 153, 70, 175 });
    drawPlayer(player);
    EndMode3D();
    EndTextureMode();

    buildBloom();
    BeginDrawing();
    const float damageAmount = std::clamp(
        player.hitFlashRemaining / PLAYER_HIT_FLASH_DURATION, 0.0F, 1.0F);
    const float dashAmount = std::clamp(
        player.dashRemaining / PLAYER_DASH_DURATION, 0.0F, 1.0F);
    const float energyPulse = match.anchorIsActive()
        ? 0.5F + 0.5F * std::sin(static_cast<float>(GetTime()) * 4.0F)
        : 0.0F;
    drawPostProcessedScene(damageAmount, dashAmount, energyPulse);

    const auto currentCell = level.cellAtWorldPoint(
        Vector2 { player.position.x, player.position.z });
    const int currentRegion = currentCell.has_value()
        ? level.roomLayout().getCellAssignment(*currentCell)
        : stalberg::rooms::EMPTY_CELL;
    const auto* currentRoom
        = generated_level::findRoomById(level, currentRegion);

    drawPlayerHud(player);
    drawHudPanel(Rectangle { 292.0F, 16.0F, 350.0F, 64.0F },
        MACHINE_GOLD);
    DrawText("CREDITS", 309, 25, 13, Color { 132, 165, 164, 255 });
    DrawText(TextFormat("%05i", match.points()), 382, 22, 23,
        MACHINE_GOLD);
    DrawText(TextFormat("WAVE %i/%i", match.round(), HORDE_FINAL_ROUND),
        309, 52, 14, Color { 204, 220, 215, 255 });
    DrawText(roundPhaseName(match.phase()), 400, 52, 14,
        Color { 132, 165, 164, 255 });
    DrawText(TextFormat("HOSTILES %02i",
                 static_cast<int>(std::ranges::count_if(match.enemies(),
                     [](const HordeEnemy& enemy) {
                         return isEnemyAlive(enemy.enemy);
                     }))),
        529, 52, 14, Color { 226, 123, 78, 255 });

    if (currentRoom != nullptr) {
        const char* label = TextFormat("%s  %02i",
            roomRoleName(currentRoom->role), currentRoom->id + 1);
        const int width = MeasureText(label, 18) + 30;
        const Rectangle roomPanel {
            static_cast<float>(GetScreenWidth() - width - 18),
            18.0F, static_cast<float>(width), 38.0F
        };
        drawHudPanel(roomPanel, roomRoleColor(currentRoom->role));
        DrawText(label, GetScreenWidth() - width - 1, 28, 18,
            roomRoleColor(currentRoom->role));
    }

    const Vector2 playerMapPosition {
        player.position.x, player.position.z
    };
    const auto anchorGate = std::ranges::find(
        match.plan().gates, GatePurpose::Anchor, &MapGate::purpose);
    const bool anchorRouteOpen = anchorGate == match.plan().gates.end()
        || anchorGate->open;
    const char* interactionPrompt = nullptr;
    float nearestGate = 2.2F;
    const auto thresholds = level.doorwayThresholds();
    for (const MapGate& gate : match.plan().gates) {
        if (gate.open || gate.doorway >= thresholds.size()) {
            continue;
        }
        const Vector2 closest = closestPointOnSegment(
            playerMapPosition, thresholds[gate.doorway].segment).position;
        const float x = closest.x - playerMapPosition.x;
        const float y = closest.y - playerMapPosition.y;
        const float distance = std::sqrt(x * x + y * y);
        if (distance > nearestGate) {
            continue;
        }
        nearestGate = distance;
        if (gate.purpose == GatePurpose::Exit) {
            interactionPrompt = "EXIT SEALED  -  POWER THE HUB";
        } else if (gate.purpose == GatePurpose::Reward
            && !anchorRouteOpen) {
            interactionPrompt = "ANCHOR ROUTE REQUIRED BEFORE OPTIONAL SPENDING";
        } else if (match.points() >= gate.cost) {
            interactionPrompt = TextFormat("E  OPEN GATE  %i POINTS", gate.cost);
        } else {
            interactionPrompt = TextFormat("GATE %i  -  NEED %i MORE",
                gate.cost, gate.cost - match.points());
        }
    }
    const auto nearDevice = [&](Vector2 position) {
        const float x = position.x - playerMapPosition.x;
        const float y = position.y - playerMapPosition.y;
        return x * x + y * y <= 2.2F * 2.2F;
    };
    if (interactionPrompt == nullptr
        && nearDevice(match.plan().anchorPosition)
        && !match.anchorIsComplete()) {
        if (match.round() < 2) {
            interactionPrompt = "ANCHOR DORMANT  -  SURVIVE TWO ROUNDS";
        } else if (!anchorRouteOpen && anchorGate != match.plan().gates.end()) {
            interactionPrompt = match.points() >= anchorGate->cost
                ? TextFormat("E  FUND + START HOLDOUT  %i  -  STAY IN RING",
                    anchorGate->cost)
                : TextFormat("ANCHOR ROUTE NEEDS %i MORE POINTS",
                    anchorGate->cost - match.points());
        } else {
            interactionPrompt = "E  START HOLDOUT  -  STAY INSIDE THE GOLD RING";
        }
    }
    if (interactionPrompt == nullptr
        && nearDevice(match.plan().hubPosition)) {
        interactionPrompt = match.anchorIsComplete() && !match.hubIsPowered()
            ? "E  POWER THE HUB"
            : !anchorRouteOpen
                ? "OPEN THE ANCHOR ROUTE TO ENABLE UPGRADES"
                : "UPGRADES  1 DAMAGE 1200 | 2 FIRE 1000 | 3 DASH 900";
    }
    if (interactionPrompt == nullptr
        && nearDevice(match.plan().exitPosition)) {
        interactionPrompt = match.hubIsPowered()
                && match.round() >= HORDE_FINAL_ROUND
            ? "E  COMPLETE THE MAP"
            : "EXIT DORMANT";
    }
    const auto rewardGate = std::ranges::find(
        match.plan().gates, GatePurpose::Reward, &MapGate::purpose);
    const bool rewardRouteOpen = rewardGate == match.plan().gates.end()
        || rewardGate->open;
    const bool nearRelay = std::ranges::any_of(
        match.plan().relayTargets, [&](const RelayTarget& relay) {
            return nearDevice(relay.position);
        });
    if (interactionPrompt == nullptr && nearRelay && match.hubIsPowered()
        && rewardRouteOpen && !match.relayIsComplete()) {
        interactionPrompt
            = "SHOOT THE GOLD RELAY  -  A WRONG TARGET RESETS THE SEQUENCE";
    }
    if (interactionPrompt != nullptr) {
        const int promptWidth = MeasureText(interactionPrompt, 18) + 30;
        const Rectangle promptPanel {
            static_cast<float>(GetScreenWidth() / 2 - promptWidth / 2),
            static_cast<float>(GetScreenHeight() - 112),
            static_cast<float>(promptWidth), 42.0F
        };
        drawHudPanel(promptPanel, MACHINE_GOLD);
        DrawText(interactionPrompt,
            GetScreenWidth() / 2 - promptWidth / 2 + 16,
            GetScreenHeight() - 100, 18, MACHINE_GOLD);
    }

    const char* objective = "Survive Round 1 and earn the first gate";
    if (match.anchorIsActive()) {
        objective = TextFormat(
            "STAY IN THE GOLD RING  %.1f / %.1f  -  LEAVING PAUSES",
            match.anchorProgress(), ANCHOR_HOLDOUT_DURATION);
    } else if (!match.anchorIsComplete() && match.round() >= 2) {
        objective
            = "At the Anchor: press E, then stay in its gold ring during the wave";
    } else if (match.anchorIsComplete() && !match.hubIsPowered()) {
        objective = "Return to the Hub and press E to power the Exit";
    } else if (match.hubIsPowered() && match.round() < HORDE_FINAL_ROUND) {
        objective = "Press N to survive the remaining rounds";
    } else if (match.hubIsPowered() && !match.matchIsComplete()) {
        objective = "Reach the Exit monument and press E";
    }
    const Rectangle objectivePanel {
        16.0F, static_cast<float>(GetScreenHeight() - 60),
        700.0F, 42.0F
    };
    drawHudPanel(objectivePanel, Color { 187, 145, 57, 255 });
    DrawText("DIRECTIVE", 31, GetScreenHeight() - 49, 12,
        Color { 132, 165, 164, 255 });
    DrawText(objective, 115, GetScreenHeight() - 50, 16,
        Color { 224, 211, 158, 255 });

    const bool progressionAllowsRound
        = !(match.round() >= 2 && !match.anchorIsComplete()
                && !match.anchorIsActive())
        && !(match.round() >= 3 && match.anchorIsComplete()
            && !match.hubIsPowered());
    if (match.phase() == RoundPhase::Intermission
        && match.round() < HORDE_FINAL_ROUND && isPlayerAlive(player)
        && progressionAllowsRound) {
        DrawText("[ N ]  DEPLOY NEXT WAVE", GetScreenWidth() - 280,
            GetScreenHeight() - 46, 18, ENERGY_CYAN);
    }

    if (showDebug) {
        DrawRectangleRounded(Rectangle { 16.0F, 90.0F, 720.0F, 142.0F },
            0.08F, 6, Color { 7, 17, 24, 225 });
        DrawText("F3 HIDE DEBUG", 28, 101, 17,
            Color { 151, 193, 190, 255 });
        DrawText("WASD | SPACE dash | LMB fire | E interact | N round | 1/2/3 upgrades",
            28, 127, 15, Color { 180, 203, 200, 255 });
        DrawText("R reset | F1 regression arena | F2 overview / recipe browser",
            28, 150, 15, Color { 180, 203, 200, 255 });
        DrawText(TextFormat("%s  rooms %i  doors %i  room %i  walls %i",
                     smallMapRecipeName(level.roomLayout().getSmallMapRecipe()),
                     static_cast<int>(level.roomLayout().getRoomCount()),
                     static_cast<int>(level.roomLayout().getDoorways().size()),
                     currentRegion, static_cast<int>(session.activeWalls().size())),
            28, 177, 15, Color { 224, 211, 158, 255 });
        DrawText(TextFormat("anchor %.1f  relay %i/%i  damage %i  shots %i/%i",
                     match.anchorProgress(),
                     static_cast<int>(match.relayProgress()),
                     static_cast<int>(match.plan().relayTargets.size()),
                     match.weaponDamage(),
                     static_cast<int>(match.playerProjectiles().activeCount()),
                     static_cast<int>(match.enemyProjectiles().activeCount())),
            28, 201, 15, Color { 180, 203, 200, 255 });
        DrawFPS(GetScreenWidth() - 96, 70);
    }

    const char* statusMessage = nullptr;
    Color statusColor { 255, 174, 92, 255 };
    if (!isPlayerAlive(player)) {
        statusMessage = "DEFEATED  -  press R to restart";
    } else if (match.matchIsComplete()) {
        statusMessage = "MAP COMPLETE  -  press R to restart";
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
    const LevelSession* session, const HordeMatch* match,
    std::size_t selectedConfiguration,
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

    if (match != nullptr) {
        const auto drawSite = [&](Vector2 world, const char* label, Color color) {
            const Vector2 site = overviewPoint(world, transform);
            DrawCircleV(site, 11.0F, Color { 7, 17, 24, 235 });
            DrawCircleLines(static_cast<int>(site.x), static_cast<int>(site.y),
                12.0F, color);
            const int width = MeasureText(label, 13);
            DrawText(label, static_cast<int>(site.x) - width / 2,
                static_cast<int>(site.y) - 6, 13, color);
        };
        drawSite(match->plan().anchorPosition, "A",
            match->anchorIsComplete()
                ? Color { 151, 231, 190, 255 }
                : Color { 255, 211, 91, 255 });
        drawSite(match->plan().hubPosition, "H",
            match->hubIsPowered()
                ? Color { 92, 225, 255, 255 }
                : Color { 151, 193, 190, 255 });
        drawSite(match->plan().exitPosition, "X",
            match->hubIsPowered()
                ? Color { 151, 231, 190, 255 }
                : Color { 170, 120, 115, 255 });
        for (std::size_t relay = 0;
             relay < match->plan().relayTargets.size(); ++relay) {
            drawSite(match->plan().relayTargets[relay].position,
                TextFormat("%i", static_cast<int>(relay + 1)),
                relay < match->relayProgress()
                    ? Color { 151, 231, 190, 255 }
                    : Color { 125, 162, 211, 255 });
        }
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
    DrawText("MAP RECIPE", detailsX, detailsY, 16, heading);
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
    DrawText("puzzle graph: anchor -> hub -> exit", detailsX, detailsY,
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
    float interpolationAmount, bool showDebug)
{
    ensurePostProcessTargets();
    updateLighting(camera, ARENA_BACKGROUND);
    clearLocalLights();

    BeginTextureMode(sceneTarget);
    ClearBackground(ARENA_BACKGROUND);
    BeginMode3D(camera);
    drawVoidGrid();
    setMaterial(1.0F);
    DrawModel(groundModel, Vector3 { 0.0F, -0.015F, 0.0F }, 1.0F,
        Color { 48, 65, 68, 255 });
    setMaterial(2.0F);
    drawArena();
    drawWorldReticle(aimPoint);
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
    EndTextureMode();

    buildBloom();
    BeginDrawing();
    const float damageAmount = std::clamp(
        player.hitFlashRemaining / PLAYER_HIT_FLASH_DURATION, 0.0F, 1.0F);
    const float dashAmount = std::clamp(
        player.dashRemaining / PLAYER_DASH_DURATION, 0.0F, 1.0F);
    drawPostProcessedScene(damageAmount, dashAmount, 0.0F);

    drawPlayerHud(player);
    const Rectangle trainingPanel {
        static_cast<float>(GetScreenWidth() - 238),
        18.0F, 220.0F, 38.0F
    };
    drawHudPanel(trainingPanel, Color { 180, 112, 220, 255 });
    DrawText("COMBAT SIMULATION", GetScreenWidth() - 218, 28, 18,
        Color { 201, 160, 229, 255 });

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
    setMaterial(0.0F);
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
    setMaterial(0.0F);
    const Vector2 position = interpolateEnemyPosition(
        enemy, interpolationAmount);
    const Vector3 center { position.x, ENEMY_RADIUS, position.y };
    const Vector3 groundCenter { position.x, 0.025F, position.y };
    if (!isEnemyAlive(enemy)) {
        DrawCircle3D(groundCenter, ENEMY_RADIUS * 1.8F,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 151, 231, 190, 210 });
        DrawModel(enemyModel, center, 0.55F, Color { 72, 74, 81, 255 });
        DrawSphereWires(center, ENEMY_RADIUS * 1.2F,
            8, 12, Color { 186, 244, 211, 190 });
        return;
    }

    drawActorShadow(center, ENEMY_RADIUS);
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

void PrototypeRenderer::drawHordeEnemy(
    const HordeEnemy& entry, float interpolationAmount) const
{
    setMaterial(0.0F);
    const Enemy& enemy = entry.enemy;
    const Vector2 position = interpolateEnemyPosition(
        enemy, interpolationAmount);
    const Vector3 center { position.x, ENEMY_RADIUS, position.y };
    const Vector3 ground { position.x, 0.026F, position.y };

    Color color { 188, 73, 57, 255 };
    Color accent { 255, 116, 61, 255 };
    Vector3 scale { 0.88F, 0.88F, 0.88F };
    switch (entry.role) {
    case HordeEnemyRole::Drifter:
        break;
    case HordeEnemyRole::Runner:
        color = Color { 213, 104, 43, 255 };
        accent = Color { 255, 180, 62, 255 };
        scale = Vector3 { 0.62F, 0.58F, 1.05F };
        break;
    case HordeEnemyRole::Caster:
        color = Color { 112, 61, 153, 255 };
        accent = Color { 210, 109, 239, 255 };
        scale = Vector3 { 0.72F, 1.12F, 0.72F };
        break;
    case HordeEnemyRole::Elite:
        color = Color { 157, 69, 148, 255 };
        accent = Color { 255, 103, 205, 255 };
        scale = Vector3 { 1.22F, 1.3F, 1.22F };
        break;
    }
    if (!isEnemyAlive(enemy)) {
        return;
    }
    drawActorShadow(center, ENEMY_RADIUS);
    if (enemy.hitFlashRemaining > 0.0F) {
        color = Color { 255, 238, 194, 255 };
        accent = WHITE;
    }
    const float footprint = std::max(scale.x, scale.z);
    DrawCircle3D(ground, ENEMY_RADIUS * footprint * 1.16F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { accent.r, accent.g, accent.b, 115 });
    const float facingAngle = -std::atan2(
        enemy.facing.y, enemy.facing.x) * RAD2DEG;
    const Model* bodyModel = &enemyModel;
    switch (entry.role) {
    case HordeEnemyRole::Drifter:
        break;
    case HordeEnemyRole::Runner:
        bodyModel = &runnerModel;
        break;
    case HordeEnemyRole::Caster:
        bodyModel = &casterModel;
        break;
    case HordeEnemyRole::Elite:
        bodyModel = &eliteModel;
        break;
    }
    DrawModelEx(*bodyModel, center, Vector3 { 0.0F, 1.0F, 0.0F },
        facingAngle, scale, color);

    if (entry.role == HordeEnemyRole::Drifter) {
        const Vector3 eye {
            center.x + enemy.facing.x * 0.66F,
            center.y + 0.08F,
            center.z + enemy.facing.y * 0.66F
        };
        DrawCylinderEx(center, eye, 0.13F, 0.045F, 7, accent);
        DrawSphere(eye, 0.085F, accent);
    } else if (entry.role == HordeEnemyRole::Runner) {
        const Vector3 snout {
            center.x + enemy.facing.x * 0.85F,
            center.y * 0.72F,
            center.z + enemy.facing.y * 0.85F
        };
        DrawCylinderEx(center, snout, 0.2F, 0.04F, 7, accent);
        const Vector3 tail {
            center.x - enemy.facing.x * 0.92F,
            center.y * 0.72F,
            center.z - enemy.facing.y * 0.92F
        };
        DrawCylinderEx(center, tail, 0.11F, 0.025F, 6,
            Color { accent.r, accent.g, accent.b, 170 });
    } else if (entry.role == HordeEnemyRole::Caster) {
        DrawCircle3D(Vector3 { center.x, center.y + 0.64F, center.z },
            0.55F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F, accent);
        DrawSphere(Vector3 { center.x, center.y + 0.64F, center.z },
            0.12F, accent);
    } else if (entry.role == HordeEnemyRole::Elite) {
        DrawCircle3D(Vector3 { center.x, center.y + 0.12F, center.z },
            ENEMY_RADIUS * 1.38F,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F, accent);
        DrawSphereWires(center, ENEMY_RADIUS * 1.4F,
            7, 10, Color { accent.r, accent.g, accent.b, 180 });
    }

    if (entry.role == HordeEnemyRole::Caster
        || entry.role == HordeEnemyRole::Elite) {
        const Vector3 aimEnd {
            center.x + enemy.facing.x * 1.3F,
            center.y,
            center.z + enemy.facing.y * 1.3F
        };
        DrawCylinderEx(center, aimEnd, 0.13F, 0.035F, 8, accent);
    }
}

void PrototypeRenderer::drawHordeLandmarks(const GeneratedLevel& level,
    const LevelSession& session, const HordeMatch& match) const
{
    const float pulse = 0.5F + 0.5F * std::sin(
        static_cast<float>(GetTime()) * 2.4F);
    constexpr Color darkMetal { 31, 42, 48, 255 };
    constexpr Color edgeMetal { 76, 91, 94, 255 };

    const Vector2 hub = match.plan().hubPosition;
    const Color hubEnergy = match.hubIsPowered()
        ? ENERGY_CYAN
        : Color { 56, 112, 116, 220 };
    drawRadialFloorDecal(hub, 1.55F, 8,
        Color { hubEnergy.r, hubEnergy.g, hubEnergy.b, 145 });
    DrawCylinder(Vector3 { hub.x, 0.16F, hub.y },
        1.05F, 0.9F, 0.32F, 12, darkMetal);
    DrawCircle3D(Vector3 { hub.x, 0.035F, hub.y }, 1.3F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { hubEnergy.r, hubEnergy.g, hubEnergy.b, 145 });
    DrawCylinder(Vector3 { hub.x, 1.35F, hub.y },
        0.62F, 0.44F, 2.4F, 10, edgeMetal);
    DrawCylinder(Vector3 { hub.x, 1.45F, hub.y },
        0.38F, 0.3F, 2.25F, 10, darkMetal);
    for (const float height : { 0.65F, 1.45F, 2.2F }) {
        DrawCircle3D(Vector3 { hub.x, height, hub.y },
            0.58F + pulse * 0.05F,
            Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { hubEnergy.r, hubEnergy.g, hubEnergy.b, 215 });
    }
    DrawSphere(Vector3 { hub.x, 2.72F, hub.y },
        0.3F + pulse * 0.04F, hubEnergy);

    const Vector2 anchor = match.plan().anchorPosition;
    const float anchorAmount = std::clamp(
        match.anchorProgress() / ANCHOR_HOLDOUT_DURATION, 0.0F, 1.0F);
    const Color anchorEnergy = match.anchorIsComplete()
        ? Color { 112, 229, 185, 255 }
        : match.anchorIsActive()
            ? MACHINE_GOLD
            : Color { 164, 102, 43, 220 };
    drawRadialFloorDecal(anchor, 1.25F, 6,
        Color { anchorEnergy.r, anchorEnergy.g, anchorEnergy.b, 150 });
    DrawCylinder(Vector3 { anchor.x, 0.14F, anchor.y },
        0.95F, 0.78F, 0.28F, 8, darkMetal);
    DrawCylinder(Vector3 { anchor.x, 0.95F, anchor.y },
        0.48F, 0.32F, 1.7F, 8, edgeMetal);
    DrawSphere(Vector3 { anchor.x, 1.72F, anchor.y },
        0.3F + anchorAmount * 0.08F, anchorEnergy);
    for (const Vector2 offset : { Vector2 { -0.62F, 0.0F },
             Vector2 { 0.62F, 0.0F }, Vector2 { 0.0F, -0.62F },
             Vector2 { 0.0F, 0.62F } }) {
        DrawCylinderEx(Vector3 { anchor.x + offset.x, 0.18F,
                           anchor.y + offset.y },
            Vector3 { anchor.x + offset.x * 0.62F, 1.35F,
                anchor.y + offset.y * 0.62F },
            0.1F, 0.055F, 7, anchorEnergy);
    }
    DrawCircle3D(Vector3 { anchor.x, 0.04F, anchor.y },
        ANCHOR_HOLDOUT_RADIUS,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { anchorEnergy.r, anchorEnergy.g, anchorEnergy.b,
            static_cast<unsigned char>(95 + anchorAmount * 130.0F) });
    DrawCircle3D(Vector3 { anchor.x, 0.045F, anchor.y },
        ANCHOR_HOLDOUT_RADIUS - 0.16F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { anchorEnergy.r, anchorEnergy.g, anchorEnergy.b, 70 });

    const Vector2 exit = match.plan().exitPosition;
    const Color exitEnergy = match.hubIsPowered()
        ? ENERGY_CYAN
        : Color { 86, 71, 62, 220 };
    drawRadialFloorDecal(exit, 1.35F, 4,
        Color { exitEnergy.r, exitEnergy.g, exitEnergy.b, 140 });
    DrawCube(Vector3 { exit.x - 0.72F, 1.55F, exit.y },
        0.48F, 3.1F, 0.82F, edgeMetal);
    DrawCube(Vector3 { exit.x + 0.72F, 1.55F, exit.y },
        0.48F, 3.1F, 0.82F, edgeMetal);
    DrawCube(Vector3 { exit.x, 2.9F, exit.y },
        1.9F, 0.42F, 0.82F, darkMetal);
    DrawCubeWires(Vector3 { exit.x, 1.6F, exit.y },
        1.05F, 2.35F, 0.16F, exitEnergy);
    DrawCircle3D(Vector3 { exit.x, 0.04F, exit.y }, 1.15F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
        Color { exitEnergy.r, exitEnergy.g, exitEnergy.b, 150 });

    for (std::size_t target = 0;
         target < match.plan().relayTargets.size(); ++target) {
        const RelayTarget& relay = match.plan().relayTargets[target];
        Color energy { 66, 103, 143, 255 };
        if (match.relayIsComplete() || target < match.relayProgress()) {
            energy = Color { 112, 229, 185, 255 };
        } else if (target == match.relayProgress()) {
            energy = MACHINE_GOLD;
        }
        drawRadialFloorDecal(relay.position, 0.62F, 4,
            Color { energy.r, energy.g, energy.b, 120 });
        DrawCylinder(Vector3 { relay.position.x, 0.24F,
                         relay.position.y },
            0.48F, 0.36F, 0.48F, 8, darkMetal);
        DrawCylinder(Vector3 { relay.position.x, 0.78F,
                         relay.position.y },
            0.12F, 0.12F, 0.92F, 7, edgeMetal);
        DrawSphere(Vector3 { relay.position.x, 1.25F,
                       relay.position.y },
            0.25F + (target == match.relayProgress() ? pulse * 0.04F : 0.0F),
            energy);
        DrawCircle3D(Vector3 { relay.position.x, 1.25F,
                         relay.position.y },
            0.43F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { energy.r, energy.g, energy.b, 210 });
    }

    const auto thresholds = level.doorwayThresholds();
    for (const MapGate& gate : match.plan().gates) {
        if (gate.doorway >= thresholds.size()) {
            continue;
        }
        const DoorwayThreshold& threshold = thresholds[gate.doorway];
        const bool locked = session.doorwayIsLocked(gate.doorway);
        const Color gateEnergy = locked
            ? gate.purpose == GatePurpose::Exit
                ? Color { 157, 79, 186, 255 }
                : Color { 224, 104, 55, 255 }
            : Color { 74, 211, 170, 210 };
        const Vector2 gateDirection {
            threshold.segment.end.x - threshold.segment.start.x,
            threshold.segment.end.y - threshold.segment.start.y
        };
        const float gateLength = std::sqrt(
            gateDirection.x * gateDirection.x
            + gateDirection.y * gateDirection.y);
        if (gateLength > 0.0001F) {
            const Vector2 gateNormal {
                -gateDirection.y / gateLength,
                gateDirection.x / gateLength
            };
            for (int stripe = 0; stripe < 7; ++stripe) {
                const float amount = (static_cast<float>(stripe) + 0.5F)
                    / 7.0F;
                const Vector2 stripeCenter {
                    threshold.segment.start.x + gateDirection.x * amount,
                    threshold.segment.start.y + gateDirection.y * amount
                };
                const Color stripeColor = stripe % 2 == 0
                    ? gateEnergy : Color { 32, 40, 43, 230 };
                DrawCylinderEx(Vector3 {
                                   stripeCenter.x - gateNormal.x * 0.28F,
                                   0.045F,
                                   stripeCenter.y - gateNormal.y * 0.28F },
                    Vector3 {
                        stripeCenter.x + gateNormal.x * 0.28F,
                        0.045F,
                        stripeCenter.y + gateNormal.y * 0.28F },
                    0.035F, 0.035F, 6, stripeColor);
            }
        }
        for (const Vector2 endpoint
            : { threshold.segment.start, threshold.segment.end }) {
            DrawCylinder(Vector3 { endpoint.x, 1.1F, endpoint.y },
                0.22F, 0.18F, 2.2F, 8, darkMetal);
            DrawCylinder(Vector3 { endpoint.x, 1.1F, endpoint.y },
                0.11F, 0.11F, 2.05F, 8, gateEnergy);
            DrawCylinder(Vector3 { endpoint.x, 0.1F, endpoint.y },
                0.34F, 0.28F, 0.2F, 8, edgeMetal);
        }
        const auto gateBar = [&](float height, float radius, Color color) {
            DrawCylinderEx(Vector3 { threshold.segment.start.x, height,
                               threshold.segment.start.y },
                Vector3 { threshold.segment.end.x, height,
                    threshold.segment.end.y },
                radius, radius, 8, color);
        };
        gateBar(2.18F, 0.16F, darkMetal);
        gateBar(2.19F, 0.07F, gateEnergy);
        if (locked) {
            const Color barrier {
                gateEnergy.r, gateEnergy.g, gateEnergy.b, 210
            };
            gateBar(0.48F, 0.045F, barrier);
            gateBar(0.86F, 0.045F, barrier);
            gateBar(1.24F, 0.045F, barrier);
            gateBar(1.62F, 0.045F, barrier);
        }
    }
}

void PrototypeRenderer::drawPlayer(const Player& player) const
{
    setMaterial(0.0F);
    constexpr Color armorColor { 45, 58, 64, 255 };
    constexpr Color armorEdge { 103, 119, 119, 255 };
    const Color bodyColor = player.hitFlashRemaining > 0.0F
        ? Color { 255, 245, 210, 255 }
        : isPlayerAlive(player)
            ? Color { 211, 142, 43, 255 }
            : Color { 77, 65, 61, 255 };
    const float bodyScale = isPlayerAlive(player) ? 1.0F : 0.65F;
    const Vector2 side { -player.facing.y, player.facing.x };

    drawActorShadow(player.position, PLAYER_RADIUS * bodyScale);
    if (isPlayerAlive(player)) {
        DrawCircle3D(Vector3 {
                         player.position.x, 0.024F, player.position.z },
            PLAYER_RADIUS * 1.16F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 171, 111, 29, 175 });
        if (player.dashRemaining > 0.0F) {
            DrawCircle3D(Vector3 {
                             player.position.x, 0.03F, player.position.z },
                PLAYER_RADIUS * 1.7F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
                Color { ENERGY_CYAN.r, ENERGY_CYAN.g,
                    ENERGY_CYAN.b, 175 });
            const Vector3 dashTail {
                player.position.x - player.dashDirection.x * 1.15F,
                player.position.y * 0.85F,
                player.position.z - player.dashDirection.y * 1.15F
            };
            DrawCylinderEx(dashTail, player.position,
                0.18F, 0.04F, 8,
                Color { ENERGY_CYAN.r, ENERGY_CYAN.g,
                    ENERGY_CYAN.b, 115 });
        }
    }
    DrawModelEx(playerModel, player.position,
        Vector3 { 0.0F, 1.0F, 0.0F }, 0.0F,
        Vector3 { bodyScale, bodyScale * 0.82F, bodyScale }, bodyColor);
    for (const float sign : { -1.0F, 1.0F }) {
        const Vector3 shoulder {
            player.position.x + side.x * 0.5F * sign,
            player.position.y + 0.04F,
            player.position.z + side.y * 0.5F * sign
        };
        DrawSphere(shoulder, 0.22F * bodyScale, armorColor);
        DrawSphereWires(shoulder, 0.225F * bodyScale,
            6, 8, armorEdge);
    }
    if (player.invulnerabilityRemaining > 0.0F) {
        const float shieldScale = 1.12F
            + 0.16F * (player.invulnerabilityRemaining
                / PLAYER_INVULNERABILITY_DURATION);
        DrawSphereWires(player.position, PLAYER_RADIUS * shieldScale,
            10, 14, Color { ENERGY_CYAN.r, ENERGY_CYAN.g,
                ENERGY_CYAN.b, 190 });
    }
    if (!isPlayerAlive(player)) {
        DrawCircle3D(Vector3 {
                         player.position.x, 0.025F, player.position.z },
            PLAYER_RADIUS * 1.8F, Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
            Color { 255, 113, 74, 210 });
        return;
    }

    const Vector3 noseStart {
        player.position.x + player.facing.x * PLAYER_RADIUS * 0.42F,
        player.position.y + 0.06F,
        player.position.z + player.facing.y * PLAYER_RADIUS * 0.42F
    };
    const Vector3 noseEnd {
        player.position.x + player.facing.x * PLAYER_FACING_MARKER_DISTANCE,
        player.position.y + 0.06F,
        player.position.z + player.facing.y * PLAYER_FACING_MARKER_DISTANCE
    };
    DrawCylinderEx(noseStart, noseEnd, 0.15F, 0.045F, 8, armorColor);
    DrawCylinderEx(Vector3 {
                       noseStart.x + side.x * 0.22F,
                       noseStart.y + 0.2F,
                       noseStart.z + side.y * 0.22F },
        Vector3 {
            noseStart.x - side.x * 0.22F,
            noseStart.y + 0.2F,
            noseStart.z - side.y * 0.22F },
        0.075F, 0.075F, 7, ENERGY_CYAN);
    DrawSphere(noseEnd, 0.09F, ENERGY_CYAN);
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

void PrototypeRenderer::updateGeneratedLights(const HordeMatch& match) const
{
    const Vector2 hub = match.plan().hubPosition;
    const Vector2 anchor = match.plan().anchorPosition;
    const float hubPosition[3] { hub.x, 1.45F, hub.y };
    const float anchorPosition[3] { anchor.x, 1.2F, anchor.y };
    const float hubColor[3] {
        match.hubIsPowered() ? 0.08F : 0.015F,
        match.hubIsPowered() ? 1.15F : 0.08F,
        match.hubIsPowered() ? 1.35F : 0.10F
    };
    const float anchorColor[3] {
        match.anchorIsActive() ? 1.35F
            : match.anchorIsComplete() ? 0.25F : 0.12F,
        match.anchorIsActive() ? 0.72F
            : match.anchorIsComplete() ? 0.82F : 0.055F,
        match.anchorIsActive() ? 0.12F
            : match.anchorIsComplete() ? 0.48F : 0.02F
    };
    SetShaderValue(lightingShader, pointLightPositionALocation,
        hubPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightColorALocation,
        hubColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightPositionBLocation,
        anchorPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightColorBLocation,
        anchorColor, SHADER_UNIFORM_VEC3);
}

void PrototypeRenderer::clearLocalLights() const
{
    constexpr float empty[3] { 0.0F, 0.0F, 0.0F };
    SetShaderValue(lightingShader, pointLightPositionALocation,
        empty, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightColorALocation,
        empty, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightPositionBLocation,
        empty, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, pointLightColorBLocation,
        empty, SHADER_UNIFORM_VEC3);
}

void PrototypeRenderer::setMaterial(float kind) const
{
    SetShaderValue(lightingShader, materialKindLocation,
        &kind, SHADER_UNIFORM_FLOAT);
}

void PrototypeRenderer::ensurePostProcessTargets()
{
    const int width = std::max(GetScreenWidth(), 1);
    const int height = std::max(GetScreenHeight(), 1);
    if (sceneTarget.id != 0
        && width == postProcessWidth && height == postProcessHeight) {
        return;
    }
    if (sceneTarget.id != 0) {
        UnloadRenderTexture(sceneTarget);
        UnloadRenderTexture(bloomTargetA);
        UnloadRenderTexture(bloomTargetB);
    }

    postProcessWidth = width;
    postProcessHeight = height;
    sceneTarget = LoadRenderTexture(width, height);
    bloomTargetA = LoadRenderTexture(
        std::max(width / 2, 1), std::max(height / 2, 1));
    bloomTargetB = LoadRenderTexture(
        std::max(width / 2, 1), std::max(height / 2, 1));
    for (Texture2D texture : { sceneTarget.texture,
             bloomTargetA.texture, bloomTargetB.texture }) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);
    }
}

void PrototypeRenderer::buildBloom()
{
    const float bloomWidth = static_cast<float>(bloomTargetA.texture.width);
    const float bloomHeight = static_cast<float>(bloomTargetA.texture.height);

    BeginTextureMode(bloomTargetA);
    ClearBackground(BLANK);
    BeginShaderMode(bloomExtractShader);
    drawRenderTexture(sceneTarget, bloomWidth, bloomHeight);
    EndShaderMode();
    EndTextureMode();

    for (int pass = 0; pass < 3; ++pass) {
        const float radius = 1.0F + static_cast<float>(pass) * 0.55F;
        const float horizontal[2] { radius / bloomWidth, 0.0F };
        SetShaderValue(bloomBlurShader, blurDirectionLocation,
            horizontal, SHADER_UNIFORM_VEC2);
        BeginTextureMode(bloomTargetB);
        ClearBackground(BLANK);
        BeginShaderMode(bloomBlurShader);
        drawRenderTexture(bloomTargetA, bloomWidth, bloomHeight);
        EndShaderMode();
        EndTextureMode();

        const float vertical[2] { 0.0F, radius / bloomHeight };
        SetShaderValue(bloomBlurShader, blurDirectionLocation,
            vertical, SHADER_UNIFORM_VEC2);
        BeginTextureMode(bloomTargetA);
        ClearBackground(BLANK);
        BeginShaderMode(bloomBlurShader);
        drawRenderTexture(bloomTargetB, bloomWidth, bloomHeight);
        EndShaderMode();
        EndTextureMode();
    }
}

void PrototypeRenderer::drawPostProcessedScene(float damageAmount,
    float dashAmount, float energyPulse) const
{
    const float resolution[2] {
        static_cast<float>(postProcessWidth),
        static_cast<float>(postProcessHeight)
    };
    const float time = static_cast<float>(GetTime());
    SetShaderValue(compositeShader, compositeResolutionLocation,
        resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(compositeShader, compositeTimeLocation,
        &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeDamageLocation,
        &damageAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeDashLocation,
        &dashAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeEnergyLocation,
        &energyPulse, SHADER_UNIFORM_FLOAT);

    BeginShaderMode(compositeShader);
    drawRenderTexture(sceneTarget,
        static_cast<float>(postProcessWidth),
        static_cast<float>(postProcessHeight));
    EndShaderMode();

    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(bloomTargetA.texture,
        Rectangle { 0.0F, 0.0F,
            static_cast<float>(bloomTargetA.texture.width),
            -static_cast<float>(bloomTargetA.texture.height) },
        Rectangle { 0.0F, 0.0F,
            static_cast<float>(postProcessWidth),
            static_cast<float>(postProcessHeight) },
        Vector2 {}, 0.0F, Color { 255, 255, 255, 145 });
    EndBlendMode();
}

void PrototypeRenderer::drawPlayerHud(const Player& player) const
{
    constexpr float panelWidth = 260.0F;
    constexpr float meterX = 101.0F;
    constexpr float meterWidth = 156.0F;
    constexpr float segmentGap = 4.0F;
    const float dashAmount = 1.0F - std::clamp(
        player.dashCooldownRemaining / PLAYER_DASH_COOLDOWN, 0.0F, 1.0F);
    const Color healthColor = player.health <= 1
        ? Color { 241, 78, 55, 255 }
        : MACHINE_GOLD;

    drawHudPanel(Rectangle { 16.0F, 16.0F, panelWidth, 64.0F },
        healthColor);
    DrawText("VITAL", 30, 25, 14, Color { 132, 165, 164, 255 });
    const float segmentWidth = (meterWidth
        - segmentGap * static_cast<float>(PLAYER_MAX_HEALTH - 1))
        / static_cast<float>(PLAYER_MAX_HEALTH);
    for (int segment = 0; segment < PLAYER_MAX_HEALTH; ++segment) {
        const float x = meterX
            + static_cast<float>(segment) * (segmentWidth + segmentGap);
        DrawRectangleRounded(Rectangle { x, 27.0F, segmentWidth, 14.0F },
            0.22F, 5,
            segment < player.health ? healthColor
                                    : Color { 25, 38, 43, 255 });
    }

    DrawText("BOOST", 30, 52, 13, Color { 132, 165, 164, 255 });
    DrawRectangleRounded(Rectangle { meterX, 54.0F, meterWidth, 10.0F },
        0.45F, 7, Color { 25, 38, 43, 255 });
    if (dashAmount > 0.0F) {
        DrawRectangleRounded(Rectangle {
                                 meterX, 54.0F,
                                 meterWidth * dashAmount, 10.0F },
            0.45F, 7, ENERGY_CYAN);
    }
    if (dashAmount >= 0.999F) {
        DrawText("READY", 214, 52, 10, Color { 204, 255, 245, 255 });
    }
}
