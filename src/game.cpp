// game.cpp - exploration, story, menus, save/load
#include "game.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

#include "art.h"

namespace rpg {

namespace {

bool validFloat(float v) { return std::isfinite(v); }

}  // namespace

const ShopItem SHOP[SHOP_COUNT] = {
    {0, 0, "Ramuan", 12},           {1, 0, "Eliksir", 30},
    {2, 1, "Pedang Perunggu", 60},  {3, 1, "Armor Kulit", 50},
    {2, 2, "Pedang Besi", 140},     {3, 2, "Baju Rantai", 130},
};

Game::Game(uint64_t seed, std::string savePath) : rng_(seed), savePath_(std::move(savePath)) {
    chestOpened_.resize(static_cast<size_t>(world_.count()));
    for (int i = 0; i < world_.count(); ++i)
        chestOpened_[static_cast<size_t>(i)].assign(world_.def(i).chests.size(), false);
    map_ = 0;
    px_ = 14 * TILE;
    py_ = 18 * TILE;
    facing_ = 1;
    updateCamera(100.0f);
}

void Game::newGame() {
    player_ = Player();
    flags_ = Flags();
    map_ = world_.indexOf("village");
    if (map_ < 0) map_ = 0;
    px_ = 14 * TILE;
    py_ = 18 * TILE;
    facing_ = 1;
    moveAxis(0.0f, 0.0f);
    clampPlayerToMap();
    updateCamera(100.0f);
    chestOpened_.assign(static_cast<size_t>(world_.count()), {});
    for (int i = 0; i < world_.count(); ++i)
        chestOpened_[static_cast<size_t>(i)].assign(world_.def(i).chests.size(), false);
    stepsSinceBattle_ = 0;
    stepAccum_ = 0.0f;
    portalCooldown_ = 0.5f;
    interactCooldown_ = 0.0f;
    particles_.clear();
    floats_.clear();
    flash_ = 0.0f;
    shake_ = 0.0f;
    transition_ = false;
    mapBanner_ = 3.0f;
    mapBannerText_ = world_.def(map_).name;
    state_ = GameState::Explore;
    startDialogue("Narator",
                  {"Dahulu, Lembah Aetheria diterangi oleh Batu Matahari.",
                   "Kini relik itu hilang, dan bayangan merayap keluar dari Gua Bayangan.",
                   "Elmwood menanti seorang pahlawan. Mungkin... kau?"},
                  [this] {
                      flags_.introSeen = true;
                      state_ = GameState::Explore;
                  });
}

bool Game::hasSave() const {
    std::ifstream f(savePath_);
    return f.good();
}

bool Game::saveGame(std::string* err) {
    if (err) err->clear();
    std::string tmp = savePath_ + ".tmp";
    std::ofstream f(tmp, std::ios::trunc);
    if (!f) {
        if (err) *err = "Tidak bisa menulis berkas simpanan.";
        return false;
    }
    int flagBits = 0;
    if (flags_.questAccepted) flagBits |= 1;
    if (flags_.bossDefeated) flagBits |= 2;
    if (flags_.sunstoneFound) flagBits |= 4;
    if (flags_.gameCompleted) flagBits |= 8;
    if (flags_.metElder) flagBits |= 16;
    if (flags_.introSeen) flagBits |= 32;

    f << "version 1\n";
    f << "map " << map_ << "\n";
    f << "x " << px_ << "\n";
    f << "y " << py_ << "\n";
    f << "facing " << facing_ << "\n";
    f << "level " << player_.level << "\n";
    f << "xp " << player_.xp << "\n";
    f << "gold " << player_.gold << "\n";
    f << "hp " << player_.hp << "\n";
    f << "mp " << player_.mp << "\n";
    f << "weapon " << player_.weapon << "\n";
    f << "armor " << player_.armor << "\n";
    f << "potions " << player_.potions << "\n";
    f << "elixirs " << player_.elixirs << "\n";
    f << "sunstone " << (player_.hasSunstone ? 1 : 0) << "\n";
    f << "flags " << flagBits << "\n";
    f << "chests";
    for (int i = 0; i < world_.count(); ++i) {
        int mask = 0;
        const auto& opened = chestOpened_[static_cast<size_t>(i)];
        for (size_t k = 0; k < opened.size() && k < 31; ++k)
            if (opened[k]) mask |= (1 << k);
        f << " " << mask;
    }
    f << "\n";
    if (!f) {
        if (err) *err = "Gagal menulis data simpanan.";
        f.close();
        std::remove(tmp.c_str());
        return false;
    }
    f.close();
    if (std::rename(tmp.c_str(), savePath_.c_str()) != 0) {
        if (err) *err = "Gagal menyimpan berkas.";
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool Game::loadGame(std::string* err) {
    if (err) err->clear();
    auto fail = [&](const char* msg) {
        if (err) *err = msg;
        return false;
    };
    std::ifstream f(savePath_);
    if (!f) return fail("Tidak ada berkas simpanan.");

    std::map<std::string, std::string> kv;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        kv[line.substr(0, sp)] = line.substr(sp + 1);
    }
    auto getInt = [&](const char* key, int def, int lo, int hi) {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        try {
            long v = std::stol(it->second);
            return clampInt(static_cast<int>(v), lo, hi);
        } catch (...) {
            return def;
        }
    };
    auto getFloat = [&](const char* key, float def) {
        auto it = kv.find(key);
        if (it == kv.end()) return def;
        try {
            return std::stof(it->second);
        } catch (...) {
            return std::numeric_limits<float>::quiet_NaN();
        }
    };

    if (getInt("version", 0, 0, 999) != 1) return fail("Versi simpanan tidak dikenal.");
    int newMap = getInt("map", -1, -1, world_.count());
    if (newMap < 0 || newMap >= world_.count()) return fail("Peta pada simpanan tidak valid.");

    Player p;
    p.level = getInt("level", 1, 1, 99);
    p.recalcStats(true);
    p.xp = getInt("xp", 0, 0, 1000000);
    p.gold = getInt("gold", 30, 0, 9999999);
    p.hp = getInt("hp", p.maxHp, 0, p.maxHp);
    if (p.hp <= 0) p.hp = 1;
    p.mp = getInt("mp", p.maxMp, 0, p.maxMp);
    p.weapon = getInt("weapon", 0, 0, 2);
    p.armor = getInt("armor", 0, 0, 2);
    p.potions = getInt("potions", 0, 0, 99);
    p.elixirs = getInt("elixirs", 0, 0, 99);
    p.hasSunstone = getInt("sunstone", 0, 0, 1) != 0;

    float nx = getFloat("x", 224.0f);
    float ny = getFloat("y", 288.0f);
    const MapDef& md = world_.def(newMap);
    if (!validFloat(nx) || !validFloat(ny)) return fail("Posisi simpanan rusak.");
    nx = clampf(nx, 0.0f, static_cast<float>(md.w * TILE - TILE));
    ny = clampf(ny, 0.0f, static_cast<float>(md.h * TILE - TILE));

    // Ensure the loaded position is not inside a wall.
    bool freeSpot = false;
    for (int radius = 0; radius <= 5 && !freeSpot; ++radius) {
        for (int dy = -radius; dy <= radius && !freeSpot; ++dy) {
            for (int dx = -radius; dx <= radius && !freeSpot; ++dx) {
                int tx = tileOfX(nx) + dx, ty = tileOfY(ny) + dy;
                if (!world_.blocked(newMap, tx, ty) && !world_.npcAt(newMap, tx, ty) &&
                    !world_.chestAt(newMap, tx, ty)) {
                    nx = static_cast<float>(tx * TILE);
                    ny = static_cast<float>(ty * TILE);
                    freeSpot = true;
                }
            }
        }
    }
    if (!freeSpot) return fail("Posisi simpanan tidak bisa dipakai.");

    int flagBits = getInt("flags", 0, 0, 255);
    Player oldPlayer = player_;
    Flags oldFlags = flags_;
    (void)oldPlayer;
    (void)oldFlags;

    player_ = p;
    flags_ = Flags{};
    flags_.questAccepted = (flagBits & 1) != 0;
    flags_.bossDefeated = (flagBits & 2) != 0;
    flags_.sunstoneFound = (flagBits & 4) != 0;
    flags_.gameCompleted = (flagBits & 8) != 0;
    flags_.metElder = (flagBits & 16) != 0;
    flags_.introSeen = (flagBits & 32) != 0;

    map_ = newMap;
    px_ = nx;
    py_ = ny;
    facing_ = getInt("facing", 0, 0, 3);

    chestOpened_.assign(static_cast<size_t>(world_.count()), {});
    for (int i = 0; i < world_.count(); ++i)
        chestOpened_[static_cast<size_t>(i)].assign(world_.def(i).chests.size(), false);

    auto cit = kv.find("chests");
    if (cit != kv.end()) {
        std::istringstream ss(cit->second);
        int mask = 0;
        for (int i = 0; i < world_.count() && (ss >> mask); ++i) {
            auto& opened = chestOpened_[static_cast<size_t>(i)];
            for (size_t k = 0; k < opened.size() && k < 31; ++k) opened[k] = (mask & (1 << k)) != 0;
        }
    }

    stepsSinceBattle_ = 0;
    stepAccum_ = 0.0f;
    portalCooldown_ = 1.0f;
    interactCooldown_ = 0.0f;
    particles_.clear();
    floats_.clear();
    state_ = GameState::Explore;
    mapBanner_ = 3.0f;
    mapBannerText_ = world_.def(map_).name;
    updateCamera(100.0f);
    return true;
}

void Game::startDialogue(const std::string& speaker, const std::vector<std::string>& pages,
                         std::function<void()> onClose) {
    dlg_.pages = pages;
    dlg_.speaker = speaker;
    dlg_.page = 0;
    dlg_.chars = 0.0f;
    dlg_.active = true;
    dlg_.onClose = std::move(onClose);
    state_ = GameState::Dialogue;
}

void Game::closeDialogue() {
    dlg_.active = false;
    auto cb = dlg_.onClose;
    dlg_.onClose = nullptr;
    state_ = GameState::Explore;
    if (cb) cb();
}

void Game::startTransition(std::function<void()> action) {
    if (transition_) return;
    transition_ = true;
    transitionT_ = 0.0f;
    transitionMid_ = false;
    transitionAction_ = std::move(action);
}

void Game::updateTransition(float dt) {
    if (!transition_) return;
    transitionT_ += dt / 0.42f;
    if (!transitionMid_ && transitionT_ >= 0.5f) {
        transitionMid_ = true;
        auto act = transitionAction_;
        transitionAction_ = nullptr;
        if (act) act();
    }
    if (transitionT_ >= 1.0f) {
        transitionT_ = 0.0f;
        transition_ = false;
    }
}

void Game::changeMap(int target, int spawnTileX, int spawnTileY, int facing) {
    if (target < 0 || target >= world_.count()) return;
    map_ = target;
    px_ = static_cast<float>(spawnTileX * TILE);
    py_ = static_cast<float>(spawnTileY * TILE);
    clampPlayerToMap();
    facing_ = clampInt(facing, 0, 3);
    stepAccum_ = 0.0f;
    stepsSinceBattle_ = 0;
    updateCamera(100.0f);
    mapBanner_ = 2.8f;
    mapBannerText_ = world_.def(map_).name;
    if (transition_) transitionT_ = std::max(transitionT_, 0.5f);
}

void Game::updateCamera(float dt) {
    const MapDef& md = world_.def(map_);
    float targetX = px_ + 8.0f - VIEW_W * 0.5f;
    float targetY = py_ + 8.0f - VIEW_H * 0.5f;
    float maxX = static_cast<float>(md.w * TILE - VIEW_W);
    float maxY = static_cast<float>(md.h * TILE - VIEW_H);
    targetX = clampf(targetX, 0.0f, std::max(0.0f, maxX));
    targetY = clampf(targetY, 0.0f, std::max(0.0f, maxY));
    float k = clampf(dt * 10.0f, 0.0f, 1.0f);
    camera_.x = lerp(camera_.x, targetX, k);
    camera_.y = lerp(camera_.y, targetY, k);
    if (std::abs(camera_.x - targetX) < 1.5f) camera_.x = targetX;
    if (std::abs(camera_.y - targetY) < 1.5f) camera_.y = targetY;
    camera_.x = clampf(camera_.x, 0.0f, std::max(0.0f, maxX));
    camera_.y = clampf(camera_.y, 0.0f, std::max(0.0f, maxY));
}

void Game::clampPlayerToMap() {
    const MapDef& md = world_.def(map_);
    px_ = clampf(px_, 0.0f, static_cast<float>(md.w * TILE - TILE));
    py_ = clampf(py_, 0.0f, static_cast<float>(md.h * TILE - TILE));
}

bool Game::collides(float nx, float ny) const {
    float x0 = nx + 4.0f, y0 = ny + 10.0f;
    float x1 = nx + 11.99f, y1 = ny + 15.99f;
    int tx0 = tileOfX(x0), tx1 = tileOfX(x1);
    int ty0 = tileOfY(y0), ty1 = tileOfY(y1);
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            if (world_.blocked(map_, tx, ty)) return true;
            const NpcSpawn* npc = world_.npcAt(map_, tx, ty);
            if (npc) {
                bool hidden = npc->style == NpcStyle::Warden && flags_.bossDefeated;
                if (!hidden) return true;
            }
            if (world_.chestAt(map_, tx, ty)) return true;
        }
    }
    return false;
}

void Game::moveAxis(float axisX, float axisY) {
    if (axisX == 0.0f && axisY == 0.0f) return;
    float nx = px_ + axisX;
    float ny = py_ + axisY;
    if (!collides(nx, ny)) {
        px_ = nx;
        py_ = ny;
        return;
    }
    int steps = static_cast<int>(std::ceil(std::max(std::abs(axisX), std::abs(axisY))));
    if (steps < 1) steps = 1;
    float sx = axisX / static_cast<float>(steps);
    float sy = axisY / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        if (collides(px_ + sx, py_ + sy)) break;
        px_ += sx;
        py_ += sy;
    }
}

void Game::spawnDust() {
    Particle p;
    p.pos = {px_ + 6.0f + rng_.unit() * 4.0f, py_ + 15.0f};
    p.vel = {rng_.unit() * 6.0f - 3.0f, -rng_.unit() * 8.0f - 2.0f};
    p.life = p.maxLife = 0.4f;
    p.color = 0xFFD8C8A8;
    p.size = 1;
    p.gravity = 20.0f;
    particles_.push_back(p);
}

void Game::spawnBurst(float x, float y, int count, uint32_t color, float speed) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = {x, y};
        float a = rng_.unit() * 6.2831853f;
        float s = speed * (0.35f + rng_.unit());
        p.vel = {std::cos(a) * s, std::sin(a) * s - speed * 0.35f};
        p.life = p.maxLife = 0.5f + rng_.unit() * 0.5f;
        p.color = color;
        p.size = rng_.range(1, 2);
        p.gravity = 40.0f;
        particles_.push_back(p);
    }
}

void Game::addFloat(float x, float y, const std::string& text, uint32_t color) {
    FlyingText ft;
    ft.pos = {x, y};
    ft.life = 1.0f;
    ft.text = text;
    ft.color = color;
    floats_.push_back(ft);
}

void Game::updateParticles(float dt) {
    for (auto& p : particles_) {
        p.life -= dt;
        p.vel.y += p.gravity * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle& p) { return p.life <= 0.0f; }),
                     particles_.end());
    for (auto& f : floats_) {
        f.life -= dt;
        f.pos.y -= 14.0f * dt;
    }
    floats_.erase(std::remove_if(floats_.begin(), floats_.end(),
                                 [](const FlyingText& f) { return f.life <= 0.0f; }),
                  floats_.end());
}

void Game::onStep() {
    int tx = tileOfX(px_ + 8.0f);
    int ty = tileOfY(py_ + 13.0f);
    ++stepsSinceBattle_;
    if (!world_.encounter(map_, tx, ty)) return;
    if (stepsSinceBattle_ < 4) return;
    float chance = map_ == 2 ? 0.16f : 0.13f;
    if (!rng_.chance(chance)) return;
    EnemyType type = EnemyType::Slime;
    pickRandomEnemy(&type);
    uint64_t seed = rng_.next();
    stepsSinceBattle_ = 0;
    startTransition([this, type, seed] {
        state_ = GameState::Battle;
        battle_.begin(player_, type, false, seed);
    });
}

void Game::pickRandomEnemy(EnemyType* type) {
    int r = rng_.range(0, 99);
    if (map_ == 2) {
        if (r < 25) *type = EnemyType::Bat;
        else if (r < 60) *type = EnemyType::Wolf;
        else *type = EnemyType::Skeleton;
    } else if (map_ == 1) {
        if (r < 50) *type = EnemyType::Slime;
        else if (r < 80) *type = EnemyType::Bat;
        else *type = EnemyType::Wolf;
    } else {
        *type = EnemyType::Slime;
    }
}

void Game::checkPortal() {
    if (portalCooldown_ > 0.0f) return;
    int tx = tileOfX(px_ + 8.0f);
    int ty = tileOfY(py_ + 13.0f);
    const Portal* portal = world_.portalAt(map_, tx, ty);
    if (!portal) return;
    int target = portal->targetMap, sx = portal->spawnX, sy = portal->spawnY, sd = portal->spawnDir;
    portalCooldown_ = 1.4f;
    startTransition([this, target, sx, sy, sd] { changeMap(target, sx, sy, sd); });
}

void Game::openChest(const ChestSpawn& chest, int index) {
    if (index < 0 || index >= static_cast<int>(chestOpened_[static_cast<size_t>(map_)].size())) return;
    if (chestOpened_[static_cast<size_t>(map_)][static_cast<size_t>(index)]) {
        startDialogue("Peti", {"Peti ini sudah kosong."});
        return;
    }
    chestOpened_[static_cast<size_t>(map_)][static_cast<size_t>(index)] = true;
    std::string msg;
    float cx = chest.x * TILE + 8.0f;
    float cy = chest.y * TILE + 8.0f;
    switch (chest.item) {
        case ItemId::Gold:
            player_.gold += chest.amount;
            msg = "Kau menemukan " + std::to_string(chest.amount) + " emas!";
            addFloat(cx, cy, "+" + std::to_string(chest.amount) + "G", pal::gold);
            spawnBurst(cx, cy, 14, pal::gold, 30.0f);
            break;
        case ItemId::Potion:
            player_.potions += chest.amount;
            msg = "Kau menemukan Ramuan x" + std::to_string(chest.amount) + "!";
            addFloat(cx, cy, "+Ramuan", 0xFFFF8080u);
            spawnBurst(cx, cy, 14, 0xFFFF8080u, 30.0f);
            break;
        case ItemId::Elixir:
            player_.elixirs += chest.amount;
            msg = "Kau menemukan Eliksir x" + std::to_string(chest.amount) + "!";
            addFloat(cx, cy, "+Eliksir", 0xFF80A8FFu);
            spawnBurst(cx, cy, 14, 0xFF80A8FFu, 30.0f);
            break;
        case ItemId::IronSword:
            if (player_.weapon < 2) {
                player_.weapon = 2;
                msg = "Kau menemukan Pedang Besi! Serangan meningkat.";
            } else {
                player_.gold += 80;
                msg = "Kau menemukan Pedang Besi, tapi sudah memakainya. Dijual +80 emas.";
            }
            spawnBurst(cx, cy, 14, pal::white, 30.0f);
            break;
        case ItemId::ChainMail:
            if (player_.armor < 2) {
                player_.armor = 2;
                msg = "Kau menemukan Baju Rantai! Pertahanan meningkat.";
            } else {
                player_.gold += 75;
                msg = "Kau menemukan Baju Rantai, tapi sudah memakainya. Dijual +75 emas.";
            }
            spawnBurst(cx, cy, 14, pal::white, 30.0f);
            break;
        case ItemId::Sunstone:
            player_.hasSunstone = true;
            flags_.sunstoneFound = true;
            msg = "Batu Matahari! Kehangatannya mengusir kegelapan.";
            break;
        default:
            msg = "Peti kosong.";
            break;
    }
    startDialogue("Peti Harta", {msg}, [this] { state_ = GameState::Explore; });
}

void Game::interact() {
    int dx = facing_ == 2 ? -1 : (facing_ == 3 ? 1 : 0);
    int dy = facing_ == 0 ? 1 : (facing_ == 1 ? -1 : 0);
    int tx = tileOfX(px_ + 8.0f) + dx;
    int ty = tileOfY(py_ + 12.0f) + dy;

    const NpcSpawn* npc = world_.npcAt(map_, tx, ty);
    if (npc) {
        bool hidden = npc->style == NpcStyle::Warden && flags_.bossDefeated;
        if (hidden) return;
        switch (npc->style) {
            case NpcStyle::Elder: elderTalk(); break;
            case NpcStyle::Shopkeeper:
                state_ = GameState::Shop;
                shopIndex_ = 0;
                shopMessage_ = "Selamat datang di Toko Rilla!";
                shopMessageTimer_ = 3.0f;
                break;
            case NpcStyle::Innkeeper: innTalk(); break;
            case NpcStyle::Guard: guardTalk(); break;
            case NpcStyle::Warden: wardenTalk(); break;
            default: flavorTalk(npc->dialogue); break;
        }
        return;
    }

    const ChestSpawn* chest = world_.chestAt(map_, tx, ty);
    if (chest) {
        int index = static_cast<int>(chest - world_.def(map_).chests.data());
        openChest(*chest, index);
        return;
    }

    Tile t = world_.tile(map_, tx, ty);
    if (t == Tile::Sign) {
        if (map_ == 0) {
            startDialogue("Papan Kayu", {"\"Desa Elmwood\"", "Damai di bawah naungan lembah."});
        } else {
            startDialogue("Papan Kayu", {"\"Jembatan Tua\"", "Ke utara menuju Gua Bayangan. Hati-hati."});
        }
    } else if (t == Tile::Door) {
        startDialogue("Pintu", {"Pintu ini terkunci dari dalam."});
    } else if (t == Tile::CaveEntrance) {
        startDialogue("Gua Bayangan", {"Angin dingin berhembus dari dalam gua...",
                                       "Masuklah untuk melanjutkan perjalanan."});
    }
}

void Game::elderTalk() {
    if (!flags_.metElder) {
        flags_.metElder = true;
        flags_.questAccepted = true;
        startDialogue("Tetua Ardan",
                      {"Ah, kau pasti pemuda yang diceritakan ramalan itu.",
                       "Batu Matahari, cahaya lembah ini, dicuri oleh Warden Bayangan.",
                       "Tanpa relik itu, kegelapan akan menelan Elmwood.",
                       "Pergilah ke Gua Bayangan di timur laut. Kalahkan Warden, dan bawa kembali Batu Matahari."},
                      [this] { state_ = GameState::Explore; });
        return;
    }
    if (flags_.sunstoneFound && !flags_.gameCompleted) {
        flags_.gameCompleted = true;
        startDialogue("Tetua Ardan",
                      {"Batu Matahari! Aku bisa merasakan kehangatannya.",
                       "Kau telah mengusir bayangan dari Lembah Aetheria.",
                       "Terimalah restu kami, pahlawan. Namamu akan dikenang selamanya."},
                      [this] {
                          state_ = GameState::Ending;
                          endingScroll_ = 0.0f;
                      });
        return;
    }
    if (flags_.bossDefeated) {
        startDialogue("Tetua Ardan", {"Kau membawa harapan bagi kami semua.",
                                      "Istirahatlah, pahlawan. Desa ini aman berkatmu."});
        return;
    }
    if (flags_.questAccepted) {
        startDialogue("Tetua Ardan",
                      {"Gua Bayangan berada di timur laut, di balik jembatan tua.",
                       "Bawalah ramuan yang cukup, dan jangan ragu untuk berlatih.",
                       "Semoga cahaya menyertaimu."});
        return;
    }
    startDialogue("Tetua Ardan", {"Semoga harimu damai, anak muda."});
}

void Game::innTalk() {
    if (player_.hp >= player_.maxHp && player_.mp >= player_.maxMp) {
        startDialogue("Pemilik Penginapan", {"Kau belum lelah. Simpan emasmu untuk perjalanan."});
        return;
    }
    if (player_.gold < 10) {
        startDialogue("Pemilik Penginapan", {"Istirahat semalam 10 emas.", "Maaf, emasmu belum cukup."});
        return;
    }
    player_.gold -= 10;
    player_.fullRestore();
    spawnBurst(px_ + 8.0f, py_ + 8.0f, 18, 0xFF8CFFB0u, 26.0f);
    addFloat(px_ + 8.0f, py_ - 4.0f, "PULIH!", 0xFF8CFFB0u);
    startDialogue("Pemilik Penginapan",
                  {"Silakan beristirahat.", "Kau terbangun dengan tubuh segar.", "-10 emas, HP & MP penuh."});
}

void Game::guardTalk() {
    if (flags_.bossDefeated) {
        startDialogue("Penjaga Bram", {"Kegelapan telah pergi dari lembah.",
                                       "Desa ini berutang budi padamu, pahlawan."});
    } else {
        startDialogue("Penjaga Bram",
                      {"Monster berkeliaran di Padang Aetheria, terutama di rumput tinggi.",
                       "Jembatan tua di timur laut menuju Gua Bayangan.",
                       "Jangan masuk tanpa persiapan."});
    }
}

void Game::wardenTalk() {
    if (flags_.bossDefeated) return;
    uint64_t seed = rng_.next();
    startDialogue("Warden Bayangan",
                  {"Jadi... cahaya itu mengirim seorang anak manusia.",
                   "Batu Matahari kini milikku. Kegelapan akan menelan lembah ini.",
                   "Mari kita akhiri ini."},
                  [this, seed] {
                      startTransition([this, seed] {
                          state_ = GameState::Battle;
                          battle_.begin(player_, EnemyType::Warden, true, seed);
                      });
                  });
}

void Game::flavorTalk(int dialogueId) {
    switch (dialogueId) {
        case 4:
            startDialogue("Anak Desa", {"Kalau kau menginjak rumput tinggi, monster bisa muncul!",
                                        "Aku pernah lihat slime hijau besar di sana."});
            break;
        case 5:
            startDialogue("Petani Ruslan", {"Panen tahun ini buruk. Tanah terasa dingin.",
                                            "Mereka bilang itu ulah Warden di gua."});
            break;
        case 8:
            startDialogue("Warga Desa", {"Toko Rilla buka tiap hari.",
                                         "Ramuan menyembuhkan luka, Eliksir memulihkan sihir."});
            break;
        default:
            startDialogue("Warga Desa", {"Semoga harimu menyenangkan."});
            break;
    }
}

void Game::openMenu() {
    state_ = GameState::Menu;
    menuPage_ = 0;
    menuIndex_ = 0;
    menuMessage_.clear();
    menuMessageTimer_ = 0.0f;
}

void Game::closeMenu() {
    state_ = GameState::Explore;
    menuPage_ = 0;
    menuIndex_ = 0;
    menuMessage_.clear();
}

void Game::useFieldItem(bool elixir) {
    if (elixir) {
        if (player_.elixirs <= 0) {
            menuMessage_ = "Kau tidak punya Eliksir.";
        } else if (player_.mp >= player_.maxMp) {
            menuMessage_ = "MP sudah penuh.";
        } else {
            player_.useElixir();
            menuMessage_ = "Eliksir digunakan. MP pulih.";
        }
    } else {
        if (player_.potions <= 0) {
            menuMessage_ = "Kau tidak punya Ramuan.";
        } else if (player_.hp >= player_.maxHp) {
            menuMessage_ = "HP sudah penuh.";
        } else {
            player_.usePotion();
            menuMessage_ = "Ramuan digunakan. HP pulih.";
        }
    }
    menuMessageTimer_ = 2.4f;
}

void Game::saveFromMenu() {
    std::string err;
    if (saveGame(&err)) {
        menuMessage_ = "Permainan disimpan.";
    } else {
        menuMessage_ = err.empty() ? "Gagal menyimpan." : err;
    }
    menuMessageTimer_ = 2.6f;
}

void Game::updateTitle(float dt, const InputState& in) {
    titleTime_ += dt;
    titleMsgTimer_ = std::max(0.0f, titleMsgTimer_ - dt);
    if (in.hit(Key::Up)) titleIndex_ = (titleIndex_ + 2) % 3;
    if (in.hit(Key::Down)) titleIndex_ = (titleIndex_ + 1) % 3;
    if (in.hit(Key::Confirm)) {
        if (titleIndex_ == 0) {
            newGame();
        } else if (titleIndex_ == 1) {
            std::string err;
            if (!loadGame(&err)) {
                titleMsg_ = err;
                titleMsgTimer_ = 2.5f;
            }
        } else {
            quit_ = true;
        }
    }
}

void Game::updateExplore(float dt, const InputState& in) {
    portalCooldown_ = std::max(0.0f, portalCooldown_ - dt);
    interactCooldown_ = std::max(0.0f, interactCooldown_ - dt);
    mapBanner_ = std::max(0.0f, mapBanner_ - dt);
    flash_ = std::max(0.0f, flash_ - dt);
    shake_ = std::max(0.0f, shake_ - dt);

    if (in.hit(Key::Cancel) || in.hit(Key::Menu)) {
        openMenu();
        return;
    }
    if (in.hit(Key::Confirm) && interactCooldown_ <= 0.0f) {
        interactCooldown_ = 0.18f;
        interact();
        return;
    }

    float vx = 0.0f, vy = 0.0f;
    if (in.down(Key::Left)) vx -= 1.0f;
    if (in.down(Key::Right)) vx += 1.0f;
    if (in.down(Key::Up)) vy -= 1.0f;
    if (in.down(Key::Down)) vy += 1.0f;
    bool wasMoving = moving_;
    moving_ = (vx != 0.0f || vy != 0.0f);
    bool running = in.down(Key::Run);

    if (moving_) {
        if (vx < 0) facing_ = 2;
        else if (vx > 0) facing_ = 3;
        else if (vy < 0) facing_ = 1;
        else if (vy > 0) facing_ = 0;
        float len = std::sqrt(vx * vx + vy * vy);
        vx /= len;
        vy /= len;
        float speed = running ? 108.0f : 74.0f;
        float dx = vx * speed * dt;
        float dy = vy * speed * dt;
        moveAxis(dx, 0.0f);
        moveAxis(0.0f, dy);
        animPhase_ += dt * (running ? 11.0f : 7.5f);
        stepAccum_ += std::abs(dx) + std::abs(dy);
        while (stepAccum_ >= TILE) {
            stepAccum_ -= TILE;
            onStep();
            if (transition_) break;
        }
        dustTimer_ -= dt;
        if (dustTimer_ <= 0.0f) {
            dustTimer_ = running ? 0.10f : 0.16f;
            spawnDust();
        }
    } else {
        animPhase_ = 0.0f;
        dustTimer_ = 0.0f;
    }
    (void)wasMoving;

    clampPlayerToMap();
    updateCamera(dt);
    if (!transition_) checkPortal();
}

void Game::updateDialogue(float dt, const InputState& in) {
    if (!dlg_.active || dlg_.pages.empty()) {
        closeDialogue();
        return;
    }
    const std::string& page = dlg_.pages[static_cast<size_t>(dlg_.page)];
    int len = static_cast<int>(page.size());
    if (static_cast<int>(dlg_.chars) < len) dlg_.chars += dt * 48.0f;
    bool advance = in.hit(Key::Confirm) || in.hit(Key::Cancel);
    if (!advance) return;
    if (static_cast<int>(dlg_.chars) < len) {
        dlg_.chars = static_cast<float>(len);
        return;
    }
    ++dlg_.page;
    dlg_.chars = 0.0f;
    if (dlg_.page >= static_cast<int>(dlg_.pages.size())) closeDialogue();
}

void Game::updateBattle(float dt, const InputState& in) {
    battle_.update(dt, in, player_);
    if (!battle_.done()) return;

    if (battle_.victory()) {
        if (battle_.isBoss() && !flags_.bossDefeated) {
            flags_.bossDefeated = true;
            flags_.sunstoneFound = true;
            player_.hasSunstone = true;
            state_ = GameState::Dialogue;
            startDialogue("Warden Bayangan",
                          {"Kegelapan... menarikku kembali...",
                           "Batu Matahari... bebas...",
                           "Bawa relik itu ke Tetua Elmwood. Selamatkan lembah ini."},
                          [this] {
                              state_ = GameState::Explore;
                              mapBanner_ = 3.0f;
                              mapBannerText_ = "Batu Matahari ditemukan!";
                              spawnBurst(px_ + 8.0f, py_ + 8.0f, 26, pal::gold, 40.0f);
                          });
        } else {
            state_ = GameState::Explore;
        }
        stepsSinceBattle_ = 0;
    } else if (battle_.fled()) {
        state_ = GameState::Explore;
        stepsSinceBattle_ = 0;
    } else {
        state_ = GameState::GameOver;
        gameOverIndex_ = 0;
    }
    flash_ = 0.0f;
    shake_ = 0.0f;
}

void Game::updateMenu(float dt, const InputState& in) {
    menuMessageTimer_ = std::max(0.0f, menuMessageTimer_ - dt);
    if (menuPage_ == 0) {
        if (in.hit(Key::Up)) menuIndex_ = (menuIndex_ + 3) % 4;
        if (in.hit(Key::Down)) menuIndex_ = (menuIndex_ + 1) % 4;
        if (in.hit(Key::Cancel)) {
            closeMenu();
            return;
        }
        if (in.hit(Key::Confirm)) {
            if (menuIndex_ == 0) {
                menuPage_ = 1;
                menuIndex_ = 0;
            } else if (menuIndex_ == 1) {
                menuPage_ = 2;
            } else if (menuIndex_ == 2) {
                saveFromMenu();
            } else {
                closeMenu();
            }
        }
    } else if (menuPage_ == 1) {
        if (in.hit(Key::Up) || in.hit(Key::Down)) menuIndex_ = (menuIndex_ + 1) % 2;
        if (in.hit(Key::Cancel)) {
            menuPage_ = 0;
            menuIndex_ = 0;
            return;
        }
        if (in.hit(Key::Confirm)) useFieldItem(menuIndex_ == 1);
    } else {
        if (in.hit(Key::Cancel) || in.hit(Key::Confirm)) {
            menuPage_ = 0;
            menuIndex_ = 0;
        }
    }
}

void Game::updateShop(float dt, const InputState& in) {
    shopMessageTimer_ = std::max(0.0f, shopMessageTimer_ - dt);
    if (in.hit(Key::Up)) shopIndex_ = (shopIndex_ + SHOP_COUNT - 1) % SHOP_COUNT;
    if (in.hit(Key::Down)) shopIndex_ = (shopIndex_ + 1) % SHOP_COUNT;
    if (in.hit(Key::Cancel) || in.hit(Key::Menu)) {
        state_ = GameState::Explore;
        return;
    }
    if (!in.hit(Key::Confirm)) return;
    const ShopItem& item = SHOP[shopIndex_];
    if (player_.gold < item.price) {
        shopMessage_ = "Emasmu tidak cukup.";
        shopMessageTimer_ = 2.2f;
        return;
    }
    if (item.kind == 0) {
        player_.potions = std::min(99, player_.potions + 1);
        player_.gold -= item.price;
        shopMessage_ = "Ramuan dibeli.";
    } else if (item.kind == 1) {
        player_.elixirs = std::min(99, player_.elixirs + 1);
        player_.gold -= item.price;
        shopMessage_ = "Eliksir dibeli.";
    } else if (item.kind == 2) {
        if (player_.weapon >= item.index) {
            shopMessage_ = "Kau sudah punya senjata yang lebih baik.";
        } else {
            player_.weapon = item.index;
            player_.gold -= item.price;
            shopMessage_ = std::string(item.name) + " dibeli!";
        }
    } else {
        if (player_.armor >= item.index) {
            shopMessage_ = "Kau sudah punya armor yang lebih baik.";
        } else {
            player_.armor = item.index;
            player_.gold -= item.price;
            shopMessage_ = std::string(item.name) + " dibeli!";
        }
    }
    shopMessageTimer_ = 2.2f;
}

void Game::updateGameOver(float dt, const InputState& in) {
    (void)dt;
    if (in.hit(Key::Up) || in.hit(Key::Down)) gameOverIndex_ = 1 - gameOverIndex_;
    if (!in.hit(Key::Confirm)) return;
    if (gameOverIndex_ == 0) {
        int lost = player_.gold / 5;
        player_.gold = std::max(0, player_.gold - lost);
        player_.hp = std::max(1, player_.maxHp / 2);
        player_.mp = player_.maxMp / 2;
        map_ = world_.indexOf("village");
        if (map_ < 0) map_ = 0;
        px_ = 14 * TILE;
        py_ = 18 * TILE;
        facing_ = 1;
        updateCamera(100.0f);
        state_ = GameState::Explore;
        mapBanner_ = 3.0f;
        mapBannerText_ = "Kau bangkit kembali di Elmwood.";
    } else {
        state_ = GameState::Title;
        titleIndex_ = 0;
    }
}

void Game::updateEnding(float dt, const InputState& in) {
    endingScroll_ += dt * 22.0f;
    if (in.hit(Key::Confirm) && endingScroll_ > 100.0f) {
        state_ = GameState::Title;
        titleIndex_ = 0;
        endingScroll_ = 0.0f;
    }
}

void Game::update(float dt, const InputState& in) {
    globalTime_ += dt;
    updateParticles(dt);
    if (flash_ > 0.0f) flash_ = std::max(0.0f, flash_ - dt);
    if (shake_ > 0.0f) shake_ = std::max(0.0f, shake_ - dt);

    if (transition_) {
        updateTransition(dt);
        return;
    }

    switch (state_) {
        case GameState::Title: updateTitle(dt, in); break;
        case GameState::Explore: updateExplore(dt, in); break;
        case GameState::Dialogue: updateDialogue(dt, in); break;
        case GameState::Battle: updateBattle(dt, in); break;
        case GameState::Menu: updateMenu(dt, in); break;
        case GameState::Shop: updateShop(dt, in); break;
        case GameState::GameOver: updateGameOver(dt, in); break;
        case GameState::Ending: updateEnding(dt, in); break;
    }
}

void Game::debugWarp(int map, int tx, int ty) {
    if (map < 0 || map >= world_.count()) return;
    map_ = map;
    px_ = static_cast<float>(tx * TILE);
    py_ = static_cast<float>(ty * TILE);
    clampPlayerToMap();
    updateCamera(100.0f);
    state_ = GameState::Explore;
    portalCooldown_ = 1.0f;
    mapBanner_ = 2.5f;
    mapBannerText_ = world_.def(map_).name;
}

void Game::debugSetFacing(int facing) { facing_ = clampInt(facing, 0, 3); }

void Game::debugStartBattle(EnemyType type, bool boss) {
    state_ = GameState::Battle;
    battle_.begin(player_, type, boss, rng_.next());
}

void Game::debugOpenMenu() {
    state_ = GameState::Menu;
    menuPage_ = 0;
    menuIndex_ = 0;
}

void Game::debugInteract() { interact(); }

void Game::debugAdvanceDialogue() {
    for (int i = 0; i < 64 && state_ == GameState::Dialogue; ++i) {
        InputState in;
        in.pressed[static_cast<int>(Key::Confirm)] = true;
        updateDialogue(0.0f, in);
    }
}

bool Game::debugChestOpened(int map, int index) const {
    if (map < 0 || map >= static_cast<int>(chestOpened_.size())) return false;
    const auto& v = chestOpened_[static_cast<size_t>(map)];
    if (index < 0 || index >= static_cast<int>(v.size())) return false;
    return v[static_cast<size_t>(index)];
}

void Game::debugOpenShop() {
    state_ = GameState::Shop;
    shopIndex_ = 0;
    shopMessage_ = "Selamat datang di Toko Rilla!";
    shopMessageTimer_ = 3.0f;
}

void Game::debugStartEnding() {
    state_ = GameState::Ending;
    endingScroll_ = 0.0f;
}

void Game::debugGameOver() {
    state_ = GameState::GameOver;
    gameOverIndex_ = 0;
}

}  // namespace rpg
