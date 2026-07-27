#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 800;
constexpr float PLAYER_RADIUS = 0.65F;
constexpr float PLAYER_SPEED = 8.0F;
constexpr float PLAYER_ACCELERATION = 38.0F;
constexpr float PLAYER_DECELERATION = 46.0F;
constexpr float CAMERA_FOLLOW_SPEED = 10.0F;
constexpr float CAMERA_HEIGHT = 17.0F;
constexpr float CAMERA_HORIZONTAL_OFFSET = 12.0F;
constexpr float MAX_FRAME_TIME = 0.05F;
constexpr Vector3 DIRECTIONAL_LIGHT { 0.45F, -1.0F, 0.25F };

constexpr const char* LIGHTING_VERTEX_SHADER = R"(
#version 330

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr const char* LIGHTING_FRAGMENT_SHADER = R"(
#version 330

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 ambientColor;

out vec4 finalColor;

void main()
{
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 normal = normalize(fragNormal);
    float diffuse = max(dot(normal, -normalize(lightDirection)), 0.0);
    vec3 lighting = ambientColor + lightColor * diffuse;
    finalColor = vec4(surface.rgb * lighting, surface.a);
}
)";

struct Player {
    Vector3 position { 0.0F, PLAYER_RADIUS, 0.0F };
    Vector2 velocity {};
    Vector2 facing { 0.0F, -1.0F };
};

float length(Vector2 vector)
{
    return std::sqrt(vector.x * vector.x + vector.y * vector.y);
}

Vector2 normalized(Vector2 vector)
{
    const float magnitude = length(vector);
    if (magnitude <= 0.0001F) {
        return Vector2 {};
    }
    return Vector2 { vector.x / magnitude, vector.y / magnitude };
}

Vector2 moveTowards(Vector2 current, Vector2 target, float maximumChange)
{
    const Vector2 difference { target.x - current.x, target.y - current.y };
    const float distance = length(difference);
    if (distance <= maximumChange || distance <= 0.0001F) {
        return target;
    }

    const float scale = maximumChange / distance;
    return Vector2 {
        current.x + difference.x * scale,
        current.y + difference.y * scale
    };
}

void updateMovement(Player& player, float frameTime)
{
    const float horizontal
        = static_cast<float>(IsKeyDown(KEY_D)) - static_cast<float>(IsKeyDown(KEY_A));
    const float vertical
        = static_cast<float>(IsKeyDown(KEY_W)) - static_cast<float>(IsKeyDown(KEY_S));
    constexpr float diagonal = 0.70710678F;
    Vector2 input {
        diagonal * (horizontal - vertical),
        -diagonal * (horizontal + vertical)
    };
    input = normalized(input);

    const Vector2 targetVelocity {
        input.x * PLAYER_SPEED,
        input.y * PLAYER_SPEED
    };
    const float acceleration = length(input) > 0.0F
        ? PLAYER_ACCELERATION
        : PLAYER_DECELERATION;
    player.velocity = moveTowards(
        player.velocity, targetVelocity, acceleration * frameTime);

    player.position.x += player.velocity.x * frameTime;
    player.position.z += player.velocity.y * frameTime;
}

void updateCamera(Camera3D& camera, const Player& player, float frameTime)
{
    const float followAmount = 1.0F - std::exp(-CAMERA_FOLLOW_SPEED * frameTime);
    camera.target.x += (player.position.x - camera.target.x) * followAmount;
    camera.target.z += (player.position.z - camera.target.z) * followAmount;

    camera.position.x = camera.target.x + CAMERA_HORIZONTAL_OFFSET;
    camera.position.y = CAMERA_HEIGHT;
    camera.position.z = camera.target.z + CAMERA_HORIZONTAL_OFFSET;
}

bool groundPointUnderMouse(const Camera3D& camera, Vector3& point)
{
    const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    if (std::abs(ray.direction.y) <= 0.0001F) {
        return false;
    }

    const float distance = -ray.position.y / ray.direction.y;
    if (distance < 0.0F) {
        return false;
    }

    point = Vector3 {
        ray.position.x + ray.direction.x * distance,
        0.0F,
        ray.position.z + ray.direction.z * distance
    };
    return true;
}

void updateFacing(Player& player, const Vector3& aimPoint)
{
    const Vector2 direction {
        aimPoint.x - player.position.x,
        aimPoint.z - player.position.z
    };
    if (length(direction) > 0.05F) {
        player.facing = normalized(direction);
    }
}

void drawPlayerShadow(const Player& player, const Model& shadowModel)
{
    const float groundDistance = player.position.y / -DIRECTIONAL_LIGHT.y;
    const Vector3 shadowPosition {
        player.position.x + DIRECTIONAL_LIGHT.x * groundDistance,
        0.006F,
        player.position.z + DIRECTIONAL_LIGHT.z * groundDistance
    };
    const float shadowAngle = std::atan2(
        DIRECTIONAL_LIGHT.z, DIRECTIONAL_LIGHT.x) * RAD2DEG;
    DrawModelEx(shadowModel, shadowPosition, Vector3 { 0.0F, 1.0F, 0.0F },
        shadowAngle, Vector3 { 1.35F, 1.0F, 0.78F },
        Color { 7, 15, 17, 82 });
}

void drawPlayer(const Player& player, const Model& playerModel, Shader lightingShader)
{
    constexpr Color bodyColor { 239, 180, 74, 255 };
    constexpr Color facingColor { 255, 231, 145, 255 };

    DrawModel(playerModel, player.position, 1.0F, bodyColor);

    const Vector3 noseStart {
        player.position.x + player.facing.x * PLAYER_RADIUS * 0.55F,
        player.position.y,
        player.position.z + player.facing.y * PLAYER_RADIUS * 0.55F
    };
    const Vector3 noseEnd {
        player.position.x + player.facing.x * (PLAYER_RADIUS + 0.65F),
        player.position.y,
        player.position.z + player.facing.y * (PLAYER_RADIUS + 0.65F)
    };
    BeginShaderMode(lightingShader);
    DrawCylinderEx(noseStart, noseEnd, 0.18F, 0.05F, 12, facingColor);
    EndShaderMode();
}

} // namespace

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Stalberg game prototype");

    Shader lightingShader = LoadShaderFromMemory(
        LIGHTING_VERTEX_SHADER, LIGHTING_FRAGMENT_SHADER);
    const int lightDirectionLocation
        = GetShaderLocation(lightingShader, "lightDirection");
    const int lightColorLocation = GetShaderLocation(lightingShader, "lightColor");
    const int ambientColorLocation = GetShaderLocation(lightingShader, "ambientColor");
    const float lightDirection[3] {
        DIRECTIONAL_LIGHT.x,
        DIRECTIONAL_LIGHT.y,
        DIRECTIONAL_LIGHT.z
    };
    const float lightColor[3] { 0.78F, 0.75F, 0.68F };
    const float ambientColor[3] { 0.38F, 0.43F, 0.46F };
    SetShaderValue(lightingShader, lightDirectionLocation,
        lightDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, lightColorLocation,
        lightColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingShader, ambientColorLocation,
        ambientColor, SHADER_UNIFORM_VEC3);

    Model groundModel = LoadModelFromMesh(GenMeshPlane(80.0F, 80.0F, 1, 1));
    Model playerModel = LoadModelFromMesh(GenMeshSphere(PLAYER_RADIUS, 16, 24));
    Model shadowModel = LoadModelFromMesh(
        GenMeshCylinder(PLAYER_RADIUS * 1.05F, 0.01F, 32));
    groundModel.materials[0].shader = lightingShader;
    playerModel.materials[0].shader = lightingShader;

    Player player;
    Camera3D camera {};
    camera.position = Vector3 {
        CAMERA_HORIZONTAL_OFFSET,
        CAMERA_HEIGHT,
        CAMERA_HORIZONTAL_OFFSET
    };
    camera.target = Vector3 { 0.0F, 0.0F, 0.0F };
    camera.up = Vector3 { 0.0F, 1.0F, 0.0F };
    camera.fovy = 24.0F;
    camera.projection = CAMERA_ORTHOGRAPHIC;

    Vector3 aimPoint {};

    while (!WindowShouldClose()) {
        const float frameTime = std::min(GetFrameTime(), MAX_FRAME_TIME);
        updateMovement(player, frameTime);
        updateCamera(camera, player, frameTime);
        if (groundPointUnderMouse(camera, aimPoint)) {
            updateFacing(player, aimPoint);
        }

        BeginDrawing();
        ClearBackground(Color { 27, 39, 45, 255 });

        BeginMode3D(camera);
        DrawModel(groundModel, Vector3 { 0.0F, -0.015F, 0.0F }, 1.0F,
            Color { 64, 104, 105, 255 });
        DrawGrid(80, 1.0F);
        drawPlayerShadow(player, shadowModel);
        DrawSphere(Vector3 { aimPoint.x, 0.06F, aimPoint.z }, 0.12F,
            Color { 225, 241, 232, 210 });
        drawPlayer(player, playerModel, lightingShader);
        EndMode3D();

        DrawRectangle(16, 16, 310, 76, Color { 8, 25, 30, 220 });
        DrawText("MOVEMENT PROTOTYPE", 28, 27, 22, Color { 225, 241, 232, 255 });
        DrawText("WASD move  |  mouse aim", 28, 60, 17,
            Color { 151, 193, 190, 255 });
        DrawFPS(GetScreenWidth() - 96, 20);

        EndDrawing();
    }

    UnloadModel(shadowModel);
    UnloadModel(playerModel);
    UnloadModel(groundModel);
    UnloadShader(lightingShader);
    CloseWindow();
}
