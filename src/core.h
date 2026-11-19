// core.h - fundamental types, math, RNG, input, palette
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace rpg {

constexpr int TILE = 16;
constexpr int VIEW_W = 320;
constexpr int VIEW_H = 240;
constexpr int VIEW_TX = VIEW_W / TILE;
constexpr int VIEW_TY = VIEW_H / TILE;

inline int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float approach(float cur, float target, float delta) {
    if (cur < target) return std::min(cur + delta, target);
    return std::max(cur - delta, target);
}
inline int signf(float v) { return (v > 0.0f) - (v < 0.0f); }
inline float distancef(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    return std::sqrt(dx * dx + dy * dy);
}

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float nx, float ny) : x(nx), y(ny) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    bool overlaps(const Rect& o) const {
        return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
    }
};

// Deterministic xorshift64* RNG. Same seed => same sequence.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) { s = seed ? seed : 0x123456789ABCDEFull; }
    uint32_t next() {
        uint64_t x = s;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        s = x;
        return static_cast<uint32_t>((x * 2685821657736338717ull) >> 32);
    }
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }
    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
    bool chance(float p) { return unit() < p; }
};

inline uint32_t rgba(int r, int g, int b, int a = 255) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r & 255) << 16) |
           (static_cast<uint32_t>(g & 255) << 8) | static_cast<uint32_t>(b & 255);
}
inline int alphaOf(uint32_t c) { return static_cast<int>(c >> 24); }
inline uint32_t withAlpha(uint32_t c, int a) {
    return (c & 0x00FFFFFFu) | (static_cast<uint32_t>(clampInt(a, 0, 255)) << 24);
}
inline uint32_t blend(uint32_t dst, uint32_t src) {
    int a = alphaOf(src);
    if (a >= 255) return src | 0xFF000000u;
    if (a <= 0) return dst;
    int dr = (dst >> 16) & 255, dg = (dst >> 8) & 255, db = dst & 255;
    int sr = (src >> 16) & 255, sg = (src >> 8) & 255, sb = src & 255;
    int r = (sr * a + dr * (255 - a)) / 255;
    int g = (sg * a + dg * (255 - a)) / 255;
    int b = (sb * a + db * (255 - a)) / 255;
    return 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}
inline uint32_t mix(uint32_t a, uint32_t b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return rgba(static_cast<int>(ar + (br - ar) * t), static_cast<int>(ag + (bg - ag) * t),
                static_cast<int>(ab + (bb - ab) * t));
}
inline uint32_t shade(uint32_t c, float f) {
    int r = static_cast<int>(((c >> 16) & 255) * f);
    int g = static_cast<int>(((c >> 8) & 255) * f);
    int b = static_cast<int>((c & 255) * f);
    return rgba(clampInt(r, 0, 255), clampInt(g, 0, 255), clampInt(b, 0, 255));
}
inline uint32_t hash2(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

enum class Key { Up, Down, Left, Right, Confirm, Cancel, Menu, Run, Count };
constexpr int KEY_COUNT = static_cast<int>(Key::Count);

struct InputState {
    bool held[KEY_COUNT] = {};
    bool pressed[KEY_COUNT] = {};
    bool down(Key k) const { return held[static_cast<int>(k)]; }
    bool hit(Key k) const { return pressed[static_cast<int>(k)]; }
    void clearPressed() {
        for (int i = 0; i < KEY_COUNT; ++i) pressed[i] = false;
    }
};

// Cozy 16-bit style palette.
namespace pal {
constexpr uint32_t black = 0xFF000000u;
constexpr uint32_t white = 0xFFFFFFFFu;
// Nusakalpa palette: pale salt flats, ash, brine, and petrified coral.
constexpr uint32_t grass = 0xFFB4B8A4u;
constexpr uint32_t grassDark = 0xFF969A86u;
constexpr uint32_t grassLight = 0xFFCCD0BCu;
constexpr uint32_t path = 0xFFC6BCA2u;
constexpr uint32_t pathDark = 0xFFA89E86u;
constexpr uint32_t pathLight = 0xFFDED4BAu;
constexpr uint32_t sand = 0xFFE6DEC4u;
constexpr uint32_t sandDark = 0xFFC8BEA6u;
constexpr uint32_t waterDeep = 0xFF244850u;
constexpr uint32_t water = 0xFF3A6A72u;
constexpr uint32_t waterLight = 0xFF7AB8C0u;
constexpr uint32_t foam = 0xFFB8D8DCu;
constexpr uint32_t trunk = 0xFF8A7A6Au;
constexpr uint32_t leafDark = 0xFF8A8478u;
constexpr uint32_t leaf = 0xFFB0A898u;
constexpr uint32_t leafLight = 0xFFCCC4B4u;
constexpr uint32_t roof = 0xFF4A4A58u;
constexpr uint32_t roofDark = 0xFF34343Eu;
constexpr uint32_t roofLight = 0xFF666674u;
constexpr uint32_t wall = 0xFFE8E4D8u;
constexpr uint32_t wallDark = 0xFFB8B4A8u;
constexpr uint32_t beam = 0xFF7A6A52u;
constexpr uint32_t door = 0xFF6A5238u;
constexpr uint32_t doorDark = 0xFF4A3826u;
constexpr uint32_t stone = 0xFFC0C4CCu;
constexpr uint32_t stoneDark = 0xFF8A8E98u;
constexpr uint32_t caveWall = 0xFF3A3648u;
constexpr uint32_t caveWallDark = 0xFF2A2735u;
constexpr uint32_t caveFloor = 0xFF565064u;
constexpr uint32_t caveFloorDark = 0xFF443E52u;
constexpr uint32_t lava = 0xFFE05A1Au;
constexpr uint32_t lavaLight = 0xFFFFA23Eu;
constexpr uint32_t uiPanel = 0xFF141A2Eu;
constexpr uint32_t uiPanelLight = 0xFF232B46u;
constexpr uint32_t uiBorder = 0xFFE2C275u;
constexpr uint32_t uiText = 0xFFF6F0DCu;
constexpr uint32_t uiTextDim = 0xFFB8B4C8u;
constexpr uint32_t hp = 0xFFE04A4Au;
constexpr uint32_t hpLight = 0xFFFF8080u;
constexpr uint32_t hpDark = 0xFF8E2626u;
constexpr uint32_t mp = 0xFF4A8FE0u;
constexpr uint32_t mpDark = 0xFF2A4F8Eu;
constexpr uint32_t xp = 0xFFE2C275u;
constexpr uint32_t gold = 0xFFF5C542u;
constexpr uint32_t skin = 0xFFF0C89Au;
constexpr uint32_t skinDark = 0xFFC99A6Eu;
constexpr uint32_t hair = 0xFF6B4326u;
constexpr uint32_t hairDark = 0xFF523218u;
constexpr uint32_t tunic = 0xFF3C5A8Au;
constexpr uint32_t tunicDark = 0xFF2A4066u;
constexpr uint32_t cloth = 0xFFB8B4C8u;
}  // namespace pal

}  // namespace rpg
