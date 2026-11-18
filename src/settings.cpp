#include "settings.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "core.h"
#include "updater.h"

namespace rpg {

namespace {

bool parseFloat(const std::string& text, float* out) {
    try {
        size_t used = 0;
        float v = std::stof(text, &used);
        if (used == 0 || !std::isfinite(v)) return false;
        *out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseBool(const std::string& text, bool* out) {
    if (text == "1" || text == "true" || text == "yes" || text == "ya") {
        *out = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "no" || text == "tidak") {
        *out = false;
        return true;
    }
    return false;
}

std::string trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

}  // namespace

Settings loadSettings(const std::string& path) {
    Settings settings;
    std::ifstream f(path);
    if (!f) return settings;

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        float fv = 0.0f;
        bool bv = false;
        if (key == "music" && parseFloat(value, &fv)) {
            settings.musicVolume = clampf(fv, 0.0f, 1.0f);
        } else if (key == "sfx" && parseFloat(value, &fv)) {
            settings.sfxVolume = clampf(fv, 0.0f, 1.0f);
        } else if (key == "muted" && parseBool(value, &bv)) {
            settings.muted = bv;
        } else if (key == "fullscreen" && parseBool(value, &bv)) {
            settings.fullscreen = bv;
        }
    }
    return settings;
}

bool saveSettings(const std::string& path, const Settings& settings, std::string* err) {
    if (err) err->clear();
    std::error_code ec;
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) {
        if (err) *err = "Tidak bisa membuat folder pengaturan.";
        return false;
    }

    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) {
            if (err) *err = "Tidak bisa menulis berkas pengaturan.";
            return false;
        }
        f << "# Aetheria: Bayangan Terakhir\n";
        f << "music=" << clampf(settings.musicVolume, 0.0f, 1.0f) << "\n";
        f << "sfx=" << clampf(settings.sfxVolume, 0.0f, 1.0f) << "\n";
        f << "muted=" << (settings.muted ? 1 : 0) << "\n";
        f << "fullscreen=" << (settings.fullscreen ? 1 : 0) << "\n";
        if (!f) {
            if (err) *err = "Gagal menulis data pengaturan.";
            f.close();
            std::remove(tmp.c_str());
            return false;
        }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        if (err) *err = "Gagal menyimpan berkas pengaturan.";
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

std::string defaultSettingsPath() {
    std::string exeDir = executableDirectory();
    if (!exeDir.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(exeDir) / "source", ec))
            return exeDir + "/settings.cfg";
    }
    return "settings.cfg";
}

}  // namespace rpg
