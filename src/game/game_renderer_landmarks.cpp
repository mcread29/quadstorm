#include "game_renderer.hpp"

#include "render_primitives.hpp"
#include "render_style.hpp"

#include <algorithm>
#include <cmath>

using namespace game_render;

void GameRenderer::drawHordeLandmarks(const GeneratedLevel& level,
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

void GameRenderer::updateGeneratedLights(const HordeMatch& match) const
{
    const Vector2 hub = match.plan().hubPosition;
    const Vector2 anchor = match.plan().anchorPosition;
    const Vector3 hubPosition { hub.x, 1.45F, hub.y };
    const Vector3 anchorPosition { anchor.x, 1.2F, anchor.y };
    const Vector3 hubColor {
        match.hubIsPowered() ? 0.08F : 0.015F,
        match.hubIsPowered() ? 1.15F : 0.08F,
        match.hubIsPowered() ? 1.35F : 0.10F
    };
    const Vector3 anchorColor {
        match.anchorIsActive() ? 1.35F
            : match.anchorIsComplete() ? 0.25F : 0.12F,
        match.anchorIsActive() ? 0.72F
            : match.anchorIsComplete() ? 0.82F : 0.055F,
        match.anchorIsActive() ? 0.12F
            : match.anchorIsComplete() ? 0.48F : 0.02F
    };
    lighting.setPointLights(hubPosition, hubColor,
        anchorPosition, anchorColor);
}
