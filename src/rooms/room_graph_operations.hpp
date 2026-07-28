#pragma once

#include "rooms/room_grid.hpp"

#include <algorithm>
#include <cstddef>
#include <queue>
#include <ranges>
#include <utility>
#include <vector>

namespace stalberg::rooms::detail {

using CellAdjacency = std::vector<std::vector<CellIndex>>;

inline CellAdjacency canonicalAdjacency(const RoomGrid& grid)
{
    CellAdjacency adjacency;
    adjacency.reserve(grid.getConnections().size());
    for (const auto& connections : grid.getConnections()) {
        std::vector<CellIndex> neighbors;
        neighbors.reserve(connections.size());
        for (const CellConnection& connection : connections) {
            neighbors.push_back(connection.cell);
        }
        std::ranges::sort(neighbors);
        adjacency.push_back(std::move(neighbors));
    }
    return adjacency;
}

inline const CellConnection* findConnection(
    const RoomGrid& grid, CellIndex first, CellIndex second)
{
    const auto connection = std::ranges::find(
        grid.connections[first], second, &CellConnection::cell);
    return connection == grid.connections[first].end()
        ? nullptr
        : &*connection;
}

inline float sharedBoundaryLength(
    const RoomGrid& grid, CellIndex first, CellIndex second)
{
    const CellConnection* connection = findConnection(grid, first, second);
    return connection == nullptr ? 0.0F : connection->sharedBoundaryLength;
}

template <typename Node>
std::vector<int> breadthFirstDistances(
    const std::vector<std::vector<Node>>& graph, Node start)
{
    std::vector<int> distances(graph.size(), -1);
    const std::size_t startIndex = static_cast<std::size_t>(start);
    if (startIndex >= graph.size()) {
        return distances;
    }

    std::queue<Node> queue;
    distances[startIndex] = 0;
    queue.push(start);
    while (!queue.empty()) {
        const Node node = queue.front();
        queue.pop();
        const std::size_t nodeIndex = static_cast<std::size_t>(node);
        for (const Node neighbor : graph[nodeIndex]) {
            const std::size_t neighborIndex = static_cast<std::size_t>(neighbor);
            if (neighborIndex >= graph.size() || distances[neighborIndex] >= 0) {
                continue;
            }
            distances[neighborIndex] = distances[nodeIndex] + 1;
            queue.push(neighbor);
        }
    }
    return distances;
}

template <typename Predicate>
std::size_t reachableCellCount(const CellAdjacency& adjacency,
    CellIndex start,
    Predicate&& included)
{
    if (start >= adjacency.size() || !included(start)) {
        return 0;
    }

    std::vector<bool> reached(adjacency.size(), false);
    std::queue<CellIndex> queue;
    reached[start] = true;
    queue.push(start);
    std::size_t count = 0;
    while (!queue.empty()) {
        const CellIndex cell = queue.front();
        queue.pop();
        ++count;
        for (const CellIndex neighbor : adjacency[cell]) {
            if (!reached[neighbor] && included(neighbor)) {
                reached[neighbor] = true;
                queue.push(neighbor);
            }
        }
    }
    return count;
}

class DisjointSet {
public:
    explicit DisjointSet(std::size_t size)
        : parent(size)
    {
        for (std::size_t item = 0; item < size; ++item) {
            parent[item] = item;
        }
    }

    std::size_t find(std::size_t item)
    {
        while (parent[item] != item) {
            parent[item] = parent[parent[item]];
            item = parent[item];
        }
        return item;
    }

    bool unite(std::size_t first, std::size_t second)
    {
        const std::size_t firstRoot = find(first);
        const std::size_t secondRoot = find(second);
        if (firstRoot == secondRoot) {
            return false;
        }
        parent[secondRoot] = firstRoot;
        return true;
    }

private:
    std::vector<std::size_t> parent;
};

} // namespace stalberg::rooms::detail
