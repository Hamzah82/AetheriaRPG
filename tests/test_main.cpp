// tests/test_main.cpp - deterministic regression suite for Pelita di Atas Segara Garam
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "art.h"
#include "audio.h"
#include "audio_synth.h"
#include "battle.h"
#include "game.h"
#include "gfx.h"
#include "party.h"
#include "player.h"
#include "savegame.h"
#include "settings.h"
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

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        ++gChecks;                                                               \
        auto va = (a);                                                           \
        auto vb = (b);                                                           \
        if (!(va == vb)) {                                                       \
            ++gFailures;                                                         \
            std::printf("FAIL %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b);   \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b) checkNearImpl((a), (b), 0.01f, #a " ~= " #b, __FILE__, __LINE__)

void checkNearImpl(float a, float b, float eps, const char* expr, const char* file, int line) {
    ++gChecks;
    if (std::fabs(a - b) > eps) {
        ++gFailures;
        std::printf("FAIL %s:%d  %s  (%.3f vs %.3f)\n", file, line, expr, a, b);
    }
}

void removeSaveFiles(const std::string& base) {
    for (int slot = -1; slot <= 2; ++slot) {
        std::string path = slotPath(base, slot);
        std::remove(path.c_str());
        std::remove((path + ".tmp").c_str());
    }
}

const std::string& tempDir() {
    static const std::string dir = [] {
        std::error_code ec;
        std::filesystem::path base = std::filesystem::temp_directory_path(ec);
        if (ec || base.empty()) base = ".";
        base /= "aetheria_tests_pelita";
        std::filesystem::create_directories(base, ec);
        return base.string();
    }();
    return dir;
}

std::string tmpPath(const char* name) {
    std::error_code ec;
    std::filesystem::create_directories(tempDir(), ec);
    std::string p = tempDir() + "/" + name;
    removeSaveFiles(p);
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

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Wait for a story-triggered battle, then finish it with a forced one-hit kill.
bool killBoss(Game& g) {
    for (int i = 0; i < 90 && g.state() != GameState::Battle; ++i) idle(g, 1);
    if (g.state() != GameState::Battle) return false;
    g.battle().forceEnemyHp(1);
    g.battle().forceCommandPhase();
    tap(g, Key::Confirm);
    for (int i = 0; i < 240 && g.state() == GameState::Battle; ++i) {
        idle(g, 1);
        if (i % 4 == 0) tap(g, Key::Confirm);
    }
    if (g.state() == GameState::Dialogue) g.debugAdvanceDialogue();
    return true;
}

void talkTo(Game& g, int map, int x, int y) {
    g.debugWarp(map, x, y + 1);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
}

void writeFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
}

}  // namespace

// ---------------------------------------------------------------- core/player/party

void testCore() {
    CHECK_EQ(clampInt(5, 0, 3), 3);
    CHECK_EQ(clampInt(-2, 0, 3), 0);
    CHECK_NEAR(lerp(0.0f, 10.0f, 0.5f), 5.0f);
    CHECK_NEAR(approach(0.0f, 10.0f, 3.0f), 3.0f);
    Rng a(123), b(123);
    CHECK_EQ(a.next(), b.next());
    CHECK_EQ(a.range(0, 9), b.range(0, 9));
    CHECK_EQ(wrapText("aaaa bbbb cccc", 8, 1).size() > 0, true);
}

void testPlayer() {
    Player p;
    p.level = 3;
    p.recalcStats(true);
    CHECK_EQ(p.maxHp, 36 + 3 * 9);
    CHECK_EQ(p.maxMp, 8 + 3 * 4);
    CHECK(p.atk() >= p.baseAtk);
    CHECK_EQ(static_cast<int>(ItemId::Count), 13);
    CHECK_EQ(std::strcmp(ITEMS[static_cast<int>(ItemId::Potion)].name, "Ramuan Pelita"), 0);
    CHECK_EQ(std::strcmp(WEAPONS[2].name, "Belati Penarah Jiwa"), 0);
    CHECK_EQ(std::strcmp(ARMORS[3].name, "Pelat Kapur"), 0);
    p.potions = 1;
    p.hp = 1;
    CHECK(p.usePotion());
    CHECK_EQ(p.potions, 0);
    CHECK(p.hp > 1);
    p.elixirs = 1;
    p.mp = 0;
    CHECK(p.useElixir());
    CHECK_EQ(p.elixirs, 0);
}

void testParty() {
    Companion sekar = makeCompanion(CompanionId::Sekar, 2);
    Companion bima = makeCompanion(CompanionId::Bima, 1);
    Companion laras = makeCompanion(CompanionId::Larasati, 1);
    Companion bagas = makeCompanion(CompanionId::Bagas, 1);
    CHECK_EQ(sekar.name, "Sekar");
    CHECK_EQ(bima.name, "Bima");
    CHECK_EQ(laras.name, "Larasati");
    CHECK_EQ(bagas.name, "Bagas");
    CHECK(sekar.maxHp > 0);
    CHECK(sekar.spd > bima.spd);
    sekar.hp = 10;
    sekar.healHp(500);
    CHECK_EQ(sekar.hp, sekar.maxHp);
    sekar.hp = 0;
    sekar.recalc();
    CHECK(!sekar.alive);
}

// ---------------------------------------------------------------- world

void testWorld() {
    World w;
    CHECK_EQ(w.count(), 9);
    const char* ids[9] = {"village", "harbor", "rimba", "sumur", "candi",
                          "biara", "taman", "kraton", "palung"};
    for (int i = 0; i < 9; ++i) {
        CHECK_EQ(w.indexOf(ids[i]), i);
        CHECK_EQ(w.def(i).name.empty(), false);
        CHECK(w.def(i).w > 10 && w.def(i).h > 10);
    }
    CHECK_EQ(w.def(w.indexOf("village")).encounterSet, 0);
    CHECK_EQ(w.def(w.indexOf("harbor")).encounterSet, 0);
    CHECK_EQ(w.def(w.indexOf("rimba")).encounterSet, 1);
    CHECK_EQ(w.def(w.indexOf("sumur")).encounterSet, 2);
    CHECK_EQ(w.def(w.indexOf("candi")).encounterSet, 3);
    CHECK_EQ(w.def(w.indexOf("palung")).encounterSet, 4);

    for (int m = 0; m < w.count(); ++m) {
        const MapDef& md = w.def(m);
        for (const auto& p : md.portals) {
            CHECK(p.targetMap >= 0 && p.targetMap < w.count());
            CHECK(!w.blocked(p.targetMap, p.spawnX, p.spawnY));
            CHECK(w.npcAt(p.targetMap, p.spawnX, p.spawnY) == nullptr);
            CHECK(w.chestAt(p.targetMap, p.spawnX, p.spawnY) == nullptr);
            CHECK(!w.blocked(m, p.x, p.y));
        }
        for (const auto& n : md.npcs) {
            CHECK(!w.blocked(m, n.x, n.y));
        }
        for (const auto& c : md.chests) {
            CHECK(!w.blocked(m, c.x, c.y));
        }
        for (const auto& e : md.echoes) {
            CHECK(e.id >= 0 && e.id < 12);
            CHECK(!w.blocked(m, e.x, e.y));
        }
        for (const auto& c : md.camps) {
            CHECK(!w.blocked(m, c.x, c.y));
        }
    }

    // Every map has at least one portal or is a dead-end safe area.
    for (int m = 0; m < w.count(); ++m) CHECK(w.def(m).portals.size() > 0);
    CHECK(w.def(w.indexOf("village")).echoes.size() >= 1);
    CHECK(w.def(w.indexOf("sumur")).echoes.size() >= 2);
    CHECK(w.def(w.indexOf("palung")).echoes.size() >= 1);
}

// ---------------------------------------------------------------- save format

void testSaveEncoding() {
    std::string plain = "version 3\nmap 1\nresonance 70\n";
    std::string encoded = encodeSaveText(plain);
    CHECK(encoded.rfind("AETHSAVE1", 0) == 0);
    CHECK(encoded.find("version") == std::string::npos);
    std::string decoded;
    CHECK(isEncodedSaveText(encoded));
    CHECK(!isEncodedSaveText(plain));
    CHECK(decodeSaveText(encoded, &decoded));
    CHECK_EQ(decoded, plain);
    CHECK(decodeSaveText(plain, &decoded));
    CHECK_EQ(decoded, plain);
    CHECK(!decodeSaveText("AETHSAVE1\n!!!", &decoded));

    std::string tampered = encoded;
    tampered[tampered.size() / 2] ^= 1;
    CHECK(!decodeSaveText(tampered, &decoded));
}

void testSaveGame() {
    std::string base = tmpPath("pelita_save.dat");
    Game g(4242, base);
    startExplore(g);
    g.player().level = 5;
    g.player().gold = 777;
    g.player().oil = 4;
    g.flagsMutable().resonance = 55;
    g.flagsMutable().act = 2;
    g.flagsMutable().memories = 0x05;
    g.debugRecruit(CompanionId::Sekar);
    g.debugRecruit(CompanionId::Bima);
    std::string err;
    CHECK(g.saveGame(&err));
    CHECK(err.empty());
    CHECK(!std::filesystem::exists(base + ".tmp"));
    CHECK(readFile(base).rfind("AETHSAVE1", 0) == 0);

    Game h(999, base);
    CHECK(h.loadGame(&err));
    CHECK(err.empty());
    CHECK_EQ(h.player().level, 5);
    CHECK_EQ(h.player().gold, 777);
    CHECK_EQ(h.player().oil, 4);
    CHECK_EQ(h.resonance(), 55);
    CHECK_EQ(h.act(), 2);
    CHECK_EQ(h.memoryCount(), 2);
    CHECK_EQ(h.companionCount(), 2);
    CHECK_EQ(h.companion(0).name, "Sekar");
    CHECK_EQ(h.companion(1).name, "Bima");
    CHECK_EQ(inspectSave(base).version, 3);
    CHECK_EQ(inspectSave(base).gold, 777);
    CHECK(anySaveExists(base));

    // Legacy plaintext version 2 still loads and is upgraded on load.
    {
        std::ofstream old(base, std::ios::binary | std::ios::trunc);
        old << "version 2\nmap 0\nx 224\ny 256\nlevel 2\ngold 42\n";
    }
    Game legacy(7, base);
    CHECK(legacy.loadGame(&err));
    CHECK_EQ(legacy.player().level, 2);
    CHECK_EQ(legacy.player().gold, 42);
    CHECK(inspectSave(base).version == 3);
    CHECK(readFile(base).rfind("AETHSAVE1", 0) == 0);

    // Corrupt saves are rejected.
    writeFile(base, "version 99\nmap 0\n");
    Game bad(8, base);
    CHECK(!bad.loadGame(&err));
    CHECK(!err.empty());
    removeSaveFiles(base);
}

// ---------------------------------------------------------------- exploration/story

void testStoryStart() {
    std::string base = tmpPath("pelita_story.dat");
    Game g(123, base);
    startExplore(g);
    CHECK(g.state() == GameState::Explore);
    CHECK_EQ(g.act(), 0);
    CHECK_EQ(g.currentMap(), g.world().indexOf("village"));

    // Talk to Mbok Ratih (12,6) from below.
    g.debugWarp(0, 12, 7);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.flags().courierSaved);
    CHECK_EQ(g.act(), 1);
    CHECK(g.hasMemory(0));

    // Portals gated by act: harbor -> candi requires act 1.
    g.debugWarp(1, 20, 3);
    g.debugSetFacing(1);
    g.debugInteract();  // stand below the portal tile and press up
    idle(g, 10);
    CHECK_EQ(g.currentMap(), 1);
    // Step onto the portal itself.
    g.debugWarp(1, 20, 3);
    g.debugSetPlayerPos(20 * TILE, 2 * TILE);
    idle(g, 110);
    CHECK_EQ(g.currentMap(), g.world().indexOf("candi"));
    removeSaveFiles(base);
}

void testRecruitAndEcho() {
    std::string base = tmpPath("pelita_recruit.dat");
    Game g(321, base);
    startExplore(g);
    g.debugRecruit(CompanionId::Sekar);
    g.debugRecruit(CompanionId::Bima);
    g.debugRecruit(CompanionId::Larasati);
    g.debugRecruit(CompanionId::Bagas);
    CHECK_EQ(g.companionCount(), 4);
    CHECK(g.flags().sekarJoined);
    CHECK(g.flags().bimaJoined);
    CHECK(g.flags().larasJoined);
    CHECK(g.flags().bagasJoined);

    int before = g.resonance();
    // Village echo at (18,12): stand below and face up.
    g.debugWarp(0, 18, 13);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.hasMemory(0));
    CHECK(g.resonance() >= before);
    removeSaveFiles(base);
}

void testCampfireAndChest() {
    std::string base = tmpPath("pelita_camp.dat");
    Game g(555, base);
    startExplore(g);
    g.player().hp = 5;
    g.player().oil = 2;
    g.flagsMutable().resonance = 40;
    // Village campfire at (14,16).
    g.debugWarp(0, 14, 17);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK_EQ(g.player().hp, g.player().maxHp);
    CHECK_EQ(g.player().oil, 1);
    CHECK(g.resonance() > 40);

    // Village chest at (26,3) contains oil.
    int oilBefore = g.player().oil;
    g.debugWarp(0, 26, 4);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.player().oil > oilBefore);
    removeSaveFiles(base);
}

void testPortalAndCollision() {
    std::string base = tmpPath("pelita_portal.dat");
    Game g(99, base);
    startExplore(g);
    g.flagsMutable().act = 4;
    // Harbor -> kraton requires act 3; with act 4 it opens.
    g.debugWarp(1, 19, 3);
    g.debugSetPlayerPos(19 * TILE, 2 * TILE);
    idle(g, 110);
    CHECK_EQ(g.currentMap(), g.world().indexOf("kraton"));
    removeSaveFiles(base);
}

// ---------------------------------------------------------------- battle

void testActProgression() {
    std::string base = tmpPath("pelita_act.dat");
    Game g(1234, base);
    startExplore(g);
    g.flagsMutable().act = 1;
    g.debugRecruit(CompanionId::Bima);
    g.debugRecruit(CompanionId::Sekar);
    g.debugRecruit(CompanionId::Larasati);
    g.debugWarp(1, 8, 9);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.hasMemory(1));
    g.debugWarp(1, 28, 9);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.hasMemory(2));
    CHECK_EQ(g.act(), 2);

    // Arwah Gema boss victory ends Act II.
    g.debugStartBattle(EnemyType::ArwahGema, true);
    g.battle().forceEnemyHp(1);
    g.battle().forceCommandPhase();
    tap(g, Key::Confirm);
    for (int i = 0; i < 240 && g.state() == GameState::Battle; ++i) {
        idle(g, 1);
        if (i % 4 == 0) tap(g, Key::Confirm);
    }
    CHECK(g.flags().fatherMemory);
    CHECK_EQ(g.act(), 3);
    removeSaveFiles(base);
}

void testLanternQuest() {
    std::string base = tmpPath("pelita_lantern.dat");
    Game g(4321, base);
    startExplore(g);
    // Nenek Sari at village (21,14).
    g.debugWarp(0, 21, 15);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.flags().lanternQuestAccepted);

    // Lantern echo in Rimba at (24,8).
    g.debugWarp(2, 24, 9);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.hasMemory(3));

    // Return the lantern.
    g.debugWarp(0, 21, 15);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.flags().lanternQuestDone);
    CHECK(g.player().hasAshCharm);
    removeSaveFiles(base);
}

void testFullFlow() {
    std::string base = tmpPath("pelita_full.dat");
    Game g(2026, base);
    startExplore(g);
    talkTo(g, 0, 12, 6);
    CHECK_EQ(g.act(), 1);

    talkTo(g, 1, 15, 9);
    talkTo(g, 1, 26, 9);
    talkTo(g, 1, 20, 15);
    CHECK(g.flags().bimaJoined);
    CHECK(g.flags().sekarJoined);
    CHECK(g.flags().larasJoined);
    g.debugWarp(1, 8, 9);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    g.debugWarp(1, 28, 9);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK_EQ(g.act(), 2);

    talkTo(g, 4, 17, 8);
    CHECK(g.flags().bagasJoined);
    talkTo(g, 4, 17, 4);
    CHECK(killBoss(g));
    CHECK(g.flags().gajaminaDefeated);
    CHECK(g.player().hasTirtaCore);

    g.debugWarp(2, 24, 9);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.hasMemory(3));

    // Reading the father's echo must not hide the Sumur Ratapan boss.
    g.debugWarp(3, 16, 9);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.hasMemory(5));
    talkTo(g, 3, 16, 4);
    CHECK(killBoss(g));
    CHECK(g.flags().fatherMemory);
    CHECK_EQ(g.act(), 3);

    talkTo(g, 5, 7, 6);
    CHECK(g.flags().nirmalaTruth);

    talkTo(g, 7, 15, 8);
    CHECK(g.flags().kiranaHelped);
    CHECK(g.player().hasCourtKey);
    talkTo(g, 7, 20, 4);
    CHECK_EQ(g.act(), 4);
    talkTo(g, 7, 15, 6);
    CHECK(killBoss(g));

    talkTo(g, 8, 17, 9);
    CHECK(killBoss(g));
    CHECK(g.flags().gunturDefeated);

    // Reading the final echo must not hide Sang Gema Pertama.
    g.debugWarp(8, 17, 11);
    g.debugSetFacing(1);
    g.debugInteract();
    g.debugAdvanceDialogue();
    CHECK(g.hasMemory(11));
    talkTo(g, 8, 17, 3);
    killBoss(g);
    CHECK(g.state() == GameState::Ending);
    CHECK_EQ(g.endingId(), 2);
    removeSaveFiles(base);
}

void testPartyBattle() {
    std::string base = tmpPath("pelita_battle.dat");
    Game g(777, base);
    startExplore(g);
    g.debugRecruit(CompanionId::Sekar);
    g.debugRecruit(CompanionId::Bima);
    g.debugRecruit(CompanionId::Larasati);
    g.debugRecruit(CompanionId::Bagas);
    g.debugStartBattle(EnemyType::KumbangPualam, false);
    CHECK(g.state() == GameState::Battle);
    CHECK(g.battle().enemyType() == EnemyType::KumbangPualam);
    g.battle().forceEnemyHp(1);
    g.battle().forceCommandPhase();
    int goldBefore = g.player().gold;
    int xpBefore = g.player().xp + g.player().level * 1000;
    tap(g, Key::Confirm);
    for (int i = 0; i < 240 && g.state() == GameState::Battle; ++i) {
        idle(g, 1);
        if (i % 4 == 0) tap(g, Key::Confirm);
    }
    CHECK(g.state() == GameState::Explore);
    CHECK(g.player().gold > goldBefore);
    CHECK(g.player().xp + g.player().level * 1000 > xpBefore);

    // Gema skill spends resonance.
    g.flagsMutable().resonance = 80;
    g.debugStartBattle(EnemyType::GukAbu, false);
    g.battle().forceCommandPhase();
    tap(g, Key::Down);  // Gema menu
    tap(g, Key::Confirm);
    tap(g, Key::Down);
    tap(g, Key::Down);  // Ledakan Kenangan
    tap(g, Key::Confirm);
    for (int i = 0; i < 240 && g.state() == GameState::Battle; ++i) {
        idle(g, 1);
        if (i % 4 == 0) tap(g, Key::Confirm);
    }
    CHECK(g.resonance() < 80);
    removeSaveFiles(base);
}

void testEndingChoice() {
    std::string base = tmpPath("pelita_ending.dat");
    Game g(888, base);
    startExplore(g);
    g.debugRecruit(CompanionId::Sekar);
    g.debugRecruit(CompanionId::Bima);
    g.debugRecruit(CompanionId::Larasati);
    g.debugRecruit(CompanionId::Bagas);
    g.flagsMutable().resonance = 90;
    g.flagsMutable().hiddenQuestDone = true;
    g.flagsMutable().gajaminaDefeated = true;
    g.player().hasTirtaCore = true;
    g.flagsMutable().memories = 0xFFF;
    g.debugStartBattle(EnemyType::GemaPertama, true);
    g.battle().forceEnemyHp(1);
    g.battle().forceCommandPhase();
    tap(g, Key::Confirm);
    for (int i = 0; i < 240 && g.state() == GameState::Battle; ++i) {
        idle(g, 1);
        if (i % 4 == 0) tap(g, Key::Confirm);
    }
    CHECK(g.state() == GameState::Dialogue);
    g.debugAdvanceDialogue();  // chooses the first available ending
    CHECK(g.state() == GameState::Ending);
    CHECK_EQ(g.endingId(), 4);
    removeSaveFiles(base);
}

// ---------------------------------------------------------------- render/audio/settings

void testRenderSmoke() {
    std::string base = tmpPath("pelita_render.dat");
    Game g(2026, base);
    startExplore(g);
    Framebuffer fb;
    fb.resize(VIEW_W, VIEW_H);
    g.render(fb);
    int lit = 0;
    for (uint32_t c : fb.px)
        if ((c & 0x00FFFFFFu) != 0) ++lit;
    CHECK(lit > VIEW_W * VIEW_H / 2);

    g.debugOpenMenu();
    g.render(fb);
    g.debugStartBattle(EnemyType::ArwahGema, false);
    g.render(fb);
    g.debugStartEnding();
    g.render(fb);
    removeSaveFiles(base);
}

void testAudio() {
    for (int i = 0; i < static_cast<int>(MusicId::Count); ++i) {
        MusicId id = static_cast<MusicId>(i);
        std::vector<int16_t> a = buildMusic(id);
        CHECK(a == buildMusic(id));
        CHECK(std::strcmp(musicName(id), "?") != 0);
    }
    CHECK(buildMusic(MusicId::None).empty());
    for (int i = 0; i < static_cast<int>(SfxId::Count); ++i) {
        SfxId id = static_cast<SfxId>(i);
        std::vector<int16_t> a = buildSfx(id);
        CHECK(!a.empty());
        CHECK(a == buildSfx(id));
        CHECK(std::strcmp(sfxName(id), "?") != 0);
    }
}

void testSettings() {
    std::string root = tempDir() + "/pelita_settings";
    std::string path = root + "/settings.cfg";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    Settings s;
    s.musicVolume = 0.4f;
    s.sfxVolume = 0.7f;
    s.fullscreen = true;
    CHECK(saveSettings(path, s));
    Settings loaded = loadSettings(path);
    CHECK_NEAR(loaded.musicVolume, 0.4f);
    CHECK_NEAR(loaded.sfxVolume, 0.7f);
    CHECK(loaded.fullscreen);
}

int main() {
    std::printf("pelita test suite\n");
    testCore();
    testPlayer();
    testParty();
    testWorld();
    testSaveEncoding();
    testSaveGame();
    testStoryStart();
    testRecruitAndEcho();
    testCampfireAndChest();
    testPortalAndCollision();
    testActProgression();
    testLanternQuest();
    testFullFlow();
    testPartyBattle();
    testEndingChoice();
    testRenderSmoke();
    testAudio();
    testSettings();
    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
