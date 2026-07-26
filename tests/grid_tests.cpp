#include "grid/stalberg_grid.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct EdgeHash {
    std::size_t operator()(const stalberg::Edge& edge) const
    {
        return std::hash<std::size_t> {}(edge.a)
            ^ (std::hash<std::size_t> {}(edge.b) << 1U);
    }
};

bool check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool topologyIsValid(const stalberg::StalbergGrid& grid)
{
    bool valid = true;
    const auto vertices = grid.getVertices();
    const auto neighbors = grid.getNeighbors();
    std::unordered_map<stalberg::Edge, std::size_t, EdgeHash> edgeUseCounts;

    valid &= check(neighbors.size() == vertices.size(), "each vertex has a neighbor list");
    for (const stalberg::Quad& quad : grid.getQuads()) {
        const bool indicesAreValid = std::ranges::all_of(quad,
            [vertexCount = vertices.size()](stalberg::VertexIndex index) {
                return index < vertexCount;
            });
        valid &= check(indicesAreValid, "quad indices are in range");
        if (!indicesAreValid) {
            continue;
        }
        for (std::size_t index = 0; index < quad.size(); ++index) {
            ++edgeUseCounts[stalberg::Edge(
                quad[index], quad[(index + 1) % quad.size()])];
        }
    }

    for (const auto& [edge, count] : edgeUseCounts) {
        valid &= check(count == 1 || count == 2, "edge belongs to one or two quads");
        if (count == 1) {
            valid &= check(vertices[edge.a].fixed && vertices[edge.b].fixed,
                "boundary edge endpoints are fixed");
        }
    }

    for (std::size_t vertex = 0; vertex < neighbors.size(); ++vertex) {
        for (const stalberg::VertexIndex neighbor : neighbors[vertex]) {
            const bool neighborIsValid = neighbor < neighbors.size();
            valid &= check(neighborIsValid, "neighbor index is in range");
            if (neighborIsValid) {
                valid &= check(std::ranges::find(neighbors[neighbor], vertex)
                        != neighbors[neighbor].end(),
                    "neighbor relationship is symmetric");
            }
        }
    }
    return valid;
}

bool generationIsRepeatable()
{
    stalberg::StalbergGrid first;
    stalberg::StalbergGrid second;
    first.generate(6, 1);
    second.generate(6, 1);

    return check(first.getQuads().size() == second.getQuads().size(),
               "same settings produce the same quad count")
        && check(std::ranges::equal(first.getQuads(), second.getQuads()),
            "same settings produce the same topology");
}

bool relaxationPreservesBoundary()
{
    stalberg::StalbergGrid grid;
    grid.generate(6, 1);

    std::vector<stalberg::Point> boundaryPositions(grid.getVertexCount());
    for (std::size_t index = 0; index < grid.getVertices().size(); ++index) {
        if (grid.getVertices()[index].fixed) {
            boundaryPositions[index] = grid.getVertices()[index].position;
        }
    }

    for (int step = 0; step <= stalberg::MAX_RELAXATION_STEPS; ++step) {
        grid.relaxOnce();
    }

    bool valid = check(grid.getRelaxationSteps() == stalberg::MAX_RELAXATION_STEPS,
        "relaxation stops at the configured step limit");
    for (std::size_t index = 0; index < grid.getVertices().size(); ++index) {
        if (grid.getVertices()[index].fixed) {
            valid &= check(grid.getVertices()[index].position == boundaryPositions[index],
                "fixed boundary vertex does not move");
        }
    }
    return valid;
}

} // namespace

int main()
{
    stalberg::StalbergGrid grid;
    grid.generate(6, 1);

    bool valid = true;
    valid &= check(grid.getVertexCount() > 0, "generation creates vertices");
    valid &= check(grid.getQuadCount() == 460, "radius 6, seed 1 produces 460 quads");
    valid &= topologyIsValid(grid);
    valid &= generationIsRepeatable();
    valid &= relaxationPreservesBoundary();

    if (!valid) {
        return 1;
    }
    std::cout << "All grid generation tests passed\n";
    return 0;
}
