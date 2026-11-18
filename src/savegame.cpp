#include "savegame.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "core.h"

namespace rpg {

namespace {

std::string addSuffix(const std::string& path, const std::string& suffix) {
    if (suffix.empty()) return path;
    size_t slash = path.find_last_of('/');
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + suffix;
    return path.substr(0, dot) + suffix + path.substr(dot);
}

int readInt(const std::map<std::string, std::string>& kv, const char* key, int def,
            int lo, int hi) {
    auto it = kv.find(key);
    if (it == kv.end()) return def;
    try {
        long v = std::stol(it->second);
        return clampInt(static_cast<int>(v), lo, hi);
    } catch (...) {
        return def;
    }
}

}  // namespace

std::string slotPath(const std::string& basePath, int slot) {
    switch (slot) {
        case -1:
            return addSuffix(basePath, "-auto");
        case 1:
            return addSuffix(basePath, "-2");
        case 2:
            return addSuffix(basePath, "-3");
        default:
            return basePath;
    }
}

std::string slotLabel(int slot) {
    if (slot < 0) return "Otomatis";
    return "Slot " + std::to_string(slot + 1);
}

SaveInfo inspectSave(const std::string& path) {
    SaveInfo info;
    std::error_code ec;
    info.exists = std::filesystem::is_regular_file(path, ec);
    if (!info.exists) return info;

    std::ifstream f(path);
    if (!f) return info;

    std::map<std::string, std::string> kv;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        kv[line.substr(0, sp)] = line.substr(sp + 1);
    }

    info.version = readInt(kv, "version", 0, 0, 999);
    info.map = readInt(kv, "map", -1, -1, 1024);
    info.level = readInt(kv, "level", 1, 1, 99);
    info.gold = readInt(kv, "gold", 0, 0, 9999999);
    info.valid = (info.version == 1 || info.version == 2) && info.map >= 0;
    return info;
}

bool anySaveExists(const std::string& basePath) {
    for (int slot = -1; slot <= 2; ++slot)
        if (inspectSave(slotPath(basePath, slot)).valid) return true;
    return false;
}

}  // namespace rpg
