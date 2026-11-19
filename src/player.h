// player.h - hero stats, inventory, equipment, leveling
#pragma once
#include <string>

#include "core.h"

namespace rpg {

enum class ItemId : uint8_t {
    Gold,
    Potion,
    Elixir,
    Oil,
    SaltCrystal,
    SoulDagger,
    TuningStaff,
    TirtaCore,
    SaltNoSorrow,
    KidungBook,
    CourtKey,
    AshCharm,
    ResonanceRing,
    Count,
};

struct ItemDef {
    const char* name;
    const char* desc;
    int price;
    bool usableInField;
};

struct WeaponDef {
    const char* name;
    int atk;
    int price;
};

struct ArmorDef {
    const char* name;
    int def;
    int price;
};

extern const ItemDef ITEMS[static_cast<int>(ItemId::Count)];
extern const WeaponDef WEAPONS[5];
extern const ArmorDef ARMORS[5];

struct Player {
    int level = 1;
    int xp = 0;
    int gold = 30;

    int hp = 44;
    int maxHp = 44;
    int mp = 14;
    int maxMp = 14;

    int baseAtk = 7;
    int baseDef = 5;

    int weapon = 0;
    int armor = 0;

    int potions = 2;
    int elixirs = 1;
    int oil = 2;
    int saltCrystal = 0;

    bool hasAshCharm = false;
    bool hasResonanceRing = false;
    bool hasSaltNoSorrow = false;
    bool hasKidungBook = false;
    bool hasTirtaCore = false;
    bool hasCourtKey = false;

    Player() { recalcStats(true); }

    int atk() const { return baseAtk + WEAPONS[weapon].atk; }
    int def() const { return baseDef + ARMORS[armor].def; }
    int xpToNext() const { return 14 + level * level * 4 + level * 6; }

    void recalcStats(bool fullHeal) {
        int nmaxHp = 36 + level * 9;
        int nmaxMp = 8 + level * 4;
        maxHp = nmaxHp;
        maxMp = nmaxMp;
        baseAtk = 5 + level * 2;
        baseDef = 3 + level * 2;
        if (fullHeal) {
            hp = maxHp;
            mp = maxMp;
        } else {
            hp = clampInt(hp, 0, maxHp);
            mp = clampInt(mp, 0, maxMp);
        }
    }

    // Adds xp, handles multiple level ups. Fills levelMsg with a short notice.
    int gainXp(int amount, std::string* levelMsg = nullptr) {
        if (amount <= 0) return 0;
        xp += amount;
        int ups = 0;
        while (xp >= xpToNext() && level < 99) {
            xp -= xpToNext();
            ++level;
            ++ups;
        }
        if (ups > 0) {
            recalcStats(true);
            if (levelMsg) {
                *levelMsg = "Naik ke Level " + std::to_string(level) + "!";
            }
        }
        return ups;
    }

    void healHp(int amount) { hp = clampInt(hp + amount, 0, maxHp); }
    void healMp(int amount) { mp = clampInt(mp + amount, 0, maxMp); }
    bool alive() const { return hp > 0; }

    bool usePotion() {
        if (potions <= 0 || hp >= maxHp) return false;
        --potions;
        healHp(45);
        return true;
    }
    bool useElixir() {
        if (elixirs <= 0 || mp >= maxMp) return false;
        --elixirs;
        healMp(30);
        return true;
    }

    void fullRestore() {
        hp = maxHp;
        mp = maxMp;
    }
};

}  // namespace rpg
