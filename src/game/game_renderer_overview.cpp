#include "game_renderer.hpp"

#include "match_generator.hpp"
#include "render_style.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

using namespace game_render;

namespace {

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
    if (level.roomLayout().hasSmallMapRecipe()) {
        return smallMapRecipeName(level.roomLayout().getSmallMapRecipe());
    }
    if (level.roomLayout().hasLargeMapArchetype()) {
        return largeMapArchetypeName(
            level.roomLayout().getLargeMapArchetype());
    }
    return "UNPUBLISHED TOPOLOGY";
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

void GameRenderer::drawGeneratedOverview(const GeneratedLevel& level,
    const LevelSession* session, const HordeMatch* match,
    std::size_t selectedConfiguration,
    std::size_t configurationCount) const
{
    const float screenWidth = UI_CANVAS_WIDTH;
    const float screenHeight = UI_CANVAS_HEIGHT;
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
    beginUiCanvas();
    drawText("GENERATED LEVEL OVERVIEW", 20, 17, 24,
        Color { 205, 229, 224, 255 });
    drawText("F2 return  |  LEFT / RIGHT browse  |  HOME active layout",
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
        const int labelWidth = measureText(roomLabel, 13);
        const char* geometryLabel = baselineGeometryLabel(room.role);
        const int geometryWidth = measureText(geometryLabel, 9);
        const int badgeWidth = std::max(labelWidth, geometryWidth) + 10;
        DrawRectangleRounded(Rectangle {
                                 center.x - static_cast<float>(badgeWidth) * 0.5F,
                                 center.y - 10.0F,
                                 static_cast<float>(badgeWidth), 31.0F },
            0.18F, 5, Color { 7, 17, 24, 205 });
        drawText(roomLabel, static_cast<int>(center.x) - labelWidth / 2,
            static_cast<int>(center.y) - 7, 13, labelColor);
        drawText(geometryLabel,
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
            const int width = measureText(label, 13);
            drawText(label, static_cast<int>(site.x) - width / 2,
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

    drawText(session != nullptr ? "ACTIVE SESSION" : "READ-ONLY PREVIEW",
        detailsX, detailsY, 18,
        session != nullptr ? Color { 255, 211, 91, 255 }
                           : Color { 151, 193, 190, 255 });
    detailsY += 34;
    drawText(TextFormat("CONFIGURATION  %02i / %02i",
                 static_cast<int>(selectedConfiguration + 1),
                 static_cast<int>(configurationCount)),
        detailsX, detailsY, 16, heading);
    detailsY += 27;
    if (level.matchGeneration().has_value()) {
        const MatchGenerationInfo& generation = *level.matchGeneration();
        const std::string seedLabel = "match seed   "
            + std::to_string(generation.matchSeed);
        drawText(seedLabel.c_str(), detailsX, detailsY, 16, primary);
        detailsY += 23;
        const std::string attemptLabel = "attempts     "
            + std::to_string(generation.attempts)
            + (generation.usedFallback ? "  FALLBACK" : "");
        drawText(attemptLabel.c_str(), detailsX, detailsY, 16,
            generation.usedFallback
                ? Color { 234, 105, 80, 255 } : secondary);
        detailsY += 23;
        const std::string profileLabel = "profile      "
            + std::string(physicalMapProfileName(
                generation.physicalProfile));
        drawText(profileLabel.c_str(), detailsX, detailsY, 16, primary);
        detailsY += 23;
    }
    drawText(TextFormat("radius       %i", level.grid().getRadius()),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    drawText(TextFormat("grid seed    %u", level.grid().getSeed()),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    drawText(TextFormat("room seed    %u", level.roomLayout().getSeed()),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    drawText(TextFormat("candidate    %i",
                 static_cast<int>(level.roomLayout().getSelectedCandidate())),
        detailsX, detailsY, 16, primary);
    detailsY += 23;
    drawText(TextFormat("quality      %.2f",
                 level.roomLayout().getQualityScore()),
        detailsX, detailsY, 16, primary);

    detailsY += 38;
    drawText("MAP RECIPE", detailsX, detailsY, 16, heading);
    detailsY += 27;
    drawText(baselineTopologyLabel(level), detailsX, detailsY, 17, primary);
    detailsY += 25;
    const std::size_t doorwayCount = level.roomLayout().getDoorways().size();
    const auto& signature = level.roomLayout().getTopologySignature();
    drawText(TextFormat("arenas %i  passages %i  cycles %i",
                 static_cast<int>(signature.substantialRoomCount),
                 static_cast<int>(signature.connectorCount),
                 static_cast<int>(signature.cycleRank)),
        detailsX, detailsY, 15, secondary);
    detailsY += 22;
    drawText(TextFormat("junctions %i  depth %i  direct %.0f%%",
                 static_cast<int>(signature.meaningfulJunctionCount),
                 static_cast<int>(signature.maximumBranchDepth),
                 signature.directArenaEdgeRatio * 100.0F),
        detailsX, detailsY, 15, secondary);
    detailsY += 22;
    drawText(TextFormat("alternation %i  route %i  locked %i/%i",
                 static_cast<int>(signature.longestAlternatingChain),
                 static_cast<int>(signature.startExitDistance),
                 static_cast<int>(lockedDoorways),
                 static_cast<int>(doorwayCount)),
        detailsX, detailsY, 15, secondary);
    detailsY += 22;
    drawText("puzzle graph: anchor -> hub -> exit", detailsX, detailsY,
        14, Color { 226, 166, 102, 255 });

    detailsY += 39;
    drawText("MAP KEY", detailsX, detailsY, 16, heading);
    detailsY += 29;
    DrawLineEx(Vector2 { static_cast<float>(detailsX),
                   static_cast<float>(detailsY + 6) },
        Vector2 { static_cast<float>(detailsX + 30),
            static_cast<float>(detailsY + 6) },
        5.0F, Color { 105, 226, 178, 255 });
    drawText("open threshold", detailsX + 42, detailsY, 15, secondary);
    detailsY += 25;
    DrawLineEx(Vector2 { static_cast<float>(detailsX),
                   static_cast<float>(detailsY + 6) },
        Vector2 { static_cast<float>(detailsX + 30),
            static_cast<float>(detailsY + 6) },
        5.0F, Color { 234, 105, 80, 255 });
    drawText("locked threshold", detailsX + 42, detailsY, 15, secondary);
    detailsY += 25;
    DrawLineEx(Vector2 { static_cast<float>(detailsX),
                   static_cast<float>(detailsY + 6) },
        Vector2 { static_cast<float>(detailsX + 30),
            static_cast<float>(detailsY + 6) },
        2.0F, Color { 198, 182, 119, 200 });
    drawText("published room graph", detailsX + 42, detailsY, 15, secondary);
    detailsY += 25;
    DrawCircleV(Vector2 { static_cast<float>(detailsX + 6),
                    static_cast<float>(detailsY + 6) },
        6.0F, Color { 255, 211, 91, 255 });
    drawText("active player", detailsX + 42, detailsY, 15, secondary);

    drawText("Role markers: triangle Start | hex Hub | diamond Reward | octagon Exit",
        static_cast<int>(mapBounds.x + 14.0F),
        static_cast<int>(mapBounds.y + mapBounds.height - 24.0F),
        13, Color { 185, 205, 200, 230 });
    endUiCanvas();
    EndDrawing();
}
