// platform.cpp - X11 window, keyboard, nearest-neighbour presentation
#include "platform.h"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace rpg {

struct Platform::Impl {
    Display* dpy = nullptr;
    Window win = 0;
    GC gc = nullptr;
    XImage* img = nullptr;
    int scale = 3;
    int ww = 0, hh = 0;
    bool openFlag = false;
    bool held[KEY_COUNT] = {};
    ColorChannel red, green, blue;
    std::chrono::steady_clock::time_point lastFrame;
    Atom wmDelete = 0;
};

Platform::Platform() : impl_(new Impl()) {}

Platform::~Platform() { close(); }

bool Platform::open(int scale, const std::string& title, std::string* err) {
    close();
    Impl& d = *impl_;
    d.scale = clampInt(scale, 1, 6);
    d.dpy = XOpenDisplay(nullptr);
    if (!d.dpy) {
        if (err) *err = "Tidak bisa membuka display X11. Jalankan di sesi grafis, atau pakai --headless.";
        return false;
    }
    int screen = DefaultScreen(d.dpy);
    Visual* visual = DefaultVisual(d.dpy, screen);
    int depth = DefaultDepth(d.dpy, screen);
    d.ww = VIEW_W * d.scale;
    d.hh = VIEW_H * d.scale;

    d.win = XCreateSimpleWindow(d.dpy, RootWindow(d.dpy, screen), 0, 0, d.ww, d.hh, 1,
                                BlackPixel(d.dpy, screen), BlackPixel(d.dpy, screen));
    if (!d.win) {
        if (err) *err = "Gagal membuat jendela.";
        close();
        return false;
    }
    XStoreName(d.dpy, d.win, title.c_str());

    XSizeHints hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = hints.max_width = d.ww;
    hints.min_height = hints.max_height = d.hh;
    XSetWMNormalHints(d.dpy, d.win, &hints);

    d.wmDelete = XInternAtom(d.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d.dpy, d.win, &d.wmDelete, 1);
    XSelectInput(d.dpy, d.win, KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask);

    Bool supported = False;
    XkbSetDetectableAutoRepeat(d.dpy, True, &supported);
    (void)supported;

    d.gc = XCreateGC(d.dpy, d.win, 0, nullptr);
    XSetForeground(d.dpy, d.gc, BlackPixel(d.dpy, screen));

    d.img = XCreateImage(d.dpy, visual, depth, ZPixmap, 0, nullptr, d.ww, d.hh, 32, 0);
    if (!d.img) {
        if (err) *err = "Gagal membuat buffer gambar.";
        close();
        return false;
    }
    d.img->data = static_cast<char*>(std::calloc(static_cast<size_t>(d.img->bytes_per_line) * d.hh, 1));
    if (!d.img->data) {
        if (err) *err = "Kehabisan memori untuk buffer gambar.";
        close();
        return false;
    }
    if (d.img->bits_per_pixel != 32) {
        if (err) *err = "Kedalaman warna layar tidak didukung (butuh 32-bit).";
        close();
        return false;
    }
    d.red = colorChannelFromMask(visual->red_mask);
    d.green = colorChannelFromMask(visual->green_mask);
    d.blue = colorChannelFromMask(visual->blue_mask);

    XMapWindow(d.dpy, d.win);
    XFlush(d.dpy);
    d.openFlag = true;
    d.lastFrame = std::chrono::steady_clock::now();
    for (int i = 0; i < KEY_COUNT; ++i) d.held[i] = false;
    return true;
}

void Platform::close() {
    Impl& d = *impl_;
    if (d.img) {
        if (d.img->data) std::free(d.img->data);
        d.img->data = nullptr;
        XDestroyImage(d.img);
        d.img = nullptr;
    }
    if (d.gc && d.dpy) {
        XFreeGC(d.dpy, d.gc);
        d.gc = nullptr;
    }
    if (d.dpy) {
        if (d.win) XDestroyWindow(d.dpy, d.win);
        XCloseDisplay(d.dpy);
        d.dpy = nullptr;
        d.win = 0;
    }
    d.openFlag = false;
}

bool Platform::isOpen() const { return impl_->openFlag; }
int Platform::renderWidth() const { return impl_->ww; }
int Platform::renderHeight() const { return impl_->hh; }

bool Platform::pump(InputState& in) {
    Impl& d = *impl_;
    if (!d.openFlag) return false;
    for (int i = 0; i < KEY_COUNT; ++i) in.pressed[i] = false;

    while (XPending(d.dpy) > 0) {
        XEvent ev;
        XNextEvent(d.dpy, &ev);
        if (ev.type == ClientMessage) {
            if (static_cast<Atom>(ev.xclient.data.l[0]) == d.wmDelete) return false;
        } else if (ev.type == DestroyNotify) {
            return false;
        } else if (ev.type == KeyPress || ev.type == KeyRelease) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            int key = -1;
            switch (ks) {
                case XK_Up: case XK_w: case XK_W: key = static_cast<int>(Key::Up); break;
                case XK_Down: case XK_s: case XK_S: key = static_cast<int>(Key::Down); break;
                case XK_Left: case XK_a: case XK_A: key = static_cast<int>(Key::Left); break;
                case XK_Right: case XK_d: case XK_D: key = static_cast<int>(Key::Right); break;
                case XK_Return: case XK_KP_Enter: case XK_z: case XK_Z: case XK_space:
                    key = static_cast<int>(Key::Confirm); break;
                case XK_Escape: case XK_x: case XK_X: case XK_BackSpace:
                    key = static_cast<int>(Key::Cancel); break;
                case XK_e: case XK_E: case XK_Tab:
                    key = static_cast<int>(Key::Menu); break;
                case XK_Shift_L: case XK_Shift_R:
                    key = static_cast<int>(Key::Run); break;
                default: break;
            }
            if (key >= 0) {
                if (ev.type == KeyPress) {
                    if (!d.held[key]) in.pressed[key] = true;
                    d.held[key] = true;
                } else {
                    d.held[key] = false;
                }
            }
        }
    }
    for (int i = 0; i < KEY_COUNT; ++i) in.held[i] = d.held[i];
    return true;
}

void Platform::present(const Framebuffer& fb) {
    Impl& d = *impl_;
    if (!d.openFlag || !d.img) return;
    if (fb.w != VIEW_W || fb.h != VIEW_H) return;
    for (int y = 0; y < d.hh; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(d.img->data + static_cast<size_t>(y) * d.img->bytes_per_line);
        const uint32_t* src = fb.px.data() + static_cast<size_t>(y / d.scale) * fb.w;
        for (int x = 0; x < d.ww; ++x) {
            row[x] = packPixel(src[x / d.scale], d.red, d.green, d.blue);
        }
    }
    XPutImage(d.dpy, d.win, d.gc, d.img, 0, 0, 0, 0, d.ww, d.hh);
    XFlush(d.dpy);
}

void Platform::frameDelay() {
    Impl& d = *impl_;
    using clock = std::chrono::steady_clock;
    auto frame = std::chrono::microseconds(16667);
    auto target = d.lastFrame + frame;
    auto now = clock::now();
    if (now < target) std::this_thread::sleep_for(target - now);
    d.lastFrame = clock::now();
}

}  // namespace rpg
