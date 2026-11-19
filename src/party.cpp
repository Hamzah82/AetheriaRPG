#include "party.h"

#include <algorithm>

namespace rpg {

namespace {
const Companion BASE[static_cast<int>(CompanionId::Count)] = {
    {CompanionId::Sekar, "Sekar", "Vanguard", 1, 58, 58, 12, 5, 8, 3, 2, false, true},
    {CompanionId::Bima, "Bima", "Tank", 1, 84, 84, 9, 10, 3, 1, 3, false, true},
    {CompanionId::Larasati, "Larasati", "Healer", 1, 44, 44, 5, 4, 5, 2, 1, false, true},
    {CompanionId::Bagas, "Bagas", "Tactician", 1, 50, 50, 7, 5, 6, 1, 2, false, true},
};
}  // namespace

void Companion::recalc() {
    const Companion& b = BASE[static_cast<int>(id)];
    maxHp = b.maxHp + (level - 1) * 9;
    atk = b.atk + (level - 1) * 2;
    def = b.def + (level - 1);
    spd = b.spd;
    hp = std::clamp(hp, 0, maxHp);
    alive = hp > 0;
}

const Companion& companionDef(CompanionId id) {
    int i = static_cast<int>(id);
    if (i < 0 || i >= static_cast<int>(CompanionId::Count)) i = 0;
    return BASE[i];
}

Companion makeCompanion(CompanionId id, int level) {
    Companion c = BASE[static_cast<int>(id)];
    c.level = std::max(1, level);
    c.recalc();
    c.hp = c.maxHp;
    c.recruited = true;
    c.alive = true;
    return c;
}

}  // namespace rpg
