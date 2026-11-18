// gfx.cpp - rasterizer, 5x7 font, PNG encoder
#include "gfx.h"

#include <cstring>
#include <fstream>

namespace rpg {

namespace {

// Classic 5x7 font, column-major, bit0 = top row.
const uint8_t GLYPHS[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00}, {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00}, {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00}, {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06}, {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40}, {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x7F, 0x10, 0x28, 0x44, 0x00}, {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00}, {0x00, 0x00, 0x7F, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00}, {0x08, 0x04, 0x08, 0x10, 0x08}};

uint32_t crcTable[256];
bool crcReady = false;

void initCrc() {
    if (crcReady) return;
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crcTable[n] = c;
    }
    crcReady = true;
}

uint32_t crc32buf(const uint8_t* buf, size_t len) {
    initCrc();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = crcTable[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void put32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>(x >> 24));
    v.push_back(static_cast<uint8_t>(x >> 16));
    v.push_back(static_cast<uint8_t>(x >> 8));
    v.push_back(static_cast<uint8_t>(x));
}

void putChunk(std::vector<uint8_t>& png, const char* type, const std::vector<uint8_t>& data) {
    put32(png, static_cast<uint32_t>(data.size()));
    size_t start = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    uint32_t c = crc32buf(png.data() + start, data.size() + 4);
    put32(png, c);
}

}  // namespace

void Framebuffer::fillRect(int x, int y, int rw, int rh, uint32_t c) {
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(w, x + rw), y1 = std::min(h, y + rh);
    c |= 0xFF000000u;
    for (int yy = y0; yy < y1; ++yy) {
        auto* row = px.data() + static_cast<size_t>(yy) * w;
        for (int xx = x0; xx < x1; ++xx) row[xx] = c;
    }
}

void Framebuffer::fillRectA(int x, int y, int rw, int rh, uint32_t c, int alpha) {
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(w, x + rw), y1 = std::min(h, y + rh);
    for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx)
            px[static_cast<size_t>(yy) * w + xx] =
                blend(px[static_cast<size_t>(yy) * w + xx], withAlpha(c, alpha));
}

void Framebuffer::outline(int x, int y, int rw, int rh, uint32_t c) {
    hline(x, y, rw, c);
    hline(x, y + rh - 1, rw, c);
    vline(x, y, rh, c);
    vline(x + rw - 1, y, rh, c);
}

void Framebuffer::hline(int x, int y, int len, uint32_t c) {
    if (y < 0 || y >= h) return;
    int x0 = std::max(0, x), x1 = std::min(w, x + len);
    for (int xx = x0; xx < x1; ++xx) px[static_cast<size_t>(y) * w + xx] = c | 0xFF000000u;
}

void Framebuffer::vline(int x, int y, int len, uint32_t c) {
    if (x < 0 || x >= w) return;
    int y0 = std::max(0, y), y1 = std::min(h, y + len);
    for (int yy = y0; yy < y1; ++yy) px[static_cast<size_t>(yy) * w + x] = c | 0xFF000000u;
}

void Framebuffer::line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Framebuffer::fillCircle(int cx, int cy, int r, uint32_t c) {
    for (int y = -r; y <= r; ++y) {
        int span = static_cast<int>(std::sqrt(static_cast<float>(r * r - y * y)));
        hline(cx - span, cy + y, span * 2 + 1, c);
    }
}

void Framebuffer::fillEllipse(int cx, int cy, int rx, int ry, uint32_t c) {
    if (rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(ry);
        int span = static_cast<int>(rx * std::sqrt(std::max(0.0f, 1.0f - t * t)));
        hline(cx - span, cy + y, span * 2 + 1, c);
    }
}

void Framebuffer::fillEllipseA(int cx, int cy, int rx, int ry, uint32_t c, int A) {
    if (rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(ry);
        int span = static_cast<int>(rx * std::sqrt(std::max(0.0f, 1.0f - t * t)));
        int yy = cy + y;
        if (yy < 0 || yy >= h) continue;
        int x0 = std::max(0, cx - span), x1 = std::min(w, cx + span + 1);
        for (int xx = x0; xx < x1; ++xx)
            px[static_cast<size_t>(yy) * w + xx] =
                blend(px[static_cast<size_t>(yy) * w + xx], withAlpha(c, A));
    }
}

int Framebuffer::textWidth(const std::string& s, int scale) const {
    int best = 0, cur = 0;
    for (char ch : s) {
        if (ch == '\n') {
            best = std::max(best, cur);
            cur = 0;
        } else {
            cur += 6 * scale;
        }
    }
    return std::max(best, cur);
}

void Framebuffer::text(int x, int y, const std::string& s, uint32_t c, int scale) {
    if (scale < 1) scale = 1;
    int cx = x, cy = y;
    for (char raw : s) {
        unsigned char ch = static_cast<unsigned char>(raw);
        if (ch == '\n') {
            cx = x;
            cy += 8 * scale;
            continue;
        }
        if (ch < 32 || ch > 126) ch = '?';
        const uint8_t* g = GLYPHS[ch - 32];
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; ++row) {
                if (bits & (1u << row))
                    fillRect(cx + col * scale, cy + row * scale, scale, scale, c);
            }
        }
        cx += 6 * scale;
    }
}

void Framebuffer::textShadow(int x, int y, const std::string& s, uint32_t c, uint32_t sh, int scale) {
    text(x + scale, y + scale, s, sh, scale);
    text(x, y, s, c, scale);
}

std::vector<uint8_t> Framebuffer::toPNG() const {
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(h) * (1 + static_cast<size_t>(w) * 3));
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);  // filter type none
        for (int x = 0; x < w; ++x) {
            uint32_t p = px[static_cast<size_t>(y) * w + x];
            raw.push_back(static_cast<uint8_t>(p >> 16));
            raw.push_back(static_cast<uint8_t>(p >> 8));
            raw.push_back(static_cast<uint8_t>(p));
        }
    }

    std::vector<uint8_t> z;
    z.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    z.push_back(0x78);
    z.push_back(0x01);
    size_t pos = 0;
    if (raw.empty()) {
        z.push_back(0x01);
        z.push_back(0x00);
        z.push_back(0x00);
        z.push_back(0xFF);
        z.push_back(0xFF);
    }
    while (pos < raw.size()) {
        size_t n = std::min<size_t>(65535, raw.size() - pos);
        bool final = (pos + n >= raw.size());
        z.push_back(final ? 0x01 : 0x00);
        z.push_back(static_cast<uint8_t>(n & 0xFF));
        z.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
        z.push_back(static_cast<uint8_t>((~n) & 0xFF));
        z.push_back(static_cast<uint8_t>(((~n) >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<long>(pos), raw.begin() + static_cast<long>(pos + n));
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    put32(z, (b << 16) | a);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    put32(ihdr, static_cast<uint32_t>(w));
    put32(ihdr, static_cast<uint32_t>(h));
    ihdr.push_back(8);
    ihdr.push_back(2);  // truecolor RGB
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    putChunk(png, "IHDR", ihdr);
    putChunk(png, "IDAT", z);
    putChunk(png, "IEND", {});
    return png;
}

bool Framebuffer::writePNG(const std::string& path) const {
    auto data = toPNG();
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(f);
}

std::vector<std::string> wrapText(const std::string& text, int maxWidthPx, int scale) {
    std::vector<std::string> lines;
    std::string line;
    size_t i = 0;
    while (i <= text.size()) {
        size_t sp = text.find(' ', i);
        std::string word = text.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
        if (word == "\n") {
            lines.push_back(line);
            line.clear();
        } else {
            std::string cand = line.empty() ? word : line + " " + word;
            int width = static_cast<int>(cand.size()) * 6 * scale;
            if (width > maxWidthPx && !line.empty()) {
                lines.push_back(line);
                line = word;
            } else {
                line = cand;
            }
        }
        if (sp == std::string::npos) break;
        i = sp + 1;
    }
    if (!line.empty() || lines.empty()) lines.push_back(line);
    return lines;
}

}  // namespace rpg
