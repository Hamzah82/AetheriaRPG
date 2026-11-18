// audio.h - optional ALSA playback for procedural music and effects
#pragma once
#include <memory>
#include <string>

#include "audio_synth.h"

namespace rpg {

// The game runs fine without audio: init() reports failure and every other
// method becomes a no-op. ALSA is loaded at runtime, so no development
// headers or link-time dependency on libasound are required.
class Audio {
public:
    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init(std::string* err = nullptr);
    void shutdown();
    bool available() const;

    void playMusic(MusicId id);
    void stopMusic();
    void playSfx(SfxId id);

    void setMusicVolume(float volume);
    void setSfxVolume(float volume);
    float musicVolume() const;
    float sfxVolume() const;

    void setMuted(bool muted);
    void toggleMute();
    bool muted() const;

    const std::string& error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rpg
