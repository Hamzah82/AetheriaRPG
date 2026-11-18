// world.h - tile maps, portals, NPC and chest anchors
#pragma once
#include <string>
#include <vector>

#include "core.h"
#include "player.h"

namespace rpg {

enum class Tile : uint8_t {
    Grass,
    Flower,
    TallGrass,
    Path,
    Sand,
    Water,
    DeepWater,
    Tree,
    Rock,
    Wall,
    Roof,
    Door,
    Fence,
    Bridge,
    CaveFloor,
    CaveWall,
    Lava,
    CaveEntrance,
    RuinEntrance,
    Sign,
    Pillar,
    Void,
};

enum class EnemyType : uint8_t { Slime, Bat, Wolf, Skeleton, Warden, Guardian };

// NPC visual/role styles.
enum class NpcStyle : uint8_t {
    Elder,
    Shopkeeper,
    Innkeeper,
    Guard,
    Kid,
    Farmer,
    Villager,
    Warden,
    Guardian,
};

struct Portal {
    int x = 0, y = 0, w = 1, h = 1;
    int targetMap = 0;
    int spawnX = 0, spawnY = 0;
    int spawnDir = 0;
    // Optional seal: the portal refuses to open until the Sunstone is found.
    bool requiresSunstone = false;
};

struct NpcSpawn {
    int x = 0, y = 0;
    NpcStyle style = NpcStyle::Villager;
    int dialogue = 0;
    int facing = 0;
};

struct ChestSpawn {
    int x = 0, y = 0;
    ItemId item = ItemId::Gold;
    int amount = 0;
};

struct MapDef {
    std::string id;
    std::string name;
    int w = 0, h = 0;
    std::vector<Tile> tiles;
    std::vector<Portal> portals;
    std::vector<NpcSpawn> npcs;
    std::vector<ChestSpawn> chests;
    int encounterSet = 0;  // 0 none, 1 overworld, 2 cave, 3 ruins

    Tile at(int x, int y) const {
        if (x < 0 || y < 0 || x >= w || y >= h) return Tile::Void;
        return tiles[static_cast<size_t>(y) * w + x];
    }
    void set(int x, int y, Tile t) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        tiles[static_cast<size_t>(y) * w + x] = t;
    }
};

bool tileBlocks(Tile t);

class World {
public:
    World();

    int count() const { return static_cast<int>(maps_.size()); }
    const MapDef& def(int index) const;
    int indexOf(const std::string& id) const;

    Tile tile(int map, int tx, int ty) const;
    bool blocked(int map, int tx, int ty) const;
    bool encounter(int map, int tx, int ty) const;
    bool inBounds(int map, int tx, int ty) const;

    const Portal* portalAt(int map, int tx, int ty) const;
    const NpcSpawn* npcAt(int map, int tx, int ty) const;
    const ChestSpawn* chestAt(int map, int tx, int ty) const;

private:
    std::vector<MapDef> maps_;
    void build();
};

}  // namespace rpg
