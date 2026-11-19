#include "player.h"

namespace rpg {

const ItemDef ITEMS[static_cast<int>(ItemId::Count)] = {
    {"Keping Garam", "Mata uang kepulauan Nusakalpa.", 1, false},
    {"Ramuan Pelita", "Memulihkan 45 HP.", 14, true},
    {"Eliksir Sumsum", "Memulihkan 30 MP.", 34, true},
    {"Minyak Sumsum", "Bahan bakar lentera dan api unggun.", 8, false},
    {"Garam Kenangan", "Kristal memori untuk pengrajin.", 20, false},
    {"Belati Penarah Jiwa", "Senjata dari pecahan inti meteorit.", 0, false},
    {"Tongkat Garpu Tala", "Senjata Larasati.", 0, false},
    {"Inti Tirta Purba", "Relik air purba penutup palung.", 0, false},
    {"Garam Tanpa Duka", "Menyerap Pengikisan Rasa.", 0, false},
    {"Kitab Kidung Pertama", "Syarat True Ending.", 0, false},
    {"Kunci Kraton", "Kunci rahasia ruang tahta.", 0, false},
    {"Jimat Penahan Abu", "Mengurangi racun dan pembatuan.", 0, false},
    {"Cincin Resonansi Perak", "Meningkatkan resonansi emosi.", 0, false},
};

const WeaponDef WEAPONS[5] = {
    {"Belati Penarah Garam", 2, 0},
    {"Belati Besi", 5, 80},
    {"Belati Penarah Jiwa", 9, 180},
    {"Pedang Cakrawala", 13, 0},
    {"Belati Pelita", 17, 0},
};

const ArmorDef ARMORS[5] = {
    {"Kain Biasa", 0, 0},
    {"Jas Kulit", 3, 60},
    {"Baju Rantai", 7, 150},
    {"Pelat Kapur", 11, 0},
    {"Jubah Pelita", 15, 0},
};

}  // namespace rpg
