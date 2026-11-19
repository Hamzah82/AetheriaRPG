// battle.cpp - party-based turn combat logic and presentation
#include "battle.h"

#include <algorithm>
#include <cmath>

#include "art.h"
#include "audio.h"

namespace rpg {

namespace {
const EnemyInfo ENEMIES[] = {
    {EnemyType::KumbangPualam, "Kumbang Pualam", 1, 22, 6, 2, 10, 6, 1},
    {EnemyType::GukAbu, "Anjing Kabut", 2, 28, 9, 2, 14, 10, 2},
    {EnemyType::ArwahGema, "Arwah Gema", 3, 36, 11, 4, 20, 16, 2},
    {EnemyType::PenambangPualam, "Penambang Pualam", 4, 48, 13, 7, 30, 24, 1},
    {EnemyType::Koral, "Koral Berduri", 5, 60, 15, 9, 40, 32, 2},
    {EnemyType::Inkuisitor, "Inkuisitor", 8, 200, 18, 8, 180, 260, 0},
    {EnemyType::Gajamina, "Gajamina Pualam", 10, 300, 22, 11, 260, 400, 2},
    {EnemyType::Adipati, "Adipati Mahardika", 12, 340, 24, 12, 320, 500, 0},
    {EnemyType::Guntur, "Raden Guntur", 14, 420, 27, 14, 400, 600, 0},
    {EnemyType::GemaPertama, "Gema Pertama", 16, 520, 30, 16, 600, 800, 0},
};
constexpr int ENEMY_COUNT = 10;

const int ENEMY_X = 214;
const int ENEMY_Y = 118;
const int HERO_X = 36;
const int HERO_Y = 106;

void panel(Framebuffer& fb, int x, int y, int w, int h) {
    fb.fillRectA(x, y, w, h, pal::uiPanel, 238);
    fb.outline(x, y, w, h, pal::uiBorder);
    fb.outline(x + 1, y + 1, w - 2, h - 2, 0xFF3A4160);
    fb.fillRect(x + 2, y + 2, 2, 2, pal::uiBorder);
    fb.fillRect(x + w - 4, y + 2, 2, 2, pal::uiBorder);
    fb.fillRect(x + 2, y + h - 4, 2, 2, pal::uiBorder);
    fb.fillRect(x + w - 4, y + h - 4, 2, 2, pal::uiBorder);
}

void bar(Framebuffer& fb, int x, int y, int w, int h, float frac, uint32_t col, uint32_t colDark) {
    fb.fillRect(x, y, w, h, colDark);
    int fill = clampInt(static_cast<int>(w * clampf(frac, 0.0f, 1.0f)), 0, w);
    if (fill > 0) {
        fb.fillRect(x, y, fill, h, col);
        fb.fillRect(x, y, fill, 1, mix(col, pal::white, 0.45f));
    }
    fb.outline(x, y, w, h, 0xFF0A0C14);
}

std::string itoa2(int v) { return std::to_string(v); }
}  // namespace

const EnemyInfo& enemyInfo(EnemyType type) {
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= ENEMY_COUNT) idx = 0;
    return ENEMIES[idx];
}

void Battle::begin(Player& p, EnemyType type, bool boss, uint64_t seed,
                   std::vector<Companion>* party, int* resonance) {
    (void)p;
    info_ = enemyInfo(type);
    boss_ = boss;
    enemyMaxHp_ = info_.hp;
    enemyHp_ = info_.hp;
    rng_ = Rng(seed);
    party_ = party;
    resonance_ = resonance;
    phase_ = Phase::Message;
    next_ = Next::Command;
    messages_.clear();
    pushMessage(std::string(info_.name) + " muncul!");
    if (boss_) pushMessage("Pertarungan besar dimulai!");
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
    menuIndex_ = 0;
    subIndex_ = 0;
    turnCount_ = 0;
    companionIndex_ = 0;
    guard_ = 0;
    enemyStun_ = 0;
    enemyDefDown_ = 0;
    tumbalUsed_ = false;
    resonanceSpent_ = false;
    bossPhase_ = 1;
    t_ = 0.0f;
    hurtT_ = shakeT_ = flashT_ = healT_ = 0.0f;
    effect_ = 0;
    effectT_ = 0.0f;
    xpGained_ = 0;
    goldGained_ = 0;
    levelMsg_.clear();
    rewardsGiven_ = false;
    done_ = false;
    victory_ = false;
    fled_ = false;
    if (boss_) sfx(SfxId::BossRoar);
}

void Battle::pushMessage(const std::string& m) { messages_.push_back(m); }

void Battle::sfx(SfxId id) {
    if (audio_) audio_->playSfx(id);
}

void Battle::goCommand() {
    phase_ = Phase::Command;
    menuIndex_ = 0;
    subIndex_ = 0;
    messages_.clear();
    msgIndex_ = 0;
    chars_ = 0.0f;
}

int damageRoll(Rng& rng, int base, int spread) {
    return std::max(1, base + rng.range(-spread, spread));
}

void Battle::playerAttack(Player& p) {
    bool crit = rng_.chance(0.10f);
    int def = std::max(0, info_.def - enemyDefDown_);
    int dmg = damageRoll(rng_, p.atk() * 2 - def, 2);
    if (crit) dmg = dmg * 3 / 2;
    sfx(crit ? SfxId::Crit : SfxId::Attack);
    enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
    effect_ = 4;
    effectT_ = 0.0f;
    hurtT_ = 0.25f;
    shakeT_ = 0.18f;
    pushMessage("Raka menebas " + std::string(info_.name) + "! (-" + itoa2(dmg) + " HP)");
    if (crit) pushMessage("Serangan kritis!");
    if (enemyHp_ <= 0) {
        pushMessage(std::string(info_.name) + " dikalahkan!");
        next_ = Next::Victory;
        messages_.push_back("Kemenangan!");
    } else {
        next_ = Next::EnemyTurn;
    }
    phase_ = Phase::Message;
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
}

void Battle::castSkill(Player& p, int index) {
    auto spendResonance = [&](int amount) {
        if (!resonance_) return;
        *resonance_ = clampInt(*resonance_ - amount, 0, 100);
        resonanceSpent_ = true;
    };
    if (index == 0) {
        const int cost = 5;
        if (p.mp < cost) {
            pushMessage("MP tidak cukup untuk Api Garam!");
            next_ = Next::Command;
            phase_ = Phase::Message;
            msgIndex_ = 0;
            chars_ = 0.0f;
            return;
        }
        p.mp -= cost;
        sfx(SfxId::Fire);
        int dmg = damageRoll(rng_, 12 + p.level * 4, 3);
        if (info_.weak == 1) dmg = dmg * 3 / 2;
        enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
        effect_ = 1;
        effectT_ = 0.0f;
        hurtT_ = 0.3f;
        flashT_ = 0.12f;
        pushMessage("Api Garam menghantam " + std::string(info_.name) + "! (-" + itoa2(dmg) + " HP)");
        if (info_.weak == 1) pushMessage("Super efektif!");
    } else if (index == 1) {
        const int cost = 7;
        if (p.mp < cost) {
            pushMessage("MP tidak cukup untuk Petir Garam!");
            next_ = Next::Command;
            phase_ = Phase::Message;
            msgIndex_ = 0;
            chars_ = 0.0f;
            return;
        }
        p.mp -= cost;
        sfx(SfxId::Thunder);
        int dmg = damageRoll(rng_, 16 + p.level * 4, 3);
        if (info_.weak == 2) dmg = dmg * 3 / 2;
        enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
        effect_ = 2;
        effectT_ = 0.0f;
        hurtT_ = 0.3f;
        flashT_ = 0.2f;
        shakeT_ = 0.2f;
        pushMessage("Petir Garam menyambar " + std::string(info_.name) + "! (-" + itoa2(dmg) + " HP)");
        if (info_.weak == 2) pushMessage("Super efektif!");
    } else if (index == 2) {
        const int cost = 8;
        if (p.mp < cost) {
            pushMessage("MP tidak cukup untuk Ledakan Kenangan!");
            next_ = Next::Command;
            phase_ = Phase::Message;
            msgIndex_ = 0;
            chars_ = 0.0f;
            return;
        }
        p.mp -= cost;
        spendResonance(4);
        sfx(SfxId::BossRoar);
        int dmg = damageRoll(rng_, 22 + p.level * 5, 4);
        enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
        effect_ = 5;
        effectT_ = 0.0f;
        hurtT_ = 0.35f;
        shakeT_ = 0.28f;
        flashT_ = 0.18f;
        pushMessage("Ledakan Kenangan menghantam " + std::string(info_.name) + "! (-" +
                    itoa2(dmg) + " HP)");
        pushMessage("Resonansimu terkikis...");
    } else if (index == 3) {
        const int cost = 6;
        if (p.mp < cost) {
            pushMessage("MP tidak cukup untuk Kidung Garam!");
            next_ = Next::Command;
            phase_ = Phase::Message;
            msgIndex_ = 0;
            chars_ = 0.0f;
            return;
        }
        p.mp -= cost;
        sfx(SfxId::Heal);
        int amount = 30 + p.level * 5;
        int before = p.hp;
        p.healHp(amount);
        int healed = p.hp - before;
        effect_ = 3;
        effectT_ = 0.0f;
        healT_ = 0.5f;
        pushMessage("Kidung Garam memulihkan Raka. (+" + itoa2(healed) + " HP)");
    } else {
        if (tumbalUsed_) {
            pushMessage("Tumbal Rasa sudah dipakai di pertarungan ini.");
            next_ = Next::Command;
            phase_ = Phase::Message;
            msgIndex_ = 0;
            chars_ = 0.0f;
            return;
        }
        if (!resonance_ || *resonance_ < 15) {
            pushMessage("Resonansi emosi terlalu rendah untuk Tumbal Rasa.");
            next_ = Next::Command;
            phase_ = Phase::Message;
            msgIndex_ = 0;
            chars_ = 0.0f;
            return;
        }
        spendResonance(15);
        tumbalUsed_ = true;
        guard_ = 2;
        p.hp = p.maxHp;
        sfx(SfxId::Heal);
        effect_ = 3;
        effectT_ = 0.0f;
        healT_ = 0.6f;
        pushMessage("Tumbal Rasa memulihkan Raka seutuhnya!");
        pushMessage("Para rekan menahan serangan untukmu.");
    }
    if (enemyHp_ <= 0) {
        pushMessage(std::string(info_.name) + " dikalahkan!");
        next_ = Next::Victory;
        messages_.push_back("Kemenangan!");
    } else {
        next_ = Next::EnemyTurn;
    }
    phase_ = Phase::Message;
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
}

void Battle::useItem(Player& p, bool elixir) {
    if (elixir) {
        if (p.elixirs <= 0 || p.mp >= p.maxMp) {
            pushMessage(p.elixirs <= 0 ? "Kau tidak punya Eliksir Sumsum." : "MP sudah penuh.");
            next_ = Next::Command;
        } else {
            p.useElixir();
            effect_ = 3;
            effectT_ = 0.0f;
            healT_ = 0.5f;
            sfx(SfxId::Potion);
            pushMessage("Raka memakai Eliksir Sumsum. (+30 MP)");
            next_ = Next::EnemyTurn;
        }
    } else {
        if (p.potions <= 0 || p.hp >= p.maxHp) {
            pushMessage(p.potions <= 0 ? "Kau tidak punya Ramuan Pelita." : "HP sudah penuh.");
            next_ = Next::Command;
        } else {
            p.usePotion();
            effect_ = 3;
            effectT_ = 0.0f;
            healT_ = 0.5f;
            sfx(SfxId::Potion);
            pushMessage("Raka memakai Ramuan Pelita. (+45 HP)");
            next_ = Next::EnemyTurn;
        }
    }
    phase_ = Phase::Message;
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
}

void Battle::tryFlee(Player& p) {
    (void)p;
    if (boss_) {
        pushMessage("Musuh menahanmu! Kau tidak bisa kabur.");
        next_ = Next::EnemyTurn;
    } else if (rng_.chance(0.7f)) {
        sfx(SfxId::Flee);
        pushMessage("Kau berhasil kabur!");
        next_ = Next::Fled;
    } else {
        pushMessage("Kau gagal kabur!");
        next_ = Next::EnemyTurn;
    }
    phase_ = Phase::Message;
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
}

void Battle::enemyTurn(Player& p, std::vector<Companion>& party) {
    ++turnCount_;
    if (enemyStun_ > 0) {
        --enemyStun_;
        pushMessage(std::string(info_.name) + " terperangkap gema dan kehilangan giliran!");
        next_ = party.empty() ? Next::Command : Next::CompanionTurn;
        phase_ = Phase::Message;
        msgIndex_ = 0;
        chars_ = 0.0f;
        autoTimer_ = 0.0f;
        return;
    }

    // Choose a target: Raka or one of the alive companions. Bima's guard draws
    // the attack to him while it lasts.
    std::vector<std::string> names;
    std::vector<int*> hps;
    std::vector<int> maxs;
    names.push_back("Raka");
    hps.push_back(&p.hp);
    maxs.push_back(p.maxHp);
    if (party_ && guard_ > 0) {
        for (auto& c : *party_)
            if (c.recruited && c.alive && c.id == CompanionId::Bima) {
                names.push_back(c.name);
                hps.push_back(&c.hp);
                maxs.push_back(c.maxHp);
                break;
            }
    } else if (party_) {
        for (auto& c : *party_)
            if (c.recruited && c.alive) {
                names.push_back(c.name);
                hps.push_back(&c.hp);
                maxs.push_back(c.maxHp);
            }
    }
    int target = static_cast<int>(rng_.range(0, static_cast<int>(names.size()) - 1));

    int def = 0;
    if (names[target] == "Raka") def = p.def();
    else {
        for (auto& c : *party_)
            if (c.recruited && c.name == names[target]) def = c.def;
    }

    int dmg = 0;
    std::string verb = "menyerang";
    bool special = false;
    if (boss_ && turnCount_ % 3 == 0) {
        dmg = damageRoll(rng_, info_.atk * 3 - def, 3);
        verb = "melancarkan serangan penuh";
        special = true;
    } else if (!boss_ && (info_.type == EnemyType::GukAbu || info_.type == EnemyType::ArwahGema) &&
               rng_.chance(0.25f)) {
        dmg = damageRoll(rng_, info_.atk * 2 + 4 - def, 2);
        special = true;
    } else {
        dmg = damageRoll(rng_, info_.atk * 2 - def, 2);
    }
    if (boss_ && bossPhase_ == 1 && enemyHp_ * 2 <= enemyMaxHp_) {
        bossPhase_ = 2;
        pushMessage(std::string(info_.name) + " memasuki fase kedua!");
    }
    if (boss_ && bossPhase_ == 2) dmg = dmg * 5 / 4;
    if (guard_ > 0) {
        dmg = dmg * 3 / 10;
        --guard_;
    }
    if (!boss_ && rng_.chance(0.05f)) dmg = dmg * 3 / 2;
    sfx(special && boss_ ? SfxId::BossRoar : SfxId::Hurt);
    *hps[target] = clampInt(*hps[target] - dmg, 0, maxs[target]);
    shakeT_ = special ? 0.35f : 0.2f;
    flashT_ = 0.15f;
    pushMessage(std::string(info_.name) + " " + verb + " " + names[target] + "! (-" +
                itoa2(dmg) + " HP)");

    if (names[target] == "Raka" && p.hp <= 0) {
        pushMessage("Raka tumbang...");
        next_ = Next::Defeat;
    } else {
        bool companionDown = false;
        if (party_) {
            for (auto& c : *party_) {
                if (c.recruited && c.alive && c.hp <= 0) {
                    c.alive = false;
                    pushMessage(c.name + " tumbang!");
                    companionDown = true;
                }
            }
        }
        (void)companionDown;
        next_ = party_ && !party.empty() ? Next::CompanionTurn : Next::Command;
    }
    phase_ = Phase::Message;
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
}

void Battle::companionTurn(Player& p, std::vector<Companion>& party) {
    if (!party_) {
        next_ = Next::Command;
        phase_ = Phase::Message;
        msgIndex_ = 0;
        chars_ = 0.0f;
        return;
    }
    int def = std::max(0, info_.def - enemyDefDown_);
    for (auto& c : party) {
        if (!c.recruited || !c.alive) continue;
        switch (c.id) {
            case CompanionId::Sekar: {
                int dmg = damageRoll(rng_, c.atk * 2 - def, 2);
                if (rng_.chance(0.25f)) {
                    int dmg2 = damageRoll(rng_, c.atk * 2 - def, 2);
                    dmg += dmg2;
                    pushMessage("Sekar menebas dua kali! (-" + itoa2(dmg) + " HP)");
                } else {
                    pushMessage("Sekar menebas " + std::string(info_.name) + "! (-" +
                                itoa2(dmg) + " HP)");
                }
                enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
                break;
            }
            case CompanionId::Bima: {
                if (rng_.chance(0.25f)) {
                    guard_ = 2;
                    pushMessage("Bima mengangkat Benteng Kapur!");
                } else {
                    int dmg = damageRoll(rng_, c.atk * 2 - def, 2);
                    enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
                    pushMessage("Bima menghantam " + std::string(info_.name) + "! (-" +
                                itoa2(dmg) + " HP)");
                }
                break;
            }
            case CompanionId::Larasati: {
                int lowest = 100;
                bool healRaka = true;
                int before = p.hp;
                if (p.hp * 100 / std::max(1, p.maxHp) < 60) {
                    lowest = p.hp * 100 / std::max(1, p.maxHp);
                }
                for (auto& other : party)
                    if (other.recruited && other.alive) {
                        int pct = other.hp * 100 / std::max(1, other.maxHp);
                        if (pct < lowest) {
                            lowest = pct;
                            healRaka = false;
                        }
                    }
                if (lowest < 60) {
                    int healed = 0;
                    if (healRaka) {
                        int amount = 25 + c.level * 4;
                        p.healHp(amount);
                        healed = p.hp - before;
                        pushMessage("Larasati menyembuhkan Raka. (+" + itoa2(healed) + " HP)");
                    } else {
                        for (auto& other : party)
                            if (other.recruited && other.alive) {
                                int pct = other.hp * 100 / std::max(1, other.maxHp);
                                if (pct == lowest) {
                                    int amount = 25 + c.level * 4;
                                    int b = other.hp;
                                    other.healHp(amount);
                                    pushMessage("Larasati menyembuhkan " + other.name + ". (+" +
                                                itoa2(other.hp - b) + " HP)");
                                    break;
                                }
                            }
                    }
                } else {
                    int dmg = damageRoll(rng_, c.atk * 2 - def, 2);
                    enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
                    pushMessage("Larasati memukul " + std::string(info_.name) + "! (-" +
                                itoa2(dmg) + " HP)");
                }
                break;
            }
            case CompanionId::Bagas: {
                if (rng_.chance(0.25f)) {
                    enemyStun_ = 1;
                    enemyDefDown_ = 2;
                    pushMessage("Bagas membalik giliran dan melemahkan musuh!");
                } else {
                    int dmg = damageRoll(rng_, c.atk * 2 - def, 2);
                    enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
                    pushMessage("Bagas menyerang titik lemah! (-" + itoa2(dmg) + " HP)");
                }
                break;
            }
            default:
                break;
        }
        if (enemyHp_ <= 0) break;
    }

    if (enemyHp_ <= 0) {
        pushMessage(std::string(info_.name) + " dikalahkan!");
        next_ = Next::Victory;
        messages_.push_back("Kemenangan!");
    } else {
        next_ = Next::Command;
    }
    phase_ = Phase::Message;
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
}

void Battle::applyRewards(Player& p) {
    if (rewardsGiven_) return;
    rewardsGiven_ = true;
    xpGained_ = info_.xp;
    goldGained_ = info_.gold;
    p.gold += info_.gold;
    std::string lvl;
    p.gainXp(info_.xp, &lvl);
    levelMsg_ = lvl;
    if (!levelMsg_.empty()) sfx(SfxId::LevelUp);
}

void Battle::finishMessages(Player& p, std::vector<Companion>& party) {
    messages_.clear();
    msgIndex_ = 0;
    chars_ = 0.0f;
    switch (next_) {
        case Next::Command:
            goCommand();
            break;
        case Next::EnemyTurn:
            enemyTurn(p, party);
            break;
        case Next::CompanionTurn:
            companionTurn(p, party);
            break;
        case Next::Victory:
            applyRewards(p);
            phase_ = Phase::Victory;
            sfx(SfxId::Victory);
            break;
        case Next::Defeat:
            phase_ = Phase::Defeat;
            sfx(SfxId::Defeat);
            break;
        case Next::Fled:
            phase_ = Phase::Fled;
            break;
    }
}

void Battle::update(float dt, const InputState& in, Player& p, std::vector<Companion>& party) {
    t_ += dt;
    hurtT_ = std::max(0.0f, hurtT_ - dt);
    shakeT_ = std::max(0.0f, shakeT_ - dt);
    flashT_ = std::max(0.0f, flashT_ - dt);
    healT_ = std::max(0.0f, healT_ - dt);
    effectT_ += dt;

    if (phase_ == Phase::Command || phase_ == Phase::SkillMenu || phase_ == Phase::ItemMenu) {
        if (in.hit(Key::Up) || in.hit(Key::Down) || in.hit(Key::Left) || in.hit(Key::Right)) {
            sfx(SfxId::MenuMove);
        }
        if (in.hit(Key::Confirm)) sfx(SfxId::MenuSelect);
        if (in.hit(Key::Cancel)) sfx(SfxId::MenuCancel);
    } else if (phase_ == Phase::Victory || phase_ == Phase::Defeat || phase_ == Phase::Fled) {
        if (in.hit(Key::Confirm)) sfx(SfxId::MenuSelect);
    }

    switch (phase_) {
        case Phase::Message: {
            if (msgIndex_ >= messages_.size()) {
                finishMessages(p, party);
                break;
            }
            const std::string& msg = messages_[msgIndex_];
            int len = static_cast<int>(msg.size());
            int shown = static_cast<int>(chars_);
            if (shown < len) {
                chars_ += dt * 52.0f;
                autoTimer_ = 0.0f;
            } else {
                autoTimer_ += dt;
            }
            if (in.hit(Key::Confirm)) {
                if (shown < len) {
                    chars_ = static_cast<float>(len);
                    autoTimer_ = 0.0f;
                } else {
                    ++msgIndex_;
                    chars_ = 0.0f;
                    autoTimer_ = 0.0f;
                    if (msgIndex_ >= messages_.size()) finishMessages(p, party);
                }
            } else if (shown >= len && autoTimer_ > 2.6f) {
                ++msgIndex_;
                chars_ = 0.0f;
                autoTimer_ = 0.0f;
                if (msgIndex_ >= messages_.size()) finishMessages(p, party);
            }
            break;
        }
        case Phase::Command: {
            if (in.hit(Key::Up)) menuIndex_ = (menuIndex_ + 3) % 4;
            if (in.hit(Key::Down)) menuIndex_ = (menuIndex_ + 1) % 4;
            if (in.hit(Key::Confirm)) {
                if (menuIndex_ == 0) playerAttack(p);
                else if (menuIndex_ == 1) {
                    phase_ = Phase::SkillMenu;
                    subIndex_ = 0;
                } else if (menuIndex_ == 2) {
                    phase_ = Phase::ItemMenu;
                    subIndex_ = 0;
                } else {
                    tryFlee(p);
                }
            }
            break;
        }
        case Phase::SkillMenu: {
            if (in.hit(Key::Up)) subIndex_ = (subIndex_ + 4) % 5;
            if (in.hit(Key::Down)) subIndex_ = (subIndex_ + 1) % 5;
            if (in.hit(Key::Cancel)) phase_ = Phase::Command;
            if (in.hit(Key::Confirm)) castSkill(p, subIndex_);
            break;
        }
        case Phase::ItemMenu: {
            if (in.hit(Key::Up)) subIndex_ = (subIndex_ + 1) % 2;
            if (in.hit(Key::Down)) subIndex_ = (subIndex_ + 1) % 2;
            if (in.hit(Key::Cancel)) phase_ = Phase::Command;
            if (in.hit(Key::Confirm)) useItem(p, subIndex_ == 1);
            break;
        }
        case Phase::Victory:
            if (in.hit(Key::Confirm)) {
                done_ = true;
                victory_ = true;
            }
            break;
        case Phase::Defeat:
            if (in.hit(Key::Confirm)) {
                done_ = true;
                victory_ = false;
            }
            break;
        case Phase::Fled:
            if (in.hit(Key::Confirm)) {
                done_ = true;
                fled_ = true;
            }
            break;
    }
}

void Battle::render(Framebuffer& fb, const Player& p, const std::vector<Companion>& party,
                    float time) {
    (void)time;
    bool dark = boss_;
    for (int y = 0; y < 240; ++y) {
        float k = static_cast<float>(y) / 240.0f;
        uint32_t c = dark ? mix(rgba(16, 10, 30), rgba(72, 30, 90), k)
                          : mix(rgba(38, 60, 92), rgba(120, 92, 120), k);
        fb.fillRect(0, y, VIEW_W, 1, c);
    }
    fb.fillEllipse(60, 134, 90, 40, dark ? rgba(26, 18, 40) : rgba(52, 76, 78));
    fb.fillEllipse(260, 138, 110, 44, dark ? rgba(32, 20, 48) : rgba(58, 84, 82));
    fb.fillRect(0, 124, VIEW_W, 116, dark ? rgba(24, 16, 36) : rgba(48, 74, 56));
    fb.fillRect(0, 124, VIEW_W, 3, dark ? rgba(60, 30, 80) : rgba(96, 140, 90));

    int shakeX = 0, shakeY = 0;
    if (shakeT_ > 0.0f) {
        shakeX = static_cast<int>(std::sin(t_ * 90.0f) * 3.0f);
        shakeY = static_cast<int>(std::cos(t_ * 70.0f) * 2.0f);
    }

    art::enemy(fb, info_.type, ENEMY_X + shakeX, ENEMY_Y + shakeY, t_, hurtT_ > 0.0f);
    art::playerScaled(fb, HERO_X, HERO_Y, 3, 3, 0.0f, false, p.weapon, p.armor);
    int followerX = HERO_X - 24;
    int followerY = HERO_Y + 6;
    for (const auto& c : party) {
        if (!c.recruited || !c.alive) continue;
        art::companion(fb, c.id, followerX, followerY, t_, false);
        followerX -= 22;
        if (followerX < 4) followerX = HERO_X - 24;
    }

    if (effect_ > 0 && effectT_ < 0.65f) {
        float k = effectT_ / 0.65f;
        int fade = static_cast<int>((1.0f - k) * 255.0f);
        int ex = ENEMY_X, ey = ENEMY_Y - 6;
        if (effect_ == 1) {
            for (int i = 0; i < 8; ++i) {
                float a = i * 0.8f + k * 3.0f;
                int px = ex + static_cast<int>(std::cos(a) * (18 + k * 26));
                int py = ey + static_cast<int>(std::sin(a) * (14 + k * 20)) - static_cast<int>(k * 30);
                fb.fillCircle(px, py, 1 + ((i + 1) % 3), withAlpha(rgba(255, 160, 40), fade));
            }
        } else if (effect_ == 2) {
            int x = ex, y = 0;
            while (y < ey - 20) {
                int nx = x + (hash2(y, static_cast<int>(k * 100)) % 9) - 4;
                fb.line(x, y, nx, y + 8, withAlpha(rgba(220, 230, 255), fade));
                x = nx;
                y += 8;
            }
            fb.fillRectA(0, 0, VIEW_W, 240, pal::white, fade / 4);
        } else if (effect_ == 3) {
            for (int i = 0; i < 10; ++i) {
                int px = HERO_X + 24 + static_cast<int>(std::sin(i * 1.7f + k * 4) * 28);
                int py = HERO_Y + 40 - static_cast<int>(k * 46) + (i % 4) * 6;
                fb.fillRect(px, py, 2, 2, withAlpha(rgba(140, 255, 170), fade));
            }
        } else if (effect_ == 4) {
            int y0 = ey - 26, y1 = ey + 24;
            fb.line(ex - 30, y0 + static_cast<int>(k * 8), ex + 30, y1 - static_cast<int>(k * 8),
                    withAlpha(pal::white, fade));
            fb.line(ex + 26, y0, ex - 26, y1, withAlpha(rgba(255, 240, 200), fade / 2));
        } else if (effect_ == 5) {
            for (int i = 0; i < 14; ++i) {
                float a = i * 0.9f + k * 4.0f;
                int px = ex + static_cast<int>(std::cos(a) * (20 + k * 34));
                int py = ey + static_cast<int>(std::sin(a) * (18 + k * 26));
                fb.fillRect(px, py, 2, 2, withAlpha(rgba(190, 160, 255), fade));
            }
            fb.fillRectA(0, 0, VIEW_W, 240, rgba(40, 20, 60), fade / 5);
        }
    }

    renderEnemyInfo(fb);

    bool showMenu =
        (phase_ == Phase::Command || phase_ == Phase::SkillMenu || phase_ == Phase::ItemMenu);
    if (showMenu) {
        panel(fb, 4, 148, 182, 88);
        renderStatus(fb, p, party, 10, 154, 168);
        panel(fb, 192, 148, 124, 88);
        renderMenu(fb, p, t_);
    }
    if (phase_ == Phase::Message) renderMessage(fb);

    if (phase_ == Phase::Victory) {
        panel(fb, 40, 54, 240, 124);
        fb.text(64, 64, "KEMENANGAN!", pal::uiBorder, 2);
        fb.text(64, 88, std::string(info_.name) + " dikalahkan.", pal::uiText);
        fb.text(64, 102, "XP +" + std::to_string(xpGained_), pal::xp);
        fb.text(64, 114, "Keping Garam +" + std::to_string(goldGained_), pal::gold);
        if (resonanceSpent_) fb.text(64, 126, "Resonansi terkikis.", pal::uiTextDim);
        if (!levelMsg_.empty()) fb.text(64, 142, levelMsg_, pal::hpLight);
        if (static_cast<int>(t_ * 2.0f) % 2 == 0) fb.text(64, 162, "Z: lanjut", pal::uiTextDim);
    } else if (phase_ == Phase::Defeat) {
        panel(fb, 46, 66, 228, 100);
        fb.text(86, 80, "KAU TUMBANG", pal::hp, 2);
        fb.text(70, 112, "Abu menutup pandanganmu...", pal::uiTextDim);
        fb.text(70, 134, "Z: lanjut", pal::uiTextDim);
    } else if (phase_ == Phase::Fled) {
        panel(fb, 76, 86, 168, 64);
        fb.text(96, 100, "Kau berhasil kabur.", pal::uiText);
        fb.text(112, 124, "Z: lanjut", pal::uiTextDim);
    }

    if (flashT_ > 0.0f) {
        int a = static_cast<int>(clampf(flashT_ / 0.2f, 0.0f, 1.0f) * 60.0f);
        fb.fillRectA(0, 0, VIEW_W, 240, pal::white, a);
    }
}

void Battle::renderEnemyInfo(Framebuffer& fb) const {
    panel(fb, 8, 8, 152, 42);
    fb.text(16, 14, info_.name, boss_ ? pal::hp : pal::uiText);
    std::string lv = "Lv " + std::to_string(info_.level);
    fb.text(148 - static_cast<int>(lv.size()) * 6, 14, lv, pal::uiTextDim);
    bar(fb, 16, 28, 132, 8, enemyMaxHp_ > 0 ? static_cast<float>(enemyHp_) / enemyMaxHp_ : 0.0f,
        pal::hp, pal::hpDark);
    if (boss_) fb.text(16, 40, bossPhase_ == 2 ? "BOSS - FASE 2" : "BOSS", pal::uiBorder);
}

void Battle::renderStatus(Framebuffer& fb, const Player& p, const std::vector<Companion>& party,
                          int x, int y, int w) const {
    fb.text(x, y, "RAKA", pal::uiText);
    fb.text(x, y + 11, "HP", pal::uiTextDim);
    bar(fb, x + 18, y + 10, w - 40, 5, p.maxHp > 0 ? static_cast<float>(p.hp) / p.maxHp : 0.0f,
        pal::hp, pal::hpDark);
    fb.text(x + w - 18, y + 11, std::to_string(p.hp), pal::uiTextDim);
    int yy = y + 22;
    for (const auto& c : party) {
        if (!c.recruited) continue;
        fb.text(x, yy, c.name, c.alive ? pal::uiText : pal::uiTextDim);
        bar(fb, x + 58, yy - 1, w - 78, 5,
            c.maxHp > 0 ? static_cast<float>(c.hp) / c.maxHp : 0.0f,
            c.alive ? pal::hp : pal::hpDark, pal::hpDark);
        fb.text(x + w - 18, yy, std::to_string(c.hp), pal::uiTextDim);
        yy += 11;
        if (yy > y + 82) break;
    }
}

void Battle::renderMenu(Framebuffer& fb, const Player& p, float time) const {
    (void)time;
    const int x = 200;
    auto cursor = [&](int rowY) {
        int cy = rowY + 2;
        fb.fillRect(x - 8, cy, 4, 3, pal::uiBorder);
        fb.fillRect(x - 6, cy - 2, 2, 7, pal::uiBorder);
    };
    if (phase_ == Phase::Command) {
        const int y = 156;
        const char* labels[4] = {"Serang", "Gema", "Item", "Kabur"};
        for (int i = 0; i < 4; ++i) {
            int rowY = y + i * 18;
            if (i == menuIndex_) fb.fillRect(x - 4, rowY - 2, 112, 15, pal::uiPanelLight);
            fb.text(x, rowY, labels[i], i == menuIndex_ ? pal::white : pal::uiTextDim);
        }
        cursor(y + menuIndex_ * 18);
        fb.text(x, 224, "Z: pilih", pal::uiTextDim);
    } else if (phase_ == Phase::SkillMenu) {
        const char* labels[5] = {"Api Garam  5MP", "Petir Garam 7MP", "Ledakan Rasa 8MP",
                                 "Kidung Garam 6MP", "Tumbal Rasa 15R"};
        fb.text(x, 150, "Gema", pal::uiBorder);
        for (int i = 0; i < 5; ++i) {
            int rowY = 164 + i * 13;
            if (i == subIndex_) fb.fillRect(x - 4, rowY - 2, 112, 12, pal::uiPanelLight);
            fb.text(x, rowY, labels[i], i == subIndex_ ? pal::white : pal::uiTextDim);
        }
        cursor(164 + subIndex_ * 13);
        fb.text(x, 232, "X: kembali", pal::uiTextDim);
    } else if (phase_ == Phase::ItemMenu) {
        fb.text(x, 150, "Item", pal::uiBorder);
        std::string labels[2] = {"Ramuan  x" + std::to_string(p.potions),
                                 "Eliksir x" + std::to_string(p.elixirs)};
        for (int i = 0; i < 2; ++i) {
            int rowY = 168 + i * 16;
            if (i == subIndex_) fb.fillRect(x - 4, rowY - 2, 112, 14, pal::uiPanelLight);
            fb.text(x, rowY, labels[i], i == subIndex_ ? pal::white : pal::uiTextDim);
        }
        cursor(168 + subIndex_ * 16);
        fb.text(x, 232, "X: kembali", pal::uiTextDim);
    }
}

void Battle::renderMessage(Framebuffer& fb) const {
    panel(fb, 6, 148, 308, 86);
    if (messages_.empty() || msgIndex_ >= messages_.size()) return;
    const std::string& msg = messages_[msgIndex_];
    int shown = std::min<int>(static_cast<int>(chars_), static_cast<int>(msg.size()));
    auto lines = wrapText(msg.substr(0, static_cast<size_t>(shown)), 286, 1);
    int y = 160;
    for (const auto& line : lines) {
        fb.text(16, y, line, pal::uiText);
        y += 11;
    }
    if (shown >= static_cast<int>(msg.size())) {
        if (static_cast<int>(t_ * 3.0f) % 2 == 0) {
            fb.fillRect(296, 218, 4, 4, pal::uiBorder);
            fb.fillRect(294, 220, 8, 2, pal::uiBorder);
        }
    }
}

}  // namespace rpg
