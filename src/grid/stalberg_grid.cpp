#include "grid/stalberg_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace stalberg {
namespace {

constexpr float SQRT_3 = 1.7320508075688772F;
constexpr float POINT_SPACING = 58.0F;
constexpr float RELAXATION_STRENGTH = 0.12F;

float signedArea(
    std::span<const Vertex> vertices, std::span<const VertexIndex> face)
{
    float twiceArea = 0.0F;
    for (std::size_t i = 0; i < face.size(); ++i) {
        const Point a = vertices[face[i]].position;
        const Point b = vertices[face[(i + 1) % face.size()]].position;
        twiceArea += a.x * b.y - b.x * a.y;
    }
    return twiceArea * 0.5F;
}

} // namespace

Edge::Edge(VertexIndex first, VertexIndex second)
    : a(std::min(first, second)), b(std::max(first, second))
{
}

std::size_t StalbergGrid::AxialHash::operator()(const Axial& coordinate) const
{
    const auto q = static_cast<std::uint32_t>(coordinate.q);
    const auto r = static_cast<std::uint32_t>(coordinate.r);
    return static_cast<std::size_t>((static_cast<std::uint64_t>(q) << 32U) ^ r);
}

std::size_t StalbergGrid::EdgeHash::operator()(const Edge& edge) const
{
    const auto a = static_cast<std::uint32_t>(edge.a);
    const auto b = static_cast<std::uint32_t>(edge.b);
    return static_cast<std::size_t>((static_cast<std::uint64_t>(a) << 32U) ^ b);
}

void StalbergGrid::generate(int newRadius, std::uint32_t newSeed)
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

Bounds StalbergGrid::getBounds() const
{
    if (vertices.empty()) {
        return Bounds { Point { 0.0F, 0.0F }, Point { 0.0F, 0.0F } };
    }

    Bounds bounds { vertices.front().position, vertices.front().position };
    for (const Vertex& vertex : vertices) {
        bounds.minimum.x = std::min(bounds.minimum.x, vertex.position.x);
        bounds.minimum.y = std::min(bounds.minimum.y, vertex.position.y);
        bounds.maximum.x = std::max(bounds.maximum.x, vertex.position.x);
        bounds.maximum.y = std::max(bounds.maximum.y, vertex.position.y);
    }
    return bounds;
}

void StalbergGrid::relaxOnce()
{
    if (relaxationSteps >= MAX_RELAXATION_STEPS) {
        return;
    }

    std::vector<Point> nextPositions;
    nextPositions.reserve(vertices.size());
    for (const Vertex& vertex : vertices) {
        nextPositions.push_back(vertex.position);
    }

    for (std::size_t i = 0; i < vertices.size(); ++i) {
        if (vertices[i].fixed || neighbors[i].empty()) {
            continue;
        }

        Point average { 0.0F, 0.0F };
        for (const VertexIndex neighbor : neighbors[i]) {
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

void StalbergGrid::relaxToCompletion()
{
    while (!isFullyRelaxed()) {
        relaxOnce();
    }
}

void StalbergGrid::generateHexagonalLattice()
{
    vertices.clear();
    latticeIndices.clear();

    for (int q = -radius; q <= radius; ++q) {
        const int minimumR = std::max(-radius, -q - radius);
        const int maximumR = std::min(radius, -q + radius);
        for (int r = minimumR; r <= maximumR; ++r) {
            const Point position {
                POINT_SPACING * (static_cast<float>(q) + static_cast<float>(r) * 0.5F),
                POINT_SPACING * (SQRT_3 * 0.5F * static_cast<float>(r))
            };
            const VertexIndex index = vertices.size();
            vertices.push_back(Vertex { position, false });
            latticeIndices.emplace(Axial { q, r }, index);
        }
    }
}

std::optional<VertexIndex> StalbergGrid::findLatticeVertex(Axial coordinate) const
{
    const auto found = latticeIndices.find(coordinate);
    if (found == latticeIndices.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<StalbergGrid::Triangle> StalbergGrid::triangulateLattice() const
{
    std::vector<Triangle> triangles;

    for (const auto& [coordinate, vertex] : latticeIndices) {
        const auto east = findLatticeVertex(Axial { coordinate.q + 1, coordinate.r });
        const auto northEast = findLatticeVertex(Axial { coordinate.q, coordinate.r + 1 });
        const auto southEast = findLatticeVertex(Axial { coordinate.q + 1, coordinate.r - 1 });

        if (east && northEast) {
            triangles.push_back(Triangle { vertex, *east, *northEast });
        }
        if (east && southEast) {
            triangles.push_back(Triangle { vertex, *southEast, *east });
        }
    }

    for (Triangle& triangle : triangles) {
        std::ranges::sort(triangle);
    }
    std::ranges::sort(triangles);
    return triangles;
}

Point StalbergGrid::faceCenter(std::span<const VertexIndex> face) const
{
    Point center { 0.0F, 0.0F };
    for (const VertexIndex index : face) {
        center.x += vertices[index].position.x;
        center.y += vertices[index].position.y;
    }
    const float inverseSize = 1.0F / static_cast<float>(face.size());
    center.x *= inverseSize;
    center.y *= inverseSize;
    return center;
}

StalbergGrid::Face StalbergGrid::orderedFace(Face face) const
{
    const Point center = faceCenter(face);

    std::ranges::sort(face, [&](VertexIndex lhs, VertexIndex rhs) {
        const Point left = vertices[lhs].position;
        const Point right = vertices[rhs].position;
        return std::atan2(left.y - center.y, left.x - center.x)
            < std::atan2(right.y - center.y, right.x - center.x);
    });

    if (signedArea(vertices, face) < 0.0F) {
        std::ranges::reverse(face);
    }
    return face;
}

StalbergGrid::Faces StalbergGrid::randomlyPairTriangles(
    const std::vector<Triangle>& triangles)
{
    std::unordered_map<Edge, std::vector<std::size_t>, EdgeHash> incidentTriangles;
    for (std::size_t triangleIndex = 0; triangleIndex < triangles.size(); ++triangleIndex) {
        const Triangle& triangle = triangles[triangleIndex];
        for (std::size_t edge = 0; edge < triangle.size(); ++edge) {
            incidentTriangles[Edge(triangle[edge], triangle[(edge + 1) % triangle.size()])]
                .push_back(triangleIndex);
        }
    }

    std::vector<Edge> candidates;
    for (const auto& [edge, incident] : incidentTriangles) {
        if (incident.size() == 2) {
            candidates.push_back(edge);
        }
    }

    // Hash-container iteration is deliberately normalized before consuming the
    // seeded random stream so a seed identifies one topology across platforms.
    std::ranges::sort(candidates, [](const Edge& first, const Edge& second) {
        return std::tie(first.a, first.b) < std::tie(second.a, second.b);
    });
    std::mt19937 random(seed);
    std::ranges::shuffle(candidates, random);

    std::vector<bool> paired(triangles.size(), false);
    Faces faces;
    for (const Edge& edge : candidates) {
        const auto& incident = incidentTriangles.at(edge);
        const std::size_t first = incident[0];
        const std::size_t second = incident[1];
        if (paired[first] || paired[second]) {
            continue;
        }

        Face merged;
        merged.reserve(4);
        for (const VertexIndex vertex : triangles[first]) {
            merged.push_back(vertex);
        }
        for (const VertexIndex vertex : triangles[second]) {
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
            faces.push_back(orderedFace(Face(triangles[i].begin(), triangles[i].end())));
        }
    }
    return faces;
}

void StalbergGrid::subdivideFaces(const Faces& faces)
{
    std::unordered_map<Edge, VertexIndex, EdgeHash> midpointIndices;
    std::vector<Quad> subdivided;

    auto midpointFor = [&](VertexIndex first, VertexIndex second) {
        const Edge edge(first, second);
        if (const auto found = midpointIndices.find(edge); found != midpointIndices.end()) {
            return found->second;
        }

        const Point a = vertices[first].position;
        const Point b = vertices[second].position;
        const VertexIndex index = vertices.size();
        vertices.push_back(Vertex { Point { (a.x + b.x) * 0.5F, (a.y + b.y) * 0.5F }, false });
        midpointIndices.emplace(edge, index);
        return index;
    };

    for (const Face& face : faces) {
        const Point center = faceCenter(face);
        const VertexIndex centerIndex = vertices.size();
        vertices.push_back(Vertex { center, false });

        std::vector<VertexIndex> midpoints(face.size());
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

void StalbergGrid::rebuildTopology()
{
    std::unordered_map<Edge, std::size_t, EdgeHash> edgeUseCounts;
    std::vector<std::unordered_set<VertexIndex>> neighborSets(vertices.size());

    for (Vertex& vertex : vertices) {
        vertex.fixed = false;
    }

    for (const Quad& quad : quads) {
        for (std::size_t i = 0; i < quad.size(); ++i) {
            const VertexIndex first = quad[i];
            const VertexIndex second = quad[(i + 1) % quad.size()];
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
    std::ranges::sort(edges, [](const Edge& first, const Edge& second) {
        return std::tie(first.a, first.b) < std::tie(second.a, second.b);
    });

    neighbors.clear();
    neighbors.reserve(neighborSets.size());
    for (const auto& set : neighborSets) {
        neighbors.emplace_back(set.begin(), set.end());
        std::ranges::sort(neighbors.back());
    }
}

} // namespace stalberg
