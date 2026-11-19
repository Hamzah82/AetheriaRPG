// game.h - exploration, story, menus, save system, overall state machine
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "audio_synth.h"
#include "battle.h"
#include "core.h"
#include "gfx.h"
#include "party.h"
#include "player.h"
#include "savegame.h"
#include "world.h"

namespace rpg {

class Audio;
class Updater;

enum class GameState { Title, Explore, Dialogue, Battle, Menu, Shop, GameOver, Ending };

struct Flags {
    int act = 0;  // 0 prologue, 1..4 main acts
    int stage = 0;
    int resonance = 70;
    int memories = 0;  // 12-bit mask of memory fragments

    bool introSeen = false;
    bool courierSaved = false;
    bool bimaJoined = false;
    bool sekarJoined = false;
    bool larasJoined = false;
    bool bagasJoined = false;
    bool fatherMemory = false;
    bool nirmalaTruth = false;
    bool kiranaHelped = false;
    bool senoDefect = false;
    bool gunturDefeated = false;
    bool gemaDefeated = false;
    bool gameCompleted = false;

    bool herbQuestAccepted = false;
    bool herbQuestDone = false;
    bool lanternQuestAccepted = false;
    bool lanternQuestDone = false;
    bool sekarQuestAccepted = false;
    bool sekarQuestDone = false;
    bool hiddenQuestDone = false;
    bool gajaminaDefeated = false;

    int endingId = 0;  // 1 true, 2 good, 3 bad, 4 secret
    int senoChoice = 0;
    int sekarChoice = 0;
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

struct RoamingEnemy {
    EnemyType type = EnemyType::KumbangPualam;
    float x = 0.0f, y = 0.0f;
    float homeX = 0.0f, homeY = 0.0f;
    float dirX = 0.0f, dirY = 0.0f;
    float moveTimer = 0.0f;
    float pauseTimer = 0.0f;
    float anim = 0.0f;
    float respawnTimer = 0.0f;
    bool active = true;
};

struct DialogueChoice {
    std::string text;
    int resonanceDelta = 0;
    std::function<void()> action;
};

struct DialogueState {
    std::vector<std::string> pages;
    std::vector<DialogueChoice> choices;
    std::string speaker;
    int page = 0;
    float chars = 0.0f;
    bool active = false;
    bool choosing = false;
    int choiceIndex = 0;
    std::function<void()> onClose;
};

struct ShopItem {
    int kind;  // 0 potion, 1 elixir, 2 oil, 3 weapon, 4 armor
    int index;
    const char* name;
    int price;
};

constexpr int SHOP_COUNT = 7;
extern const ShopItem SHOP[SHOP_COUNT];

class Game {
public:
    enum class TitlePage { Main, Load, ConfirmNew };

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
    int resonance() const { return flags_.resonance; }
    int memoryCount() const;
    bool hasMemory(int id) const;
    int act() const { return flags_.act; }
    int endingId() const { return flags_.endingId; }
    int companionCount() const;
    const Companion& companion(int index) const;

    void newGame();
    bool hasSave() const;
    bool saveGame(std::string* err = nullptr);
    bool loadGame(std::string* err = nullptr);

    int saveSlot() const { return saveSlot_; }
    bool anySaveExists() const;
    bool saveToSlot(int slot, std::string* err = nullptr);
    bool loadFromSlot(int slot, std::string* err = nullptr);
    bool autosave();
    std::string slotSummary(int slot) const;
    std::string questObjective() const;
    std::string sideQuestObjective() const;

    bool takeSettingsDirty();
    void setAudio(Audio* audio);
    void setUpdater(Updater* updater) { updater_ = updater; }
    bool restartRequested() const { return restart_; }

    // Debug/test hooks (also used by the screenshot tool).
    void debugWarp(int map, int tx, int ty);
    void debugSetPlayerPos(float x, float y);
    void debugSetFacing(int facing);
    void debugStartBattle(EnemyType type, bool boss);
    void debugOpenMenu();
    int debugMenuPage() const { return menuPage_; }
    int debugMenuIndex() const { return menuIndex_; }
    int debugTitleIndex() const { return titleIndex_; }
    int debugTitlePage() const { return static_cast<int>(titlePage_); }
    int debugTitleSlot() const { return titleSlotIndex_; }
    int debugTitleConfirm() const { return titleConfirmIndex_; }
    void debugInteract();
    void debugAdvanceDialogue();
    bool debugChestOpened(int map, int index) const;
    bool debugVisited(int map) const {
        return map >= 0 && map < static_cast<int>(visited_.size()) &&
               visited_[static_cast<size_t>(map)];
    }
    void debugOpenShop();
    void debugStartEnding();
    void debugGameOver();
    int debugEnemyCount() const;
    const RoamingEnemy& debugEnemy(int index) const;
    void debugSetAct(int act) { flags_.act = clampInt(act, 0, 4); }
    void debugRecruit(CompanionId id);

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
    void updateFollowers(float dt);

    void startDialogue(const std::string& speaker, const std::vector<std::string>& pages,
                       std::function<void()> onClose = nullptr,
                       const std::vector<DialogueChoice>& choices = {});
    void closeDialogue();
    void applyChoice(int index);

    void startTransition(std::function<void()> action);
    void changeMap(int target, int spawnTileX, int spawnTileY, int facing);

    void moveAxis(float axisX, float axisY);
    bool collides(float nx, float ny) const;
    void onStep();
    void checkPortal();
    void interact();
    void openChest(const ChestSpawn& chest, int index);
    void talkToNpc(const NpcSpawn& npc);
    void talkMbokRatih();
    void talkSeno();
    void talkJoko();
    void talkBima();
    void talkSekar();
    void talkLarasati();
    void talkBagas();
    void talkNirmala();
    void talkKirana();
    void talkAdipati();
    void talkGuntur();
    void talkFlavor(int dialogueId);
    void recruit(CompanionId id, int level);
    void addMemory(int id);
    void readEcho(int id);
    void restCampfire();
    void advanceActIfReady();
    void startBoss(EnemyType type);
    void finishGame();

    void resetRoamingEnemies(float avoidX = -1.0f, float avoidY = -1.0f);
    void spawnRoamingEnemies(int map, float avoidX, float avoidY);
    void updateEnemies(float dt);
    void checkEnemyContact();
    void startEnemyBattle(int index);
    void pickEnemyDirection(RoamingEnemy& enemy);
    bool enemyBlocked(int map, float nx, float ny, int self) const;

    void openMenu();
    void closeMenu();
    void useFieldItem(bool elixir);
    void saveFromMenu(int slot);
    void markVisited(int map);
    bool npcHidden(const NpcSpawn& npc) const;
    bool writeSaveTo(const std::string& path, std::string* err);
    bool readSaveFrom(const std::string& path, std::string* err);

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

    void renderTitle(Framebuffer& fb);
    void renderExplore(Framebuffer& fb);
    void renderDialogue(Framebuffer& fb);
    void renderMenu(Framebuffer& fb);
    void renderJournal(Framebuffer& fb);
    void renderSaveSlots(Framebuffer& fb);
    void renderMinimap(Framebuffer& fb, int x, int y, int w, int h);
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
    Rng enemyRng_;
    std::string saveBase_;
    std::string savePath_;
    int saveSlot_ = 0;
    Audio* audio_ = nullptr;
    Updater* updater_ = nullptr;
    bool quit_ = false;
    bool restart_ = false;
    bool titleUpdateActive_ = false;

    int titleIndex_ = 0;
    TitlePage titlePage_ = TitlePage::Main;
    int titleSlotIndex_ = 1;
    int titleConfirmIndex_ = 1;
    float titleTime_ = 0.0f;
    std::string titleMsg_;
    float titleMsgTimer_ = 0.0f;

    DialogueState dlg_;
    std::vector<Companion> companions_;
    std::vector<Vec2> followers_;

    float stepAccum_ = 0.0f;
    float portalCooldown_ = 0.0f;
    float interactCooldown_ = 0.0f;
    float enemyGrace_ = 0.0f;
    float mapBanner_ = 0.0f;
    std::string mapBannerText_;

    std::vector<std::vector<RoamingEnemy>> enemies_;
    std::vector<std::vector<bool>> chestOpened_;
    std::vector<bool> visited_;
    bool settingsDirty_ = false;

    int menuPage_ = 0;  // 0 root, 1 items, 2 status, 3 journal, 4 save, 5 audio
    int menuIndex_ = 0;
    int saveConfirmSlot_ = -1;
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
