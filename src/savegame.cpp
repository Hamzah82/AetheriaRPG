#include "savegame.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <utility>

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

constexpr char kSaveHeader[] = "AETHSAVE1\n";
constexpr char kPayloadMagic[] = "AES1";
constexpr size_t kPayloadPrefix = 12;  // magic (4) + checksum (8)
constexpr size_t kMaxRawSave = 4 * 1024 * 1024;
constexpr uint64_t kSaveSeed = 0xA37E1A5C0FFEE123ull;

uint64_t saveChecksum(const std::string& data) {
    uint64_t h = 1469598103934665603ull;  // FNV-1a
    for (unsigned char c : data) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t streamNext(uint64_t& state) {
    state += 0x9E3779B97F4A7C15ull;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Symmetric byte mask: the same call both hides and reveals the payload.
void mixSaveBytes(std::string& data, uint64_t seed) {
    uint64_t state = seed;
    uint64_t block = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        if ((i & 7) == 0) block = streamNext(state);
        unsigned char key = static_cast<unsigned char>((block >> ((i & 7) * 8)) & 0xFFu);
        data[i] = static_cast<char>(static_cast<unsigned char>(data[i]) ^ key);
    }
}

int base64Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::string base64Encode(const std::string& in) {
    static const char digits[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= in.size()) {
        uint32_t v = (static_cast<uint32_t>(static_cast<unsigned char>(in[i])) << 16) |
                     (static_cast<uint32_t>(static_cast<unsigned char>(in[i + 1])) << 8) |
                     static_cast<uint32_t>(static_cast<unsigned char>(in[i + 2]));
        out.push_back(digits[(v >> 18) & 63]);
        out.push_back(digits[(v >> 12) & 63]);
        out.push_back(digits[(v >> 6) & 63]);
        out.push_back(digits[v & 63]);
        i += 3;
    }
    if (i + 1 == in.size()) {
        uint32_t v = static_cast<uint32_t>(static_cast<unsigned char>(in[i])) << 16;
        out.push_back(digits[(v >> 18) & 63]);
        out.push_back(digits[(v >> 12) & 63]);
        out += "==";
    } else if (i + 2 == in.size()) {
        uint32_t v = (static_cast<uint32_t>(static_cast<unsigned char>(in[i])) << 16) |
                     (static_cast<uint32_t>(static_cast<unsigned char>(in[i + 1])) << 8);
        out.push_back(digits[(v >> 18) & 63]);
        out.push_back(digits[(v >> 12) & 63]);
        out.push_back(digits[(v >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

bool base64Decode(const std::string& in, std::string* out) {
    std::string clean;
    clean.reserve(in.size());
    for (char c : in) {
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        clean.push_back(c);
    }
    if (clean.empty() || clean.size() % 4 != 0) return false;
    out->clear();
    out->reserve((clean.size() / 4) * 3);
    for (size_t i = 0; i < clean.size(); i += 4) {
        bool pad2 = clean[i + 2] == '=';
        bool pad3 = clean[i + 3] == '=';
        if (pad2 && (!pad3 || i + 4 != clean.size())) return false;
        if (!pad2 && pad3 && i + 4 != clean.size()) return false;
        int a = base64Value(clean[i]);
        int b = base64Value(clean[i + 1]);
        int c = pad2 ? 0 : base64Value(clean[i + 2]);
        int d = pad3 ? 0 : base64Value(clean[i + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return false;
        uint32_t v = (static_cast<uint32_t>(a) << 18) | (static_cast<uint32_t>(b) << 12) |
                     (static_cast<uint32_t>(c) << 6) | static_cast<uint32_t>(d);
        out->push_back(static_cast<char>((v >> 16) & 0xFF));
        if (!pad2) out->push_back(static_cast<char>((v >> 8) & 0xFF));
        if (!pad3) out->push_back(static_cast<char>(v & 0xFF));
    }
    return true;
}

}  // namespace

std::string encodeSaveText(const std::string& plain) {
    uint64_t sum = saveChecksum(plain);
    std::string payload(kPayloadMagic);
    for (int i = 0; i < 8; ++i)
        payload.push_back(static_cast<char>((sum >> (8 * i)) & 0xFF));
    payload += plain;
    mixSaveBytes(payload, kSaveSeed);
    return std::string(kSaveHeader) + base64Encode(payload) + "\n";
}

bool decodeSaveText(const std::string& raw, std::string* plain) {
    if (!plain || raw.size() > kMaxRawSave) return false;
    plain->clear();

    // Files written before the encoded format (version 1 and 2) are plain
    // key/value text. Keep reading them so existing saves survive upgrades.
    if (raw.compare(0, sizeof(kSaveHeader) - 1, kSaveHeader) != 0) {
        *plain = raw;
        return true;
    }

    std::string payload;
    if (!base64Decode(raw.substr(sizeof(kSaveHeader) - 1), &payload)) return false;
    mixSaveBytes(payload, kSaveSeed);
    if (payload.size() < kPayloadPrefix || payload.compare(0, 4, kPayloadMagic) != 0)
        return false;

    uint64_t expected = 0;
    for (int i = 0; i < 8; ++i)
        expected |= static_cast<uint64_t>(static_cast<unsigned char>(payload[4 + i]))
                    << (8 * i);
    std::string text = payload.substr(kPayloadPrefix);
    if (saveChecksum(text) != expected) return false;
    *plain = std::move(text);
    return true;
}

bool isEncodedSaveText(const std::string& raw) {
    return raw.compare(0, sizeof(kSaveHeader) - 1, kSaveHeader) == 0;
}

bool writeSaveText(const std::string& path, const std::string& plain, std::string* err) {
    if (err) err->clear();
    std::string encoded = encodeSaveText(plain);
    std::string tmp = path + ".tmp";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) {
        if (err) *err = "Tidak bisa menulis berkas simpanan.";
        return false;
    }
    f.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    if (!f) {
        if (err) *err = "Gagal menulis data simpanan.";
        f.close();
        std::remove(tmp.c_str());
        return false;
    }
    f.close();
    // std::rename does not replace an existing file on Windows, while the
    // filesystem overload does; both platforms keep the replace atomic.
    std::error_code renameEc;
    std::filesystem::rename(tmp, path, renameEc);
    if (renameEc) {
        if (err) *err = "Gagal menyimpan berkas.";
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

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

    std::ifstream f(path, std::ios::binary);
    if (!f) return info;

    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    std::string text;
    if (!decodeSaveText(raw, &text)) return info;

    std::map<std::string, std::string> kv;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        kv[line.substr(0, sp)] = line.substr(sp + 1);
    }

    info.version = readInt(kv, "version", 0, 0, 999);
    info.map = readInt(kv, "map", -1, -1, 1024);
    info.level = readInt(kv, "level", 1, 1, 99);
    info.gold = readInt(kv, "gold", 0, 0, 9999999);
    info.valid = (info.version == 1 || info.version == 2 || info.version == 3) && info.map >= 0;
    return info;
}

bool anySaveExists(const std::string& basePath) {
    for (int slot = -1; slot <= 2; ++slot)
        if (inspectSave(slotPath(basePath, slot)).valid) return true;
    return false;
}

}  // namespace rpg
