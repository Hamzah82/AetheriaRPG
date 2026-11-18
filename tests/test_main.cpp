// tests/test_main.cpp - self-contained logic, world and render tests
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "audio.h"
#include "audio_synth.h"
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"
#include "player.h"
#include "updater.h"
#include "world.h"

using namespace rpg;

namespace {

int gChecks = 0;
int gFailures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++gChecks;                                                               \
        if (!(cond)) {                                                           \
            ++gFailures;                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

template <typename A, typename B>
void checkEqImpl(const A& a, const B& b, const char* expr, const char* file, int line) {
    ++gChecks;
    if (!(a == b)) {
        ++gFailures;
        std::printf("FAIL %s:%d  %s  (%lld vs %lld)\n", file, line, expr,
                    static_cast<long long>(a), static_cast<long long>(b));
    }
}
#define CHECK_EQ(a, b) checkEqImpl((a), (b), #a " == " #b, __FILE__, __LINE__)

void checkNearImpl(float a, float b, float tol, const char* expr, const char* file, int line) {
    ++gChecks;
    if (!(std::fabs(a - b) <= tol)) {
        ++gFailures;
        std::printf("FAIL %s:%d  %s  (%.3f vs %.3f)\n", file, line, expr, a, b);
    }
}
#define CHECK_NEAR(a, b) checkNearImpl((a), (b), 0.01f, #a " ~= " #b, __FILE__, __LINE__)

std::string tmpPath(const char* name) {
    std::error_code ec;
    std::filesystem::create_directories("/tmp/opencode", ec);
    std::string p = std::string("/tmp/opencode/") + name;
    std::remove(p.c_str());
    std::remove((p + ".tmp").c_str());
    return p;
}

InputState keyInput(Key k) {
    InputState in;
    in.pressed[static_cast<int>(k)] = true;
    in.held[static_cast<int>(k)] = true;
    return in;
}

void tap(Game& g, Key k, int times = 1) {
    for (int i = 0; i < times; ++i) g.update(1.0f / 60.0f, keyInput(k));
}

void idle(Game& g, int frames) {
    InputState empty;
    for (int i = 0; i < frames; ++i) g.update(1.0f / 60.0f, empty);
}

void startExplore(Game& g) {
    g.newGame();
    g.debugAdvanceDialogue();
}

std::vector<char> reachableFrom(const World& world, int map, int sx, int sy) {
    const MapDef& m = world.def(map);
    std::vector<char> seen(static_cast<size_t>(m.w) * m.h, 0);
    if (!world.inBounds(map, sx, sy)) return seen;
    if (world.blocked(map, sx, sy) || world.npcAt(map, sx, sy) || world.chestAt(map, sx, sy))
        return seen;
    std::queue<std::pair<int, int>> q;
    seen[static_cast<size_t>(sy) * m.w + sx] = 1;
    q.push({sx, sy});
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i], ny = y + dy[i];
            if (!world.inBounds(map, nx, ny)) continue;
            size_t idx = static_cast<size_t>(ny) * m.w + nx;
            if (seen[idx]) continue;
            if (world.blocked(map, nx, ny) || world.npcAt(map, nx, ny) ||
                world.chestAt(map, nx, ny))
                continue;
            seen[idx] = 1;
            q.push({nx, ny});
        }
    }
    return seen;
}

bool seenAt(const std::vector<char>& seen, const MapDef& m, int x, int y) {
    if (x < 0 || y < 0 || x >= m.w || y >= m.h) return false;
    return seen[static_cast<size_t>(y) * m.w + x] != 0;
}

bool hasAdjacent(const std::vector<char>& seen, const MapDef& m, int x, int y) {
    return seenAt(seen, m, x + 1, y) || seenAt(seen, m, x - 1, y) ||
           seenAt(seen, m, x, y + 1) || seenAt(seen, m, x, y - 1);
}

// ---------------------------------------------------------------- core/rng

void testCore() {
    Rng a(42), b(42), c(43);
    for (int i = 0; i < 64; ++i) CHECK_EQ(a.next(), b.next());
    CHECK(c.next() != a.next());

    Rng r(7);
    for (int i = 0; i < 5000; ++i) {
        int v = r.range(3, 7);
        CHECK(v >= 3 && v <= 7);
    }
    for (int i = 0; i < 5000; ++i) {
        float u = r.unit();
        CHECK(u >= 0.0f && u < 1.0f);
    }
    CHECK(!Rng(9).chance(0.0f));
    CHECK(Rng(9).chance(1.0f));
    CHECK_EQ(clampInt(-4, 0, 10), 0);
    CHECK_EQ(clampInt(44, 0, 10), 10);
    CHECK_NEAR(clampf(1.5f, 0.0f, 1.0f), 1.0f);
    CHECK_NEAR(lerp(10.0f, 20.0f, 0.25f), 12.5f);
    CHECK_EQ(signf(-3.0f), -1);
    CHECK_EQ(signf(0.0f), 0);
    CHECK_EQ(signf(9.0f), 1);
    CHECK_EQ(alphaOf(0x80112233u), 128);
    CHECK_EQ(withAlpha(0xFF112233u, 10), 0x0A112233u);
    CHECK_EQ((mix(0x00000000u, 0xFFFFFFFFu, 0.5f) >> 16) & 255u, 127u);
    CHECK_EQ(hash2(4, 9), hash2(4, 9));
    CHECK(hash2(4, 9) != hash2(9, 4));
    CHECK_EQ(static_cast<int>(static_cast<uint8_t>(Tile::Void)), 20);
}

// ---------------------------------------------------------------- player

void testPlayer() {
    Player p;
    CHECK_EQ(p.level, 1);
    CHECK_EQ(p.maxHp, 42);
    CHECK_EQ(p.maxMp, 10);
    CHECK_EQ(p.hp, p.maxHp);
    CHECK_EQ(p.xpToNext(), 22);
    CHECK_EQ(p.atk(), 7);
    CHECK_EQ(p.def(), 4);

    CHECK_EQ(p.gainXp(21), 0);
    CHECK_EQ(p.level, 1);
    CHECK_EQ(p.xp, 21);
    p.hp = 5;
    CHECK_EQ(p.gainXp(1), 1);
    CHECK_EQ(p.level, 2);
    CHECK_EQ(p.xp, 0);
    CHECK_EQ(p.maxHp, 50);
    CHECK_EQ(p.hp, 50);

    Player q;
    CHECK_EQ(q.gainXp(100), 2);
    CHECK_EQ(q.level, 3);
    CHECK_EQ(q.xp, 38);
    CHECK_EQ(q.maxHp, 58);
    CHECK_EQ(q.atk(), 11);
    CHECK_EQ(q.def(), 8);

    q.weapon = 1;
    CHECK_EQ(q.atk(), 14);
    q.weapon = 2;
    CHECK_EQ(q.atk(), 18);
    q.armor = 1;
    CHECK_EQ(q.def(), 11);
    q.armor = 2;
    CHECK_EQ(q.def(), 15);

    Player h;
    CHECK_EQ(h.potions, 2);
    h.hp = h.maxHp;
    CHECK(!h.usePotion());
    CHECK_EQ(h.potions, 2);
    h.hp = 1;
    CHECK(h.usePotion());
    CHECK_EQ(h.potions, 1);
    CHECK_EQ(h.hp, 41);
    h.hp = h.maxHp;
    CHECK(!h.useElixir());
    h.elixirs = 1;
    h.mp = 0;
    CHECK(h.useElixir());
    CHECK_EQ(h.elixirs, 0);
    CHECK_EQ(h.mp, h.maxMp);

    Player dead;
    dead.hp = 0;
    CHECK(!dead.alive());
    dead.fullRestore();
    CHECK(dead.alive());
}

// ---------------------------------------------------------------- world

void testWorld() {
    World w;
    CHECK_EQ(w.count(), 3);
    CHECK_EQ(w.indexOf("village"), 0);
    CHECK_EQ(w.indexOf("overworld"), 1);
    CHECK_EQ(w.indexOf("cave"), 2);
    CHECK_EQ(w.indexOf("missing"), -1);

    CHECK_EQ(static_cast<int>(w.tile(0, 2, 13)), static_cast<int>(Tile::Water));
    CHECK(w.blocked(0, 2, 13));
    CHECK(w.blocked(0, 0, 0));
    CHECK(!w.blocked(0, 14, 18));
    CHECK(w.encounter(1, 8, 10));
    CHECK(!w.encounter(1, 14, 18));
    CHECK(w.encounter(2, 16, 15));
    CHECK(!w.encounter(0, 10, 10));
    CHECK(!w.inBounds(2, 33, 26));
    CHECK(w.blocked(2, 33, 26));

    const Portal* vp = w.portalAt(0, 14, 19);
    CHECK(vp != nullptr);
    if (vp) {
        CHECK_EQ(vp->targetMap, 1);
        CHECK_EQ(vp->spawnX, 5);
        CHECK_EQ(vp->spawnY, 27);
        const Portal* back = w.portalAt(1, 4, 29);
        CHECK(back != nullptr);
        if (back) {
            CHECK_EQ(back->targetMap, 0);
            CHECK_EQ(back->spawnX, 14);
            CHECK_EQ(back->spawnY, 18);
        }
    }
    const Portal* caveDoor = w.portalAt(1, 34, 6);
    CHECK(caveDoor != nullptr);
    if (caveDoor) {
        CHECK_EQ(caveDoor->targetMap, 2);
        CHECK_EQ(caveDoor->spawnX, 16);
        CHECK_EQ(caveDoor->spawnY, 23);
    }
    const Portal* caveExit = w.portalAt(2, 16, 25);
    CHECK(caveExit != nullptr);
    if (caveExit) {
        CHECK_EQ(caveExit->targetMap, 1);
        CHECK_EQ(caveExit->spawnX, 34);
        CHECK_EQ(caveExit->spawnY, 7);
    }

    const NpcSpawn* elder = w.npcAt(0, 12, 6);
    CHECK(elder != nullptr);
    if (elder) {
        CHECK_EQ(static_cast<int>(elder->style), static_cast<int>(NpcStyle::Elder));
        CHECK_EQ(elder->dialogue, 0);
    }
    const ChestSpawn* chest = w.chestAt(0, 26, 3);
    CHECK(chest != nullptr);
    if (chest) {
        CHECK_EQ(static_cast<int>(chest->item), static_cast<int>(ItemId::Gold));
        CHECK_EQ(chest->amount, 20);
    }
    CHECK(w.chestAt(2, 23, 5) != nullptr);

    const char* ids[3] = {"village", "overworld", "cave"};
    for (int i = 0; i < 3; ++i) {
        const MapDef& m = w.def(i);
        CHECK(m.id == ids[i]);
        CHECK(!m.name.empty());
        CHECK(m.w >= 20 && m.h >= 20);
        CHECK_EQ(static_cast<long long>(m.tiles.size()),
                 static_cast<long long>(m.w) * m.h);
        CHECK(i > 0 || m.encounterSet == 0);
        CHECK(i == 0 || m.encounterSet != 0);

        for (const Portal& p : m.portals) {
            CHECK(p.targetMap >= 0 && p.targetMap < w.count());
            if (p.targetMap < 0 || p.targetMap >= w.count()) continue;
            const MapDef& tm = w.def(p.targetMap);
            CHECK(p.spawnX >= 0 && p.spawnX < tm.w);
            CHECK(p.spawnY >= 0 && p.spawnY < tm.h);
            CHECK(!w.blocked(p.targetMap, p.spawnX, p.spawnY));
            CHECK(w.npcAt(p.targetMap, p.spawnX, p.spawnY) == nullptr);
            CHECK(w.chestAt(p.targetMap, p.spawnX, p.spawnY) == nullptr);
            for (int yy = 0; yy < p.h; ++yy)
                for (int xx = 0; xx < p.w; ++xx) CHECK(w.inBounds(i, p.x + xx, p.y + yy));
        }
        for (const NpcSpawn& n : m.npcs) {
            CHECK(!tileBlocks(m.at(n.x, n.y)));
            CHECK(w.portalAt(i, n.x, n.y) == nullptr);
            CHECK(w.chestAt(i, n.x, n.y) == nullptr);
            CHECK(n.x >= 1 && n.y >= 1 && n.x < m.w - 1 && n.y < m.h - 1);
        }
        for (const ChestSpawn& c : m.chests) {
            CHECK(!tileBlocks(m.at(c.x, c.y)));
            CHECK(w.portalAt(i, c.x, c.y) == nullptr);
            CHECK(w.npcAt(i, c.x, c.y) == nullptr);
            CHECK(c.x >= 1 && c.y >= 1 && c.x < m.w - 1 && c.y < m.h - 1);
        }
        for (size_t a = 0; a < m.npcs.size(); ++a)
            for (size_t b = a + 1; b < m.npcs.size(); ++b)
                CHECK(!(m.npcs[a].x == m.npcs[b].x && m.npcs[a].y == m.npcs[b].y));
        for (size_t a = 0; a < m.chests.size(); ++a)
            for (size_t b = a + 1; b < m.chests.size(); ++b)
                CHECK(!(m.chests[a].x == m.chests[b].x && m.chests[a].y == m.chests[b].y));
    }

    // Every portal, NPC and chest must be reachable from the map's arrival point.
    const int arrival[3][2] = {{14, 18}, {5, 27}, {16, 23}};
    for (int i = 0; i < 3; ++i) {
        const MapDef& m = w.def(i);
        int ax = arrival[i][0], ay = arrival[i][1];
        auto seen = reachableFrom(w, i, ax, ay);
        CHECK(seenAt(seen, m, ax, ay));
        for (const Portal& p : m.portals) {
            bool any = false;
            for (int yy = 0; yy < p.h; ++yy)
                for (int xx = 0; xx < p.w; ++xx)
                    if (seenAt(seen, m, p.x + xx, p.y + yy)) any = true;
            CHECK(any);
        }
        for (const NpcSpawn& n : m.npcs) CHECK(hasAdjacent(seen, m, n.x, n.y));
        for (const ChestSpawn& c : m.chests) CHECK(hasAdjacent(seen, m, c.x, c.y));
    }
}

// ---------------------------------------------------------------- save/load

void testSaveLoad() {
    std::string path = tmpPath("rpgcpp_save.dat");
    Game g(1234, path);
    CHECK(g.state() == GameState::Title);
    CHECK(!g.hasSave());
    std::string err;
    CHECK(!g.loadGame(&err));
    CHECK(!err.empty());
    CHECK(g.state() == GameState::Title);

    startExplore(g);
    CHECK(g.state() == GameState::Explore);
    CHECK(g.flags().introSeen);

    // Open the potion chest on the overworld.
    g.debugWarp(1, 4, 10);
    g.debugSetFacing(2);
    g.debugInteract();
    CHECK(g.debugChestOpened(1, 0));
    CHECK_EQ(g.player().potions, 4);
    g.debugAdvanceDialogue();

    Player& p = g.player();
    p.level = 4;
    p.recalcStats(true);
    p.xp = 7;
    p.gold = 321;
    p.hp = 20;
    p.mp = 3;
    p.weapon = 2;
    p.armor = 1;
    p.potions = 4;
    p.elixirs = 2;
    p.hasSunstone = true;
    Flags& f = g.flagsMutable();
    f.questAccepted = true;
    f.bossDefeated = true;
    f.sunstoneFound = true;
    f.gameCompleted = false;
    f.metElder = true;
    f.introSeen = true;

    CHECK(g.saveGame(&err));
    CHECK(err.empty());
    CHECK(g.hasSave());
    CHECK(!std::filesystem::exists(path + ".tmp"));

    Game h(999, path);
    CHECK(h.hasSave());
    CHECK(h.loadGame(&err));
    CHECK(err.empty());
    CHECK(h.state() == GameState::Explore);
    CHECK_EQ(h.currentMap(), 1);
    CHECK_NEAR(h.playerX(), 64.0f);
    CHECK_NEAR(h.playerY(), 160.0f);
    CHECK_EQ(h.player().level, 4);
    CHECK_EQ(h.player().xp, 7);
    CHECK_EQ(h.player().gold, 321);
    CHECK_EQ(h.player().hp, 20);
    CHECK_EQ(h.player().mp, 3);
    CHECK_EQ(h.player().maxHp, 66);
    CHECK_EQ(h.player().maxMp, 22);
    CHECK_EQ(h.player().weapon, 2);
    CHECK_EQ(h.player().armor, 1);
    CHECK_EQ(h.player().potions, 4);
    CHECK_EQ(h.player().elixirs, 2);
    CHECK(h.player().hasSunstone);
    CHECK(h.flags().questAccepted);
    CHECK(h.flags().bossDefeated);
    CHECK(h.flags().sunstoneFound);
    CHECK(!h.flags().gameCompleted);
    CHECK(h.flags().metElder);
    CHECK(h.flags().introSeen);
    CHECK(h.debugChestOpened(1, 0));
    CHECK(!h.debugChestOpened(1, 1));
    CHECK(!h.debugChestOpened(0, 0));

    // Loading through the title menu also works.
    Game menuLoad(1, path);
    tap(menuLoad, Key::Down);
    CHECK(menuLoad.state() == GameState::Title);
    tap(menuLoad, Key::Confirm);
    CHECK(menuLoad.state() == GameState::Explore);
    CHECK_EQ(menuLoad.player().gold, 321);

    // Overwrite and reload.
    h.player().gold = 55;
    CHECK(h.saveGame(&err));
    Game k(5, path);
    CHECK(k.loadGame(&err));
    CHECK_EQ(k.player().gold, 55);

    // Broken saves are rejected cleanly.
    {
        std::ofstream bad(path, std::ios::trunc);
        bad << "version 2\nmap 0\n";
    }
    CHECK(!k.loadGame(&err));
    CHECK(!err.empty());
    {
        std::ofstream bad(path, std::ios::trunc);
        bad << "version 1\nmap 99\n";
    }
    CHECK(!k.loadGame(&err));
    {
        std::ofstream bad(path, std::ios::trunc);
        bad << "version 1\nmap 0\nx nan\n";
    }
    CHECK(!k.loadGame(&err));
    {
        std::ofstream bad(path, std::ios::trunc);
        bad << "garbage\n";
    }
    CHECK(!k.loadGame(&err));
    std::remove(path.c_str());
    CHECK(!k.loadGame(&err));
    CHECK(!k.hasSave());
    std::remove((path + ".tmp").c_str());
}

// ---------------------------------------------------------------- chests/flavor

void testChestsAndFlavor() {
    std::string path = tmpPath("rpgcpp_chest.dat");
    Game g(77, path);
    startExplore(g);
    CHECK_EQ(g.player().gold, 30);

    g.debugWarp(0, 25, 3);
    g.debugSetFacing(3);
    g.debugInteract();
    CHECK(g.debugChestOpened(0, 0));
    CHECK_EQ(g.player().gold, 50);
    CHECK(g.dialogue().active);
    g.debugAdvanceDialogue();
    CHECK(g.state() == GameState::Explore);

    g.debugInteract();
    CHECK(g.dialogue().active);
    CHECK_EQ(g.player().gold, 50);
    g.debugAdvanceDialogue();
    CHECK_EQ(g.player().gold, 50);

    // Iron sword chest when the hero already owns the iron sword: sold for gold.
    g.player().weapon = 2;
    int gold = g.player().gold;
    g.debugWarp(2, 23, 6);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.debugChestOpened(2, 2));
    CHECK_EQ(g.player().weapon, 2);
    CHECK_EQ(g.player().gold, gold + 80);
    g.debugAdvanceDialogue();

    // Chain mail chest when already equipped: sold for gold.
    g.player().armor = 2;
    gold = g.player().gold;
    g.debugWarp(2, 12, 5);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.debugChestOpened(2, 3));
    CHECK_EQ(g.player().armor, 2);
    CHECK_EQ(g.player().gold, gold + 75);
    g.debugAdvanceDialogue();

    // Elixir chest.
    int elixirs = g.player().elixirs;
    g.debugWarp(2, 8, 16);
    g.debugSetFacing(2);
    g.debugInteract();
    CHECK(g.debugChestOpened(2, 0));
    CHECK_EQ(g.player().elixirs, elixirs + 2);
    g.debugAdvanceDialogue();

    // Gold chest.
    gold = g.player().gold;
    g.debugWarp(2, 28, 15);
    g.debugSetFacing(3);
    g.debugInteract();
    CHECK(g.debugChestOpened(2, 1));
    CHECK_EQ(g.player().gold, gold + 80);
    g.debugAdvanceDialogue();

    // Flavor interactions and blockers.
    g.debugWarp(0, 17, 13);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.dialogue().active);
    CHECK(g.dialogue().speaker == "Papan Kayu");
    g.debugAdvanceDialogue();

    g.debugWarp(0, 15, 5);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.dialogue().active);
    CHECK(g.dialogue().speaker == "Pintu");
    g.debugAdvanceDialogue();

    g.debugWarp(0, 16, 17);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.dialogue().active);
    CHECK(g.dialogue().speaker == "Penjaga Bram");
    g.debugAdvanceDialogue();

    g.debugWarp(0, 9, 15);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.dialogue().active);
    CHECK(g.dialogue().speaker == "Anak Desa");
    g.debugAdvanceDialogue();

    // Chests block walking.
    g.debugWarp(1, 4, 10);
    InputState left;
    left.held[static_cast<int>(Key::Left)] = true;
    for (int i = 0; i < 90; ++i) g.update(1.0f / 60.0f, left);
    CHECK(g.playerX() >= 60.0f);
    CHECK_EQ(g.currentMap(), 1);

    std::remove(path.c_str());
}

// ---------------------------------------------------------------- shop/inn

void testShopAndInn() {
    CHECK_EQ(SHOP_COUNT, 6);
    CHECK_EQ(SHOP[0].price, 12);
    CHECK_EQ(SHOP[1].price, 30);
    CHECK_EQ(SHOP[2].price, 60);
    CHECK_EQ(SHOP[3].price, 50);

    std::string path = tmpPath("rpgcpp_shop.dat");
    Game g(11, path);
    startExplore(g);
    CHECK_EQ(g.player().gold, 30);

    g.debugOpenShop();
    CHECK(g.state() == GameState::Shop);
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().potions, 3);
    CHECK_EQ(g.player().gold, 18);

    tap(g, Key::Down);
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().elixirs, 0);
    CHECK_EQ(g.player().gold, 18);

    g.player().gold = 100;
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().elixirs, 1);
    CHECK_EQ(g.player().gold, 70);

    tap(g, Key::Down);
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().weapon, 1);
    CHECK_EQ(g.player().gold, 10);
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().weapon, 1);
    CHECK_EQ(g.player().gold, 10);

    tap(g, Key::Down);
    g.player().gold = 200;
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().armor, 1);
    CHECK_EQ(g.player().gold, 150);

    tap(g, Key::Down);
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().weapon, 2);
    CHECK_EQ(g.player().gold, 10);

    tap(g, Key::Down);
    g.player().gold = 200;
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().armor, 2);
    CHECK_EQ(g.player().gold, 70);
    tap(g, Key::Confirm);
    CHECK_EQ(g.player().armor, 2);
    CHECK_EQ(g.player().gold, 70);

    tap(g, Key::Cancel);
    CHECK(g.state() == GameState::Explore);

    // Inn: paid rest.
    g.player().gold = 100;
    g.player().hp = 5;
    g.player().mp = 0;
    g.debugWarp(0, 24, 13);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.state() == GameState::Dialogue);
    CHECK_EQ(g.player().gold, 90);
    CHECK_EQ(g.player().hp, g.player().maxHp);
    CHECK_EQ(g.player().mp, g.player().maxMp);
    g.debugAdvanceDialogue();

    // Inn: already rested, no charge.
    g.debugInteract();
    CHECK_EQ(g.player().gold, 90);
    g.debugAdvanceDialogue();

    // Inn: not enough gold.
    g.player().hp = 1;
    g.player().gold = 5;
    g.debugInteract();
    CHECK_EQ(g.player().gold, 5);
    CHECK_EQ(g.player().hp, 1);
    g.debugAdvanceDialogue();

    std::remove(path.c_str());
}

// ---------------------------------------------------------------- battle

void battleKey(Battle& b, Player& p, Key k, int times = 1) {
    for (int i = 0; i < times; ++i) b.update(1.0f / 60.0f, keyInput(k), p);
}

void testBattle() {
    const EnemyInfo& slime = enemyInfo(EnemyType::Slime);
    CHECK_EQ(slime.hp, 18);
    CHECK_EQ(slime.gold, 5);
    CHECK_EQ(enemyInfo(EnemyType::Warden).hp, 180);
    CHECK_EQ(static_cast<int>(enemyInfo(EnemyType::Warden).type),
             static_cast<int>(EnemyType::Warden));
    CHECK_EQ(static_cast<int>(enemyInfo(EnemyType::Bat).type),
             static_cast<int>(EnemyType::Bat));

    // One-hit victory, rewards and end screen.
    {
        Player p;
        Battle b;
        b.begin(p, EnemyType::Slime, false, 1234);
        CHECK_EQ(b.enemyHp(), 18);
        CHECK_EQ(b.enemyMaxHp(), 18);
        CHECK(b.isBoss() == false);
        CHECK(!b.done());
        b.forceEnemyHp(1);
        b.forceCommandPhase();
        CHECK(b.phase() == Battle::Phase::Command);
        battleKey(b, p, Key::Confirm);
        CHECK_EQ(b.enemyHp(), 0);
        CHECK(b.phase() == Battle::Phase::Message);
        for (int i = 0; i < 12; ++i) battleKey(b, p, Key::Confirm);
        CHECK(b.phase() == Battle::Phase::Victory);
        CHECK_EQ(p.gold, 30 + slime.gold);
        CHECK_EQ(p.xp, slime.xp);
        battleKey(b, p, Key::Confirm);
        CHECK(b.done());
        CHECK(b.victory());
        CHECK(!b.fled());
    }

    // Fire is super effective against wolves.
    {
        Player p;
        Battle b;
        b.begin(p, EnemyType::Wolf, false, 99);
        CHECK_EQ(p.mp, 10);
        b.forceCommandPhase();
        battleKey(b, p, Key::Down);
        battleKey(b, p, Key::Confirm);
        CHECK(b.phase() == Battle::Phase::SkillMenu);
        battleKey(b, p, Key::Confirm);
        CHECK_EQ(p.mp, 5);
        int drop = b.enemyMaxHp() - b.enemyHp();
        CHECK(drop >= 16 && drop <= 25);
        CHECK(b.phase() == Battle::Phase::Message);
    }

    // Healing skill.
    {
        Player p;
        p.hp = 10;
        Battle b;
        b.begin(p, EnemyType::Slime, false, 5);
        b.forceCommandPhase();
        battleKey(b, p, Key::Down);
        battleKey(b, p, Key::Confirm);
        battleKey(b, p, Key::Down, 2);
        battleKey(b, p, Key::Confirm);
        CHECK_EQ(p.mp, 5);
        CHECK_EQ(p.hp, 35);
        CHECK(b.phase() == Battle::Phase::Message);
    }

    // Item menus: potion works, missing elixir does not.
    {
        Player p;
        p.hp = 10;
        p.potions = 1;
        Battle b;
        b.begin(p, EnemyType::Slime, false, 6);
        b.forceCommandPhase();
        battleKey(b, p, Key::Down, 2);
        battleKey(b, p, Key::Confirm);
        CHECK(b.phase() == Battle::Phase::ItemMenu);
        battleKey(b, p, Key::Confirm);
        CHECK_EQ(p.hp, 42);
        CHECK_EQ(p.potions, 0);
        CHECK(b.phase() == Battle::Phase::Message);
    }
    {
        Player p;
        p.elixirs = 0;
        Battle b;
        b.begin(p, EnemyType::Slime, false, 6);
        b.forceCommandPhase();
        battleKey(b, p, Key::Down, 2);
        battleKey(b, p, Key::Confirm);
        battleKey(b, p, Key::Down);
        battleKey(b, p, Key::Confirm);
        CHECK_EQ(p.elixirs, 0);
        CHECK_EQ(p.mp, p.maxMp);
        CHECK(b.phase() == Battle::Phase::Message);
        battleKey(b, p, Key::Confirm, 2);
        CHECK(b.phase() == Battle::Phase::Command);
    }

    // Not enough MP.
    {
        Player p;
        p.mp = 0;
        Battle b;
        b.begin(p, EnemyType::Slime, false, 8);
        b.forceCommandPhase();
        battleKey(b, p, Key::Down);
        battleKey(b, p, Key::Confirm);
        battleKey(b, p, Key::Confirm);
        CHECK_EQ(p.mp, 0);
        CHECK_EQ(b.enemyHp(), b.enemyMaxHp());
        CHECK(b.phase() == Battle::Phase::Message);
        battleKey(b, p, Key::Confirm, 2);
        CHECK(b.phase() == Battle::Phase::Command);
    }

    // Menu cancel returns to the command list.
    {
        Player p;
        Battle b;
        b.begin(p, EnemyType::Slime, false, 9);
        b.forceCommandPhase();
        battleKey(b, p, Key::Down, 2);
        battleKey(b, p, Key::Confirm);
        CHECK(b.phase() == Battle::Phase::ItemMenu);
        battleKey(b, p, Key::Cancel);
        CHECK(b.phase() == Battle::Phase::Command);
        battleKey(b, p, Key::Up);
        battleKey(b, p, Key::Confirm);
        CHECK(b.phase() == Battle::Phase::SkillMenu);
        battleKey(b, p, Key::Cancel);
        CHECK(b.phase() == Battle::Phase::Command);
    }

    // The boss cannot be fled from.
    {
        Player p;
        p.maxHp = 9999;
        p.hp = 9999;
        Battle b;
        b.begin(p, EnemyType::Warden, true, 7);
        CHECK(b.isBoss());
        b.forceCommandPhase();
        battleKey(b, p, Key::Down, 3);
        battleKey(b, p, Key::Confirm);
        CHECK(b.phase() == Battle::Phase::Message);
        CHECK(!b.fled());
        battleKey(b, p, Key::Confirm, 4);
        CHECK(b.phase() == Battle::Phase::Command);
        CHECK(!b.fled());
        CHECK_EQ(b.enemyHp(), b.enemyMaxHp());
        CHECK(p.hp > 0);
        CHECK(p.hp < 9999);
    }

    // Normal enemies can both escape and fail to escape.
    {
        int fledCount = 0, stayedCount = 0;
        for (uint64_t seed = 1; seed <= 40; ++seed) {
            Player p;
            p.maxHp = 9999;
            p.hp = 9999;
            Battle b;
            b.begin(p, EnemyType::Slime, false, seed);
            b.forceCommandPhase();
            battleKey(b, p, Key::Down, 3);
            battleKey(b, p, Key::Confirm);
            battleKey(b, p, Key::Confirm, 3);
            if (b.phase() == Battle::Phase::Fled)
                ++fledCount;
            else
                ++stayedCount;
        }
        CHECK(fledCount > 0);
        CHECK(stayedCount > 0);
    }

    // Defeat when the enemy turn lands the killing blow.
    {
        Player p;
        p.hp = 1;
        p.weapon = 0;
        p.baseAtk = 0;
        Battle b;
        b.begin(p, EnemyType::Warden, true, 11);
        b.forceCommandPhase();
        battleKey(b, p, Key::Confirm);
        for (int i = 0; i < 30; ++i) battleKey(b, p, Key::Confirm);
        CHECK(b.phase() == Battle::Phase::Defeat);
        CHECK(b.done());
        CHECK(!b.victory());
        CHECK(!b.fled());
        CHECK_EQ(p.hp, 0);
    }
}

// ---------------------------------------------------------------- quest flow

void testQuestFlow() {
    std::string path = tmpPath("rpgcpp_quest.dat");
    Game g(55, path);
    startExplore(g);

    g.debugWarp(0, 12, 7);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.state() == GameState::Dialogue);
    CHECK(g.flags().questAccepted);
    CHECK(g.flags().metElder);
    CHECK(g.dialogue().speaker == "Tetua Ardan");
    g.debugAdvanceDialogue();
    CHECK(g.state() == GameState::Explore);
    CHECK(!g.flags().gameCompleted);

    // Talking again before the relic does not finish the quest.
    g.debugInteract();
    CHECK(g.state() == GameState::Dialogue);
    CHECK(!g.flags().gameCompleted);
    g.debugAdvanceDialogue();

    // The Warden starts a transition into a boss battle.
    g.debugWarp(2, 17, 5);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.state() == GameState::Dialogue);
    CHECK(g.dialogue().speaker == "Warden Bayangan");
    g.debugAdvanceDialogue();
    idle(g, 40);
    CHECK(g.state() == GameState::Battle);
    CHECK(g.battle().isBoss());

    g.battle().forceEnemyHp(1);
    g.battle().forceCommandPhase();
    for (int i = 0; i < 200 && g.state() == GameState::Battle; ++i) tap(g, Key::Confirm);
    CHECK(g.flags().bossDefeated);
    CHECK(g.flags().sunstoneFound);
    CHECK(g.player().hasSunstone);
    CHECK(g.state() == GameState::Dialogue);
    g.debugAdvanceDialogue();
    CHECK(g.state() == GameState::Explore);

    // The warden disappears once defeated.
    g.debugWarp(2, 17, 5);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.state() == GameState::Explore);

    // Bring the relic to the elder and reach the ending.
    g.debugWarp(0, 12, 7);
    g.debugSetFacing(1);
    g.debugInteract();
    CHECK(g.flags().gameCompleted);
    CHECK(g.state() == GameState::Dialogue);
    g.debugAdvanceDialogue();
    CHECK(g.state() == GameState::Ending);

    idle(g, 20);
    tap(g, Key::Confirm);
    CHECK(g.state() == GameState::Ending);
    idle(g, 400);
    tap(g, Key::Confirm);
    CHECK(g.state() == GameState::Title);

    std::remove(path.c_str());
}

// ---------------------------------------------------------------- portals/collision

void walkUntilMap(Game& g, Key key, int startMap, int expectedMap) {
    for (int i = 0; i < 500 && g.currentMap() == startMap; ++i) {
        InputState in;
        in.held[static_cast<int>(key)] = true;
        g.update(1.0f / 60.0f, in);
    }
    for (int i = 0; i < 120 && g.transitionActive(); ++i) {
        InputState empty;
        g.update(1.0f / 60.0f, empty);
    }
    CHECK_EQ(g.currentMap(), expectedMap);
}

void testPortalsAndCollision() {
    std::string path = tmpPath("rpgcpp_portals.dat");
    Game g(9, path);
    startExplore(g);
    CHECK_EQ(g.currentMap(), 0);

    g.debugWarp(0, 14, 18);
    walkUntilMap(g, Key::Down, 0, 1);
    CHECK_NEAR(g.playerX(), 5 * TILE);
    CHECK_NEAR(g.playerY(), 27 * TILE);

    g.debugWarp(1, 5, 27);
    walkUntilMap(g, Key::Down, 1, 0);
    CHECK_NEAR(g.playerX(), 14 * TILE);
    CHECK_NEAR(g.playerY(), 18 * TILE);

    g.debugWarp(1, 34, 7);
    walkUntilMap(g, Key::Up, 1, 2);
    CHECK_NEAR(g.playerX(), 16 * TILE);
    CHECK_NEAR(g.playerY(), 23 * TILE);

    g.debugWarp(2, 16, 24);
    walkUntilMap(g, Key::Down, 2, 1);
    CHECK_NEAR(g.playerX(), 34 * TILE);
    CHECK_NEAR(g.playerY(), 7 * TILE);

    // The pond blocks movement: the player must stay out of the water tile.
    g.debugWarp(0, 6, 13);
    CHECK_EQ(static_cast<int>(g.world().tile(0, 5, 13)), static_cast<int>(Tile::Water));
    CHECK_EQ(static_cast<int>(g.world().tile(0, 6, 13)), static_cast<int>(Tile::Sand));
    InputState left;
    left.held[static_cast<int>(Key::Left)] = true;
    for (int i = 0; i < 120; ++i) g.update(1.0f / 60.0f, left);
    CHECK(g.playerX() >= 92.0f);
    CHECK_EQ(g.currentMap(), 0);

    // NPCs also block movement.
    g.debugWarp(0, 12, 7);
    InputState up;
    up.held[static_cast<int>(Key::Up)] = true;
    for (int i = 0; i < 60; ++i) g.update(1.0f / 60.0f, up);
    CHECK(g.playerY() >= 102.0f);

    std::remove(path.c_str());
}

// ---------------------------------------------------------------- menus/gameover

void testMenusAndGameOver() {
    std::string path = tmpPath("rpgcpp_menus.dat");
    Game g(21, path);
    CHECK(g.state() == GameState::Title);

    // Four title rows wrap in both directions.
    CHECK_EQ(g.debugTitleIndex(), 0);
    tap(g, Key::Up);
    CHECK_EQ(g.debugTitleIndex(), 3);
    tap(g, Key::Down);
    CHECK_EQ(g.debugTitleIndex(), 0);

    // Loading without a save shows a message and stays on the title.
    tap(g, Key::Down);
    CHECK_EQ(g.debugTitleIndex(), 1);
    tap(g, Key::Confirm);
    CHECK(g.state() == GameState::Title);
    CHECK(!g.quitRequested());

    // Without an Updater the update row stays safe and explains why.
    tap(g, Key::Down);
    CHECK_EQ(g.debugTitleIndex(), 2);
    tap(g, Key::Confirm);
    CHECK(g.state() == GameState::Title);
    CHECK(!g.quitRequested());
    CHECK(!g.restartRequested());

    tap(g, Key::Down);
    CHECK_EQ(g.debugTitleIndex(), 3);
    tap(g, Key::Confirm);
    CHECK(g.quitRequested());

    Game h(22, path);
    startExplore(h);
    h.debugOpenMenu();
    CHECK(h.state() == GameState::Menu);
    tap(h, Key::Confirm);  // items page
    CHECK(h.state() == GameState::Menu);
    h.player().hp = 1;
    tap(h, Key::Confirm);  // use potion
    CHECK_EQ(h.player().hp, 41);
    tap(h, Key::Cancel);   // back to root
    tap(h, Key::Down, 2);  // save row
    h.player().gold = 88;
    tap(h, Key::Confirm);  // save
    CHECK(h.hasSave());
    tap(h, Key::Cancel);   // close menu
    CHECK(h.state() == GameState::Explore);

    // Continue after defeat: half HP/MP, lose a fifth of the gold.
    h.player().gold = 100;
    h.debugGameOver();
    CHECK(h.state() == GameState::GameOver);
    tap(h, Key::Confirm);
    CHECK(h.state() == GameState::Explore);
    CHECK_EQ(h.player().gold, 80);
    CHECK_EQ(h.currentMap(), 0);
    CHECK_EQ(h.player().hp, h.player().maxHp / 2);
    CHECK_EQ(h.player().mp, h.player().maxMp / 2);

    h.debugGameOver();
    tap(h, Key::Down);
    CHECK(h.state() == GameState::GameOver);
    tap(h, Key::Confirm);
    CHECK(h.state() == GameState::Title);

    // Save file written by the menu loads correctly.
    Game loaded(23, path);
    CHECK(loaded.hasSave());
    CHECK(loaded.loadGame());
    CHECK_EQ(loaded.player().gold, 88);

    std::remove(path.c_str());
}

// ---------------------------------------------------------------- updater

void testUpdater() {
    namespace fs = std::filesystem;
    std::error_code ec;
    std::string root = tmpPath("rpgcpp_updater_repo");
    fs::remove_all(root, ec);
    fs::create_directories(root + "/src", ec);
    {
        std::ofstream make(root + "/Makefile");
        make << "all:\n\t@true\n";
    }
    {
        std::ofstream src(root + "/src/main.cpp");
        src << "int main() { return 0; }\n";
    }

    CHECK(!executableDirectory().empty());
    CHECK(!isSourceCheckout(""));
    CHECK(!isSourceCheckout(root));
    fs::create_directories(root + "/.git", ec);
    CHECK(isSourceCheckout(root));

    // AETHERIA_REPO points the in-game updater at an explicit checkout.
    setenv("AETHERIA_REPO", root.c_str(), 1);
    std::string found = findSourceCheckout();
    CHECK(!found.empty());
    CHECK(fs::equivalent(found, root, ec));
    unsetenv("AETHERIA_REPO");

    Updater updater;
    CHECK(!updater.running());
    CHECK(!updater.finished());
    CHECK(!updater.success());
    CHECK(updater.restartPath().empty());

    std::string err;
    CHECK(!updater.start(root + "/missing", "", &err));
    CHECK(!err.empty());
    CHECK(!updater.running());

    // A directory with a fake .git fails at `git status` without ever
    // touching the network or building anything.
    err.clear();
    CHECK(updater.start(root, "", &err));
    CHECK(err.empty());
    for (int i = 0; i < 600 && !updater.finished(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(updater.finished());
    CHECK(!updater.running());
    CHECK(!updater.success());
    CHECK(!updater.errorText().empty());
    CHECK(updater.restartPath().empty());

    // The title row starts the worker, stays on the title screen, and does
    // not request a restart when the update fails.
    setenv("AETHERIA_REPO", root.c_str(), 1);
    {
        Game game(77, tmpPath("rpgcpp_updater_game.dat"));
        Updater menuUpdater;
        game.setUpdater(&menuUpdater);
        tap(game, Key::Down);
        tap(game, Key::Down);
        CHECK_EQ(game.debugTitleIndex(), 2);
        tap(game, Key::Confirm);
        CHECK(game.state() == GameState::Title);
        CHECK(!game.restartRequested());
        for (int i = 0; i < 600 && !menuUpdater.finished(); ++i) {
            InputState idle;
            game.update(1.0f / 60.0f, idle);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        idle(game, 2);
        CHECK(menuUpdater.finished());
        CHECK(!menuUpdater.success());
        CHECK(!game.restartRequested());
        CHECK_EQ(game.debugTitleIndex(), 2);
    }
    unsetenv("AETHERIA_REPO");

    fs::remove_all(root, ec);
}

// ---------------------------------------------------------------- audio

void testAudio() {
    // Every track is deterministic, non-silent, and loopable.
    for (int i = 1; i < static_cast<int>(MusicId::Count); ++i) {
        MusicId id = static_cast<MusicId>(i);
        std::vector<int16_t> a = buildMusic(id);
        std::vector<int16_t> b = buildMusic(id);
        CHECK(!a.empty());
        CHECK(a == b);
        float seconds = static_cast<float>(a.size()) / AUDIO_RATE;
        CHECK(seconds > 1.0f);
        CHECK(seconds < 15.0f);
        int peak = 0;
        for (int16_t v : a) peak = std::max(peak, std::abs(static_cast<int>(v)));
        CHECK(peak > 4000);
        CHECK(peak <= 32767);
        // A clean loop point keeps the generated music free of clicks.
        CHECK(std::abs(static_cast<int>(a.front())) <= 8);
        CHECK(std::abs(static_cast<int>(a.back())) <= 8);
        CHECK(std::strcmp(musicName(id), "?") != 0);
    }
    CHECK(std::strcmp(musicName(MusicId::None), "hening") == 0);
    CHECK(buildMusic(MusicId::None).empty());

    // Sound effects are short, audible one-shots.
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        SfxId id = static_cast<SfxId>(i);
        std::vector<int16_t> a = buildSfx(id);
        CHECK(!a.empty());
        CHECK(a == buildSfx(id));
        float seconds = static_cast<float>(a.size()) / AUDIO_RATE;
        CHECK(seconds >= 0.05f);
        CHECK(seconds <= 2.0f);
        int peak = 0;
        for (int16_t v : a) peak = std::max(peak, std::abs(static_cast<int>(v)));
        CHECK(peak > 500);
        CHECK(peak <= 32767);
        CHECK(std::abs(static_cast<int>(a.front())) <= 64);
        CHECK(std::strcmp(sfxName(id), "?") != 0);
    }

    // Audio without a device stays completely inert but keeps its settings.
    Audio audio;
    CHECK(!audio.available());
    CHECK_NEAR(audio.musicVolume(), 0.62f);
    CHECK_NEAR(audio.sfxVolume(), 0.78f);
    CHECK(!audio.muted());
    audio.playMusic(MusicId::Battle);
    audio.playSfx(SfxId::Attack);
    audio.stopMusic();
    CHECK(!audio.available());
    audio.setMusicVolume(2.5f);
    audio.setSfxVolume(-1.0f);
    CHECK_NEAR(audio.musicVolume(), 1.0f);
    CHECK_NEAR(audio.sfxVolume(), 0.0f);
    audio.toggleMute();
    CHECK(audio.muted());
    audio.toggleMute();
    CHECK(!audio.muted());
    audio.shutdown();
    CHECK(!audio.available());

    // Menu navigation: root has five rows and row 3 opens the audio page.
    std::string path = tmpPath("rpgcpp_audio.dat");
    Game g(41, path);
    startExplore(g);
    g.debugOpenMenu();
    CHECK_EQ(g.debugMenuPage(), 0);
    tap(g, Key::Down, 4);
    CHECK_EQ(g.debugMenuIndex(), 4);
    tap(g, Key::Down);
    CHECK_EQ(g.debugMenuIndex(), 0);
    tap(g, Key::Up);
    CHECK_EQ(g.debugMenuIndex(), 4);
    tap(g, Key::Up);
    tap(g, Key::Confirm);
    CHECK_EQ(g.debugMenuPage(), 3);
    CHECK_EQ(g.debugMenuIndex(), 0);

    // Without an Audio object the page reports unavailability and backs out.
    tap(g, Key::Confirm);
    CHECK_EQ(g.debugMenuPage(), 0);
    CHECK_EQ(g.debugMenuIndex(), 3);
    tap(g, Key::Confirm);
    CHECK_EQ(g.debugMenuPage(), 3);
    tap(g, Key::Cancel);
    CHECK_EQ(g.debugMenuPage(), 0);
    tap(g, Key::Cancel);
    CHECK(g.state() == GameState::Explore);

    // With an Audio object the page edits and clamps live settings.
    Audio settings;
    g.setAudio(&settings);
    g.debugOpenMenu();
    tap(g, Key::Up);  // root row 0 -> 4
    tap(g, Key::Up);  // root row 4 -> 3
    CHECK_EQ(g.debugMenuIndex(), 3);
    tap(g, Key::Confirm);
    CHECK_EQ(g.debugMenuPage(), 3);
    CHECK_EQ(g.debugMenuIndex(), 0);
    tap(g, Key::Right, 5);
    CHECK_NEAR(settings.musicVolume(), 1.0f);
    tap(g, Key::Left, 20);
    CHECK_NEAR(settings.musicVolume(), 0.0f);
    tap(g, Key::Down);
    CHECK_EQ(g.debugMenuIndex(), 1);
    tap(g, Key::Right, 3);
    CHECK_NEAR(settings.sfxVolume(), 1.0f);
    tap(g, Key::Down);
    CHECK_EQ(g.debugMenuIndex(), 2);
    tap(g, Key::Confirm);
    CHECK(settings.muted());
    tap(g, Key::Confirm);
    CHECK(!settings.muted());
    tap(g, Key::Down);
    CHECK_EQ(g.debugMenuIndex(), 3);
    tap(g, Key::Confirm);
    CHECK_EQ(g.debugMenuPage(), 0);
    CHECK_EQ(g.debugMenuIndex(), 3);
    tap(g, Key::Cancel);
    CHECK(g.state() == GameState::Explore);
    g.setAudio(nullptr);
    CHECK(g.state() == GameState::Explore);

    std::remove(path.c_str());
}

// ---------------------------------------------------------------- rendering

void testRender() {
    Framebuffer fb;
    fb.resize(VIEW_W, VIEW_H);
    CHECK_EQ(fb.px.size(), static_cast<size_t>(VIEW_W) * VIEW_H);
    fb.clear(pal::black);
    CHECK_EQ(fb.px[0], pal::black);
    fb.fillRect(1, 1, 2, 2, pal::white);
    CHECK_EQ(fb.px[1 * VIEW_W + 1], pal::white);
    CHECK_EQ(fb.px[0], pal::black);
    fb.hline(0, 0, 10, pal::gold);
    CHECK_EQ(fb.px[0], pal::gold);
    fb.text(4, 4, "ABC", pal::white);
    int lit = 0;
    for (uint32_t px : fb.px)
        if (px != pal::black) ++lit;
    CHECK(lit > 20);

    auto lines = wrapText("satu dua tiga empat lima enam tujuh delapan", 60, 1);
    CHECK(lines.size() > 1);
    for (const auto& line : lines) CHECK(fb.textWidth(line, 1) <= 60);
    std::string joined;
    for (const auto& line : lines) joined += line + " ";
    CHECK(joined.find("delapan") != std::string::npos);
    CHECK(fb.textWidth("A", 1) > 0);
    CHECK_EQ(fb.textHeight(1), 7);

    std::string pngPath = tmpPath("rpgcpp_test.png");
    std::vector<uint8_t> png = fb.toPNG();
    CHECK(png.size() > 32);
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (png.size() >= 8) CHECK(std::memcmp(png.data(), sig, 8) == 0);
    CHECK(fb.writePNG(pngPath));
    {
        std::ifstream f(pngPath, std::ios::binary);
        char magic[8] = {};
        f.read(magic, 8);
        CHECK_EQ(f.gcount(), 8);
        CHECK(std::memcmp(magic, sig, 8) == 0);
    }
    std::remove(pngPath.c_str());

    // Two identically seeded games must render identical frames.
    std::string pa = tmpPath("rpgcpp_render_a.dat");
    std::string pb = tmpPath("rpgcpp_render_b.dat");
    Game a(20261118, pa);
    Game b(20261118, pb);
    startExplore(a);
    startExplore(b);
    a.debugWarp(0, 14, 18);
    b.debugWarp(0, 14, 18);
    idle(a, 10);
    idle(b, 10);
    Framebuffer fa, fb2;
    fa.resize(VIEW_W, VIEW_H);
    fb2.resize(VIEW_W, VIEW_H);
    a.render(fa);
    b.render(fb2);
    CHECK(fa.px == fb2.px);
    b.render(fb2);
    CHECK(fa.px == fb2.px);

    // All game states can render without crashing and produce non-trivial output.
    auto distinctColors = [](const Framebuffer& f) {
        std::set<uint32_t> colors;
        for (uint32_t px : f.px) colors.insert(px);
        return colors.size();
    };
    Framebuffer shot;
    shot.resize(VIEW_W, VIEW_H);

    std::string statePath = tmpPath("rpgcpp_render_states.dat");
    Game g(31, statePath);
    auto renderState = [&](const char* name) {
        shot.clear(pal::black);
        g.render(shot);
        CHECK(distinctColors(shot) > 8);
        (void)name;
    };
    renderState("title");
    startExplore(g);
    renderState("explore");
    g.debugWarp(0, 12, 7);
    g.debugSetFacing(1);
    g.debugInteract();
    idle(g, 10);
    renderState("dialogue");
    g.debugAdvanceDialogue();
    g.debugOpenMenu();
    renderState("menu");
    tap(g, Key::Confirm);
    renderState("menu-items");
    tap(g, Key::Cancel);
    tap(g, Key::Down);
    tap(g, Key::Confirm);
    renderState("menu-status");
    tap(g, Key::Cancel);
    tap(g, Key::Down, 2);
    tap(g, Key::Confirm);
    renderState("menu-audio");
    tap(g, Key::Cancel);
    tap(g, Key::Cancel);
    g.debugOpenShop();
    renderState("shop");
    g.debugGameOver();
    renderState("gameover");
    g.debugStartEnding();
    idle(g, 60);
    renderState("ending");
    g.debugStartBattle(EnemyType::Wolf, false);
    idle(g, 30);
    renderState("battle");
    g.debugStartBattle(EnemyType::Warden, true);
    idle(g, 30);
    renderState("battle-boss");
    g.battle().forceCommandPhase();
    renderState("battle-command");

    std::remove(pa.c_str());
    std::remove(pb.c_str());
    std::remove(statePath.c_str());
}

void testDisplayPixels() {
    ColorChannel red = colorChannelFromMask(0x00FF0000UL);
    ColorChannel green = colorChannelFromMask(0x0000FF00UL);
    ColorChannel blue = colorChannelFromMask(0x000000FFUL);
    CHECK_EQ(red.shift, 16);
    CHECK_EQ(red.bits, 8);
    CHECK_EQ(green.shift, 8);
    CHECK_EQ(green.bits, 8);
    CHECK_EQ(blue.shift, 0);
    CHECK_EQ(blue.bits, 8);

    CHECK_EQ(packPixel(0xFFFFFFu, red, green, blue), 0xFFFFFFu);
    CHECK_EQ(packPixel(0xFF8000u, red, green, blue), 0xFF8000u);
    CHECK_EQ(packPixel(0x123456u, red, green, blue), 0x123456u);
    CHECK_EQ(packPixel(0x808080u, red, green, blue), 0x808080u);

    // BGR mask layout must move each component to its own bit range.
    ColorChannel bgrRed = colorChannelFromMask(0x000000FFUL);
    ColorChannel bgrGreen = colorChannelFromMask(0x0000FF00UL);
    ColorChannel bgrBlue = colorChannelFromMask(0x00FF0000UL);
    CHECK_EQ(bgrRed.shift, 0);
    CHECK_EQ(bgrBlue.shift, 16);
    CHECK_EQ(packPixel(0x123456u, bgrRed, bgrGreen, bgrBlue), 0x563412u);
    CHECK_EQ(packPixel(0x0000FFu, bgrRed, bgrGreen, bgrBlue), 0xFF0000u);
    CHECK_EQ(packPixel(0xFF0000u, bgrRed, bgrGreen, bgrBlue), 0x0000FFu);

    // 10-bit channels: widen the 8-bit source instead of overflowing.
    ColorChannel deepRed = colorChannelFromMask(0x3FF00000UL);
    ColorChannel deepGreen = colorChannelFromMask(0x000FFC00UL);
    ColorChannel deepBlue = colorChannelFromMask(0x000003FFUL);
    CHECK_EQ(deepRed.shift, 20);
    CHECK_EQ(deepRed.bits, 10);
    CHECK_EQ(deepBlue.bits, 10);
    CHECK_EQ(channelValue(255, deepBlue), 1020);
    CHECK_EQ(packPixel(0x0000FFu, deepRed, deepGreen, deepBlue), 0x000003FCu);
    CHECK_EQ(packPixel(0xFFFFFFu, deepRed, deepGreen, deepBlue), 0x3FCFF3FCu);

    // Unknown masks fall back without shifting or truncating.
    ColorChannel fallback = colorChannelFromMask(0);
    CHECK_EQ(fallback.shift, 0);
    CHECK_EQ(fallback.bits, 8);
}

}  // namespace

int main() {
    std::printf("rpgcpp test suite\n");
    testCore();
    testPlayer();
    testWorld();
    testSaveLoad();
    testChestsAndFlavor();
    testShopAndInn();
    testBattle();
    testQuestFlow();
    testPortalsAndCollision();
    testMenusAndGameOver();
    testUpdater();
    testAudio();
    testRender();
    testDisplayPixels();
    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
