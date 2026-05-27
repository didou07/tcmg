#!/bin/bash
# =============================================================================
# build.sh — tcmg local build helper
#
# Usage:
#   ./build.sh                  Build Linux x64 (native)
#   ./build.sh linux            Build Linux x64
#   ./build.sh windows          Build Windows x64 (requires mingw-w64)
#   ./build.sh all              Build both Linux x64 and Windows x64
#   ./build.sh clean            Remove build output
#   ./build.sh --help
#
# GitHub Actions handles Android. Run this script only for local dev builds.
# =============================================================================

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
ok()   { echo -e "  ${GREEN}OK${NC}  $*"; }
err()  { echo -e "  ${RED}ERR${NC} $*" >&2; }
info() { echo -e "  ${CYAN}-->${NC} $*"; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"

# ---------------------------------------------------------------------------
# Read version
# ---------------------------------------------------------------------------
VERSION=$(grep -oP '#define\s+TCMG_VERSION\s+"\K[^"]+' "$SRC_DIR/tcmg-globals.h" 2>/dev/null || echo "dev")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
check_src() {
    if [[ ! -f "$SRC_DIR/tcmg.c" ]]; then
        err "src/tcmg.c not found. Run from the repository root."
        exit 1
    fi
}

build_linux() {
    info "Building Linux x64  (tcmg v${VERSION})"
    check_src
    make -C "$SCRIPT_DIR" RELEASE=1 -j"$(nproc)"
    ok "Output: $SCRIPT_DIR/tcmg"
}

build_windows() {
    info "Building Windows x64  (tcmg_${VERSION}.exe)"
    check_src
    if ! command -v x86_64-w64-mingw32-gcc &>/dev/null; then
        err "MinGW-w64 not found. Install with:  sudo apt install mingw-w64"
        exit 1
    fi
    make -C "$SCRIPT_DIR" RELEASE=1 \
        CC=x86_64-w64-mingw32-gcc \
        STRIP=x86_64-w64-mingw32-strip \
        -j"$(nproc)"
    ok "Output: $SCRIPT_DIR/tcmg.exe"
}

do_clean() {
    info "Cleaning build output"
    make -C "$SCRIPT_DIR" clean
    ok "Done"
}

show_help() {
    echo ""
    echo "  tcmg build helper — v${VERSION}"
    echo ""
    echo "  Usage: ./build.sh [target]"
    echo ""
    echo "  Targets:"
    echo "    linux     Build Linux x64 (default)"
    echo "    windows   Build Windows x64 (requires mingw-w64)"
    echo "    all       Build both"
    echo "    clean     Remove build output"
    echo ""
    echo "  Android is built exclusively via GitHub Actions."
    echo ""
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
TARGET="${1:-linux}"

case "$TARGET" in
    linux)          build_linux   ;;
    windows|win)    build_windows ;;
    all)            build_linux && build_windows ;;
    clean)          do_clean ;;
    --help|-h|help) show_help ;;
    *)
        err "Unknown target: $TARGET"
        show_help
        exit 1
        ;;
esac
