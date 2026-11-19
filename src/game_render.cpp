// game_render.cpp - all screens
#include <algorithm>
#include <cmath>

#include "art.h"
#include "audio.h"
#include "game.h"
#include "updater.h"

namespace rpg {

namespace {

void panel(Framebuffer& fb, int x, int y, int w, int h) {
    fb.fillRectA(x, y, w, h, pal::uiPanel, 242);
    fb.outline(x, y, w, h, pal::uiBorder);
    fb.outline(x + 1, y + 1, w - 2, h - 2, 0xFF39405C);
    fb.fillRect(x + 2, y + 2, 2, 2, pal::uiBorder);
    fb.fillRect(x + w - 4, y + 2, 2, 2, pal::uiBorder);
    fb.fillRect(x + 2, y + h - 4, 2, 2, pal::uiBorder);
    fb.fillRect(x + w - 4, y + h - 4, 2, 2, pal::uiBorder);
}

void bar(Framebuffer& fb, int x, int y, int w, int h, float frac, uint32_t col, uint32_t dark) {
    fb.fillRect(x, y, w, h, dark);
    int fill = clampInt(static_cast<int>(w * clampf(frac, 0.0f, 1.0f)), 0, w);
    if (fill > 0) {
        fb.fillRect(x, y, fill, h, col);
        fb.fillRect(x, y, fill, 1, mix(col, pal::white, 0.4f));
    }
    fb.outline(x, y, w, h, 0xFF0A0C14);
}

void centered(Framebuffer& fb, int y, const std::string& s, uint32_t c, int scale = 1) {
    fb.text((VIEW_W - fb.textWidth(s, scale)) / 2, y, s, c, scale);
}

void dim(Framebuffer& fb, int alpha) { fb.fillRectA(0, 0, VIEW_W, VIEW_H, pal::black, alpha); }

void cursorArrow(Framebuffer& fb, int x, int y, uint32_t c) {
    fb.fillRect(x, y, 4, 1, c);
    fb.fillRect(x + 1, y + 1, 2, 1, c);
    fb.fillRect(x + 2, y + 2, 1, 1, c);
    fb.fillRect(x + 1, y + 3, 2, 1, c);
    fb.fillRect(x, y + 4, 4, 1, c);
}

void triFill(Framebuffer& fb, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
    int minX = std::min({x0, x1, x2}), maxX = std::max({x0, x1, x2});
    int minY = std::min({y0, y1, y2}), maxY = std::max({y0, y1, y2});
    auto edge = [](int ax, int ay, int bx, int by, int px, int py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    };
    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;
    if (area < 0) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x) {
            if (edge(x0, y0, x1, y1, x, y) >= 0 && edge(x1, y1, x2, y2, x, y) >= 0 &&
                edge(x2, y2, x0, y0, x, y) >= 0)
                fb.pixel(x, y, c);
        }
}

}  // namespace

void Game::render(Framebuffer& fb) {
    fb.clear(pal::black);
    switch (state_) {
        case GameState::Title: renderTitle(fb); break;
        case GameState::Explore: renderExplore(fb); break;
        case GameState::Dialogue:
            renderExplore(fb);
            renderDialogue(fb);
            break;
        case GameState::Battle: battle_.render(fb, player_, companions_, globalTime_); break;
        case GameState::Menu:
            renderExplore(fb);
            renderMenu(fb);
            break;
        case GameState::Shop:
            renderExplore(fb);
            renderShop(fb);
            break;
        case GameState::GameOver: renderGameOver(fb); break;
        case GameState::Ending: renderEnding(fb); break;
    }
    if (flash_ > 0.0f) {
        int a = clampInt(static_cast<int>(flash_ * 300.0f), 0, 90);
        fb.fillRectA(0, 0, VIEW_W, VIEW_H, pal::white, a);
    }
    if (transition_) renderTransition(fb);
}

void Game::renderTitle(Framebuffer& fb) {
    for (int y = 0; y < VIEW_H; ++y) {
        float k = static_cast<float>(y) / VIEW_H;
        fb.fillRect(0, y, VIEW_W, 1, mix(rgba(12, 12, 18), rgba(58, 52, 48), k));
    }
    for (int i = 0; i < 90; ++i) {
        int sx = static_cast<int>(hash2(i, 11) % VIEW_W);
        int sy = static_cast<int>(hash2(i, 29) % 150);
        float tw = 0.5f + 0.5f * std::sin(globalTime_ * 2.2f + i * 1.7f);
        int a = 60 + static_cast<int>(tw * 140.0f);
        fb.fillRect(sx, sy, 1, 1, withAlpha(pal::white, a));
    }
    // A pale sun behind ash clouds, kept clear of the title.
    fb.fillCircle(302, 16, 9, 0xFFE8DCC0);
    fb.fillCircle(299, 13, 2, 0xFFC8BCA0);
    fb.fillCircle(305, 19, 2, 0xFFC8BCA0);
    // Salt hills and the Sky Ark tower.
    fb.fillEllipse(40, 214, 120, 60, rgba(28, 28, 32));
    fb.fillEllipse(210, 220, 150, 66, rgba(22, 22, 28));
    fb.fillEllipse(320, 224, 130, 70, rgba(32, 30, 34));
    fb.fillRect(148, 118, 24, 72, rgba(14, 12, 18));
    triFill(fb, 144, 118, 160, 94, 176, 118, rgba(14, 12, 18));
    fb.fillRect(156, 132, 4, 8, rgba(240, 190, 110, 90));
    fb.fillRect(152, 150, 4, 8, rgba(240, 190, 110, 60));

    fb.textShadow((VIEW_W - fb.textWidth("PELITA DI ATAS", 3)) / 2, 36, "PELITA DI ATAS",
                  pal::uiBorder, rgba(20, 14, 10), 3);
    centered(fb, 72, "SEGARA GARAM", 0xFFE8D8B0, 2);
    centered(fb, 94, "Hikayat Laut yang Membeku", 0xFFB8A88A, 1);

    if (titlePage_ == TitlePage::Load) {
        panel(fb, 30, 106, 260, 108);
        centered(fb, 112, "MUAT PERMAINAN", pal::uiBorder, 1);
        const int slots[4] = {-1, 0, 1, 2};
        for (int i = 0; i < 5; ++i) {
            int y = 126 + i * 16;
            bool sel = i == titleSlotIndex_;
            if (sel) fb.fillRect(36, y - 4, 248, 15, pal::uiPanelLight);
            if (sel) cursorArrow(fb, 40, y + 1, pal::uiBorder);
            std::string label = i < 4 ? slotLabel(slots[i]) : "Kembali";
            fb.text(52, y, label, sel ? pal::white : pal::uiTextDim);
            if (i < 4) {
                std::string summary = slotSummary(slots[i]);
                fb.text(108, y, summary,
                        summary == "Kosong" ? pal::uiTextDim : (sel ? pal::gold : pal::uiText));
            }
        }
    } else if (titlePage_ == TitlePage::ConfirmNew) {
        panel(fb, 48, 110, 224, 100);
        centered(fb, 118, "MULAI PERMAINAN BARU?", pal::uiBorder, 1);
        auto lines = wrapText("Permainan baru mulai dari Slot 1. Simpanan lain tetap utuh.", 200, 1);
        int ly = 134;
        for (const auto& line : lines) {
            centered(fb, ly, line, pal::uiTextDim, 1);
            ly += 11;
        }
        const char* options[2] = {"Ya, mulai baru", "Batal"};
        for (int i = 0; i < 2; ++i) {
            int y = 158 + i * 18;
            bool sel = i == titleConfirmIndex_;
            if (sel) fb.fillRect(58, y - 4, 204, 16, pal::uiPanelLight);
            if (sel) cursorArrow(fb, 62, y + 1, pal::uiBorder);
            fb.text(78, y, options[i], sel ? pal::white : pal::uiTextDim);
        }
    } else {
        int boxY = 128;
        panel(fb, 92, boxY, 136, 92);
        const char* labels[4] = {"Mulai Baru", "Lanjutkan", "Perbarui Game", "Keluar"};
        if (updater_ && updater_->running()) labels[2] = "Memperbarui...";
        else if (updater_ && updater_->finished() && updater_->success()) labels[2] = "Mulai Ulang";
        for (int i = 0; i < 4; ++i) {
            int y = boxY + 10 + i * 20;
            bool sel = i == titleIndex_;
            if (sel) fb.fillRect(98, y - 4, 124, 16, pal::uiPanelLight);
            fb.text(116, y, labels[i], sel ? pal::white : pal::uiTextDim);
            if (sel) cursorArrow(fb, 102, y + 1, pal::uiBorder);
            if (i == 1 && !anySaveExists()) fb.text(214, y, "-", pal::uiTextDim);
        }
    }

    std::string message;
    if (updater_ && updater_->running()) message = updater_->statusText();
    else if (updater_ && updater_->finished() && updater_->success())
        message = "Pembaruan selesai. Tekan Z untuk mulai ulang.";
    else if (titleMsgTimer_ > 0.0f && !titleMsg_.empty()) message = titleMsg_;
    if (!message.empty()) {
        int w = std::min(300, fb.textWidth(message) + 18);
        panel(fb, (VIEW_W - w) / 2, 218, w, 22);
        fb.text((VIEW_W - fb.textWidth(message)) / 2, 225, message, pal::hp);
    } else if (titlePage_ == TitlePage::Main) {
        centered(fb, 228, "Panah: gerak   Z: pilih   X: menu", rgba(200, 190, 170, 180), 1);
    } else {
        centered(fb, 228, "Z: pilih   X: kembali", rgba(200, 190, 170, 180), 1);
    }
}

void Game::renderExplore(Framebuffer& fb) {
    int camX = static_cast<int>(std::round(camera_.x));
    int camY = static_cast<int>(std::round(camera_.y));
    const MapDef& md = world_.def(map_);

    int tx0 = std::max(0, camX / TILE - 1);
    int ty0 = std::max(0, camY / TILE - 1);
    int tx1 = std::min(md.w - 1, (camX + VIEW_W) / TILE + 1);
    int ty1 = std::min(md.h - 1, (camY + VIEW_H) / TILE + 1);
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int tx = tx0; tx <= tx1; ++tx)
            art::tile(fb, md.at(tx, ty), tx * TILE - camX, ty * TILE - camY, tx, ty, globalTime_);

    struct Drawable { float y; int kind; int index; };
    std::vector<Drawable> items;
    for (size_t i = 0; i < md.chests.size(); ++i)
        items.push_back({static_cast<float>(md.chests[i].y * TILE + 16), 0, static_cast<int>(i)});
    for (size_t i = 0; i < md.npcs.size(); ++i) {
        const auto& n = md.npcs[i];
        if (npcHidden(n)) continue;
        items.push_back({static_cast<float>(n.y * TILE + 16), 1, static_cast<int>(i)});
    }
    for (size_t i = 0; i < md.echoes.size(); ++i)
        items.push_back({static_cast<float>(md.echoes[i].y * TILE + 16), 4, static_cast<int>(i)});
    for (size_t i = 0; i < md.camps.size(); ++i)
        items.push_back({static_cast<float>(md.camps[i].y * TILE + 16), 5, static_cast<int>(i)});
    if (map_ >= 0 && map_ < static_cast<int>(enemies_.size())) {
        const auto& roamers = enemies_[static_cast<size_t>(map_)];
        for (size_t i = 0; i < roamers.size(); ++i)
            if (roamers[i].active)
                items.push_back({roamers[i].y + 16.0f, 3, static_cast<int>(i)});
    }
    for (size_t i = 0; i < followers_.size() && i < companions_.size(); ++i) {
        if (!companions_[i].recruited) continue;
        items.push_back({followers_[i].y + 16.0f, 6, static_cast<int>(i)});
    }
    items.push_back({py_ + 16.0f, 2, 0});
    std::stable_sort(items.begin(), items.end(),
                     [](const Drawable& a, const Drawable& b) { return a.y < b.y; });

    for (const auto& d : items) {
        if (d.kind == 0) {
            const auto& chest = md.chests[static_cast<size_t>(d.index)];
            bool opened = chestOpened_[static_cast<size_t>(map_)][static_cast<size_t>(d.index)];
            art::chest(fb, chest.x * TILE - camX, chest.y * TILE - camY, opened, globalTime_);
        } else if (d.kind == 1) {
            const auto& npc = md.npcs[static_cast<size_t>(d.index)];
            art::npc(fb, npc.x * TILE - camX, npc.y * TILE - camY, npc.style, npc.facing, globalTime_);
        } else if (d.kind == 3) {
            const auto& e = enemies_[static_cast<size_t>(map_)][static_cast<size_t>(d.index)];
            art::enemyWorld(fb, e.type, static_cast<int>(std::floor(e.x)) - camX,
                            static_cast<int>(std::floor(e.y)) - camY, globalTime_ + e.anim);
        } else if (d.kind == 4) {
            const auto& e = md.echoes[static_cast<size_t>(d.index)];
            art::echoSpot(fb, e.x * TILE - camX, e.y * TILE - camY, globalTime_, !hasMemory(e.id));
        } else if (d.kind == 5) {
            const auto& c = md.camps[static_cast<size_t>(d.index)];
            art::campfire(fb, c.x * TILE - camX, c.y * TILE - camY, globalTime_);
        } else if (d.kind == 6) {
            const auto& c = companions_[static_cast<size_t>(d.index)];
            art::companion(fb, c.id, static_cast<int>(followers_[d.index].x) - camX,
                           static_cast<int>(followers_[d.index].y) - camY, globalTime_, moving_);
        } else {
            art::player(fb, static_cast<int>(px_) - camX, static_cast<int>(py_) - camY, facing_,
                        animPhase_, moving_, player_.weapon, player_.armor);
        }
    }

    for (const auto& p : particles_) {
        int a = clampInt(static_cast<int>(255.0f * (p.life / std::max(0.01f, p.maxLife))), 0, 255);
        fb.fillRect(static_cast<int>(p.pos.x) - camX, static_cast<int>(p.pos.y) - camY, p.size, p.size,
                    withAlpha(p.color, a));
    }
    for (const auto& f : floats_) {
        int a = clampInt(static_cast<int>(255.0f * f.life), 0, 255);
        fb.text(static_cast<int>(f.pos.x) - camX - 10, static_cast<int>(f.pos.y) - camY, f.text,
                withAlpha(f.color, a));
    }

    // Interaction hint.
    {
        int dx = facing_ == 2 ? -1 : (facing_ == 3 ? 1 : 0);
        int dy = facing_ == 0 ? 1 : (facing_ == 1 ? -1 : 0);
        int tx = tileOfX(px_ + 8.0f) + dx;
        int ty = tileOfY(py_ + 12.0f) + dy;
        bool show = false;
        const NpcSpawn* npc = world_.npcAt(map_, tx, ty);
        if (npc && !npcHidden(*npc)) show = true;
        if (world_.chestAt(map_, tx, ty) || world_.echoAt(map_, tx, ty) ||
            world_.campAt(map_, tx, ty))
            show = true;
        Tile t = world_.tile(map_, tx, ty);
        if (t == Tile::Sign || t == Tile::Door || t == Tile::CaveEntrance ||
            t == Tile::RuinEntrance)
            show = true;
        if (show) {
            int ax = tx * TILE - camX + 8;
            int ay = ty * TILE - camY - 4 + static_cast<int>(std::sin(globalTime_ * 5.0f) * 2.0f);
            fb.fillRect(ax - 1, ay, 3, 5, pal::uiBorder);
            fb.fillRect(ax - 2, ay + 4, 5, 2, pal::uiBorder);
        }
    }

    // Cave and deep-ruin lighting.
    if (map_ == 3 || map_ == 4 || map_ == 8) {
        float lx = px_ + 8.0f - camX;
        float ly = py_ + 8.0f - camY;
        for (int y = 0; y < VIEW_H; ++y) {
            for (int x = 0; x < VIEW_W; ++x) {
                float dx = static_cast<float>(x) - lx;
                float dy = static_cast<float>(y) - ly;
                float dist = std::sqrt(dx * dx + dy * dy);
                int a = clampInt(static_cast<int>((dist - 52.0f) * 2.4f), 0, 190);
                if (a > 0) fb.blendPixel(x, y, pal::black, a);
            }
        }
    }

    // HUD: Raka, resonance, oil, memories.
    panel(fb, 6, 6, 118, 48);
    fb.text(12, 10, "RAKA", pal::uiText);
    std::string lv = "Lv " + std::to_string(player_.level);
    fb.text(112 - static_cast<int>(lv.size()) * 6, 10, lv, pal::uiTextDim);
    bar(fb, 12, 20, 96, 5, static_cast<float>(player_.hp) / player_.maxHp, pal::hp, pal::hpDark);
    bar(fb, 12, 28, 96, 5, static_cast<float>(player_.mp) / player_.maxMp, pal::mp, pal::mpDark);
    bar(fb, 12, 36, 96, 5, static_cast<float>(flags_.resonance) / 100.0f, 0xFF9AD8E8,
        0xFF3A5A68);
    fb.text(12, 44, "R " + std::to_string(flags_.resonance), pal::uiTextDim);

    panel(fb, 236, 6, 78, 30);
    fb.text(244, 10, "GARAM", pal::uiTextDim);
    std::string g = std::to_string(player_.gold);
    fb.text(304 - static_cast<int>(g.size()) * 6, 10, g, pal::gold);
    fb.text(244, 22, "MINYAK", pal::uiTextDim);
    fb.text(304 - static_cast<int>(std::to_string(player_.oil).size()) * 6, 22,
            std::to_string(player_.oil), 0xFFFFB06A);

    if (mapBanner_ > 0.0f) {
        int w = fb.textWidth(mapBannerText_) + 20;
        int bx = (VIEW_W - w) / 2;
        fb.fillRectA(bx, 30, w, 18, pal::uiPanel, 210);
        fb.outline(bx, 30, w, 18, pal::uiBorder);
        fb.text(bx + 10, 37, mapBannerText_, pal::uiText);
    }

    renderMinimap(fb, 244, 166, 70, 64);
}

void Game::renderDialogue(Framebuffer& fb) {
    dim(fb, 40);
    if (dlg_.pages.empty()) return;
    int tw = fb.textWidth(dlg_.speaker) + 16;
    fb.fillRectA(12, 146, tw, 14, pal::uiPanel, 246);
    fb.outline(12, 146, tw, 14, pal::uiBorder);
    fb.text(20, 150, dlg_.speaker, pal::uiBorder);

    panel(fb, 6, 158, 308, 76);
    if (dlg_.choosing) {
        for (size_t i = 0; i < dlg_.choices.size(); ++i) {
            int y = 168 + static_cast<int>(i) * 18;
            bool sel = static_cast<int>(i) == dlg_.choiceIndex;
            if (sel) fb.fillRect(12, y - 3, 296, 16, pal::uiPanelLight);
            fb.text(20, y, dlg_.choices[i].text, sel ? pal::white : pal::uiTextDim);
            if (sel) cursorArrow(fb, 12, y + 2, pal::uiBorder);
        }
        fb.text(20, 224, "Z: pilih", pal::uiTextDim);
        return;
    }
    const std::string& page = dlg_.pages[static_cast<size_t>(dlg_.page)];
    int shown = std::min<int>(static_cast<int>(dlg_.chars), static_cast<int>(page.size()));
    auto lines = wrapText(page.substr(0, static_cast<size_t>(shown)), 288, 1);
    int y = 170;
    for (size_t i = 0; i < lines.size() && i < 4; ++i) {
        fb.text(16, y, lines[i], pal::uiText);
        y += 12;
    }
    if (shown >= static_cast<int>(page.size()) && static_cast<int>(globalTime_ * 3.0f) % 2 == 0) {
        fb.fillRect(296, 220, 4, 4, pal::uiBorder);
        fb.fillRect(294, 222, 8, 2, pal::uiBorder);
    }
}

void Game::renderMenu(Framebuffer& fb) {
    dim(fb, 170);
    panel(fb, 18, 14, 284, 212);
    fb.text(28, 22, "MENU", pal::uiBorder, 2);
    std::string g = "Garam " + std::to_string(player_.gold);
    fb.text(292 - static_cast<int>(g.size()) * 6, 28, g, pal::gold);

    if (menuPage_ == 0) {
        const char* labels[6] = {"Item", "Status", "Jurnal", "Simpan", "Audio", "Tutup"};
        const char* desc[6] = {
            "Gunakan ramuan, eliksir, dan minyak.",
            "Lihat status Raka dan party.",
            "Baca misi, memori, dan peta.",
            "Simpan ke salah satu slot.",
            "Atur musik dan efek suara.",
            "Kembali menjelajah.",
        };
        for (int i = 0; i < 6; ++i) {
            int y = 48 + i * 19;
            bool sel = i == menuIndex_;
            if (sel) fb.fillRect(30, y - 4, 110, 17, pal::uiPanelLight);
            fb.text(48, y, labels[i], sel ? pal::white : pal::uiTextDim);
            if (sel) cursorArrow(fb, 34, y + 1, pal::uiBorder);
        }
        fb.fillRect(150, 48, 1, 112, 0xFF39405C);
        int sel = menuIndex_;
        auto lines = wrapText(desc[sel], 130, 1);
        int y = 54;
        for (const auto& line : lines) {
            fb.text(160, y, line, pal::uiText);
            y += 12;
        }
        fb.text(160, 88, "Misi Utama:", pal::uiTextDim);
        auto ql = wrapText(questObjective(), 128, 1);
        int qy = 99;
        for (const auto& line : ql) {
            fb.text(160, qy, line, pal::uiText);
            qy += 11;
        }
        fb.text(160, 136, "Sampingan:", pal::uiTextDim);
        auto sl = wrapText(sideQuestObjective(), 128, 1);
        int sy = 147;
        for (const auto& line : sl) {
            fb.text(160, sy, line, pal::uiText);
            sy += 11;
        }
        fb.text(28, 180, "Z: pilih   X: kembali", pal::uiTextDim);
    } else if (menuPage_ == 1) {
        fb.text(28, 50, "ITEM", pal::uiTextDim);
        struct Row { std::string name; std::string detail; int count; };
        Row rows[4] = {{"Ramuan Pelita", "Memulihkan 45 HP", player_.potions},
                       {"Eliksir Sumsum", "Memulihkan 30 MP", player_.elixirs},
                       {"Minyak Sumsum", "Bahan bakar api unggun", player_.oil},
                       {"Garam Kenangan", "Material pengrajin", player_.saltCrystal}};
        for (int i = 0; i < 4; ++i) {
            int y = 64 + i * 26;
            bool sel = i == menuIndex_;
            if (sel) fb.fillRect(26, y - 4, 268, 22, pal::uiPanelLight);
            fb.text(38, y, rows[i].name, sel ? pal::white : pal::uiTextDim);
            fb.text(38, y + 9, rows[i].detail, pal::uiTextDim);
            std::string c = "x" + std::to_string(rows[i].count);
            fb.text(280 - static_cast<int>(c.size()) * 6, y, c, sel ? pal::gold : pal::uiTextDim);
            if (sel) cursorArrow(fb, 30, y + 2, pal::uiBorder);
        }
        if (menuMessageTimer_ > 0.0f) fb.text(28, 176, menuMessage_, pal::hpLight);
        fb.text(28, 196, "Z: pakai   X: kembali", pal::uiTextDim);
    } else if (menuPage_ == 2) {
        fb.text(28, 50, "STATUS", pal::uiTextDim);
        fb.text(38, 64, "RAKA", pal::uiText);
        std::string lv = "Lv " + std::to_string(player_.level);
        fb.text(160, 64, lv, pal::uiTextDim);
        fb.text(38, 76, "HP", pal::uiTextDim);
        bar(fb, 60, 75, 90, 6, static_cast<float>(player_.hp) / player_.maxHp, pal::hp, pal::hpDark);
        fb.text(156, 76, std::to_string(player_.hp) + "/" + std::to_string(player_.maxHp), pal::uiText);
        fb.text(38, 88, "MP", pal::uiTextDim);
        bar(fb, 60, 87, 90, 6, static_cast<float>(player_.mp) / player_.maxMp, pal::mp, pal::mpDark);
        fb.text(156, 88, std::to_string(player_.mp) + "/" + std::to_string(player_.maxMp), pal::uiText);
        fb.text(38, 100, "Resonansi", pal::uiTextDim);
        bar(fb, 108, 99, 90, 6, static_cast<float>(flags_.resonance) / 100.0f, 0xFF9AD8E8,
            0xFF3A5A68);
        fb.text(202, 100, std::to_string(flags_.resonance), 0xFF9AD8E8);
        fb.text(38, 112, "Serangan", pal::uiTextDim);
        fb.text(100, 112, std::to_string(player_.atk()), pal::uiText);
        fb.text(152, 112, "Pertahanan", pal::uiTextDim);
        fb.text(226, 112, std::to_string(player_.def()), pal::uiText);
        fb.text(38, 124, "Senjata", pal::uiTextDim);
        fb.text(100, 124, WEAPONS[player_.weapon].name, pal::uiText);
        fb.text(38, 136, "Armor", pal::uiTextDim);
        fb.text(100, 136, ARMORS[player_.armor].name, pal::uiText);
        fb.text(38, 148, "Memori", pal::uiTextDim);
        fb.text(100, 148, std::to_string(memoryCount()) + "/12", 0xFF9AD8E8);
        fb.text(38, 160, "Minyak", pal::uiTextDim);
        fb.text(100, 160, std::to_string(player_.oil), 0xFFFFB06A);
        fb.text(160, 160, "Garam", pal::uiTextDim);
        fb.text(212, 160, std::to_string(player_.gold), pal::gold);

        fb.text(38, 170, "PARTY:", pal::uiTextDim);
        int py = 182;
        for (const auto& c : companions_) {
            if (!c.recruited) continue;
            fb.text(38, py, c.name, pal::uiText);
            fb.text(108, py, "Lv" + std::to_string(c.level), pal::uiTextDim);
            fb.text(150, py, std::to_string(c.hp) + "/" + std::to_string(c.maxHp),
                    c.alive ? pal::hpLight : pal::uiTextDim);
            py += 9;
        }
        fb.text(28, 220, "X: kembali", pal::uiTextDim);
    } else if (menuPage_ == 3) {
        renderJournal(fb);
    } else if (menuPage_ == 4) {
        renderSaveSlots(fb);
    } else {
        fb.text(28, 50, "AUDIO", pal::uiTextDim);
        if (!audio_) {
            fb.text(38, 74, "Audio tidak tersedia.", pal::uiText);
            fb.text(38, 90, "Perangkat suara tidak aktif atau game", pal::uiTextDim);
            fb.text(38, 102, "dijalankan dengan --no-audio.", pal::uiTextDim);
            fb.text(28, 176, "X: kembali", pal::uiTextDim);
        } else {
            const char* labels[4] = {"Musik", "Efek Suara", "Bisukan", "Kembali"};
            for (int i = 0; i < 4; ++i) {
                int y = 66 + i * 24;
                bool sel = i == menuIndex_;
                if (sel) fb.fillRect(30, y - 5, 252, 20, pal::uiPanelLight);
                fb.text(38, y, labels[i], sel ? pal::white : pal::uiTextDim);
                if (sel) cursorArrow(fb, 32, y + 1, pal::uiBorder);
            }
            float mv = audio_->musicVolume();
            float sv = audio_->sfxVolume();
            bar(fb, 128, 65, 108, 7, mv, pal::mp, pal::mpDark);
            bar(fb, 128, 89, 108, 7, sv, pal::hp, pal::hpDark);
            std::string mPct = std::to_string(static_cast<int>(std::lround(mv * 100.0f))) + "%";
            std::string sPct = std::to_string(static_cast<int>(std::lround(sv * 100.0f))) + "%";
            fb.text(244, 64, mPct, menuIndex_ == 0 ? pal::gold : pal::uiTextDim);
            fb.text(244, 88, sPct, menuIndex_ == 1 ? pal::gold : pal::uiTextDim);
            fb.text(128, 110, audio_->muted() ? "Ya" : "Tidak",
                    menuIndex_ == 2 ? pal::gold : pal::uiText);
            if (menuIndex_ == 3) fb.text(128, 134, "Kembali ke menu", pal::uiTextDim);
            fb.text(28, 176, "Kiri/kanan: ubah   Z: pilih   X: kembali", pal::uiTextDim);
        }
    }

    if (menuMessageTimer_ > 0.0f && menuPage_ == 4) {
        auto lines = wrapText(menuMessage_, 260, 1);
        int y = 205;
        for (size_t i = 0; i < lines.size() && i < 2; ++i) {
            fb.text(28, y, lines[i], pal::hpLight);
            y += 10;
        }
    }
}

void Game::renderJournal(Framebuffer& fb) {
    fb.text(28, 46, "JURNAL", pal::uiTextDim);
    fb.text(28, 58, "Misi Utama", pal::uiBorder);
    auto ql = wrapText(questObjective(), 140, 1);
    int qy = 69;
    for (size_t i = 0; i < ql.size() && i < 3; ++i) {
        fb.text(28, qy, ql[i], pal::uiText);
        qy += 11;
    }
    fb.text(28, 108, "Misi Sampingan", pal::uiBorder);
    auto sl = wrapText(sideQuestObjective(), 140, 1);
    int sy = 119;
    for (size_t i = 0; i < sl.size() && i < 2; ++i) {
        fb.text(28, sy, sl[i], pal::uiText);
        sy += 11;
    }
    fb.text(28, 148, "Memori: " + std::to_string(memoryCount()) + "/12", pal::uiBorder);
    static const char* names[12] = {
        "Kurir yang Membatu", "Segel Adipati", "Manifes Minyak", "Lentera Nenek Sari",
        "Gema Karang", "Gema Ayah", "Pipa Penyedot", "Prasasti Meteorit",
        "Kunci Tirta", "Jurnal Pendiri", "Gema Terlarang", "Sang Gema Pertama"};
    for (int i = 0; i < 12; ++i) {
        bool got = hasMemory(i);
        int colX = 24 + (i / 6) * 150;
        int colY = 159 + (i % 6) * 9;
        fb.text(colX, colY, std::string(got ? "[x] " : "[ ] ") + names[i],
                got ? pal::uiText : pal::uiTextDim);
    }
    fb.text(28, 214, "X: kembali", pal::uiTextDim);
}

void Game::renderSaveSlots(Framebuffer& fb) {
    fb.text(28, 50, "SIMPAN", pal::uiTextDim);
    auto tip = wrapText("Autosave ditulis otomatis. Pilih Slot 1-3 untuk menyimpan manual.", 260, 1);
    int ty = 62;
    for (const auto& line : tip) {
        fb.text(28, ty, line, pal::uiTextDim);
        ty += 10;
    }
    for (int i = 0; i < 5; ++i) {
        int y = 88 + i * 20;
        bool sel = i == menuIndex_;
        if (sel) fb.fillRect(26, y - 4, 268, 18, pal::uiPanelLight);
        if (sel) cursorArrow(fb, 30, y + 1, pal::uiBorder);
        if (i == 4) {
            fb.text(48, y, "Kembali", sel ? pal::white : pal::uiTextDim);
            continue;
        }
        int slot = i == 0 ? -1 : i - 1;
        std::string label = slotLabel(slot);
        if (saveSlot_ == slot) label += " *";
        fb.text(48, y, label, sel ? pal::white : pal::uiTextDim);
        fb.text(120, y, slotSummary(slot), sel ? pal::gold : pal::uiText);
    }
    fb.text(28, 196, "Z: pilih   X: kembali", pal::uiTextDim);
}

void Game::renderMinimap(Framebuffer& fb, int x, int y, int w, int h) {
    panel(fb, x, y, w, h);
    const MapDef& md = world_.def(map_);
    if (md.w <= 0 || md.h <= 0) return;
    int innerW = w - 8;
    int innerH = h - 18;
    if (innerW < 1 || innerH < 1) return;
    int scale = std::min(innerW / md.w, innerH / md.h);
    if (scale < 1) scale = 1;
    int mw = md.w * scale, mh = md.h * scale;
    int ox = x + (w - mw) / 2;
    int oy = y + 13 + (innerH - mh) / 2;
    fb.fillRect(ox - 1, oy - 1, mw + 2, mh + 2, 0xFF05060C);
    for (int ty = 0; ty < md.h; ++ty)
        for (int tx = 0; tx < md.w; ++tx) {
            uint32_t c = 0xFF8A8E7C;
            switch (md.at(tx, ty)) {
                case Tile::Flower: c = 0xFF9AA08C; break;
                case Tile::TallGrass: c = 0xFF6E7460; break;
                case Tile::Path: c = 0xFFB0A88E; break;
                case Tile::Sand: c = 0xFFC8C0A4; break;
                case Tile::Water: c = 0xFF3A6A72; break;
                case Tile::DeepWater: c = 0xFF244850; break;
                case Tile::Tree: c = 0xFF6E6A5C; break;
                case Tile::Rock: c = 0xFF9A9EA8; break;
                case Tile::Wall: c = 0xFFD8D4C8; break;
                case Tile::Roof: c = 0xFF4A4A58; break;
                case Tile::Door: c = 0xFFB08A58; break;
                case Tile::Fence: c = 0xFF8A7A5A; break;
                case Tile::Bridge: c = 0xFF8A6A42; break;
                case Tile::CaveFloor: c = 0xFF565064; break;
                case Tile::CaveWall: c = 0xFF221F2E; break;
                case Tile::Lava: c = 0xFFB04A2A; break;
                case Tile::CaveEntrance: c = 0xFFD8C87A; break;
                case Tile::RuinEntrance: c = 0xFFE8D888; break;
                case Tile::Sign: c = 0xFF9A8A6A; break;
                case Tile::Pillar: c = 0xFF8A8498; break;
                case Tile::Void: c = 0xFF0A0C14; break;
                case Tile::Grass: break;
            }
            if (scale == 1) fb.pixel(ox + tx, oy + ty, c);
            else fb.fillRect(ox + tx * scale, oy + ty * scale, scale, scale, c);
        }
    for (const Portal& p : md.portals)
        for (int yy = 0; yy < p.h; ++yy)
            for (int xx = 0; xx < p.w; ++xx)
                fb.fillRect(ox + (p.x + xx) * scale, oy + (p.y + yy) * scale,
                            std::max(1, scale), std::max(1, scale), 0xFF66E0FF);
    for (size_t i = 0; i < md.echoes.size(); ++i) {
        const EchoSpot& e = md.echoes[i];
        fb.fillRect(ox + e.x * scale, oy + e.y * scale, std::max(1, scale),
                    std::max(1, scale), hasMemory(e.id) ? 0xFF9AD8E8 : 0xFF5A7A88);
    }
    for (size_t i = 0; i < md.chests.size(); ++i) {
        if (chestOpened_[static_cast<size_t>(map_)][i]) continue;
        const ChestSpawn& c = md.chests[i];
        fb.fillRect(ox + c.x * scale, oy + c.y * scale, std::max(1, scale),
                    std::max(1, scale), pal::gold);
    }
    for (const NpcSpawn& n : md.npcs) {
        if (npcHidden(n)) continue;
        fb.fillRect(ox + n.x * scale, oy + n.y * scale, std::max(1, scale),
                    std::max(1, scale), 0xFFFFA8C8);
    }
    if (map_ >= 0 && map_ < static_cast<int>(enemies_.size())) {
        for (const RoamingEnemy& e : enemies_[static_cast<size_t>(map_)]) {
            if (!e.active) continue;
            fb.fillRect(ox + tileOfX(e.x + 8.0f) * scale, oy + tileOfY(e.y + 12.0f) * scale,
                        std::max(1, scale), std::max(1, scale), pal::hp);
        }
    }
    if ((static_cast<int>(globalTime_ * 4.0f) & 1) == 0) {
        fb.fillRect(ox + tileOfX(px_ + 8.0f) * scale, oy + tileOfY(py_ + 8.0f) * scale,
                    std::max(2, scale), std::max(2, scale), pal::white);
    }
    std::string label = fb.textWidth(md.name) <= w - 8 ? md.name : "PETA";
    fb.text(x + (w - fb.textWidth(label)) / 2, y + 3, label, pal::uiBorder);
}

void Game::renderShop(Framebuffer& fb) {
    dim(fb, 170);
    panel(fb, 12, 20, 296, 196);
    fb.text(24, 28, "TOKO JOKO KELING", pal::uiBorder, 2);
    std::string g = "Garam " + std::to_string(player_.gold);
    fb.text(296 - static_cast<int>(g.size()) * 6, 34, g, pal::gold);
    fb.text(24, 50, "Barang", pal::uiTextDim);
    fb.text(226, 50, "Harga", pal::uiTextDim);
    for (int i = 0; i < SHOP_COUNT; ++i) {
        const ShopItem& item = SHOP[i];
        int y = 62 + i * 16;
        bool sel = i == shopIndex_;
        if (sel) fb.fillRect(20, y - 3, 272, 15, pal::uiPanelLight);
        if (sel) cursorArrow(fb, 24, y + 1, pal::uiBorder);
        uint32_t col = sel ? pal::white : pal::uiTextDim;
        std::string name = item.name;
        bool owned = (item.kind == 3 && player_.weapon >= item.index) ||
                     (item.kind == 4 && player_.armor >= item.index);
        if (owned) name += " (dipakai)";
        fb.text(40, y, name, owned ? 0xFF8AA88A : col);
        std::string price = std::to_string(item.price) + "G";
        fb.text(286 - static_cast<int>(price.size()) * 6, y, price, owned ? 0xFF8AA88A : pal::gold);
    }
    fb.fillRect(20, 168, 268, 1, 0xFF39405C);
    if (shopMessageTimer_ > 0.0f && !shopMessage_.empty()) {
        fb.text(24, 174, shopMessage_, pal::hpLight);
    } else {
        fb.text(24, 174, "Ramuan HP, Eliksir MP, Minyak untuk api unggun.", pal::uiTextDim);
    }
    fb.text(24, 194, "Z: beli   X: keluar", pal::uiTextDim);
}

void Game::renderGameOver(Framebuffer& fb) {
    for (int y = 0; y < VIEW_H; ++y) {
        float k = static_cast<float>(y) / VIEW_H;
        fb.fillRect(0, y, VIEW_W, 1, mix(rgba(20, 8, 8), rgba(56, 20, 18), k));
    }
    centered(fb, 44, "KAU TUMBANG", pal::hp, 3);
    centered(fb, 78, "Abu menutup pandanganmu...", pal::uiTextDim, 1);
    panel(fb, 52, 104, 216, 92);
    const char* options[2] = {"Bangkit di Tambak-Abu (-20% garam)", "Kembali ke judul"};
    for (int i = 0; i < 2; ++i) {
        int y = 118 + i * 32;
        bool sel = i == gameOverIndex_;
        if (sel) fb.fillRect(58, y - 4, 204, 24, pal::uiPanelLight);
        if (sel) cursorArrow(fb, 62, y + 2, pal::uiBorder);
        fb.text(78, y, options[i], sel ? pal::white : pal::uiTextDim);
    }
    centered(fb, 210, "Z: pilih", pal::uiTextDim, 1);
    centered(fb, 224, "Garammu saat ini: " + std::to_string(player_.gold), pal::gold, 1);
}

void Game::renderEnding(Framebuffer& fb) {
    for (int y = 0; y < VIEW_H; ++y) {
        float k = static_cast<float>(y) / VIEW_H;
        fb.fillRect(0, y, VIEW_W, 1, mix(rgba(8, 8, 18), rgba(40, 34, 30), k));
    }
    for (int i = 0; i < 70; ++i) {
        int sx = static_cast<int>(hash2(i, 71) % VIEW_W);
        int sy = static_cast<int>(hash2(i, 97) % VIEW_H);
        float tw = 0.5f + 0.5f * std::sin(globalTime_ * 1.6f + i);
        fb.fillRect(sx, sy, 1, 1, withAlpha(pal::white, 60 + static_cast<int>(tw * 120)));
    }
    std::vector<std::string> lines;
    switch (flags_.endingId) {
        case 1:
            lines = {"Hujan pertama di atas Nusakalpa.", "Memori yang dicuri dikembalikan ke bumi.",
                     "Raka dan para rekan membagikan beban Jangkar Gema.", "",
                     "RAKA DAMAR", "Era Tirta dimulai", "", "TAMAT"};
            break;
        case 2:
            lines = {"Adipati Mahardika menyatu dengan inti palung.", "Retakan tertutup, hujan abu berhenti.",
                     "Kirana memimpin Kertarajasa menjadi republik baru.", "",
                     "PELITA SANG Penebus", "", "TAMAT"};
            break;
        case 3:
            lines = {"Raka menghentikan rekan-rekannya.", "Mesin menyedot seluruh ingatan umat manusia.",
                     "Nusakalpa damai, sunyi, tanpa nama.", "",
                     "SUNYI TANPA NAMA", "TAMAT"};
            break;
        case 4:
            lines = {"Inti Tirta Purba menghancurkan meteorit.", "Samudra biru kembali mengisi Segara Garam.",
                     "Cakrawala Karat berlayar sebagai kapal laut sejati.", "",
                     "CAKRAWALA TIRTA PURBA", "TAMAT"};
            break;
        default:
            lines = {"Pelita di atas segara menyala kembali.", "Nusakalpa memasuki era baru.", "",
                     "TAMAT"};
            break;
    }
    int startY = 250 - static_cast<int>(endingScroll_);
    for (size_t i = 0; i < lines.size(); ++i) {
        int y = startY + static_cast<int>(i) * 16;
        if (y < -20 || y > VIEW_H + 10) continue;
        if (i == 4 && !lines[i].empty()) {
            fb.textShadow((VIEW_W - fb.textWidth(lines[i], 2)) / 2, y, lines[i], pal::uiBorder,
                          pal::black, 2);
        } else if (i == lines.size() - 1) {
            fb.textShadow((VIEW_W - fb.textWidth(lines[i], 3)) / 2, y, lines[i], pal::white,
                          rgba(40, 24, 20), 3);
        } else {
            centered(fb, y, lines[i], 0xFFE0D8C0, 1);
        }
    }
    if (endingScroll_ > 160.0f) centered(fb, 220, "Z: kembali ke judul", pal::uiTextDim, 1);
}

void Game::renderTransition(Framebuffer& fb) {
    float k = transitionT_ < 0.5f ? transitionT_ * 2.0f : (1.0f - transitionT_) * 2.0f;
    int a = clampInt(static_cast<int>(k * 255.0f), 0, 255);
    fb.fillRectA(0, 0, VIEW_W, VIEW_H, pal::black, a);
}

}  // namespace rpg
