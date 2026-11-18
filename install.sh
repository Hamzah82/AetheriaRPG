#!/usr/bin/env bash
# install.sh - build dan pasang Aetheria: Bayangan Terakhir
#
# Skrip ini memeriksa dependensi build, menawarkan pemasangan paket yang
# diperlukan, membangun game, lalu memasang biner dan launcher desktop.
# Jalankan sebagai pengguna biasa; sudo hanya dipakai saat benar-benar perlu.
set -Eeuo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$SCRIPT_DIR"

PREFIX="${HOME}/.local"
JOBS=""
ASSUME_YES=0
INSTALL_DEPS=1
BUILD_ONLY=0
CLEAN_BUILD=0
UNINSTALL=0
CXX_BIN=""
TMP_DESKTOP=""

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
}
trap on_error ERR
trap cleanup EXIT

usage() {
    cat <<'EOF'
Penggunaan: ./install.sh [opsi]

Membangun dan memasang Aetheria: Bayangan Terakhir.

Opsi:
  --prefix DIR    direktori instalasi (bawaan: ~/.local)
  --jobs N        jumlah pekerjaan build (bawaan: jumlah CPU)
  --build-only    hanya membangun, tidak memasang
  --clean         hapus hasil build sebelum membangun
  --no-deps       jangan pasang dependensi otomatis
  -y, --yes       jawab ya saat diminta memasang paket
  --uninstall     hapus biner dan launcher yang terpasang
  -h, --help      tampilkan bantuan ini

Bawaan instalasi:
  biner    : ~/.local/bin/rpgcpp
  launcher : ~/.local/share/applications/aetheria.desktop

Contoh:
  ./install.sh
  ./install.sh --build-only
  ./install.sh --prefix /usr/local
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
        --prefix)
            [ $# -ge 2 ] || die "--prefix membutuhkan sebuah direktori"
            PREFIX="$2"
            shift 2
            ;;
        --prefix=*)
            PREFIX="${1#*=}"
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

PREFIX="$(expand_path "$PREFIX")"
BIN_DIR="$PREFIX/bin"
APPS_DIR="$PREFIX/share/applications"
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
            info "Memasang build-essential dan libx11-dev..."
            as_root env DEBIAN_FRONTEND=noninteractive \
                apt-get install -y build-essential libx11-dev
            ;;
        dnf)
            info "Memasang gcc-c++, make, dan libX11-devel..."
            as_root dnf install -y gcc-c++ make libX11-devel
            ;;
        pacman)
            info "Memasang base-devel dan libx11..."
            as_root pacman -S --needed --noconfirm base-devel libx11
            ;;
        zypper)
            info "Memasang gcc-c++, make, dan libX11-devel..."
            as_root zypper --non-interactive install gcc-c++ make libX11-devel
            ;;
        apk)
            info "Memasang build-base dan libx11-dev..."
            as_root apk add --no-cache build-base libx11-dev
            ;;
        *)
            die "Manajer paket tidak dikenali. Pasang manual: compiler C++17, make, dan header X11."
            ;;
    esac
}

update_desktop_database() {
    if command -v update-desktop-database >/dev/null 2>&1; then
        as_priv "$APPS_DIR" update-desktop-database "$APPS_DIR" >/dev/null 2>&1 ||
            true
    fi
}

if [ "$UNINSTALL" -eq 1 ]; then
    info "Menghapus Aetheria dari $PREFIX..."
    if [ -e "$BIN_DIR/rpgcpp" ]; then
        as_priv "$BIN_DIR" rm -f "$BIN_DIR/rpgcpp"
        ok "Biner dihapus: $BIN_DIR/rpgcpp"
    else
        warn "Biner tidak ditemukan: $BIN_DIR/rpgcpp"
    fi
    if [ -e "$DESKTOP_FILE" ]; then
        as_priv "$DESKTOP_FILE" rm -f "$DESKTOP_FILE"
        update_desktop_database
        ok "Launcher dihapus: $DESKTOP_FILE"
    else
        warn "Launcher tidak ditemukan: $DESKTOP_FILE"
    fi
    ok "Instalasi dihapus. Hasil build dan berkas simpanan tidak disentuh."
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
    if [ -z "$CXX_BIN" ] || ! command -v make >/dev/null 2>&1 || ! has_x11_headers; then
        die "Dependensi masih belum lengkap setelah pemasangan. Periksa keluaran di atas."
    fi
    ok "Dependensi siap."
fi

[ -f src/main.cpp ] || die "Berkas sumber tidak ditemukan. Jalankan skrip ini dari folder proyek."
[ -f Makefile ] || die "Makefile tidak ditemukan. Jalankan skrip ini dari folder proyek."

if [ "$CLEAN_BUILD" -eq 1 ]; then
    info "Membersihkan hasil build sebelumnya..."
    make clean
fi

info "Membangun Aetheria dengan $CXX_BIN -j$JOBS..."
make -j"$JOBS" CXX="$CXX_BIN"
[ -x rpgcpp ] || die "Build selesai, tetapi biner rpgcpp tidak ditemukan."

info "Menjalankan uji asap headless..."
if ! ./rpgcpp --headless --frames 60 >/dev/null 2>&1; then
    die "Uji asap gagal. Coba ./rpgcpp --headless --frames 60 untuk melihat kesalahan."
fi
ok "Build dan uji asap berhasil."

if [ "$BUILD_ONLY" -eq 1 ]; then
    ok "Mode build-only: biner tersedia di $SCRIPT_DIR/rpgcpp"
    printf '  Jalankan: %s/rpgcpp\n' "$SCRIPT_DIR"
    exit 0
fi

info "Memasang biner ke $BIN_DIR..."
as_priv "$BIN_DIR" install -d "$BIN_DIR"
as_priv "$BIN_DIR" install -m 0755 rpgcpp "$BIN_DIR/rpgcpp"
ok "Biner terpasang: $BIN_DIR/rpgcpp"

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
Exec="$BIN_DIR/rpgcpp"
TryExec=$BIN_DIR/rpgcpp
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

if ! "$BIN_DIR/rpgcpp" --headless --frames 30 >/dev/null 2>&1; then
    warn "Biner terpasang, tetapi uji asap dari lokasi instalasi gagal."
else
    ok "Uji asap biner terpasang berhasil."
fi

printf '\n%sAetheria siap dimainkan.%s\n' "$C_GREEN" "$C_RESET"
printf '  Biner    : %s\n' "$BIN_DIR/rpgcpp"
printf '  Launcher : %s\n' "$DESKTOP_FILE"
case ":${PATH:-}:" in
    *":$BIN_DIR:"*)
        printf '  Jalankan : rpgcpp\n'
        ;;
    *)
        printf '  Jalankan : %s\n' "$BIN_DIR/rpgcpp"
        printf '\n  %s belum ada di PATH. Tambahkan baris berikut ke ~/.profile:%s\n' \
            "$BIN_DIR" "$C_RESET"
        printf '  export PATH="%s:$PATH"\n' "$BIN_DIR"
        ;;
esac
printf '  Simpanan : savegame.dat di direktori kerja (opsional: --save PATH)\n'
