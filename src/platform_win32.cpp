// platform_win32.cpp - Win32 window, keyboard, nearest-neighbour presentation
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "platform.h"

namespace rpg {

namespace {

constexpr const char* kWindowClass = "PelitaSegaraGaramWindow";

// Windows wants a packed BGRA pixel for a 32-bit DIB section.
inline uint32_t toBGRA(uint32_t argb) {
    return ((argb & 0xFFu) << 16) | (argb & 0xFF00u) | ((argb >> 16) & 0xFFu) | 0xFF000000u;
}

HICON buildWin32Icon(int size) {
    std::vector<uint32_t> pixels = buildWindowIcon(size);
    if (pixels.empty()) return nullptr;

    BITMAPV5HEADER header;
    std::memset(&header, 0, sizeof(header));
    header.bV5Size = sizeof(header);
    header.bV5Width = size;
    header.bV5Height = -size;  // top-down rows match buildWindowIcon
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00FF0000u;
    header.bV5GreenMask = 0x0000FF00u;
    header.bV5BlueMask = 0x000000FFu;
    header.bV5AlphaMask = 0xFF000000u;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header),
                                     DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color || !bits) {
        if (color) DeleteObject(color);
        return nullptr;
    }
    uint32_t* dst = static_cast<uint32_t*>(bits);
    for (size_t i = 0; i < pixels.size(); ++i) dst[i] = toBGRA(pixels[i]);

    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    ICONINFO info;
    std::memset(&info, 0, sizeof(info));
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(color);
    if (mask) DeleteObject(mask);
    return icon;
}

}  // namespace

struct Platform::Impl {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    int scale = 3;
    int winW = VIEW_W * 3;
    int winH = VIEW_H * 3;
    bool openFlag = false;
    bool closed = false;
    bool fullscreen = false;
    bool fullscreenChanged = false;
    bool focused = true;
    bool timerActive = false;
    LONG_PTR windowedStyle = 0;
    RECT windowedRect = {};
    bool held[KEY_COUNT] = {};
    bool pressedNext[KEY_COUNT] = {};
    HICON bigIcon = nullptr;
    HICON smallIcon = nullptr;
    std::vector<uint32_t> surface;
    Viewport viewport;
    std::chrono::steady_clock::time_point lastFrame;

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void clearHeld() {
        for (int i = 0; i < KEY_COUNT; ++i) held[i] = false;
    }

    void handleKey(int vk, bool down) {
        if (vk == VK_F11) {
            if (down) setFullscreen(!fullscreen);
            return;
        }
        if (vk == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000) != 0) {
            if (down) setFullscreen(!fullscreen);
            return;
        }

        int key = -1;
        switch (vk) {
            case VK_UP: case 'W': key = static_cast<int>(Key::Up); break;
            case VK_DOWN: case 'S': key = static_cast<int>(Key::Down); break;
            case VK_LEFT: case 'A': key = static_cast<int>(Key::Left); break;
            case VK_RIGHT: case 'D': key = static_cast<int>(Key::Right); break;
            case VK_RETURN: case VK_SPACE: case 'Z':
                key = static_cast<int>(Key::Confirm); break;
            case VK_ESCAPE: case VK_BACK: case 'X':
                key = static_cast<int>(Key::Cancel); break;
            case 'E': case VK_TAB: key = static_cast<int>(Key::Menu); break;
            case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
                key = static_cast<int>(Key::Run); break;
            default: break;
        }
        if (key < 0) return;
        if (down) {
            if (!held[key]) pressedNext[key] = true;
            held[key] = true;
        } else {
            held[key] = false;
        }
    }

    void setFullscreen(bool on) {
        if (!window || closed || fullscreen == on) return;
        fullscreen = on;
        fullscreenChanged = true;
        if (on) {
            windowedStyle = GetWindowLongPtrA(window, GWL_STYLE);
            GetWindowRect(window, &windowedRect);
            MONITORINFO monitor;
            std::memset(&monitor, 0, sizeof(monitor));
            monitor.cbSize = sizeof(monitor);
            HMONITOR current = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
            if (GetMonitorInfoA(current, &monitor)) {
                SetWindowLongPtrA(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                SetWindowPos(window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                             monitor.rcMonitor.right - monitor.rcMonitor.left,
                             monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                             SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
            }
        } else {
            SetWindowLongPtrA(window, GWL_STYLE, windowedStyle);
            SetWindowPos(window, nullptr, windowedRect.left, windowedRect.top,
                         windowedRect.right - windowedRect.left,
                         windowedRect.bottom - windowedRect.top,
                         SWP_FRAMECHANGED | SWP_NOZORDER);
        }
        InvalidateRect(window, nullptr, FALSE);
    }

    void paintTo(HDC dc) {
        RECT client;
        GetClientRect(window, &client);
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        if (surface.empty() || viewport.width <= 0 || viewport.height <= 0) return;

        BITMAPINFO info;
        std::memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = viewport.width;
        info.bmiHeader.biHeight = -viewport.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, viewport.x, viewport.y, viewport.width, viewport.height, 0, 0,
                      viewport.width, viewport.height, surface.data(), &info,
                      DIB_RGB_COLORS, SRCCOPY);
    }

    void destroyIcons() {
        if (bigIcon) {
            DestroyIcon(bigIcon);
            bigIcon = nullptr;
        }
        if (smallIcon) {
            DestroyIcon(smallIcon);
            smallIcon = nullptr;
        }
    }
};

LRESULT CALLBACK Platform::Impl::windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTA*>(lp);
        self = static_cast<Impl*>(create->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->window = hwnd;
    }
    if (self) return self->handleMessage(msg, wp, lp);
    return DefWindowProcA(hwnd, msg, wp, lp);
}

LRESULT Platform::Impl::handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
            closed = true;
            return 0;
        case WM_DESTROY:
            closed = true;
            window = nullptr;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(window, &ps);
            paintTo(dc);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                winW = std::max(1, static_cast<int>(LOWORD(lp)));
                winH = std::max(1, static_cast<int>(HIWORD(lp)));
                InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        case WM_SETFOCUS:
            focused = true;
            return 0;
        case WM_KILLFOCUS:
            focused = false;
            clearHeld();
            return 0;
        case WM_ACTIVATEAPP:
            focused = wp != 0;
            if (!focused) clearHeld();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // Bit 30 marks keyboard auto-repeat; only the first press counts.
            if ((lp & (1 << 30)) != 0) return 0;
            handleKey(static_cast<int>(wp), true);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            handleKey(static_cast<int>(wp), false);
            return 0;
        case WM_SYSCHAR:
            // Swallow the beep Windows makes for Alt+Enter.
            if (wp == VK_RETURN) return 0;
            break;
        default:
            break;
    }
    return DefWindowProcA(window, msg, wp, lp);
}

Platform::Platform() : impl_(new Impl()) {}

Platform::~Platform() { close(); }

bool Platform::open(int scale, const std::string& title, std::string* err,
                    bool fullscreen) {
    close();
    Impl& d = *impl_;
    // Keep the window at native pixels instead of letting the desktop stretch
    // a bitmap-scaled copy; nearest-neighbour presentation stays crisp.
    SetProcessDPIAware();
    d.scale = clampInt(scale, 1, 6);
    d.instance = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc;
    std::memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &Impl::windowProc;
    wc.hInstance = d.instance;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        if (err) *err = "Gagal mendaftarkan kelas jendela Win32.";
        return false;
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect = {0, 0, VIEW_W * d.scale, VIEW_H * d.scale};
    AdjustWindowRectEx(&rect, style, FALSE, 0);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int x = std::max(0, (screenW - width) / 2);
    const int y = std::max(0, (screenH - height) / 2);

    d.window = CreateWindowExA(0, kWindowClass, title.c_str(), style, x, y, width, height,
                               nullptr, nullptr, d.instance, &d);
    if (!d.window) {
        if (err) *err = "Gagal membuat jendela Win32.";
        close();
        return false;
    }
    d.closed = false;
    d.windowedStyle = style;

    d.bigIcon = buildWin32Icon(64);
    d.smallIcon = buildWin32Icon(32);
    if (d.bigIcon) SendMessageA(d.window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(d.bigIcon));
    if (d.smallIcon) {
        SendMessageA(d.window, WM_SETICON, ICON_SMALL,
                     reinterpret_cast<LPARAM>(d.smallIcon));
    }

    ShowWindow(d.window, SW_SHOW);
    UpdateWindow(d.window);

    RECT client;
    GetClientRect(d.window, &client);
    d.winW = std::max(1, static_cast<int>(client.right - client.left));
    d.winH = std::max(1, static_cast<int>(client.bottom - client.top));
    d.openFlag = true;
    d.focused = true;
    d.fullscreen = false;
    d.fullscreenChanged = false;
    d.viewport = Viewport();
    d.surface.clear();
    d.clearHeld();
    for (int i = 0; i < KEY_COUNT; ++i) d.pressedNext[i] = false;

    if (fullscreen) {
        // The preference is already stored; do not report it back as a change.
        d.setFullscreen(true);
        d.fullscreenChanged = false;
    }

    timeBeginPeriod(1);
    d.timerActive = true;
    d.lastFrame = std::chrono::steady_clock::now();
    return true;
}

void Platform::close() {
    Impl& d = *impl_;
    if (d.window) {
        DestroyWindow(d.window);
        d.window = nullptr;
    }
    if (d.timerActive) {
        timeEndPeriod(1);
        d.timerActive = false;
    }
    d.destroyIcons();
    d.surface.clear();
    d.surface.shrink_to_fit();
    d.viewport = Viewport();
    d.openFlag = false;
    d.closed = false;
    d.fullscreen = false;
    d.fullscreenChanged = false;
    d.focused = true;
    d.windowedStyle = 0;
    d.windowedRect = RECT();
    d.winW = VIEW_W * d.scale;
    d.winH = VIEW_H * d.scale;
    d.clearHeld();
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
    in.clearPressed();
    for (int i = 0; i < KEY_COUNT; ++i) d.pressedNext[i] = false;

    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            d.closed = true;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (d.closed) return false;
    for (int i = 0; i < KEY_COUNT; ++i) {
        in.pressed[i] = d.pressedNext[i];
        in.held[i] = d.held[i];
    }
    return true;
}

void Platform::present(const Framebuffer& fb) {
    Impl& d = *impl_;
    if (!d.openFlag || !d.window) return;
    if (fb.w != VIEW_W || fb.h != VIEW_H) return;

    const Viewport vp = fitViewport(d.winW, d.winH);
    if (vp.width <= 0 || vp.height <= 0) return;
    d.surface.resize(static_cast<size_t>(vp.width) * vp.height);
    for (int y = 0; y < vp.height; ++y) {
        const uint32_t* src = fb.px.data() + static_cast<size_t>(y / vp.scale) * fb.w;
        uint32_t* dst = d.surface.data() + static_cast<size_t>(y) * vp.width;
        for (int x = 0; x < vp.width; ++x) dst[x] = toBGRA(src[x / vp.scale]);
    }
    d.viewport = vp;

    HDC dc = GetDC(d.window);
    if (dc) {
        d.paintTo(dc);
        ReleaseDC(d.window, dc);
    }
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
