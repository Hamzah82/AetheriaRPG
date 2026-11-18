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

void World::build() {
    // ---------------- Village of Elmwood ----------------
    {
        MapDef m = makeBase("village", "Desa Elmwood", 30, 20, Tile::Grass);
        border(m, Tile::Tree);

        // Pond with a sandy shore.
        rect(m, 1, 12, 6, 5, Tile::Sand);
        rect(m, 2, 13, 4, 3, Tile::Water);

        // Roads.
        rect(m, 14, 5, 2, 15, Tile::Path);
        rect(m, 5, 10, 21, 2, Tile::Path);
        rect(m, 12, 10, 6, 3, Tile::Path);

        // Buildings.
        house(m, 11, 1, 8);   // Elder's hall, door at (15,4)
        house(m, 3, 6, 6);    // Shop, door at (6,9)
        house(m, 21, 6, 6);   // Inn, door at (24,9)

        // Trees and greenery.
        rect(m, 27, 2, 2, 4, Tile::Tree);
        rect(m, 1, 1, 2, 2, Tile::Tree);
        rect(m, 27, 13, 2, 4, Tile::Tree);
        rect(m, 2, 17, 3, 2, Tile::Tree);
        rect(m, 24, 17, 3, 2, Tile::Tree);

        Rng rng(0xA11CE);
        scatterFlowers(m, rng, 0.06f);

        m.set(17, 12, Tile::Sign);

        m.npcs.push_back({12, 6, NpcStyle::Elder, 0, 3});
        m.npcs.push_back({6, 12, NpcStyle::Shopkeeper, 1, 0});
        m.npcs.push_back({24, 12, NpcStyle::Innkeeper, 2, 0});
        m.npcs.push_back({16, 16, NpcStyle::Guard, 3, 2});
        m.npcs.push_back({9, 14, NpcStyle::Kid, 4, 0});
        m.npcs.push_back({21, 14, NpcStyle::Farmer, 5, 2});
        m.npcs.push_back({9, 7, NpcStyle::Villager, 8, 0});

        m.chests.push_back({26, 3, ItemId::Gold, 20});

        m.portals.push_back({14, 19, 2, 1, 1, 5, 27, 0});

        maps_.push_back(std::move(m));
    }

    // ---------------- Aetheria Plains ----------------
    {
        MapDef m = makeBase("overworld", "Padang Aetheria", 40, 30, Tile::Grass);
        border(m, Tile::Tree);

        // River with a wooden bridge.
        rect(m, 24, 0, 3, 30, Tile::Water);
        rect(m, 24, 14, 3, 3, Tile::Bridge);

        // Road network.
        rect(m, 5, 22, 1, 7, Tile::Path);
        rect(m, 5, 22, 14, 1, Tile::Path);
        rect(m, 18, 15, 1, 8, Tile::Path);
        rect(m, 18, 15, 6, 1, Tile::Path);
        rect(m, 27, 16, 5, 1, Tile::Path);
        rect(m, 31, 7, 1, 10, Tile::Path);
        rect(m, 31, 7, 4, 1, Tile::Path);
        rect(m, 4, 29, 3, 1, Tile::Path);

        // Mountain wall, cave mouth, and the sealed ruin gate.
        rect(m, 30, 0, 10, 6, Tile::CaveWall);
        m.set(34, 6, Tile::CaveEntrance);
        m.set(36, 6, Tile::RuinEntrance);

        // Tall grass patches.
        rect(m, 8, 10, 5, 4, Tile::TallGrass);
        rect(m, 13, 24, 7, 3, Tile::TallGrass);
        rect(m, 19, 8, 4, 3, Tile::TallGrass);
        rect(m, 33, 18, 5, 4, Tile::TallGrass);
        rect(m, 7, 17, 4, 2, Tile::TallGrass);
        rect(m, 2, 24, 3, 3, Tile::TallGrass);

        // Forests.
        rect(m, 2, 2, 6, 4, Tile::Tree);
        rect(m, 12, 3, 5, 5, Tile::Tree);
        rect(m, 20, 3, 4, 3, Tile::Tree);
        rect(m, 27, 22, 7, 5, Tile::Tree);
        rect(m, 35, 10, 4, 4, Tile::Tree);
        rect(m, 2, 12, 4, 3, Tile::Tree);
        rect(m, 9, 25, 3, 3, Tile::Tree);

        m.set(22, 17, Tile::Sign);

        m.chests.push_back({3, 10, ItemId::Potion, 2});
        m.chests.push_back({36, 22, ItemId::Gold, 60});

        m.portals.push_back({34, 6, 1, 1, 2, 16, 23, 1});
        m.portals.push_back({4, 29, 3, 1, 0, 14, 18, 1});
        m.portals.push_back({36, 6, 1, 1, 3, 17, 24, 2, true});

        m.encounterSet = 1;
        maps_.push_back(std::move(m));
    }

    // ---------------- Shadow Cave ----------------
    {
        MapDef m = makeBase("cave", "Gua Bayangan", 34, 26, Tile::CaveWall);

        // Entry hall and corridors.
        rect(m, 12, 19, 10, 7, Tile::CaveFloor);
        rect(m, 16, 14, 2, 6, Tile::CaveFloor);
        rect(m, 5, 13, 8, 6, Tile::CaveFloor);
        rect(m, 12, 15, 11, 2, Tile::CaveFloor);
        rect(m, 22, 12, 10, 7, Tile::CaveFloor);
        // Upper corridor into the boss chamber and the right-hand room.
        rect(m, 19, 8, 4, 5, Tile::CaveFloor);
        rect(m, 11, 2, 14, 7, Tile::CaveFloor);

        m.set(16, 25, Tile::CaveEntrance);

        // Hazards and pillars.
        rect(m, 23, 13, 2, 2, Tile::Lava);
        rect(m, 27, 13, 2, 1, Tile::Lava);
        m.set(13, 3, Tile::Pillar);
        m.set(21, 3, Tile::Pillar);
        m.set(13, 6, Tile::Pillar);
        m.set(21, 6, Tile::Pillar);

        m.chests.push_back({7, 16, ItemId::Elixir, 2});
        m.chests.push_back({29, 15, ItemId::Gold, 80});
        m.chests.push_back({23, 5, ItemId::IronSword, 1});
        m.chests.push_back({12, 4, ItemId::ChainMail, 1});

        m.npcs.push_back({17, 4, NpcStyle::Warden, 7, 0});

        m.portals.push_back({16, 25, 1, 1, 1, 34, 7, 0});

        m.encounterSet = 2;
        maps_.push_back(std::move(m));
    }

    // ---------------- Dawn Ruins ----------------
    {
        MapDef m = makeBase("ruins", "Reruntuhan Fajar", 36, 28, Tile::CaveWall);

        // Entry hall, central nave, side chapels, and the guardian's arena.
        rect(m, 12, 21, 12, 6, Tile::CaveFloor);
        rect(m, 17, 14, 2, 7, Tile::CaveFloor);
        rect(m, 7, 8, 22, 7, Tile::CaveFloor);
        rect(m, 3, 10, 5, 5, Tile::CaveFloor);
        rect(m, 27, 10, 6, 5, Tile::CaveFloor);
        rect(m, 12, 2, 12, 7, Tile::CaveFloor);

        m.set(17, 26, Tile::RuinEntrance);

        // Cracked floor hazards.
        rect(m, 14, 11, 2, 1, Tile::Lava);
        rect(m, 20, 12, 3, 1, Tile::Lava);

        // Broken pillars frame the guardian arena.
        m.set(13, 3, Tile::Pillar);
        m.set(22, 3, Tile::Pillar);
        m.set(13, 6, Tile::Pillar);
        m.set(22, 6, Tile::Pillar);

        m.chests.push_back({4, 11, ItemId::DawnSword, 1});
        m.chests.push_back({30, 11, ItemId::DawnShield, 1});
        m.chests.push_back({25, 9, ItemId::Gold, 150});
        m.chests.push_back({9, 12, ItemId::Elixir, 2});
        m.chests.push_back({20, 22, ItemId::Potion, 2});

        m.npcs.push_back({17, 4, NpcStyle::Guardian, 9, 0});

        m.portals.push_back({17, 26, 1, 1, 1, 36, 7, 0});

        m.encounterSet = 3;
        maps_.push_back(std::move(m));
    }
}

}  // namespace rpg
