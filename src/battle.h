// battle.h - turn based combat
#pragma once
#include <string>
#include <vector>

#include "audio_synth.h"
#include "core.h"
#include "gfx.h"
#include "player.h"
#include "world.h"

namespace rpg {

class Audio;

struct EnemyInfo {
    EnemyType type;
    const char* name;
    int level;
    int hp;
    int atk;
    int def;
    int xp;
    int gold;
    int weak;  // 0 none, 1 fire, 2 thunder
};

const EnemyInfo& enemyInfo(EnemyType type);

class Battle {
public:
    enum class Phase { Message, Command, SkillMenu, ItemMenu, Victory, Defeat, Fled };

    void begin(Player& p, EnemyType type, bool boss, uint64_t seed);
    void setAudio(Audio* audio) { audio_ = audio; }
    void update(float dt, const InputState& in, Player& p);
    void render(Framebuffer& fb, const Player& p, float time);

    bool done() const { return done_; }
    bool victory() const { return victory_; }
    bool fled() const { return fled_; }
    bool isBoss() const { return boss_; }
    EnemyType enemyType() const { return info_.type; }
    Phase phase() const { return phase_; }
    int enemyHp() const { return enemyHp_; }
    int enemyMaxHp() const { return enemyMaxHp_; }

    // Debug/test hooks.
    void forceEnemyHp(int hp) { enemyHp_ = clampInt(hp, 0, enemyMaxHp_); }
    void forceCommandPhase() {
        phase_ = Phase::Command;
        messages_.clear();
        msgIndex_ = 0;
    }

private:
    enum class Next { Command, EnemyTurn, Victory, Defeat, Fled };

    void pushMessage(const std::string& m);
    void sfx(SfxId id);
    void finishMessages(Player& p);
    void goCommand();
    void playerAttack(Player& p);
    void castFire(Player& p);
    void castThunder(Player& p);
    void castHeal(Player& p);
    void useItem(Player& p, bool elixir);
    void tryFlee(Player& p);
    void enemyTurn(Player& p);
    void applyRewards(Player& p);

    void renderPanel(Framebuffer& fb, int x, int y, int w, int h) const;
    void renderMenu(Framebuffer& fb, const Player& p, float time) const;
    void renderMessage(Framebuffer& fb) const;
    void renderEnemyInfo(Framebuffer& fb) const;
    void renderStatus(Framebuffer& fb, const Player& p, int x, int y, int w) const;

    Audio* audio_ = nullptr;
    EnemyInfo info_{EnemyType::Slime, "Slime", 1, 10, 3, 1, 5, 3, 0};
    int enemyHp_ = 10;
    int enemyMaxHp_ = 10;
    bool boss_ = false;

    Phase phase_ = Phase::Command;
    Next next_ = Next::Command;

    std::vector<std::string> messages_;
    size_t msgIndex_ = 0;
    float chars_ = 0.0f;
    float autoTimer_ = 0.0f;

    int menuIndex_ = 0;
    int subIndex_ = 0;
    int turnCount_ = 0;

    Rng rng_{1};
    float t_ = 0.0f;
    float hurtT_ = 0.0f;
    float shakeT_ = 0.0f;
    float flashT_ = 0.0f;
    float healT_ = 0.0f;
    int effect_ = 0;  // 1 fire, 2 thunder, 3 heal, 4 slash
    float effectT_ = 0.0f;

    int xpGained_ = 0;
    int goldGained_ = 0;
    std::string levelMsg_;
    bool rewardsGiven_ = false;

    bool done_ = false;
    bool victory_ = false;
    bool fled_ = false;
};

}  // namespace rpg
