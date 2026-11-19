#!/usr/bin/env bash
# install.sh - build dan pasang Pelita di Atas Segara Garam
#
# Skrip ini memeriksa dependensi build, menawarkan pemasangan paket yang
# diperlukan, mengkloning sumber ke ~/.aetheria/source, membangun game dari
# salinan itu, lalu memasang biner dan peluncur. Launcher desktop dipasang
# terpisah. Jalankan sebagai pengguna biasa; sudo hanya dipakai saat perlu.
#
# Linux: jendela X11. Windows: jalankan lewat Git Bash atau MSYS2; skrip
# memakai compiler MinGW-w64 dan memasang shortcut Start Menu.
set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$SCRIPT_DIR"

GAME_DIR="${HOME}/.aetheria"
REPO_URL="https://github.com/Hamzah82/AetheriaRPG.git"
REPO_EXPLICIT=0
BUILD_DIR=""
JOBS=""
ASSUME_YES=0
INSTALL_DEPS=1
BUILD_ONLY=0
CLEAN_BUILD=0
UNINSTALL=0
CXX_BIN=""
TMP_DESKTOP=""
TMP_LAUNCHER=""
TMP_SHORTCUT=""
IS_WINDOWS=0
EXE_SUFFIX=""
MSYS2_BASH=""
MSYS2_ROOT=""

if [ -t 1 ]; then
    C_BOLD=$'\033[1m'
    C_GREEN=$'\033[32m'
    C_YELLOW=$'\033[33m'
    C_RED=$'\033[31m'
    C_RESET=$'\033[0m'
else
    C_BOLD=""
    C_GREEN=""
    C_YELLOW=""
    C_RED=""
    C_RESET=""
fi

info() { printf '%s==>%s %s\n' "$C_BOLD" "$C_RESET" "$*"; }
ok() { printf '%s  ok%s %s\n' "$C_GREEN" "$C_RESET" "$*"; }
warn() { printf '%s  !%s %s\n' "$C_YELLOW" "$C_RESET" "$*" >&2; }
die() {
    printf '%s  x%s %s\n' "$C_RED" "$C_RESET" "$*" >&2
    exit 1
}

on_error() {
    local status=$?
    printf '\n%s: gagal pada baris %s (kode %s)\n' "${0##*/}" "$LINENO" "$status" >&2
    exit "$status"
}
cleanup() {
    if [ -n "$TMP_DESKTOP" ]; then
        rm -f "$TMP_DESKTOP" || true
    fi
    if [ -n "$TMP_LAUNCHER" ]; then
        rm -f "$TMP_LAUNCHER" || true
    fi
    if [ -n "$TMP_SHORTCUT" ]; then
        rm -f "$TMP_SHORTCUT" || true
    fi
}
trap on_error ERR
trap cleanup EXIT

usage() {
    cat <<'EOF'
Penggunaan: ./install.sh [opsi]

Membangun dan memasang Pelita di Atas Segara Garam.

Instalasi biasa mengkloning sumber dari GitHub ke folder game, lalu membangun
game dari salinan itu. Pemasangan ulang memperbarui salinan tersebut dengan
git pull --ff-only. Mode --build-only membangun checkout saat ini.

Linux memakai jendela X11. Windows didukung lewat Git Bash atau MSYS2 dengan
compiler MinGW-w64; skrip dapat memasang MSYS2 dan toolchain-nya otomatis.

Opsi:
  --dir DIR       folder data game (bawaan: ~/.aetheria)
  --prefix DIR    alias lama untuk --dir
  --repo URL      repositori sumber (bawaan: https://github.com/Hamzah82/AetheriaRPG.git)
  --jobs N        jumlah pekerjaan build (bawaan: jumlah CPU)
  --build-only    hanya membangun checkout saat ini, tidak memasang
  --clean         hapus hasil build sebelum membangun
  --no-deps       jangan pasang dependensi otomatis
  -y, --yes       jawab ya saat diminta memasang paket
  --uninstall     hapus biner, peluncur, dan launcher desktop
  -h, --help      tampilkan bantuan ini

Bawaan instalasi:
  folder game : ~/.aetheria
  biner       : ~/.aetheria/rpgcpp (Windows: rpgcpp.exe)
  peluncur    : ~/.aetheria/aetheria (Windows: Aetheria.bat)
  simpanan    : ~/.aetheria/savegame.dat
  sumber      : ~/.aetheria/source (salinan Git)
  launcher    : ~/.local/share/applications/aetheria.desktop (Linux)
                shortcut Start Menu Aetheria.lnk (Windows)
  ikon        : ~/.local/share/icons/hicolor/scalable/apps/aetheria.svg (Linux)
                tertanam di rpgcpp.exe (Windows)

Contoh:
  ./install.sh
  ./install.sh --build-only
  ./install.sh --dir ~/games/aetheria
  ./install.sh --repo https://github.com/pengguna/AetheriaRPG.git
  ./install.sh --uninstall
EOF
}

expand_path() {
    local path="$1"
    case "$path" in
        "~") path="$HOME" ;;
        "~/"*) path="$HOME/${path#\~/}" ;;
    esac
    case "$path" in
        [A-Za-z]:[\\/]*) ;;
        /*) ;;
        *) path="$SCRIPT_DIR/$path" ;;
    esac
    while [ "${#path}" -gt 1 ] && [ "${path%/}" != "$path" ]; do
        path="${path%/}"
    done
    printf '%s\n' "$path"
}

# cygpath converts between MSYS (/c/Users/...) and Windows (C:\Users\...)
# paths. Outside Windows the helpers are harmless pass-throughs.
to_posix() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$1"
    else
        printf '%s\n' "$1"
    fi
}

to_windows() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        --dir|--prefix)
            [ $# -ge 2 ] || die "$1 membutuhkan sebuah direktori"
            GAME_DIR="$2"
            shift 2
            ;;
        --dir=*|--prefix=*)
            GAME_DIR="${1#*=}"
            shift
            ;;
        --repo)
            [ $# -ge 2 ] || die "--repo membutuhkan alamat repositori"
            REPO_URL="$2"
            REPO_EXPLICIT=1
            shift 2
            ;;
        --repo=*)
            REPO_URL="${1#*=}"
            REPO_EXPLICIT=1
            shift
            ;;
        --jobs)
            [ $# -ge 2 ] || die "--jobs membutuhkan sebuah angka"
            JOBS="$2"
            shift 2
            ;;
        --jobs=*)
            JOBS="${1#*=}"
            shift
            ;;
        --build-only)
            BUILD_ONLY=1
            shift
            ;;
        --clean)
            CLEAN_BUILD=1
            shift
            ;;
        --no-deps)
            INSTALL_DEPS=0
            shift
            ;;
        -y|--yes)
            ASSUME_YES=1
            shift
            ;;
        --uninstall)
            UNINSTALL=1
            shift
            ;;
        *)
            die "Opsi tidak dikenal: $1 (lihat --help)"
            ;;
    esac
done

if [ "$UNINSTALL" -eq 1 ] && [ "$BUILD_ONLY" -eq 1 ]; then
    die "--uninstall dan --build-only tidak dapat dipakai bersamaan"
fi
[ -n "$REPO_URL" ] || die "--repo tidak boleh kosong"
if [ "$REPO_EXPLICIT" -eq 1 ] && [ "$BUILD_ONLY" -eq 1 ]; then
    warn "Opsi --repo diabaikan pada mode --build-only."
fi
if [ "$REPO_EXPLICIT" -eq 1 ] && [ "$UNINSTALL" -eq 1 ]; then
    warn "Opsi --repo diabaikan pada mode --uninstall."
fi

# Windows is detected from the MSYS/MinGW uname or the native OS variable.
case "$(uname -s 2>/dev/null || printf unknown)" in
    MINGW*|MSYS*|CYGWIN*) IS_WINDOWS=1 ;;
esac
if [ "${OS:-}" = "Windows_NT" ]; then
    IS_WINDOWS=1
fi
if [ "$IS_WINDOWS" -eq 1 ]; then
    EXE_SUFFIX=".exe"
fi

GAME_DIR="$(expand_path "$GAME_DIR")"
BINARY="$GAME_DIR/rpgcpp$EXE_SUFFIX"
SOURCE_DIR="$GAME_DIR/source"
if [ "$IS_WINDOWS" -eq 1 ]; then
    LAUNCHER="$GAME_DIR/Aetheria.bat"
    APPS_DIR=""
    DESKTOP_FILE=""
    ICONS_DIR=""
    ICON_FILE=""
    START_MENU_DIR="$(to_posix "${APPDATA:-$HOME/AppData/Roaming}")/Microsoft/Windows/Start Menu/Programs"
else
    LAUNCHER="$GAME_DIR/aetheria"
    APPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
    DESKTOP_FILE="$APPS_DIR/aetheria.desktop"
    ICONS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps"
    ICON_FILE="$ICONS_DIR/aetheria.svg"
    START_MENU_DIR=""
fi

if [ -z "$JOBS" ]; then
    JOBS="$(nproc 2>/dev/null || printf '%s' "${NUMBER_OF_PROCESSORS:-2}")"
fi
JOBS="$(printf '%s' "$JOBS" | tr -d '[:space:]')"
case "$JOBS" in
    ''|*[!0-9]*) die "--jobs harus berupa angka positif" ;;
esac
[ "$JOBS" -ge 1 ] || die "--jobs harus berupa angka positif"

can_write() {
    local path="$1"
    while [ ! -e "$path" ]; do
        local parent
        parent="$(dirname -- "$path")"
        [ "$parent" != "$path" ] || break
        path="$parent"
    done
    [ -w "$path" ]
}

as_priv() {
    local target="$1"
    shift
    if [ "$IS_WINDOWS" -eq 1 ] || [ "$EUID" -eq 0 ] || can_write "$target"; then
        "$@"
    else
        command -v sudo >/dev/null 2>&1 ||
            die "Butuh sudo untuk menulis ke $target, tetapi sudo tidak ditemukan."
        sudo "$@"
    fi
}

as_root() {
    if [ "$IS_WINDOWS" -eq 1 ] || [ "$EUID" -eq 0 ]; then
        "$@"
    else
        command -v sudo >/dev/null 2>&1 ||
            die "Butuh sudo untuk memasang paket, tetapi sudo tidak ditemukan."
        sudo "$@"
    fi
}

resolve_compiler() {
    if [ "$IS_WINDOWS" -eq 1 ] && find_msys2_bash; then
        # Prefer the MinGW-w64 toolchain over an MSYS compiler, which would
        # produce binaries that need the MSYS runtime DLL.
        if [ "$MSYS2_ROOT" = "/" ] && [ -x /mingw64/bin/g++.exe ]; then
            case ":$PATH:" in
                *":/mingw64/bin:"*) ;;
                *) PATH="/mingw64/bin:$PATH" ;;
            esac
        elif [ -n "$MSYS2_ROOT" ] && [ -x "$MSYS2_ROOT/mingw64/bin/g++.exe" ]; then
            case ":$PATH:" in
                *":$MSYS2_ROOT/mingw64/bin:"*) ;;
                *) PATH="$MSYS2_ROOT/mingw64/bin:$MSYS2_ROOT/usr/bin:$PATH" ;;
            esac
        fi
        export PATH
    fi

    if [ -n "${CXX:-}" ] && command -v "${CXX}" >/dev/null 2>&1; then
        CXX_BIN="$CXX"
    elif [ "$IS_WINDOWS" -eq 1 ]; then
        local candidate
        for candidate in g++ x86_64-w64-mingw32-g++ clang++; do
            if command -v "$candidate" >/dev/null 2>&1 &&
                "$candidate" -dumpmachine 2>/dev/null | grep -q mingw; then
                CXX_BIN="$candidate"
                return
            fi
        done
        if command -v g++ >/dev/null 2>&1; then
            CXX_BIN="g++"
        elif command -v clang++ >/dev/null 2>&1; then
            CXX_BIN="clang++"
        else
            CXX_BIN=""
        fi
    elif command -v g++ >/dev/null 2>&1; then
        CXX_BIN="g++"
    elif command -v clang++ >/dev/null 2>&1; then
        CXX_BIN="clang++"
    else
        CXX_BIN=""
    fi
}

has_platform_headers() {
    [ -n "$CXX_BIN" ] || return 1
    if [ "$IS_WINDOWS" -eq 1 ]; then
        "$CXX_BIN" -dumpmachine 2>/dev/null | grep -q mingw || return 1
        local probe status
        probe="$(mktemp "${TMPDIR:-/tmp}/aetheria-probe.XXXXXX")" || return 1
        printf '#include <windows.h>\n#include <thread>\nint main(){std::thread t([]{});t.join();return 0;}\n' |
            "$CXX_BIN" -x c++ - -o "$probe.exe" -lwinmm >/dev/null 2>&1
        status=$?
        rm -f "$probe" "$probe.exe"
        return "$status"
    fi
    printf '#include <X11/Xlib.h>\n' |
        "$CXX_BIN" -x c++ -E - >/dev/null 2>&1
}

# Locate an MSYS2 installation. Sets MSYS2_BASH and MSYS2_ROOT and returns 0
# when one is usable. Inside MSYS2 the root is "/". Git Bash also has
# /usr/bin/bash.exe and /mingw64, so pacman is the reliable marker.
find_msys2_bash() {
    [ "$IS_WINDOWS" -eq 1 ] || return 1
    MSYS2_BASH=""
    MSYS2_ROOT=""
    if [ -x /usr/bin/bash.exe ] && [ -d /mingw64 ] &&
        { [ -x /usr/bin/pacman.exe ] || [ -x /usr/bin/pacman ]; }; then
        MSYS2_ROOT="/"
        MSYS2_BASH="/usr/bin/bash.exe"
        return 0
    fi
    local candidate
    local roots=(
        "/c/msys64"
        "/c/msys2"
        "$(to_posix "${LOCALAPPDATA:-/nonexistent}")/Programs/msys64"
        "$(to_posix "${LOCALAPPDATA:-/nonexistent}")/msys64"
        "$(to_posix "${ProgramFiles:-/nonexistent}")/msys64"
    )
    for candidate in "${roots[@]}"; do
        if [ -x "$candidate/usr/bin/bash.exe" ] &&
            { [ -x "$candidate/usr/bin/pacman.exe" ] || [ -x "$candidate/usr/bin/pacman" ]; }; then
            MSYS2_ROOT="$candidate"
            MSYS2_BASH="$candidate/usr/bin/bash.exe"
            return 0
        fi
    done
    return 1
}

install_dependencies_windows() {
    local pacman_line
    pacman_line='pacman -S --needed --noconfirm --disable-download-timeout mingw-w64-x86_64-gcc make git'

    if find_msys2_bash; then
        if [ "$MSYS2_ROOT" = "/" ] && command -v pacman >/dev/null 2>&1; then
            info "Memasang compiler MinGW-w64, make, dan git..."
            pacman -S --needed --noconfirm --disable-download-timeout \
                mingw-w64-x86_64-gcc make git
            return
        fi
        info "Memasang compiler MinGW-w64, make, dan git lewat MSYS2..."
        MSYSTEM=MINGW64 "$MSYS2_BASH" -lc "$pacman_line"
        return
    fi

    local winget_bin=""
    if command -v winget.exe >/dev/null 2>&1; then
        winget_bin="winget.exe"
    elif command -v winget >/dev/null 2>&1; then
        winget_bin="winget"
    fi
    if [ -z "$winget_bin" ]; then
        die "MSYS2 belum terpasang. Pasang dari https://www.msys2.org/ lalu jalankan ulang ./install.sh."
    fi

    info "Memasang MSYS2 lewat winget..."
    "$winget_bin" install --id MSYS2.MSYS2 -e --source winget \
        --accept-package-agreements --accept-source-agreements --silent ||
        warn "winget melaporkan kegagalan; mencari MSYS2 yang sudah ada."
    if ! find_msys2_bash; then
        die "MSYS2 belum ditemukan. Buka 'MSYS2 MinGW 64-bit' dari menu Start, lalu jalankan ./install.sh lagi."
    fi
    info "Memasang compiler MinGW-w64, make, dan git lewat MSYS2..."
    MSYSTEM=MINGW64 "$MSYS2_BASH" -lc "$pacman_line"
}

detect_pkg_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        printf 'apt'
    elif command -v dnf >/dev/null 2>&1; then
        printf 'dnf'
    elif command -v pacman >/dev/null 2>&1; then
        printf 'pacman'
    elif command -v zypper >/dev/null 2>&1; then
        printf 'zypper'
    elif command -v apk >/dev/null 2>&1; then
        printf 'apk'
    else
        printf 'unknown'
    fi
}

install_dependencies() {
    if [ "$IS_WINDOWS" -eq 1 ]; then
        install_dependencies_windows
        return
    fi
    local pm
    pm="$(detect_pkg_manager)"
    case "$pm" in
        apt)
            info "Memperbarui indeks paket apt..."
            as_root apt-get update
            info "Memasang build-essential, libx11-dev, dan git..."
            as_root env DEBIAN_FRONTEND=noninteractive \
                apt-get install -y build-essential libx11-dev git
            ;;
        dnf)
            info "Memasang gcc-c++, make, libX11-devel, dan git..."
            as_root dnf install -y gcc-c++ make libX11-devel git
            ;;
        pacman)
            info "Memasang base-devel, libx11, dan git..."
            as_root pacman -S --needed --noconfirm base-devel libx11 git
            ;;
        zypper)
            info "Memasang gcc-c++, make, libX11-devel, dan git..."
            as_root zypper --non-interactive install gcc-c++ make libX11-devel git
            ;;
        apk)
            info "Memasang build-base, libx11-dev, dan git..."
            as_root apk add --no-cache build-base libx11-dev git
            ;;
        *)
            die "Manajer paket tidak dikenali. Pasang manual: compiler C++17, make, git, dan header X11."
            ;;
    esac
}

update_desktop_database() {
    [ "$IS_WINDOWS" -eq 0 ] || return 0
    if command -v update-desktop-database >/dev/null 2>&1; then
        as_priv "$APPS_DIR" update-desktop-database "$APPS_DIR" >/dev/null 2>&1 ||
            true
    fi
}

update_icon_cache() {
    [ "$IS_WINDOWS" -eq 0 ] || return 0
    local theme_dir="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        as_priv "$theme_dir" gtk-update-icon-cache -f -t "$theme_dir" >/dev/null 2>&1 ||
            true
    fi
}

remove_legacy_install() {
    [ "$IS_WINDOWS" -eq 0 ] || return 0
    local old_bin="$HOME/.local/bin/rpgcpp"
    local old_marker="$HOME/.local/share/aetheria/source"
    local old_desktop="$HOME/.local/share/applications/aetheria.desktop"
    if [ -e "$old_bin" ]; then
        rm -f "$old_bin"
        ok "Biner lama dihapus: $old_bin"
    fi
    if [ -f "$old_marker" ]; then
        rm -f "$old_marker"
        rmdir "$HOME/.local/share/aetheria" 2>/dev/null || true
        ok "Catatan sumber lama dihapus: $old_marker"
    fi
    if [ "$old_desktop" != "$DESKTOP_FILE" ] && [ -e "$old_desktop" ]; then
        rm -f "$old_desktop"
        ok "Launcher lama dihapus: $old_desktop"
    fi
}

prepare_source() {
    if [ -e "$GAME_DIR" ] && [ ! -d "$GAME_DIR" ]; then
        die "$GAME_DIR sudah ada dan bukan sebuah folder."
    fi
    as_priv "$GAME_DIR" install -d -m 0755 "$GAME_DIR"

    # Versi lama hanya menulis berkas penunjuk berisi lokasi checkout.
    # Berkas itu diganti salinan Git sungguhan agar game bisa memperbarui diri.
    if [ -f "$SOURCE_DIR" ]; then
        info "Mengganti catatan sumber lama dengan salinan Git..."
        as_priv "$GAME_DIR" rm -f "$SOURCE_DIR"
        ok "Catatan sumber lama dihapus: $SOURCE_DIR"
    elif [ -e "$SOURCE_DIR" ] && [ ! -d "$SOURCE_DIR" ]; then
        die "$SOURCE_DIR ada dan bukan folder. Pindahkan atau hapus dulu."
    fi

    if [ -d "$SOURCE_DIR/.git" ]; then
        info "Memperbarui salinan sumber di $SOURCE_DIR..."
        if [ -n "$(git -C "$SOURCE_DIR" status --porcelain --untracked-files=no 2>/dev/null)" ]; then
            die "Salinan sumber punya perubahan lokal. Commit atau kembalikan dulu: $SOURCE_DIR"
        fi
        local current_url
        current_url="$(git -C "$SOURCE_DIR" remote get-url origin 2>/dev/null || true)"
        if [ -n "$current_url" ] && [ "$current_url" != "$REPO_URL" ]; then
            info "Mengarahkan remote origin ke $REPO_URL..."
            git -C "$SOURCE_DIR" remote set-url origin "$REPO_URL"
        fi
        if ! git -C "$SOURCE_DIR" pull --ff-only; then
            die "Git pull gagal. Periksa koneksi, lalu coba lagi: $SOURCE_DIR"
        fi
        ok "Sumber diperbarui: $SOURCE_DIR"
    elif [ -d "$SOURCE_DIR" ] && [ -n "$(ls -A "$SOURCE_DIR" 2>/dev/null)" ]; then
        die "$SOURCE_DIR sudah berisi berkas lain. Kosongkan folder itu atau pakai --dir lain."
    else
        info "Mengkloning sumber dari $REPO_URL..."
        if ! git clone -- "$REPO_URL" "$SOURCE_DIR"; then
            die "Git clone gagal. Periksa alamat repositori dan koneksi."
        fi
        ok "Sumber dikloning: $SOURCE_DIR"
    fi

    BUILD_DIR="$SOURCE_DIR"
}

if [ "$UNINSTALL" -eq 1 ]; then
    info "Menghapus Pelita dari $GAME_DIR..."
    removed=0
    for file in "$BINARY" "$BINARY.old" "$LAUNCHER" "$GAME_DIR/rpgcpp" "$GAME_DIR/aetheria"; do
        if [ -e "$file" ]; then
            as_priv "$GAME_DIR" rm -f "$file"
            ok "Dihapus: $file"
            removed=1
        fi
    done
    # Catatan sumber lama berupa berkas penunjuk; salinan sumber berupa folder
    # dan sengaja dipertahankan.
    if [ -f "$SOURCE_DIR" ]; then
        as_priv "$GAME_DIR" rm -f "$SOURCE_DIR"
        ok "Catatan sumber lama dihapus: $SOURCE_DIR"
        removed=1
    fi
    if [ "$removed" -eq 0 ]; then
        warn "Tidak ada berkas Pelita di $GAME_DIR"
    fi
    if [ "$IS_WINDOWS" -eq 1 ]; then
        shortcut="$START_MENU_DIR/Aetheria.lnk"
        if [ -e "$shortcut" ]; then
            rm -f "$shortcut"
            ok "Shortcut Start Menu dihapus: $shortcut"
        else
            warn "Shortcut Start Menu tidak ditemukan: $shortcut"
        fi
    else
        if [ -e "$DESKTOP_FILE" ]; then
            as_priv "$DESKTOP_FILE" rm -f "$DESKTOP_FILE"
            update_desktop_database
            ok "Launcher dihapus: $DESKTOP_FILE"
        else
            warn "Launcher tidak ditemukan: $DESKTOP_FILE"
        fi
        if [ -e "$ICON_FILE" ]; then
            as_priv "$ICONS_DIR" rm -f "$ICON_FILE"
            update_icon_cache
            ok "Ikon dihapus: $ICON_FILE"
        else
            warn "Ikon tidak ditemukan: $ICON_FILE"
        fi
    fi
    remove_legacy_install
    if [ -d "$SOURCE_DIR" ]; then
        ok "Salinan sumber dipertahankan: $SOURCE_DIR"
    fi
    if [ -e "$GAME_DIR/savegame.dat" ]; then
        ok "Berkas simpanan dipertahankan: $GAME_DIR/savegame.dat"
    fi
    if [ -d "$GAME_DIR" ]; then
        if rmdir "$GAME_DIR" 2>/dev/null; then
            ok "Folder game kosong dihapus: $GAME_DIR"
        else
            warn "Folder game dibiarkan: $GAME_DIR"
        fi
    fi
    ok "Instalasi dihapus. Hasil build tidak disentuh."
    exit 0
fi

resolve_compiler
missing=()
if [ -z "$CXX_BIN" ]; then
    missing+=("compiler C++17")
fi
if ! command -v make >/dev/null 2>&1; then
    missing+=("make")
fi
if [ "$BUILD_ONLY" -eq 0 ] && ! command -v git >/dev/null 2>&1; then
    missing+=("git")
fi
if [ -n "$CXX_BIN" ] && ! has_platform_headers; then
    if [ "$IS_WINDOWS" -eq 1 ]; then
        missing+=("compiler MinGW-w64 dan pustaka Win32")
    else
        missing+=("header X11 (libX11)")
    fi
fi

if [ ${#missing[@]} -gt 0 ]; then
    warn "Dependensi belum lengkap: ${missing[*]}"
    if [ "$INSTALL_DEPS" -eq 0 ]; then
        die "Pasang dependensi tersebut lalu jalankan ulang, atau jalankan tanpa --no-deps."
    fi
    if [ "$ASSUME_YES" -ne 1 ]; then
        if [ ! -t 0 ]; then
            die "Dependensi belum lengkap dan sesi tidak interaktif. Gunakan --yes atau pasang manual."
        fi
        printf '%sPasang paket dependensi sekarang? [Y/n]%s ' "$C_BOLD" "$C_RESET"
        read -r reply
        case "$reply" in
            ""|y|Y|ya|Ya) ;;
            *) die "Dibatalkan oleh pengguna." ;;
        esac
    fi
    install_dependencies
    resolve_compiler
    if [ -z "$CXX_BIN" ] || ! command -v make >/dev/null 2>&1 || ! has_platform_headers ||
        { [ "$BUILD_ONLY" -eq 0 ] && ! command -v git >/dev/null 2>&1; }; then
        die "Dependensi masih belum lengkap setelah pemasangan. Periksa keluaran di atas."
    fi
    ok "Dependensi siap."
fi

if [ "$BUILD_ONLY" -eq 1 ]; then
    BUILD_DIR="$SCRIPT_DIR"
else
    prepare_source
fi

[ -f "$BUILD_DIR/src/main.cpp" ] || die "Berkas sumber tidak ditemukan di $BUILD_DIR."
[ -f "$BUILD_DIR/Makefile" ] || die "Makefile tidak ditemukan di $BUILD_DIR."

if [ "$CLEAN_BUILD" -eq 1 ]; then
    info "Membersihkan hasil build sebelumnya..."
    make -C "$BUILD_DIR" clean
fi

info "Membangun Pelita dengan $CXX_BIN -j$JOBS..."
make -C "$BUILD_DIR" -j"$JOBS" CXX="$CXX_BIN"
BUILD_BINARY="$BUILD_DIR/rpgcpp$EXE_SUFFIX"
if [ ! -f "$BUILD_BINARY" ]; then
    die "Build selesai, tetapi biner rpgcpp$EXE_SUFFIX tidak ditemukan."
fi

info "Menjalankan uji asap headless..."
if ! "$BUILD_BINARY" --headless --frames 60 >/dev/null 2>&1; then
    die "Uji asap gagal. Coba $BUILD_BINARY --headless --frames 60 untuk melihat kesalahan."
fi
ok "Build dan uji asap berhasil."

if [ "$BUILD_ONLY" -eq 1 ]; then
    ok "Mode build-only: biner tersedia di $BUILD_BINARY"
    printf '  Jalankan: %s\n' "$BUILD_BINARY"
    exit 0
fi

mkdir -p "$GAME_DIR" || die "Tidak bisa membuat folder game: $GAME_DIR"

remove_legacy_install

# Create a Start Menu shortcut on Windows. The .bat launcher remains available
# when PowerShell is missing or blocked by policy.
install_windows_shortcut() {
    [ "$IS_WINDOWS" -eq 1 ] || return 0
    local powershell_bin=""
    if command -v powershell.exe >/dev/null 2>&1; then
        powershell_bin="powershell.exe"
    elif command -v powershell >/dev/null 2>&1; then
        powershell_bin="powershell"
    fi
    if [ -z "$powershell_bin" ]; then
        warn "PowerShell tidak ditemukan; shortcut Start Menu dilewati."
        return 0
    fi
    mkdir -p "$START_MENU_DIR" 2>/dev/null || true
    local script target workdir link target_win
    script="$(mktemp "${TMPDIR:-/tmp}/aetheria-shortcut.XXXXXX.ps1")"
    TMP_SHORTCUT="$script"
    target="$(to_windows "$BINARY")"
    workdir="$(to_windows "$GAME_DIR")"
    link="$(to_windows "$START_MENU_DIR/Aetheria.lnk")"
    target_win="$(to_windows "$BINARY")"
    cat >"$script" <<EOF
\$shell = New-Object -ComObject WScript.Shell
\$shortcut = \$shell.CreateShortcut('$link')
\$shortcut.TargetPath = '$target'
\$shortcut.WorkingDirectory = '$workdir'
\$shortcut.IconLocation = '$target_win,0'
\$shortcut.Description = 'Pelita di Atas Segara Garam'
\$shortcut.Save()
EOF
    if "$powershell_bin" -NoProfile -ExecutionPolicy Bypass \
        -File "$(to_windows "$script")" >/dev/null 2>&1; then
        ok "Shortcut Start Menu: $START_MENU_DIR/Aetheria.lnk"
    else
        warn "Gagal membuat shortcut Start Menu; gunakan $LAUNCHER."
    fi
    rm -f "$script"
    TMP_SHORTCUT=""
}

info "Memasang biner ke $GAME_DIR..."
as_priv "$GAME_DIR" install -m 0755 "$BUILD_BINARY" "$BINARY"
ok "Biner terpasang: $BINARY"

info "Memasang peluncur $LAUNCHER..."
if [ "$IS_WINDOWS" -eq 1 ]; then
    tmp_launcher="$(mktemp "${TMPDIR:-/tmp}/aetheria-launcher.XXXXXX.bat")"
    TMP_LAUNCHER="$tmp_launcher"
    cat >"$tmp_launcher" <<'LAUNCHER'
@echo off
rem Jalankan Pelita dari folder game agar savegame.dat tetap di sini.
cd /d "%~dp0"
start "" "%~dp0rpgcpp.exe" %*
LAUNCHER
    as_priv "$GAME_DIR" install -m 0644 "$tmp_launcher" "$LAUNCHER"
    rm -f "$tmp_launcher"
    TMP_LAUNCHER=""
    ok "Peluncur terpasang: $LAUNCHER"
    install_windows_shortcut
else
    tmp_launcher="$(mktemp "${TMPDIR:-/tmp}/aetheria-launcher.XXXXXX")"
    TMP_LAUNCHER="$tmp_launcher"
    cat >"$tmp_launcher" <<'LAUNCHER'
#!/bin/sh
# Jalankan Pelita dari folder game agar savegame.dat tetap di sini.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P) || exit 1
cd "$DIR" || exit 1
exec ./rpgcpp "$@"
LAUNCHER
    as_priv "$GAME_DIR" install -m 0755 "$tmp_launcher" "$LAUNCHER"
    rm -f "$tmp_launcher"
    TMP_LAUNCHER=""
    ok "Peluncur terpasang: $LAUNCHER"

    info "Memasang ikon aplikasi ke $ICONS_DIR..."
    [ -f "$BUILD_DIR/assets/aetheria.svg" ] || die "Ikon aplikasi tidak ditemukan: $BUILD_DIR/assets/aetheria.svg"
    as_priv "$ICONS_DIR" install -d -m 0755 "$ICONS_DIR"
    as_priv "$ICONS_DIR" install -m 0644 "$BUILD_DIR/assets/aetheria.svg" "$ICON_FILE"
    update_icon_cache
    ok "Ikon terpasang: $ICON_FILE"

    info "Memasang launcher desktop ke $APPS_DIR..."
    tmp_desktop="$(mktemp "${TMPDIR:-/tmp}/aetheria-desktop.XXXXXX.desktop")"
    TMP_DESKTOP="$tmp_desktop"
    cat >"$tmp_desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Pelita di Atas Segara Garam
GenericName=RPG 2D
Comment=Petualangan RPG 2D prosedural berbahasa Indonesia
Exec="$LAUNCHER"
TryExec=$LAUNCHER
Path=$GAME_DIR
Icon=aetheria
Terminal=false
Categories=Game;RolePlaying;
Keywords=RPG;game;fantasi;
StartupNotify=true
EOF
    if command -v desktop-file-validate >/dev/null 2>&1; then
        desktop-file-validate "$tmp_desktop" ||
            warn "Launcher memiliki peringatan validasi; tetap dipasang."
    fi
    as_priv "$APPS_DIR" install -d "$APPS_DIR"
    as_priv "$APPS_DIR" install -m 0644 "$tmp_desktop" "$DESKTOP_FILE"
    rm -f "$tmp_desktop"
    TMP_DESKTOP=""
    update_desktop_database
    ok "Launcher terpasang: $DESKTOP_FILE"
fi

if ! "$BINARY" --headless --frames 30 >/dev/null 2>&1; then
    warn "Biner terpasang, tetapi uji asap dari folder game gagal."
else
    ok "Uji asap biner terpasang berhasil."
fi

printf '\n%sPelita siap dimainkan.%s\n' "$C_GREEN" "$C_RESET"
printf '  Folder   : %s\n' "$GAME_DIR"
printf '  Biner    : %s\n' "$BINARY"
printf '  Jalankan : %s\n' "$LAUNCHER"
printf '  Simpanan : %s/savegame.dat\n' "$GAME_DIR"
printf '  Sumber   : %s\n' "$SOURCE_DIR"
if [ "$IS_WINDOWS" -eq 1 ]; then
    printf '  Shortcut : %s\n' "$START_MENU_DIR/Aetheria.lnk"
else
    printf '  Launcher : %s\n' "$DESKTOP_FILE"
    printf '  Ikon     : %s\n' "$ICON_FILE"
fi
