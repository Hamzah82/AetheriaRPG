// audio_synth.cpp - chiptune synthesis with no external assets
#include "audio_synth.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "core.h"

namespace rpg {

namespace {

constexpr float kTau = 6.283185307179586f;

struct Noise {
    uint32_t state;
    explicit Noise(uint32_t seed) : state(seed ? seed : 0x12345678u) {}
    float next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(state) * (2.0f / 4294967295.0f) - 1.0f;
    }
};

enum class Wave { Square, Triangle, Sine, Saw };

float oscillate(Wave wave, float phase, float duty) {
    float p = phase - std::floor(phase);
    switch (wave) {
        case Wave::Square: return p < duty ? 1.0f : -1.0f;
        case Wave::Triangle: return 2.0f * std::fabs(2.0f * (p - std::floor(p + 0.5f))) - 1.0f;
        case Wave::Sine: return std::sin(kTau * p);
        case Wave::Saw: return 2.0f * (p - std::floor(p + 0.5f));
    }
    return 0.0f;
}

float midiFreq(int note) { return 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f); }

float envelope(float t, float duration, float attack, float release) {
    float env = 1.0f;
    if (attack > 0.0f && t < attack) env = t / attack;
    float left = duration - t;
    if (release > 0.0f && left < release) env = std::min(env, left / release);
    return clampf(env, 0.0f, 1.0f);
}

void addTone(std::vector<float>& out, float startSec, float duration, float freqStart, float freqEnd,
             Wave wave, float amp, float attack = 0.005f, float release = 0.02f, float decay = 0.0f,
             float vibrato = 0.0f, float vibratoHz = 5.5f, float duty = 0.5f) {
    if (out.empty() || duration <= 0.0f || amp <= 0.0f) return;
    int start = static_cast<int>(startSec * AUDIO_RATE);
    if (start >= static_cast<int>(out.size())) return;
    int length = static_cast<int>(duration * AUDIO_RATE);
    length = std::min(length, static_cast<int>(out.size()) - start);
    float phase = 0.0f;
    for (int i = 0; i < length; ++i) {
        float t = static_cast<float>(i) / AUDIO_RATE;
        float u = duration > 0.0f ? t / duration : 0.0f;
        float f = freqStart + (freqEnd - freqStart) * u;
        if (vibrato > 0.0f) f *= 1.0f + vibrato * std::sin(kTau * vibratoHz * t);
        phase += f / AUDIO_RATE;
        float env = envelope(t, duration, attack, release);
        if (decay > 0.0f) env *= std::exp(-decay * t);
        out[static_cast<size_t>(start + i)] +=
            oscillate(wave, phase, duty) * amp * env;
    }
}

void addNoise(std::vector<float>& out, float startSec, float duration, float amp, float cutoff,
              float attack = 0.003f, float release = 0.02f, uint32_t seed = 1u) {
    if (out.empty() || duration <= 0.0f || amp <= 0.0f) return;
    int start = static_cast<int>(startSec * AUDIO_RATE);
    if (start >= static_cast<int>(out.size())) return;
    int length = static_cast<int>(duration * AUDIO_RATE);
    length = std::min(length, static_cast<int>(out.size()) - start);
    Noise noise(seed);
    float filtered = 0.0f;
    for (int i = 0; i < length; ++i) {
        float t = static_cast<float>(i) / AUDIO_RATE;
        filtered += (noise.next() - filtered) * clampf(cutoff, 0.001f, 1.0f);
        float env = envelope(t, duration, attack, release);
        out[static_cast<size_t>(start + i)] += filtered * amp * env;
    }
}

std::vector<float> makeBuffer(float seconds) {
    int samples = std::max(1, static_cast<int>(seconds * AUDIO_RATE));
    return std::vector<float>(static_cast<size_t>(samples), 0.0f);
}

std::vector<int16_t> toPcm(const std::vector<float>& input, float targetPeak, bool normalizeUp) {
    float peak = 0.0f;
    for (float v : input) peak = std::max(peak, std::fabs(v));
    float scale = 0.0f;
    if (peak > 0.00001f) {
        if (normalizeUp || peak > targetPeak) scale = targetPeak / peak;
        else scale = 1.0f;
    }
    std::vector<int16_t> out(input.size(), 0);
    for (size_t i = 0; i < input.size(); ++i) {
        long v = std::lround(input[i] * scale * 32767.0f);
        out[i] = static_cast<int16_t>(clampInt(static_cast<int>(v), -32768, 32767));
    }
    return out;
}

void addNote(std::vector<float>& out, float start, float duration, int note, Wave wave, float amp,
             float duty, float decay = 1.5f) {
    if (note < 0 || note > 127) return;
    addTone(out, start, duration, midiFreq(note), midiFreq(note), wave, amp, 0.004f,
            std::min(0.05f, duration * 0.3f), decay, 0.0f, 5.5f, duty);
}

std::vector<int16_t> buildTrack(float bpm, const std::vector<int>& lead, const std::vector<int>& bass,
                                bool kick, bool hats, float leadAmp, float bassAmp,
                                uint32_t seed) {
    size_t steps = std::max(lead.size(), bass.size());
    if (steps == 0) return {};
    float stepDur = 60.0f / bpm / 2.0f;
    float loop = static_cast<float>(steps) * stepDur;
    std::vector<float> mix = makeBuffer(loop);
    int previousBass = -1;

    for (size_t i = 0; i < steps; ++i) {
        float at = static_cast<float>(i) * stepDur;
        int note = i < lead.size() ? lead[i] : -1;
        if (note >= 0) {
            addNote(mix, at, stepDur * 0.92f, note, Wave::Square, leadAmp, 0.25f, 2.2f);
            addNote(mix, at, stepDur * 0.92f, note - 12, Wave::Square, leadAmp * 0.28f, 0.125f, 2.8f);
        }
        int bassNote = i < bass.size() ? bass[i] : -1;
        if (bassNote >= 0 && (bassNote != previousBass || i == 0)) {
            addNote(mix, at, stepDur * 1.85f, bassNote, Wave::Triangle, bassAmp, 0.5f, 0.55f);
        }
        previousBass = bassNote;

        if (kick && i % 4 == 0) {
            addTone(mix, at, 0.13f, 92.0f, 42.0f, Wave::Sine, 0.26f, 0.002f, 0.05f, 7.0f);
        }
        if (hats && (i % 2 == 1)) {
            addNoise(mix, at, 0.035f, 0.075f, 0.72f, 0.002f, 0.012f,
                     seed + static_cast<uint32_t>(i) * 977u);
        }
    }
    return toPcm(mix, 0.62f, true);
}

std::vector<int16_t> buildSfxImpl(SfxId id) {
    switch (id) {
        case SfxId::Step: {
            std::vector<float> b = makeBuffer(0.08f);
            addNoise(b, 0.0f, 0.05f, 0.45f, 0.18f, 0.002f, 0.025f, 0x51A30001u);
            addTone(b, 0.0f, 0.055f, 100.0f, 62.0f, Wave::Triangle, 0.35f, 0.002f, 0.03f, 9.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::MenuMove: {
            std::vector<float> b = makeBuffer(0.07f);
            addTone(b, 0.0f, 0.045f, 880.0f, 880.0f, Wave::Square, 0.32f, 0.002f, 0.012f, 18.0f,
                    0.0f, 5.5f, 0.5f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::MenuSelect: {
            std::vector<float> b = makeBuffer(0.16f);
            addTone(b, 0.0f, 0.05f, 660.0f, 660.0f, Wave::Square, 0.3f, 0.002f, 0.015f, 8.0f);
            addTone(b, 0.045f, 0.09f, 990.0f, 990.0f, Wave::Square, 0.3f, 0.002f, 0.03f, 8.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::MenuCancel: {
            std::vector<float> b = makeBuffer(0.14f);
            addTone(b, 0.0f, 0.11f, 500.0f, 240.0f, Wave::Square, 0.32f, 0.003f, 0.03f, 4.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Chest: {
            std::vector<float> b = makeBuffer(0.62f);
            const int notes[] = {72, 76, 79, 84};
            for (int i = 0; i < 4; ++i) {
                addNote(b, 0.06f * static_cast<float>(i), 0.2f, notes[i], Wave::Square, 0.34f, 0.35f,
                        7.0f);
                addNote(b, 0.06f * static_cast<float>(i), 0.16f, notes[i] + 12, Wave::Sine, 0.1f, 0.5f,
                        9.0f);
            }
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Coin: {
            std::vector<float> b = makeBuffer(0.3f);
            addNote(b, 0.0f, 0.12f, 88, Wave::Square, 0.3f, 0.5f, 9.0f);
            addNote(b, 0.055f, 0.22f, 93, Wave::Square, 0.28f, 0.5f, 7.0f);
            addNote(b, 0.055f, 0.2f, 100, Wave::Sine, 0.08f, 0.5f, 8.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Potion: {
            std::vector<float> b = makeBuffer(0.42f);
            addTone(b, 0.0f, 0.3f, 300.0f, 720.0f, Wave::Sine, 0.42f, 0.01f, 0.06f, 2.2f, 0.035f,
                    17.0f);
            addNote(b, 0.14f, 0.16f, 86, Wave::Sine, 0.16f, 0.5f, 10.0f);
            addNote(b, 0.22f, 0.18f, 93, Wave::Sine, 0.14f, 0.5f, 10.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Attack: {
            std::vector<float> b = makeBuffer(0.22f);
            addNoise(b, 0.0f, 0.13f, 0.65f, 0.55f, 0.002f, 0.04f, 0xA77AC001u);
            addTone(b, 0.0f, 0.14f, 250.0f, 82.0f, Wave::Square, 0.42f, 0.002f, 0.05f, 11.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Crit: {
            std::vector<float> b = makeBuffer(0.38f);
            addNoise(b, 0.0f, 0.16f, 0.7f, 0.62f, 0.001f, 0.05f, 0xC8170001u);
            addTone(b, 0.0f, 0.18f, 300.0f, 90.0f, Wave::Saw, 0.38f, 0.001f, 0.06f, 12.0f);
            addTone(b, 0.03f, 0.24f, 1500.0f, 1100.0f, Wave::Sine, 0.22f, 0.002f, 0.1f, 9.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Hurt: {
            std::vector<float> b = makeBuffer(0.22f);
            addTone(b, 0.0f, 0.17f, 140.0f, 55.0f, Wave::Square, 0.5f, 0.002f, 0.05f, 9.0f);
            addNoise(b, 0.0f, 0.09f, 0.35f, 0.3f, 0.001f, 0.04f, 0x48A70001u);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Fire: {
            std::vector<float> b = makeBuffer(0.55f);
            addNoise(b, 0.0f, 0.45f, 0.7f, 0.14f, 0.015f, 0.2f, 0xF12E0001u);
            addTone(b, 0.0f, 0.36f, 100.0f, 52.0f, Wave::Saw, 0.3f, 0.01f, 0.12f, 2.4f);
            addNoise(b, 0.18f, 0.06f, 0.3f, 0.7f, 0.001f, 0.03f, 0xF12E0002u);
            addNoise(b, 0.3f, 0.05f, 0.22f, 0.6f, 0.001f, 0.025f, 0xF12E0003u);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Thunder: {
            std::vector<float> b = makeBuffer(0.62f);
            addNoise(b, 0.0f, 0.12f, 0.9f, 0.88f, 0.001f, 0.04f, 0x7E4D0001u);
            addNoise(b, 0.05f, 0.5f, 0.6f, 0.08f, 0.01f, 0.25f, 0x7E4D0002u);
            addTone(b, 0.02f, 0.5f, 62.0f, 42.0f, Wave::Triangle, 0.45f, 0.005f, 0.2f, 1.5f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Heal: {
            std::vector<float> b = makeBuffer(0.62f);
            const int notes[] = {72, 76, 79, 84};
            for (int i = 0; i < 4; ++i) {
                addNote(b, 0.07f * static_cast<float>(i), 0.34f, notes[i], Wave::Sine, 0.26f, 0.5f,
                        3.2f);
                addNote(b, 0.07f * static_cast<float>(i), 0.3f, notes[i] + 12, Wave::Sine, 0.07f, 0.5f,
                        4.0f);
            }
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Flee: {
            std::vector<float> b = makeBuffer(0.28f);
            for (int i = 0; i < 3; ++i) {
                addNoise(b, 0.06f * static_cast<float>(i), 0.045f, 0.42f, 0.4f, 0.002f, 0.02f,
                         0x11EE0000u + static_cast<uint32_t>(i));
            }
            addTone(b, 0.0f, 0.22f, 260.0f, 620.0f, Wave::Triangle, 0.2f, 0.01f, 0.05f, 2.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Victory: {
            std::vector<float> b = makeBuffer(1.05f);
            const int lead[] = {72, 76, 79, 84};
            for (int i = 0; i < 4; ++i) {
                addNote(b, 0.11f * static_cast<float>(i), 0.22f, lead[i], Wave::Square, 0.32f, 0.25f,
                        3.0f);
            }
            const int chord[] = {84, 88, 91};
            for (int n : chord) {
                addNote(b, 0.46f, 0.5f, n, Wave::Square, 0.18f, 0.35f, 2.0f);
                addNote(b, 0.46f, 0.5f, n - 12, Wave::Triangle, 0.12f, 0.5f, 1.5f);
            }
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Defeat: {
            std::vector<float> b = makeBuffer(1.05f);
            const int notes[] = {67, 64, 60, 55};
            for (int i = 0; i < 4; ++i) {
                addNote(b, 0.17f * static_cast<float>(i), 0.34f, notes[i], Wave::Triangle, 0.34f, 0.5f,
                        2.0f);
                addNote(b, 0.17f * static_cast<float>(i), 0.34f, notes[i] - 12, Wave::Sine, 0.14f, 0.5f,
                        2.0f);
            }
            return toPcm(b, 0.95f, false);
        }
        case SfxId::LevelUp: {
            std::vector<float> b = makeBuffer(0.52f);
            const int notes[] = {72, 76, 79, 84, 88};
            for (int i = 0; i < 5; ++i) {
                addNote(b, 0.055f * static_cast<float>(i), 0.16f, notes[i], Wave::Square, 0.28f, 0.25f,
                        5.0f);
            }
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Save: {
            std::vector<float> b = makeBuffer(0.46f);
            addNote(b, 0.0f, 0.2f, 84, Wave::Sine, 0.24f, 0.5f, 4.0f);
            addNote(b, 0.08f, 0.24f, 88, Wave::Sine, 0.22f, 0.5f, 4.0f);
            addNote(b, 0.16f, 0.28f, 91, Wave::Sine, 0.2f, 0.5f, 3.5f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Portal: {
            std::vector<float> b = makeBuffer(0.72f);
            addTone(b, 0.0f, 0.58f, 180.0f, 1400.0f, Wave::Sine, 0.32f, 0.01f, 0.15f, 1.2f, 0.03f,
                    11.0f);
            addNoise(b, 0.0f, 0.6f, 0.16f, 0.5f, 0.02f, 0.2f, 0x90170001u);
            addNote(b, 0.34f, 0.2f, 96, Wave::Sine, 0.12f, 0.5f, 7.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::BossRoar: {
            std::vector<float> b = makeBuffer(0.9f);
            addTone(b, 0.0f, 0.8f, 88.0f, 38.0f, Wave::Saw, 0.5f, 0.01f, 0.2f, 1.2f, 0.02f, 7.0f);
            addNoise(b, 0.0f, 0.8f, 0.38f, 0.08f, 0.01f, 0.25f, 0xB0550001u);
            addTone(b, 0.0f, 0.75f, 46.0f, 30.0f, Wave::Triangle, 0.3f, 0.01f, 0.2f, 1.0f);
            return toPcm(b, 0.95f, false);
        }
        case SfxId::Count:
            break;
    }
    return {};
}

}  // namespace

std::vector<int16_t> buildMusic(MusicId id) {
    switch (id) {
        case MusicId::Title:
            return buildTrack(78.0f, {69, 72, 76, 74, 72, 71, 69, 64, 69, 72, 76, 79, 76, 74, 72, 71},
                              {45, -1, 45, -1, 41, -1, 41, -1, 48, -1, 48, -1, 43, -1, 40, -1},
                              false, false, 0.16f, 0.13f, 0x10010001u);
        case MusicId::Village:
            return buildTrack(112.0f,
                              {72, 76, 79, 76, 69, 72, 76, 74, 72, 76, 79, 81, 79, 76, 74, 72},
                              {48, -1, 48, -1, 45, -1, 45, -1, 41, -1, 41, -1, 43, -1, 43, -1},
                              true, true, 0.17f, 0.12f, 0x10020001u);
        case MusicId::Overworld:
            return buildTrack(126.0f,
                              {74, 78, 81, 79, 78, 76, 74, 69, 74, 78, 81, 83, 81, 79, 78, 76},
                              {50, -1, 50, -1, 45, -1, 45, -1, 47, -1, 47, -1, 43, -1, 43, -1},
                              true, true, 0.17f, 0.12f, 0x10030001u);
        case MusicId::Cave:
            return buildTrack(72.0f, {62, 65, 69, 72, 69, 65, 64, 62, 62, 65, 69, 72, 74, 72, 69, 65},
                              {38, -1, 38, -1, 38, -1, 38, -1, 41, -1, 41, -1, 36, -1, 43, -1},
                              false, false, 0.15f, 0.14f, 0x10040001u);
        case MusicId::Battle:
            return buildTrack(150.0f,
                              {76, 79, 83, 81, 79, 78, 76, 71, 76, 79, 83, 86, 83, 81, 79, 78},
                              {52, -1, 47, -1, 48, -1, 50, -1, 52, -1, 47, -1, 48, -1, 50, -1},
                              true, true, 0.18f, 0.13f, 0x10050001u);
        case MusicId::Boss:
            return buildTrack(158.0f,
                              {72, 75, 79, 77, 75, 74, 72, 67, 72, 75, 79, 82, 79, 77, 75, 74},
                              {48, -1, 43, -1, 44, -1, 46, -1, 48, -1, 43, -1, 44, -1, 46, -1},
                              true, true, 0.19f, 0.14f, 0x10060001u);
        case MusicId::Ending:
            return buildTrack(92.0f, {65, 69, 72, 74, 72, 69, 67, 65, 65, 69, 72, 77, 76, 74, 72, 69},
                              {41, -1, 41, -1, 45, -1, 45, -1, 50, -1, 50, -1, 46, -1, 46, -1},
                              false, false, 0.16f, 0.13f, 0x10070001u);
        case MusicId::None:
        case MusicId::Count:
            break;
    }
    return {};
}

std::vector<int16_t> buildSfx(SfxId id) { return buildSfxImpl(id); }

const char* musicName(MusicId id) {
    switch (id) {
        case MusicId::None: return "hening";
        case MusicId::Title: return "judul";
        case MusicId::Village: return "desa";
        case MusicId::Overworld: return "kota";
        case MusicId::Cave: return "dungeon";
        case MusicId::Battle: return "pertarungan";
        case MusicId::Boss: return "boss";
        case MusicId::Ending: return "ending";
        case MusicId::Count: break;
    }
    return "?";
}

const char* sfxName(SfxId id) {
    switch (id) {
        case SfxId::Step: return "langkah";
        case SfxId::MenuMove: return "menu";
        case SfxId::MenuSelect: return "pilih";
        case SfxId::MenuCancel: return "batal";
        case SfxId::Chest: return "peti";
        case SfxId::Coin: return "emas";
        case SfxId::Potion: return "ramuan";
        case SfxId::Attack: return "serang";
        case SfxId::Crit: return "kritis";
        case SfxId::Hurt: return "luka";
        case SfxId::Fire: return "api";
        case SfxId::Thunder: return "petir";
        case SfxId::Heal: return "pulih";
        case SfxId::Flee: return "kabur";
        case SfxId::Victory: return "menang";
        case SfxId::Defeat: return "tumbang";
        case SfxId::LevelUp: return "naik level";
        case SfxId::Save: return "simpan";
        case SfxId::Portal: return "portal";
        case SfxId::BossRoar: return "auman boss";
        case SfxId::Count: break;
    }
    return "?";
}

}  // namespace rpg
