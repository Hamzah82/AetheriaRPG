// battle.cpp - turn based combat logic and presentation
#include "battle.h"

#include <cmath>

#include "art.h"
#include "audio.h"

namespace rpg {

namespace {
const EnemyInfo ENEMIES[] = {
    {EnemyType::Slime, "Slime", 1, 18, 5, 1, 8, 5, 1},
    {EnemyType::Bat, "Kelelawar", 2, 22, 7, 2, 12, 8, 2},
    {EnemyType::Wolf, "Serigala", 3, 34, 10, 3, 20, 15, 1},
    {EnemyType::Skeleton, "Tengkorak", 4, 46, 13, 6, 32, 26, 2},
    {EnemyType::Warden, "Warden Bayangan", 8, 180, 18, 8, 150, 250, 0},
};
constexpr int ENEMY_COUNT = 5;

const int ENEMY_X = 214;
const int ENEMY_Y = 124;
const int HERO_X = 36;
const int HERO_Y = 112;

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

void Battle::begin(Player& p, EnemyType type, bool boss, uint64_t seed) {
    (void)p;
    info_ = enemyInfo(type);
    boss_ = boss;
    enemyMaxHp_ = info_.hp;
    enemyHp_ = info_.hp;
    rng_ = Rng(seed);
    phase_ = Phase::Message;
    next_ = Next::Command;
    messages_.clear();
    pushMessage(std::string(info_.name) + " muncul!");
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
    menuIndex_ = 0;
    subIndex_ = 0;
    turnCount_ = 0;
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
    int dmg = damageRoll(rng_, p.atk() * 2 - info_.def, 2);
    if (crit) dmg = dmg * 3 / 2;
    sfx(crit ? SfxId::Crit : SfxId::Attack);
    enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
    effect_ = 4;
    effectT_ = 0.0f;
    hurtT_ = 0.25f;
    shakeT_ = 0.18f;
    pushMessage("Kau menebas " + std::string(info_.name) + "! (-" + itoa2(dmg) + " HP)");
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

void Battle::castFire(Player& p) {
    const int cost = 5;
    if (p.mp < cost) {
        pushMessage("MP tidak cukup untuk Bola Api!");
        next_ = Next::Command;
        phase_ = Phase::Message;
        msgIndex_ = 0;
        chars_ = 0.0f;
        return;
    }
    p.mp -= cost;
    sfx(SfxId::Fire);
    int dmg = damageRoll(rng_, 10 + p.level * 4, 3);
    bool weak = info_.weak == 1;
    if (weak) dmg = dmg * 3 / 2;
    enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
    effect_ = 1;
    effectT_ = 0.0f;
    hurtT_ = 0.3f;
    flashT_ = 0.12f;
    pushMessage("Bola api menghantam " + std::string(info_.name) + "! (-" + itoa2(dmg) + " HP)");
    if (weak) pushMessage("Super efektif!");
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

void Battle::castThunder(Player& p) {
    const int cost = 7;
    if (p.mp < cost) {
        pushMessage("MP tidak cukup untuk Petir!");
        next_ = Next::Command;
        phase_ = Phase::Message;
        msgIndex_ = 0;
        chars_ = 0.0f;
        return;
    }
    p.mp -= cost;
    sfx(SfxId::Thunder);
    int dmg = damageRoll(rng_, 14 + p.level * 4, 3);
    bool weak = info_.weak == 2;
    if (weak) dmg = dmg * 3 / 2;
    enemyHp_ = clampInt(enemyHp_ - dmg, 0, enemyMaxHp_);
    effect_ = 2;
    effectT_ = 0.0f;
    hurtT_ = 0.3f;
    flashT_ = 0.2f;
    shakeT_ = 0.2f;
    pushMessage("Petir menyambar " + std::string(info_.name) + "! (-" + itoa2(dmg) + " HP)");
    if (weak) pushMessage("Super efektif!");
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

void Battle::castHeal(Player& p) {
    const int cost = 5;
    if (p.mp < cost) {
        pushMessage("MP tidak cukup untuk Pulihkan!");
        next_ = Next::Command;
        phase_ = Phase::Message;
        msgIndex_ = 0;
        chars_ = 0.0f;
        return;
    }
    p.mp -= cost;
    sfx(SfxId::Heal);
    int amount = 20 + p.level * 5;
    int before = p.hp;
    p.healHp(amount);
    int healed = p.hp - before;
    effect_ = 3;
    effectT_ = 0.0f;
    healT_ = 0.5f;
    pushMessage("Cahaya memulihkanmu. (+" + itoa2(healed) + " HP)");
    next_ = Next::EnemyTurn;
    phase_ = Phase::Message;
    msgIndex_ = 0;
    chars_ = 0.0f;
    autoTimer_ = 0.0f;
}

void Battle::useItem(Player& p, bool elixir) {
    if (elixir) {
        if (p.elixirs <= 0 || p.mp >= p.maxMp) {
            pushMessage(p.elixirs <= 0 ? "Kau tidak punya Eliksir." : "MP sudah penuh.");
            next_ = Next::Command;
        } else {
            p.useElixir();
            effect_ = 3;
            effectT_ = 0.0f;
            healT_ = 0.5f;
            sfx(SfxId::Potion);
            pushMessage("Kau memakai Eliksir. (+25 MP)");
            next_ = Next::EnemyTurn;
        }
    } else {
        if (p.potions <= 0 || p.hp >= p.maxHp) {
            pushMessage(p.potions <= 0 ? "Kau tidak punya Ramuan." : "HP sudah penuh.");
            next_ = Next::Command;
        } else {
            p.usePotion();
            effect_ = 3;
            effectT_ = 0.0f;
            healT_ = 0.5f;
            sfx(SfxId::Potion);
            pushMessage("Kau memakai Ramuan. (+40 HP)");
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
        pushMessage("Bayangan menahanmu! Kau tidak bisa kabur.");
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

void Battle::enemyTurn(Player& p) {
    ++turnCount_;
    int dmg = 0;
    std::string verb = "menyerangmu";
    bool special = false;
    if (boss_ && turnCount_ % 3 == 0) {
        dmg = damageRoll(rng_, info_.atk * 3 - p.def(), 3);
        verb = "melancarkan Hantaman Bayangan";
        special = true;
    } else if (!boss_ && (info_.type == EnemyType::Wolf || info_.type == EnemyType::Skeleton) &&
               rng_.chance(0.25f)) {
        dmg = damageRoll(rng_, info_.atk * 2 + 4 - p.def(), 2);
        verb = info_.type == EnemyType::Wolf ? "menerkammu" : "mengayun tulangnya";
        special = true;
    } else {
        dmg = damageRoll(rng_, info_.atk * 2 - p.def(), 2);
    }
    if (!boss_ && rng_.chance(0.05f)) dmg = dmg * 3 / 2;
    sfx(special && boss_ ? SfxId::BossRoar : SfxId::Hurt);
    p.hp = clampInt(p.hp - dmg, 0, p.maxHp);
    shakeT_ = special ? 0.35f : 0.2f;
    flashT_ = 0.15f;
    pushMessage(std::string(info_.name) + " " + verb + "! (-" + itoa2(dmg) + " HP)");
    if (p.hp <= 0) {
        pushMessage("Kau tumbang...");
        next_ = Next::Defeat;
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

void Battle::finishMessages(Player& p) {
    // Messages for this step are done; never replay them on the next phase.
    messages_.clear();
    msgIndex_ = 0;
    chars_ = 0.0f;
    switch (next_) {
        case Next::Command:
            goCommand();
            break;
        case Next::EnemyTurn:
            enemyTurn(p);
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

void Battle::update(float dt, const InputState& in, Player& p) {
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
                finishMessages(p);
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
                    if (msgIndex_ >= messages_.size()) finishMessages(p);
                }
            } else if (shown >= len && autoTimer_ > 2.6f) {
                ++msgIndex_;
                chars_ = 0.0f;
                autoTimer_ = 0.0f;
                if (msgIndex_ >= messages_.size()) finishMessages(p);
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
            if (in.hit(Key::Up)) subIndex_ = (subIndex_ + 2) % 3;
            if (in.hit(Key::Down)) subIndex_ = (subIndex_ + 1) % 3;
            if (in.hit(Key::Cancel)) phase_ = Phase::Command;
            if (in.hit(Key::Confirm)) {
                if (subIndex_ == 0) castFire(p);
                else if (subIndex_ == 1) castThunder(p);
                else castHeal(p);
            }
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

void Battle::render(Framebuffer& fb, const Player& p, float time) {
    (void)time;
    bool dark = boss_;
    for (int y = 0; y < 240; ++y) {
        float k = static_cast<float>(y) / 240.0f;
        uint32_t c = dark ? mix(rgba(16, 10, 30), rgba(72, 30, 90), k)
                          : mix(rgba(38, 60, 92), rgba(120, 92, 120), k);
        fb.fillRect(0, y, VIEW_W, 1, c);
    }
    // Distant hills on the horizon.
    fb.fillEllipse(60, 140, 90, 40, dark ? rgba(26, 18, 40) : rgba(52, 76, 78));
    fb.fillEllipse(260, 144, 110, 44, dark ? rgba(32, 20, 48) : rgba(58, 84, 82));
    fb.fillRect(0, 130, VIEW_W, 110, dark ? rgba(24, 16, 36) : rgba(48, 74, 56));
    fb.fillRect(0, 130, VIEW_W, 3, dark ? rgba(60, 30, 80) : rgba(96, 140, 90));

    int shakeX = 0, shakeY = 0;
    if (shakeT_ > 0.0f) {
        shakeX = static_cast<int>(std::sin(t_ * 90.0f) * 3.0f);
        shakeY = static_cast<int>(std::cos(t_ * 70.0f) * 2.0f);
    }

    art::enemy(fb, info_.type, ENEMY_X + shakeX, ENEMY_Y + shakeY, t_, hurtT_ > 0.0f);
    art::playerScaled(fb, HERO_X, HERO_Y, 3, 3, 0.0f, false);

    // Attack effects.
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
        }
    }

    renderEnemyInfo(fb);

    bool showMenu = (phase_ == Phase::Command || phase_ == Phase::SkillMenu || phase_ == Phase::ItemMenu);
    if (showMenu) {
        panel(fb, 4, 166, 312, 70);
        renderStatus(fb, p, 12, 174, 132);
        renderMenu(fb, p, t_);
    }
    if (phase_ == Phase::Message) renderMessage(fb);

    if (phase_ == Phase::Victory) {
        panel(fb, 46, 62, 228, 116);
        fb.text(70, 72, "KEMENANGAN!", pal::uiBorder, 2);
        fb.text(70, 96, std::string(info_.name) + " dikalahkan.", pal::uiText);
        fb.text(70, 110, "XP +" + std::to_string(xpGained_), pal::xp);
        fb.text(70, 122, "Emas +" + std::to_string(goldGained_), pal::gold);
        if (!levelMsg_.empty()) fb.text(70, 138, levelMsg_, pal::hpLight);
        if (static_cast<int>(t_ * 2.0f) % 2 == 0) fb.text(70, 156, "Z: lanjut", pal::uiTextDim);
    } else if (phase_ == Phase::Defeat) {
        panel(fb, 46, 70, 228, 96);
        fb.text(86, 84, "KAU TUMBANG", pal::hp, 2);
        fb.text(70, 116, "Kegelapan menutup pandanganmu...", pal::uiTextDim);
        fb.text(70, 138, "Z: lanjut", pal::uiTextDim);
    } else if (phase_ == Phase::Fled) {
        panel(fb, 76, 86, 168, 64);
        fb.text(96, 100, "Kau berhasil kabur.", pal::uiText);
        fb.text(112, 124, "Z: lanjut", pal::uiTextDim);
    }

    if (flashT_ > 0.0f) {
        int a = static_cast<int>(clampf(flashT_ / 0.2f, 0.0f, 1.0f) * 60.0f);
        fb.fillRectA(0, 0, VIEW_W, VIEW_H, pal::white, a);
    }
}

void Battle::renderEnemyInfo(Framebuffer& fb) const {
    panel(fb, 8, 8, 152, 42);
    fb.text(16, 14, info_.name, boss_ ? pal::hp : pal::uiText);
    std::string lv = "Lv " + std::to_string(info_.level);
    fb.text(148 - static_cast<int>(lv.size()) * 6, 14, lv, pal::uiTextDim);
    bar(fb, 16, 28, 132, 8, enemyMaxHp_ > 0 ? static_cast<float>(enemyHp_) / enemyMaxHp_ : 0.0f,
        pal::hp, pal::hpDark);
    if (boss_) {
        fb.text(16, 40, "BOSS", pal::uiBorder);
    }
}

void Battle::renderStatus(Framebuffer& fb, const Player& p, int x, int y, int w) const {
    fb.text(x, y, "BAGUS", pal::uiText);
    std::string lv = "Lv " + std::to_string(p.level);
    fb.text(x + w - static_cast<int>(lv.size()) * 6, y, lv, pal::uiTextDim);
    fb.text(x, y + 12, "HP", pal::uiTextDim);
    bar(fb, x + 20, y + 11, w - 24, 8, p.maxHp > 0 ? static_cast<float>(p.hp) / p.maxHp : 0.0f,
        pal::hp, pal::hpDark);
    fb.text(x, y + 24, "MP", pal::uiTextDim);
    bar(fb, x + 20, y + 23, w - 24, 8, p.maxMp > 0 ? static_cast<float>(p.mp) / p.maxMp : 0.0f,
        pal::mp, pal::mpDark);
    fb.text(x, y + 36, std::to_string(p.hp) + "/" + std::to_string(p.maxHp), pal::uiTextDim);
}

void Battle::renderMenu(Framebuffer& fb, const Player& p, float time) const {
    (void)time;
    const int x = 158;
    auto cursor = [&](int rowY) {
        int cy = rowY + 2;
        fb.fillRect(x - 10, cy, 4, 3, pal::uiBorder);
        fb.fillRect(x - 8, cy - 2, 2, 7, pal::uiBorder);
    };
    if (phase_ == Phase::Command) {
        const int y = 174;
        const char* labels[4] = {"Serang", "Sihir", "Item", "Kabur"};
        for (int i = 0; i < 4; ++i) {
            int rowY = y + i * 12;
            if (i == menuIndex_) fb.fillRect(x - 4, rowY - 2, 152, 11, pal::uiPanelLight);
            fb.text(x, rowY, labels[i], i == menuIndex_ ? pal::white : pal::uiTextDim);
        }
        cursor(y + menuIndex_ * 12);
        fb.text(x, 222, "Z: pilih", pal::uiTextDim);
    } else if (phase_ == Phase::SkillMenu) {
        const char* labels[3] = {"Bola Api  5MP", "Petir     7MP", "Pulihkan  5MP"};
        fb.text(x, 168, "Sihir", pal::uiBorder);
        for (int i = 0; i < 3; ++i) {
            int rowY = 180 + i * 11;
            if (i == subIndex_) fb.fillRect(x - 4, rowY - 2, 152, 11, pal::uiPanelLight);
            fb.text(x, rowY, labels[i], i == subIndex_ ? pal::white : pal::uiTextDim);
        }
        cursor(180 + subIndex_ * 11);
        fb.text(x, 218, "X: kembali", pal::uiTextDim);
    } else if (phase_ == Phase::ItemMenu) {
        fb.text(x, 168, "Item", pal::uiBorder);
        std::string labels[2] = {"Ramuan  x" + std::to_string(p.potions),
                                 "Eliksir x" + std::to_string(p.elixirs)};
        for (int i = 0; i < 2; ++i) {
            int rowY = 182 + i * 12;
            if (i == subIndex_) fb.fillRect(x - 4, rowY - 2, 152, 11, pal::uiPanelLight);
            fb.text(x, rowY, labels[i], i == subIndex_ ? pal::white : pal::uiTextDim);
        }
        cursor(182 + subIndex_ * 12);
        fb.text(x, 218, "X: kembali", pal::uiTextDim);
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
