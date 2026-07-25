#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace stalberg {

inline constexpr int MAX_RELAXATION_STEPS = 240;

using VertexIndex = std::size_t;

struct Point {
    float x;
    float y;

    bool operator==(const Point&) const = default;
};

struct Vertex {
    Point position;
    bool fixed = false;
};

struct Bounds {
    Point minimum;
    Point maximum;
};

struct Edge {
    VertexIndex a;
    VertexIndex b;

    Edge(VertexIndex first, VertexIndex second);

    bool operator==(const Edge&) const = default;
};

using Quad = std::array<VertexIndex, 4>;

class StalbergGrid {
public:
    void generate(int newRadius, std::uint32_t newSeed);
    void relaxOnce();

    int getRadius() const { return radius; }
    std::uint32_t getSeed() const { return seed; }
    int getRelaxationSteps() const { return relaxationSteps; }
    std::size_t getVertexCount() const { return vertices.size(); }
    std::size_t getQuadCount() const { return quads.size(); }
    Bounds getBounds() const;

    std::span<const Vertex> getVertices() const { return vertices; }
    std::span<const Quad> getQuads() const { return quads; }
    std::span<const Edge> getEdges() const { return edges; }
    std::span<const std::vector<VertexIndex>> getNeighbors() const { return neighbors; }

private:
    struct Axial {
        int q;
        int r;

        bool operator==(const Axial&) const = default;
    };

    struct AxialHash {
        std::size_t operator()(const Axial& coordinate) const;
    };

    struct EdgeHash {
        std::size_t operator()(const Edge& edge) const;
    };

    using Triangle = std::array<VertexIndex, 3>;
    using Face = std::vector<VertexIndex>;
    using Faces = std::vector<Face>;

    int radius = 6;
    std::uint32_t seed = 1;
    int relaxationSteps = 0;
    std::vector<Vertex> vertices;
    std::vector<Quad> quads;
    std::vector<Edge> edges;
    std::vector<std::vector<VertexIndex>> neighbors;
    std::unordered_map<Axial, VertexIndex, AxialHash> latticeIndices;

    void generateHexagonalLattice();
    std::optional<VertexIndex> findLatticeVertex(Axial coordinate) const;
    std::vector<Triangle> triangulateLattice() const;
    Point faceCenter(std::span<const VertexIndex> face) const;
    Face orderedFace(Face face) const;
    Faces randomlyPairTriangles(const std::vector<Triangle>& triangles);
    void subdivideFaces(const Faces& faces);
    void rebuildTopology();
};

} // namespace stalberg
