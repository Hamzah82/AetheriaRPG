// game.h - world exploration, menus, save system, overall state machine
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "audio_synth.h"
#include "battle.h"
#include "core.h"
#include "gfx.h"
#include "player.h"
#include "world.h"

namespace rpg {

class Audio;

enum class GameState { Title, Explore, Dialogue, Battle, Menu, Shop, GameOver, Ending };

struct Flags {
    bool questAccepted = false;
    bool bossDefeated = false;
    bool sunstoneFound = false;
    bool gameCompleted = false;
    bool metElder = false;
    bool introSeen = false;
};

struct Particle {
    Vec2 pos;
    Vec2 vel;
    float life = 0.0f;
    float maxLife = 1.0f;
    uint32_t color = 0xFFFFFFFF;
    int size = 1;
    float gravity = 0.0f;
};

struct FlyingText {
    Vec2 pos;
    float life = 0.0f;
    std::string text;
    uint32_t color = 0xFFFFFFFF;
};

struct DialogueState {
    std::vector<std::string> pages;
    std::string speaker;
    int page = 0;
    float chars = 0.0f;
    bool active = false;
    std::function<void()> onClose;
};

struct ShopItem {
    int kind;  // 0 potion, 1 elixir, 2 weapon, 3 armor
    int index;
    const char* name;
    int price;
};

constexpr int SHOP_COUNT = 6;
extern const ShopItem SHOP[SHOP_COUNT];

class Game {
public:
    Game(uint64_t seed, std::string savePath);

    void update(float dt, const InputState& in);
    void render(Framebuffer& fb);

    bool quitRequested() const { return quit_; }
    GameState state() const { return state_; }
    int currentMap() const { return map_; }
    float playerX() const { return px_; }
    float playerY() const { return py_; }
    const Player& player() const { return player_; }
    Player& player() { return player_; }
    const World& world() const { return world_; }
    const Flags& flags() const { return flags_; }
    Flags& flagsMutable() { return flags_; }
    const DialogueState& dialogue() const { return dlg_; }
    const Battle& battle() const { return battle_; }
    Battle& battle() { return battle_; }
    bool transitionActive() const { return transition_; }

    void newGame();
    bool hasSave() const;
    bool saveGame(std::string* err = nullptr);
    bool loadGame(std::string* err = nullptr);

    // Audio is optional; passing nullptr (or never calling this) keeps the
    // game completely silent, which tests and screenshot runs rely on.
    void setAudio(Audio* audio);

    // Debug/test hooks (also used by the screenshot tool).
    void debugWarp(int map, int tx, int ty);
    void debugSetFacing(int facing);
    void debugStartBattle(EnemyType type, bool boss);
    void debugOpenMenu();
    int debugMenuPage() const { return menuPage_; }
    int debugMenuIndex() const { return menuIndex_; }
    void debugInteract();
    void debugAdvanceDialogue();
    bool debugChestOpened(int map, int index) const;
    void debugOpenShop();
    void debugStartEnding();
    void debugGameOver();

private:
    void updateTitle(float dt, const InputState& in);
    void updateExplore(float dt, const InputState& in);
    void updateDialogue(float dt, const InputState& in);
    void updateBattle(float dt, const InputState& in);
    void updateMenu(float dt, const InputState& in);
    void updateShop(float dt, const InputState& in);
    void updateGameOver(float dt, const InputState& in);
    void updateEnding(float dt, const InputState& in);

    void updateParticles(float dt);
    void updateTransition(float dt);

    void startDialogue(const std::string& speaker, const std::vector<std::string>& pages,
                       std::function<void()> onClose = nullptr);
    void closeDialogue();

    void startTransition(std::function<void()> action);
    void changeMap(int target, int spawnTileX, int spawnTileY, int facing);

    void moveAxis(float axisX, float axisY);
    bool collides(float nx, float ny) const;
    void onStep();
    void checkPortal();
    void interact();
    void openChest(const ChestSpawn& chest, int index);
    void elderTalk();
    void innTalk();
    void guardTalk();
    void wardenTalk();
    void flavorTalk(int dialogueId);
    void pickRandomEnemy(EnemyType* type);

    void openMenu();
    void closeMenu();
    void useFieldItem(bool elixir);
    void saveFromMenu();

    void spawnDust();
    void spawnBurst(float x, float y, int count, uint32_t color, float speed);
    void addFloat(float x, float y, const std::string& text, uint32_t color);

    void updateCamera(float dt);
    void clampPlayerToMap();
    void updateMusic();
    void menuFeedback(const InputState& in);
    void playSfx(SfxId id);
    int tileOfX(float x) const { return static_cast<int>(std::floor(x / TILE)); }
    int tileOfY(float y) const { return static_cast<int>(std::floor(y / TILE)); }

    // Render passes.
    void renderTitle(Framebuffer& fb);
    void renderExplore(Framebuffer& fb);
    void renderDialogue(Framebuffer& fb);
    void renderMenu(Framebuffer& fb);
    void renderShop(Framebuffer& fb);
    void renderGameOver(Framebuffer& fb);
    void renderEnding(Framebuffer& fb);
    void renderTransition(Framebuffer& fb);

    World world_;
    Player player_;
    Flags flags_;
    Battle battle_;
    GameState state_ = GameState::Title;
    int map_ = 0;
    float px_ = 224.0f;
    float py_ = 288.0f;
    int facing_ = 1;
    float animPhase_ = 0.0f;
    bool moving_ = false;

    Vec2 camera_;
    Rng rng_;
    std::string savePath_;
    Audio* audio_ = nullptr;
    bool quit_ = false;

    int titleIndex_ = 0;
    float titleTime_ = 0.0f;
    std::string titleMsg_;
    float titleMsgTimer_ = 0.0f;

    DialogueState dlg_;

    float stepAccum_ = 0.0f;
    int stepsSinceBattle_ = 0;
    float portalCooldown_ = 0.0f;
    float interactCooldown_ = 0.0f;
    float mapBanner_ = 0.0f;
    std::string mapBannerText_;

    std::vector<std::vector<bool>> chestOpened_;

    int menuPage_ = 0;  // 0 root, 1 items, 2 status, 3 audio
    int menuIndex_ = 0;
    std::string menuMessage_;
    float menuMessageTimer_ = 0.0f;

    int shopIndex_ = 0;
    std::string shopMessage_;
    float shopMessageTimer_ = 0.0f;

    bool transition_ = false;
    float transitionT_ = 0.0f;
    bool transitionMid_ = false;
    std::function<void()> transitionAction_;

    float flash_ = 0.0f;
    float shake_ = 0.0f;

    std::vector<Particle> particles_;
    std::vector<FlyingText> floats_;
    float dustTimer_ = 0.0f;

    float endingScroll_ = 0.0f;
    int gameOverIndex_ = 0;
    float globalTime_ = 0.0f;
};

}  // namespace rpg
