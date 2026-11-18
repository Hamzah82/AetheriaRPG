// audio.cpp - real-time ALSA playback, loaded dynamically at runtime
#include "audio.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

#include "core.h"

namespace rpg {

namespace {

struct AlsaPcm;
using FnOpen = int (*)(AlsaPcm**, const char*, int, int);
using FnSetParams = int (*)(AlsaPcm*, int, int, unsigned int, unsigned int, int, unsigned int);
using FnWrite = long (*)(AlsaPcm*, const void*, unsigned long);
using FnRecover = int (*)(AlsaPcm*, int, int);
using FnDrop = int (*)(AlsaPcm*);
using FnClose = int (*)(AlsaPcm*);

constexpr int kStreamPlayback = 0;
constexpr int kFormatS16LE = 2;
constexpr int kAccessRwInterleaved = 3;
constexpr int kFramesPerChunk = 512;

constexpr float kSfxGain[] = {
    0.30f,  // Step
    0.32f,  // MenuMove
    0.42f,  // MenuSelect
    0.42f,  // MenuCancel
    0.62f,  // Chest
    0.52f,  // Coin
    0.56f,  // Potion
    0.72f,  // Attack
    0.78f,  // Crit
    0.66f,  // Hurt
    0.72f,  // Fire
    0.78f,  // Thunder
    0.56f,  // Heal
    0.52f,  // Flee
    0.68f,  // Victory
    0.68f,  // Defeat
    0.62f,  // LevelUp
    0.46f,  // Save
    0.58f,  // Portal
    0.82f,  // BossRoar
};
static_assert(sizeof(kSfxGain) / sizeof(kSfxGain[0]) == static_cast<size_t>(SfxId::Count),
              "every sound effect needs a playback gain");

template <typename T>
bool loadSymbol(void* library, const char* name, T* out, std::string* error) {
    dlerror();
    void* symbol = dlsym(library, name);
    if (!symbol) {
        const char* detail = dlerror();
        if (error) *error = std::string("Simbol audio tidak ditemukan: ") + name +
                            (detail ? std::string(" (") + detail + ")" : "");
        return false;
    }
    static_assert(sizeof(T) == sizeof(symbol), "function pointer size mismatch");
    std::memcpy(out, &symbol, sizeof(symbol));
    return true;
}

}  // namespace

struct Audio::Impl {
    struct Voice {
        const std::vector<int16_t>* data = nullptr;
        size_t pos = 0;
        float gain = 1.0f;
    };

    void* library = nullptr;
    AlsaPcm* pcm = nullptr;
    FnOpen open = nullptr;
    FnSetParams setParams = nullptr;
    FnWrite write = nullptr;
    FnRecover recover = nullptr;
    FnDrop drop = nullptr;
    FnClose close = nullptr;

    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> muted{false};
    std::atomic<bool> paused{false};
    std::atomic<float> musicVol{0.62f};
    std::atomic<float> sfxVol{0.78f};

    std::mutex ctl;
    std::vector<SfxId> pending;
    MusicId requested = MusicId::None;
    bool requestDirty = false;

    std::vector<std::vector<int16_t>> music;
    std::vector<std::vector<int16_t>> sfx;
    std::string errorText;

    void buildBanks() {
        music.resize(static_cast<size_t>(MusicId::Count));
        for (int i = 1; i < static_cast<int>(MusicId::Count); ++i) {
            music[static_cast<size_t>(i)] = buildMusic(static_cast<MusicId>(i));
        }
        sfx.resize(static_cast<size_t>(SfxId::Count));
        for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
            sfx[static_cast<size_t>(i)] = buildSfx(static_cast<SfxId>(i));
        }
    }

    void closeResources() {
        running.store(false);
        if (worker.joinable()) worker.join();
        if (pcm) {
            if (drop) drop(pcm);
            if (close) close(pcm);
            pcm = nullptr;
        }
        if (library) {
            dlclose(library);
            library = nullptr;
        }
        open = nullptr;
        setParams = nullptr;
        write = nullptr;
        recover = nullptr;
        drop = nullptr;
        close = nullptr;
    }

    void run() {
        std::vector<int16_t> chunk(static_cast<size_t>(kFramesPerChunk), 0);
        std::vector<SfxId> events;
        std::vector<Voice> voices;
        MusicId active = MusicId::None;
        MusicId previous = MusicId::None;
        size_t musicPos = 0;
        size_t previousPos = 0;
        float fade = 0.0f;
        float previousFade = 0.0f;
        constexpr float fadeStep = 1.0f / (AUDIO_RATE * 0.35f);

        auto trackFor = [&](MusicId id) -> const std::vector<int16_t>* {
            if (id == MusicId::None || id == MusicId::Count) return nullptr;
            int idx = static_cast<int>(id);
            if (idx <= 0 || idx >= static_cast<int>(music.size())) return nullptr;
            const auto& data = music[static_cast<size_t>(idx)];
            return data.empty() ? nullptr : &data;
        };

        // Crossfade instead of hard cuts: the outgoing track keeps playing
        // under the incoming one for a third of a second.
        auto selectTrack = [&](MusicId id) {
            MusicId next = id == MusicId::Count ? MusicId::None : id;
            if (next == active) return;
            if (active != MusicId::None && trackFor(active)) {
                previous = active;
                previousPos = musicPos;
                previousFade = 1.0f;
            }
            active = next;
            musicPos = 0;
            fade = 0.0f;
        };

        while (running.load(std::memory_order_relaxed)) {
            {
                std::lock_guard<std::mutex> lock(ctl);
                if (requestDirty) {
                    requestDirty = false;
                    if (requested != active) selectTrack(requested);
                }
                events.swap(pending);
            }

            for (SfxId id : events) {
                int idx = static_cast<int>(id);
                if (idx < 0 || idx >= static_cast<int>(sfx.size())) continue;
                const std::vector<int16_t>& data = sfx[static_cast<size_t>(idx)];
                if (data.empty()) continue;
                if (voices.size() >= 12) voices.erase(voices.begin());
                Voice voice;
                voice.data = &data;
                voice.gain = kSfxGain[static_cast<size_t>(idx)];
                voices.push_back(voice);
            }
            events.clear();
            voices.erase(std::remove_if(voices.begin(), voices.end(),
                                        [](const Voice& v) {
                                            return !v.data || v.pos >= v.data->size();
                                        }),
                         voices.end());

            const std::vector<int16_t>* track = trackFor(active);
            const std::vector<int16_t>* outgoing = trackFor(previous);
            const bool silent = muted.load(std::memory_order_relaxed);
            const bool frozen = paused.load(std::memory_order_relaxed);
            const float mv = silent ? 0.0f : musicVol.load(std::memory_order_relaxed);
            const float sv = silent ? 0.0f : sfxVol.load(std::memory_order_relaxed);

            for (int i = 0; i < kFramesPerChunk; ++i) {
                int32_t sample = 0;
                if (!frozen) {
                    if (track) {
                        sample += static_cast<int32_t>((*track)[musicPos] * mv * fade);
                        ++musicPos;
                        if (musicPos >= track->size()) musicPos = 0;
                        if (fade < 1.0f) fade = std::min(1.0f, fade + fadeStep);
                    }
                    if (outgoing && previousFade > 0.0f) {
                        sample +=
                            static_cast<int32_t>((*outgoing)[previousPos] * mv * previousFade);
                        ++previousPos;
                        if (previousPos >= outgoing->size()) previousPos = 0;
                        previousFade = std::max(0.0f, previousFade - fadeStep);
                        if (previousFade <= 0.0f) previous = MusicId::None;
                    }
                    for (Voice& voice : voices) {
                        sample += static_cast<int32_t>((*voice.data)[voice.pos] * sv * voice.gain);
                        ++voice.pos;
                    }
                }
                sample = std::max(-32768, std::min(32767, static_cast<int>(sample)));
                chunk[static_cast<size_t>(i)] = static_cast<int16_t>(sample);
            }

            int written = 0;
            while (written < kFramesPerChunk && running.load(std::memory_order_relaxed)) {
                long n = write(pcm, chunk.data() + written,
                               static_cast<unsigned long>(kFramesPerChunk - written));
                if (n < 0) {
                    if (recover(pcm, static_cast<int>(n), 1) < 0) {
                        failed.store(true);
                        running.store(false);
                        return;
                    }
                    continue;
                }
                if (n == 0) {
                    std::this_thread::yield();
                    continue;
                }
                written += static_cast<int>(n);
            }
        }
    }
};

Audio::Audio() : impl_(new Impl()) {}

Audio::~Audio() { shutdown(); }

bool Audio::init(std::string* err) {
    Impl& d = *impl_;
    if (d.running.load()) {
        if (err) err->clear();
        return true;
    }
    d.closeResources();
    d.failed.store(false);
    d.errorText.clear();

    try {
        if (d.music.empty() || d.sfx.empty()) d.buildBanks();
    } catch (const std::exception& e) {
        d.errorText = std::string("Gagal menyiapkan suara: ") + e.what();
        if (err) *err = d.errorText;
        return false;
    }

    d.library = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!d.library) {
        d.errorText = "Pustaka libasound.so.2 tidak ditemukan. Game berjalan tanpa suara.";
        if (err) *err = d.errorText;
        return false;
    }

    bool symbolsOk = true;
    symbolsOk = symbolsOk && loadSymbol(d.library, "snd_pcm_open", &d.open, &d.errorText);
    symbolsOk = symbolsOk && loadSymbol(d.library, "snd_pcm_set_params", &d.setParams, &d.errorText);
    symbolsOk = symbolsOk && loadSymbol(d.library, "snd_pcm_writei", &d.write, &d.errorText);
    symbolsOk = symbolsOk && loadSymbol(d.library, "snd_pcm_recover", &d.recover, &d.errorText);
    symbolsOk = symbolsOk && loadSymbol(d.library, "snd_pcm_drop", &d.drop, &d.errorText);
    symbolsOk = symbolsOk && loadSymbol(d.library, "snd_pcm_close", &d.close, &d.errorText);
    if (!symbolsOk) {
        d.closeResources();
        if (err) *err = d.errorText;
        return false;
    }

    const char* devices[] = {"default", "pipewire", "pulse"};
    int rc = -1;
    for (const char* device : devices) {
        rc = d.open(&d.pcm, device, kStreamPlayback, 0);
        if (rc == 0 && d.pcm) break;
        d.pcm = nullptr;
    }
    if (rc != 0 || !d.pcm) {
        d.closeResources();
        d.errorText = "Tidak ada perangkat audio yang bisa dibuka. Game berjalan tanpa suara.";
        if (err) *err = d.errorText;
        return false;
    }

    rc = d.setParams(d.pcm, kFormatS16LE, kAccessRwInterleaved, 1, AUDIO_RATE, 1, 60000);
    if (rc < 0) {
        d.closeResources();
        d.errorText = "Perangkat audio menolak format 44100 Hz mono.";
        if (err) *err = d.errorText;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(d.ctl);
        d.pending.clear();
        d.requested = MusicId::None;
        d.requestDirty = false;
    }
    d.paused.store(false);
    d.running.store(true);
    try {
        d.worker = std::thread(&Impl::run, &d);
    } catch (const std::exception& e) {
        d.closeResources();
        d.errorText = std::string("Gagal membuat thread audio: ") + e.what();
        if (err) *err = d.errorText;
        return false;
    }
    if (err) err->clear();
    return true;
}

void Audio::shutdown() {
    Impl& d = *impl_;
    d.closeResources();
    std::lock_guard<std::mutex> lock(d.ctl);
    d.pending.clear();
    d.requested = MusicId::None;
    d.requestDirty = false;
    d.failed.store(false);
    d.paused.store(false);
}

bool Audio::available() const {
    const Impl& d = *impl_;
    return d.running.load() && !d.failed.load() && d.pcm != nullptr;
}

void Audio::playMusic(MusicId id) {
    Impl& d = *impl_;
    if (!available()) return;
    if (id == MusicId::Count) id = MusicId::None;
    std::lock_guard<std::mutex> lock(d.ctl);
    if (d.requested == id) return;
    d.requested = id;
    d.requestDirty = true;
}

void Audio::stopMusic() { playMusic(MusicId::None); }

void Audio::playSfx(SfxId id) {
    Impl& d = *impl_;
    if (!available()) return;
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(SfxId::Count)) return;
    std::lock_guard<std::mutex> lock(d.ctl);
    if (d.pending.size() < 32) d.pending.push_back(id);
}

void Audio::setMusicVolume(float volume) {
    impl_->musicVol.store(clampf(volume, 0.0f, 1.0f));
}

void Audio::setSfxVolume(float volume) { impl_->sfxVol.store(clampf(volume, 0.0f, 1.0f)); }

float Audio::musicVolume() const { return impl_->musicVol.load(); }

float Audio::sfxVolume() const { return impl_->sfxVol.load(); }

void Audio::setMuted(bool mute) { impl_->muted.store(mute); }

void Audio::toggleMute() { impl_->muted.store(!impl_->muted.load()); }

bool Audio::muted() const { return impl_->muted.load(); }

void Audio::setPaused(bool paused) { impl_->paused.store(paused); }

bool Audio::paused() const { return impl_->paused.load(); }

const std::string& Audio::error() const { return impl_->errorText; }

}  // namespace rpg
