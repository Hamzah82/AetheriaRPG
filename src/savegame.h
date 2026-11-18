// savegame.h - save slot naming and lightweight metadata
#pragma once
#include <string>

namespace rpg {

// Slot 0 is the original savegame.dat path, so old saves keep working.
// Slots 1 and 2 add -2/-3 before the extension, and -1 is the autosave.
std::string slotPath(const std::string& basePath, int slot);

// Human readable label: "Slot 1", "Slot 2", "Slot 3", or "Otomatis".
std::string slotLabel(int slot);

// Saves are obfuscated and checksummed before they hit disk so casual text
// editing cannot change gold, level, or items. This is tamper deterrence, not
// real cryptography. Legacy plaintext saves are still accepted.
std::string encodeSaveText(const std::string& plain);
bool decodeSaveText(const std::string& raw, std::string* plain);

struct SaveInfo {
    bool exists = false;
    bool valid = false;
    int version = 0;
    int map = -1;
    int level = 1;
    int gold = 0;
};

// Reads only the header fields needed by the title/menu slot lists.
SaveInfo inspectSave(const std::string& path);

// True when at least one manual or automatic save can be loaded.
bool anySaveExists(const std::string& basePath);

}  // namespace rpg
