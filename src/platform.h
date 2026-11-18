// platform.h - native window and input (X11)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core.h"
#include "gfx.h"

namespace rpg {

// Placement of the 320x240 framebuffer inside a window. Fullscreen letterboxes
// or pillarboxes with the largest integer scale that fits, so pixels stay sharp.
struct Viewport {
    int scale = 1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline Viewport fitViewport(int windowWidth, int windowHeight) {
    Viewport v;
    if (windowWidth <= 0 || windowHeight <= 0) return v;
    int fit = std::min(windowWidth / VIEW_W, windowHeight / VIEW_H);
    if (fit < 1) fit = 1;
    if (fit > 6) fit = 6;  // match --scale's limit and bound 4K frame costs
    v.scale = fit;
    v.width = std::min(VIEW_W * fit, windowWidth);
    v.height = std::min(VIEW_H * fit, windowHeight);
    v.x = (windowWidth - v.width) / 2;
    v.y = (windowHeight - v.height) / 2;
    return v;
}

// Mapping of one display channel (for example red) inside a native pixel.
struct ColorChannel {
    int shift = 0;
    int bits = 8;
};

// Derive shift and width from an X11 color mask such as 0x00FF0000.
inline ColorChannel colorChannelFromMask(unsigned long mask) {
    ColorChannel c;
    if (mask == 0) return c;
    while ((mask & 1UL) == 0UL) {
        mask >>= 1;
        ++c.shift;
    }
    int bits = 0;
    while (mask & 1UL) {
        mask >>= 1;
        ++bits;
    }
    c.bits = bits > 0 ? bits : 8;
    return c;
}

// Deterministic ARGB pixels for _NET_WM_ICON: a golden sun over dark hills.
// Kept independent from X11 so it can be unit tested headlessly.
inline std::vector<uint32_t> buildWindowIcon(int size) {
    auto mix = [](uint32_t a, uint32_t b, float t) {
        t = clampf(t, 0.0f, 1.0f);
        int ar = static_cast<int>((a >> 16) & 255), ag = static_cast<int>((a >> 8) & 255),
            ab = static_cast<int>(a & 255);
        int br = static_cast<int>((b >> 16) & 255), bg = static_cast<int>((b >> 8) & 255),
            bb = static_cast<int>(b & 255);
        int r = ar + static_cast<int>((br - ar) * t);
        int g = ag + static_cast<int>((bg - ag) * t);
        int bl = ab + static_cast<int>((bb - ab) * t);
        return 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(bl);
    };
    if (size < 8) size = 8;
    std::vector<uint32_t> px(static_cast<size_t>(size) * size, 0xFF0A0C1E);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float u = (x + 0.5f) / static_cast<float>(size);
            float v = (y + 0.5f) / static_cast<float>(size);
            uint32_t c = mix(0xFF0A0C1E, 0xFF3A2A5A, v * 0.85f);

            // Stars in the upper half.
            float star = static_cast<float>(hash2(x / 2, y / 2) % 97) / 97.0f;
            if (v < 0.55f && star > 0.94f) c = mix(c, 0xFFFFFFFF, 0.55f);

            // Golden sun with a bright core.
            float dx = u - 0.70f, dy = v - 0.28f;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < 0.20f) c = mix(0xFFFFD87A, 0xFFFFF2C0, 1.0f - d / 0.20f);
            if (d < 0.10f) c = 0xFFFFF2C0;

            // Layered hills, with a darker foreground.
            float backHill = 0.63f + 0.075f * std::sin(u * 7.0f + 0.6f);
            float frontHill = 0.76f + 0.055f * std::sin(u * 5.0f + 2.2f);
            if (v > backHill) c = mix(c, 0xFF1E3A2A, 0.90f);
            if (v > frontHill) c = mix(c, 0xFF10241C, 0.92f);

            px[static_cast<size_t>(y) * size + x] = c | 0xFF000000u;
        }
    }
    return px;
}

// Scale an 8-bit color component into the native channel width.
inline int channelValue(int value, const ColorChannel& c) {
    if (c.bits >= 8) return value << (c.bits - 8);
    return value >> (8 - c.bits);
}

// Pack a 0xRRGGBB color into a native pixel using the display masks.
inline uint32_t packPixel(uint32_t rgb, const ColorChannel& red, const ColorChannel& green,
                          const ColorChannel& blue) {
    auto place = [](int value, const ColorChannel& c) -> uint32_t {
        uint32_t mask = c.bits >= 32 ? 0xffffffffu : ((1u << c.bits) - 1u);
        return (static_cast<uint32_t>(channelValue(value, c)) & mask) << c.shift;
    };
    return place(static_cast<int>((rgb >> 16) & 255), red) |
           place(static_cast<int>((rgb >> 8) & 255), green) |
           place(static_cast<int>(rgb & 255), blue);
}

class Platform {
public:
    Platform();
    ~Platform();

    bool open(int scale, const std::string& title, std::string* err,
              bool fullscreen = false);
    void close();
    bool pump(InputState& in);
    void present(const Framebuffer& fb);
    void frameDelay();
    int renderWidth() const;
    int renderHeight() const;
    bool isOpen() const;
    bool isFullscreen() const;
    bool focused() const;

    // True once after the player toggles fullscreen, so main can persist the
    // new preference immediately instead of waiting for a clean exit.
    bool takeFullscreenChanged();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rpg
