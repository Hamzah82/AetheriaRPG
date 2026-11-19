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
        case EnemyType::GukAbu: return 30;
        case EnemyType::ArwahGema: return 18;
        case EnemyType::PenambangPualam: return 12;
        case EnemyType::Koral: return 8;
        case EnemyType::KumbangPualam: return 10;
        default: return 11;
    }
}

EnemyType rollRoamer(int map, Rng& rng) {
    int r = rng.range(0, 99);
    if (map == 8) {
        if (r < 40) return EnemyType::ArwahGema;
        if (r < 75) return EnemyType::PenambangPualam;
        return EnemyType::GukAbu;
    }
    if (map == 3) {
        if (r < 35) return EnemyType::ArwahGema;
        if (r < 70) return EnemyType::PenambangPualam;
        return EnemyType::Koral;
    }
    if (map == 4) {
        if (r < 35) return EnemyType::ArwahGema;
        if (r < 70) return EnemyType::PenambangPualam;
        return EnemyType::Koral;
    }
    if (map == 2) {
        if (r < 40) return EnemyType::KumbangPualam;
        if (r < 75) return EnemyType::GukAbu;
        return EnemyType::Koral;
    }
    return EnemyType::KumbangPualam;
}

}  // namespace

const ShopItem SHOP[SHOP_COUNT] = {
    {0, 0, "Ramuan Pelita", 14},   {1, 0, "Eliksir Sumsum", 34},
    {2, 0, "Minyak Sumsum", 8},    {3, 1, "Belati Besi", 80},
    {4, 1, "Jas Kulit", 60},       {3, 2, "Belati Penarah Jiwa", 180},
    {4, 2, "Baju Rantai", 150},
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
    map_ = world_.indexOf("village");
    if (map_ < 0) map_ = 0;
    px_ = 14 * TILE;
    py_ = 16 * TILE;
    facing_ = 1;
    resetRoamingEnemies();
    updateCamera(100.0f);
}

int Game::memoryCount() const {
    int n = 0;
    for (int i = 0; i < 12; ++i)
        if (flags_.memories & (1 << i)) ++n;
    return n;
}

bool Game::hasMemory(int id) const {
    return id >= 0 && id < 12 && (flags_.memories & (1 << id)) != 0;
}

int Game::companionCount() const { return static_cast<int>(companions_.size()); }

const Companion& Game::companion(int index) const {
    static const Companion dummy = makeCompanion(CompanionId::Sekar, 1);
    if (index < 0 || index >= static_cast<int>(companions_.size())) return dummy;
    return companions_[static_cast<size_t>(index)];
}

void Game::markVisited(int map) {
    if (map < 0 || map >= static_cast<int>(visited_.size())) return;
    visited_[static_cast<size_t>(map)] = true;
}

bool Game::npcHidden(const NpcSpawn& npc) const {
    switch (npc.style) {
        case NpcStyle::Bima: return flags_.bimaJoined;
        case NpcStyle::Sekar: return flags_.sekarJoined;
        case NpcStyle::Larasati: return flags_.larasJoined;
        case NpcStyle::Bagas: return flags_.bagasJoined;
        case NpcStyle::Warden: return flags_.fatherMemory;
        case NpcStyle::Guardian:
            if (map_ == 4) return flags_.gajaminaDefeated;
            if (map_ == 8) return flags_.gemaDefeated;
            return false;
        case NpcStyle::Guntur: return flags_.gunturDefeated;
        case NpcStyle::Adipati: return flags_.gameCompleted;
        case NpcStyle::Seno:
            if (map_ == 7) return !(flags_.act >= 4 || flags_.senoDefect);
            return false;
        default: return false;
    }
}

std::string Game::questObjective() const {
    if (!flags_.introSeen) return "Bicara dengan Mbok Ratih di Warung Asap.";
    switch (flags_.act) {
        case 0:
            if (!flags_.courierSaved) return "Bicara dengan Mbok Ratih di Warung Asap.";
            return "Berlayar ke Pelabuhan Besi Kertarajasa.";
        case 1:
            if (!flags_.bimaJoined) return "Temui Bima di bengkel Kertarajasa.";
            if (!flags_.sekarJoined) return "Temui Kapten Sekar di dermaga.";
            if (!flags_.larasJoined) return "Temukan Larasati di distrik bawah.";
            if (!hasMemory(1) || !hasMemory(2))
                return "Baca gema di gudang minyak Kertarajasa.";
            return "Berlayar ke Candi Tenggelam Saptatirta.";
        case 2:
            if (!flags_.bagasJoined) return "Temui Kakek Bagas di Candi Saptatirta.";
            if (!flags_.fatherMemory) return "Selidiki Sumur Ratapan.";
            return "Kembali ke Kertarajasa.";
        case 3:
            if (!flags_.nirmalaTruth) return "Hadapi Bunda Nirmala di Biara Pelita.";
            if (!flags_.kiranaHelped) return "Temui Kirana di Kraton Pualam Surya.";
            if (!flags_.gunturDefeated) return "Hentikan Raden Guntur.";
            return "Berlayar ke Jantung Palung Sunyi.";
        case 4:
            return "Tutup retakan di Jantung Palung Sunyi.";
        default:
            return "Nusakalpa menunggu.";
    }
}

std::string Game::sideQuestObjective() const {
    if (flags_.lanternQuestDone) return "Selesai: Lentera Nenek Sari.";
    if (flags_.lanternQuestAccepted) return "Temukan lentera di Rimba Karang-Kelabu.";
    if (flags_.herbQuestDone) return "Selesai: Ramuan untuk Petani.";
    if (flags_.herbQuestAccepted)
        return "Bawa 3 Ramuan Pelita (" + std::to_string(player_.potions) + "/3).";
    if (flags_.hiddenQuestDone) return "Selesai: Nyanyian Taman Lentera Mati.";
    if (flags_.gajaminaDefeated) return "Selesai: Gajamina Pualam.";
    return "Misi sampingan dapat ditemukan di desa dan kota.";
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

void Game::playSfx(SfxId id) {
    if (audio_) audio_->playSfx(id);
}

void Game::updateMusic() {
    if (!audio_) return;
    MusicId track = MusicId::None;
    switch (state_) {
        case GameState::Title: track = MusicId::Title; break;
        case GameState::Explore:
        case GameState::Dialogue:
        case GameState::Menu:
        case GameState::Shop:
            if (map_ == 2 || map_ == 3 || map_ == 4 || map_ == 8) track = MusicId::Cave;
            else if (map_ == 1 || map_ == 7) track = MusicId::Overworld;
            else track = MusicId::Village;
            break;
        case GameState::Battle:
            track = battle_.isBoss() ? MusicId::Boss : MusicId::Battle;
            break;
        case GameState::GameOver: track = MusicId::None; break;
        case GameState::Ending: track = MusicId::Ending; break;
    }
    if (track == MusicId::None) audio_->stopMusic();
    else audio_->playMusic(track);
}

void Game::menuFeedback(const InputState& in) {
    if (in.hit(Key::Up) || in.hit(Key::Down) || in.hit(Key::Left) || in.hit(Key::Right))
        playSfx(SfxId::MenuMove);
    if (in.hit(Key::Confirm)) playSfx(SfxId::MenuSelect);
    if (in.hit(Key::Cancel)) playSfx(SfxId::MenuCancel);
}

void Game::newGame() {
    player_ = Player();
    flags_ = Flags();
    companions_.clear();
    followers_.clear();
    map_ = world_.indexOf("village");
    if (map_ < 0) map_ = 0;
    px_ = 14 * TILE;
    py_ = 16 * TILE;
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
                  {"Nusakalpa: kepulauan garam di bawah langit kelabu.",
                   "Ingatan adalah bahan bakar terakhir, dan Raka Damar adalah Juru Gema.",
                   "Di Desa Tambak-Abu, pelita terakhir sedang redup."},
                  [this] {
                      flags_.introSeen = true;
                      state_ = GameState::Explore;
                  });
}

bool Game::hasSave() const {
    std::ifstream f(savePath_, std::ios::binary);
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
    std::ostringstream out;
    out << "version 3\n";
    out << "map " << map_ << "\n";
    out << "x " << px_ << "\n";
    out << "y " << py_ << "\n";
    out << "facing " << facing_ << "\n";
    out << "level " << player_.level << "\n";
    out << "xp " << player_.xp << "\n";
    out << "gold " << player_.gold << "\n";
    out << "hp " << player_.hp << "\n";
    out << "mp " << player_.mp << "\n";
    out << "oil " << player_.oil << "\n";
    out << "salt " << player_.saltCrystal << "\n";
    out << "weapon " << player_.weapon << "\n";
    out << "armor " << player_.armor << "\n";
    out << "potions " << player_.potions << "\n";
    out << "elixirs " << player_.elixirs << "\n";
    out << "resonance " << flags_.resonance << "\n";
    out << "act " << flags_.act << "\n";
    out << "stage " << flags_.stage << "\n";
    out << "memories " << flags_.memories << "\n";
    out << "ending " << flags_.endingId << "\n";
    out << "senoChoice " << flags_.senoChoice << "\n";
    out << "sekarChoice " << flags_.sekarChoice << "\n";
    out << "f_intro " << (flags_.introSeen ? 1 : 0) << "\n";
    out << "f_courier " << (flags_.courierSaved ? 1 : 0) << "\n";
    out << "f_bima " << (flags_.bimaJoined ? 1 : 0) << "\n";
    out << "f_sekar " << (flags_.sekarJoined ? 1 : 0) << "\n";
    out << "f_laras " << (flags_.larasJoined ? 1 : 0) << "\n";
    out << "f_bagas " << (flags_.bagasJoined ? 1 : 0) << "\n";
    out << "f_father " << (flags_.fatherMemory ? 1 : 0) << "\n";
    out << "f_nirmala " << (flags_.nirmalaTruth ? 1 : 0) << "\n";
    out << "f_kirana " << (flags_.kiranaHelped ? 1 : 0) << "\n";
    out << "f_seno " << (flags_.senoDefect ? 1 : 0) << "\n";
    out << "f_guntur " << (flags_.gunturDefeated ? 1 : 0) << "\n";
    out << "f_gema " << (flags_.gemaDefeated ? 1 : 0) << "\n";
    out << "f_complete " << (flags_.gameCompleted ? 1 : 0) << "\n";
    out << "q_herb " << (flags_.herbQuestAccepted ? 1 : 0) << "\n";
    out << "q_herbDone " << (flags_.herbQuestDone ? 1 : 0) << "\n";
    out << "q_lantern " << (flags_.lanternQuestAccepted ? 1 : 0) << "\n";
    out << "q_lanternDone " << (flags_.lanternQuestDone ? 1 : 0) << "\n";
    out << "q_sekar " << (flags_.sekarQuestAccepted ? 1 : 0) << "\n";
    out << "q_sekarDone " << (flags_.sekarQuestDone ? 1 : 0) << "\n";
    out << "q_hidden " << (flags_.hiddenQuestDone ? 1 : 0) << "\n";
    out << "q_gajamina " << (flags_.gajaminaDefeated ? 1 : 0) << "\n";
    out << "hasAshCharm " << (player_.hasAshCharm ? 1 : 0) << "\n";
    out << "hasRing " << (player_.hasResonanceRing ? 1 : 0) << "\n";
    out << "hasSaltNoSorrow " << (player_.hasSaltNoSorrow ? 1 : 0) << "\n";
    out << "hasKidung " << (player_.hasKidungBook ? 1 : 0) << "\n";
    out << "hasTirta " << (player_.hasTirtaCore ? 1 : 0) << "\n";
    out << "hasCourtKey " << (player_.hasCourtKey ? 1 : 0) << "\n";
    for (const auto& c : companions_)
        out << "comp " << static_cast<int>(c.id) << " " << c.level << " " << c.hp << " "
            << (c.recruited ? 1 : 0) << "\n";
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
    f.close();
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
    if (version != 1 && version != 2 && version != 3) return fail("Versi simpanan tidak dikenal.");
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
    p.oil = getInt("oil", 2, 0, 99);
    p.saltCrystal = getInt("salt", 0, 0, 999);
    p.weapon = getInt("weapon", 0, 0, 4);
    p.armor = getInt("armor", 0, 0, 4);
    p.potions = getInt("potions", 0, 0, 99);
    p.elixirs = getInt("elixirs", 0, 0, 99);
    p.hasAshCharm = getInt("hasAshCharm", 0, 0, 1) != 0;
    p.hasResonanceRing = getInt("hasRing", 0, 0, 1) != 0;
    p.hasSaltNoSorrow = getInt("hasSaltNoSorrow", 0, 0, 1) != 0;
    p.hasKidungBook = getInt("hasKidung", 0, 0, 1) != 0;
    p.hasTirtaCore = getInt("hasTirta", 0, 0, 1) != 0;
    p.hasCourtKey = getInt("hasCourtKey", 0, 0, 1) != 0;

    float nx = getFloat("x", 224.0f);
    float ny = getFloat("y", 288.0f);
    const MapDef& md = world_.def(newMap);
    if (!validFloat(nx) || !validFloat(ny)) return fail("Posisi simpanan rusak.");
    nx = clampf(nx, 0.0f, static_cast<float>(md.w * TILE - TILE));
    ny = clampf(ny, 0.0f, static_cast<float>(md.h * TILE - TILE));

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

    Flags fl;
    fl.resonance = getInt("resonance", 70, 0, 100);
    fl.act = getInt("act", 0, 0, 4);
    fl.stage = getInt("stage", 0, 0, 99);
    fl.memories = getInt("memories", 0, 0, 4095);
    fl.endingId = getInt("ending", 0, 0, 4);
    fl.senoChoice = getInt("senoChoice", 0, 0, 2);
    fl.sekarChoice = getInt("sekarChoice", 0, 0, 3);
    fl.introSeen = getInt("f_intro", 0, 0, 1) != 0;
    fl.courierSaved = getInt("f_courier", 0, 0, 1) != 0;
    fl.bimaJoined = getInt("f_bima", 0, 0, 1) != 0;
    fl.sekarJoined = getInt("f_sekar", 0, 0, 1) != 0;
    fl.larasJoined = getInt("f_laras", 0, 0, 1) != 0;
    fl.bagasJoined = getInt("f_bagas", 0, 0, 1) != 0;
    fl.fatherMemory = getInt("f_father", 0, 0, 1) != 0;
    fl.nirmalaTruth = getInt("f_nirmala", 0, 0, 1) != 0;
    fl.kiranaHelped = getInt("f_kirana", 0, 0, 1) != 0;
    fl.senoDefect = getInt("f_seno", 0, 0, 1) != 0;
    fl.gunturDefeated = getInt("f_guntur", 0, 0, 1) != 0;
    fl.gemaDefeated = getInt("f_gema", 0, 0, 1) != 0;
    fl.gameCompleted = getInt("f_complete", 0, 0, 1) != 0;
    fl.herbQuestAccepted = getInt("q_herb", 0, 0, 1) != 0;
    fl.herbQuestDone = getInt("q_herbDone", 0, 0, 1) != 0;
    fl.lanternQuestAccepted = getInt("q_lantern", 0, 0, 1) != 0;
    fl.lanternQuestDone = getInt("q_lanternDone", 0, 0, 1) != 0;
    fl.sekarQuestAccepted = getInt("q_sekar", 0, 0, 1) != 0;
    fl.sekarQuestDone = getInt("q_sekarDone", 0, 0, 1) != 0;
    fl.hiddenQuestDone = getInt("q_hidden", 0, 0, 1) != 0;
    fl.gajaminaDefeated = getInt("q_gajamina", 0, 0, 1) != 0;

    companions_.clear();
    {
        std::istringstream cs(text);
        std::string cline;
        while (std::getline(cs, cline)) {
            if (!cline.empty() && cline.back() == '\r') cline.pop_back();
            if (cline.rfind("comp ", 0) != 0) continue;
            std::istringstream ss(cline.substr(5));
            int id = 0, level = 1, hp = 1, recruited = 0;
            if (ss >> id >> level >> hp >> recruited) {
                Companion c = makeCompanion(static_cast<CompanionId>(clampInt(id, 0, 3)), level);
                c.hp = clampInt(hp, 0, c.maxHp);
                c.recruited = recruited != 0;
                c.alive = c.hp > 0;
                companions_.push_back(c);
            }
        }
    }

    player_ = p;
    flags_ = fl;
    map_ = newMap;
    px_ = nx;
    py_ = ny;
    facing_ = getInt("facing", 0, 0, 3);

    chestOpened_.assign(static_cast<size_t>(world_.count()), {});
    for (int i = 0; i < world_.count(); ++i)
        chestOpened_[static_cast<size_t>(i)].assign(world_.def(i).chests.size(), false);
    visited_.assign(static_cast<size_t>(world_.count()), false);
    markVisited(map_);

    resetRoamingEnemies(px_, py_);
    followers_.assign(companions_.size(), Vec2{px_, py_});
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
    if (legacy) writeSaveTo(path, nullptr);
    return true;
}

void Game::startDialogue(const std::string& speaker, const std::vector<std::string>& pages,
                        std::function<void()> onClose,
                        const std::vector<DialogueChoice>& choices) {
    dlg_.pages = pages;
    dlg_.choices = choices;
    dlg_.speaker = speaker;
    dlg_.page = 0;
    dlg_.chars = 0.0f;
    dlg_.active = true;
    dlg_.choosing = false;
    dlg_.choiceIndex = 0;
    dlg_.onClose = std::move(onClose);
    state_ = GameState::Dialogue;
}

void Game::closeDialogue() {
    dlg_.active = false;
    auto cb = dlg_.onClose;
    dlg_.onClose = nullptr;
    if (state_ == GameState::Dialogue) state_ = GameState::Explore;
    if (cb) cb();
}

void Game::applyChoice(int index) {
    if (index < 0 || index >= static_cast<int>(dlg_.choices.size())) return;
    const DialogueChoice& c = dlg_.choices[static_cast<size_t>(index)];
    flags_.resonance = clampInt(flags_.resonance + c.resonanceDelta, 0, 100);
    if (c.action) c.action();
    dlg_.choosing = false;
    dlg_.choices.clear();
    closeDialogue();
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
    followers_.assign(companions_.size(), Vec2{px_, py_});
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

void Game::updateFollowers(float dt) {
    if (followers_.empty()) return;
    for (size_t i = 0; i < companions_.size(); ++i) {
        if (i >= followers_.size()) followers_.push_back(Vec2{px_, py_});
        Vec2 target = i == 0 ? Vec2{px_, py_} : followers_[i - 1];
        float k = clampf(dt * 5.0f, 0.0f, 1.0f);
        followers_[i].x = lerp(followers_[i].x, target.x, k);
        followers_[i].y = lerp(followers_[i].y, target.y + static_cast<float>(i) * 2.0f, k);
    }
}

void Game::resetRoamingEnemies(float avoidX, float avoidY) {
    enemies_.clear();
    enemies_.resize(static_cast<size_t>(world_.count()));
    for (int map = 0; map < world_.count(); ++map) spawnRoamingEnemies(map, avoidX, avoidY);
    enemyGrace_ = 1.0f;
}

void Game::spawnRoamingEnemies(int map, float avoidX, float avoidY) {
    if (map < 0 || map >= world_.count()) return;
    int wanted = 0;
    if (map == 2) wanted = 8;
    else if (map == 3) wanted = 7;
    else if (map == 4) wanted = 6;
    else if (map == 8) wanted = 6;
    if (wanted <= 0) return;

    const MapDef& md = world_.def(map);
    struct Spot { int x, y; };
    std::vector<Spot> pool;
    for (int ty = 0; ty < md.h; ++ty) {
        for (int tx = 0; tx < md.w; ++tx) {
            if (!world_.encounter(map, tx, ty)) continue;
            if (world_.blocked(map, tx, ty)) continue;
            if (world_.npcAt(map, tx, ty) || world_.chestAt(map, tx, ty) ||
                world_.portalAt(map, tx, ty) || world_.echoAt(map, tx, ty) ||
                world_.campAt(map, tx, ty))
                continue;
            bool tooClose = false;
            for (const Portal& p : md.portals) {
                if (tx >= p.x - 3 && tx < p.x + p.w + 3 && ty >= p.y - 3 &&
                    ty < p.y + p.h + 3) {
                    tooClose = true;
                    break;
                }
            }
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
            if (world_.echoAt(map, tx, ty) || world_.campAt(map, tx, ty)) return true;
        }
    }
    if (map < 0 || map >= static_cast<int>(enemies_.size())) return false;
    const auto& list = enemies_[static_cast<size_t>(map)];
    for (size_t i = 0; i < list.size(); ++i) {
        if (static_cast<int>(i) == self || !list[i].active) continue;
        if (std::abs(list[i].x - nx) < 10.0f && std::abs(list[i].y - ny) < 10.0f) return true;
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
        battle_.begin(player_, type, false, seed, &companions_, &flags_.resonance);
    });
}

void Game::startBoss(EnemyType type) {
    uint64_t seed = rng_.next();
    startTransition([this, type, seed] {
        state_ = GameState::Battle;
        battle_.begin(player_, type, true, seed, &companions_, &flags_.resonance);
    });
}

void Game::onStep() { playSfx(SfxId::Step); }

void Game::checkPortal() {
    if (portalCooldown_ > 0.0f) return;
    int tx = tileOfX(px_ + 8.0f);
    int ty = tileOfY(py_ + 13.0f);
    const Portal* portal = world_.portalAt(map_, tx, ty);
    if (!portal) return;
    if (portal->requireAct > flags_.act) {
        portalCooldown_ = 1.8f;
        playSfx(SfxId::MenuCancel);
        startDialogue("Segel Kertarajasa",
                      {"Jalan ini disegel oleh status darurat Keadipatian.",
                       "Selesaikan misi aktual terlebih dahulu."});
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
            msg = "Kau menemukan " + std::to_string(chest.amount) + " Keping Garam!";
            addFloat(cx, cy, "+" + std::to_string(chest.amount) + "G", pal::gold);
            break;
        case ItemId::Potion:
            player_.potions += chest.amount;
            msg = "Kau menemukan Ramuan Pelita x" + std::to_string(chest.amount) + "!";
            break;
        case ItemId::Elixir:
            player_.elixirs += chest.amount;
            msg = "Kau menemukan Eliksir Sumsum x" + std::to_string(chest.amount) + "!";
            break;
        case ItemId::Oil:
            player_.oil += chest.amount;
            msg = "Kau menemukan Minyak Sumsum x" + std::to_string(chest.amount) + "!";
            break;
        case ItemId::SaltCrystal:
            player_.saltCrystal += chest.amount;
            msg = "Kau menemukan Garam Kenangan x" + std::to_string(chest.amount) + "!";
            break;
        case ItemId::SoulDagger:
            if (player_.weapon < 2) {
                player_.weapon = 2;
                msg = "Kau menemukan Belati Penarah Jiwa!";
            } else {
                player_.gold += 150;
                msg = "Belati Penarah Jiwa cadangan. Dijual +150G.";
            }
            break;
        case ItemId::TuningStaff:
            msg = "Tongkat Garpu Tala! Larasati akan bergabung.";
            if (!flags_.larasJoined) recruit(CompanionId::Larasati, 1 + flags_.act);
            break;
        case ItemId::TirtaCore:
            player_.hasTirtaCore = true;
            msg = "Inti Tirta Purba! Relik air purba berdenyut hangat.";
            break;
        case ItemId::SaltNoSorrow:
            player_.hasSaltNoSorrow = true;
            msg = "Garam Tanpa Duka. Pengikisan Rasa tidak lagi menyakitimu.";
            break;
        case ItemId::KidungBook:
            player_.hasKidungBook = true;
            msg = "Kitab Kidung Pertama! Syarat True Ending terpenuhi.";
            if (hasMemory(10)) flags_.hiddenQuestDone = true;
            break;
        case ItemId::CourtKey:
            player_.hasCourtKey = true;
            msg = "Kunci Kraton! Pintu rahasia ruang tahta terbuka.";
            break;
        case ItemId::AshCharm:
            player_.hasAshCharm = true;
            msg = "Jimat Penahan Abu!";
            break;
        case ItemId::ResonanceRing:
            player_.hasResonanceRing = true;
            msg = "Cincin Resonansi Perak!";
            break;
        default:
            msg = "Peti kosong.";
            break;
    }
    playSfx(chest.item == ItemId::Gold ? SfxId::Coin : SfxId::Chest);
    startDialogue("Peti Harta", {msg}, [this] { state_ = GameState::Explore; });
    advanceActIfReady();
}

void Game::recruit(CompanionId id, int level) {
    for (const auto& c : companions_)
        if (c.id == id) return;
    companions_.push_back(makeCompanion(id, level));
    followers_.assign(companions_.size(), Vec2{px_, py_});
    if (id == CompanionId::Bima) flags_.bimaJoined = true;
    if (id == CompanionId::Sekar) flags_.sekarJoined = true;
    if (id == CompanionId::Larasati) flags_.larasJoined = true;
    if (id == CompanionId::Bagas) flags_.bagasJoined = true;
    advanceActIfReady();
}

void Game::advanceActIfReady() {
    if (flags_.act == 1 && flags_.bimaJoined && flags_.sekarJoined && flags_.larasJoined &&
        hasMemory(1) && hasMemory(2)) {
        flags_.act = 2;
        autosave();
        mapBanner_ = 3.0f;
        mapBannerText_ = "Act II - Bisikan di Bawah Garam";
        spawnBurst(px_ + 8.0f, py_ + 8.0f, 20, 0xFF9AD8E8u, 34.0f);
    }
}

void Game::addMemory(int id) {
    if (id < 0 || id >= 12) return;
    flags_.memories |= 1 << id;
}

void Game::readEcho(int id) {
    if (hasMemory(id)) {
        startDialogue("Gema Garam", {"Gema ini sudah kau baca. Fragmen itu tersimpan di jurnal."});
        return;
    }
    addMemory(id);
    flags_.resonance = clampInt(flags_.resonance + 5, 0, 100);
    playSfx(SfxId::Heal);
    spawnBurst(px_ + 8.0f, py_ + 8.0f, 16, 0xFF9AD8E8u, 28.0f);
    switch (id) {
        case 0:
            startDialogue("Gema Kurir", {"Tabung logam itu berat. Di dalamnya cetak biru mesin raksasa.",
                                         "Lentera kota sedang memakan jiwa desa."});
            break;
        case 1:
            startDialogue("Gema Segel", {"Segel lilin ini memakai cincin pribadi Adipati Mahardika.",
                                         "Minyak dari krematorium dikirim ke Kraton."});
            break;
        case 2:
            startDialogue("Gema Manifes", {"Manifes gudang: ribuan guci Minyak Sumsum untuk Bahtera Langit.",
                                           "Distrik bawah tidak menerima jatah minggu ini."});
            break;
        case 3:
            startDialogue("Gema Lentera", {"Lentera ini milik suami Nenek Sari. Di dalamnya pesan terakhirnya.",
                                           "Ia tidak meninggalkan istrinya. Badai menahannya di rimba."});
            break;
        case 4:
            startDialogue("Gema Karang", {"Terumbu karang purba mengalami kehausan yang sama.",
                                          "Rimba ini dulunya dasar laut yang tersadar."});
            break;
        case 5:
            startDialogue("Gema Ayah", {"Ayah Raka tidak tewas tertimbun.",
                                        "Ia ditembak Raden Guntur saat mencoba meledakkan katup pipa."},
                          [this] {
                              flags_.resonance = clampInt(flags_.resonance + 10, 0, 100);
                          });
            break;
        case 6:
            startDialogue("Gema Pipa", {"Pipa penyedot mengarah ke Jantung Palung Sunyi.",
                                        "Mereka menyedot kelembapan jiwa bumi, bukan menambang garam."});
            break;
        case 7:
            startDialogue("Gema Prasasti", {"Meteorit Tahun 0 EG menyerap air samudra.",
                                            "Entitas di dalamnya mengurung dirinya dengan garam."});
            break;
        case 8:
            startDialogue("Gema Kunci Tirta", {"Empat Kunci Tirta dijaga makhluk purba.",
                                               "Candi ini menyimpan jalan ke Jantung Palung."});
            break;
        case 9:
            startDialogue("Gema Pendiri", {"Ordo Pelita awalnya menyimpan ingatan, bukan membakarnya.",
                                           "Taman Lentera Mati menyimpan kebenaran yang dibuang."});
            break;
        case 10:
            startDialogue("Gema Terlarang", {"Bunda Nirmala tahu minyak itu mempercepat pembatuan.",
                                             "Ia memilih diam demi harapan Bahtera Langit."},
                          [this] {
                              flags_.hiddenQuestDone = true;
                              if (player_.hasKidungBook) flags_.hiddenQuestDone = true;
                          });
            break;
        case 11:
            startDialogue("Gema Pertama", {"Sang Gema Pertama bukan dewa.",
                                           "Ia parasit yang menjanjikan tanah hijau palsu."});
            break;
        default:
            break;
    }
    advanceActIfReady();
}

void Game::restCampfire() {
    if (player_.oil <= 0) {
        startDialogue("Api Unggun", {"Kau tidak punya Minyak Sumsum untuk menyalakan api.",
                                     "Beli atau cari guci minyak di peti."});
        return;
    }
    --player_.oil;
    player_.fullRestore();
    for (auto& c : companions_) {
        if (!c.recruited) continue;
        c.hp = c.maxHp;
        c.alive = true;
    }
    flags_.resonance = clampInt(flags_.resonance + 10, 0, 100);
    playSfx(SfxId::Heal);
    spawnBurst(px_ + 8.0f, py_ + 8.0f, 18, 0xFFFFB06Au, 26.0f);
    addFloat(px_ + 8.0f, py_ - 4.0f, "PULIH + RESONANSI", 0xFFFFB06Au);
    startDialogue("Api Unggun", {"Api hangat memulihkan tubuh dan mengingatkan Raka pada wajah-wajah",
                                 "yang ia sayangi. Minyak -1, HP/MP penuh, resonansi +10."});
}

void Game::interact() {
    int dx = facing_ == 2 ? -1 : (facing_ == 3 ? 1 : 0);
    int dy = facing_ == 0 ? 1 : (facing_ == 1 ? -1 : 0);
    int tx = tileOfX(px_ + 8.0f) + dx;
    int ty = tileOfY(py_ + 12.0f) + dy;

    const NpcSpawn* npc = world_.npcAt(map_, tx, ty);
    if (npc) {
        if (npcHidden(*npc)) return;
        talkToNpc(*npc);
        return;
    }
    const ChestSpawn* chest = world_.chestAt(map_, tx, ty);
    if (chest) {
        int index = static_cast<int>(chest - world_.def(map_).chests.data());
        openChest(*chest, index);
        return;
    }
    const EchoSpot* echo = world_.echoAt(map_, tx, ty);
    if (echo) {
        readEcho(echo->id);
        return;
    }
    const CampSpot* camp = world_.campAt(map_, tx, ty);
    if (camp) {
        restCampfire();
        return;
    }
    Tile t = world_.tile(map_, tx, ty);
    if (t == Tile::Sign) {
        if (map_ == 0) startDialogue("Papan Kayu", {"\"Desa Tambak-Abu\"", "Tanah para penambang garam."});
        else if (map_ == 1) startDialogue("Papan Kayu", {"\"Pelabuhan Besi Kertarajasa\"", "Status darurat militer berlaku."});
        else startDialogue("Papan Kayu", {"\"Jalan Segara Garam\"", "Berhati-hatilah terhadap kabut abu."});
    } else if (t == Tile::Door) {
        startDialogue("Pintu", {"Pintu ini terkunci dari dalam."});
    } else if (t == Tile::CaveEntrance) {
        startDialogue("Mulut Gua", {"Angin dingin berhembus dari dalam...",
                                    "Masuklah untuk melanjutkan perjalanan."});
    } else if (t == Tile::RuinEntrance) {
        startDialogue("Gerbang Reruntuhan", {"Candi kuno setengah terkubur garam.",
                                             "Ukiran berbisik tentang meteorit."});
    }
}

void Game::talkToNpc(const NpcSpawn& npc) {
    switch (npc.style) {
        case NpcStyle::MbokRatih: talkMbokRatih(); break;
        case NpcStyle::Seno: talkSeno(); break;
        case NpcStyle::JokoKeling: state_ = GameState::Shop; shopIndex_ = 0;
            shopMessage_ = "Selamat datang di toko Joko Keling!";
            shopMessageTimer_ = 3.0f; break;
        case NpcStyle::Bima: talkBima(); break;
        case NpcStyle::Sekar: talkSekar(); break;
        case NpcStyle::Larasati: talkLarasati(); break;
        case NpcStyle::Bagas: talkBagas(); break;
        case NpcStyle::Nirmala: talkNirmala(); break;
        case NpcStyle::Kirana: talkKirana(); break;
        case NpcStyle::Adipati: talkAdipati(); break;
        case NpcStyle::Guntur: talkGuntur(); break;
        case NpcStyle::Warden:
            if (!flags_.fatherMemory) {
                startDialogue("Arwah Gema", {"Kau datang ke kedalaman tempat ayahmu gugur.",
                                             "Baca gema terakhirnya, lalu hadapi aku!"},
                              [this] { startBoss(EnemyType::ArwahGema); });
            }
            break;
        case NpcStyle::Guardian:
            if (map_ == 4 && !flags_.gajaminaDefeated) {
                startDialogue("Gajamina Pualam", {"Paus purba yang menyatu dengan candi ini terbangun.",
                                                   "Empat Kunci Tirta berdentang di bawah siripnya."},
                              [this] { startBoss(EnemyType::Gajamina); });
            } else if (map_ == 8 && !flags_.gemaDefeated) {
                startDialogue("Sang Gema Pertama", {"Aku menawarkan dunia tanpa duka.",
                                                    "Kau hanya perlu menyerahkan ingatan mereka."},
                              [this] { startBoss(EnemyType::GemaPertama); });
            }
            break;
        default: talkFlavor(npc.dialogue); break;
    }
}

void Game::talkMbokRatih() {
    if (!flags_.courierSaved) {
        flags_.courierSaved = true;
        addMemory(0);
        autosave();
        startDialogue("Mbok Ratih",
                      {"Kurir Kertarajasa itu membatu sambil memegang tabung logam.",
                       "Seno dan Garda Besi mengepung desa. Pergi lewat lorong bukit timur!",
                       "Bawa cetak biru itu ke Pelabuhan Besi Kertarajasa. Lindungi desaku."},
                      [this] {
                          flags_.act = 1;
                          flags_.stage = 0;
                          autosave();
                          mapBanner_ = 3.0f;
                          mapBannerText_ = "Act I - Kota Seribu Lentera";
                      },
                      {{"Lari sekarang; jaga tabungnya.", 5, [this] {}},
                       {"Hadapi Seno dan tunjukkan kebenarannya.", -5, [this] {}}});
        return;
    }
    startDialogue("Mbok Ratih", {"Hati-hati di kota, Raka. Lentera yang terlalu terang",
                                 "sering menyembunyikan bayangan di bawahnya."});
}

void Game::talkSeno() {
    if (map_ == 0 && !flags_.courierSaved) {
        startDialogue("Seno Wiratama",
                      {"Raka? Barang di kapal itu milik Keadipatian. Jangan buat aku menangkapmu.",
                       "Desa kita sudah kau tinggalkan lima tahun lalu."},
                      nullptr,
                      {{"Lari sekarang; lindungi tabungnya.", 5,
                        [this] { flags_.senoChoice = 1; flags_.resonance = clampInt(flags_.resonance + 5, 0, 100); }},
                       {"Hadapi Seno dan Buktikan kau benar.", -5,
                        [this] { flags_.senoChoice = 2; flags_.resonance = clampInt(flags_.resonance - 5, 0, 100); }}});
        return;
    }
    if (map_ == 7 && flags_.act >= 4) {
        startDialogue("Seno Wiratama", {"Aku membelot. Perintah membombardir desa kita",
                                         "adalah hal yang tak pernah bisa kupenuhi. Pergi, Raka."},
                      [this] { flags_.senoDefect = true; });
        return;
    }
    startDialogue("Seno Wiratama", {"Kertarajasa berjalan sesuai aturanku.",
                                    "Jangan menghalangi Garda Besi."});
}

void Game::talkJoko() {
    startDialogue("Joko Keling", {"Relik langka, kabar dari istana, semua ada harganya.",
                                  "Cari tahu sendiri apa yang dijual di gudang sektor empat."});
}

void Game::talkBima() {
    if (!flags_.bimaJoined) {
        recruit(CompanionId::Bima, 1 + flags_.act);
        autosave();
        startDialogue("Bima Prakasa",
                      {"Cetak biru ini milikku. Dulu kutemukan inti pembakar untuk Bahtera Langit.",
                       "Bahan bakarnya bukan garam biasa, Raka. Itu sari ingatan manusia.",
                       "Aku akan bergabung dan memperbaiki kesalahanku."});
        return;
    }
    startDialogue("Bima Prakasa", {"Api dan garam tidak bisa bohong.",
                                   "Aku akan membuat pelita angin-air sebelum era ini berakhir."});
}

void Game::talkSekar() {
    if (!flags_.sekarJoined) {
        recruit(CompanionId::Sekar, 1 + flags_.act);
        autosave();
        startDialogue("Sekar Wijayanti",
                      {"Kapal luncur Cakrawala Karat siap berlayar, Juru Gema.",
                       "Kuhancurkan monopoli minyak Kertarajasa, lalu kita bagi rata.",
                       "Tunggu dulu—kau tidak akan pergi sendirian, kan?"});
        return;
    }
    if (flags_.act >= 2 && !flags_.sekarQuestAccepted) {
        flags_.sekarQuestAccepted = true;
        startDialogue("Sekar Wijayanti",
                      {"Gubernur Kartiko, pengkhianat yang menjual ayahku, kini menjaga",
                       "penjara tambang utara. Bawa aku ke sana, dan biarkan kutuntaskan."});
        return;
    }
    if (flags_.sekarQuestAccepted && !flags_.sekarQuestDone) {
        startDialogue("Sekar Wijayanti",
                      {"Kartiko mengakui keluarganya disandera Raden Guntur.",
                       "Kau ingin mengeksekusinya atau menyerahkannya ke pengadilan rakyat?"},
                       nullptr,
                       {{"Biarkan hidup untuk bersaksi.", 5,
                         [this] {
                             flags_.sekarQuestDone = true;
                             flags_.sekarChoice = 1;
                             for (auto& c : companions_)
                                 if (c.id == CompanionId::Sekar) c.weapon = 3;
                         }},
                        {"Eksekusi di tempat.", -10,
                         [this] {
                             flags_.sekarQuestDone = true;
                             flags_.sekarChoice = 2;
                             for (auto& c : companions_)
                                 if (c.id == CompanionId::Sekar) c.weapon = 3;
                         }}});
        return;
    }
    startDialogue("Sekar Wijayanti", {"Angin garam di buritanku. Kita berlayar saat kau siap.",
                                      "Revolusi itu membutuhkan pembangunan, bukan hanya balas dendam."});
}

void Game::talkLarasati() {
    if (!flags_.larasJoined) {
        recruit(CompanionId::Larasati, 1 + flags_.act);
        autosave();
        startDialogue("Larasati",
                      {"Bunda Nirmala mengirim minyak ke Kraton, tapi distrik bawah kehilangan cahaya.",
                       "Aku mendengar patung-patung pualam berbisik. Mereka belum mati.",
                       "Tolong tunjukkan bahwa masih ada jalan lain."});
        return;
    }
    startDialogue("Larasati", {"Jika resonansimu menurun, istirahatlah di api unggun.",
                               "Aku akan menyanyikan ingatanmu kembali."});
}

void Game::talkBagas() {
    if (!flags_.bagasJoined) {
        recruit(CompanionId::Bagas, 1 + flags_.act);
        autosave();
        startDialogue("Kakek Bagas",
                      {"Candi ini menyimpan prasasti meteorit, Bocah Lentera.",
                       "Wabah Pualam bukan penyakit. Ini kehausan energi jiwa bumi.",
                       "Aku akan memetakan jalan menuju Sumur Ratapan."});
        return;
    }
    startDialogue("Kakek Bagas", {"Sejarah sejati sering dikubur oleh orang yang mengaku mensucikannya.",
                                  "Baca prasasti, dan jangan percaya mantra kosong."});
}

void Game::talkNirmala() {
    if (flags_.act < 3) {
        startDialogue("Bunda Nirmala", {"Pelita Putih menyimpan ingatan sampai samudra pulih.",
                                        "Pergi, anak kecil. Jangan mengganggu pekerjaan suci ini."});
        return;
    }
    if (!flags_.nirmalaTruth) {
        flags_.nirmalaTruth = true;
        autosave();
        startDialogue("Bunda Nirmala",
                      {"Catatan penyulingan ini... setiap guci minyak mempercepat pembatuan desa.",
                       "Mahardika bilang minyak itu menyembuhkan wabah. Dia bohong.",
                       "Taman Lentera Mati di balik air terjun pasir menyimpan sisa abu jiwa."},
                      [this] {
                          flags_.resonance = clampInt(flags_.resonance + 5, 0, 100);
                      });
        return;
    }
    startDialogue("Bunda Nirmala", {"Aku akan mereformasi Ordo Pelita.",
                                    "Jangan biarkan Bahtera Langit menyedot jiwa yang tersisa."});
}

void Game::talkKirana() {
    if (flags_.act < 3) {
        startDialogue("Kirana Surya", {"Kursi rodaku tidak bisa membawaku keluar tembok.",
                                      "Tolong hentikan penderitaan rakyat di distrik bawah."});
        return;
    }
    if (!flags_.kiranaHelped) {
        flags_.kiranaHelped = true;
        player_.hasCourtKey = true;
        autosave();
        startDialogue("Kirana Surya",
                      {"Ayahku membangun Bahtera Langit demi menyelamatkanku.",
                       "Tapi kesembuhanku dibayar dengan ingatan rakyat jelata.",
                       "Ambil kunci Kraton ini. Berhentikan mesin itu, Raka."});
        return;
    }
    startDialogue("Kirana Surya", {"Aku tidak ingin sembuh dengan harga seperti itu.",
                                   "Biarkan aku memimpin republik baru kelak."});
}

void Game::talkAdipati() {
    if (flags_.act < 3) {
        startDialogue("Adipati Mahardika", {"Lembah ini butuh keteraturan, Juru Gema.",
                                            "Pemberontakan hanya membawa abu."});
        return;
    }
    startDialogue("Adipati Mahardika",
                  {"Kau menyebutku monster karena mengorbankan seribu nyawa.",
                   "Lalu sanggupkah kalian menanggung matinya seluruh peradaban?!",
                   "Bahtera Langit adalah satu-satunya sekoci sebelum bumi ini mati."},
                  [this] { startBoss(EnemyType::Adipati); });
}

void Game::talkGuntur() {
    if (map_ == 7 && flags_.act < 4) {
        startDialogue("Raden Guntur",
                      {"Tanah hijau di atas awan itu nyata, Yang Mulia.",
                       "Suara di dasar palung menjanjikannya. Bahtera Langit adalah kuncinya.",
                       "Aku hanya mempercepat apa yang tak terhindarkan."},
                      [this] {
                          flags_.act = 4;
                          autosave();
                          mapBanner_ = 3.0f;
                          mapBannerText_ = "Act IV - Requiem Segara Garam";
                      });
        return;
    }
    if (!flags_.gunturDefeated) {
        startDialogue("Raden Guntur",
                      {"Mesin inti sudah menyatu denganku. Aku akan menjadi manusia pertama",
                       "yang menembus langit, atau menghancurkan segelnya dari dalam."},
                      [this] { startBoss(EnemyType::Guntur); });
        return;
    }
    startDialogue("Raden Guntur", {"Semua hitunganku... salah. Palung itu tidak pernah berterima kasih."});
}

void Game::talkFlavor(int dialogueId) {
    switch (dialogueId) {
        case 6:
            if (flags_.lanternQuestDone) {
                startDialogue("Nenek Sari", {"Lentera suamiku kembali. Terima kasih, Raka.",
                                             "Jimat Penahan Abu ini untukmu."});
            } else if (!flags_.lanternQuestAccepted) {
                flags_.lanternQuestAccepted = true;
                startDialogue("Nenek Sari", {"Suamiku hilang di Rimba Karang-Kelabu sepuluh tahun lalu.",
                                             "Bawa pulang lentera terakhirnya, dan dengarkan pesannya.",
                                             "Aku ingin berpisah dengan benar sebelum aku membatu."});
            } else if (hasMemory(3)) {
                flags_.lanternQuestDone = true;
                player_.hasAshCharm = true;
                flags_.resonance = clampInt(flags_.resonance + 10, 0, 100);
                autosave();
                startDialogue("Nenek Sari", {"Lentera ini... suaranya masih hangat.",
                                             "Terima kasih. Terimalah Jimat Penahan Abu ini.",
                                             "Resonansimu sedikit pulih."});
            } else {
                startDialogue("Nenek Sari", {"Lentera itu berada di Rimba Karang-Kelabu.",
                                             "Ikuti peta kasar ini ke pohon karang kembar."});
            }
            break;
        case 2:
            if (flags_.herbQuestDone) {
                startDialogue("Petani", {"Ramuanmu menyelamatkan bibit musim ini. Terima kasih!"});
            } else if (!flags_.herbQuestAccepted) {
                flags_.herbQuestAccepted = true;
                startDialogue("Petani", {"Tanah tambak semakin dingin. Bawakan 3 Ramuan Pelita,",
                                         "dan kubayar 120 Keping Garam plus Eliksir Sumsum."});
            } else if (player_.potions >= 3) {
                player_.potions -= 3;
                player_.gold += 120;
                player_.elixirs = std::min(99, player_.elixirs + 1);
                flags_.herbQuestDone = true;
                autosave();
                startDialogue("Petani", {"Musim panen ini masih bisa diselamatkan!",
                                         "Terimalah 120 Keping Garam dan sebuah Eliksir."});
            } else {
                startDialogue("Petani", {"Bibit-bibit masih lemah. Ramuan: " +
                                             std::to_string(player_.potions) + "/3."});
            }
            break;
        case 3:
            startDialogue("Anak Desa", {"Kabarnya Kapten Sekar akan menyerbu gudang minyak.",
                                        "Kalau kota gelap, siapa yang akan melindungi desa?"});
            break;
        case 15:
            startDialogue("Garda Besi", {"Distrik bawah dalam status darurat.",
                                         "Jangan minta soal minyak. Semua untuk Bahtera Langit."});
            break;
        case 20:
            startDialogue("Pengintai", {"Konvoi garam merah lewat di jalur tengah setiap beberapa hari.",
                                        "Cegat dan bagi muatannya, atau biarkan desa kita membatu."});
            break;
        case 21:
            startDialogue("Pertapa Sunyi", {"Pembatuan bukan kutukan. Ia adalah evolusi.",
                                            "Bumi ingin mengingat kita dengan cara lain."});
            break;
        case 40:
            startDialogue("Kakek Bagas", {"Candi ini miring karena meteorit di bawahnya.",
                                          "Baca prasastinya sebelum memutuskan apa pun."});
            break;
        case 50:
            startDialogue("Bunda Nirmala", {"Pelita menyimpan nama orang yang telah pergi.",
                                            "Jangan membakarnya demi kenyamanan sesaat."});
            break;
        case 51:
            startDialogue("Pelarian Ordo", {"Taman Lentera Mati di balik air terjun pasir.",
                                            "Tiga nisan tanpa nama menunggu dinyalakan."});
            break;
        case 60:
            startDialogue("Pelarian Taman", {"Jurnal pendiri menunggu di ruang arsip.",
                                             "Ordo Pelita dulu menyimpan ingatan, bukan membakarnya."});
            break;
        case 70:
            startDialogue("Kirana Surya", {"Kunci Kraton ada di tanganku. Pintu ruang tahta menunggu."});
            break;
        case 71:
            startDialogue("Adipati Mahardika", {"Lengan kananku sudah pualam. Lihatlah harganya."});
            break;
        case 72:
            startDialogue("Raden Guntur", {"Kau tidak akan mengizinkan manusia tetap menjadi budak garam."});
            break;
        case 73:
            startDialogue("Seno Wiratama", {"Aku membelot. Jaga keamanan kota saat kau pergi."});
            break;
        case 74:
            startDialogue("Garda Kraton", {"Ruang tahta hanya untuk pemilik Kunci Kraton."});
            break;
        case 80:
            startDialogue("Raden Guntur", {"Palung akan menelanmu, Juru Gema.",
                                           "Aku sudah melihat akhir yang benar."});
            break;
        case 81:
            startDialogue("Sang Gema Pertama", {"Serahkan ingatan mereka. Dunia tanpa duka menantimu."});
            break;
        default:
            startDialogue("Warga", {"Semoga lenteramu tidak padam, Juru Gema."});
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
        if (player_.elixirs <= 0) menuMessage_ = "Kau tidak punya Eliksir Sumsum.";
        else if (player_.mp >= player_.maxMp) menuMessage_ = "MP sudah penuh.";
        else {
            player_.useElixir();
            menuMessage_ = "Eliksir digunakan. MP pulih.";
            playSfx(SfxId::Potion);
        }
    } else {
        if (player_.potions <= 0) menuMessage_ = "Kau tidak punya Ramuan Pelita.";
        else if (player_.hp >= player_.maxHp) menuMessage_ = "HP sudah penuh.";
        else {
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
    if (titleUpdateActive_ && updater_ && updater_->running()) return;
    menuFeedback(in);

    if (titlePage_ == TitlePage::Load) {
        if (in.hit(Key::Up)) titleSlotIndex_ = (titleSlotIndex_ + 4) % 5;
        if (in.hit(Key::Down)) titleSlotIndex_ = (titleSlotIndex_ + 1) % 5;
        if (in.hit(Key::Cancel)) { titlePage_ = TitlePage::Main; titleIndex_ = 1; return; }
        if (!in.hit(Key::Confirm)) return;
        if (titleSlotIndex_ == 4) { titlePage_ = TitlePage::Main; titleIndex_ = 1; return; }
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
        if (in.hit(Key::Cancel)) { titlePage_ = TitlePage::Main; titleIndex_ = 0; return; }
        if (!in.hit(Key::Confirm)) return;
        if (titleConfirmIndex_ == 0) newGame();
        else { titlePage_ = TitlePage::Main; titleIndex_ = 0; }
        return;
    }
    if (in.hit(Key::Up)) titleIndex_ = (titleIndex_ + 3) % 4;
    if (in.hit(Key::Down)) titleIndex_ = (titleIndex_ + 1) % 4;
    if (!in.hit(Key::Confirm)) return;
    if (titleIndex_ == 0) {
        if (anySaveExists()) { titlePage_ = TitlePage::ConfirmNew; titleConfirmIndex_ = 1; }
        else newGame();
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

    if (in.hit(Key::Cancel) || in.hit(Key::Menu)) { openMenu(); return; }
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

    clampPlayerToMap();
    updateCamera(dt);
    updateFollowers(dt);
    if (transition_) return;
    checkPortal();
    if (transition_) return;
    updateEnemies(dt);
    checkEnemyContact();
}

void Game::updateDialogue(float dt, const InputState& in) {
    if (!dlg_.active) { closeDialogue(); return; }
    if (dlg_.choosing) {
        if (in.hit(Key::Up)) {
            dlg_.choiceIndex = (dlg_.choiceIndex + static_cast<int>(dlg_.choices.size()) - 1) %
                               static_cast<int>(dlg_.choices.size());
            playSfx(SfxId::MenuMove);
        }
        if (in.hit(Key::Down)) {
            dlg_.choiceIndex = (dlg_.choiceIndex + 1) % static_cast<int>(dlg_.choices.size());
            playSfx(SfxId::MenuMove);
        }
        if (in.hit(Key::Confirm)) applyChoice(dlg_.choiceIndex);
        return;
    }
    if (dlg_.pages.empty()) { closeDialogue(); return; }
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
    if (dlg_.page >= static_cast<int>(dlg_.pages.size())) {
        if (!dlg_.choices.empty()) {
            dlg_.choosing = true;
            dlg_.choiceIndex = 0;
        } else {
            closeDialogue();
        }
    }
}

void Game::updateBattle(float dt, const InputState& in) {
    battle_.update(dt, in, player_, companions_);
    if (!battle_.done()) return;

    if (battle_.victory()) {
        EnemyType type = battle_.enemyType();
        bool boss = battle_.isBoss();
        if (boss) {
            if (type == EnemyType::ArwahGema && !flags_.fatherMemory) {
                flags_.fatherMemory = true;
                addMemory(5);
                flags_.act = 3;
                autosave();
                state_ = GameState::Dialogue;
                startDialogue("Arwah Gema", {"Kau telah membaca gema ayahmu.",
                                             "Pipa penyedot harus dihentikan. Kembali ke Kertarajasa."},
                              [this] { state_ = GameState::Explore; });
            } else if (type == EnemyType::Gajamina && !flags_.gajaminaDefeated) {
                flags_.gajaminaDefeated = true;
                player_.hasTirtaCore = true;
                autosave();
                state_ = GameState::Dialogue;
                startDialogue("Gajamina Pualam", {"Paus purba itu akhirnya beristirahat.",
                                                   "Inti Tirta Purba kini ada di tanganmu."},
                              [this] { state_ = GameState::Explore; });
            } else if (type == EnemyType::Adipati && !flags_.gemaDefeated) {
                flags_.act = 4;
                autosave();
                state_ = GameState::Dialogue;
                startDialogue("Adipati Mahardika", {"Guntur mengkhianatiku dan mempercepat penyedotan.",
                                                    "Kejar dia ke Jantung Palung Sunyi. Biar aku yang menahannya."},
                              [this] {
                                  state_ = GameState::Explore;
                                  mapBanner_ = 3.0f;
                                  mapBannerText_ = "Act IV - Requiem Segara Garam";
                              });
            } else if (type == EnemyType::Guntur && !flags_.gunturDefeated) {
                flags_.gunturDefeated = true;
                autosave();
                state_ = GameState::Dialogue;
                startDialogue("Raden Guntur", {"Mesin... berhenti. Tapi palung masih lapar.",
                                               "Sang Gema Pertama menantimu di jantungnya."},
                              [this] { state_ = GameState::Explore; });
            } else if (type == EnemyType::GemaPertama && !flags_.gemaDefeated) {
                flags_.gemaDefeated = true;
                finishGame();
            } else {
                state_ = GameState::Explore;
            }
        } else {
            state_ = GameState::Explore;
        }
    } else if (battle_.fled()) {
        state_ = GameState::Explore;
    } else {
        state_ = GameState::GameOver;
        gameOverIndex_ = 0;
    }
    for (auto& c : companions_)
        if (c.recruited && c.hp <= 0) {
            c.hp = std::max(1, c.maxHp / 4);
            c.alive = true;
        }
    flash_ = 0.0f;
    shake_ = 0.0f;
}

void Game::finishGame() {
    bool trueReady = flags_.resonance >= 70 && flags_.hiddenQuestDone &&
                     flags_.bimaJoined && flags_.sekarJoined && flags_.larasJoined &&
                     flags_.bagasJoined && memoryCount() >= 10;
    bool secretReady = trueReady && flags_.gajaminaDefeated && memoryCount() >= 12 &&
                       player_.hasTirtaCore;
    std::vector<DialogueChoice> choices;
    if (secretReady) {
        choices.push_back({"Gunakan Inti Tirta Purba untuk menetralkan meteorit.", 0, [this] {
                               flags_.endingId = 4;
                               flags_.gameCompleted = true;
                               state_ = GameState::Ending;
                               endingScroll_ = 0.0f;
                               autosave();
                           }});
    }
    if (trueReady) {
        choices.push_back({"Bagikan beban Jangkar Gema bersama seluruh party.", 10, [this] {
                               flags_.endingId = 1;
                               flags_.gameCompleted = true;
                               state_ = GameState::Ending;
                               endingScroll_ = 0.0f;
                               autosave();
                           }});
    }
    if (flags_.resonance >= 40) {
        choices.push_back({"Biarkan Adipati Mahardika menebus dosanya.", 5, [this] {
                               flags_.endingId = 2;
                               flags_.gameCompleted = true;
                               state_ = GameState::Ending;
                               endingScroll_ = 0.0f;
                               autosave();
                           }});
    }
    choices.push_back({"Ikuti bisikan Sang Gema Pertama.", -20, [this] {
                           flags_.endingId = 3;
                           flags_.gameCompleted = true;
                           state_ = GameState::Ending;
                           endingScroll_ = 0.0f;
                           autosave();
                       }});
    startDialogue("Jantung Palung Sunyi",
                  {"Retakan di dasar palung menyedot seluruh energi jiwa Nusakalpa.",
                   "Seseorang harus menjadi Jangkar Gema, atau dunia ini akan dikuasai ilusi.",
                   "Pilihanmu menentukan akhir era garam."},
                  nullptr, choices);
}

void Game::updateMenu(float dt, const InputState& in) {
    menuMessageTimer_ = std::max(0.0f, menuMessageTimer_ - dt);
    menuFeedback(in);
    if (menuPage_ == 0) {
        if (in.hit(Key::Up)) menuIndex_ = (menuIndex_ + 5) % 6;
        if (in.hit(Key::Down)) menuIndex_ = (menuIndex_ + 1) % 6;
        if (in.hit(Key::Cancel)) { closeMenu(); return; }
        if (in.hit(Key::Confirm)) {
            if (menuIndex_ == 0) { menuPage_ = 1; menuIndex_ = 0; }
            else if (menuIndex_ == 1) { menuPage_ = 2; menuIndex_ = 0; }
            else if (menuIndex_ == 2) { menuPage_ = 3; menuIndex_ = 0; }
            else if (menuIndex_ == 3) {
                menuPage_ = 4; saveConfirmSlot_ = -1;
                menuIndex_ = clampInt(saveSlot_ + 1, 1, 3);
            } else if (menuIndex_ == 4) { menuPage_ = 5; menuIndex_ = 0; }
            else closeMenu();
        }
    } else if (menuPage_ == 1) {
        if (in.hit(Key::Up) || in.hit(Key::Down)) menuIndex_ = (menuIndex_ + 3) % 4;
        if (in.hit(Key::Cancel)) { menuPage_ = 0; menuIndex_ = 0; return; }
        if (in.hit(Key::Confirm)) {
            if (menuIndex_ < 2) {
                useFieldItem(menuIndex_ == 1);
            } else if (menuIndex_ == 2) {
                menuMessage_ = "Minyak Sumsum dipakai di api unggun.";
                menuMessageTimer_ = 2.4f;
                playSfx(SfxId::MenuCancel);
            } else {
                menuMessage_ = "Garam Kenangan untuk pengrajin.";
                menuMessageTimer_ = 2.4f;
                playSfx(SfxId::MenuCancel);
            }
        }
    } else if (menuPage_ == 2) {
        if (in.hit(Key::Cancel) || in.hit(Key::Confirm)) { menuPage_ = 0; menuIndex_ = 1; }
    } else if (menuPage_ == 3) {
        if (in.hit(Key::Cancel) || in.hit(Key::Confirm)) { menuPage_ = 0; menuIndex_ = 2; }
    } else if (menuPage_ == 4) {
        if (in.hit(Key::Up)) { menuIndex_ = (menuIndex_ + 4) % 5; saveConfirmSlot_ = -1; }
        if (in.hit(Key::Down)) { menuIndex_ = (menuIndex_ + 1) % 5; saveConfirmSlot_ = -1; }
        if (in.hit(Key::Cancel)) { menuPage_ = 0; menuIndex_ = 3; saveConfirmSlot_ = -1; return; }
        if (in.hit(Key::Confirm)) {
            if (menuIndex_ == 4) { menuPage_ = 0; menuIndex_ = 3; saveConfirmSlot_ = -1; }
            else if (menuIndex_ == 0) {
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
                if (menuIndex_ == 0) { audio_->setMusicVolume(audio_->musicVolume() + delta); changed = true; }
                else if (menuIndex_ == 1) { audio_->setSfxVolume(audio_->sfxVolume() + delta); changed = true; }
            }
            if (in.hit(Key::Confirm)) {
                if (menuIndex_ == 2) { audio_->toggleMute(); changed = true; }
                else if (menuIndex_ == 3) { menuPage_ = 0; menuIndex_ = 4; }
            }
            if (in.hit(Key::Cancel)) { menuPage_ = 0; menuIndex_ = 4; }
        } else if (in.hit(Key::Cancel) || in.hit(Key::Confirm)) {
            menuPage_ = 0; menuIndex_ = 4;
        }
        if (changed) settingsDirty_ = true;
    }
}

void Game::updateShop(float dt, const InputState& in) {
    shopMessageTimer_ = std::max(0.0f, shopMessageTimer_ - dt);
    menuFeedback(in);
    if (in.hit(Key::Up)) shopIndex_ = (shopIndex_ + SHOP_COUNT - 1) % SHOP_COUNT;
    if (in.hit(Key::Down)) shopIndex_ = (shopIndex_ + 1) % SHOP_COUNT;
    if (in.hit(Key::Cancel) || in.hit(Key::Menu)) { state_ = GameState::Explore; return; }
    if (!in.hit(Key::Confirm)) return;
    const ShopItem& item = SHOP[shopIndex_];
    if (player_.gold < item.price) {
        shopMessage_ = "Keping Garammu tidak cukup.";
        shopMessageTimer_ = 2.2f;
        playSfx(SfxId::MenuCancel);
        return;
    }
    int goldBefore = player_.gold;
    if (item.kind == 0) {
        player_.potions = std::min(99, player_.potions + 1);
        player_.gold -= item.price;
        shopMessage_ = "Ramuan Pelita dibeli.";
    } else if (item.kind == 1) {
        player_.elixirs = std::min(99, player_.elixirs + 1);
        player_.gold -= item.price;
        shopMessage_ = "Eliksir Sumsum dibeli.";
    } else if (item.kind == 2) {
        player_.oil = std::min(99, player_.oil + 1);
        player_.gold -= item.price;
        shopMessage_ = "Minyak Sumsum dibeli.";
    } else if (item.kind == 3) {
        if (player_.weapon >= item.index) shopMessage_ = "Kau sudah punya senjata yang lebih baik.";
        else { player_.weapon = item.index; player_.gold -= item.price; shopMessage_ = std::string(item.name) + " dibeli!"; }
    } else {
        if (player_.armor >= item.index) shopMessage_ = "Kau sudah punya armor yang lebih baik.";
        else { player_.armor = item.index; player_.gold -= item.price; shopMessage_ = std::string(item.name) + " dibeli!"; }
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
        py_ = 16 * TILE;
        facing_ = 1;
        updateCamera(100.0f);
        state_ = GameState::Explore;
        mapBanner_ = 3.0f;
        mapBannerText_ = "Kau bangkit kembali di Tambak-Abu.";
    } else {
        state_ = GameState::Title;
        titleIndex_ = 0;
        titlePage_ = TitlePage::Main;
    }
}

void Game::updateEnding(float dt, const InputState& in) {
    endingScroll_ += dt * 22.0f;
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
    battle_.begin(player_, type, boss, rng_.next(), &companions_, &flags_.resonance);
}

void Game::debugOpenMenu() {
    state_ = GameState::Menu;
    menuPage_ = 0;
    menuIndex_ = 0;
}

void Game::debugInteract() { interact(); }

void Game::debugAdvanceDialogue() {
    for (int i = 0; i < 64 && state_ == GameState::Dialogue; ++i) {
        if (dlg_.choosing) {
            applyChoice(0);
            break;
        }
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
    shopMessage_ = "Selamat datang di toko Joko Keling!";
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

void Game::debugRecruit(CompanionId id) { recruit(id, 1 + flags_.act); }

}  // namespace rpg
