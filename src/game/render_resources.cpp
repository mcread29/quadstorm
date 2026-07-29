#include "render_resources.hpp"

#include "combat.hpp"
#include "directional_shader.hpp"
#include "render_style.hpp"
#include "target.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {

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
        const Color color = game_render::generatedFloorColor(
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

Mesh makeGeneratedFloorDetailMesh(const GeneratedLevel& level)
{
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    const auto assignments = level.roomLayout().getCellAssignments();
    const float scale = level.worldScale();

    const auto appendVertex = [&](Vector3 point, Color color) {
        vertices.insert(vertices.end(), { point.x, point.y, point.z });
        normals.insert(normals.end(), { 0.0F, 1.0F, 0.0F });
        texcoords.insert(texcoords.end(), { 0.0F, 0.0F });
        colors.insert(colors.end(), { color.r, color.g, color.b, color.a });
    };
    const auto appendStrip = [&](Vector2 start, Vector2 end,
                                 float width, Color color) {
        const float x = end.x - start.x;
        const float z = end.y - start.y;
        const float length = std::sqrt(x * x + z * z);
        if (length <= 0.0001F) {
            return;
        }
        const Vector2 side {
            -z / length * width * 0.5F,
            x / length * width * 0.5F
        };
        constexpr float height = 0.024F;
        const Vector3 first { start.x + side.x, height, start.y + side.y };
        const Vector3 second { end.x + side.x, height, end.y + side.y };
        const Vector3 third { end.x - side.x, height, end.y - side.y };
        const Vector3 fourth { start.x - side.x, height, start.y - side.y };
        appendVertex(first, color);
        appendVertex(second, color);
        appendVertex(third, color);
        appendVertex(first, color);
        appendVertex(third, color);
        appendVertex(fourth, color);
    };

    for (const stalberg::DualConnection& connection
        : level.dualGrid().connections) {
        if (connection.cells.a >= assignments.size()
            || connection.cells.b >= assignments.size()) {
            continue;
        }
        const int firstRoom = assignments[connection.cells.a];
        const int secondRoom = assignments[connection.cells.b];
        if (firstRoom == stalberg::rooms::EMPTY_CELL
            || firstRoom != secondRoom) {
            continue;
        }
        const std::uint32_t hash = static_cast<std::uint32_t>(
            connection.cells.a * 2246822519U
            + connection.cells.b * 3266489917U);
        const bool circuitTrace = hash % 11U == 0U;
        if (!circuitTrace) {
            continue;
        }
        appendStrip(
            Vector2 { connection.first.x * scale,
                connection.first.y * scale },
            Vector2 { connection.second.x * scale,
                connection.second.y * scale },
            0.065F, Color { 46, 112, 108, 255 });
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

Mesh makeWallShadowMesh(std::span<const Segment2D> walls)
{
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    vertices.reserve(walls.size() * 12U * 3U);
    normals.reserve(walls.size() * 12U * 3U);
    texcoords.reserve(walls.size() * 12U * 2U);
    colors.reserve(walls.size() * 12U * 4U);

    const auto appendVertex = [&](Vector2 point, Color color) {
        vertices.insert(vertices.end(), { point.x, 0.018F, point.y });
        normals.insert(normals.end(), { 0.0F, 1.0F, 0.0F });
        texcoords.insert(texcoords.end(), { 0.0F, 0.0F });
        colors.insert(colors.end(), { color.r, color.g, color.b, color.a });
    };
    const auto appendQuad = [&](Vector2 first, Vector2 second,
                                Vector2 third, Vector2 fourth, Color color) {
        const float cross = (second.x - first.x) * (third.y - first.y)
            - (second.y - first.y) * (third.x - first.x);
        if (cross < 0.0F) {
            std::swap(second, fourth);
        }
        appendVertex(first, color);
        appendVertex(second, color);
        appendVertex(third, color);
        appendVertex(first, color);
        appendVertex(third, color);
        appendVertex(fourth, color);
    };

    const float projection = game_render::WALL_HEIGHT
        / -DIRECTIONAL_LIGHT.y;
    const Vector2 innerOffset {
        DIRECTIONAL_LIGHT.x * projection * 0.7F,
        DIRECTIONAL_LIGHT.z * projection * 0.7F
    };
    const Vector2 outerOffset {
        DIRECTIONAL_LIGHT.x * projection * 1.08F,
        DIRECTIONAL_LIGHT.z * projection * 1.08F
    };
    for (const Segment2D& wall : walls) {
        const Vector2 innerStart {
            wall.start.x + innerOffset.x,
            wall.start.y + innerOffset.y
        };
        const Vector2 innerEnd {
            wall.end.x + innerOffset.x,
            wall.end.y + innerOffset.y
        };
        const Vector2 outerStart {
            wall.start.x + outerOffset.x,
            wall.start.y + outerOffset.y
        };
        const Vector2 outerEnd {
            wall.end.x + outerOffset.x,
            wall.end.y + outerOffset.y
        };
        appendQuad(wall.start, wall.end, innerEnd, innerStart,
            Color { 2, 6, 9, 54 });
        appendQuad(innerStart, innerEnd, outerEnd, outerStart,
            Color { 4, 9, 12, 19 });
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

    constexpr Color topColor { 126, 143, 139, 255 };
    constexpr Color sideColor { 43, 53, 61, 255 };
    constexpr Color capColor { 75, 96, 98, 255 };
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
            -along.z * game_render::WALL_THICKNESS * 0.5F,
            0.0F,
            along.x * game_render::WALL_THICKNESS * 0.5F
        };
        const Vector3 a0 { wall.start.x + side.x, 0.0F,
            wall.start.y + side.z };
        const Vector3 b0 { wall.end.x + side.x, 0.0F,
            wall.end.y + side.z };
        const Vector3 c0 { wall.end.x - side.x, 0.0F,
            wall.end.y - side.z };
        const Vector3 d0 { wall.start.x - side.x, 0.0F,
            wall.start.y - side.z };
        const Vector3 a1 { a0.x, game_render::WALL_HEIGHT, a0.z };
        const Vector3 b1 { b0.x, game_render::WALL_HEIGHT, b0.z };
        const Vector3 c1 { c0.x, game_render::WALL_HEIGHT, c0.z };
        const Vector3 d1 { d0.x, game_render::WALL_HEIGHT, d0.z };
        const Vector3 leftNormal { side.x / (game_render::WALL_THICKNESS * 0.5F),
            0.0F, side.z / (game_render::WALL_THICKNESS * 0.5F) };
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
            game_render::WALL_HEIGHT * 0.72F, a0.z + leftNormal.z * 0.004F };
        const Vector3 stripB { b0.x + leftNormal.x * 0.004F,
            game_render::WALL_HEIGHT * 0.72F, b0.z + leftNormal.z * 0.004F };
        const Vector3 stripC { stripB.x, game_render::WALL_HEIGHT * 0.76F, stripB.z };
        const Vector3 stripD { stripA.x, game_render::WALL_HEIGHT * 0.76F, stripA.z };
        appendQuad(stripA, stripB, stripC, stripD, leftNormal, stripColor);

        const Vector3 capSide {
            side.x * 1.34F, 0.0F, side.z * 1.34F
        };
        const Vector3 capA { wall.start.x + capSide.x,
            game_render::WALL_HEIGHT - 0.08F, wall.start.y + capSide.z };
        const Vector3 capB { wall.end.x + capSide.x,
            game_render::WALL_HEIGHT - 0.08F, wall.end.y + capSide.z };
        const Vector3 capC { wall.end.x - capSide.x,
            game_render::WALL_HEIGHT - 0.08F, wall.end.y - capSide.z };
        const Vector3 capD { wall.start.x - capSide.x,
            game_render::WALL_HEIGHT - 0.08F, wall.start.y - capSide.z };
        const Vector3 capTopA { capA.x, game_render::WALL_HEIGHT + 0.04F, capA.z };
        const Vector3 capTopB { capB.x, game_render::WALL_HEIGHT + 0.04F, capB.z };
        const Vector3 capTopC { capC.x, game_render::WALL_HEIGHT + 0.04F, capC.z };
        const Vector3 capTopD { capD.x, game_render::WALL_HEIGHT + 0.04F, capD.z };
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
            leftNormal.x * game_render::WALL_THICKNESS * 0.95F,
            0.0F,
            leftNormal.z * game_render::WALL_THICKNESS * 0.95F
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
        constexpr float ribHeight = game_render::WALL_HEIGHT * 0.82F;
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

Font loadUiFont(bool& ownsFont)
{
    constexpr const char* relativePath
        = "assets/fonts/ComicShannsMonoNerdFontMono-Regular.otf";
    const std::string applicationPath
        = std::string(GetApplicationDirectory()) + relativePath;
    const char* fontPath = FileExists(relativePath)
        ? relativePath
        : applicationPath.c_str();
    if (!FileExists(fontPath)) {
        TraceLog(LOG_WARNING,
            "UI font was not found; falling back to the raylib font");
        ownsFont = false;
        return GetFontDefault();
    }

    Font font = LoadFontEx(fontPath, 64, nullptr, 0);
    if (font.texture.id == 0) {
        TraceLog(LOG_WARNING,
            "UI font could not be loaded; falling back to the raylib font");
        ownsFont = false;
        return GetFontDefault();
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    ownsFont = true;
    return font;
}

} // namespace

RenderResources::RenderResources(const GeneratedLevel& level, Shader lightingShader)
    : groundModel(LoadModelFromMesh(GenMeshPlane(80.0F, 80.0F, 1, 1)))
    , generatedFloorModel(LoadModelFromMesh(makeGeneratedFloorMesh(level)))
    , generatedFloorDetailModel(LoadModelFromMesh(
          makeGeneratedFloorDetailMesh(level)))
    , generatedWallShadowModel(LoadModelFromMesh(
          makeWallShadowMesh(level.walls())))
    , generatedWallModel(LoadModelFromMesh(makeWallMesh(level.walls())))
    , wallModel(LoadModelFromMesh(
          GenMeshCube(1.0F, game_render::WALL_HEIGHT, game_render::WALL_THICKNESS)))
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
    , uiFont(loadUiFont(ownsUiFont))
{
    for (Model* model : { &groundModel, &generatedFloorModel,
             &generatedFloorDetailModel, &generatedWallShadowModel,
             &generatedWallModel, &wallModel, &playerModel, &targetModel,
             &enemyModel, &runnerModel, &casterModel, &eliteModel }) {
        model->materials[0].shader = lightingShader;
    }
}

RenderResources::~RenderResources()
{
    if (ownsUiFont) {
        UnloadFont(uiFont);
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
    UnloadModel(generatedWallShadowModel);
    UnloadModel(generatedFloorDetailModel);
    UnloadModel(generatedFloorModel);
    UnloadModel(groundModel);
}
