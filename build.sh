#!/bin/bash
# tcmg - Multi-Platform Build System v2.0
# 41 targets · 12 platform groups · Pure C11
#
# Usage:
#   ./build.sh                         Interactive menu
#   ./build.sh --all                   Build all available targets
#   ./build.sh linux_x64 rpi_aarch64   Build specific targets
#   ./build.sh --group 3               Build a full group (e.g. Raspberry Pi)
#   ./build.sh --list                  List all targets with status
#   ./build.sh --install               Install missing apt compilers
#   ./build.sh --src /path/to/tcmg    Set source directory
#   ./build.sh --out /tmp/out          Set output directory
#   ./build.sh --help                  Show help
#
# Optional build.conf (same directory):
#   COPT=-O2         Optimization level (default: -Os)
#   MAX_JOBS=4       Parallel compile jobs (default: nproc)
#   UPX_ARGS=--best  UPX compression (default: --best)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${TCMG_SRC:-$SCRIPT_DIR}"
BUILD_ROOT="$SCRIPT_DIR/obj"
BUILD_OK_DIR="$SCRIPT_DIR/bin"
TC_ROOT="$SCRIPT_DIR/toolchains"
MASTER_LOG="$SCRIPT_DIR/build.log"
BATCH_MODE=false
BINARY_NAME="tcmg"

# Load external config
COPT="${COPT:--Os}"
MAX_JOBS="${MAX_JOBS:-$(nproc 2>/dev/null || echo 2)}"
UPX_ARGS="${UPX_ARGS:---best}"
[[ -f "$SCRIPT_DIR/build.conf" ]] && source "$SCRIPT_DIR/build.conf"

# Source files
SOURCES=(
    tcmg.c
    tcmg-log.c
    tcmg-crypto.c
    tcmg-net.c
    tcmg-ban.c
    tcmg-conf.c
    tcmg-emu.c
    tcmg-srvid2.c
    tcmg-webif.c
)

# Base compiler flags (GCC 5+, C11)
BASE_CFLAGS=(
    -std=c11
    "$COPT"
    -ffunction-sections -fdata-sections
    -fmerge-all-constants -fno-ident
    -fstack-protector-strong
    -fvisibility=hidden
    -D_FORTIFY_SOURCE=2
    -Wall -Wextra -Wno-unused-parameter
    -Wmissing-prototypes -Wstrict-prototypes
)

# Legacy compiler flags (GCC < 5, C99) for old OE1.6 toolchains
LEGACY_CFLAGS=(
    -std=c99
    "$COPT"
    -ffunction-sections -fdata-sections
    -fmerge-all-constants -fno-ident
    -fvisibility=hidden
    -Wall -Wno-unused-parameter
)

# Colors
RED='\033[0;31m';   GREEN='\033[0;32m';  YELLOW='\033[1;33m'
CYAN='\033[0;36m';  BLUE='\033[0;34m';   MAGENTA='\033[0;35m'
BOLD='\033[1m';     DIM='\033[2m';        NC='\033[0m'

ok()   { echo -e "  ${GREEN}OK${NC} $*"; }
warn() { echo -e "  ${YELLOW}WARN${NC} $*"; }
err()  { echo -e "  ${RED}ERR${NC} $*" >&2; }
info() { echo -e "  ${CYAN}->${NC} $*"; }
mlog() { echo "$*" >> "$MASTER_LOG" 2>/dev/null || true; }

# Cross-toolchain registry
# _reg NAME DESC CC URL MD5 ARCH_FLAGS EXTRA_CF LDFLAGS STRIP TYPE APT LEGACY NO_SYSROOT EXT
#
#  LEGACY=1     -> use LEGACY_CFLAGS (old GCC, max C99)
#  NO_SYSROOT=1 -> skip sysroot detection (avoid path issues)
#  EXT          -> output file extension (empty default, .exe for Windows)

declare -A TC_CC TC_URL TC_MD5 TC_DESC TC_ARCH_FLAGS
declare -A TC_EXTRA_CFLAGS TC_LDFLAGS TC_STRIP TC_TYPE TC_APT
declare -A TC_LEGACY TC_NO_SYSROOT TC_EXT
declare -A TC_CONFDIR

_reg() {
    local name="$1"  desc="$2"    cc="$3"    url="$4"
    local md5="$5"   archf="$6"   xcf="$7"   ldf="$8"
    local strip="$9" type="${10}" apt="${11:-}" legacy="${12:-0}"
    local nosys="${13:-0}" ext="${14:-}"
    TC_CC[$name]="$cc";             TC_URL[$name]="$url"
    TC_MD5[$name]="$md5";           TC_DESC[$name]="$desc"
    TC_ARCH_FLAGS[$name]="$archf";  TC_EXTRA_CFLAGS[$name]="$xcf"
    TC_LDFLAGS[$name]="$ldf";       TC_STRIP[$name]="$strip"
    TC_TYPE[$name]="$type";         TC_APT[$name]="$apt"
    TC_LEGACY[$name]="$legacy";     TC_NO_SYSROOT[$name]="$nosys"
    TC_EXT[$name]="$ext"
}

BASE_URL="https://simplebuild.dedyn.io/toolchains"
BOOTLIN="https://toolchains.bootlin.com/downloads/releases/toolchains"
PVER="0.26.2"

# Group 1 - Local Linux
_reg "linux_x64" \
    "Linux x86-64 (native)" \
    "gcc" "" "" \
    "-march=x86-64 -mtune=generic" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "strip" "linux" "build-essential" "0" "0" ""

_reg "linux_x86" \
    "Linux x86-32" \
    "gcc" "" "" \
    "-march=i686 -m32" "-I/usr/include/i386-linux-gnu" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all" \
    "strip" "linux" "gcc-multilib" "0" "1" ""

# Group 2 - Windows (via MinGW-w64)
# Note: -lpthread first to link winpthreads before ws2_32
_reg "windows_x64" \
    "Windows x86-64" \
    "x86_64-w64-mingw32-gcc" "" "" \
    "-march=x86-64 -mtune=generic" \
    "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601" \
    "-lpthread -lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -Wl,--gc-sections -Wl,--strip-all" \
    "x86_64-w64-mingw32-strip" "windows" "mingw-w64" "0" "0" ".exe"

_reg "windows_x86" \
    "Windows x86-32" \
    "i686-w64-mingw32-gcc" "" "" \
    "-march=i686 -mtune=generic" \
    "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -m32" \
    "-lpthread -lws2_32 -ladvapi32 -lbcrypt -static -static-libgcc -Wl,--gc-sections -Wl,--strip-all" \
    "i686-w64-mingw32-strip" "windows" "mingw-w64" "0" "0" ".exe"

# Group 3 - Raspberry Pi
_reg "rpi_armv6" \
    "Raspberry Pi 1 / Zero / Zero W" \
    "armv6-rpi1-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-rpi_armv6.tar.xz" \
    "f38a134e7b868a495e19c070f0758a32" \
    "-march=armv6 -mfpu=vfp -mfloat-abi=hard" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "armv6-rpi1-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "rpi_armv7" \
    "Raspberry Pi 2 Model B (Cortex-A7)" \
    "armv7-rpi2-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-rpi_armv7.tar.xz" \
    "481a328043fe65cca003f9ac48972243" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard -mtune=cortex-a7" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "armv7-rpi2-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "rpi_armv8" \
    "Raspberry Pi 3 / 4 (32-bit, Cortex-A53)" \
    "armv8-rpi3-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-rpi_armv8.tar.xz" \
    "a466111ea2207ef867eaf02cefeb4f78" \
    "-march=armv8-a -mfpu=neon-fp-armv8 -mfloat-abi=hard -mtune=cortex-a53" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "armv8-rpi3-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "rpi_aarch64" \
    "Raspberry Pi 3 / 4 / 5 (64-bit)" \
    "aarch64-rpi3-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-rpi_aarch64.tar.xz" \
    "60ca857fcde9eb94a99402db8ac34c37" \
    "-march=armv8-a -mtune=cortex-a53" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "aarch64-rpi3-linux-gnu-strip" "linux" "gcc-aarch64-linux-gnu"

# Group 4 - Dreambox
_reg "dream_arm" \
    "Dreambox DM900/DM920 UHD (Cortex-A15)" \
    "arm-dreambox-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-dream_arm.tar.xz" \
    "d0cb74426ebfb020ab531550f5c6412d" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard -mtune=cortex-a15" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "arm-dreambox-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

# dream_mipsel: LEGACY (GCC 13 OE1.6) + NO_SYSROOT (libatomic in internal GCC paths)
_reg "dream_mipsel" \
    "Dreambox DM500HD / 52x / 8x0HD / 70x0HD (MIPSEL)" \
    "mipsel-dreambox-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-dream_mipsel.tar.xz" \
    "731671ac09cbed5910e00b19c243218f" \
    "-march=mips32 -mabi=32 -EL -msoft-float -fno-tree-vectorize" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "mipsel-dreambox-linux-gnu-strip" "linux" "gcc-mipsel-linux-gnu" "1" "1" ""

_reg "dream_aarch64" \
    "Dreambox DM920 / DM7080 HD (AArch64)" \
    "aarch64-dreambox-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-dream_aarch64.tar.xz" \
    "1a2bb4d8c63dd1014f66f4136d258b65" \
    "-march=armv8-a -mtune=cortex-a53" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "aarch64-dreambox-linux-gnu-strip" "linux" "gcc-aarch64-linux-gnu"

# Group 5 - Vu+
_reg "vuplus4k_arm" \
    "Vu+ 4K Solo / Duo / Ultimo (ARMv7 hard-float)" \
    "arm-vuplus4k-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-vuplus4k_arm.tar.xz" \
    "de5e60f0a028353504c9ed314def15ba" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "arm-vuplus4k-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "vuplus4k_armv7" \
    "Vu+ Zero / Uno / Duo / Solo / Ultimo 4K (Cortex-A15)" \
    "armv7-vuplus4k-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-vuplus4k_armv7.tar.xz" \
    "00091f77312cdbb9d2e4173d77bd9e4d" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard -mtune=cortex-a15" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "armv7-vuplus4k-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

# Group 6 - Synology NAS
_reg "synology_aarch64" \
    "Synology DS418 / DS920+ (AArch64, Cortex-A53)" \
    "aarch64-ds418-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-synology_aarch64.tar.xz" \
    "c6f691c9904fc19509fa6766d3d62784" \
    "-march=armv8-a -mtune=cortex-a53" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "aarch64-ds418-linux-gnu-strip" "linux" "gcc-aarch64-linux-gnu"

# synology_armv7: LEGACY + NO_SYSROOT (sysroot missing stubs-soft.h)
_reg "synology_armv7" \
    "Synology DS216play / DS115j (ARMv7 softfp, Cortex-A9)" \
    "arm-ds216_play-linux-gnueabi-gcc" \
    "$BASE_URL/$PVER/Toolchain-synology_armv7.tar.xz" \
    "2a79d278f8096b146c045dbd9bf880fc" \
    "-march=armv7-a -mfloat-abi=softfp -mtune=cortex-a9" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all" \
    "arm-ds216_play-linux-gnueabi-strip" "linux" "gcc-arm-linux-gnueabi" "1" "1"

_reg "synology_armv7a" \
    "Synology DS218j / DS216j (Marvell Armada, ARMv7a softfp)" \
    "arm-marvell-linux-gnueabi-gcc" \
    "$BASE_URL/$PVER/Toolchain-synology_armv7a.tar.xz" \
    "8b706a2950f68a877f6f4cafa022d954" \
    "-march=armv7-a -mfpu=vfpv3 -mfloat-abi=softfp" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "arm-marvell-linux-gnueabi-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "synology_armv5" \
    "Synology DS109 / DS110j (ARMv5te soft-float)" \
    "arm-synology-linux-gnueabi-gcc" \
    "$BASE_URL/$PVER/Toolchain-synology_armv5.tar.xz" \
    "9512d405a1fda034bd8ce9d15ffac13e" \
    "-march=armv5te -mfloat-abi=soft" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "arm-synology-linux-gnueabi-strip" "linux" "gcc-arm-linux-gnueabi"

_reg "synology_ppc" \
    "Synology DS213+ / DS411+ (PowerPC e500v2 SPE)" \
    "powerpc-ds213+-linux-gnuspe-gcc" \
    "$BASE_URL/$PVER/Toolchain-synology_ppc.tar.xz" \
    "a93f537210ee7a4158eb73a3b048f65d" \
    "-mcpu=8548 -mabi=spe -mspe -mfloat-gprs=double" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "powerpc-ds213+-linux-gnuspe-strip" "linux" "gcc-powerpc-linux-gnu"

# Group 7 - QNAP NAS
_reg "qnap_armv5" \
    "QNAP TS-109 / TS-209 (ARMv5te soft-float)" \
    "arm-qnap-linux-gnueabi-gcc" \
    "$BASE_URL/$PVER/Toolchain-qnap_armv5.tar.xz" \
    "ac3b6d54a43cff0229658665211024af" \
    "-march=armv5te -mfloat-abi=soft" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "arm-qnap-linux-gnueabi-strip" "linux" "gcc-arm-linux-gnueabi"

_reg "qnap_armv7" \
    "QNAP TS-x51 / TS-x53 / TS-231+ (ARMv7 hard)" \
    "armv7-qnap-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-qnap_armv7.tar.xz" \
    "74d1ab5da80d30ab28622df7cb2d8dfe" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "armv7-qnap-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "qnap_x64" \
    "QNAP TS-x51+ / TS-x53 / TS-x73 (x86-64)" \
    "x86_64-qnap-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-qnap_x64.tar.xz" \
    "0a0c8a8bf7660d6390eeba9d0642e68b" \
    "-march=x86-64 -mtune=generic" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "x86_64-qnap-linux-gnu-strip" "linux" ""

# Group 8 - OpenWrt (routers)
_reg "owrt_ar71xx_mips" \
    "OpenWrt ar71xx MIPS BE - Atheros AR71xx/AR93xx" \
    "mips-ar71xx-linux-musl-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_ar71xx_mips.tar.xz" \
    "6db39b16d4cef95f9312e67210a519b8" \
    "-march=mips32r2 -mabi=32 -EB -mtune=24kc" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "mips-ar71xx-linux-musl-strip" "linux" "gcc-mips-linux-gnu"

_reg "owrt_ath79_mips" \
    "OpenWrt ath79 MIPS BE - Qualcomm Atheros QCA95xx" \
    "mips-ath79-linux-musl-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_ath79_mips.tar.xz" \
    "478bafe9143e176cdfca4e7b25cfcacd" \
    "-march=mips32r2 -mabi=32 -EB -mtune=24kc" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "mips-ath79-linux-musl-strip" "linux" "gcc-mips-linux-gnu"

_reg "owrt_ramips_mips" \
    "OpenWrt ramips MIPSEL LE - MediaTek MT7620/MT7621/MT76xx" \
    "mipsel-ramips-linux-musl-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_ramips_mips.tar.xz" \
    "af6c03c9fa8b4e9c765a5757e83be5af" \
    "-march=mips32r2 -mabi=32 -EL" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "mipsel-ramips-linux-musl-strip" "linux" "gcc-mipsel-linux-gnu"

_reg "owrt_rpi_armv6" \
    "OpenWrt - Raspberry Pi ARMv6 (bcm2708)" \
    "armv6-rpi1-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_rpi_armv6.tar.xz" \
    "6886bc5d3791f342bd48d00c16fa9f4e" \
    "-march=armv6 -mfpu=vfp -mfloat-abi=hard" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "armv6-rpi1-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "owrt_rpi_armv7" \
    "OpenWrt - Raspberry Pi ARMv7 (bcm2709)" \
    "armv7-rpi2-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_rpi_armv7.tar.xz" \
    "cf6098fe66b8c28ab3348785e813706a" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all" \
    "armv7-rpi2-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "owrt_mediatek_armv8" \
    "OpenWrt - MediaTek AArch64 (GL.iNet/MT7622/MT7981)" \
    "aarch64-mediatek-linux-musl-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_mediatek_armv8.tar.xz" \
    "e4869d80b549c0415b52a74545065c54" \
    "-march=armv8-a -mtune=cortex-a53" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all" \
    "aarch64-mediatek-linux-musl-strip" "linux" "gcc-aarch64-linux-gnu"

_reg "owrt_kirkwood_arm" \
    "OpenWrt - Marvell Kirkwood ARMv5 (WD MyCloud, NAS)" \
    "arm-kirkwood-linux-musleabi-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_kirkwood_arm.tar.xz" \
    "573b19f8d477f895e46fcae1d6601e08" \
    "-march=armv5te -mfloat-abi=soft -mtune=marvell-pj4" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "arm-kirkwood-linux-musleabi-strip" "linux" "gcc-arm-linux-gnueabi"

_reg "owrt_mpc85xx_ppc" \
    "OpenWrt - Freescale MPC85xx PowerPC BE" \
    "powerpc-mpc85xx-linux-musl-gcc" \
    "$BASE_URL/$PVER/Toolchain-owrt_mpc85xx_ppc.tar.xz" \
    "48e1a43265ce8fc40901eb585ac7363f" \
    "-mcpu=8548" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "powerpc-mpc85xx-linux-musl-strip" "linux" "gcc-powerpc-linux-gnu"

# Group 9 - Ubiquiti
_reg "ubnt_aarch64" \
    "Ubiquiti UniFi Dream Machine / UDM Pro (Cortex-A57)" \
    "aarch64-ubnt-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-ubnt_aarch64.tar.xz" \
    "da8c9e4c4d9b01958165ce6063bc8a94" \
    "-march=armv8-a -mtune=cortex-a57" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "aarch64-ubnt-linux-gnu-strip" "linux" "gcc-aarch64-linux-gnu"

# ubnt_mips64: NO_SYSROOT (old glibc issue in sysroot)
_reg "ubnt_mips64" \
    "Ubiquiti EdgeRouter Lite / ER-X (MIPS64 Octeon)" \
    "mips-ubnt-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-ubnt_mips64.tar.xz" \
    "a8be217a49ca688051ae1c5d166260d3" \
    "-march=mips64r2 -mabi=64" "-U_FORTIFY_SOURCE" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all" \
    "mips-ubnt-linux-gnu-strip" "linux" "gcc-mips64-linux-gnuabi64" "0" "1"

# Group 10 - Generic STB devices (Bootlin)
_reg "aarch64_generic" \
    "Generic AArch64 STBs (Amlogic, HiSilicon, Rockchip)" \
    "aarch64-buildroot-linux-gnu-gcc" \
    "$BOOTLIN/aarch64/tarballs/aarch64--glibc--stable-2018.11-1.tar.bz2" \
    "3dabef3ced22e58bc061be2f74dfae85" \
    "-march=armv8-a" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "aarch64-buildroot-linux-gnu-strip" "linux" "gcc-aarch64-linux-gnu"

_reg "armv7_generic" \
    "Generic ARMv7 STBs (Cortex-A7/A9/A15 hard-float)" \
    "arm-buildroot-linux-gnueabihf-gcc" \
    "$BOOTLIN/armv7-eabihf/tarballs/armv7-eabihf--glibc--stable-2018.11-1.tar.bz2" \
    "cf6286d326c14d6a51ba05c33c8218a0" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "arm-buildroot-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "mipsel_generic" \
    "Generic MIPSEL 32-bit LE STBs (old satellite devices)" \
    "mipsel-buildroot-linux-gnu-gcc" \
    "$BOOTLIN/mips32el/tarballs/mips32el--glibc--stable-2018.11-1.tar.bz2" \
    "6c1170df27d57dd3bbff9456f0afa813" \
    "-march=mips32r2 -mabi=32 -EL" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "mipsel-buildroot-linux-gnu-strip" "linux" "gcc-mipsel-linux-gnu"

_reg "powerpc_generic" \
    "Generic PowerPC STBs (e500mc, cable devices)" \
    "powerpc-buildroot-linux-gnu-gcc" \
    "$BOOTLIN/powerpc-e500mc/tarballs/powerpc-e500mc--glibc--stable-2018.11-1.tar.bz2" \
    "23439204ec3952cf122d9f748e966004" \
    "-mcpu=e500mc" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "powerpc-buildroot-linux-gnu-strip" "linux" "gcc-powerpc-linux-gnu"

# Group 11 - OpenEmbedded 2.0 (various STB devices)
_reg "oe20_sh4" \
    "OE2.0 SH4 - Vu+ SH4 / Dreambox DM500 / AZbox" \
    "sh4-oe20-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-oe20_sh4.tar.xz" \
    "f7fa9ee20c8e2c1f7fd9b533b49a1a27" \
    "" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "sh4-oe20-linux-gnu-strip" "linux" "gcc-sh4-linux-gnu"

_reg "oe20_armv7" \
    "OE2.0 ARMv7 - Cortex-A9 devices (Vu+ UNO 4K SE, Xtrend)" \
    "armv7-oe20-linux-gnueabihf-gcc" \
    "$BASE_URL/$PVER/Toolchain-oe20_armv7.tar.xz" \
    "d1fec214ee48bbecb0041949431d5c41" \
    "-march=armv7-a -mfpu=neon -mfloat-abi=hard -mtune=cortex-a9" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "armv7-oe20-linux-gnueabihf-strip" "linux" "gcc-arm-linux-gnueabihf"

_reg "oe20_mipsel" \
    "OE2.0 MIPSEL - various MIPSEL devices" \
    "mipsel-oe20-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-oe20_mipsel.tar.xz" \
    "3f14f79c3a78ce5f1f21480a71422c7f" \
    "-march=mips32r2 -mabi=32 -EL" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "mipsel-oe20-linux-gnu-strip" "linux" "gcc-mipsel-linux-gnu"

# Group 12 - Other / Legacy
_reg "igel_x86" \
    "Igel VIA VX855 x86 (thin client PCs)" \
    "i686-igel-linux-gnu-gcc" \
    "$BASE_URL/$PVER/Toolchain-igel_x86.tar.xz" \
    "ac2e27c58889862ee921653c9164aad1" \
    "-march=i686 -m32 -mtune=c7" "" \
    "-lpthread -Wl,--gc-sections -Wl,--strip-all -Wl,-z,relro -Wl,-z,now" \
    "i686-igel-linux-gnu-strip" "linux" "gcc-i686-linux-gnu"

_reg "pogoplug_arm" \
    "Pogoplug v2/v3 / Dockstar (ARMv5te XScale)" \
    "arm-pogoplug-linux-gnueabi-gcc" \
    "$BASE_URL/$PVER/Toolchain-pogoplug_arm.tar.xz" \
    "ab560b964a9d07dadacc2a2d40e7967e" \
    "-march=armv5te -mfloat-abi=soft -mtune=xscale" "" \
    "-lpthread -latomic -Wl,--gc-sections -Wl,--strip-all" \
    "arm-pogoplug-linux-gnueabi-strip" "linux" "gcc-arm-linux-gnueabi"

# mipsel_oe16: GCC 4.x Legacy - OE1.6 Dreambox/Vu+ (very old)
# Needs -lrt: clock_gettime was not in libc.so until glibc 2.17; GCC 4.3.2
# ships with glibc ~2.7 where it lives in librt.
_reg "mipsel_oe16" \
    "Dreambox/Vu+ OE1.6 GCC4 Legacy (old MIPSEL)" \
    "mipsel-unknown-linux-gnu-gcc" \
    "$BASE_URL/legacy/Toolchain-MIPS-Tuxbox-OE1.6_s_u_p.tar.xz" \
    "09cfbf19e4f4e7c1a2c033dd12a09b4a" \
    "-march=mips32r2 -mabi=32 -EL" "" \
    "-lpthread -lrt -Wl,--gc-sections -Wl,--strip-all" \
    "mipsel-unknown-linux-gnu-strip" "linux" "gcc-mipsel-linux-gnu" "1" "0"

# Config directory per target (compile-time default, like OSCam CONF_DIR)
TC_CONFDIR["rpi_armv6"]="/etc/tcmg"
TC_CONFDIR["rpi_armv7"]="/etc/tcmg"
TC_CONFDIR["rpi_armv8"]="/etc/tcmg"
TC_CONFDIR["rpi_aarch64"]="/etc/tcmg"
TC_CONFDIR["dream_arm"]="/etc/tuxbox/config"
TC_CONFDIR["dream_mipsel"]="/etc/tuxbox/config"
TC_CONFDIR["dream_aarch64"]="/etc/tuxbox/config"
TC_CONFDIR["vuplus4k_arm"]="/etc/tuxbox/config"
TC_CONFDIR["vuplus4k_armv7"]="/etc/tuxbox/config"
TC_CONFDIR["qnap_armv5"]="/etc/config/tcmg"
TC_CONFDIR["qnap_armv7"]="/etc/config/tcmg"
TC_CONFDIR["qnap_x64"]="/etc/config/tcmg"
TC_CONFDIR["owrt_ar71xx_mips"]="/etc/tcmg"
TC_CONFDIR["owrt_ath79_mips"]="/etc/tcmg"
TC_CONFDIR["owrt_ramips_mips"]="/etc/tcmg"
TC_CONFDIR["owrt_rpi_armv6"]="/etc/tcmg"
TC_CONFDIR["owrt_rpi_armv7"]="/etc/tcmg"
TC_CONFDIR["owrt_mediatek_armv8"]="/etc/tcmg"
TC_CONFDIR["owrt_kirkwood_arm"]="/usr/tcmg"
TC_CONFDIR["owrt_mpc85xx_ppc"]="/etc/tcmg"
TC_CONFDIR["ubnt_aarch64"]="/etc/tcmg"
TC_CONFDIR["ubnt_mips64"]="/etc/tcmg"
TC_CONFDIR["aarch64_generic"]="/var/tuxbox/config"
TC_CONFDIR["armv7_generic"]="/var/tuxbox/config"
TC_CONFDIR["mipsel_generic"]="/var/tuxbox/config"
TC_CONFDIR["powerpc_generic"]="/var/tuxbox/config"
TC_CONFDIR["oe20_sh4"]="/var/tuxbox/config"
TC_CONFDIR["oe20_armv7"]="/var/tuxbox/config"
TC_CONFDIR["oe20_mipsel"]="/var/tuxbox/config"
TC_CONFDIR["pogoplug_arm"]="/usr/tcmg"
TC_CONFDIR["mipsel_oe16"]="/var/tuxbox/config"

# Group definitions and build order
GROUP_NAMES=(
    "Local Linux"
    "Windows"
    "Raspberry Pi"
    "Dreambox"
    "Vu+"
    "Synology NAS"
    "QNAP NAS"
    "OpenWrt"
    "Ubiquiti"
    "Generic STB"
    "OpenEmbedded 2.0"
    "Other / Legacy"
)

GROUP_TARGETS_ARR=(
    "linux_x64 linux_x86"
    "windows_x64 windows_x86"
    "rpi_armv6 rpi_armv7 rpi_armv8 rpi_aarch64"
    "dream_arm dream_mipsel dream_aarch64"
    "vuplus4k_arm vuplus4k_armv7"
    "synology_aarch64 synology_armv7 synology_armv7a synology_armv5 synology_ppc"
    "qnap_armv5 qnap_armv7 qnap_x64"
    "owrt_ar71xx_mips owrt_ath79_mips owrt_ramips_mips owrt_rpi_armv6 owrt_rpi_armv7 owrt_mediatek_armv8 owrt_kirkwood_arm owrt_mpc85xx_ppc"
    "ubnt_aarch64 ubnt_mips64"
    "aarch64_generic armv7_generic mipsel_generic powerpc_generic"
    "oe20_sh4 oe20_armv7 oe20_mipsel"
    "igel_x86 pogoplug_arm mipsel_oe16"
)

# Build order: native first, then by popularity
ALL_TARGETS=(
    linux_x64 linux_x86
    windows_x64 windows_x86
    rpi_armv6 rpi_armv7 rpi_armv8 rpi_aarch64
    dream_arm dream_mipsel dream_aarch64
    vuplus4k_arm vuplus4k_armv7
    synology_aarch64 synology_armv7 synology_armv7a synology_armv5 synology_ppc
    qnap_armv5 qnap_armv7 qnap_x64
    ubnt_aarch64 ubnt_mips64
    aarch64_generic armv7_generic mipsel_generic
    owrt_ar71xx_mips owrt_ath79_mips owrt_ramips_mips
    owrt_rpi_armv6 owrt_rpi_armv7 owrt_mediatek_armv8
    owrt_kirkwood_arm owrt_mpc85xx_ppc
    oe20_sh4 oe20_armv7 oe20_mipsel
    powerpc_generic
    igel_x86 pogoplug_arm mipsel_oe16
)

# UPX binary compression
UPX_CMD=""
find_upx() {
    command -v upx &>/dev/null && UPX_CMD="upx" && return
    [[ -x "$SCRIPT_DIR/upx" ]] && UPX_CMD="$SCRIPT_DIR/upx" && return
}

apply_upx() {
    local bin="$1" type="$2" name="$3"
    [[ -z "$UPX_CMD" ]] && return 0
    [[ "$type" == "windows" ]] && return 0    # MinGW + UPX can be problematic
    [[ "$name" == *"sh4"* ]]   && return 0    # SH4 not supported by UPX
    [[ "$name" == *"mips64"* ]] && return 0   # MIPS64 not supported
    local before after
    before=$(stat -c%s "$bin" 2>/dev/null || echo 0)
    echo -ne "  ${DIM}UPX:${NC} ... "
    if "$UPX_CMD" $UPX_ARGS --no-color --quiet "$bin" 2>/dev/null; then
        after=$(stat -c%s "$bin" 2>/dev/null || echo 0)
        local pct=0
        [[ $before -gt 0 ]] && pct=$(( (before - after) * 100 / before ))
        echo -e "${GREEN}${before} -> ${after} bytes (${pct}% smaller)${NC}"
    else
        echo -e "${YELLOW}skipped${NC}"
    fi
}

# Check compiler availability
compiler_available() {
    local name="$1"
    local cc="${TC_CC[$name]}"

    # linux_x86: gcc always present but need 32-bit support (gcc-multilib)
    if [[ "$name" == "linux_x86" ]]; then
        command -v gcc &>/dev/null || return 1
        echo 'int main(){}' | gcc -m32 -x c - -o /tmp/._tcmg_m32test 2>/dev/null \
            && rm -f /tmp/._tcmg_m32test && return 0
        return 1
    fi

    # Search in local toolchain directory first
    local url="${TC_URL[$name]:-}"
    if [[ -n "$url" ]]; then
        local bn; bn=$(basename "$url" | sed 's/\.tar\.\(xz\|bz2\|gz\)$//')
        local tc_dir="$TC_ROOT/$bn"
        if [[ -d "$tc_dir" ]]; then
            find "$tc_dir" -name "$cc" \( -type f -o -type l \) -perm /111 2>/dev/null | grep -q . && return 0
        fi
    fi

    # Then check PATH
    command -v "$cc" &>/dev/null
}

# Download and install toolchain
ensure_compiler() {
    local t="$1"
    local url="${TC_URL[$t]:-}"
    local cc="${TC_CC[$t]}"
    local apt_pkg="${TC_APT[$t]:-}"

    compiler_available "$t" && return 0

    # No URL -> try apt
    if [[ -z "$url" ]]; then
        [[ -n "$apt_pkg" ]] || { err "No compiler or package for: $t"; return 1; }
        warn "Package required: $apt_pkg"
        if $BATCH_MODE; then
            _apt_install "$apt_pkg" && compiler_available "$t" && return 0
        else
            echo -ne "  Install now? [Y/n] "; read -r ans
            [[ "${ans,,}" == "n" ]] && return 1
            _apt_install "$apt_pkg" && compiler_available "$t" && return 0
        fi
        return 1
    fi

    # Download toolchain
    local fn; fn=$(basename "$url")
    local bn
    bn=$(basename "$url" | sed 's/\.tar\.\(xz\|bz2\|gz\)$//')
    local tc_dir="$TC_ROOT/$bn"
    local cache="$TC_ROOT/.cache/$fn"

    mkdir -p "$tc_dir" "$TC_ROOT/.cache"

    if [[ ! -f "$cache" ]]; then
        info "Downloading: ${BOLD}$bn${NC}"
        mlog "=== Download: $t === URL: $url"
        if ! curl -fL --progress-bar --retry 3 --connect-timeout 30 -o "$cache" "$url" 2>&1; then
            rm -f "$cache"
            err "Download failed: $t"
            [[ -n "$apt_pkg" ]] && warn "Alternative: apt-get install $apt_pkg" \
                && $BATCH_MODE && _apt_install "$apt_pkg" && compiler_available "$t" && return 0
            return 1
        fi
    else
        info "Using cache: $fn"
    fi

    # Verify MD5
    local expected_md5="${TC_MD5[$t]:-}"
    if [[ -n "$expected_md5" ]]; then
        local actual_md5; actual_md5=$(md5sum "$cache" | cut -d' ' -f1)
        if [[ "$actual_md5" != "$expected_md5" ]]; then
            warn "MD5 mismatch for $t (${actual_md5} != ${expected_md5}) - re-download needed"
            rm -f "$cache"
            err "Delete cache and retry: $cache"
            return 1
        fi
    fi

    # Extract (Bootlin needs --strip-components=1)
    info "Extracting: $bn"
    local strip=0
    [[ "$url" == *"bootlin.com"* ]] && strip=1

    case "$fn" in
        *.tar.xz)  tar -xJf "$cache" -C "$tc_dir" --strip-components=$strip 2>/dev/null ;;
        *.tar.bz2) tar -xjf "$cache" -C "$tc_dir" --strip-components=$strip 2>/dev/null ;;
        *.tar.gz)  tar -xzf "$cache" -C "$tc_dir" --strip-components=$strip 2>/dev/null ;;
        *) err "Unknown archive format: $fn"; rm -f "$cache"; return 1 ;;
    esac

    # Add bin directory to PATH
    local bin_dir
    bin_dir=$(find "$tc_dir" -maxdepth 4 -name "$cc" \( -type f -o -type l \) -perm /111 2>/dev/null \
               | head -1 | xargs dirname 2>/dev/null || true)
    [[ -n "$bin_dir" ]] && export PATH="$bin_dir:$PATH"

    if compiler_available "$t"; then
        patch_sysroot_stubs "$tc_dir"
        ok "Toolchain ready: $bn"
        mlog "Toolchain ready: $t -> $tc_dir"
        return 0
    fi

    err "Compiler '$cc' not found after extraction"
    return 1
}

_apt_install() {
    local pkg="$1"; [[ -z "$pkg" ]] && return 1
    command -v apt-get &>/dev/null || { warn "apt-get not available"; return 1; }
    local sudo_=""
    [[ $EUID -ne 0 ]] && sudo_="sudo"
    $sudo_ apt-get update -qq 2>/dev/null || true
    $sudo_ apt-get install -y --fix-missing $pkg 2>&1 | grep -E "^(Inst|Setting up)" || true
}

# Resolve the absolute path to the compiler for a specific target.
# Searches the target's own toolchain directory FIRST to avoid using a
# different toolchain that happens to share the same binary name
# (e.g. rpi_armv6 vs owrt_rpi_armv6 both use armv6-rpi1-linux-gnueabihf-gcc).
# Falls back to PATH only for native compilers (no URL).
resolve_cc_path() {
    local t="$1"
    local cc="${TC_CC[$t]}"
    local url="${TC_URL[$t]:-}"

    if [[ -n "$url" ]]; then
        local bn; bn=$(basename "$url" | sed 's/\.tar\.\(xz\|bz2\|gz\)$//')
        local tc_dir="$TC_ROOT/$bn"
        if [[ -d "$tc_dir" ]]; then
            local found
            found=$(find "$tc_dir" -name "$cc" \( -type f -o -type l \) -perm /111 2>/dev/null | head -1)
            [[ -n "$found" ]] && { echo "$found"; return 0; }
        fi
    fi

    # Native/apt compiler: look up in PATH
    command -v "$cc" 2>/dev/null && return 0
    return 1
}

# Detect sysroot directory
# Patch broken sysroots: scan all gnu/stubs.h files in the toolchain and
# create any referenced stub files that are missing (e.g. stubs-soft.h,
# stubs-n64_soft.h).  Fixes synology_armv7 and ubnt_mips64.
patch_sysroot_stubs() {
    local tc_dir="$1"
    while IFS= read -r -d '' stubsh; do
        local dir; dir=$(dirname "$stubsh")
        while IFS= read -r missing_name; do
            local target="$dir/$missing_name"
            [[ -f "$target" ]] || touch "$target" 2>/dev/null || true
        done < <(grep -o 'stubs-[a-z0-9_]*\.h' "$stubsh" 2>/dev/null | sort -u)
    done < <(find "$tc_dir" -name "stubs.h" -path "*/gnu/stubs.h" -print0 2>/dev/null)
}

find_sysroot() {
    local t="$1"
    # Targets marked NO_SYSROOT=1 are skipped entirely
    [[ "${TC_NO_SYSROOT[$t]:-0}" == "1" ]] && return

    # Use resolve_cc_path so we look in THIS target's toolchain directory,
    # not whichever toolchain happens to be first in PATH.
    local full
    full=$(resolve_cc_path "$t") || return
    [[ -z "$full" ]] && return

    local dir; dir=$(dirname "$(dirname "$full")")
    local cc_host="${cc%-gcc}"
    local candidates=(
        "$dir/sysroot"
        "$dir/$cc_host/sysroot"
        "$dir/$cc_host/libc"
    )
    for c in "${candidates[@]}"; do
        [[ -d "$c/usr/include" ]] && { echo "$c"; return; }
    done
}

# Build a single target
build_target() {
    local t="$1"
    local cc="${TC_CC[$t]}"
    local ext="${TC_EXT[$t]:-.elf}"
    local is_legacy="${TC_LEGACY[$t]:-0}"

    local obj_dir="$BUILD_ROOT/$t"
    local out_bin="$BUILD_OK_DIR/${t}_${BINARY_NAME}${ext}"
    local log_file="$obj_dir/build.log"

    mkdir -p "$obj_dir" "$BUILD_OK_DIR"

    # Resolve the absolute path to THIS target's compiler.
    # Using the full path prevents accidental use of a different toolchain
    # that shares the same binary name (e.g. rpi_armv6 vs owrt_rpi_armv6).
    local cc_path
    cc_path=$(resolve_cc_path "$t") || {
        err "Compiler binary not found for $t: $cc"
        return 1
    }

    # Always patch missing sysroot stub headers for this toolchain.
    # patch_sysroot_stubs is also called after download, but toolchains that
    # were already on disk from a previous run never went through ensure_compiler,
    # so they need it here too. The function is cheap (touch on missing files).
    local _tc_dir
    local _url="${TC_URL[$t]:-}"
    if [[ -n "$_url" ]]; then
        local _bn; _bn=$(basename "$_url" | sed 's/\.tar\.\(xz\|bz2\|gz\)$//')
        _tc_dir="$TC_ROOT/$_bn"
        [[ -d "$_tc_dir" ]] && patch_sysroot_stubs "$_tc_dir"
    fi

    # Select flag set
    local FLAGS=()
    [[ "$is_legacy" == "1" ]] && FLAGS=("${LEGACY_CFLAGS[@]}") || FLAGS=("${BASE_CFLAGS[@]}")

    # Architecture flags
    local arch="${TC_ARCH_FLAGS[$t]:-}"
    [[ -n "$arch" ]] && read -ra _af <<< "$arch" && FLAGS+=("${_af[@]}")

    # Extra flags
    local xcf="${TC_EXTRA_CFLAGS[$t]:-}"
    [[ -n "$xcf" ]] && read -ra _xf <<< "$xcf" && FLAGS+=("${_xf[@]}")

    # Config directory — set at compile time (identical to OSCam CONF_DIR)
    # Only inject for embedded targets; Windows and generic Linux use header defaults
    local confdir="${TC_CONFDIR[$t]:-}"
    [[ -n "$confdir" ]] && FLAGS+=("-DCS_CONFDIR=\"$confdir\"")

    # Detect sysroot
    local sysroot; sysroot=$(find_sysroot "$t")
    [[ -n "$sysroot" ]] && FLAGS+=("--sysroot=$sysroot")

    local LDFLAGS="${TC_LDFLAGS[$t]:-}"
    : > "$log_file"

    local step="  ${BOLD}${CYAN}[$t]${NC}"
    echo -e "$step Compiling ${#SOURCES[@]} files ..."
    mlog ""; mlog "=== TARGET: $t ==="
    mlog "CC: $cc_path  FLAGS: ${FLAGS[*]}"

    # Compile step
    local objs=()
    local build_ok=true
    for src in "${SOURCES[@]}"; do
        local src_path="$SRC_DIR/$src"
        [[ -f "$src_path" ]] || { err "Missing file: $src_path"; return 1; }
        local obj="$obj_dir/${src%.c}.o"
        mkdir -p "$(dirname "$obj")"
        "$cc_path" "${FLAGS[@]}" -c "$src_path" -o "$obj" >> "$log_file" 2>&1 || {
            err "Failed: $src"
            tail -5 "$log_file" >&2
            build_ok=false
            break
        }
        objs+=("$obj")
    done
    $build_ok || return 1

    # Link step
    echo -e "$step Linking ..."
    "$cc_path" "${FLAGS[@]}" "${objs[@]}" -o "$out_bin" $LDFLAGS >> "$log_file" 2>&1 || {
        err "Link failed"
        tail -8 "$log_file" >&2
        return 1
    }

    # Strip — use the strip tool from the same toolchain directory
    local strip_cmd="${TC_STRIP[$t]:-strip}"
    local strip_full
    strip_full="$(dirname "$cc_path")/$strip_cmd"
    if [[ -x "$strip_full" ]]; then
        "$strip_full" --strip-all "$out_bin" 2>/dev/null || true
    elif command -v "$strip_cmd" &>/dev/null; then
        "$strip_cmd" --strip-all "$out_bin" 2>/dev/null || true
    fi

    # UPX
    apply_upx "$out_bin" "${TC_TYPE[$t]}" "$t"

    local sz; sz=$(du -sh "$out_bin" 2>/dev/null | cut -f1)
    ok "$t -> ${BINARY_NAME}${ext}  [$sz]"
    mlog "  OK -> $out_bin ($sz)"
    return 0
}

# Run builds for a list of targets
run_builds() {
    local targets=("$@")
    local total=${#targets[@]}
    [[ $total -eq 0 ]] && return

    mkdir -p "$BUILD_OK_DIR"
    {   echo "============================================"
        echo "  Build Log - $(date '+%Y-%m-%d %H:%M:%S')"
        echo "  Targets: $total"
        echo "============================================"
    } >> "$MASTER_LOG"

    local t_start=$SECONDS
    local passed=0 failed=0 skipped=0
    local failed_list=() skipped_list=()

    # Phase 1 (batch mode): prepare all compilers first
    if $BATCH_MODE; then
        echo
        echo -e "  ${BOLD}[Phase 1/2]${NC} Preparing toolchains..."
        local need_apt=""
        for t in "${targets[@]}"; do
            compiler_available "$t" && continue
            local url="${TC_URL[$t]:-}"
            local apt="${TC_APT[$t]:-}"
            [[ -n "$apt" && -z "$url" && " $need_apt " != *" $apt "* ]] && need_apt+=" $apt"
        done
        if [[ -n "${need_apt// }" ]]; then
            info "Installing: $need_apt"
            _apt_install "$need_apt"
        fi
        for t in "${targets[@]}"; do
            compiler_available "$t" && continue
            [[ -z "${TC_URL[$t]:-}" ]] && continue
            echo -ne "  ${DIM}Downloading${NC} ${BOLD}$t${NC} ... "
            ensure_compiler "$t" 2>/dev/null && echo -e "${GREEN}ready${NC}" || echo -e "${RED}failed${NC}"
        done
        echo
        echo -e "  ${BOLD}[Phase 2/2]${NC} Building ..."
    fi

    # Build loop
    local idx=0
    for t in "${targets[@]}"; do
        idx=$((idx + 1))
        echo
        echo -e "  ${DIM}[$idx/$total]${NC}"

        if ! compiler_available "$t"; then
            if ! ensure_compiler "$t"; then
                warn "Skipping $t - no compiler"
                skipped=$((skipped + 1)); skipped_list+=("$t"); continue
            fi
        fi

        if build_target "$t"; then
            passed=$((passed + 1))
        else
            failed=$((failed + 1)); failed_list+=("$t")
        fi
    done

    local elapsed=$((SECONDS - t_start))

    # Summary
    echo
    echo -e "  ${BOLD}============================================${NC}"
    printf "  %b%-10s%b %d   %b%-10s%b %d   %b%s%b %d   %bTime: %ds%b\n" \
        "$GREEN" "Passed:"  "$NC" "$passed" \
        "$RED"   "Failed:"  "$NC" "$failed" \
        "$DIM"   "Skipped:" "$NC" "$skipped" \
        "$DIM" "$elapsed" "$NC"
    if [[ $failed -gt 0 ]]; then
        echo -e "  ${RED}Failed targets:${NC}"
        for f in "${failed_list[@]}"; do echo -e "    ${RED}x${NC} $f"; done
    fi
    [[ $skipped -gt 0 ]] && echo -e "  ${DIM}No compiler: ${skipped_list[*]}${NC}"
    echo -e "  ${CYAN}Binaries: $BUILD_OK_DIR${NC}"
    echo -e "  ${CYAN}Log:      $MASTER_LOG${NC}"
    echo -e "  ${BOLD}============================================${NC}"
    echo
    {   echo ""
        echo "SUMMARY: passed=$passed failed=$failed skipped=$skipped time=${elapsed}s"
        [[ $failed  -gt 0 ]] && echo "Failed:  ${failed_list[*]}"
        [[ $skipped -gt 0 ]] && echo "Skipped: ${skipped_list[*]}"
        echo "============================================"
    } >> "$MASTER_LOG"
}

# List all targets
list_all_targets() {
    echo
    echo -e "  ${BOLD}All available targets (${#ALL_TARGETS[@]} total):${NC}"
    echo
    printf "  %-26s %-6s %-8s  %s\n" "Target" "Ready" "Type" "Description"
    echo -e "  ${DIM}--------------------------------------------------------------${NC}"
    for i in "${!GROUP_NAMES[@]}"; do
        echo -e "\n  ${BOLD}${BLUE}-- ${GROUP_NAMES[$i]} --${NC}"
        read -ra gt <<< "${GROUP_TARGETS_ARR[$i]}"
        for t in "${gt[@]}"; do
            local rdy leg=""
            compiler_available "$t" && rdy="${GREEN}yes${NC}" || rdy="${RED}no ${NC}"
            [[ "${TC_LEGACY[$t]:-0}" == "1" ]] && leg="${DIM}[c99]${NC}"
            printf "  %-26s " "$t"
            echo -ne "$rdy  $leg"
            printf "    %s\n" "${TC_DESC[$t]:-}"
        done
    done
    echo
}

# Install missing packages
install_missing() {
    echo; echo -e "  ${BOLD}Checking for missing compilers...${NC}"; echo
    declare -A needed
    for t in "${ALL_TARGETS[@]}"; do
        compiler_available "$t" && continue
        local pkg="${TC_APT[$t]:-}" url="${TC_URL[$t]:-}"
        [[ -n "$pkg" && -z "$url" ]] && needed["$pkg"]=1
    done
    if [[ ${#needed[@]} -eq 0 ]]; then
        ok "All system compilers are present."
        echo -e "  ${DIM}(Cross-toolchains are downloaded automatically when needed)${NC}"
        echo; return
    fi
    echo -e "  ${YELLOW}Missing packages:${NC}"
    for p in "${!needed[@]}"; do echo -e "    ${YELLOW}->$NC} $p"; done
    echo
    echo -ne "  Install all? [Y/n] "; read -r ans
    [[ "${ans,,}" == "n" ]] && return
    _apt_install "${!needed[*]}"
    ok "Done."
}

# Verify source files
verify_sources() {
    local missing=()
    for src in "${SOURCES[@]}"; do
        [[ -f "$SRC_DIR/$src" ]] || missing+=("$src")
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        err "Missing source files in: $SRC_DIR"
        for f in "${missing[@]}"; do err "  $f"; done
        echo
        err "Set source path: TCMG_SRC=/path/to/tcmg ./build.sh"
        exit 1
    fi
}

# Print banner
print_banner() {
    clear 2>/dev/null || true
    local ver=""
    ver=$(grep 'TCMG_VERSION' "$SRC_DIR/tcmg-globals.h" 2>/dev/null \
        | grep -o '"[0-9.]*"' | tr -d '"' | head -1) || true
    echo -e "\n  ${BOLD}${CYAN}tcmg ${ver:-?} - Multi-Platform Build System${NC}"
    echo -e "  ${DIM}Pure C11 · $(date '+%H:%M:%S') · ${#ALL_TARGETS[@]} targets · 12 groups${NC}"
    echo -e "  ${DIM}Source: $SRC_DIR${NC}"
    echo -e "  ${DIM}Output: $BUILD_OK_DIR${NC}"
    [[ -n "$UPX_CMD" ]] \
        && echo -e "  ${GREEN}[UPX]${NC} ${DIM}Compression enabled ($UPX_ARGS)${NC}" \
        || echo -e "  ${YELLOW}[UPX]${NC} ${DIM}Not found - sudo apt install upx-ucl${NC}"
    echo
}

# Group submenu
group_submenu() {
    local gidx="$1"
    local gname="${GROUP_NAMES[$gidx]}"
    read -ra gtargets <<< "${GROUP_TARGETS_ARR[$gidx]}"
    while true; do
        print_banner
        echo -e "  ${BOLD}${BLUE}-- $gname --${NC}"; echo
        for i in "${!gtargets[@]}"; do
            local t="${gtargets[$i]}"
            local icon color
            if compiler_available "$t"; then
                icon="*" color="$GREEN"
            elif [[ -n "${TC_URL[$t]:-}" ]]; then
                icon="o" color="$YELLOW"
            else
                icon="x" color="$RED"
            fi
            local leg=""
            [[ "${TC_LEGACY[$t]:-0}" == "1" ]] && leg=" ${DIM}[c99]${NC}"
            printf "  ${BOLD}[%2d]${NC}  ${color}%s${NC}  %-28s  ${DIM}%s${NC}%b\n" \
                $((i+1)) "$icon" "$t" "${TC_DESC[$t]:-}" "$leg"
        done
        echo
        echo -e "  ${DIM}* ready   o needs download   x needs install${NC}"
        echo -e "  ${DIM}Enter numbers (e.g. 1 3 4)  or:  a = all  b = back${NC}"
        echo
        echo -ne "  > "; read -r sel; sel="${sel,,}"
        [[ "$sel" == "b" || -z "$sel" ]] && return
        if [[ "$sel" == "a" ]]; then
            BATCH_MODE=true; run_builds "${gtargets[@]}"; BATCH_MODE=false
            echo -ne "  Press Enter..."; read -r; return
        fi
        local chosen=()
        for token in $sel; do
            if [[ "$token" =~ ^[0-9]+$ ]]; then
                local idx=$((token - 1))
                [[ $idx -ge 0 && $idx -lt ${#gtargets[@]} ]] \
                    && chosen+=("${gtargets[$idx]}") || warn "Invalid number: $token"
            fi
        done
        if [[ ${#chosen[@]} -gt 0 ]]; then
            run_builds "${chosen[@]}"
            echo -ne "  Press Enter..."; read -r
        fi
    done
}

# Interactive mode
interactive_mode() {
    verify_sources
    while true; do
        print_banner
        echo -e "  ${BOLD}Select a group:${NC}"; echo
        for i in "${!GROUP_NAMES[@]}"; do
            read -ra gt <<< "${GROUP_TARGETS_ARR[$i]}"
            local ready=0
            for t in "${gt[@]}"; do compiler_available "$t" && ready=$((ready+1)); done
            printf "  ${BOLD}${CYAN}[%2d]${NC}  %-18s  ${DIM}%d/%d ready${NC}\n" \
                $((i+1)) "${GROUP_NAMES[$i]}" "$ready" "${#gt[@]}"
        done
        echo
        echo -e "  ${BOLD}${GREEN}[ a]${NC}  Build all (${#ALL_TARGETS[@]} targets)"
        echo -e "  ${BOLD}${YELLOW}[ i]${NC}  Install missing compilers"
        echo -e "  ${BOLD}${MAGENTA}[ l]${NC}  List all targets"
        echo -e "  ${BOLD}${RED}[ q]${NC}  Quit"
        echo
        echo -ne "  > "; read -r choice; choice="${choice,,}"
        case "$choice" in
            q|quit|exit) echo; echo "  Goodbye."; echo; exit 0 ;;
            a|all)
                BATCH_MODE=true; verify_sources; run_builds "${ALL_TARGETS[@]}"; BATCH_MODE=false
                echo -ne "  Press Enter..."; read -r ;;
            i|install) install_missing; echo -ne "  Press Enter..."; read -r ;;
            l|list)    list_all_targets; echo -ne "  Press Enter..."; read -r ;;
            [0-9]*)
                local gidx=$((choice - 1))
                if [[ $gidx -ge 0 && $gidx -lt ${#GROUP_NAMES[@]} ]]; then
                    group_submenu "$gidx"
                else
                    warn "Invalid group: $choice (1-${#GROUP_NAMES[@]})"
                    sleep 1
                fi ;;
            *) warn "Unknown option"; sleep 1 ;;
        esac
    done
}

# Help
usage() {
    echo
    echo -e "  ${BOLD}tcmg Build System v2.0${NC} - ${#ALL_TARGETS[@]} targets · 12 groups"
    echo
    echo "  Usage: $0 [options] [targets...]"
    echo
    echo "  Options:"
    echo "    --all            Build all available targets"
    echo "    --group N        Build all targets in group N"
    echo "    --list           List all targets with status"
    echo "    --install        Install missing compilers"
    echo "    --src DIR        Source files directory"
    echo "    --tc  DIR        Toolchains directory"
    echo "    --out DIR        Output directory"
    echo "    --help           Show this help"
    echo
    echo "  Examples:"
    echo "    $0 linux_x64 rpi_aarch64"
    echo "    $0 --group 4"
    echo "    $0 --all"
    echo
    echo "  Groups:"
    for i in "${!GROUP_NAMES[@]}"; do
        read -ra gt <<< "${GROUP_TARGETS_ARR[$i]}"
        printf "    [%2d] %-18s (%d targets)\n" $((i+1)) "${GROUP_NAMES[$i]}" "${#gt[@]}"
    done
    echo
}

# Main
main() {
    local build_all=false do_list=false do_install=false
    local cli_targets=() cli_group=""
    BATCH_MODE=false

    find_upx

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --all)     build_all=true ;;
            --group)   cli_group="$2"; shift ;;
            --list)    do_list=true ;;
            --install) do_install=true ;;
            --src)     SRC_DIR="$2"; shift ;;
            --tc)      TC_ROOT="$2"; shift ;;
            --out)     BUILD_ROOT="$2"; BUILD_OK_DIR="$2/bin"
                       MASTER_LOG="$2/build.log"; shift ;;
            --help|-h) usage; exit 0 ;;
            -*)        err "Unknown option: $1"; usage; exit 1 ;;
            *)         cli_targets+=("$1") ;;
        esac
        shift
    done

    $do_install && { install_missing; exit 0; }
    $do_list    && { list_all_targets; exit 0; }

    if [[ -n "$cli_group" ]]; then
        local gidx=$((cli_group - 1))
        if [[ $gidx -ge 0 && $gidx -lt ${#GROUP_NAMES[@]} ]]; then
            verify_sources
            read -ra gt <<< "${GROUP_TARGETS_ARR[$gidx]}"
            BATCH_MODE=true; run_builds "${gt[@]}"; exit 0
        else
            err "Invalid group: $cli_group (1-${#GROUP_NAMES[@]})"; exit 1
        fi
    fi

    if $build_all; then
        verify_sources; BATCH_MODE=true; run_builds "${ALL_TARGETS[@]}"; exit 0
    fi

    if [[ ${#cli_targets[@]} -gt 0 ]]; then
        verify_sources
        local valid=()
        for t in "${cli_targets[@]}"; do
            [[ -v TC_CC[$t] ]] && valid+=("$t") || warn "Unknown target: $t"
        done
        [[ ${#valid[@]} -gt 0 ]] \
            && { BATCH_MODE=true; run_builds "${valid[@]}"; } \
            || { err "No valid targets."; exit 1; }
        exit 0
    fi

    interactive_mode
}

main "$@"
