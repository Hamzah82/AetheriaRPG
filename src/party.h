// party.h - companion members and their combat roles
#pragma once
#include <cstdint>
#include <string>

#include "core.h"

namespace rpg {

enum class CompanionId : uint8_t { Sekar, Bima, Larasati, Bagas, Count };

struct Companion {
    CompanionId id = CompanionId::Sekar;
    std::string name;
    std::string role;
    int level = 1;
    int hp = 1;
    int maxHp = 1;
    int atk = 1;
    int def = 1;
    int spd = 1;
    int weapon = 0;
    int armor = 0;
    bool recruited = false;
    bool alive = true;

    void recalc();
    void healHp(int amount) { hp = clampInt(hp + amount, 0, maxHp); }
};

const Companion& companionDef(CompanionId id);
Companion makeCompanion(CompanionId id, int level);

}  // namespace rpg
