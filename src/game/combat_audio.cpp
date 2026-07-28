#include "combat_audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr unsigned int SAMPLE_RATE = 22050;
constexpr float TWO_PI = 6.28318530718F;

Sound makeTone(float startFrequency, float endFrequency,
    float duration, float gain)
{
    const auto sampleCount = static_cast<unsigned int>(
        static_cast<float>(SAMPLE_RATE) * duration);
    std::vector<std::int16_t> samples(sampleCount);
    float phase = 0.0F;
    for (unsigned int index = 0; index < sampleCount; ++index) {
        const float amount = static_cast<float>(index)
            / static_cast<float>(sampleCount);
        const float frequency = startFrequency
            + (endFrequency - startFrequency) * amount;
        phase += TWO_PI * frequency / static_cast<float>(SAMPLE_RATE);
        const float envelope = (1.0F - amount) * (1.0F - amount);
        const float sample = std::sin(phase) * envelope * gain;
        samples[index] = static_cast<std::int16_t>(std::clamp(
            sample, -1.0F, 1.0F) * 32767.0F);
    }

    Wave wave {
        .frameCount = sampleCount,
        .sampleRate = SAMPLE_RATE,
        .sampleSize = 16,
        .channels = 1,
        .data = samples.data()
    };
    return LoadSoundFromWave(wave);
}

} // namespace

CombatAudio::CombatAudio()
{
#if defined(PLATFORM_WEB)
    // Browser audio must be created synchronously from a user gesture. raylib's
    // current ScriptProcessor backend is not safe to initialize during startup,
    // so keep web audio disabled rather than destabilizing the game loop.
    return;
#endif
    InitAudioDevice();
    ready = IsAudioDeviceReady();
    if (!ready) {
        return;
    }

    enemyShot = makeTone(260.0F, 150.0F, 0.12F, 0.22F);
    playerHit = makeTone(120.0F, 72.0F, 0.18F, 0.34F);
    playerDeath = makeTone(180.0F, 42.0F, 0.55F, 0.38F);
    enemyHit = makeTone(520.0F, 360.0F, 0.08F, 0.18F);
    enemyDeath = makeTone(420.0F, 90.0F, 0.48F, 0.34F);
    restart = makeTone(240.0F, 520.0F, 0.24F, 0.24F);
}

CombatAudio::~CombatAudio()
{
    if (!ready) {
        return;
    }
    UnloadSound(restart);
    UnloadSound(enemyDeath);
    UnloadSound(enemyHit);
    UnloadSound(playerDeath);
    UnloadSound(playerHit);
    UnloadSound(enemyShot);
    CloseAudioDevice();
}

void CombatAudio::playEnemyShot() const
{
    if (ready) {
        PlaySound(enemyShot);
    }
}

void CombatAudio::playPlayerDamage(PlayerDamageResult result) const
{
    if (!ready || result == PlayerDamageResult::none) {
        return;
    }
    PlaySound(result == PlayerDamageResult::died ? playerDeath : playerHit);
}

void CombatAudio::playEnemyDamage(EnemyDamageResult result) const
{
    if (!ready || result == EnemyDamageResult::none) {
        return;
    }
    PlaySound(result == EnemyDamageResult::died ? enemyDeath : enemyHit);
}

void CombatAudio::playRestart() const
{
    if (ready) {
        PlaySound(restart);
    }
}
