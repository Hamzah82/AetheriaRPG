// platform.cpp - X11 window, keyboard, nearest-neighbour presentation
#include "platform.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

namespace rpg {

namespace {

constexpr long kStateRemove = 0;
constexpr long kStateAdd = 1;

bool wmSupportsFullscreen(Display* dpy, int screen) {
    Atom netSupported = XInternAtom(dpy, "_NET_SUPPORTED", True);
    Atom fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", True);
    if (!netSupported || !fullscreen) return false;

    Atom actualType = 0;
    int actualFormat = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy, RootWindow(dpy, screen), netSupported, 0, 4096, False,
                           XA_ATOM, &actualType, &actualFormat, &count, &after,
                           &data) != Success ||
        !data) {
        return false;
    }
    bool supported = false;
    if (actualFormat == 32 && actualType == XA_ATOM) {
        const Atom* atoms = reinterpret_cast<const Atom*>(data);
        for (unsigned long i = 0; i < count; ++i) {
            if (atoms[i] == fullscreen) {
                supported = true;
                break;
            }
        }
    }
    XFree(data);
    return supported;
}

}  // namespace

struct Platform::Impl {
    Display* dpy = nullptr;
    Window win = 0;
    GC gc = nullptr;
    XImage* img = nullptr;
    int scale = 3;
    int winW = 0, winH = 0;
    int screenW = 0, screenH = 0;
    int screen = 0;
    Visual* visual = nullptr;
    int depth = 0;
    bool openFlag = false;
    bool fullscreen = false;
    bool fullscreenChanged = false;
    bool focused = true;
    bool wmFullscreen = false;
    bool held[KEY_COUNT] = {};
    ColorChannel red, green, blue;
    std::chrono::steady_clock::time_point lastFrame;
    Atom wmDelete = 0;
    Atom wmState = 0;
    Atom fullscreenAtom = 0;

    void destroyImage() {
        if (!img) return;
        if (img->data) std::free(img->data);
        img->data = nullptr;
        XDestroyImage(img);
        img = nullptr;
    }

    bool createImage(int width, int height, std::string* err) {
        destroyImage();
        if (width < 1 || height < 1) return false;
        img = XCreateImage(dpy, visual, depth, ZPixmap, 0, nullptr, width, height, 32, 0);
        if (!img) {
            if (err) *err = "Gagal membuat buffer gambar.";
            return false;
        }
        img->data = static_cast<char*>(
            std::calloc(static_cast<size_t>(img->bytes_per_line) * height, 1));
        if (!img->data) {
            if (err) *err = "Kehabisan memori untuk buffer gambar.";
            XDestroyImage(img);
            img = nullptr;
            return false;
        }
        return true;
    }

    void useWindowedHints() {
        XSizeHints hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.flags = PMinSize | PMaxSize;
        hints.min_width = hints.max_width = VIEW_W * scale;
        hints.min_height = hints.max_height = VIEW_H * scale;
        XSetWMNormalHints(dpy, win, &hints);
    }

    // A fullscreen window must be freely resizable by the window manager.
    void clearSizeHints() {
        XSizeHints hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.flags = 0;
        XSetWMNormalHints(dpy, win, &hints);
    }

    void sendFullscreenState(long action) {
        if (!wmState || !fullscreenAtom) return;
        XEvent ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.type = ClientMessage;
        ev.xclient.window = win;
        ev.xclient.message_type = wmState;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = action;
        ev.xclient.data.l[1] = static_cast<long>(fullscreenAtom);
        ev.xclient.data.l[2] = 0;
        ev.xclient.data.l[3] = 1;
        XSendEvent(dpy, RootWindow(dpy, screen), False,
                   SubstructureNotifyMask | SubstructureRedirectMask, &ev);
        XFlush(dpy);
    }

    void setWindowIcon() {
        if (!dpy || !win) return;
        std::vector<unsigned long> data;
        const int sizes[] = {32, 64};
        for (int size : sizes) {
            std::vector<uint32_t> pixels = buildWindowIcon(size);
            data.push_back(static_cast<unsigned long>(size));
            data.push_back(static_cast<unsigned long>(size));
            for (uint32_t p : pixels) data.push_back(static_cast<unsigned long>(p));
        }
        Atom icon = XInternAtom(dpy, "_NET_WM_ICON", False);
        if (!icon) return;
        XChangeProperty(dpy, win, icon, XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(data.data()),
                        static_cast<int>(data.size()));
    }

    void setFullscreen(bool on) {
        if (!openFlag || fullscreen == on) return;
        fullscreen = on;
        fullscreenChanged = true;
        if (on) {
            clearSizeHints();
            if (wmFullscreen) {
                sendFullscreenState(kStateAdd);
            } else {
                // No EWMH support (for example a bare X server): fill the screen
                // directly. The window keeps its decorations in this fallback.
                XMoveWindow(dpy, win, 0, 0);
                XResizeWindow(dpy, win, screenW, screenH);
            }
        } else {
            useWindowedHints();
            if (wmFullscreen) sendFullscreenState(kStateRemove);
            XResizeWindow(dpy, win, VIEW_W * scale, VIEW_H * scale);
            XMoveWindow(dpy, win, std::max(0, (screenW - VIEW_W * scale) / 2),
                        std::max(0, (screenH - VIEW_H * scale) / 2));
        }
        XFlush(dpy);
    }
};

Platform::Platform() : impl_(new Impl()) {}

Platform::~Platform() { close(); }

bool Platform::open(int scale, const std::string& title, std::string* err,
                    bool fullscreen) {
    close();
    Impl& d = *impl_;
    d.scale = clampInt(scale, 1, 6);
    d.dpy = XOpenDisplay(nullptr);
    if (!d.dpy) {
        if (err) *err = "Tidak bisa membuka display X11. Jalankan di sesi grafis, atau pakai --headless.";
        return false;
    }
    d.screen = DefaultScreen(d.dpy);
    d.visual = DefaultVisual(d.dpy, d.screen);
    d.depth = DefaultDepth(d.dpy, d.screen);
    d.screenW = DisplayWidth(d.dpy, d.screen);
    d.screenH = DisplayHeight(d.dpy, d.screen);
    d.winW = VIEW_W * d.scale;
    d.winH = VIEW_H * d.scale;
    d.fullscreen = false;
    d.wmFullscreen = wmSupportsFullscreen(d.dpy, d.screen);

    d.win = XCreateSimpleWindow(d.dpy, RootWindow(d.dpy, d.screen), 0, 0, d.winW, d.winH, 1,
                                BlackPixel(d.dpy, d.screen), BlackPixel(d.dpy, d.screen));
    if (!d.win) {
        if (err) *err = "Gagal membuat jendela.";
        close();
        return false;
    }
    XStoreName(d.dpy, d.win, title.c_str());
    XClassHint classHint;
    classHint.res_name = const_cast<char*>("aetheria");
    classHint.res_class = const_cast<char*>("Aetheria");
    XSetClassHint(d.dpy, d.win, &classHint);
    d.setWindowIcon();
    d.useWindowedHints();

    d.wmDelete = XInternAtom(d.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d.dpy, d.win, &d.wmDelete, 1);
    d.wmState = XInternAtom(d.dpy, "_NET_WM_STATE", False);
    d.fullscreenAtom = XInternAtom(d.dpy, "_NET_WM_STATE_FULLSCREEN", False);
    XSelectInput(d.dpy, d.win,
                 KeyPressMask | KeyReleaseMask | StructureNotifyMask | ExposureMask |
                     FocusChangeMask);

    Bool supported = False;
    XkbSetDetectableAutoRepeat(d.dpy, True, &supported);
    (void)supported;

    d.gc = XCreateGC(d.dpy, d.win, 0, nullptr);
    XSetForeground(d.dpy, d.gc, BlackPixel(d.dpy, d.screen));

    if (!d.createImage(d.winW, d.winH, err)) {
        close();
        return false;
    }
    d.red = colorChannelFromMask(d.visual->red_mask);
    d.green = colorChannelFromMask(d.visual->green_mask);
    d.blue = colorChannelFromMask(d.visual->blue_mask);

    if (fullscreen && d.wmFullscreen) {
        // Set the initial state before mapping so the window appears fullscreen
        // without an intermediate windowed frame.
        d.fullscreen = true;
        d.clearSizeHints();
        Atom state = d.fullscreenAtom;
        XChangeProperty(d.dpy, d.win, d.wmState, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&state), 1);
    }

    XMapWindow(d.dpy, d.win);
    XFlush(d.dpy);
    d.openFlag = true;
    if (fullscreen) {
        // Some window managers need the request again after the window is
        // mapped; the state property only covers the initial appearance.
        if (d.wmFullscreen) {
            d.sendFullscreenState(kStateAdd);
        } else {
            d.setFullscreen(true);
        }
    }
    d.lastFrame = std::chrono::steady_clock::now();
    d.focused = true;
    d.fullscreenChanged = false;
    for (int i = 0; i < KEY_COUNT; ++i) d.held[i] = false;
    return true;
}

void Platform::close() {
    Impl& d = *impl_;
    d.destroyImage();
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
    d.fullscreen = false;
    d.fullscreenChanged = false;
    d.focused = true;
    d.wmFullscreen = false;
    d.winW = 0;
    d.winH = 0;
    d.screenW = 0;
    d.screenH = 0;
    d.screen = 0;
    d.visual = nullptr;
    d.depth = 0;
    d.wmDelete = 0;
    d.wmState = 0;
    d.fullscreenAtom = 0;
}

bool Platform::isOpen() const { return impl_->openFlag; }
bool Platform::isFullscreen() const { return impl_->fullscreen; }
bool Platform::focused() const { return impl_->focused; }
bool Platform::takeFullscreenChanged() {
    bool changed = impl_->fullscreenChanged;
    impl_->fullscreenChanged = false;
    return changed;
}
int Platform::renderWidth() const { return impl_->winW; }
int Platform::renderHeight() const { return impl_->winH; }

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
        } else if (ev.type == FocusIn) {
            d.focused = true;
        } else if (ev.type == FocusOut) {
            d.focused = false;
            for (int i = 0; i < KEY_COUNT; ++i) d.held[i] = false;
        } else if (ev.type == ConfigureNotify) {
            if (ev.xconfigure.window != d.win) continue;
            int width = ev.xconfigure.width;
            int height = ev.xconfigure.height;
            if (width < 1 || height < 1) continue;
            if (width != d.winW || height != d.winH) {
                d.winW = width;
                d.winH = height;
                XClearWindow(d.dpy, d.win);
            }
        } else if (ev.type == KeyPress || ev.type == KeyRelease) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            // F11 or Alt+Enter toggles fullscreen. Consume the press so Enter
            // does not also register as a normal confirmation.
            if (ev.type == KeyPress &&
                (ks == XK_F11 || (ks == XK_Return && (ev.xkey.state & Mod1Mask)))) {
                d.setFullscreen(!d.fullscreen);
                continue;
            }
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
    if (!d.openFlag) return;
    if (fb.w != VIEW_W || fb.h != VIEW_H) return;

    const Viewport vp = fitViewport(d.winW, d.winH);
    if (vp.width <= 0 || vp.height <= 0) return;
    if (!d.img || d.img->width != vp.width || d.img->height != vp.height) {
        if (!d.createImage(vp.width, vp.height, nullptr)) return;
    }

    const int srcCols = vp.width / vp.scale;
    for (int y = 0; y < vp.height; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(d.img->data +
                                                static_cast<size_t>(y) * d.img->bytes_per_line);
        const uint32_t* src = fb.px.data() + static_cast<size_t>(y / vp.scale) * fb.w;
        uint32_t* dst = row;
        for (int sx = 0; sx < srcCols; ++sx) {
            const uint32_t px = packPixel(src[sx], d.red, d.green, d.blue);
            for (int k = 0; k < vp.scale; ++k) *dst++ = px;
        }
    }

    // Window background is black; repaint the bars every frame so an expose
    // never leaves stale pixels, then blit the scaled image into the centre.
    if (vp.y > 0) XFillRectangle(d.dpy, d.win, d.gc, 0, 0, d.winW, vp.y);
    if (vp.y + vp.height < d.winH) {
        XFillRectangle(d.dpy, d.win, d.gc, 0, vp.y + vp.height, d.winW,
                       d.winH - (vp.y + vp.height));
    }
    if (vp.x > 0) XFillRectangle(d.dpy, d.win, d.gc, 0, vp.y, vp.x, vp.height);
    if (vp.x + vp.width < d.winW) {
        XFillRectangle(d.dpy, d.win, d.gc, vp.x + vp.width, vp.y,
                       d.winW - (vp.x + vp.width), vp.height);
    }
    XPutImage(d.dpy, d.win, d.gc, d.img, 0, 0, vp.x, vp.y, vp.width, vp.height);
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
