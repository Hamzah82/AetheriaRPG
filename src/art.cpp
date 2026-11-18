// art.cpp - all sprites are drawn procedurally, no external assets needed
#include "art.h"

#include <cmath>

namespace rpg {
namespace art {
namespace {

using namespace pal;

struct Look {
    uint32_t skin, hair, shirt, shirtDark, pants, boots, accent;
    int weapon = -1;
    int armor = -1;
};

void tri(Framebuffer& fb, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
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
        area = -area;
    }
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x) {
            int w0 = edge(x0, y0, x1, y1, x, y);
            int w1 = edge(x1, y1, x2, y2, x, y);
            int w2 = edge(x2, y2, x0, y0, x, y);
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) fb.pixel(x, y, c);
        }
}

void grassBase(Framebuffer& fb, int px, int py, int tx, int ty) {
    fb.fillRect(px, py, 16, 16, grass);
    uint32_t h = hash2(tx, ty);
    if ((tx + ty) & 1) fb.fillRectA(px, py, 16, 16, grassDark, 16);
    for (int i = 0; i < 3; ++i) {
        int nx = px + static_cast<int>((h >> (i * 5)) & 13u) + 1;
        int ny = py + static_cast<int>((h >> (i * 5 + 4)) & 12u) + 2;
        fb.fillRect(nx, ny, 1, 2, (i & 1) ? grassLight : grassDark);
    }
}

// Armour details sit on top of the tunic silhouette. The plate set also
// carries the Dawn Shield, so the shield belongs to the armour slot rather
// than the weapon slot.
void armorDetails(Framebuffer& fb, int x, int y, int s, int dir, const Look& L) {
    switch (L.armor) {
        case 1: {
            // Leather cuirass with shoulder pads and a cross strap.
            const uint32_t leather = 0xFF8A5A32, leatherDark = 0xFF5A3A1E;
            fb.fillRect(x + 2 * s, y + 6 * s, 4 * s, 2 * s, leather);
            fb.fillRect(x + 10 * s, y + 6 * s, 4 * s, 2 * s, leather);
            fb.fillRect(x + 2 * s, y + 8 * s, 4 * s, 1 * s, leatherDark);
            fb.fillRect(x + 10 * s, y + 8 * s, 4 * s, 1 * s, leatherDark);
            fb.fillRect(x + 7 * s, y + 7 * s, 2 * s, 4 * s, leatherDark);
            fb.fillRect(x + 7 * s, y + 10 * s, 2 * s, 1 * s, 0xFFD8B878);
            break;
        }
        case 2: {
            // Riveted chainmail: bright shoulders and alternating rings.
            const uint32_t mail = 0xFF9AA4B4, mailDark = 0xFF596274;
            const uint32_t mailLight = 0xFFD0D8E4;
            fb.fillRect(x + 1 * s, y + 6 * s, 4 * s, 2 * s, mail);
            fb.fillRect(x + 11 * s, y + 6 * s, 4 * s, 2 * s, mail);
            fb.fillRect(x + 1 * s, y + 7 * s, 4 * s, 1 * s, mailDark);
            fb.fillRect(x + 11 * s, y + 7 * s, 4 * s, 1 * s, mailDark);
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 4; ++col) {
                    int xx = 4 + col * 2 + (row & 1);
                    int yy = 7 + row * 2;
                    fb.fillRect(x + xx * s, y + yy * s, s, s,
                                ((col + row) & 1) ? mailLight : mailDark);
                }
            }
            break;
        }
        case 3: {
            // Gilded dawn plate with a sun sigil and the matching shield.
            const uint32_t plate = 0xFFE8C874, edge = 0xFFFFF2C0;
            const uint32_t dark = 0xFF9A6A24;
            fb.fillRect(x + 1 * s, y + 6 * s, 4 * s, 2 * s, plate);
            fb.fillRect(x + 11 * s, y + 6 * s, 4 * s, 2 * s, plate);
            fb.fillRect(x + 1 * s, y + 6 * s, 4 * s, 1 * s, edge);
            fb.fillRect(x + 11 * s, y + 6 * s, 4 * s, 1 * s, edge);
            fb.fillRect(x + 4 * s, y + 7 * s, 8 * s, 1 * s, edge);
            fb.fillRect(x + 4 * s, y + 10 * s, 8 * s, 1 * s, dark);
            fb.fillRect(x + 6 * s, y + 8 * s, 4 * s, 2 * s, 0xFFFFF2C0);
            fb.fillRect(x + 7 * s, y + 8 * s, 2 * s, 1 * s, 0xFFB85A1E);
            fb.fillRect(x + 7 * s, y + 9 * s, 2 * s, 1 * s, 0xFFFFF2C0);

            int side = (dir == 1 || dir == 2) ? 12 : 0;
            int sx = x + side * s;
            int sy = y + 8 * s;
            // Kite shield: wide shoulders, a tapered point, and a sun boss.
            fb.fillRect(sx, sy, 4 * s, 4 * s, dark);
            fb.fillRect(sx + s, sy + s, 2 * s, 3 * s, plate);
            fb.fillRect(sx + s, sy + s, 2 * s, s, edge);
            fb.fillRect(sx + 2 * s, sy + 2 * s, s, s, 0xFFB85A1E);
            fb.fillRect(sx + s, sy + 4 * s, 2 * s, s, dark);
            fb.fillRect(sx + 2 * s, sy + 5 * s, s, s, dark);
            break;
        }
        default:
            break;
    }
}

void heldWeapon(Framebuffer& fb, int x, int y, int s, int dir, const Look& L) {
    if (L.weapon < 0) return;
    uint32_t blade = 0xFF8A5A2E, edge = 0xFFC08A4A;
    uint32_t guard = 0xFFB87A3A, grip = 0xFF5A3A1E;
    switch (clampInt(L.weapon, 0, 3)) {
        case 0:
            break;
        case 1:
            blade = 0xFFB87830;
            edge = 0xFFFFC870;
            guard = 0xFFE8B050;
            grip = 0xFF3A2A10;
            break;
        case 2:
            blade = 0xFFB8C0CC;
            edge = 0xFFF0F4FA;
            guard = 0xFFD0D8E4;
            grip = 0xFF2A2A34;
            break;
        default:
            blade = 0xFFFFD870;
            edge = 0xFFFFF6D0;
            guard = 0xFFFFF2C0;
            grip = 0xFF7A4A1A;
            break;
    }

    // The sword is held in the hand nearest the screen edge so its silhouette
    // stays readable in every facing. Upgrades get a longer blade as well as
    // a different metal, which keeps the equipment change obvious at 1x.
    const bool left = dir == 1 || dir == 2;
    const int handX = left ? 1 : 14;
    const int edgeX = left ? 0 : 15;
    const int bladeLen = 5 + clampInt(L.weapon, 0, 3);
    fb.fillRect(x + handX * s, y + 10 * s, s, 3 * s, grip);
    fb.fillRect(x + (left ? 0 : 12) * s, y + 9 * s, 4 * s, s, guard);
    fb.fillRect(x + handX * s, y + 13 * s, s, s, guard);
    // Keep the hand visible around the grip so the sword reads as held.
    fb.fillRect(x + handX * s, y + 11 * s, s, s, L.skin);

    // A two-pixel blade with a bright edge reads as a sword rather than a
    // walking staff even in the tiny overworld sprite.
    for (int i = 0; i < bladeLen; ++i) {
        fb.fillRect(x + handX * s, y + (8 - i) * s, s, s, blade);
        fb.fillRect(x + edgeX * s, y + (8 - i) * s, s, s, edge);
    }
    fb.fillRect(x + handX * s, y + (8 - bladeLen) * s, s, s, edge);
    fb.fillRect(x + edgeX * s, y + (8 - bladeLen) * s, s, s, edge);
}

void humanoid(Framebuffer& fb, int x, int y, int s, int dir, float phase, bool moving, const Look& L,
              bool shorty = false, bool helmet = false, bool dress = false) {
    int step = moving ? static_cast<int>(std::round(std::sin(phase * 10.0f) * 1.4f)) : 0;
    if (step > 1) step = 1;
    if (step < -1) step = -1;

    fb.fillEllipseA(x + 8 * s, y + 15 * s, 5 * s, 2 * s, black, 70);

    int legY = shorty ? 13 : 12;
    if (!dress) {
        fb.fillRect(x + 4 * s, y + (legY + std::max(0, step)) * s, 3 * s, 3 * s, L.pants);
        fb.fillRect(x + 9 * s, y + (legY + std::max(0, -step)) * s, 3 * s, 3 * s, L.pants);
        fb.fillRect(x + 4 * s, y + (legY + 2 + std::max(0, step)) * s, 3 * s, 2 * s, L.boots);
        fb.fillRect(x + 9 * s, y + (legY + 2 + std::max(0, -step)) * s, 3 * s, 2 * s, L.boots);
    } else {
        fb.fillRect(x + 4 * s, y + (legY + 1) * s, 8 * s, 3 * s, L.shirtDark);
        fb.fillRect(x + 5 * s, y + 15 * s, 6 * s, 1 * s, L.boots);
    }

    int torsoY = shorty ? 7 : 6;
    fb.fillRect(x + 3 * s, y + torsoY * s, 10 * s, 6 * s, L.shirtDark);
    fb.fillRect(x + 4 * s, y + (torsoY + 1) * s, 8 * s, 5 * s, L.shirt);
    fb.fillRect(x + 4 * s, y + (torsoY + 4) * s, 8 * s, 1 * s, L.accent);

    fb.fillRect(x + 1 * s, y + (torsoY + 1) * s, 2 * s, 4 * s, L.shirtDark);
    fb.fillRect(x + 13 * s, y + (torsoY + 1) * s, 2 * s, 4 * s, L.shirtDark);
    fb.fillRect(x + 1 * s, y + (torsoY + 5) * s, 2 * s, 1 * s, L.skin);
    fb.fillRect(x + 13 * s, y + (torsoY + 5) * s, 2 * s, 1 * s, L.skin);

    armorDetails(fb, x, y, s, dir, L);

    int headCY = shorty ? 4 : 4;
    int headR = shorty ? 5 : 4;
    fb.fillCircle(x + 8 * s, y + headCY * s, (headR + 1) * s, L.hair);
    fb.fillCircle(x + 8 * s, y + headCY * s, headR * s, L.skin);

    if (helmet) {
        fb.fillCircle(x + 8 * s, y + 3 * s, (headR + 1) * s, stone);
        fb.fillRect(x + 3 * s, y + 4 * s, 10 * s, 1 * s, stoneDark);
        if (dir == 0) fb.fillRect(x + 7 * s, y + 4 * s, 2 * s, 3 * s, stoneDark);
    } else if (dir == 1) {
        fb.fillCircle(x + 8 * s, y + headCY * s, headR * s, L.hair);
        fb.fillRect(x + 4 * s, y + (headCY - 2) * s, 8 * s, 2 * s, shade(L.hair, 1.25f));
    } else {
        fb.fillRect(x + 3 * s, y + 1 * s, 10 * s, 3 * s, L.hair);
        if (dir == 0) {
            fb.fillRect(x + 3 * s, y + 3 * s, 2 * s, 2 * s, L.hair);
            fb.fillRect(x + 11 * s, y + 3 * s, 2 * s, 2 * s, L.hair);
            fb.fillRect(x + 6 * s, y + 5 * s, 1 * s, 1 * s, shade(L.skin, 0.35f));
            fb.fillRect(x + 9 * s, y + 5 * s, 1 * s, 1 * s, shade(L.skin, 0.35f));
            fb.fillRect(x + 7 * s, y + 7 * s, 2 * s, 1 * s, shade(L.skin, 0.75f));
        } else if (dir == 2) {
            fb.fillRect(x + 11 * s, y + 2 * s, 3 * s, 4 * s, L.hair);
            fb.fillRect(x + 5 * s, y + 5 * s, 1 * s, 1 * s, shade(L.skin, 0.35f));
            fb.fillRect(x + 3 * s, y + 5 * s, 1 * s, 1 * s, skinDark);
        } else {
            fb.fillRect(x + 2 * s, y + 2 * s, 3 * s, 4 * s, L.hair);
            fb.fillRect(x + 10 * s, y + 5 * s, 1 * s, 1 * s, shade(L.skin, 0.35f));
            fb.fillRect(x + 12 * s, y + 5 * s, 1 * s, 1 * s, skinDark);
        }
    }

    heldWeapon(fb, x, y, s, dir, L);
}

Look playerLook(int weapon, int armor) {
    Look L{skin, hair, tunic, tunicDark, 0xFF23305A, 0xFF2A1C14, 0xFFE2C275};
    L.weapon = clampInt(weapon, 0, 3);
    L.armor = clampInt(armor, 0, 3);
    switch (L.armor) {
        case 1:
            L.shirt = 0xFF8A5A32;
            L.shirtDark = 0xFF5A3A1E;
            L.pants = 0xFF4A3020;
            L.boots = 0xFF2A1C14;
            L.accent = 0xFFD8B878;
            break;
        case 2:
            L.shirt = 0xFF9AA4B4;
            L.shirtDark = 0xFF596274;
            L.pants = 0xFF3A3A46;
            L.boots = 0xFF2A2A34;
            L.accent = 0xFFD0D8E4;
            break;
        case 3:
            L.shirt = 0xFFE8C874;
            L.shirtDark = 0xFF9A6A24;
            L.pants = 0xFF5A4218;
            L.boots = 0xFF3A2A10;
            L.accent = 0xFFFFF2C0;
            break;
        default:
            break;
    }
    return L;
}

Look npcLook(NpcStyle style) {
    switch (style) {
        case NpcStyle::Elder:
            return {skin, 0xFFDDD8CE, 0xFF5B4E8C, 0xFF3C3266, 0xFF3C3266, 0xFF2A1C14, 0xFFE2C275};
        case NpcStyle::Shopkeeper:
            return {skin, 0xFF8B5A2B, 0xFF3E7A4E, 0xFF2A5535, 0xFF5A4020, 0xFF2A1C14, 0xFFE2C275};
        case NpcStyle::Innkeeper:
            return {skin, 0xFFA0522D, 0xFFA34A4A, 0xFF7A3030, 0xFF5A4020, 0xFF2A1C14, 0xFFF0D8A0};
        case NpcStyle::Guard:
            return {skin, 0xFF5A4A2A, 0xFF7A7F8C, 0xFF4F5460, 0xFF3A3A46, 0xFF2A1C14, 0xFFC0C8D8};
        case NpcStyle::Kid:
            return {skin, 0xFFC08A4A, 0xFFD08A3A, 0xFFA86A28, 0xFF4A6AA8, 0xFF2A1C14, 0xFFF0D8A0};
        case NpcStyle::Farmer:
            return {skin, 0xFF6B4326, 0xFF8A6A3A, 0xFF6A4E28, 0xFF5A4A2A, 0xFF2A1C14, 0xFFD8B878};
        case NpcStyle::Guardian:
            return {0xFFE8D8B0, 0xFFE2C275, 0xFFC89A3A, 0xFF8A6420, 0xFF5A4218,
                    0xFF3A2A10, 0xFFFFE9A8};
        default:
            return {skin, 0xFF523218, 0xFF4A8A8A, 0xFF2E6666, 0xFF3A3A5A, 0xFF2A1C14, 0xFFE2C275};
    }
}

void wardenCore(Framebuffer& fb, int cx, int cy, float time, int size) {
    float pulse = 0.5f + 0.5f * std::sin(time * 2.0f);
    auto rad = [size](float k) { return std::max(1, static_cast<int>(size * k)); };
    int top = cy - size / 2;
    int bottom = cy + size / 2;

    // Aura and ground shadow.
    fb.fillEllipseA(cx, cy + size / 3, rad(0.85f), rad(0.36f), 0xFF7A3CC8,
                    24 + static_cast<int>(pulse * 18.0f));
    fb.fillEllipseA(cx, bottom, size / 2, size / 7, black, 120);

    // Flowing cloak: broad shoulders, pinched waist, ragged hem.
    tri(fb, cx, top + size / 5, cx - size * 5 / 12, bottom, cx + size * 5 / 12, bottom, 0xFF1C132C);
    tri(fb, cx, top + size / 4, cx - size / 4, bottom, cx + size / 4, bottom, 0xFF2A1E42);
    tri(fb, cx, top + size / 3, cx - size / 6, bottom - size / 14,
        cx + size / 6, bottom - size / 14, 0xFF120C1E);
    // Purple rim light down the left edge.
    fb.line(cx - size / 4, top + size / 3, cx - size / 2 + size / 12, bottom - size / 10, 0xFF4A2E72);
    fb.line(cx - size / 4 + 1, top + size / 3 + 1, cx - size / 2 + size / 12 + 1,
            bottom - size / 10, 0xFF35244E);

    // Shoulder pauldrons with spikes.
    for (int s = -1; s <= 1; s += 2) {
        int sx = cx + s * size / 3;
        int sy = top + size * 5 / 12;
        fb.fillEllipse(sx, sy, rad(0.19f), rad(0.14f), 0xFF2E2448);
        fb.fillEllipse(sx, sy, rad(0.14f), rad(0.10f), 0xFF3E315E);
        tri(fb, sx - s * rad(0.12f), sy - rad(0.04f), sx + s * rad(0.10f), sy - rad(0.20f),
            sx + s * rad(0.16f), sy + rad(0.02f), 0xFF1A1226);
        tri(fb, sx - s * rad(0.16f), sy + rad(0.02f), sx - s * rad(0.30f), sy + rad(0.10f),
            sx - s * rad(0.10f), sy + rad(0.14f), 0xFF241A38);
    }

    // Hood: bright rim around a void.
    int hoodY = top + size / 6;
    fb.fillEllipse(cx, hoodY, rad(0.27f), rad(0.22f), 0xFF4A2E72);
    fb.fillEllipse(cx, hoodY + rad(0.035f), rad(0.235f), rad(0.19f), 0xFF241A38);
    fb.fillEllipse(cx, hoodY + rad(0.05f), rad(0.17f), rad(0.14f), 0xFF06040A);
    fb.line(cx - rad(0.20f), hoodY + rad(0.02f), cx - rad(0.08f), hoodY + rad(0.19f), 0xFF35244E);
    fb.line(cx + rad(0.20f), hoodY + rad(0.02f), cx + rad(0.08f), hoodY + rad(0.19f), 0xFF35244E);

    // Thick curved horns.
    for (int s = -1; s <= 1; s += 2) {
        const float hx[4] = {0.19f, 0.31f, 0.41f, 0.46f};
        const float hy[4] = {0.00f, -0.05f, -0.14f, -0.26f};
        const float hr[4] = {0.060f, 0.048f, 0.034f, 0.016f};
        for (int i = 0; i < 4; ++i) {
            int x = cx + s * static_cast<int>(size * hx[i]);
            int y = hoodY + static_cast<int>(size * hy[i]);
            fb.fillEllipse(x, y, rad(hr[i]), rad(hr[i] * 0.85f), i < 2 ? 0xFF2C2044 : 0xFF3A2A56);
        }
        fb.fillEllipse(cx + s * rad(0.46f), hoodY - rad(0.26f), rad(0.016f), rad(0.02f), 0xFF8A6AB8);
    }

    // Slanted glowing eyes.
    int eyeY = hoodY + rad(0.05f);
    int eyeDX = rad(0.085f);
    for (int s = -1; s <= 1; s += 2) {
        int ex = cx + s * eyeDX;
        fb.fillEllipseA(ex, eyeY, rad(0.11f), rad(0.08f), 0xFFB03CFF, 46);
        fb.fillRect(ex - rad(0.055f), eyeY - 1, rad(0.10f), rad(0.045f), 0xFFFF5AD8);
        fb.fillRect(ex - rad(0.03f), eyeY - 1, rad(0.05f), rad(0.03f), 0xFFFFD0F4);
    }

    // Belt with a soul gem.
    int waistY = top + size * 2 / 3;
    fb.fillRect(cx - size / 4, waistY, size / 2, std::max(1, size / 32), 0xFF3A2A56);
    fb.fillRect(cx - rad(0.045f), waistY - rad(0.035f), rad(0.09f), rad(0.08f), 0xFFB03CFF);
    fb.fillRect(cx - rad(0.025f), waistY - rad(0.02f), rad(0.05f), rad(0.045f), 0xFFFFC8F0);

    // Clawed hands reaching forward.
    for (int s = -1; s <= 1; s += 2) {
        int hx = cx + s * size * 5 / 12;
        int hy = top + size * 3 / 5;
        fb.line(cx + s * size / 4, top + size / 2, hx, hy, 0xFF2E2448);
        fb.fillEllipse(hx, hy, rad(0.075f), rad(0.055f), 0xFF3A2A56);
        for (int c = -1; c <= 1; ++c) {
            int fx = hx + c * rad(0.045f);
            tri(fb, fx - rad(0.018f), hy + rad(0.03f), fx + rad(0.018f), hy + rad(0.03f),
                fx, hy + rad(0.16f), 0xFF8A6AB8);
        }
    }

    // Tattered hem.
    for (int i = 0; i < 5; ++i) {
        int bx = cx - size / 2 + size / 10 + i * (size / 6);
        int sway = static_cast<int>(std::sin(time * 3.0f + i) * size / 20.0f);
        tri(fb, bx + sway, bottom - size / 8, bx - size / 14, bottom + size / 10, bx + size / 14,
            bottom + size / 10, 0xFF120C1E);
    }

    // Aura motes.
    for (int i = 0; i < 6; ++i) {
        float a = time * 1.4f + i * 1.05f;
        int mx = cx + static_cast<int>(std::cos(a) * size * 0.62f);
        int my = cy + static_cast<int>(std::sin(a * 1.3f) * size * 0.48f);
        fb.pixel(mx, my, (i & 1) ? 0xFFD08AFF : 0xFFB03CFF);
    }
}

}  // namespace

void shadow(Framebuffer& fb, int cx, int cy, int rx, int ry, int alpha) {
    fb.fillEllipseA(cx, cy, rx, ry, black, alpha);
}

void tile(Framebuffer& fb, Tile t, int px, int py, int tx, int ty, float time) {
    uint32_t h = hash2(tx, ty);
    switch (t) {
        case Tile::Grass:
            grassBase(fb, px, py, tx, ty);
            break;
        case Tile::Flower: {
            grassBase(fb, px, py, tx, ty);
            int fx = px + 4 + static_cast<int>(h & 7u);
            int fy = py + 4 + static_cast<int>((h >> 3) & 7u);
            fb.fillRect(fx + 1, fy + 2, 1, 3, grassDark);
            uint32_t c = ((h >> 6) & 1u) ? 0xFFFFE9A8u : 0xFFF2A0C0u;
            fb.fillRect(fx, fy, 3, 1, c);
            fb.fillRect(fx + 1, fy - 1, 1, 3, c);
            fb.fillRect(fx + 1, fy + 1, 1, 1, gold);
            break;
        }
        case Tile::TallGrass: {
            fb.fillRect(px, py, 16, 16, mix(grass, grassDark, 0.35f));
            for (int i = 0; i < 6; ++i) {
                int bx = px + 2 + i * 2 + static_cast<int>(h >> (i * 3) & 1u);
                int sway = static_cast<int>(std::sin(time * 2.2f + i * 1.4f + tx * 0.7f + ty) * 1.6f);
                int bh = 5 + static_cast<int>((h >> (i * 2)) & 3u);
                fb.vline(bx + sway, py + 14 - bh, bh, (i & 1) ? grassLight : 0xFF2E5A2E);
                fb.pixel(bx + sway, py + 14 - bh, 0xFF8CC46A);
            }
            break;
        }
        case Tile::Path: {
            fb.fillRect(px, py, 16, 16, path);
            if ((tx + ty) & 1) fb.fillRectA(px, py, 16, 16, pathDark, 22);
            for (int i = 0; i < 4; ++i) {
                int nx = px + static_cast<int>((h >> (i * 4)) & 13u) + 1;
                int ny = py + static_cast<int>((h >> (i * 4 + 2)) & 13u) + 1;
                fb.fillRect(nx, ny, (i & 1) ? 2 : 1, 1, (i & 1) ? pathLight : pathDark);
            }
            break;
        }
        case Tile::Sand: {
            fb.fillRect(px, py, 16, 16, sand);
            for (int i = 0; i < 4; ++i) {
                int nx = px + static_cast<int>((h >> (i * 4)) & 13u) + 1;
                int ny = py + static_cast<int>((h >> (i * 4 + 2)) & 13u) + 1;
                fb.pixel(nx, ny, (i & 1) ? sandDark : pathLight);
            }
            break;
        }
        case Tile::Water:
        case Tile::DeepWater: {
            bool deep = t == Tile::DeepWater;
            fb.fillRect(px, py, 16, 16, deep ? waterDeep : water);
            if (!deep) {
                fb.fillRectA(px, py, 16, 16, waterLight, 10);
                for (int i = 0; i < 2; ++i) {
                    int phase = static_cast<int>(std::sin(time * 1.5f + tx * 0.8f + ty * 0.55f + i * 2.1f) * 2.0f);
                    int wx = px + static_cast<int>((h >> (i * 5)) & 15u) + phase;
                    int wy = py + 3 + static_cast<int>((h >> (i * 3 + 2)) & 9u) + i * 4;
                    int len = 2 + static_cast<int>((h >> (i * 2 + 1)) & 3u);
                    wx = (wx - px + 16) % 16 + px;
                    fb.fillRect(wx, wy, len, 1, withAlpha(waterLight, 200));
                    fb.fillRect(wx + 1, wy + 2, std::max(1, len - 1), 1, withAlpha(foam, 120));
                }
            } else {
                int off = static_cast<int>(std::sin(time * 1.1f + tx * 1.3f + ty * 0.7f) * 2.0f);
                int wx = px + ((off + 16) % 16);
                fb.fillRect(wx, py + 7, 2, 1, withAlpha(water, 160));
                fb.fillRect(px + ((off + 7) % 16), py + 12, 1, 1, withAlpha(water, 120));
            }
            break;
        }
        case Tile::Tree: {
            grassBase(fb, px, py, tx, ty);
            fb.fillEllipseA(px + 8, py + 14, 5, 2, black, 80);
            fb.fillRect(px + 6, py + 9, 4, 6, trunk);
            fb.fillRect(px + 9, py + 9, 1, 6, shade(trunk, 0.7f));
            fb.fillCircle(px + 8, py + 6, 6, leafDark);
            fb.fillCircle(px + 8, py + 6, 5, leaf);
            fb.fillCircle(px + 6, py + 4, 3, leafLight);
            fb.fillRect(px + 10, py + 8, 1, 1, leafLight);
            break;
        }
        case Tile::Rock: {
            grassBase(fb, px, py, tx, ty);
            fb.fillEllipseA(px + 8, py + 13, 6, 2, black, 80);
            fb.fillEllipse(px + 8, py + 8, 6, 5, stoneDark);
            fb.fillEllipse(px + 8, py + 7, 5, 4, stone);
            fb.fillRect(px + 5, py + 4, 3, 1, 0xFFA8AEBA);
            break;
        }
        case Tile::Wall: {
            fb.fillRect(px, py, 16, 16, wall);
            fb.fillRect(px, py + 12, 16, 4, wallDark);
            fb.fillRect(px, py, 2, 16, beam);
            fb.fillRect(px + 14, py, 2, 16, beam);
            if (((tx * 3 + ty) & 3) == 0) {
                fb.fillRect(px + 5, py + 3, 6, 5, 0xFF2E3A52);
                fb.fillRect(px + 6, py + 4, 4, 3, 0xFF7FA8D8);
                fb.fillRect(px + 8, py + 3, 1, 5, beam);
                fb.fillRect(px + 5, py + 5, 6, 1, beam);
            }
            break;
        }
        case Tile::Roof: {
            fb.fillRect(px, py, 16, 16, roofDark);
            for (int row = 0; row < 4; ++row) {
                int yy = py + row * 4;
                fb.fillRect(px, yy, 16, 3, roof);
                int off = (row & 1) ? 2 : 0;
                for (int x = 0; x < 16; x += 8) {
                    int sx = px + x + off;
                    if (sx < px + 16) fb.fillRect(sx, yy, 1, 3, roofDark);
                }
                fb.fillRect(px, yy + 2, 16, 1, shade(roof, 0.85f));
                fb.fillRect(px, yy, 16, 1, roofLight);
            }
            break;
        }
        case Tile::Door: {
            fb.fillRect(px, py, 16, 16, wall);
            fb.fillRect(px + 1, py, 14, 2, beam);
            fb.fillRect(px + 2, py + 2, 12, 14, doorDark);
            fb.fillRect(px + 3, py + 3, 10, 12, door);
            fb.fillRect(px + 4, py + 4, 8, 1, shade(door, 1.3f));
            fb.fillRect(px + 4, py + 8, 8, 1, doorDark);
            fb.fillRect(px + 4, py + 12, 8, 1, doorDark);
            fb.fillCircle(px + 11, py + 9, 1, gold);
            break;
        }
        case Tile::Fence: {
            grassBase(fb, px, py, tx, ty);
            fb.fillRect(px + 1, py + 5, 14, 2, beam);
            fb.fillRect(px + 1, py + 10, 14, 2, beam);
            fb.fillRect(px + 3, py + 3, 2, 11, trunk);
            fb.fillRect(px + 11, py + 3, 2, 11, trunk);
            fb.fillRect(px + 1, py + 5, 14, 1, shade(beam, 1.25f));
            break;
        }
        case Tile::Bridge: {
            fb.fillRect(px, py, 16, 16, water);
            fb.fillRect(px, py, 16, 16, 0xFF8A6438);
            for (int x = 0; x < 16; x += 4) fb.fillRect(px + x, py, 1, 16, 0xFF6A4A28);
            fb.fillRect(px, py, 16, 2, 0xFF5A3A1E);
            fb.fillRect(px, py + 14, 16, 2, 0xFF5A3A1E);
            fb.fillRect(px + 1, py + 4, 2, 1, 0xFFB08A58);
            break;
        }
        case Tile::CaveFloor: {
            fb.fillRect(px, py, 16, 16, caveFloor);
            if ((tx + ty) & 1) fb.fillRectA(px, py, 16, 16, caveFloorDark, 40);
            for (int i = 0; i < 4; ++i) {
                int nx = px + static_cast<int>((h >> (i * 4)) & 13u) + 1;
                int ny = py + static_cast<int>((h >> (i * 4 + 2)) & 13u) + 1;
                fb.pixel(nx, ny, (i & 1) ? 0xFF6A6280 : 0xFF38324A);
            }
            if ((h & 15u) == 3u) {
                fb.line(px + 3, py + 4, px + 5, py + 9, 0xFF332E42);
                fb.line(px + 5, py + 9, px + 4, py + 13, 0xFF332E42);
            }
            break;
        }
        case Tile::CaveWall: {
            fb.fillRect(px, py, 16, 16, caveWallDark);
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 2; ++col) {
                    int ox = col * 8 + ((row & 1) ? 4 : 0);
                    int bx = px + (ox % 16);
                    int by = py + row * 8;
                    uint32_t shadeC = ((h >> (row * 4 + col)) & 1u) ? caveWall : shade(caveWall, 0.86f);
                    fb.fillRect(bx, by, 8, 8, shadeC);
                    fb.fillRect(bx, by, 8, 1, shade(caveWall, 1.35f));
                    fb.fillRect(bx, by + 7, 8, 1, caveWallDark);
                }
            }
            break;
        }
        case Tile::Lava: {
            fb.fillRect(px, py, 16, 16, lava);
            for (int i = 0; i < 4; ++i) {
                int bx = px + 2 + i * 4;
                int by = py + 6 + static_cast<int>(std::sin(time * 2.0f + i * 1.7f + tx) * 2.0f);
                fb.fillCircle(bx, by, 1 + (i & 1), lavaLight);
            }
            fb.fillRect(px, py + 13, 16, 3, 0xFFB03C10);
            fb.fillRect(px + 4, py + 2, 5, 1, 0xFFFFD07A);
            break;
        }
        case Tile::CaveEntrance: {
            fb.fillRect(px, py, 16, 16, caveWallDark);
            fb.fillRect(px, py, 16, 3, caveWall);
            fb.fillEllipse(px + 8, py + 11, 7, 6, 0xFF0A0810);
            fb.fillRect(px + 2, py + 11, 12, 5, 0xFF0A0810);
            fb.fillRect(px + 3, py + 14, 10, 2, 0xFF241E30);
            fb.fillRect(px + 4, py + 12, 8, 1, 0xFF1A1524);
            break;
        }
        case Tile::RuinEntrance: {
            fb.fillRect(px, py, 16, 16, caveWallDark);
            // Stone frame with a carved arch.
            fb.fillRect(px + 1, py + 2, 3, 14, stoneDark);
            fb.fillRect(px + 12, py + 2, 3, 14, stoneDark);
            fb.fillRect(px + 1, py + 1, 14, 3, stone);
            fb.fillRect(px + 3, py + 2, 10, 2, 0xFFB0A888);
            fb.fillEllipse(px + 8, py + 10, 5, 5, 0xFF0A0810);
            fb.fillRect(px + 3, py + 10, 10, 6, 0xFF0A0810);
            // Faint sun rune above the doorway.
            float pulse = 0.5f + 0.5f * std::sin(time * 2.4f + tx * 0.5f);
            uint32_t rune = withAlpha(0xFFFFD87A, 130 + static_cast<int>(pulse * 100.0f));
            fb.fillRect(px + 7, py + 1, 2, 1, rune);
            fb.fillRect(px + 6, py + 2, 4, 1, rune);
            fb.fillRect(px + 7, py + 3, 2, 1, rune);
            fb.pixel(px + 4, py + 13, withAlpha(gold, 120));
            fb.pixel(px + 11, py + 13, withAlpha(gold, 120));
            break;
        }
        case Tile::Sign: {
            grassBase(fb, px, py, tx, ty);
            fb.fillRect(px + 7, py + 7, 2, 8, trunk);
            fb.fillRect(px + 2, py + 2, 12, 8, doorDark);
            fb.fillRect(px + 3, py + 3, 10, 6, door);
            fb.fillRect(px + 4, py + 4, 8, 1, 0xFFC8A878);
            fb.fillRect(px + 4, py + 6, 6, 1, 0xFFC8A878);
            break;
        }
        case Tile::Pillar: {
            fb.fillRect(px, py, 16, 16, caveFloorDark);
            fb.fillRect(px + 3, py, 10, 16, stoneDark);
            fb.fillRect(px + 4, py, 8, 16, stone);
            fb.fillRect(px + 5, py, 2, 16, 0xFFA0A6B4);
            fb.fillRect(px + 2, py, 12, 2, stoneDark);
            fb.fillRect(px + 2, py + 13, 12, 2, stoneDark);
            break;
        }
        case Tile::Void:
        default:
            fb.fillRect(px, py, 16, 16, 0xFF05050Au);
            break;
    }
}

void player(Framebuffer& fb, int px, int py, int dir, float phase, bool moving, int weapon,
            int armor) {
    humanoid(fb, px, py, 1, dir, phase, moving, playerLook(weapon, armor));
}

void playerScaled(Framebuffer& fb, int x, int y, int scale, int dir, float phase, bool moving,
                  int weapon, int armor) {
    humanoid(fb, x, y, scale, dir, phase, moving, playerLook(weapon, armor));
}

void heroBack(Framebuffer& fb, int x, int y, int scale, float time, int weapon, int armor) {
    Look L = playerLook(weapon, armor);
    humanoid(fb, x, y, scale, 1, time * 0.0f, false, L);
}

void npc(Framebuffer& fb, int px, int py, NpcStyle style, int dir, float time) {
    Look L = npcLook(style);
    if (style == NpcStyle::Warden) {
        wardenWorld(fb, px, py, time);
        return;
    }
    bool guardian = style == NpcStyle::Guardian;
    humanoid(fb, px, py, 1, dir, 0.0f, false, L, style == NpcStyle::Kid,
             style == NpcStyle::Guard || guardian, style == NpcStyle::Innkeeper);
    if (guardian) {
        // Gilded aura and a ceremonial blade.
        float pulse = 0.5f + 0.5f * std::sin(time * 2.2f);
        fb.fillEllipseA(px + 8, py + 8, 9, 9, 0xFFFFD87A, 18 + static_cast<int>(pulse * 18));
        fb.vline(px + 14, py + 3, 11, 0xFFE8D8A0);
        fb.fillRect(px + 13, py + 2, 3, 2, 0xFFFFE9A8);
        fb.pixel(px + 14, py + 14, 0xFFFFFFFF);
        fb.pixel(px + 4, py + 4, withAlpha(0xFFFFE9A8, 160 + static_cast<int>(pulse * 80)));
    }

    if (style == NpcStyle::Elder) {
        fb.vline(px + 14, py + 5, 10, trunk);
        fb.fillCircle(px + 14, py + 4, 2, 0xFF6A4A9A);
        fb.fillCircle(px + 14, py + 4, 1, 0xFFD8A8FF);
    } else if (style == NpcStyle::Guard) {
        fb.vline(px + 14, py + 1, 15, 0xFF8A6A3A);
        tri(fb, px + 14, py, px + 12, py + 5, px + 16, py + 5, 0xFFC0C8D8);
    } else if (style == NpcStyle::Farmer) {
        fb.fillEllipse(px + 8, py + 2, 6, 2, 0xFFD8B878);
        fb.fillEllipse(px + 8, py + 1, 3, 2, 0xFFE8CC90);
    }
}

void chest(Framebuffer& fb, int px, int py, bool opened, float time) {
    shadow(fb, px + 8, py + 14, 6, 2, 80);
    if (!opened) {
        fb.fillRect(px + 2, py + 7, 12, 7, doorDark);
        fb.fillRect(px + 3, py + 8, 10, 5, door);
        fb.fillRect(px + 1, py + 3, 14, 5, doorDark);
        fb.fillRect(px + 2, py + 4, 12, 3, door);
        fb.fillRect(px + 2, py + 4, 12, 1, shade(door, 1.35f));
        fb.fillRect(px + 7, py + 3, 2, 11, gold);
        fb.fillRect(px + 2, py + 9, 12, 1, shade(gold, 0.7f));
        fb.fillRect(px + 7, py + 9, 2, 2, 0xFFFFE9A8);
        float glint = std::sin(time * 3.0f + px * 0.1f);
        if (glint > 0.7f) {
            fb.pixel(px + 12, py + 3, white);
            fb.pixel(px + 13, py + 4, 0xFFFFF2C0);
        }
    } else {
        fb.fillRect(px + 1, py + 1, 14, 4, doorDark);
        fb.fillRect(px + 2, py + 2, 12, 2, door);
        fb.fillRect(px + 2, py + 6, 12, 8, doorDark);
        fb.fillRect(px + 3, py + 7, 10, 6, 0xFF1C1208);
        fb.fillRect(px + 2, py + 6, 12, 2, shade(gold, 0.6f));
    }
}

void wardenWorld(Framebuffer& fb, int px, int py, float time) {
    wardenCore(fb, px + 8, py + 2, time, 34);
}

void enemy(Framebuffer& fb, EnemyType type, int cx, int cy, float time, bool hurt) {
    switch (type) {
        case EnemyType::Slime: {
            int bob = static_cast<int>(std::sin(time * 2.2f) * 2.0f);
            int sy = cy + bob;
            shadow(fb, cx, cy + 15, 14, 4, 100);
            fb.fillEllipse(cx, sy, 15, 12, 0xFF1F5A2A);
            fb.fillEllipse(cx, sy, 14, 11, 0xFF4FA83E);
            fb.fillEllipse(cx - 3, sy - 4, 6, 4, 0xFF7FD060);
            fb.fillEllipse(cx + 6, sy + 3, 4, 3, 0xFF3A8A32);
            fb.fillRect(cx - 6, sy - 2, 2, 3, 0xFF14301A);
            fb.fillRect(cx + 4, sy - 2, 2, 3, 0xFF14301A);
            fb.fillRect(cx - 2, sy + 4, 4, 1, 0xFF14301A);
            if (hurt) fb.fillEllipseA(cx, sy, 14, 11, white, 130);
            break;
        }
        case EnemyType::Bat: {
            shadow(fb, cx, cy + 18, 10, 3, 90);
            int flap = static_cast<int>(std::sin(time * 9.0f) * 8.0f);
            tri(fb, cx - 6, cy - 2, cx - 26, cy - 10 + flap, cx - 8, cy + 8, 0xFF4A2A5A);
            tri(fb, cx + 6, cy - 2, cx + 26, cy - 10 + flap, cx + 8, cy + 8, 0xFF4A2A5A);
            tri(fb, cx - 8, cy - 1, cx - 22, cy - 7 + flap, cx - 9, cy + 6, 0xFF7A4A8A);
            tri(fb, cx + 8, cy - 1, cx + 22, cy - 7 + flap, cx + 9, cy + 6, 0xFF7A4A8A);
            fb.fillEllipse(cx, cy + 2, 8, 10, 0xFF2A1636);
            fb.fillCircle(cx, cy - 7, 7, 0xFF3A2048);
            tri(fb, cx - 6, cy - 10, cx - 4, cy - 20, cx - 1, cy - 11, 0xFF3A2048);
            tri(fb, cx + 6, cy - 10, cx + 4, cy - 20, cx + 1, cy - 11, 0xFF3A2048);
            fb.fillRect(cx - 4, cy - 8, 2, 2, 0xFFFF4A6A);
            fb.fillRect(cx + 2, cy - 8, 2, 2, 0xFFFF4A6A);
            fb.fillRect(cx - 1, cy - 4, 2, 2, white);
            if (hurt) fb.fillEllipseA(cx, cy, 12, 16, white, 130);
            break;
        }
        case EnemyType::Wolf: {
            shadow(fb, cx, cy + 16, 20, 4, 100);
            tri(fb, cx - 16, cy - 2, cx - 28, cy - 12, cx - 20, cy + 4, 0xFF4A4A58);
            fb.fillEllipse(cx - 2, cy + 3, 18, 10, 0xFF3A3A48);
            fb.fillEllipse(cx - 2, cy + 2, 17, 9, 0xFF6A6A7C);
            fb.fillRect(cx - 14, cy + 9, 4, 7, 0xFF4A4A58);
            fb.fillRect(cx - 6, cy + 10, 4, 6, 0xFF4A4A58);
            fb.fillRect(cx + 4, cy + 10, 4, 6, 0xFF4A4A58);
            fb.fillRect(cx + 10, cy + 9, 4, 7, 0xFF4A4A58);
            fb.fillEllipse(cx + 14, cy - 4, 10, 9, 0xFF3A3A48);
            fb.fillEllipse(cx + 14, cy - 4, 9, 8, 0xFF7A7A8C);
            tri(fb, cx + 8, cy - 10, cx + 6, cy - 20, cx + 12, cy - 12, 0xFF3A3A48);
            tri(fb, cx + 18, cy - 10, cx + 22, cy - 20, cx + 22, cy - 11, 0xFF3A3A48);
            fb.fillRect(cx + 12, cy - 6, 2, 2, 0xFFFF3A3A);
            fb.fillRect(cx + 18, cy - 6, 2, 2, 0xFFFF3A3A);
            fb.fillRect(cx + 21, cy - 1, 4, 3, 0xFF2A2A34);
            fb.fillRect(cx + 23, cy + 3, 2, 2, white);
            if (hurt) fb.fillEllipseA(cx, cy, 24, 16, white, 120);
            break;
        }
        case EnemyType::Skeleton: {
            shadow(fb, cx, cy + 18, 12, 3, 90);
            uint32_t bone = 0xFFE8E2D0, boneDark = 0xFFA8A090;
            fb.vline(cx, cy - 2, 12, bone);
            fb.fillRect(cx - 6, cy + 4, 12, 1, boneDark);
            fb.fillRect(cx - 5, cy + 7, 10, 1, boneDark);
            fb.fillRect(cx - 4, cy + 10, 8, 1, boneDark);
            fb.fillRect(cx - 5, cy + 12, 10, 3, bone);
            fb.fillRect(cx - 8, cy + 13, 3, 5, bone);
            fb.fillRect(cx + 5, cy + 13, 3, 5, bone);
            fb.fillRect(cx - 10, cy + 1, 4, 2, bone);
            fb.fillRect(cx - 12, cy + 2, 2, 8, boneDark);
            fb.fillRect(cx + 6, cy + 1, 4, 2, bone);
            fb.fillRect(cx + 12, cy + 2, 2, 8, boneDark);
            fb.fillCircle(cx, cy - 9, 8, bone);
            fb.fillRect(cx - 6, cy - 14, 3, 3, 0xFF1A1A1A);
            fb.fillRect(cx + 3, cy - 14, 3, 3, 0xFFFF6A3A);
            fb.fillRect(cx - 4, cy - 7, 8, 3, 0xFF1A1A1A);
            fb.fillRect(cx - 3, cy - 6, 1, 2, bone);
            fb.fillRect(cx - 1, cy - 6, 1, 2, bone);
            fb.fillRect(cx + 1, cy - 6, 1, 2, bone);
            fb.line(cx + 14, cy - 4, cx + 16, cy + 16, 0xFF8A8A96);
            fb.line(cx + 13, cy + 2, cx + 17, cy + 2, 0xFF5A5A66);
            if (hurt) fb.fillEllipseA(cx, cy, 16, 22, white, 120);
            break;
        }
        case EnemyType::Warden:
            wardenCore(fb, cx, cy - 16, time, 62);
            if (hurt) fb.fillEllipseA(cx, cy, 34, 40, white, 90);
            break;
        case EnemyType::Guardian: {
            // Fallen guardian of the Dawn Ruins: gilded plate, tower shield,
            // and a sunblade that never stopped burning.
            shadow(fb, cx, cy + 20, 18, 4, 110);
            float pulse = 0.5f + 0.5f * std::sin(time * 2.4f);
            uint32_t goldArmor = 0xFFD8A83A, goldLight = 0xFFFFE08A, goldDark = 0xFF8A6420;
            uint32_t plate = 0xFFB08A48, cape = 0xFF7A2E2E;
            fb.fillEllipseA(cx, cy + 4, 30, 32, 0xFFFFD87A, 14 + static_cast<int>(pulse * 12));

            // Cape.
            tri(fb, cx - 14, cy - 14, cx - 24, cy + 18, cx + 2, cy + 18, cape);
            tri(fb, cx + 14, cy - 14, cx + 24, cy + 18, cx - 2, cy + 18, 0xFF5A2020);

            // Legs and boots.
            fb.fillRect(cx - 10, cy + 8, 7, 12, plate);
            fb.fillRect(cx + 3, cy + 8, 7, 12, plate);
            fb.fillRect(cx - 11, cy + 18, 9, 4, goldDark);
            fb.fillRect(cx + 2, cy + 18, 9, 4, goldDark);

            // Torso plate.
            fb.fillRect(cx - 15, cy - 14, 30, 24, goldDark);
            fb.fillRect(cx - 13, cy - 12, 26, 20, goldArmor);
            fb.fillRect(cx - 13, cy - 12, 26, 1, goldLight);
            fb.fillRect(cx - 4, cy - 9, 8, 10, goldLight);
            fb.fillCircle(cx, cy - 5, 3, 0xFFFFF0B0);
            fb.fillCircle(cx, cy - 5, 1, white);
            fb.fillRect(cx - 13, cy + 4, 26, 2, goldDark);

            // Pauldrons.
            fb.fillEllipse(cx - 16, cy - 10, 7, 5, goldDark);
            fb.fillEllipse(cx + 16, cy - 10, 7, 5, goldDark);
            fb.fillEllipse(cx - 16, cy - 11, 6, 4, goldArmor);
            fb.fillEllipse(cx + 16, cy - 11, 6, 4, goldArmor);

            // Helmet with plume.
            fb.fillCircle(cx, cy - 22, 10, goldDark);
            fb.fillCircle(cx, cy - 22, 9, goldArmor);
            fb.fillRect(cx - 7, cy - 25, 14, 4, 0xFF2A2010);
            fb.fillRect(cx - 6, cy - 24, 5, 2, 0xFFFFE08A);
            fb.fillRect(cx + 2, cy - 24, 5, 2, 0xFFFFE08A);
            fb.fillRect(cx - 1, cy - 21, 2, 4, goldLight);
            tri(fb, cx - 2, cy - 32, cx + 10, cy - 40, cx + 3, cy - 30, 0xFFB03C3C);
            tri(fb, cx - 2, cy - 32, cx + 7, cy - 38, cx + 2, cy - 30, 0xFFE86A4A);

            // Tower shield.
            fb.fillRect(cx - 30, cy - 12, 12, 22, goldDark);
            fb.fillRect(cx - 29, cy - 11, 10, 20, plate);
            fb.fillRect(cx - 27, cy - 8, 6, 14, 0xFFC8A858);
            fb.fillRect(cx - 25, cy - 5, 2, 8, 0xFFFFE9A8);
            fb.fillRect(cx - 27, cy - 1, 6, 2, 0xFFFFE9A8);

            // Sunblade.
            int sway = static_cast<int>(std::sin(time * 1.8f) * 1.5f);
            fb.line(cx + 26 + sway, cy - 20, cx + 30 + sway, cy + 14, 0xFF8A6A3A);
            fb.fillRect(cx + 27 + sway, cy - 34, 3, 16, 0xFFFFE9A8);
            fb.fillRect(cx + 28 + sway, cy - 36, 1, 20, white);
            fb.fillRect(cx + 23 + sway, cy - 20, 9, 2, goldDark);
            fb.fillCircle(cx + 28 + sway, cy - 36, 2, withAlpha(0xFFFFD87A, 170));
            for (int i = 0; i < 5; ++i) {
                float a = time * 1.6f + i * 1.25f;
                int mx = cx + static_cast<int>(std::cos(a) * 34.0f);
                int my = cy + static_cast<int>(std::sin(a * 1.4f) * 30.0f);
                fb.pixel(mx, my, (i & 1) ? 0xFFFFE9A8 : 0xFFFFC84A);
            }
            if (hurt) fb.fillEllipseA(cx, cy, 30, 36, white, 120);
            break;
        }
    }
}

// Compact overworld sprites: the battle versions are drawn much larger, so the
// roaming monsters get their own readable 16x16 art.
void enemyWorld(Framebuffer& fb, EnemyType type, int px, int py, float time) {
    int cx = px + 8;
    switch (type) {
        case EnemyType::Slime: {
            int bob = static_cast<int>(std::sin(time * 3.0f) * 1.0f);
            int cy = py + 10 + bob;
            shadow(fb, cx, py + 14, 7, 2, 90);
            fb.fillEllipse(cx, cy, 8, 6, 0xFF1F5A2A);
            fb.fillEllipse(cx, cy, 7, 5, 0xFF4FA83E);
            fb.fillEllipse(cx - 2, cy - 2, 3, 2, 0xFF7FD060);
            fb.fillRect(cx - 3, cy, 1, 2, 0xFF14301A);
            fb.fillRect(cx + 2, cy, 1, 2, 0xFF14301A);
            break;
        }
        case EnemyType::Bat: {
            int flap = static_cast<int>(std::sin(time * 10.0f) * 3.0f);
            shadow(fb, cx, py + 15, 5, 2, 70);
            tri(fb, cx - 3, py + 8, cx - 9, py + 4 + flap, cx - 4, py + 11, 0xFF4A2A5A);
            tri(fb, cx + 3, py + 8, cx + 9, py + 4 + flap, cx + 4, py + 11, 0xFF4A2A5A);
            tri(fb, cx - 4, py + 8, cx - 8, py + 5 + flap, cx - 4, py + 10, 0xFF7A4A8A);
            tri(fb, cx + 4, py + 8, cx + 8, py + 5 + flap, cx + 4, py + 10, 0xFF7A4A8A);
            fb.fillEllipse(cx, py + 9, 4, 4, 0xFF2A1636);
            fb.fillCircle(cx, py + 6, 3, 0xFF3A2048);
            fb.fillRect(cx - 2, py + 5, 1, 1, 0xFFFF4A6A);
            fb.fillRect(cx + 1, py + 5, 1, 1, 0xFFFF4A6A);
            break;
        }
        case EnemyType::Wolf: {
            shadow(fb, cx, py + 15, 9, 2, 90);
            tri(fb, cx - 6, py + 9, cx - 10, py + 5, cx - 7, py + 12, 0xFF4A4A58);
            fb.fillEllipse(cx, py + 10, 7, 4, 0xFF6A6A7C);
            fb.fillEllipse(cx + 5, py + 8, 4, 3, 0xFF7A7A8C);
            tri(fb, cx + 3, py + 5, cx + 3, py + 2, cx + 5, py + 6, 0xFF3A3A48);
            tri(fb, cx + 6, py + 5, cx + 8, py + 2, cx + 8, py + 6, 0xFF3A3A48);
            fb.fillRect(cx - 5, py + 12, 2, 3, 0xFF4A4A58);
            fb.fillRect(cx - 1, py + 12, 2, 3, 0xFF4A4A58);
            fb.fillRect(cx + 4, py + 12, 2, 3, 0xFF4A4A58);
            fb.fillRect(cx + 7, py + 7, 1, 1, 0xFFFF3A3A);
            break;
        }
        case EnemyType::Skeleton: {
            shadow(fb, cx, py + 15, 5, 2, 80);
            uint32_t bone = 0xFFE8E2D0, boneDark = 0xFFA8A090;
            fb.fillCircle(cx, py + 5, 3, bone);
            fb.fillRect(cx - 2, py + 4, 1, 1, 0xFF1A1A1A);
            fb.fillRect(cx + 1, py + 4, 1, 1, 0xFFFF6A3A);
            fb.fillRect(cx, py + 8, 1, 5, bone);
            fb.fillRect(cx - 3, py + 9, 7, 1, boneDark);
            fb.fillRect(cx - 2, py + 11, 5, 1, boneDark);
            fb.fillRect(cx - 3, py + 13, 2, 1, bone);
            fb.fillRect(cx + 2, py + 13, 2, 1, bone);
            fb.fillRect(cx - 5, py + 8, 2, 1, boneDark);
            fb.fillRect(cx + 4, py + 8, 2, 1, boneDark);
            break;
        }
        default: {
            shadow(fb, cx, py + 15, 6, 2, 80);
            fb.fillEllipse(cx, py + 9, 6, 5, 0xFF241A38);
            fb.fillCircle(cx, py + 5, 3, 0xFF3A2048);
            fb.fillRect(cx - 2, py + 5, 1, 1, 0xFFFF4A6A);
            fb.fillRect(cx + 1, py + 5, 1, 1, 0xFFFF4A6A);
            break;
        }
    }
}

}  // namespace art
}  // namespace rpg
