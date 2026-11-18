// audio_synth.h - deterministic procedural music and sound effects
#pragma once
#include <cstdint>
#include <vector>

namespace rpg {

constexpr int AUDIO_RATE = 44100;

enum class MusicId {
    None,
    Title,
    Village,
    Overworld,
    Cave,
    Battle,
    Boss,
    Ending,
    Count,
};

enum class SfxId {
    Step,
    MenuMove,
    MenuSelect,
    MenuCancel,
    Chest,
    Coin,
    Potion,
    Attack,
    Crit,
    Hurt,
    Fire,
    Thunder,
    Heal,
    Flee,
    Victory,
    Defeat,
    LevelUp,
    Save,
    Portal,
    BossRoar,
    Count,
};

// Both functions are deterministic and never touch an audio device.
std::vector<int16_t> buildMusic(MusicId id);
std::vector<int16_t> buildSfx(SfxId id);

const char* musicName(MusicId id);
const char* sfxName(SfxId id);

}  // namespace rpg
