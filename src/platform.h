// platform.h - native window and input (X11)
#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "core.h"
#include "gfx.h"

namespace rpg {

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

    bool open(int scale, const std::string& title, std::string* err);
    void close();
    bool pump(InputState& in);
    void present(const Framebuffer& fb);
    void frameDelay();
    int renderWidth() const;
    int renderHeight() const;
    bool isOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rpg
