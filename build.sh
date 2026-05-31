#!/bin/bash
# =============================================================================
# build.sh — tcmg build script
# Works on: Linux, macOS, Git Bash (Windows), MSYS2, Cygwin
#
# Usage:
#   ./build.sh              Build for current platform (auto-detect)
#   ./build.sh linux        Build Linux x64
#   ./build.sh windows      Cross-compile Windows x64
#   ./build.sh clean        Remove build/ directory
#   ./build.sh --help
# =============================================================================

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'
ok()   { echo -e "  ${GREEN}OK${NC}  $*"; }
err()  { echo -e "  ${RED}ERR${NC} $*" >&2; }
info() { echo -e "  ${CYAN}-->${NC} $*"; }
warn() { echo -e "  ${YELLOW}WARN${NC} $*"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR/build"
OBJ_DIR="$BUILD_DIR/obj"

SRCS="tcmg.c tcmg-log.c tcmg-crypto.c tcmg-net.c tcmg-ban.c tcmg-conf.c \
      tcmg-emu.c tcmg-srvid2.c tcmg-webif.c tcmg-webif-common.c \
      tcmg-webif-layout.c tcmg-webif-pages.c tcmg-webif-tvcas.c"

VERSION=$(grep -oP '#define\s+TCMG_VERSION\s+"\K[^"]+' "$SRC_DIR/tcmg-globals.h" 2>/dev/null || echo "dev")

# ---------------------------------------------------------------------------
# Detect if running on Windows (Git Bash / MSYS2 / Cygwin)
# ---------------------------------------------------------------------------
ON_WINDOWS=0
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) ON_WINDOWS=1 ;;
esac

# ---------------------------------------------------------------------------
check_src() {
    if [[ ! -f "$SRC_DIR/tcmg.c" ]]; then
        err "src/tcmg.c not found. Run from the project root."
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# Find gcc — searches PATH then common Windows install locations
# ---------------------------------------------------------------------------
find_gcc() {
    local want_triple="$1"   # e.g. x86_64-w64-mingw32-gcc or gcc

    # Try exact name in PATH
    if command -v "$want_triple" &>/dev/null; then
        echo "$want_triple"; return
    fi

    # On Windows: scan common install dirs
    if [[ $ON_WINDOWS -eq 1 ]]; then
        local dirs=(
            "/ucrt64/bin"
            "/mingw64/bin"
            "/mingw32/bin"
            "/c/msys64/ucrt64/bin"
            "/c/msys64/mingw64/bin"
            "/c/msys2/ucrt64/bin"
            "/c/msys2/mingw64/bin"
            "/c/mingw64/bin"
            "/c/mingw32/bin"
            "/c/MinGW/bin"
            "/c/TDM-GCC-64/bin"
            "$HOME/scoop/apps/mingw/current/bin"
        )
        for d in "${dirs[@]}"; do
            if [[ -x "$d/gcc.exe" || -x "$d/gcc" ]]; then
                export PATH="$d:$PATH"
                echo "$d/gcc"; return
            fi
        done
    fi

    echo ""
}

# ---------------------------------------------------------------------------
# Build using gcc directly (no make required — works in Git Bash)
# ---------------------------------------------------------------------------
build_direct() {
    local CC="$1"
    local OUT="$2"
    local CFLAGS="$3"
    local LDFLAGS="$4"
    local STRIP_CMD="$5"

    check_src
    mkdir -p "$OBJ_DIR"

    echo ""
    info "Compiler : $CC"
    "$CC" --version 2>/dev/null | head -1
    echo ""
    info "Building tcmg v${VERSION} → $OUT"
    echo ""

    local OBJS=()
    for src in $SRCS; do
        local obj="$OBJ_DIR/${src%.c}.o"
        echo "  CC   src/$src"
        "$CC" $CFLAGS -c "$SRC_DIR/$src" -o "$obj"
        OBJS+=("$obj")
    done

    echo ""
    echo "  LINK $OUT"
    "$CC" $CFLAGS "${OBJS[@]}" -o "$OUT" $LDFLAGS

    if [[ -n "$STRIP_CMD" ]] && command -v "$STRIP_CMD" &>/dev/null; then
        "$STRIP_CMD" --strip-all "$OUT" 2>/dev/null || true
    fi

    local sz
    sz=$(du -sh "$OUT" 2>/dev/null | cut -f1)
    echo ""
    ok "Built: $OUT  ($sz)"
}

# ---------------------------------------------------------------------------
build_windows_native() {
    # Running ON Windows (Git Bash / MSYS2) — compile natively with mingw gcc
    info "Windows native build  (tcmg v${VERSION})"

    local GCC
    GCC=$(find_gcc "gcc")
    if [[ -z "$GCC" ]]; then
        err "MinGW gcc not found."
        echo ""
        echo "  Install options:"
        echo "    1) WinLibs (easiest): https://winlibs.com"
        echo "       Unzip to C:\\mingw64 then restart Git Bash."
        echo "    2) In MSYS2 terminal: pacman -S mingw-w64-ucrt-x86_64-gcc"
        echo "    3) Scoop: scoop install mingw"
        echo "    4) Choco: choco install mingw"
        exit 1
    fi

    local STRIP_CMD
    STRIP_CMD="$(dirname "$GCC")/strip"
    [[ -x "${STRIP_CMD}.exe" ]] && STRIP_CMD="${STRIP_CMD}.exe"

    local CFLAGS="-std=c11 -Os -ffunction-sections -fdata-sections \
        -fmerge-all-constants -fno-ident -fstack-protector-strong \
        -march=x86-64 -mtune=generic \
        -I$SRC_DIR \
        -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_FORTIFY_SOURCE=2 \
        -Wall -Wextra -Wno-unused-parameter \
        -Wmissing-prototypes -Wstrict-prototypes"

    local LDFLAGS="-lws2_32 -ladvapi32 -lbcrypt \
        -static -static-libgcc -lpthread \
        -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none"

    mkdir -p "$BUILD_DIR"
    build_direct "$GCC" "$BUILD_DIR/tcmg_x64.exe" "$CFLAGS" "$LDFLAGS" "$STRIP_CMD"
}

build_linux_native() {
    info "Linux x64 build  (tcmg v${VERSION})"

    local GCC
    GCC=$(find_gcc "gcc")
    if [[ -z "$GCC" ]]; then
        err "gcc not found. Install with: sudo apt install gcc"
        exit 1
    fi

    local CFLAGS="-std=c11 -Os -ffunction-sections -fdata-sections \
        -fmerge-all-constants -fno-ident -fstack-protector-strong \
        -march=x86-64 -mtune=generic \
        -I$SRC_DIR \
        -D_FORTIFY_SOURCE=2 \
        -Wall -Wextra -Wno-unused-parameter \
        -Wmissing-prototypes -Wstrict-prototypes"

    local LDFLAGS="-lpthread -lm \
        -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none"

    mkdir -p "$BUILD_DIR"
    build_direct "$GCC" "$BUILD_DIR/tcmg" "$CFLAGS" "$LDFLAGS" "strip"
}

build_windows_cross() {
    # Running on Linux — cross compile for Windows
    info "Windows x64 cross-build  (tcmg v${VERSION})"

    local GCC
    GCC=$(find_gcc "x86_64-w64-mingw32-gcc")
    if [[ -z "$GCC" ]]; then
        err "MinGW cross-compiler not found."
        echo "  Install: sudo apt install mingw-w64"
        exit 1
    fi

    local CFLAGS="-std=c11 -Os -ffunction-sections -fdata-sections \
        -fmerge-all-constants -fno-ident -fstack-protector-strong \
        -march=x86-64 -mtune=generic \
        -I$SRC_DIR \
        -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_FORTIFY_SOURCE=2 \
        -Wall -Wextra -Wno-unused-parameter \
        -Wmissing-prototypes -Wstrict-prototypes"

    local LDFLAGS="-lws2_32 -ladvapi32 -lbcrypt \
        -static -static-libgcc -lpthread \
        -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none"

    mkdir -p "$BUILD_DIR"
    build_direct "$GCC" "$BUILD_DIR/tcmg_x64.exe" "$CFLAGS" "$LDFLAGS" "x86_64-w64-mingw32-strip"
}

do_clean() {
    info "Cleaning $BUILD_DIR ..."
    rm -rf "$BUILD_DIR"
    ok "Done"
}

show_help() {
    echo ""
    echo "  tcmg build script — v${VERSION}"
    echo ""
    echo "  Usage: ./build.sh [target]"
    echo ""
    echo "  Targets:"
    echo "    (none)    Auto-detect platform and build"
    echo "    linux     Build Linux x64"
    echo "    windows   Build Windows x64 (cross or native)"
    echo "    clean     Remove build/ directory"
    echo ""
    echo "  Works in: Linux, Git Bash, MSYS2, Cygwin"
    echo "  Output  : build/tcmg  or  build/tcmg_x64.exe"
    echo ""
}

# ---------------------------------------------------------------------------
TARGET="${1:-auto}"

case "$TARGET" in
    auto)
        if [[ $ON_WINDOWS -eq 1 ]]; then
            build_windows_native
        else
            build_linux_native
        fi
        ;;
    linux)
        build_linux_native ;;
    windows|win)
        if [[ $ON_WINDOWS -eq 1 ]]; then
            build_windows_native
        else
            build_windows_cross
        fi
        ;;
    clean)          do_clean ;;
    --help|-h|help) show_help ;;
    *)
        err "Unknown target: $TARGET"
        show_help
        exit 1
        ;;
esac
