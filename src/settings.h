// settings.h - persistent audio and display preferences
#pragma once
#include <string>

namespace rpg {

// Preferences live outside the savegame so they survive New Game and are
// shared by every save slot.
struct Settings {
    float musicVolume = 0.62f;
    float sfxVolume = 0.78f;
    bool muted = false;
    bool fullscreen = false;
};

// Missing files, unknown keys, and malformed values fall back to defaults.
Settings loadSettings(const std::string& path);

// Writes atomically through a temporary file. Returns false and fills err on
// failure; the previous file is left untouched when the rename fails.
bool saveSettings(const std::string& path, const Settings& settings,
                  std::string* err = nullptr);

// Installed layout: settings.cfg beside the binary when a source checkout is
// present. Source checkout: settings.cfg in the working directory.
std::string defaultSettingsPath();

}  // namespace rpg
