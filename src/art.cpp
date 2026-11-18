// art.cpp - all sprites are drawn procedurally, no external assets needed
#include "art.h"

#include <cmath>

namespace rpg {
namespace art {
namespace {

using namespace pal;

struct Look {
    uint32_t skin, hair, shirt, shirtDark, pants, boots, accent;
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
}

Look playerLook() {
    return {skin, hair, tunic, tunicDark, 0xFF23305A, 0xFF2A1C14, 0xFFE2C275};
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

void player(Framebuffer& fb, int px, int py, int dir, float phase, bool moving) {
    humanoid(fb, px, py, 1, dir, phase, moving, playerLook());
}

void playerScaled(Framebuffer& fb, int x, int y, int scale, int dir, float phase, bool moving) {
    humanoid(fb, x, y, scale, dir, phase, moving, playerLook());
}

void heroBack(Framebuffer& fb, int x, int y, int scale, float time) {
    Look L = playerLook();
    humanoid(fb, x, y, scale, 1, time * 0.0f, false, L);
}

void npc(Framebuffer& fb, int px, int py, NpcStyle style, int dir, float time) {
    Look L = npcLook(style);
    if (style == NpcStyle::Warden) {
        wardenWorld(fb, px, py, time);
        return;
    }
    humanoid(fb, px, py, 1, dir, 0.0f, false, L, style == NpcStyle::Kid,
             style == NpcStyle::Guard, style == NpcStyle::Innkeeper);

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
    }
}

}  // namespace art
}  // namespace rpg
