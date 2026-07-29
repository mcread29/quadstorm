#include "render_primitives.hpp"

#include "render_style.hpp"

#include <cmath>

namespace game_render {

void drawVoidGrid()
{
    constexpr int extent = 42;
    constexpr int spacing = 2;
    constexpr float height = -0.16F;
    for (int coordinate = -extent; coordinate <= extent;
         coordinate += spacing) {
        const bool major = coordinate % 10 == 0;
        const Color color = major
            ? Color { 18, 48, 52, 68 }
            : Color { 9, 23, 28, 34 };
        DrawLine3D(Vector3 { static_cast<float>(coordinate), height,
                       static_cast<float>(-extent) },
            Vector3 { static_cast<float>(coordinate), height,
                static_cast<float>(extent) }, color);
        DrawLine3D(Vector3 { static_cast<float>(-extent), height,
                       static_cast<float>(coordinate) },
            Vector3 { static_cast<float>(extent), height,
                static_cast<float>(coordinate) }, color);
    }

    const float pulse = 0.5F + 0.5F * std::sin(
        static_cast<float>(GetTime()) * 0.85F);
    for (const Vector2 position : { Vector2 { -28.0F, -20.0F },
             Vector2 { 26.0F, -24.0F }, Vector2 { -24.0F, 26.0F },
             Vector2 { 30.0F, 20.0F }, Vector2 { 4.0F, -34.0F } }) {
        DrawCube(Vector3 { position.x, -1.7F, position.y },
            0.42F, 3.0F, 0.42F, Color { 11, 25, 30, 210 });
        DrawCube(Vector3 { position.x, -0.24F, position.y },
            0.72F, 0.08F, 0.72F, Color { 34, 77, 78, 150 });
        DrawSphere(Vector3 { position.x, -0.08F, position.y },
            0.055F + pulse * 0.025F, Color { 49, 142, 140, 175 });
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
    const float pulse = 0.5F + 0.5F * std::sin(
        static_cast<float>(GetTime()) * 6.0F);
    const Vector3 center { aimPoint.x, 0.045F, aimPoint.z };
    DrawCircle3D(center, 0.25F + pulse * 0.05F,
        Vector3 { 1.0F, 0.0F, 0.0F }, 90.0F,
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

} // namespace game_render
