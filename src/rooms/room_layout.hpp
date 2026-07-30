#pragma once

#include "rooms/room_grid.hpp"
#include "rooms/room_mission_brief.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace stalberg::rooms {

inline constexpr int EMPTY_CELL = -1;
inline constexpr std::size_t MAX_GENERATED_ROOMS = 64;

enum class RoomGenerationMethod : std::uint8_t {
    ShooterLayout,
    BranchingShapes,
    OrganicGrowth
};

enum class RoomRole : std::uint8_t {
    Start,
    Combat,
    Connector,
    Hub,
    Reward,
    Exit
};

enum class SmallMapRecipe : std::uint8_t {
    HubCircuit,
    BrokenRing,
    TwinWings
};

enum class LargeMapArchetype : std::uint8_t {
    HubAndSpokes,
    RingAndBranches,
    MainSpine,
    TwinDistricts,
    DenseCoreWithSparseBranch
};

struct TopologySignature {
    static constexpr std::size_t DEGREE_BUCKETS = 7;

    std::size_t substantialRoomCount = 0;
    std::size_t connectorCount = 0;
    std::size_t contractedEdgeCount = 0;
    std::size_t directArenaEdgeCount = 0;
    std::size_t longestAlternatingChain = 0;
    std::size_t multiDoorSubstantialRoomCount = 0;
    std::size_t meaningfulJunctionCount = 0;
    std::size_t maximumDegree = 0;
    std::size_t cycleRank = 0;
    std::size_t usefulCycleCount = 0;
    std::size_t ordinaryCombatLeafCount = 0;
    std::size_t minimumShortcutSavingsTransitions = 0;
    std::size_t maximumBranchDepth = 0;
    std::size_t startExitDistance = 0;
    std::array<std::size_t, DEGREE_BUCKETS> degreeHistogram {};
    float directArenaEdgeRatio = 0.0F;

    bool operator==(const TopologySignature&) const = default;
};

struct GeneratedRoom {
    int id = 0;
    std::size_t cellCount = 0;
    float area = 0.0F;
    RoomRole role = RoomRole::Combat;
    std::vector<CellIndex> coverCandidates;
    std::vector<CellIndex> enemySpawnCandidates;

    GeneratedRoom() = default;
    GeneratedRoom(int roomId, std::size_t roomCellCount)
        : id(roomId), cellCount(roomCellCount)
    {
    }
};

struct Doorway {
    int firstRegion;
    CellIndex firstCell;
    int secondRegion;
    CellIndex secondCell;
    float width = 0.0F;
    float quality = 0.0F;
};

class RoomGenerator;

class RoomLayout {
public:
    std::uint32_t getSeed() const { return seed; }
    RoomGenerationMethod getMethod() const { return method; }
    bool hasSmallMapRecipe() const { return smallMapRecipeSelected; }
    SmallMapRecipe getSmallMapRecipe() const { return smallMapRecipe; }
    bool hasLargeMapArchetype() const { return largeMapArchetypeSelected; }
    LargeMapArchetype getLargeMapArchetype() const { return largeMapArchetype; }
    const TopologySignature& getTopologySignature() const
    {
        return topologySignature;
    }
    std::span<const MissionEdgeBrief> getMissionEdges() const
    {
        return missionEdges;
    }
    std::size_t getRoomCount() const { return rooms.size(); }
    float getQualityScore() const { return qualityScore; }
    std::size_t getSelectedCandidate() const { return selectedCandidate; }
    int getCellAssignment(CellIndex cell) const;

    std::span<const int> getCellAssignments() const { return cellAssignments; }
    std::span<const GeneratedRoom> getRooms() const { return rooms; }
    std::span<const Doorway> getDoorways() const { return doorways; }
    std::span<const CellIndex> getConnectedEntrances() const
    {
        return connectedEntrances;
    }

private:
    friend class RoomGenerator;

    std::uint32_t seed = 1;
    RoomGenerationMethod method {};
    SmallMapRecipe smallMapRecipe = SmallMapRecipe::HubCircuit;
    bool smallMapRecipeSelected = false;
    LargeMapArchetype largeMapArchetype = LargeMapArchetype::HubAndSpokes;
    bool largeMapArchetypeSelected = false;
    TopologySignature topologySignature;
    std::vector<MissionEdgeBrief> missionEdges;
    std::vector<int> cellAssignments;
    std::vector<GeneratedRoom> rooms;
    std::vector<Doorway> doorways;
    std::vector<CellIndex> connectedEntrances;
    float qualityScore = 0.0F;
    std::size_t selectedCandidate = 0;
};

} // namespace stalberg::rooms
