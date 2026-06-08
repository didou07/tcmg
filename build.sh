#!/bin/bash
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; NC='\033[0m'
ok()   { echo -e "  ${GREEN}OK${NC}  $*"; }
err()  { echo -e "  ${RED}ERR${NC} $*" >&2; }
info() { echo -e "  ${CYAN}-->${NC} $*"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
OBJ_DIR="$BUILD_DIR/obj"

ALL_SRCS="\
tcmg-globals.c \
tcmg-main.c \
tcmg-client.c \
tcmg-log.c \
tcmg-conf.c \
tcmg-failban.c \
tcmg-emu.c \
tcmg-srvid.c \
tcmg-net.c \
tcmg-platform.c \
cscrypt/crypto.c \
module-cccam.c \
module-newcamd.c \
webif/webif.c \
webif/webif-common.c \
webif/webif-layout.c \
webif/webif-page-login.c \
webif/webif-page-status.c \
webif/webif-page-users.c \
webif/webif-page-system.c \
webif/webif-api.c \
webif/webif-tvcas.c"

VERSION=$(grep -oP '#define\s+TCMG_VERSION\s+"\K[^"]+' "$SCRIPT_DIR/globals.h" 2>/dev/null || echo "dev")

ON_WINDOWS=0
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) ON_WINDOWS=1 ;; esac

check_src() {
    [[ -f "$SCRIPT_DIR/tcmg-main.c" ]] || { err "tcmg-main.c not found. Run from project root."; exit 1; }
}

regen_assets() {
    info "Regenerating webif/webif_assets.h ..."
    python3 "$SCRIPT_DIR/tools/gen_assets.py"
    ok "webif/webif_assets.h updated"
}

find_gcc() {
    local want_triple="$1"
    command -v "$want_triple" &>/dev/null && { echo "$want_triple"; return; }
    if [[ $ON_WINDOWS -eq 1 ]]; then
        local dirs=(
            "/ucrt64/bin" "/mingw64/bin" "/mingw32/bin"
            "/c/msys64/ucrt64/bin" "/c/msys64/mingw64/bin"
            "/c/mingw64/bin" "/c/MinGW/bin" "/c/TDM-GCC-64/bin"
            "$HOME/scoop/apps/mingw/current/bin"
        )
        for d in "${dirs[@]}"; do
            if [[ -x "$d/gcc.exe" || -x "$d/gcc" ]]; then
                export PATH="$d:$PATH"; echo "$d/gcc"; return
            fi
        done
    fi
    echo ""
}

build_direct() {
    local CC="$1" OUT="$2" CFLAGS="$3" LDFLAGS="$4" STRIP_CMD="$5"
    check_src
    regen_assets
    mkdir -p "$OBJ_DIR"
    echo ""
    info "Compiler : $CC"
    "$CC" --version 2>/dev/null | head -1
    echo ""
    info "Building tcmg v${VERSION} → $OUT"
    echo ""
    local OBJS=()
    for src in $ALL_SRCS; do
        local flat="${src//\//__}"
        local obj="$OBJ_DIR/${flat%.c}.o"
        echo "  CC   $src"
        "$CC" $CFLAGS -c "$SCRIPT_DIR/$src" -o "$obj"
        OBJS+=("$obj")
    done
    echo ""
    echo "  LINK $OUT"
    "$CC" $CFLAGS "${OBJS[@]}" -o "$OUT" $LDFLAGS
    if [[ -n "$STRIP_CMD" ]] && command -v "$STRIP_CMD" &>/dev/null; then
        "$STRIP_CMD" --strip-all "$OUT" 2>/dev/null || true
    fi
    local sz; sz=$(du -sh "$OUT" 2>/dev/null | cut -f1)
    echo ""
    ok "Built: $OUT  ($sz)"
}

COMMON_FLAGS="-std=c11 -Os -ffunction-sections -fdata-sections \
    -fmerge-all-constants -fno-ident -fstack-protector-strong \
    -march=x86-64 -mtune=generic \
    -I$SCRIPT_DIR \
    -D_FORTIFY_SOURCE=2 \
    -Wall -Wextra -Wno-unused-parameter \
    -Wno-overlength-strings -Wno-format"

build_linux_native() {
    info "Linux x64 build  (tcmg v${VERSION})"
    local GCC; GCC=$(find_gcc "gcc")
    [[ -n "$GCC" ]] || { err "gcc not found. Install: sudo apt install gcc"; exit 1; }
    mkdir -p "$BUILD_DIR"
    build_direct "$GCC" "$BUILD_DIR/tcmg" \
        "$COMMON_FLAGS" \
        "-lpthread -lm -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none" \
        "strip"
}

build_windows_cross() {
    info "Windows x64 cross-build  (tcmg v${VERSION})"
    local GCC; GCC=$(find_gcc "x86_64-w64-mingw32-gcc")
    [[ -n "$GCC" ]] || { err "MinGW not found. Install: sudo apt install mingw-w64"; exit 1; }
    mkdir -p "$BUILD_DIR"
    build_direct "$GCC" "$BUILD_DIR/tcmg_x64.exe" \
        "$COMMON_FLAGS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601" \
        "-lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread \
         -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none" \
        "x86_64-w64-mingw32-strip"
}

build_windows_native() {
    info "Windows native build  (tcmg v${VERSION})"
    local GCC; GCC=$(find_gcc "gcc")
    [[ -n "$GCC" ]] || { err "MinGW gcc not found."; exit 1; }
    local STRIP_CMD="$(dirname "$GCC")/strip"
    mkdir -p "$BUILD_DIR"
    build_direct "$GCC" "$BUILD_DIR/tcmg_x64.exe" \
        "$COMMON_FLAGS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601" \
        "-lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -lpthread \
         -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none" \
        "$STRIP_CMD"
}

do_clean() { info "Cleaning $BUILD_DIR ..."; rm -rf "$BUILD_DIR"; ok "Done"; }

show_help() {
    echo ""
    echo "  tcmg build script — v${VERSION}"
    echo ""
    echo "  Usage: ./build.sh [target]"
    echo ""
    echo "  Targets:"
    echo "    (none)    Auto-detect platform and build"
    echo "    linux     Build Linux x64  → build/tcmg"
    echo "    windows   Build Windows x64 → build/tcmg_x64.exe"
    echo "    all       Build both platforms"
    echo "    assets    Regenerate webif/webif_assets.h"
    echo "    clean     Remove build/ directory"
    echo ""
}

TARGET="${1:-auto}"
case "$TARGET" in
    auto)
        if [[ $ON_WINDOWS -eq 1 ]]; then build_windows_native
        else build_linux_native; fi ;;
    linux)          build_linux_native ;;
    windows|win)
        if [[ $ON_WINDOWS -eq 1 ]]; then build_windows_native
        else build_windows_cross; fi ;;
    all)
        build_linux_native
        if [[ $ON_WINDOWS -eq 1 ]]; then build_windows_native
        else build_windows_cross; fi ;;
    assets)         regen_assets ;;
    clean)          do_clean ;;
    --help|-h|help) show_help ;;
    *) err "Unknown target: $TARGET"; show_help; exit 1 ;;
esac
