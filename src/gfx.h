// gfx.h - software framebuffer, pixel font, PNG export
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core.h"

namespace rpg {

struct Framebuffer {
    int w = 0, h = 0;
    std::vector<uint32_t> px;

    void resize(int nw, int nh) {
        w = nw;
        h = nh;
        px.assign(static_cast<size_t>(nw) * nh, pal::black);
    }
    void clear(uint32_t c) { std::fill(px.begin(), px.end(), c | 0xFF000000u); }

    void pixel(int x, int y, uint32_t c) {
        if (static_cast<unsigned>(x) < static_cast<unsigned>(w) &&
            static_cast<unsigned>(y) < static_cast<unsigned>(h))
            px[static_cast<size_t>(y) * w + x] = c | 0xFF000000u;
    }
    void blendPixel(int x, int y, uint32_t c, int a = -1) {
        if (static_cast<unsigned>(x) >= static_cast<unsigned>(w) ||
            static_cast<unsigned>(y) >= static_cast<unsigned>(h))
            return;
        if (a < 0) a = alphaOf(c);
        auto& d = px[static_cast<size_t>(y) * w + x];
        d = blend(d, withAlpha(c, a));
    }
    void fillRect(int x, int y, int rw, int rh, uint32_t c);
    void fillRectA(int x, int y, int rw, int rh, uint32_t c, int alpha);
    void outline(int x, int y, int rw, int rh, uint32_t c);
    void hline(int x, int y, int len, uint32_t c);
    void vline(int x, int y, int len, uint32_t c);
    void line(int x0, int y0, int x1, int y1, uint32_t c);
    void fillCircle(int cx, int cy, int r, uint32_t c);
    void fillEllipse(int cx, int cy, int rx, int ry, uint32_t c);
    void fillEllipseA(int cx, int cy, int rx, int ry, uint32_t c, int alpha);

    int textWidth(const std::string& s, int scale = 1) const;
    int textHeight(int scale = 1) const { return 7 * scale; }
    void text(int x, int y, const std::string& s, uint32_t c, int scale = 1);
    void textShadow(int x, int y, const std::string& s, uint32_t c, uint32_t shadow, int scale = 1);

    std::vector<uint8_t> toPNG() const;
    bool writePNG(const std::string& path) const;
};

// Word wrap helper: splits text into lines that fit maxWidth pixels.
std::vector<std::string> wrapText(const std::string& text, int maxWidthPx, int scale = 1);

}  // namespace rpg
