// game.cpp - exploration, story, menus, save/load
#include "game.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <utility>

#include "art.h"
#include "audio.h"
#include "updater.h"

namespace rpg {

namespace {

bool validFloat(float v) { return std::isfinite(v); }

int roamSpeed(EnemyType type) {
    switch (type) {
        case EnemyType::Bat: return 23;
        case EnemyType::Wolf: return 30;
        case EnemyType::Skeleton: return 14;
        default: return 11;
    }
}

EnemyType rollRoamer(int map, Rng& rng) {
    int r = rng.range(0, 99);
    if (map == 3) {
        if (r < 45) return EnemyType::Skeleton;
        if (r < 80) return EnemyType::Wolf;
        return EnemyType::Bat;
    }
    if (map == 2) {
        if (r < 25) return EnemyType::Bat;
        if (r < 60) return EnemyType::Wolf;
        return EnemyType::Skeleton;
    }
    if (map == 1) {
        if (r < 50) return EnemyType::Slime;
        if (r < 80) return EnemyType::Bat;
        return EnemyType::Wolf;
    }
    return EnemyType::Slime;
}

}  // namespace

const ShopItem SHOP[SHOP_COUNT] = {
    {0, 0, "Ramuan", 12},           {1, 0, "Eliksir", 30},
    {2, 1, "Pedang Perunggu", 60},  {3, 1, "Armor Kulit", 50},
    {2, 2, "Pedang Besi", 140},     {3, 2, "Baju Rantai", 130},
};

Game::Game(uint64_t seed, std::string savePath)
    : rng_(seed),
      enemyRng_(seed ^ 0xD1B54A32D192ED03ull),
      saveBase_(std::move(savePath)),
      savePath_(slotPath(saveBase_, 0)) {
    chestOpened_.resize(static_cast<size_t>(world_.count()));
    for (int i = 0; i < world_.count(); ++i)
        chestOpened_[static_cast<size_t>(i)].assign(world_.def(i).chests.size(), false);
    visited_.assign(static_cast<size_t>(world_.count()), false);
    map_ = 0;
    px_ = 14 * TILE;
    py_ = 18 * TILE;
    facing_ = 1;
    resetRoamingEnemies();
    updateCamera(100.0f);
}

void Game::markVisited(int map) {
    if (map < 0 || map >= static_cast<int>(visited_.size())) return;
    visited_[static_cast<size_t>(map)] = true;
}

bool Game::npcHidden(const NpcSpawn& npc) const {
    if (npc.style == NpcStyle::Warden && flags_.bossDefeated) return true;
    if (npc.style == NpcStyle::Guardian && flags_.ruinsCleared) return true;
    return false;
}

std::string Game::questObjective() const {
    if (!flags_.metElder) return "Bicara dengan Tetua Ardan di Desa Elmwood.";
    if (!flags_.bossDefeated) return "Cari Batu Matahari di Gua Bayangan.";
    if (!flags_.gameCompleted) return "Bawa Batu Matahari ke Tetua Ardan.";
    if (!flags_.ruinsCleared) return "Taklukkan Penjaga Fajar di Reruntuhan Fajar.";
    return "Semua misi selesai. Lembah Aetheria aman.";
}

std::string Game::sideQuestObjective() const {
    if (flags_.herbQuestDone) return "Selesai: Ramuan untuk Ruslan.";
    if (!flags_.herbQuestAccepted) return "Petani Ruslan di Elmwood punya permintaan.";
    return "Kumpulkan 3 Ramuan untuk Petani Ruslan (" + std::to_string(player_.potions) +
           "/3).";
}

bool Game::takeSettingsDirty() {
    bool dirty = settingsDirty_;
    settingsDirty_ = false;
    return dirty;
}

void Game::setAudio(Audio* audio) {
    audio_ = audio;
    battle_.setAudio(audio);
    updateMusic();
}

void Game::setUpdater(Updater* updater) { updater_ = updater; }

void Game::playSfx(SfxId id) {
    if (audio_) audio_->playSfx(id);
}

void Game::updateMusic() {
    if (!audio_) return;
    MusicId track = MusicId::None;
    switch (state_) {
        case GameState::Title:
            track = MusicId::Title;
            break;
        case GameState::Explore:
        case GameState::Dialogue:
        case GameState::Menu:
        case GameState::Shop:
            if (map_ == 1) track = MusicId::Overworld;
            else if (map_ == 2 || map_ == 3) track = MusicId::Cave;
            else track = MusicId::Village;
            break;
        case GameState::Battle:
            track = battle_.isBoss() ? MusicId::Boss : MusicId::Battle;
            break;
        case GameState::GameOver:
            track = MusicId::None;
            break;
        case GameState::Ending:
            track = MusicId::Ending;
            break;
    }
    if (track == MusicId::None) audio_->stopMusic();
    else audio_->playMusic(track);
}

void Game::menuFeedback(const InputState& in) {
    if (in.hit(Key::Up) || in.hit(Key::Down) || in.hit(Key::Left) || in.hit(Key::Right)) {
        playSfx(SfxId::MenuMove);
    }
    if (in.hit(Key::Confirm)) playSfx(SfxId::MenuSelect);
    if (in.hit(Key::Cancel)) playSfx(SfxId::MenuCancel);
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
    visited_.assign(static_cast<size_t>(world_.count()), false);
    markVisited(map_);
    resetRoamingEnemies(px_, py_);
    savePath_ = slotPath(saveBase_, 0);
    saveSlot_ = 0;
    titlePage_ = TitlePage::Main;
    saveConfirmSlot_ = -1;
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

bool Game::anySaveExists() const { return rpg::anySaveExists(saveBase_); }

std::string Game::slotSummary(int slot) const {
    SaveInfo info = inspectSave(slotPath(saveBase_, slot));
    if (!info.exists) return "Kosong";
    if (!info.valid) return "Berkas rusak";
    std::string mapName =
        (info.map >= 0 && info.map < world_.count()) ? world_.def(info.map).name : "?";
    return "Lv " + std::to_string(info.level) + "  " + mapName + "  " +
           std::to_string(info.gold) + "G";
}

bool Game::saveToSlot(int slot, std::string* err) {
    std::string path = slotPath(saveBase_, slot);
    if (!writeSaveTo(path, err)) return false;
    savePath_ = path;
    saveSlot_ = slot;
    return true;
}

bool Game::loadFromSlot(int slot, std::string* err) {
    std::string path = slotPath(saveBase_, slot);
    if (!readSaveFrom(path, err)) return false;
    savePath_ = path;
    saveSlot_ = slot;
    titlePage_ = TitlePage::Main;
    return true;
}

bool Game::autosave() {
    if (!flags_.introSeen) return false;
    return writeSaveTo(slotPath(saveBase_, -1), nullptr);
}

bool Game::saveGame(std::string* err) { return writeSaveTo(savePath_, err); }

bool Game::writeSaveTo(const std::string& path, std::string* err) {
    if (err) err->clear();
    int flagBits = 0;
    if (flags_.questAccepted) flagBits |= 1;
    if (flags_.bossDefeated) flagBits |= 2;
    if (flags_.sunstoneFound) flagBits |= 4;
    if (flags_.gameCompleted) flagBits |= 8;
    if (flags_.metElder) flagBits |= 16;
    if (flags_.introSeen) flagBits |= 32;

    int questBits = 0;
    if (flags_.herbQuestAccepted) questBits |= 1;
    if (flags_.herbQuestDone) questBits |= 2;
    if (flags_.ruinsCleared) questBits |= 4;
    int visitedMask = 0;
    for (size_t i = 0; i < visited_.size() && i < 31; ++i)
        if (visited_[i]) visitedMask |= (1 << i);

    std::ostringstream out;
    out << "version 2\n";
    out << "map " << map_ << "\n";
    out << "x " << px_ << "\n";
    out << "y " << py_ << "\n";
    out << "facing " << facing_ << "\n";
    out << "level " << player_.level << "\n";
    out << "xp " << player_.xp << "\n";
    out << "gold " << player_.gold << "\n";
    out << "hp " << player_.hp << "\n";
    out << "mp " << player_.mp << "\n";
    out << "weapon " << player_.weapon << "\n";
    out << "armor " << player_.armor << "\n";
    out << "potions " << player_.potions << "\n";
    out << "elixirs " << player_.elixirs << "\n";
    out << "sunstone " << (player_.hasSunstone ? 1 : 0) << "\n";
    out << "flags " << flagBits << "\n";
    out << "quests " << questBits << "\n";
    out << "visited " << visitedMask << "\n";
    out << "chests";
    for (int i = 0; i < world_.count(); ++i) {
        int mask = 0;
        const auto& opened = chestOpened_[static_cast<size_t>(i)];
        for (size_t k = 0; k < opened.size() && k < 31; ++k)
            if (opened[k]) mask |= (1 << k);
        out << " " << mask;
    }
    out << "\n";

    // writeSaveText encodes the payload, writes a temporary file, and replaces
    // the target atomically so a crash cannot leave a half-written save.
    return writeSaveText(path, out.str(), err);
}

bool Game::loadGame(std::string* err) { return readSaveFrom(savePath_, err); }

bool Game::readSaveFrom(const std::string& path, std::string* err) {
    if (err) err->clear();
    auto fail = [&](const char* msg) {
        if (err) *err = msg;
        return false;
    };
    std::ifstream f(path, std::ios::binary);
    if (!f) return fail("Tidak ada berkas simpanan.");
    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    bool legacy = !isEncodedSaveText(raw);
    std::string text;
    if (!decodeSaveText(raw, &text))
        return fail("Berkas simpanan rusak atau telah dimodifikasi.");

    std::map<std::string, std::string> kv;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
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

    int version = getInt("version", 0, 0, 999);
    if (version != 1 && version != 2) return fail("Versi simpanan tidak dikenal.");
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
    p.weapon = getInt("weapon", 0, 0, 3);
    p.armor = getInt("armor", 0, 0, 3);
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
    int questBits = getInt("quests", 0, 0, 7);
    flags_.herbQuestAccepted = (questBits & 1) != 0;
    flags_.herbQuestDone = (questBits & 2) != 0;
    if (flags_.herbQuestDone) flags_.herbQuestAccepted = true;
    flags_.ruinsCleared = (questBits & 4) != 0;

    map_ = newMap;
    px_ = nx;
    py_ = ny;
    facing_ = getInt("facing", 0, 0, 3);

    chestOpened_.assign(static_cast<size_t>(world_.count()), {});
    for (int i = 0; i < world_.count(); ++i)
        chestOpened_[static_cast<size_t>(i)].assign(world_.def(i).chests.size(), false);

    visited_.assign(static_cast<size_t>(world_.count()), false);
    auto vit = kv.find("visited");
    if (vit != kv.end()) {
        std::istringstream vs(vit->second);
        int mask = 0;
        if (vs >> mask) {
            for (size_t i = 0; i < visited_.size() && i < 31; ++i)
                visited_[i] = (mask & (1 << i)) != 0;
        }
    }
    markVisited(map_);

    auto cit = kv.find("chests");
    if (cit != kv.end()) {
        std::istringstream ss(cit->second);
        int mask = 0;
        for (int i = 0; i < world_.count() && (ss >> mask); ++i) {
            auto& opened = chestOpened_[static_cast<size_t>(i)];
            for (size_t k = 0; k < opened.size() && k < 31; ++k) opened[k] = (mask & (1 << k)) != 0;
        }
    }

    resetRoamingEnemies(px_, py_);
    enemyGrace_ = 2.0f;
    stepAccum_ = 0.0f;
    portalCooldown_ = 1.0f;
    interactCooldown_ = 0.0f;
    particles_.clear();
    floats_.clear();
    state_ = GameState::Explore;
    mapBanner_ = 3.0f;
    mapBannerText_ = world_.def(map_).name;
    updateCamera(100.0f);
    // Upgrade a plaintext save in place so it is protected from casual editing
    // from this load onward. A failed rewrite must not fail the load.
    if (legacy) writeSaveText(path, text, nullptr);
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
    markVisited(map_);
    px_ = static_cast<float>(spawnTileX * TILE);
    py_ = static_cast<float>(spawnTileY * TILE);
    clampPlayerToMap();
    facing_ = clampInt(facing, 0, 3);
    stepAccum_ = 0.0f;
    enemyGrace_ = 1.2f;
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
            if (npc && !npcHidden(*npc)) return true;
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

void Game::resetRoamingEnemies(float avoidX, float avoidY) {
    enemies_.clear();
    enemies_.resize(static_cast<size_t>(world_.count()));
    for (int map = 0; map < world_.count(); ++map) spawnRoamingEnemies(map, avoidX, avoidY);
    enemyGrace_ = 1.0f;
}

void Game::spawnRoamingEnemies(int map, float avoidX, float avoidY) {
    if (map < 0 || map >= world_.count()) return;
    int wanted = map == 1 ? 7 : (map == 2 ? 6 : (map == 3 ? 5 : 0));
    if (wanted <= 0) return;

    const MapDef& md = world_.def(map);
    struct Spot {
        int x, y;
    };
    std::vector<Spot> pool;
    for (int ty = 0; ty < md.h; ++ty) {
        for (int tx = 0; tx < md.w; ++tx) {
            // Monsters only live on encounter terrain, so the roads of the
            // overworld stay safe while tall grass and cave floors stay lively.
            if (!world_.encounter(map, tx, ty)) continue;
            if (world_.blocked(map, tx, ty)) continue;
            if (world_.npcAt(map, tx, ty) || world_.chestAt(map, tx, ty) ||
                world_.portalAt(map, tx, ty))
                continue;

            bool tooClose = false;
            for (const Portal& p : md.portals) {
                if (tx >= p.x - 3 && tx < p.x + p.w + 3 && ty >= p.y - 3 &&
                    ty < p.y + p.h + 3) {
                    tooClose = true;
                    break;
                }
            }
            // Keep every arrival point clear, including portals on other maps
            // that lead into this one.
            for (int other = 0; other < world_.count() && !tooClose; ++other) {
                for (const Portal& p : world_.def(other).portals) {
                    if (p.targetMap != map) continue;
                    if (std::abs(tx - p.spawnX) <= 4 && std::abs(ty - p.spawnY) <= 4) {
                        tooClose = true;
                        break;
                    }
                }
            }
            for (const NpcSpawn& n : md.npcs) {
                if (std::abs(tx - n.x) <= 2 && std::abs(ty - n.y) <= 2) {
                    tooClose = true;
                    break;
                }
            }
            for (const ChestSpawn& c : md.chests) {
                if (std::abs(tx - c.x) <= 1 && std::abs(ty - c.y) <= 1) {
                    tooClose = true;
                    break;
                }
            }
            // Only the map the player is standing on needs a clear pocket
            // around them; the same pixel offset on another map is unrelated.
            if (!tooClose && map == map_ && avoidX >= 0.0f && avoidY >= 0.0f) {
                float cx = static_cast<float>(tx * TILE + 8);
                float cy = static_cast<float>(ty * TILE + 8);
                if (std::abs(cx - avoidX) <= 4.0f * TILE &&
                    std::abs(cy - avoidY) <= 4.0f * TILE)
                    tooClose = true;
            }
            if (!tooClose) pool.push_back({tx, ty});
        }
    }

    for (size_t i = pool.size(); i > 1; --i) {
        size_t j = static_cast<size_t>(enemyRng_.range(0, static_cast<int>(i) - 1));
        std::swap(pool[i - 1], pool[j]);
    }

    std::vector<RoamingEnemy> spawned;
    for (const Spot& spot : pool) {
        bool crowded = false;
        for (const RoamingEnemy& other : spawned) {
            int ox = static_cast<int>(std::floor(other.x / TILE));
            int oy = static_cast<int>(std::floor(other.y / TILE));
            if (std::abs(ox - spot.x) < 3 && std::abs(oy - spot.y) < 3) {
                crowded = true;
                break;
            }
        }
        if (crowded) continue;

        RoamingEnemy e;
        e.type = rollRoamer(map, enemyRng_);
        e.x = e.homeX = static_cast<float>(spot.x * TILE);
        e.y = e.homeY = static_cast<float>(spot.y * TILE);
        e.anim = enemyRng_.unit() * 10.0f;
        e.moveTimer = enemyRng_.unit() * 1.8f;
        pickEnemyDirection(e);
        spawned.push_back(e);
        if (static_cast<int>(spawned.size()) >= wanted) break;
    }
    enemies_[static_cast<size_t>(map)] = std::move(spawned);
}

void Game::pickEnemyDirection(RoamingEnemy& e) {
    float hx = e.homeX - e.x;
    float hy = e.homeY - e.y;
    if (std::abs(hx) > 5.0f * TILE || std::abs(hy) > 5.0f * TILE) {
        // Too far from home: walk back along the dominant axis.
        if (std::abs(hx) > std::abs(hy)) {
            e.dirX = hx > 0.0f ? 1.0f : -1.0f;
            e.dirY = 0.0f;
        } else {
            e.dirX = 0.0f;
            e.dirY = hy > 0.0f ? 1.0f : -1.0f;
        }
        return;
    }
    static const float dirs[8][2] = {{1.0f, 0.0f},   {-1.0f, 0.0f},  {0.0f, 1.0f},
                                     {0.0f, -1.0f},  {0.7071f, 0.7071f},
                                     {0.7071f, -0.7071f}, {-0.7071f, 0.7071f},
                                     {-0.7071f, -0.7071f}};
    int d = enemyRng_.range(0, 7);
    e.dirX = dirs[d][0];
    e.dirY = dirs[d][1];
}

bool Game::enemyBlocked(int map, float nx, float ny, int self) const {
    // The centre tile must remain encounter terrain, which keeps overworld
    // monsters inside tall grass and cave monsters on the cave floor.
    if (!world_.encounter(map, tileOfX(nx + 8.0f), tileOfY(ny + 12.0f))) return true;

    int tx0 = tileOfX(nx + 3.0f), tx1 = tileOfX(nx + 12.99f);
    int ty0 = tileOfY(ny + 6.0f), ty1 = tileOfY(ny + 15.99f);
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            if (world_.blocked(map, tx, ty)) return true;
            const NpcSpawn* npc = world_.npcAt(map, tx, ty);
            if (npc && !npcHidden(*npc)) return true;
            if (world_.chestAt(map, tx, ty)) return true;
            if (world_.portalAt(map, tx, ty)) return true;
        }
    }
    if (map < 0 || map >= static_cast<int>(enemies_.size())) return false;
    const auto& list = enemies_[static_cast<size_t>(map)];
    for (size_t i = 0; i < list.size(); ++i) {
        if (static_cast<int>(i) == self || !list[i].active) continue;
        if (std::abs(list[i].x - nx) < 10.0f && std::abs(list[i].y - ny) < 10.0f)
            return true;
    }
    return false;
}

void Game::updateEnemies(float dt) {
    if (map_ < 0 || map_ >= static_cast<int>(enemies_.size())) return;
    auto& list = enemies_[static_cast<size_t>(map_)];
    for (size_t i = 0; i < list.size(); ++i) {
        RoamingEnemy& e = list[i];
        if (!e.active) {
            e.respawnTimer -= dt;
            if (e.respawnTimer > 0.0f) continue;
            // Never materialise on top of the player.
            if (std::abs(e.homeX - px_) < 3.0f * TILE &&
                std::abs(e.homeY - py_) < 3.0f * TILE) {
                e.respawnTimer = 2.0f;
                continue;
            }
            e.x = e.homeX;
            e.y = e.homeY;
            e.active = true;
            e.pauseTimer = 0.0f;
            e.moveTimer = 0.4f;
            e.anim += 1.0f;
            spawnBurst(e.x + 8.0f, e.y + 8.0f, 10, 0xFFB080FFu, 22.0f);
            continue;
        }

        e.anim += dt;
        if (e.pauseTimer > 0.0f) {
            e.pauseTimer -= dt;
            continue;
        }
        e.moveTimer -= dt;
        if (e.moveTimer <= 0.0f) {
            e.moveTimer = 1.1f + enemyRng_.unit() * 2.2f;
            if (enemyRng_.chance(0.22f)) {
                e.dirX = 0.0f;
                e.dirY = 0.0f;
                e.pauseTimer = 0.35f + enemyRng_.unit() * 0.9f;
                continue;
            }
            pickEnemyDirection(e);
        }
        if (e.dirX == 0.0f && e.dirY == 0.0f) continue;
        float speed = static_cast<float>(roamSpeed(e.type));
        float nx = e.x + e.dirX * speed * dt;
        if (!enemyBlocked(map_, nx, e.y, static_cast<int>(i))) {
            e.x = nx;
        } else {
            e.moveTimer = 0.0f;
        }
        float ny = e.y + e.dirY * speed * dt;
        if (!enemyBlocked(map_, e.x, ny, static_cast<int>(i))) {
            e.y = ny;
        } else {
            e.moveTimer = 0.0f;
        }
    }
}

void Game::checkEnemyContact() {
    if (enemyGrace_ > 0.0f || transition_) return;
    if (map_ < 0 || map_ >= static_cast<int>(enemies_.size())) return;
    auto& list = enemies_[static_cast<size_t>(map_)];
    float px0 = px_ + 4.0f, py0 = py_ + 9.0f;
    float px1 = px_ + 11.99f, py1 = py_ + 15.99f;
    for (size_t i = 0; i < list.size(); ++i) {
        const RoamingEnemy& e = list[i];
        if (!e.active) continue;
        float ex0 = e.x + 2.0f, ey0 = e.y + 5.0f;
        float ex1 = e.x + 13.99f, ey1 = e.y + 14.99f;
        if (px0 < ex1 && ex0 < px1 && py0 < ey1 && ey0 < py1) {
            startEnemyBattle(static_cast<int>(i));
            return;
        }
    }
}

void Game::startEnemyBattle(int index) {
    if (transition_) return;
    if (map_ < 0 || map_ >= static_cast<int>(enemies_.size())) return;
    auto& list = enemies_[static_cast<size_t>(map_)];
    if (index < 0 || index >= static_cast<int>(list.size())) return;
    RoamingEnemy& e = list[static_cast<size_t>(index)];
    if (!e.active) return;

    EnemyType type = e.type;
    e.active = false;
    e.respawnTimer = 18.0f + enemyRng_.unit() * 10.0f;
    enemyGrace_ = 1.6f;
    spawnBurst(e.x + 8.0f, e.y + 8.0f, 12, 0xFFFFD8A0u, 26.0f);
    playSfx(SfxId::Attack);
    uint64_t seed = (static_cast<uint64_t>(enemyRng_.next()) << 32) ^ enemyRng_.next();
    startTransition([this, type, seed] {
        state_ = GameState::Battle;
        battle_.begin(player_, type, false, seed);
    });
}

void Game::onStep() { playSfx(SfxId::Step); }

void Game::checkPortal() {
    if (portalCooldown_ > 0.0f) return;
    int tx = tileOfX(px_ + 8.0f);
    int ty = tileOfY(py_ + 13.0f);
    const Portal* portal = world_.portalAt(map_, tx, ty);
    if (!portal) return;
    if (portal->requiresSunstone && !player_.hasSunstone) {
        portalCooldown_ = 1.8f;
        playSfx(SfxId::MenuCancel);
        startDialogue("Segel Fajar",
                      {"Sebuah segel cahaya menutup jalan ini.",
                       "Hanya pembawa Batu Matahari yang dapat membukanya."});
        return;
    }
    int target = portal->targetMap, sx = portal->spawnX, sy = portal->spawnY, sd = portal->spawnDir;
    portalCooldown_ = 1.4f;
    playSfx(SfxId::Portal);
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
        case ItemId::DawnSword:
            if (player_.weapon < 3) {
                player_.weapon = 3;
                msg = "Kau menemukan Pedang Fajar! Cahayanya membelah bayangan.";
            } else {
                player_.gold += 200;
                msg = "Pedang Fajar cadangan. Dijual +200 emas.";
            }
            spawnBurst(cx, cy, 20, 0xFFFFD87A, 36.0f);
            break;
        case ItemId::DawnShield:
            if (player_.armor < 3) {
                player_.armor = 3;
                msg = "Kau menemukan Perisai Fajar! Pertahananmu bersinar.";
            } else {
                player_.gold += 200;
                msg = "Perisai Fajar cadangan. Dijual +200 emas.";
            }
            spawnBurst(cx, cy, 20, 0xFFFFD87A, 36.0f);
            break;
        default:
            msg = "Peti kosong.";
            break;
    }
    playSfx(chest.item == ItemId::Gold ? SfxId::Coin : SfxId::Chest);
    startDialogue("Peti Harta", {msg}, [this] { state_ = GameState::Explore; });
}

void Game::interact() {
    int dx = facing_ == 2 ? -1 : (facing_ == 3 ? 1 : 0);
    int dy = facing_ == 0 ? 1 : (facing_ == 1 ? -1 : 0);
    int tx = tileOfX(px_ + 8.0f) + dx;
    int ty = tileOfY(py_ + 12.0f) + dy;

    const NpcSpawn* npc = world_.npcAt(map_, tx, ty);
    if (npc) {
        if (npcHidden(*npc)) return;
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
            case NpcStyle::Guardian: guardianTalk(); break;
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
    } else if (t == Tile::RuinEntrance) {
        if (player_.hasSunstone) {
            startDialogue("Reruntuhan Fajar", {"Batu Matahari bergetar di tasmu.",
                                               "Segel kuno terbuka. Tangga menuju cahaya fajar."});
        } else {
            startDialogue("Reruntuhan Fajar", {"Reruntuhan tertutup segel cahaya.",
                                               "Cahaya itu menolak siapa pun tanpa Batu Matahari."});
        }
    }
}

void Game::elderTalk() {
    if (!flags_.metElder) {
        flags_.metElder = true;
        flags_.questAccepted = true;
        autosave();
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
        autosave();
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
    playSfx(SfxId::Heal);
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

void Game::guardianTalk() {
    if (flags_.ruinsCleared) return;
    uint64_t seed = rng_.next();
    startDialogue("Penjaga Fajar",
                  {"Aku telah menunggu seribu tahun di bawah sinar yang padam.",
                   "Batu Matahari di tasmu... akhirnya cahaya itu kembali.",
                   "Tetapi aku tidak akan menyerahkannya tanpa ujian.",
                   "Angkat senjatamu, pahlawan. Buktikan kau layak membawa fajar."},
                  [this, seed] {
                      startTransition([this, seed] {
                          state_ = GameState::Battle;
                          battle_.begin(player_, EnemyType::Guardian, true, seed);
                      });
                  });
}

void Game::flavorTalk(int dialogueId) {
    switch (dialogueId) {
        case 4:
            startDialogue("Anak Desa",
                          {"Monster hijau berkeliaran di rumput tinggi!",
                           "Kalau kau menyentuhnya, pertarungan langsung dimulai."});
            break;
        case 5:
            if (flags_.herbQuestDone) {
                startDialogue("Petani Ruslan",
                              {"Ramuanmu menyelamatkan panenku. Terima kasih, pahlawan!",
                               "Lembah ini sungguh beruntung memilikimu."});
            } else if (!flags_.herbQuestAccepted) {
                flags_.herbQuestAccepted = true;
                startDialogue("Petani Ruslan",
                              {"Panen tahun ini buruk. Tanah terasa dingin sejak Warden datang.",
                               "Aku butuh 3 Ramuan untuk merawat bibit-bibit yang tersisa.",
                               "Bawa ke sini, dan kubayar 120 emas plus satu Eliksir."});
            } else if (player_.potions >= 3) {
                player_.potions -= 3;
                player_.gold += 120;
                player_.elixirs = std::min(99, player_.elixirs + 1);
                flags_.herbQuestDone = true;
                autosave();
                playSfx(SfxId::LevelUp);
                spawnBurst(px_ + 8.0f, py_ + 8.0f, 18, pal::gold, 30.0f);
                addFloat(px_ + 8.0f, py_ - 4.0f, "+120G", pal::gold);
                startDialogue("Petani Ruslan",
                              {"Ketiga Ramuan ini akan menyelamatkan musim panen!",
                               "Terimalah 120 emas dan sebuah Eliksir. Kau teman sejati.",
                               "Misi sampingan selesai: Ramuan untuk Ruslan."});
            } else {
                std::string progress =
                    "Ramuan terkumpul: " + std::to_string(player_.potions) + "/3.";
                startDialogue("Petani Ruslan",
                              {"Bibit-bibitku masih lemah. Aku menunggu Ramuanmu.",
                               progress, "Toko Rilla menjual Ramuan bila persediaanmu habis."});
            }
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
    saveConfirmSlot_ = -1;
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
            playSfx(SfxId::Potion);
        }
    } else {
        if (player_.potions <= 0) {
            menuMessage_ = "Kau tidak punya Ramuan.";
        } else if (player_.hp >= player_.maxHp) {
            menuMessage_ = "HP sudah penuh.";
        } else {
            player_.usePotion();
            menuMessage_ = "Ramuan digunakan. HP pulih.";
            playSfx(SfxId::Potion);
        }
    }
    if (menuMessage_.find("digunakan") == std::string::npos) playSfx(SfxId::MenuCancel);
    menuMessageTimer_ = 2.4f;
}

void Game::saveFromMenu(int slot) {
    std::string path = slotPath(saveBase_, slot);
    SaveInfo info = inspectSave(path);
    if (info.valid && saveConfirmSlot_ != slot) {
        saveConfirmSlot_ = slot;
        menuMessage_ = slotLabel(slot) + " sudah terisi. Tekan Z lagi untuk menimpa.";
        menuMessageTimer_ = 3.0f;
        playSfx(SfxId::MenuCancel);
        return;
    }
    std::string err;
    if (saveToSlot(slot, &err)) {
        menuMessage_ = "Permainan disimpan di " + slotLabel(slot) + ".";
        saveConfirmSlot_ = -1;
        playSfx(SfxId::Save);
    } else {
        menuMessage_ = err.empty() ? "Gagal menyimpan." : err;
        playSfx(SfxId::MenuCancel);
    }
    menuMessageTimer_ = 3.0f;
}

void Game::updateTitle(float dt, const InputState& in) {
    titleTime_ += dt;
    titleMsgTimer_ = std::max(0.0f, titleMsgTimer_ - dt);

    // The worker thread reports completion, not the menu, so poll it every
    // frame even when the player is only watching the progress line.
    if (updater_ && titleUpdateActive_ && !updater_->running()) {
        titleUpdateActive_ = false;
        if (updater_->success()) {
            titleIndex_ = 2;
            titleMsg_ = "Pembaruan selesai. Tekan Z untuk mulai ulang.";
            titleMsgTimer_ = 0.0f;
        } else {
            titleMsg_ = updater_->errorText();
            if (titleMsg_.empty()) titleMsg_ = updater_->statusText();
            titleMsgTimer_ = 4.0f;
        }
    }

    // Keep the menu still while git and make are working so the player cannot
    // leave the title screen in the middle of an update.
    if (titleUpdateActive_ && updater_ && updater_->running()) return;

    menuFeedback(in);

    if (titlePage_ == TitlePage::Load) {
        if (in.hit(Key::Up)) titleSlotIndex_ = (titleSlotIndex_ + 4) % 5;
        if (in.hit(Key::Down)) titleSlotIndex_ = (titleSlotIndex_ + 1) % 5;
        if (in.hit(Key::Cancel)) {
            titlePage_ = TitlePage::Main;
            titleIndex_ = 1;
            return;
        }
        if (!in.hit(Key::Confirm)) return;
        if (titleSlotIndex_ == 4) {
            titlePage_ = TitlePage::Main;
            titleIndex_ = 1;
            return;
        }
        const int slots[4] = {-1, 0, 1, 2};
        int slot = slots[titleSlotIndex_];
        std::string err;
        if (!loadFromSlot(slot, &err)) {
            titleMsg_ = err.empty() ? "Slot itu kosong." : err;
            titleMsgTimer_ = 2.5f;
        } else {
            titleMsg_.clear();
            titleMsgTimer_ = 0.0f;
        }
        return;
    }

    if (titlePage_ == TitlePage::ConfirmNew) {
        if (in.hit(Key::Up) || in.hit(Key::Down)) titleConfirmIndex_ = 1 - titleConfirmIndex_;
        if (in.hit(Key::Cancel)) {
            titlePage_ = TitlePage::Main;
            titleIndex_ = 0;
            return;
        }
        if (!in.hit(Key::Confirm)) return;
        if (titleConfirmIndex_ == 0) {
            newGame();
        } else {
            titlePage_ = TitlePage::Main;
            titleIndex_ = 0;
        }
        return;
    }

    if (in.hit(Key::Up)) titleIndex_ = (titleIndex_ + 3) % 4;
    if (in.hit(Key::Down)) titleIndex_ = (titleIndex_ + 1) % 4;
    if (!in.hit(Key::Confirm)) return;

    if (titleIndex_ == 0) {
        if (anySaveExists()) {
            titlePage_ = TitlePage::ConfirmNew;
            titleConfirmIndex_ = 1;
        } else {
            newGame();
        }
    } else if (titleIndex_ == 1) {
        if (!anySaveExists()) {
            titleMsg_ = "Belum ada simpanan. Mulai permainan baru dulu.";
            titleMsgTimer_ = 2.8f;
        } else {
            titlePage_ = TitlePage::Load;
            titleSlotIndex_ = 1;
        }
    } else if (titleIndex_ == 2) {
        if (!updater_) {
            titleMsg_ = "Pembaruan tidak tersedia di mode ini.";
            titleMsgTimer_ = 3.0f;
        } else if (updater_->finished() && updater_->success()) {
            restart_ = true;
        } else {
            std::string err;
            if (updater_->start(&err)) {
                titleUpdateActive_ = true;
                titleMsg_.clear();
                titleMsgTimer_ = 0.0f;
            } else {
                titleMsg_ = err.empty() ? "Pembaruan gagal dimulai." : err;
                titleMsgTimer_ = 4.0f;
            }
        }
    } else {
        quit_ = true;
    }
}

void Game::updateExplore(float dt, const InputState& in) {
    portalCooldown_ = std::max(0.0f, portalCooldown_ - dt);
    interactCooldown_ = std::max(0.0f, interactCooldown_ - dt);
    enemyGrace_ = std::max(0.0f, enemyGrace_ - dt);
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
    if (transition_) return;
    checkPortal();
    if (transition_) return;
    updateEnemies(dt);
    checkEnemyContact();
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
    playSfx(SfxId::MenuMove);
    if (dlg_.page >= static_cast<int>(dlg_.pages.size())) closeDialogue();
}

void Game::updateBattle(float dt, const InputState& in) {
    battle_.update(dt, in, player_);
    if (!battle_.done()) return;

    if (battle_.victory()) {
        if (battle_.isBoss() && battle_.enemyType() == EnemyType::Warden &&
            !flags_.bossDefeated) {
            flags_.bossDefeated = true;
            flags_.sunstoneFound = true;
            player_.hasSunstone = true;
            autosave();
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
        } else if (battle_.isBoss() && battle_.enemyType() == EnemyType::Guardian &&
                   !flags_.ruinsCleared) {
            flags_.ruinsCleared = true;
            autosave();
            state_ = GameState::Dialogue;
            startDialogue("Penjaga Fajar",
                          {"Kau... membawa cahaya yang sesungguhnya.",
                           "Aku telah lama terjaga tanpa tujuan. Kini tugasku selesai.",
                           "Ambil perbekalan di reruntuhan ini. Lembah Aetheria aman."},
                          [this] {
                              state_ = GameState::Explore;
                              mapBanner_ = 3.0f;
                              mapBannerText_ = "Reruntuhan Fajar ditaklukkan!";
                              spawnBurst(px_ + 8.0f, py_ + 8.0f, 26, 0xFFFFD87A, 40.0f);
                          });
        } else {
            state_ = GameState::Explore;
        }
    } else if (battle_.fled()) {
        state_ = GameState::Explore;
    } else {
        state_ = GameState::GameOver;
        gameOverIndex_ = 0;
    }
    flash_ = 0.0f;
    shake_ = 0.0f;
}

void Game::updateMenu(float dt, const InputState& in) {
    menuMessageTimer_ = std::max(0.0f, menuMessageTimer_ - dt);
    menuFeedback(in);
    if (menuPage_ == 0) {
        if (in.hit(Key::Up)) menuIndex_ = (menuIndex_ + 5) % 6;
        if (in.hit(Key::Down)) menuIndex_ = (menuIndex_ + 1) % 6;
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
                menuIndex_ = 0;
            } else if (menuIndex_ == 2) {
                menuPage_ = 3;
                menuIndex_ = 0;
            } else if (menuIndex_ == 3) {
                menuPage_ = 4;
                saveConfirmSlot_ = -1;
                menuIndex_ = clampInt(saveSlot_ + 1, 1, 3);
            } else if (menuIndex_ == 4) {
                menuPage_ = 5;
                menuIndex_ = 0;
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
    } else if (menuPage_ == 2) {
        if (in.hit(Key::Cancel) || in.hit(Key::Confirm)) {
            menuPage_ = 0;
            menuIndex_ = 1;
        }
    } else if (menuPage_ == 3) {
        if (in.hit(Key::Cancel) || in.hit(Key::Confirm)) {
            menuPage_ = 0;
            menuIndex_ = 2;
        }
    } else if (menuPage_ == 4) {
        if (in.hit(Key::Up)) {
            menuIndex_ = (menuIndex_ + 4) % 5;
            saveConfirmSlot_ = -1;
        }
        if (in.hit(Key::Down)) {
            menuIndex_ = (menuIndex_ + 1) % 5;
            saveConfirmSlot_ = -1;
        }
        if (in.hit(Key::Cancel)) {
            menuPage_ = 0;
            menuIndex_ = 3;
            saveConfirmSlot_ = -1;
            return;
        }
        if (in.hit(Key::Confirm)) {
            if (menuIndex_ == 4) {
                menuPage_ = 0;
                menuIndex_ = 3;
                saveConfirmSlot_ = -1;
            } else if (menuIndex_ == 0) {
                menuMessage_ = "Autosave ditulis otomatis di titik cerita.";
                menuMessageTimer_ = 3.0f;
                playSfx(SfxId::MenuCancel);
            } else {
                saveFromMenu(menuIndex_ - 1);
            }
        }
    } else {
        bool changed = false;
        if (audio_) {
            if (in.hit(Key::Up)) menuIndex_ = (menuIndex_ + 3) % 4;
            if (in.hit(Key::Down)) menuIndex_ = (menuIndex_ + 1) % 4;
            if (in.hit(Key::Left) || in.hit(Key::Right)) {
                float delta = in.hit(Key::Left) ? -0.1f : 0.1f;
                if (menuIndex_ == 0) {
                    audio_->setMusicVolume(audio_->musicVolume() + delta);
                    changed = true;
                } else if (menuIndex_ == 1) {
                    audio_->setSfxVolume(audio_->sfxVolume() + delta);
                    changed = true;
                }
            }
            if (in.hit(Key::Confirm)) {
                if (menuIndex_ == 2) {
                    audio_->toggleMute();
                    changed = true;
                } else if (menuIndex_ == 3) {
                    menuPage_ = 0;
                    menuIndex_ = 4;
                }
            }
            if (in.hit(Key::Cancel)) {
                menuPage_ = 0;
                menuIndex_ = 4;
            }
        } else if (in.hit(Key::Cancel) || in.hit(Key::Confirm)) {
            menuPage_ = 0;
            menuIndex_ = 4;
        }
        if (changed) settingsDirty_ = true;
    }
}

void Game::updateShop(float dt, const InputState& in) {
    shopMessageTimer_ = std::max(0.0f, shopMessageTimer_ - dt);
    menuFeedback(in);
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
        playSfx(SfxId::MenuCancel);
        return;
    }
    int goldBefore = player_.gold;
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
    playSfx(player_.gold < goldBefore ? SfxId::Coin : SfxId::MenuCancel);
    shopMessageTimer_ = 2.2f;
}

void Game::updateGameOver(float dt, const InputState& in) {
    (void)dt;
    menuFeedback(in);
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
        titlePage_ = TitlePage::Main;
    }
}

void Game::updateEnding(float dt, const InputState& in) {
    endingScroll_ += dt * 22.0f;
    if (in.hit(Key::Confirm) && endingScroll_ > 100.0f) playSfx(SfxId::MenuSelect);
    if (in.hit(Key::Confirm) && endingScroll_ > 100.0f) {
        state_ = GameState::Title;
        titleIndex_ = 0;
        titlePage_ = TitlePage::Main;
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
        updateMusic();
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
    updateMusic();
}

void Game::debugWarp(int map, int tx, int ty) {
    if (map < 0 || map >= world_.count()) return;
    map_ = map;
    markVisited(map);
    px_ = static_cast<float>(tx * TILE);
    py_ = static_cast<float>(ty * TILE);
    clampPlayerToMap();
    updateCamera(100.0f);
    state_ = GameState::Explore;
    portalCooldown_ = 1.0f;
    enemyGrace_ = 0.0f;
    mapBanner_ = 2.5f;
    mapBannerText_ = world_.def(map_).name;
}

void Game::debugSetPlayerPos(float x, float y) {
    px_ = x;
    py_ = y;
    clampPlayerToMap();
    updateCamera(100.0f);
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

int Game::debugEnemyCount() const {
    if (map_ < 0 || map_ >= static_cast<int>(enemies_.size())) return 0;
    return static_cast<int>(enemies_[static_cast<size_t>(map_)].size());
}

const RoamingEnemy& Game::debugEnemy(int index) const {
    static const RoamingEnemy dummy{};
    if (map_ < 0 || map_ >= static_cast<int>(enemies_.size())) return dummy;
    const auto& list = enemies_[static_cast<size_t>(map_)];
    if (index < 0 || index >= static_cast<int>(list.size())) return dummy;
    return list[static_cast<size_t>(index)];
}

}  // namespace rpg
