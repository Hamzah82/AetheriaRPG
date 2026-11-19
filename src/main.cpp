// main.cpp - entry point, window loop, headless smoke test, screenshot tool
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "art.h"
#include "audio.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"
#include "savegame.h"
#include "settings.h"
#include "updater.h"

using namespace rpg;

namespace {

struct Options {
    int scale = 3;
    int frames = 600;
    uint64_t seed = 20261118ull;
    std::string savePath = defaultSavePath();
    std::string settingsPath = defaultSettingsPath();
    std::string screenshotDir;
    bool headless = false;
    bool audio = true;
    bool fullscreen = false;
    bool showHelp = false;
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](int& out) {
            if (i + 1 < argc) out = std::atoi(argv[++i]);
        };
        if (a == "--scale") {
            next(o.scale);
            o.scale = clampInt(o.scale, 1, 6);
        } else if (a == "--frames") {
            next(o.frames);
            if (o.frames < 1) o.frames = 1;
        } else if (a == "--seed") {
            if (i + 1 < argc) o.seed = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--save") {
            if (i + 1 < argc) o.savePath = argv[++i];
        } else if (a == "--settings") {
            if (i + 1 < argc) o.settingsPath = argv[++i];
        } else if (a == "--screenshot") {
            if (i + 1 < argc) o.screenshotDir = argv[++i];
        } else if (a == "--headless") {
            o.headless = true;
        } else if (a == "--no-audio") {
            o.audio = false;
        } else if (a == "--fullscreen") {
            o.fullscreen = true;
        } else if (a == "--help" || a == "-h") {
            o.showHelp = true;
        }
    }
    return o;
}

Framebuffer upscale(const Framebuffer& src, int factor) {
    Framebuffer out;
    out.resize(src.w * factor, src.h * factor);
    for (int y = 0; y < out.h; ++y) {
        const uint32_t* row = src.px.data() + static_cast<size_t>(y / factor) * src.w;
        uint32_t* dst = out.px.data() + static_cast<size_t>(y) * out.w;
        for (int x = 0; x < out.w; ++x) dst[x] = row[x / factor];
    }
    return out;
}

int runScreenshots(const Options& opt) {
    std::error_code ec;
    std::filesystem::create_directories(opt.screenshotDir, ec);
    if (ec && !std::filesystem::exists(opt.screenshotDir)) {
        std::fprintf(stderr, "Tidak bisa membuat folder %s\n", opt.screenshotDir.c_str());
        return 1;
    }
    Framebuffer fb;
    fb.resize(VIEW_W, VIEW_H);
    std::string shotBase = opt.savePath + ".shot";
    for (int slot = -1; slot <= 2; ++slot) {
        std::string path = slotPath(shotBase, slot);
        std::remove(path.c_str());
        std::remove((path + ".tmp").c_str());
    }
    Game game(opt.seed, shotBase);
    // The screenshot run never opens an audio device, but a non-null Audio
    // object lets the audio settings page render with its real controls.
    Audio previewAudio;
    game.setAudio(&previewAudio);

    auto tick = [&](int frames) {
        InputState empty;
        for (int i = 0; i < frames; ++i) game.update(1.0f / 60.0f, empty);
    };
    auto save = [&](const std::string& name) {
        fb.clear(pal::black);
        game.render(fb);
        Framebuffer big = upscale(fb, 3);
        std::string path = opt.screenshotDir + "/" + name + ".png";
        if (!big.writePNG(path)) {
            std::fprintf(stderr, "Gagal menulis %s\n", path.c_str());
        } else {
            std::printf("shot: %s\n", path.c_str());
        }
    };
    auto confirmPress = [&](int times) {
        for (int i = 0; i < times; ++i) {
            InputState in;
            in.pressed[static_cast<int>(Key::Confirm)] = true;
            in.held[static_cast<int>(Key::Confirm)] = true;
            game.update(1.0f / 60.0f, in);
        }
    };
    auto pressKey = [&](Key key) {
        InputState in;
        in.pressed[static_cast<int>(key)] = true;
        in.held[static_cast<int>(key)] = true;
        game.update(1.0f / 60.0f, in);
    };

    tick(20);
    save("01_title");

    game.newGame();
    game.debugAdvanceDialogue();
    tick(10);
    save("02_village");

    game.debugWarp(0, 12, 7);
    tick(4);
    game.debugInteract();
    tick(24);
    save("03_dialogue");
    game.debugAdvanceDialogue();

    game.debugWarp(1, 20, 16);
    tick(8);
    save("04_overworld");

    game.debugWarp(2, 16, 21);
    tick(8);
    save("05_cave");

    // Recruit the party so the remaining screenshots exercise followers and the
    // party battle status.
    game.debugRecruit(CompanionId::Sekar);
    game.debugRecruit(CompanionId::Bima);
    game.debugRecruit(CompanionId::Larasati);
    game.debugRecruit(CompanionId::Bagas);

    game.debugWarp(2, 17, 7);
    tick(8);
    save("06_boss_room");

    game.debugStartBattle(EnemyType::GukAbu, false);
    confirmPress(2);
    tick(24);
    save("07_battle_wolf");

    game.debugStartBattle(EnemyType::Gajamina, true);
    confirmPress(2);
    tick(24);
    save("08_battle_boss");

    game.debugWarp(0, 14, 11);
    game.debugOpenMenu();
    tick(6);
    save("09_menu");
    confirmPress(1);
    tick(6);
    save("10_menu_items");
    pressKey(Key::Cancel);  // back to root
    pressKey(Key::Down);
    pressKey(Key::Confirm);
    tick(4);
    save("11_status");
    pressKey(Key::Cancel);
    pressKey(Key::Down);
    pressKey(Key::Confirm);
    tick(4);
    save("12_journal");
    pressKey(Key::Cancel);
    pressKey(Key::Down);
    pressKey(Key::Confirm);
    confirmPress(1);  // simpan ke Slot 1
    tick(4);
    save("13_save_slots");
    pressKey(Key::Cancel);
    pressKey(Key::Down);
    pressKey(Key::Confirm);
    tick(4);
    save("14_menu_audio");

    game.debugWarp(0, 14, 12);
    game.debugOpenShop();
    tick(6);
    save("15_shop");

    game.debugGameOver();
    tick(6);
    save("16_gameover");

    game.debugWarp(4, 17, 23);
    tick(8);
    save("17_ruins");

    game.debugStartBattle(EnemyType::GemaPertama, true);
    confirmPress(2);
    tick(24);
    save("18_battle_guardian");

    game.debugStartEnding();
    tick(300);
    save("19_ending");
    for (int slot = -1; slot <= 2; ++slot) {
        std::string path = slotPath(shotBase, slot);
        std::remove(path.c_str());
        std::remove((path + ".tmp").c_str());
    }
    return 0;
}

void runHeadless(const Options& opt) {
    Game game(opt.seed, opt.savePath);
    // Start from a fresh game so the smoke test never depends on an existing
    // save (or the new-game confirmation) on the machine running it.
    game.newGame();
    Framebuffer fb;
    fb.resize(VIEW_W, VIEW_H);
    InputState in;
    uint64_t hash = 1469598103934665603ull;
    for (int f = 0; f < opt.frames; ++f) {
        in.clearPressed();
        for (int k = 0; k < KEY_COUNT; ++k) in.held[k] = false;
        int t = f % 420;
        if (t < 40) {
            if (t % 8 == 0) in.pressed[static_cast<int>(Key::Confirm)] = true;
        } else if (t < 160) {
            in.held[static_cast<int>(Key::Right)] = (t / 30) % 2 == 0;
            in.held[static_cast<int>(Key::Up)] = (t / 30) % 2 == 1;
            if (t % 50 == 0) in.pressed[static_cast<int>(Key::Confirm)] = true;
        } else if (t == 170) {
            in.pressed[static_cast<int>(Key::Cancel)] = true;
        } else if (t < 250) {
            if (t == 200) in.pressed[static_cast<int>(Key::Down)] = true;
            if (t == 210) in.pressed[static_cast<int>(Key::Confirm)] = true;
            if (t == 240) in.pressed[static_cast<int>(Key::Cancel)] = true;
        } else if (t < 340) {
            in.held[static_cast<int>(Key::Left)] = true;
            in.held[static_cast<int>(Key::Down)] = true;
        } else if (t == 350) {
            in.pressed[static_cast<int>(Key::Cancel)] = true;
        } else if (t == 400) {
            in.pressed[static_cast<int>(Key::Cancel)] = true;
        }
        game.update(1.0f / 60.0f, in);
        game.render(fb);
        for (uint32_t px : fb.px) hash = (hash ^ px) * 1099511628211ull;
        if (game.quitRequested()) break;
    }
    std::printf("headless ok: frames=%d state=%d map=%d hp=%d checksum=%016llx\n", opt.frames,
                static_cast<int>(game.state()), game.currentMap(), game.player().hp,
                static_cast<unsigned long long>(hash));
}

#ifdef _WIN32
// The game is linked as a GUI application so double-clicking it does not leave
// a console window behind. When it is started from a terminal, borrow that
// console so --headless, --screenshot, and diagnostics keep printing.
void attachParentConsole() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;
    std::freopen("CONOUT$", "w", stdout);
    std::freopen("CONOUT$", "w", stderr);
    std::freopen("CONIN$", "r", stdin);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
}
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    attachParentConsole();
#endif
    Options opt = parseArgs(argc, argv);
    if (opt.showHelp) {
        std::printf(
            "Pelita di Atas Segara Garam\n"
            "Pemakaian: ./rpgcpp [opsi]\n"
            "  --scale N        perbesaran jendela (1-6, bawaan 3)\n"
            "  --seed N         benih acak\n"
            "  --save PATH      lokasi berkas simpanan\n"
            "  --settings PATH  lokasi berkas pengaturan\n"
            "  --headless       uji jalan tanpa jendela\n"
            "  --frames N       jumlah frame uji headless\n"
            "  --no-audio       jalankan tanpa musik dan efek suara\n"
            "  --fullscreen     mulai dalam mode layar penuh\n"
            "  --screenshot DIR tulis tangkapan layar lalu keluar\n");
        return 0;
    }
    if (!opt.screenshotDir.empty()) return runScreenshots(opt);
    if (opt.headless) {
        runHeadless(opt);
        return 0;
    }

    Settings settings = loadSettings(opt.settingsPath);
    bool wantFullscreen = opt.fullscreen || settings.fullscreen;

    Platform platform;
    std::string err;
    if (!platform.open(opt.scale, "Pelita di Atas Segara Garam", &err, wantFullscreen)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }

    Audio audio;
    if (opt.audio) {
        std::string audioErr;
        if (audio.init(&audioErr)) {
            audio.setMusicVolume(settings.musicVolume);
            audio.setSfxVolume(settings.sfxVolume);
            audio.setMuted(settings.muted);
            std::printf("audio: aktif (prosedural, 44100 Hz mono)\n");
        } else {
            std::fprintf(stderr, "audio: %s\n", audioErr.c_str());
        }
    }

    Game game(opt.seed, opt.savePath);
    if (audio.available()) game.setAudio(&audio);
    Updater updater;
    game.setUpdater(&updater);
    Framebuffer fb;
    fb.resize(VIEW_W, VIEW_H);
    InputState in;
    bool audioPaused = false;
    auto last = std::chrono::steady_clock::now();
    while (true) {
        if (!platform.pump(in)) break;
        auto now = std::chrono::steady_clock::now();
        if (!platform.focused()) {
            // Freeze simulation and audio while another window has focus, and
            // keep the clock current so refocusing never produces a dt spike.
            if (!audioPaused) {
                audio.setPaused(true);
                audioPaused = true;
            }
            last = now;
            platform.frameDelay();
            continue;
        }
        if (audioPaused) {
            audio.setPaused(false);
            audioPaused = false;
        }
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        dt = clampf(dt, 0.0f, 0.05f);
        game.update(dt, in);
        game.render(fb);
        platform.present(fb);
        platform.frameDelay();

        if (platform.takeFullscreenChanged()) {
            settings.fullscreen = platform.isFullscreen();
            saveSettings(opt.settingsPath, settings);
        }
        if (game.takeSettingsDirty() && audio.available()) {
            settings.musicVolume = audio.musicVolume();
            settings.sfxVolume = audio.sfxVolume();
            settings.muted = audio.muted();
            saveSettings(opt.settingsPath, settings);
        }
        if (game.quitRequested() || game.restartRequested()) break;
    }

    bool restart = game.restartRequested();
    if (audio.available() && !audioPaused) {
        settings.musicVolume = audio.musicVolume();
        settings.sfxVolume = audio.sfxVolume();
        settings.muted = audio.muted();
    }
    settings.fullscreen = platform.isFullscreen();
    saveSettings(opt.settingsPath, settings);
    platform.close();
    audio.shutdown();

    if (restart) {
        // The running executable may already be a replaced file after an
        // update, so prefer the path the updater just produced.
        std::string target = updater.restartPath();
#ifdef _WIN32
        if (target.empty()) {
            std::vector<char> module(MAX_PATH);
            for (;;) {
                DWORD n = GetModuleFileNameA(nullptr, module.data(),
                                             static_cast<DWORD>(module.size()));
                if (n == 0) break;
                if (n < module.size()) {
                    target.assign(module.data(), n);
                    break;
                }
                module.resize(module.size() * 2);
            }
        }
#else
        if (target.empty()) target = "/proc/self/exe";
#endif
        std::printf("memulai ulang: %s\n", target.c_str());
        std::fflush(stdout);
#ifdef _WIN32
        std::vector<const char*> childArgs;
        childArgs.reserve(static_cast<size_t>(argc) + 1);
        for (int i = 0; i < argc; ++i) childArgs.push_back(argv[i]);
        childArgs.push_back(nullptr);
        if (_spawnv(_P_NOWAIT, target.c_str(), childArgs.data()) == -1) {
            std::fprintf(stderr, "gagal memulai ulang %s: %s\n", target.c_str(),
                         std::strerror(errno));
            return 1;
        }
        return 0;
#else
        execv(target.c_str(), argv);
        std::fprintf(stderr, "gagal memulai ulang %s: %s\n", target.c_str(),
                     std::strerror(errno));
        return 1;
#endif
    }
    return 0;
}
