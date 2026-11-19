#include "world.h"

namespace rpg {

bool tileBlocks(Tile t) {
    switch (t) {
        case Tile::Water:
        case Tile::DeepWater:
        case Tile::Tree:
        case Tile::Rock:
        case Tile::Wall:
        case Tile::Roof:
        case Tile::Door:
        case Tile::Fence:
        case Tile::CaveWall:
        case Tile::Lava:
        case Tile::Sign:
        case Tile::Pillar:
        case Tile::Void:
            return true;
        default:
            return false;
    }
}

namespace {

MapDef makeBase(const char* id, const char* name, int w, int h, Tile fill) {
    MapDef m;
    m.id = id;
    m.name = name;
    m.w = w;
    m.h = h;
    m.tiles.assign(static_cast<size_t>(w) * h, fill);
    return m;
}

void rect(MapDef& m, int x, int y, int w, int h, Tile t) {
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx) m.set(xx, yy, t);
}

void border(MapDef& m, Tile t) {
    rect(m, 0, 0, m.w, 1, t);
    rect(m, 0, m.h - 1, m.w, 1, t);
    rect(m, 0, 0, 1, m.h, t);
    rect(m, m.w - 1, 0, 1, m.h, t);
}

// House is 4 tiles tall: two roof rows, two wall rows, one door on the bottom row.
void house(MapDef& m, int x, int y, int w) {
    rect(m, x, y, w, 2, Tile::Roof);
    rect(m, x, y + 2, w, 2, Tile::Wall);
    m.set(x + w / 2, y + 3, Tile::Door);
}

void scatterFlowers(MapDef& m, Rng& rng, float chance) {
    for (int y = 1; y < m.h - 1; ++y)
        for (int x = 1; x < m.w - 1; ++x)
            if (m.at(x, y) == Tile::Grass && rng.chance(chance)) m.set(x, y, Tile::Flower);
}

void scatterTiles(MapDef& m, Rng& rng, Tile t, float chance) {
    for (int y = 1; y < m.h - 1; ++y)
        for (int x = 1; x < m.w - 1; ++x)
            if (m.at(x, y) == Tile::Grass && rng.chance(chance)) m.set(x, y, t);
}

}  // namespace

World::World() { build(); }

const MapDef& World::def(int index) const {
    static const MapDef dummy{};
    if (index < 0 || index >= static_cast<int>(maps_.size())) return dummy;
    return maps_[index];
}

int World::indexOf(const std::string& id) const {
    for (size_t i = 0; i < maps_.size(); ++i)
        if (maps_[i].id == id) return static_cast<int>(i);
    return -1;
}

Tile World::tile(int map, int tx, int ty) const { return def(map).at(tx, ty); }

bool World::inBounds(int map, int tx, int ty) const {
    const MapDef& m = def(map);
    return tx >= 0 && ty >= 0 && tx < m.w && ty < m.h;
}

bool World::blocked(int map, int tx, int ty) const {
    if (!inBounds(map, tx, ty)) return true;
    return tileBlocks(tile(map, tx, ty));
}

bool World::encounter(int map, int tx, int ty) const {
    const MapDef& m = def(map);
    if (!inBounds(map, tx, ty)) return false;
    Tile t = tile(map, tx, ty);
    if (m.encounterSet == 1) return t == Tile::TallGrass;
    if (m.encounterSet == 2) return t == Tile::CaveFloor;
    if (m.encounterSet == 3) return t == Tile::CaveFloor;
    if (m.encounterSet == 4) return t == Tile::CaveFloor;
    return false;
}

const Portal* World::portalAt(int map, int tx, int ty) const {
    const MapDef& m = def(map);
    for (const auto& p : m.portals)
        if (tx >= p.x && ty >= p.y && tx < p.x + p.w && ty < p.y + p.h) return &p;
    return nullptr;
}

const NpcSpawn* World::npcAt(int map, int tx, int ty) const {
    const MapDef& m = def(map);
    for (const auto& n : m.npcs)
        if (n.x == tx && n.y == ty) return &n;
    return nullptr;
}

const ChestSpawn* World::chestAt(int map, int tx, int ty) const {
    const MapDef& m = def(map);
    for (const auto& c : m.chests)
        if (c.x == tx && c.y == ty) return &c;
    return nullptr;
}

const EchoSpot* World::echoAt(int map, int tx, int ty) const {
    const MapDef& m = def(map);
    for (const auto& e : m.echoes)
        if (e.x == tx && e.y == ty) return &e;
    return nullptr;
}

const CampSpot* World::campAt(int map, int tx, int ty) const {
    const MapDef& m = def(map);
    for (const auto& c : m.camps)
        if (c.x == tx && c.y == ty) return &c;
    return nullptr;
}

void World::build() {
    // ---------------- 0: Desa Tambak-Abu ----------------
    {
        MapDef m = makeBase("village", "Desa Tambak-Abu", 30, 20, Tile::Grass);
        border(m, Tile::Tree);

        rect(m, 1, 12, 6, 5, Tile::Sand);
        rect(m, 2, 13, 4, 3, Tile::Water);

        rect(m, 14, 5, 2, 15, Tile::Path);
        rect(m, 5, 10, 21, 2, Tile::Path);
        rect(m, 12, 10, 6, 3, Tile::Path);

        house(m, 11, 1, 8);   // Warung Asap, door (15,4)
        house(m, 3, 6, 6);    // Rumah Raka, door (6,9)
        house(m, 21, 6, 6);   // Pos Garda, door (24,9)

        rect(m, 27, 2, 2, 4, Tile::Tree);
        rect(m, 1, 1, 2, 2, Tile::Tree);
        rect(m, 27, 13, 2, 4, Tile::Tree);
        rect(m, 2, 17, 3, 2, Tile::Tree);
        rect(m, 24, 17, 3, 2, Tile::Tree);

        Rng rng(0xADD1C7);
        scatterFlowers(m, rng, 0.05f);

        m.set(17, 12, Tile::Sign);
        m.echoes.push_back({18, 12, 0});
        m.camps.push_back({14, 16});

        m.npcs.push_back({12, 6, NpcStyle::MbokRatih, 0, 3});
        m.npcs.push_back({16, 16, NpcStyle::Seno, 1, 2});
        m.npcs.push_back({21, 14, NpcStyle::Villager, 6, 0});
        m.npcs.push_back({9, 14, NpcStyle::Kid, 3, 0});
        m.npcs.push_back({9, 7, NpcStyle::Farmer, 2, 0});

        m.chests.push_back({26, 3, ItemId::Oil, 2});

        m.portals.push_back({14, 18, 2, 1, 1, 17, 20, 0});
        maps_.push_back(std::move(m));
    }

    // ---------------- 1: Pelabuhan Besi Kertarajasa ----------------
    {
        MapDef m = makeBase("harbor", "Pelabuhan Besi Kertarajasa", 34, 22, Tile::Path);
        border(m, Tile::Wall);

        house(m, 2, 2, 6);    // Toko Joko, door (5,5)
        house(m, 12, 2, 7);   // Gudang minyak, door (15,5)
        house(m, 24, 2, 6);   // Balai kapal, door (27,5)
        house(m, 2, 12, 6);   // Bengkel Bima, door (5,15)
        house(m, 24, 12, 6);  // Barak Garda, door (27,15)

        rect(m, 8, 8, 18, 2, Tile::Path);
        rect(m, 16, 4, 2, 12, Tile::Path);
        rect(m, 4, 8, 2, 8, Tile::Path);
        rect(m, 28, 8, 2, 8, Tile::Path);

        rect(m, 13, 12, 8, 3, Tile::Sand);
        m.set(15, 13, Tile::Sign);
        m.set(18, 13, Tile::Sign);

        m.camps.push_back({17, 18});
        m.echoes.push_back({8, 8, 1});
        m.echoes.push_back({28, 8, 2});

        m.npcs.push_back({5, 6, NpcStyle::JokoKeling, 10, 0});
        m.npcs.push_back({15, 9, NpcStyle::Bima, 11, 0});
        m.npcs.push_back({26, 9, NpcStyle::Sekar, 12, 0});
        m.npcs.push_back({20, 15, NpcStyle::Larasati, 13, 0});
        m.npcs.push_back({10, 15, NpcStyle::Seno, 14, 2});
        m.npcs.push_back({17, 10, NpcStyle::Guard, 15, 2});

        m.chests.push_back({30, 4, ItemId::Oil, 2});
        m.chests.push_back({14, 16, ItemId::SaltCrystal, 3});

        m.portals.push_back({2, 18, 1, 1, 0, 14, 18, 3});
        m.portals.push_back({31, 18, 1, 1, 2, 2, 18, 2});
        m.portals.push_back({20, 2, 1, 1, 4, 17, 24, 1, 1});
        m.portals.push_back({1, 8, 1, 1, 5, 2, 16, 3, 2});
        m.portals.push_back({19, 2, 1, 1, 7, 15, 20, 1, 3});
        maps_.push_back(std::move(m));
    }

    // ---------------- 2: Rimba Karang-Kelabu ----------------
    {
        MapDef m = makeBase("rimba", "Rimba Karang-Kelabu", 32, 24, Tile::Grass);
        border(m, Tile::Tree);

        rect(m, 1, 17, 8, 2, Tile::Path);
        rect(m, 8, 17, 2, 5, Tile::Path);
        rect(m, 10, 20, 12, 2, Tile::Path);

        rect(m, 4, 4, 7, 5, Tile::Tree);
        rect(m, 16, 3, 8, 6, Tile::Tree);
        rect(m, 24, 12, 6, 6, Tile::Tree);
        rect(m, 5, 12, 6, 5, Tile::Rock);
        rect(m, 20, 14, 3, 3, Tile::Rock);

        rect(m, 12, 6, 5, 4, Tile::TallGrass);
        rect(m, 22, 5, 4, 3, Tile::TallGrass);
        rect(m, 7, 18, 5, 3, Tile::TallGrass);
        rect(m, 16, 18, 6, 3, Tile::TallGrass);
        rect(m, 27, 18, 4, 3, Tile::TallGrass);

        Rng rng(0xCAF0D);
        scatterTiles(m, rng, Tile::TallGrass, 0.03f);

        m.camps.push_back({6, 18});
        m.echoes.push_back({24, 8, 3});
        m.echoes.push_back({28, 18, 4});

        m.npcs.push_back({20, 12, NpcStyle::Villager, 20, 0});
        m.npcs.push_back({26, 6, NpcStyle::Pertapa, 21, 0});

        m.chests.push_back({11, 4, ItemId::Potion, 2});
        m.chests.push_back({29, 5, ItemId::SaltCrystal, 2});

        m.portals.push_back({2, 18, 1, 1, 1, 31, 18, 3});
        m.portals.push_back({21, 21, 1, 1, 3, 16, 21, 1, 2});
        m.encounterSet = 1;
        maps_.push_back(std::move(m));
    }

    // ---------------- 3: Sumur Ratapan ----------------
    {
        MapDef m = makeBase("sumur", "Sumur Ratapan", 30, 24, Tile::CaveWall);

        rect(m, 12, 19, 10, 5, Tile::CaveFloor);
        rect(m, 16, 14, 2, 6, Tile::CaveFloor);
        rect(m, 5, 13, 8, 6, Tile::CaveFloor);
        rect(m, 12, 15, 11, 2, Tile::CaveFloor);
        rect(m, 22, 12, 6, 7, Tile::CaveFloor);
        rect(m, 19, 8, 4, 5, Tile::CaveFloor);
        rect(m, 11, 2, 12, 7, Tile::CaveFloor);

        m.set(16, 22, Tile::CaveEntrance);

        rect(m, 23, 13, 2, 2, Tile::Lava);
        rect(m, 27, 13, 2, 1, Tile::Lava);
        m.set(13, 3, Tile::Pillar);
        m.set(21, 3, Tile::Pillar);
        m.set(13, 6, Tile::Pillar);
        m.set(21, 6, Tile::Pillar);

        m.echoes.push_back({16, 8, 5});
        m.echoes.push_back({22, 12, 6});

        m.npcs.push_back({16, 4, NpcStyle::Warden, 30, 0});

        m.chests.push_back({7, 16, ItemId::Elixir, 2});
        m.chests.push_back({26, 15, ItemId::Oil, 2});
        m.chests.push_back({13, 4, ItemId::SoulDagger, 1});

        m.portals.push_back({16, 22, 1, 1, 2, 21, 21, 0});
        m.encounterSet = 2;
        maps_.push_back(std::move(m));
    }

    // ---------------- 4: Candi Tenggelam Saptatirta ----------------
    {
        MapDef m = makeBase("candi", "Candi Tenggelam Saptatirta", 34, 26, Tile::CaveWall);

        rect(m, 12, 21, 12, 5, Tile::CaveFloor);
        rect(m, 17, 14, 2, 7, Tile::CaveFloor);
        rect(m, 7, 8, 22, 7, Tile::CaveFloor);
        rect(m, 3, 10, 5, 5, Tile::CaveFloor);
        rect(m, 27, 10, 6, 5, Tile::CaveFloor);
        rect(m, 12, 2, 12, 7, Tile::CaveFloor);

        m.set(17, 24, Tile::RuinEntrance);

        rect(m, 14, 11, 2, 1, Tile::Lava);
        rect(m, 20, 12, 3, 1, Tile::Lava);

        m.set(13, 3, Tile::Pillar);
        m.set(22, 3, Tile::Pillar);
        m.set(13, 6, Tile::Pillar);
        m.set(22, 6, Tile::Pillar);

        m.echoes.push_back({8, 10, 7});
        m.echoes.push_back({26, 10, 8});

        m.npcs.push_back({17, 8, NpcStyle::Bagas, 40, 0});
        m.npcs.push_back({17, 4, NpcStyle::Guardian, 41, 0});

        m.chests.push_back({4, 12, ItemId::TuningStaff, 1});
        m.chests.push_back({29, 12, ItemId::Oil, 2});
        m.chests.push_back({17, 14, ItemId::TirtaCore, 1});

        m.portals.push_back({17, 24, 1, 1, 1, 20, 2, 0});
        m.encounterSet = 3;
        maps_.push_back(std::move(m));
    }

    // ---------------- 5: Biara Ordo Pelita Putih ----------------
    {
        MapDef m = makeBase("biara", "Biara Ordo Pelita Putih", 28, 20, Tile::Grass);
        border(m, Tile::Tree);

        house(m, 3, 2, 7);    // Aula biara, door (6,5)
        house(m, 18, 2, 6);   // Asrama, door (21,5)
        house(m, 3, 12, 6);   // Klinik, door (6,15)

        rect(m, 8, 8, 14, 2, Tile::Path);
        rect(m, 14, 4, 2, 12, Tile::Path);
        rect(m, 10, 12, 8, 2, Tile::Sand);

        Rng rng(0xB1A2A);
        scatterFlowers(m, rng, 0.04f);

        m.camps.push_back({14, 16});
        m.echoes.push_back({14, 8, 9});

        m.npcs.push_back({7, 6, NpcStyle::Nirmala, 50, 0});
        m.npcs.push_back({20, 14, NpcStyle::Pelarian, 51, 0});
        m.npcs.push_back({6, 16, NpcStyle::Villager, 52, 0});

        m.chests.push_back({24, 16, ItemId::Oil, 2});
        m.chests.push_back({5, 16, ItemId::Potion, 2});

        m.portals.push_back({2, 16, 1, 1, 1, 1, 8, 3});
        m.portals.push_back({25, 5, 1, 1, 6, 2, 16, 2, 3});
        maps_.push_back(std::move(m));
    }

    // ---------------- 6: Taman Lentera Mati ----------------
    {
        MapDef m = makeBase("taman", "Taman Lentera Mati", 26, 18, Tile::Sand);
        border(m, Tile::Rock);

        rect(m, 3, 3, 20, 2, Tile::Rock);
        rect(m, 3, 13, 20, 2, Tile::Rock);
        rect(m, 5, 7, 16, 4, Tile::Path);

        for (int x = 4; x <= 22; x += 3) {
            m.set(x, 5, Tile::Pillar);
            m.set(x, 12, Tile::Pillar);
        }

        m.echoes.push_back({13, 8, 10});
        m.npcs.push_back({20, 10, NpcStyle::Pelarian, 60, 0});

        m.chests.push_back({23, 5, ItemId::SaltNoSorrow, 1});
        m.chests.push_back({3, 5, ItemId::KidungBook, 1});

        m.portals.push_back({2, 16, 1, 1, 5, 25, 5, 3});
        maps_.push_back(std::move(m));
    }

    // ---------------- 7: Kraton Pualam Surya ----------------
    {
        MapDef m = makeBase("kraton", "Kraton Pualam Surya", 30, 22, Tile::Path);
        border(m, Tile::Wall);

        house(m, 10, 2, 10);   // Ruang tahta, door (15,5)
        house(m, 2, 2, 6);     // Aula, door (5,5)
        house(m, 22, 2, 6);    // Gajah, door (25,5)
        house(m, 2, 12, 6);    // Klinik, door (5,15)
        house(m, 22, 12, 6);   // Penjara, door (25,15)

        rect(m, 8, 8, 14, 2, Tile::Sand);
        rect(m, 14, 5, 2, 12, Tile::Path);
        rect(m, 8, 14, 14, 2, Tile::Path);

        m.camps.push_back({15, 18});
        m.echoes.push_back({8, 10, 11});

        m.npcs.push_back({15, 8, NpcStyle::Kirana, 70, 0});
        m.npcs.push_back({15, 6, NpcStyle::Adipati, 71, 0});
        m.npcs.push_back({20, 4, NpcStyle::Guntur, 72, 0});
        m.npcs.push_back({10, 14, NpcStyle::Seno, 73, 2});
        m.npcs.push_back({25, 16, NpcStyle::Guard, 74, 2});

        m.chests.push_back({4, 16, ItemId::Oil, 2});
        m.chests.push_back({25, 16, ItemId::SaltCrystal, 2});
        m.chests.push_back({5, 16, ItemId::CourtKey, 1});

        m.portals.push_back({15, 20, 1, 1, 1, 19, 2, 2});
        m.portals.push_back({8, 2, 1, 1, 8, 17, 24, 1, 4});
        maps_.push_back(std::move(m));
    }

    // ---------------- 8: Jantung Palung Sunyi ----------------
    {
        MapDef m = makeBase("palung", "Jantung Palung Sunyi", 34, 26, Tile::CaveWall);

        rect(m, 12, 20, 10, 6, Tile::CaveFloor);
        rect(m, 16, 14, 2, 7, Tile::CaveFloor);
        rect(m, 8, 8, 18, 7, Tile::CaveFloor);
        rect(m, 12, 2, 12, 7, Tile::CaveFloor);
        rect(m, 3, 10, 5, 5, Tile::CaveFloor);
        rect(m, 26, 10, 5, 5, Tile::CaveFloor);

        m.set(17, 24, Tile::CaveEntrance);

        rect(m, 14, 11, 2, 1, Tile::Lava);
        rect(m, 20, 12, 3, 1, Tile::Lava);
        m.set(13, 3, Tile::Pillar);
        m.set(22, 3, Tile::Pillar);
        m.set(13, 6, Tile::Pillar);
        m.set(22, 6, Tile::Pillar);

        m.echoes.push_back({17, 10, 11});

        m.npcs.push_back({17, 9, NpcStyle::Guntur, 80, 0});
        m.npcs.push_back({17, 3, NpcStyle::Guardian, 81, 0});

        m.chests.push_back({13, 20, ItemId::Elixir, 2});
        m.chests.push_back({20, 20, ItemId::Oil, 2});

        m.portals.push_back({17, 24, 1, 1, 7, 8, 2, 0});
        m.encounterSet = 4;
        maps_.push_back(std::move(m));
    }
}

}  // namespace rpg
