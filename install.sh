#!/usr/bin/env bash
# install.sh - build dan pasang Aetheria: Bayangan Terakhir
#
# Skrip ini memeriksa dependensi build, menawarkan pemasangan paket yang
# diperlukan, mengkloning sumber ke ~/.aetheria/source, membangun game dari
# salinan itu, lalu memasang biner dan peluncur. Launcher desktop dipasang
# terpisah. Jalankan sebagai pengguna biasa; sudo hanya dipakai saat perlu.
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
}
trap on_error ERR
trap cleanup EXIT

usage() {
    cat <<'EOF'
Penggunaan: ./install.sh [opsi]

Membangun dan memasang Aetheria: Bayangan Terakhir.

Instalasi biasa mengkloning sumber dari GitHub ke folder game, lalu membangun
game dari salinan itu. Pemasangan ulang memperbarui salinan tersebut dengan
git pull --ff-only. Mode --build-only membangun checkout saat ini.

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
  biner       : ~/.aetheria/rpgcpp
  peluncur    : ~/.aetheria/aetheria
  simpanan    : ~/.aetheria/savegame.dat
  sumber      : ~/.aetheria/source (salinan Git)
  launcher    : ~/.local/share/applications/aetheria.desktop

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
        /*) ;;
        *) path="$SCRIPT_DIR/$path" ;;
    esac
    while [ "${#path}" -gt 1 ] && [ "${path%/}" != "$path" ]; do
        path="${path%/}"
    done
    printf '%s\n' "$path"
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

GAME_DIR="$(expand_path "$GAME_DIR")"
LAUNCHER="$GAME_DIR/aetheria"
SOURCE_DIR="$GAME_DIR/source"
APPS_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
DESKTOP_FILE="$APPS_DIR/aetheria.desktop"

if [ -z "$JOBS" ]; then
    JOBS="$(nproc 2>/dev/null || printf '2')"
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
    if [ "$EUID" -eq 0 ] || can_write "$target"; then
        "$@"
    else
        command -v sudo >/dev/null 2>&1 ||
            die "Butuh sudo untuk menulis ke $target, tetapi sudo tidak ditemukan."
        sudo "$@"
    fi
}

as_root() {
    if [ "$EUID" -eq 0 ]; then
        "$@"
    else
        command -v sudo >/dev/null 2>&1 ||
            die "Butuh sudo untuk memasang paket, tetapi sudo tidak ditemukan."
        sudo "$@"
    fi
}

resolve_compiler() {
    if [ -n "${CXX:-}" ] && command -v "${CXX}" >/dev/null 2>&1; then
        CXX_BIN="$CXX"
    elif command -v g++ >/dev/null 2>&1; then
        CXX_BIN="g++"
    elif command -v clang++ >/dev/null 2>&1; then
        CXX_BIN="clang++"
    else
        CXX_BIN=""
    fi
}

has_x11_headers() {
    [ -n "$CXX_BIN" ] || return 1
    printf '#include <X11/Xlib.h>\n' |
        "$CXX_BIN" -x c++ -E - >/dev/null 2>&1
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
    if command -v update-desktop-database >/dev/null 2>&1; then
        as_priv "$APPS_DIR" update-desktop-database "$APPS_DIR" >/dev/null 2>&1 ||
            true
    fi
}

remove_legacy_install() {
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
    info "Menghapus Aetheria dari $GAME_DIR..."
    removed=0
    for file in "$GAME_DIR/rpgcpp" "$LAUNCHER"; do
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
        warn "Tidak ada berkas Aetheria di $GAME_DIR"
    fi
    if [ -e "$DESKTOP_FILE" ]; then
        as_priv "$DESKTOP_FILE" rm -f "$DESKTOP_FILE"
        update_desktop_database
        ok "Launcher dihapus: $DESKTOP_FILE"
    else
        warn "Launcher tidak ditemukan: $DESKTOP_FILE"
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
if [ -n "$CXX_BIN" ] && ! has_x11_headers; then
    missing+=("header X11 (libX11)")
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
    if [ -z "$CXX_BIN" ] || ! command -v make >/dev/null 2>&1 || ! has_x11_headers ||
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

info "Membangun Aetheria dengan $CXX_BIN -j$JOBS..."
make -C "$BUILD_DIR" -j"$JOBS" CXX="$CXX_BIN"
[ -x "$BUILD_DIR/rpgcpp" ] || die "Build selesai, tetapi biner rpgcpp tidak ditemukan."

info "Menjalankan uji asap headless..."
if ! "$BUILD_DIR/rpgcpp" --headless --frames 60 >/dev/null 2>&1; then
    die "Uji asap gagal. Coba $BUILD_DIR/rpgcpp --headless --frames 60 untuk melihat kesalahan."
fi
ok "Build dan uji asap berhasil."

if [ "$BUILD_ONLY" -eq 1 ]; then
    ok "Mode build-only: biner tersedia di $BUILD_DIR/rpgcpp"
    printf '  Jalankan: %s\n' "$BUILD_DIR/rpgcpp"
    exit 0
fi

remove_legacy_install

info "Memasang biner ke $GAME_DIR..."
as_priv "$GAME_DIR" install -m 0755 "$BUILD_DIR/rpgcpp" "$GAME_DIR/rpgcpp"
ok "Biner terpasang: $GAME_DIR/rpgcpp"

info "Memasang peluncur $LAUNCHER..."
tmp_launcher="$(mktemp "${TMPDIR:-/tmp}/aetheria-launcher.XXXXXX")"
TMP_LAUNCHER="$tmp_launcher"
cat >"$tmp_launcher" <<'LAUNCHER'
#!/bin/sh
# Jalankan Aetheria dari folder game agar savegame.dat tetap di sini.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P) || exit 1
cd "$DIR" || exit 1
exec ./rpgcpp "$@"
LAUNCHER
as_priv "$GAME_DIR" install -m 0755 "$tmp_launcher" "$LAUNCHER"
rm -f "$tmp_launcher"
TMP_LAUNCHER=""
ok "Peluncur terpasang: $LAUNCHER"

info "Memasang launcher desktop ke $APPS_DIR..."
tmp_desktop="$(mktemp "${TMPDIR:-/tmp}/aetheria-desktop.XXXXXX.desktop")"
TMP_DESKTOP="$tmp_desktop"
cat >"$tmp_desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Aetheria: Bayangan Terakhir
GenericName=RPG 2D
Comment=Petualangan RPG 2D prosedural berbahasa Indonesia
Exec="$LAUNCHER"
TryExec=$LAUNCHER
Path=$GAME_DIR
Icon=applications-games
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

if ! "$LAUNCHER" --headless --frames 30 >/dev/null 2>&1; then
    warn "Biner terpasang, tetapi uji asap dari folder game gagal."
else
    ok "Uji asap biner terpasang berhasil."
fi

printf '\n%sAetheria siap dimainkan.%s\n' "$C_GREEN" "$C_RESET"
printf '  Folder   : %s\n' "$GAME_DIR"
printf '  Biner    : %s\n' "$GAME_DIR/rpgcpp"
printf '  Jalankan : %s\n' "$LAUNCHER"
printf '  Simpanan : %s/savegame.dat\n' "$GAME_DIR"
printf '  Sumber   : %s\n' "$SOURCE_DIR"
printf '  Launcher : %s\n' "$DESKTOP_FILE"
