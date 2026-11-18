#include "player.h"

namespace rpg {

const ItemDef ITEMS[static_cast<int>(ItemId::Count)] = {
    {"Emas", "Uang untuk berbelanja.", 1, false},
    {"Ramuan", "Memulihkan 40 HP.", 12, true},
    {"Eliksir", "Memulihkan 25 MP.", 30, true},
    {"Pedang Besi", "Senjata serangan +8.", 140, false},
    {"Baju Rantai", "Armor pertahanan +7.", 130, false},
    {"Batu Matahari", "Relik suci dari Gua Bayangan.", 0, false},
    {"Pedang Fajar", "Senjata legendaris serangan +13.", 0, false},
    {"Perisai Fajar", "Armor legendaris pertahanan +11.", 0, false},
};

const WeaponDef WEAPONS[4] = {
    {"Pedang Kayu", 1, 0},
    {"Pedang Perunggu", 4, 60},
    {"Pedang Besi", 8, 140},
    {"Pedang Fajar", 13, 0},
};

const ArmorDef ARMORS[4] = {
    {"Kain Biasa", 0, 0},
    {"Armor Kulit", 3, 50},
    {"Baju Rantai", 7, 130},
    {"Perisai Fajar", 11, 0},
};

}  // namespace rpg
