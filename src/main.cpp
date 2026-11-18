// main.cpp - entry point, window loop, headless smoke test, screenshot tool
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "art.h"
#include "game.h"
#include "gfx.h"
#include "platform.h"

using namespace rpg;

namespace {

struct Options {
    int scale = 3;
    int frames = 600;
    uint64_t seed = 20261118ull;
    std::string savePath = "savegame.dat";
    std::string screenshotDir;
    bool headless = false;
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
        } else if (a == "--screenshot") {
            if (i + 1 < argc) o.screenshotDir = argv[++i];
        } else if (a == "--headless") {
            o.headless = true;
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
    Game game(opt.seed, opt.savePath + ".shot");

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

    confirmPress(1);  // Mulai Baru
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

    game.debugWarp(2, 17, 7);
    tick(8);
    save("06_boss_room");

    game.debugStartBattle(EnemyType::Wolf, false);
    confirmPress(2);
    tick(24);
    save("07_battle_wolf");

    game.debugStartBattle(EnemyType::Warden, true);
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

    game.debugWarp(0, 14, 12);
    game.debugOpenShop();
    tick(6);
    save("12_shop");

    game.debugGameOver();
    tick(6);
    save("13_gameover");

    game.debugStartEnding();
    tick(300);
    save("14_ending");
    return 0;
}

void runHeadless(const Options& opt) {
    Game game(opt.seed, opt.savePath);
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

}  // namespace

int main(int argc, char** argv) {
    Options opt = parseArgs(argc, argv);
    if (opt.showHelp) {
        std::printf(
            "Aetheria: Bayangan Terakhir\n"
            "Pemakaian: ./rpgcpp [opsi]\n"
            "  --scale N        perbesaran jendela (1-6, bawaan 3)\n"
            "  --seed N         benih acak\n"
            "  --save PATH      lokasi berkas simpanan\n"
            "  --headless       uji jalan tanpa jendela\n"
            "  --frames N       jumlah frame uji headless\n"
            "  --screenshot DIR tulis tangkapan layar lalu keluar\n");
        return 0;
    }
    if (!opt.screenshotDir.empty()) return runScreenshots(opt);
    if (opt.headless) {
        runHeadless(opt);
        return 0;
    }

    Platform platform;
    std::string err;
    if (!platform.open(opt.scale, "Aetheria: Bayangan Terakhir", &err)) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }

    Game game(opt.seed, opt.savePath);
    Framebuffer fb;
    fb.resize(VIEW_W, VIEW_H);
    InputState in;
    auto last = std::chrono::steady_clock::now();
    while (true) {
        if (!platform.pump(in)) break;
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        dt = clampf(dt, 0.0f, 0.05f);
        game.update(dt, in);
        game.render(fb);
        platform.present(fb);
        platform.frameDelay();
        if (game.quitRequested()) break;
    }
    platform.close();
    return 0;
}
