#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr float SQRT_3 = 1.7320508075688772F;
constexpr float POINT_SPACING = 58.0F;
constexpr float FULL_SIZE_POINT_GRID_RADIUS = 6.0F;
constexpr int MAX_RELAXATION_STEPS = 240;
constexpr float RELAXATION_STRENGTH = 0.12F;

struct Axial {
    int q;
    int r;

    bool operator==(const Axial&) const = default;
};

struct AxialHash {
    std::size_t operator()(const Axial& coordinate) const
    {
        const auto q = static_cast<std::uint32_t>(coordinate.q);
        const auto r = static_cast<std::uint32_t>(coordinate.r);
        return static_cast<std::size_t>((static_cast<std::uint64_t>(q) << 32U) ^ r);
    }
};

struct Edge {
    int a;
    int b;

    Edge(int first, int second)
        : a(std::min(first, second)), b(std::max(first, second))
    {
    }

    bool operator==(const Edge&) const = default;
};

struct EdgeHash {
    std::size_t operator()(const Edge& edge) const
    {
        return static_cast<std::size_t>(
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(edge.a)) << 32U)
            ^ static_cast<std::uint32_t>(edge.b));
    }
};

struct Vertex {
    Vector2 position;
    bool fixed = false;
};

using Triangle = std::array<int, 3>;
using Quad = std::array<int, 4>;

float signedArea(const std::vector<Vertex>& vertices, const std::vector<int>& face)
{
    float twiceArea = 0.0F;
    for (std::size_t i = 0; i < face.size(); ++i) {
        const Vector2 a = vertices[face[i]].position;
        const Vector2 b = vertices[face[(i + 1) % face.size()]].position;
        twiceArea += a.x * b.y - b.x * a.y;
    }
    return twiceArea * 0.5F;
}

class StalbergGrid {
public:
    void generate(int newRadius, std::uint32_t newSeed)
    {
        radius = newRadius;
        seed = newSeed;
        relaxationSteps = 0;

        generateHexagonalLattice();
        const auto triangles = triangulateLattice();
        const auto faces = randomlyPairTriangles(triangles);
        subdivideFaces(faces);
        rebuildTopology();
    }

    void relaxOnce()
    {
        if (relaxationSteps >= MAX_RELAXATION_STEPS) {
            return;
        }

        std::vector<Vector2> nextPositions;
        nextPositions.reserve(vertices.size());
        for (const Vertex& vertex : vertices) {
            nextPositions.push_back(vertex.position);
        }

        for (std::size_t i = 0; i < vertices.size(); ++i) {
            if (vertices[i].fixed || neighbors[i].empty()) {
                continue;
            }

            Vector2 average { 0.0F, 0.0F };
            for (const int neighbor : neighbors[i]) {
                average.x += vertices[neighbor].position.x;
                average.y += vertices[neighbor].position.y;
            }

            const float inverseCount = 1.0F / static_cast<float>(neighbors[i].size());
            average.x *= inverseCount;
            average.y *= inverseCount;

            nextPositions[i].x += (average.x - vertices[i].position.x) * RELAXATION_STRENGTH;
            nextPositions[i].y += (average.y - vertices[i].position.y) * RELAXATION_STRENGTH;
        }

        for (std::size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].position = nextPositions[i];
        }
        ++relaxationSteps;
    }

    void draw(bool drawPoints, float cameraZoom) const
    {
        const float zoom = std::max(cameraZoom, 0.01F);
        const float lineWidth = 1.6F / zoom;

        for (const Edge& edge : edges) {
            DrawLineEx(
                vertices[edge.a].position,
                vertices[edge.b].position,
                lineWidth,
                Color { 83, 114, 128, 255 });
        }

        if (!drawPoints) {
            return;
        }

        const float pointScale = std::min(
            1.0F, FULL_SIZE_POINT_GRID_RADIUS / static_cast<float>(radius));
        for (const Vertex& vertex : vertices) {
            const Color color = vertex.fixed
                ? Color { 239, 180, 74, 255 }
                : Color { 222, 235, 232, 255 };
            const float pointRadius = (vertex.fixed ? 3.1F : 2.5F) * pointScale;
            DrawCircleV(vertex.position, pointRadius / zoom, color);
        }
    }

    int getRadius() const { return radius; }
    std::uint32_t getSeed() const { return seed; }
    int getRelaxationSteps() const { return relaxationSteps; }
    std::size_t getVertexCount() const { return vertices.size(); }
    std::size_t getQuadCount() const { return quads.size(); }

private:
    int radius = 6;
    std::uint32_t seed = 1;
    int relaxationSteps = 0;
    std::vector<Vertex> vertices;
    std::vector<Quad> quads;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> neighbors;
    std::unordered_map<Axial, int, AxialHash> latticeIndices;

    void generateHexagonalLattice()
    {
        vertices.clear();
        latticeIndices.clear();

        for (int q = -radius; q <= radius; ++q) {
            const int minimumR = std::max(-radius, -q - radius);
            const int maximumR = std::min(radius, -q + radius);
            for (int r = minimumR; r <= maximumR; ++r) {
                const Vector2 position {
                    POINT_SPACING * (static_cast<float>(q) + static_cast<float>(r) * 0.5F),
                    POINT_SPACING * (SQRT_3 * 0.5F * static_cast<float>(r))
                };
                const int index = static_cast<int>(vertices.size());
                vertices.push_back(Vertex { position, false });
                latticeIndices.emplace(Axial { q, r }, index);
            }
        }
    }

    int findLatticeVertex(Axial coordinate) const
    {
        const auto found = latticeIndices.find(coordinate);
        return found == latticeIndices.end() ? -1 : found->second;
    }

    std::vector<Triangle> triangulateLattice() const
    {
        std::vector<Triangle> triangles;

        for (const auto& [coordinate, vertex] : latticeIndices) {
            const int east = findLatticeVertex(Axial { coordinate.q + 1, coordinate.r });
            const int northEast = findLatticeVertex(Axial { coordinate.q, coordinate.r + 1 });
            const int southEast = findLatticeVertex(Axial { coordinate.q + 1, coordinate.r - 1 });

            if (east >= 0 && northEast >= 0) {
                triangles.push_back(Triangle { vertex, east, northEast });
            }
            if (east >= 0 && southEast >= 0) {
                triangles.push_back(Triangle { vertex, southEast, east });
            }
        }

        return triangles;
    }

    std::vector<int> orderedFace(std::vector<int> face) const
    {
        Vector2 center { 0.0F, 0.0F };
        for (const int index : face) {
            center.x += vertices[index].position.x;
            center.y += vertices[index].position.y;
        }
        center.x /= static_cast<float>(face.size());
        center.y /= static_cast<float>(face.size());

        std::ranges::sort(face, [&](int lhs, int rhs) {
            const Vector2 left = vertices[lhs].position;
            const Vector2 right = vertices[rhs].position;
            return std::atan2(left.y - center.y, left.x - center.x)
                < std::atan2(right.y - center.y, right.x - center.x);
        });

        if (signedArea(vertices, face) < 0.0F) {
            std::ranges::reverse(face);
        }
        return face;
    }

    std::vector<std::vector<int>> randomlyPairTriangles(const std::vector<Triangle>& triangles)
    {
        std::unordered_map<Edge, std::vector<int>, EdgeHash> incidentTriangles;
        for (std::size_t triangleIndex = 0; triangleIndex < triangles.size(); ++triangleIndex) {
            const Triangle& triangle = triangles[triangleIndex];
            for (int edge = 0; edge < 3; ++edge) {
                incidentTriangles[Edge(triangle[edge], triangle[(edge + 1) % 3])]
                    .push_back(static_cast<int>(triangleIndex));
            }
        }

        std::vector<Edge> candidates;
        for (const auto& [edge, incident] : incidentTriangles) {
            if (incident.size() == 2) {
                candidates.push_back(edge);
            }
        }

        std::mt19937 random(seed);
        std::ranges::shuffle(candidates, random);

        std::vector<bool> paired(triangles.size(), false);
        std::vector<std::vector<int>> faces;
        for (const Edge& edge : candidates) {
            const auto& incident = incidentTriangles.at(edge);
            const int first = incident[0];
            const int second = incident[1];
            if (paired[first] || paired[second]) {
                continue;
            }

            std::vector<int> merged;
            merged.reserve(4);
            for (const int vertex : triangles[first]) {
                merged.push_back(vertex);
            }
            for (const int vertex : triangles[second]) {
                if (std::ranges::find(merged, vertex) == merged.end()) {
                    merged.push_back(vertex);
                }
            }

            if (merged.size() == 4) {
                faces.push_back(orderedFace(std::move(merged)));
                paired[first] = true;
                paired[second] = true;
            }
        }

        for (std::size_t i = 0; i < triangles.size(); ++i) {
            if (!paired[i]) {
                faces.push_back(orderedFace(
                    std::vector<int>(triangles[i].begin(), triangles[i].end())));
            }
        }
        return faces;
    }

    void subdivideFaces(const std::vector<std::vector<int>>& faces)
    {
        std::unordered_map<Edge, int, EdgeHash> midpointIndices;
        std::vector<Quad> subdivided;

        auto midpointFor = [&](int first, int second) {
            const Edge edge(first, second);
            if (const auto found = midpointIndices.find(edge); found != midpointIndices.end()) {
                return found->second;
            }

            const Vector2 a = vertices[first].position;
            const Vector2 b = vertices[second].position;
            const int index = static_cast<int>(vertices.size());
            vertices.push_back(Vertex { Vector2 { (a.x + b.x) * 0.5F, (a.y + b.y) * 0.5F }, false });
            midpointIndices.emplace(edge, index);
            return index;
        };

        for (const std::vector<int>& face : faces) {
            Vector2 center { 0.0F, 0.0F };
            for (const int vertex : face) {
                center.x += vertices[vertex].position.x;
                center.y += vertices[vertex].position.y;
            }
            center.x /= static_cast<float>(face.size());
            center.y /= static_cast<float>(face.size());

            const int centerIndex = static_cast<int>(vertices.size());
            vertices.push_back(Vertex { center, false });

            std::vector<int> midpoints(face.size());
            for (std::size_t i = 0; i < face.size(); ++i) {
                midpoints[i] = midpointFor(face[i], face[(i + 1) % face.size()]);
            }

            for (std::size_t i = 0; i < face.size(); ++i) {
                const std::size_t previous = (i + face.size() - 1) % face.size();
                subdivided.push_back(Quad {
                    face[i], midpoints[i], centerIndex, midpoints[previous]
                });
            }
        }

        quads = std::move(subdivided);
    }

    void rebuildTopology()
    {
        std::unordered_map<Edge, int, EdgeHash> edgeUseCounts;
        std::vector<std::unordered_set<int>> neighborSets(vertices.size());

        for (const Quad& quad : quads) {
            for (int i = 0; i < 4; ++i) {
                const int first = quad[i];
                const int second = quad[(i + 1) % 4];
                ++edgeUseCounts[Edge(first, second)];
                neighborSets[first].insert(second);
                neighborSets[second].insert(first);
            }
        }

        edges.clear();
        edges.reserve(edgeUseCounts.size());
        for (const auto& [edge, count] : edgeUseCounts) {
            edges.push_back(edge);
            if (count == 1) {
                vertices[edge.a].fixed = true;
                vertices[edge.b].fixed = true;
            }
        }

        neighbors.clear();
        neighbors.reserve(neighborSets.size());
        for (const auto& set : neighborSets) {
            neighbors.emplace_back(set.begin(), set.end());
        }
    }
};

void fitCamera(Camera2D& camera, int radius)
{
    camera.target = Vector2 { 0.0F, 0.0F };
    const float gridWidth = 2.0F * POINT_SPACING * static_cast<float>(radius);
    const float gridHeight = SQRT_3 * POINT_SPACING * static_cast<float>(radius);
    const float horizontalZoom = (static_cast<float>(GetScreenWidth()) - 90.0F) / gridWidth;
    const float verticalZoom = (static_cast<float>(GetScreenHeight()) - 150.0F) / gridHeight;
    camera.zoom = std::clamp(std::min(horizontalZoom, verticalZoom), 0.15F, 2.5F);
}

void handleCamera(Camera2D& camera, int radius)
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
        fitCamera(camera, radius);
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

    StalbergGrid grid;
    int radius = 6;
    std::uint32_t seed = 1;
    bool relaxing = true;
    bool drawPoints = true;
    grid.generate(radius, seed);

    Camera2D camera {};
    camera.offset = Vector2 { 640.0F, 400.0F };
    camera.rotation = 0.0F;
    fitCamera(camera, radius);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_R)) {
            ++seed;
            grid.generate(radius, seed);
            relaxing = true;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            seed = seed > 1 ? seed - 1 : 1;
            grid.generate(radius, seed);
            relaxing = true;
        }
        if (IsKeyPressed(KEY_RIGHT)) {
            ++seed;
            grid.generate(radius, seed);
            relaxing = true;
        }
        if (IsKeyPressed(KEY_UP) && radius < 14) {
            ++radius;
            grid.generate(radius, seed);
            fitCamera(camera, radius);
            relaxing = true;
        }
        if (IsKeyPressed(KEY_DOWN) && radius > 2) {
            --radius;
            grid.generate(radius, seed);
            fitCamera(camera, radius);
            relaxing = true;
        }
        if (IsKeyPressed(KEY_SPACE)) {
            relaxing = !relaxing;
        }
        if (IsKeyPressed(KEY_N)) {
            relaxing = false;
            grid.relaxOnce();
        }
        if (IsKeyPressed(KEY_P)) {
            drawPoints = !drawPoints;
        }

        handleCamera(camera, radius);
        if (relaxing) {
            grid.relaxOnce();
            if (grid.getRelaxationSteps() >= MAX_RELAXATION_STEPS) {
                relaxing = false;
            }
        }

        BeginDrawing();
        ClearBackground(Color { 17, 25, 29, 255 });

        BeginMode2D(camera);
        grid.draw(drawPoints, camera.zoom);
        EndMode2D();

        DrawRectangle(14, 14, 405, 117, Color { 8, 13, 16, 220 });
        DrawText("OSKAR STALBERG-STYLE QUAD GRID", 26, 24, 20, Color { 222, 235, 232, 255 });
        DrawText(TextFormat("radius %d   seed %u   vertices %zu   quads %zu",
                     grid.getRadius(), grid.getSeed(), grid.getVertexCount(), grid.getQuadCount()),
            26, 52, 16, Color { 150, 178, 181, 255 });
        DrawText(TextFormat("relaxation %d/%d%s", grid.getRelaxationSteps(), MAX_RELAXATION_STEPS,
                     relaxing ? "  (running)" : "  (paused)"),
            26, 74, 16, Color { 239, 180, 74, 255 });
        DrawText("R/new seed  arrows/seed+size  Space/pause  N/step", 26, 99, 14,
            Color { 150, 178, 181, 255 });
        DrawText("P/points  F/fit  wheel/zoom  middle or right drag/pan", 20,
            GetScreenHeight() - 27, 14, Color { 121, 146, 150, 255 });

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
