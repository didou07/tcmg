#!/usr/bin/env bash
set -Eeuo pipefail

# TCMG 5.9 - one-file device builder.
# Inspired by simplebuild4: each device is a complete target and owns
# its build options, toolchain, sysroot and persistent state.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="$ROOT_DIR/.tcmg-build"
TOOLCHAIN_DIR="$STATE_DIR/toolchains"
LOG_DIR="$STATE_DIR/logs"
VERSION="5.9"

[[ -f "$ROOT_DIR/Makefile" && -f "$ROOT_DIR/src/main.c" ]] || {
  echo "ERROR: run this script from the TCMG source tree" >&2
  exit 1
}

mkdir -p "$STATE_DIR" "$TOOLCHAIN_DIR" "$LOG_DIR"

# name | platform | arch | description | confdir | default_pcsc | toolchain-id
DEVICES=(
  'tubox|linux|native|Native Linux / Tuxbox-style config|/etc/tuxbox/config|off|native'
  'generic-linux|linux|native|Native Linux /usr/local/etc|/usr/local/etc|auto|native'
  'dev-box|linux|native|Native development box ./cfg|./cfg|auto|native'
  'vuplus-4k|linux|armv7|Vu+ 4K ARMv7 (SimpleBuild4 vuplus4k_armv7)|/etc/tuxbox/config|on|vuplus4k_armv7'
  'vuplus-4k-legacy|linux|armv7|Vu+ legacy ARMv7 (manual SDK)|/etc/tuxbox/config|on|manual'
  'dreambox-dm900|linux|armv7|Dreambox DM900 / DM920|/etc/tuxbox/config|on|cortexa15hf_opendreambox_krogoth'
  'dreambox-one|linux|aarch64|Dreambox ONE / TWO AArch64|/etc/tuxbox/config|on|aarch64_opendreambox_pyro'
  'dreambox-mipsel|linux|mipsel|Dreambox MIPSel (DM520/7080/820 toolchain)|/etc/tuxbox/config|on|mips32el_opendreambox_krogoth'
  'dreambox-dm520|linux|mipsel|Dreambox DM520|/etc/tuxbox/config|on|mips32el_opendreambox_krogoth'
  'dreambox-dm7080|linux|mipsel|Dreambox DM7080|/etc/tuxbox/config|on|mips32el_opendreambox_krogoth'
  'dreambox-dm820|linux|mipsel|Dreambox DM820|/etc/tuxbox/config|on|mips32el_opendreambox_krogoth'
  'dreambox-dm500hd|linux|mipsel|Dreambox DM500HD (legacy MIPSel)|/etc/tuxbox/config|on|dream_mipsel'
  'dreambox-dm800|linux|mipsel|Dreambox DM800 (legacy MIPSel)|/etc/tuxbox/config|on|dream_mipsel'
  'dreambox-dm7020hd|linux|mipsel|Dreambox DM7020HD (legacy MIPSel)|/etc/tuxbox/config|on|dream_mipsel'
  'dreambox-dm8000|linux|mipsel|Dreambox DM8000 (legacy MIPSel)|/etc/tuxbox/config|on|dream_mipsel'
  'dreambox-dm800se|linux|mipsel|Dreambox DM800SE (legacy MIPSel)|/etc/tuxbox/config|on|dream_mipsel'
  'bootlin-aarch64-generic|linux|aarch64|Generic AArch64 STB toolchain (Bootlin 2018.11)|/etc/tuxbox/config|on|bootlin_aarch64_2018'
  'bootlin-armv7-generic|linux|armv7|Generic ARMv7 STB toolchain (Bootlin 2018.11)|/etc/tuxbox/config|on|bootlin_armv7_2018'
  'bootlin-mipsel-generic|linux|mipsel|Generic MIPSel STB toolchain (Bootlin 2018.11)|/etc/tuxbox/config|on|bootlin_mipsel_2018'
  'bootlin-powerpc-generic|linux|powerpc|Generic PowerPC STB toolchain (Bootlin 2018.11)|/etc/tuxbox/config|on|bootlin_powerpc_2018'
  'windows-x64|windows|x86_64|Windows x64 MinGW|./|on|native'
  'macos-native|macos|native|Native macOS target|/usr/local/etc/tcmg|on|native'
)

# name | description | arch | url | sha256 | prefix | sysroot-rel | cflags | ldflags
TOOLCHAINS=(
  'vuplus4k_armv7|SimpleBuild4 Vu+ 4K ARMv7 toolchain|armv7|https://simplebuild.dedyn.io/toolchains/current/Toolchain-vuplus4k_armv7.tar.xz|8970a65246c85f59a445b91ab1d0251a3143ec18a914c5bb711c11a040841862|armv7-vuplus4k-linux-gnueabihf-|arm-vuplus4k-linux-gnueabihf/sysroot||'
  'cortexa15hf_opendreambox_krogoth|OpenDreambox Krogoth Cortex-A15|armv7|https://simplebuild.dedyn.io/toolchains/current/Toolchain-cortexa15hf_opendreambox_krogoth.tar.xz|238cf895f437624ecbe4940cd80932f17f74ed9916daf365900c895d243993c2|arm-oe-linux-gnueabi-|sysroots/cortexa15hf-neon-vfpv4-oe-linux-gnueabi|-march=armv7ve -mfpu=neon-vfpv4 -mfloat-abi=hard -mcpu=cortex-a15|-Wl,--dynamic-linker=/lib/ld-linux-armhf.so.3'
  'aarch64_opendreambox_pyro|OpenDreambox Pyro AArch64|aarch64|https://simplebuild.dedyn.io/toolchains/current/Toolchain-aarch64_opendreambox_pyro.tar.xz|7f41146c1f3023437602d0c80f4d9df4bf13b7e540de596f095a00c92619bdae|aarch64-oe-linux-|sysroots/aarch64-oe-linux||-Wl,--dynamic-linker=/lib/ld-linux-aarch64.so.1'
  'mips32el_opendreambox_krogoth|OpenDreambox Krogoth MIPSel|mipsel|https://simplebuild.dedyn.io/toolchains/current/Toolchain-mips32el_opendreambox_krogoth.tar.xz|72746de7ed005a6895f4795e66cebc4ebd6b9792b9eb8c0458389929f6666d89|mipsel-oe-linux-|sysroots/mips32el-oe-linux|-mel -mabi=32 -mhard-float -march=mips32|-Wl,--dynamic-linker=/lib/ld-2.23.so'
  'dream_mipsel|Dreambox legacy MIPSel (DM800/DM800SE/DM500HD/DM7020HD/DM8000)|mipsel|https://simplebuild.dedyn.io/toolchains/current/Toolchain-dream_mipsel.tar.xz|8e8c74ff8e2b9c729f54ec58a36ebd144adda9ba235ba8648eab627d1c21aab3|mipsel-dreambox-linux-gnu-|mipsel-dreambox-linux-gnu/sysroot||'
  'bootlin_aarch64_2018|Bootlin AArch64 glibc 2018.11|aarch64|https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--glibc--stable-2018.11-1.tar.bz2|abae0522480b9f37ff6cee4249e147e7cb78e1997cc6f76dba7e0fb8ec04221d||||'
  'bootlin_armv7_2018|Bootlin ARMv7 EABIhf glibc 2018.11|armv7|https://toolchains.bootlin.com/downloads/releases/toolchains/armv7-eabihf/tarballs/armv7-eabihf--glibc--stable-2018.11-1.tar.bz2|c8d4d3ca70442652e0e72f57ae6e878375640508f1e08de3152f63414c43b2e4||||'
  'bootlin_mipsel_2018|Bootlin MIPSel glibc 2018.11|mipsel|https://toolchains.bootlin.com/downloads/releases/toolchains/mips32el/tarballs/mips32el--glibc--stable-2018.11-1.tar.bz2|5b838521093a9b8c521ff518cc33e80ce8649b1c795013a47e167b79c175c049||||'
  'bootlin_powerpc_2018|Bootlin PowerPC e500mc glibc 2018.11|powerpc|https://toolchains.bootlin.com/downloads/releases/toolchains/powerpc-e500mc/tarballs/powerpc-e500mc--glibc--stable-2018.11-1.tar.bz2|b06fd8740248309146e212214abf60ae4ba2018480ec42155286cafdcb401af4||||'
)

TARGET=""
PLATFORM=""
ARCH=""
DESCRIPTION=""
CONF_DIR=""
PCSC=""
TOOLCHAIN=""
RELEASE=1
CLEAN=1
JOBS="auto"
CFLAGS_EXTRA=""
LDFLAGS_EXTRA=""
CC_EXPLICIT=""
CURRENT_CC=""
SAVE=1
PCSC_CFLAGS=""
PCSC_LIBS=""
PCSC_ENABLED=0
TC_RUNTIME_ROOT=""

say(){ printf '%s\n' "$*"; }
line(){ printf '%s\n' '------------------------------------------------------------'; }
ok(){ printf '  %-8s %s\n' OK "$*"; }
warn(){ printf '  %-8s %s\n' WARN "$*" >&2; }
err(){ printf '  %-8s %s\n' ERROR "$*" >&2; }

usage(){
  cat <<'TXT'
TCMG 5.9 - simple device builder

  ./build.sh                 Choose device -> 1) Build  2) Edit config
  ./build.sh list            List devices
  ./build.sh plan <device>   Show saved settings
  ./build.sh config <device> Edit settings and save
  ./build.sh build <device> Build one device
  ./build.sh toolchain <device> Download/prepare its toolchain
  ./build.sh clean <device> Clean only that device
  ./build.sh self-test       Test the builder itself
  ./build.sh check           Check the builder environment

Build options:
  --pcsc auto|on|off
  --release / --debug
  --clean / --no-clean
  --jobs auto|N
  --confdir DIR
  --cflags TEXT
  --ldflags TEXT
  --cc PATH
  --toolchain-dir DIR       Manual SDK/toolchain root

Cross Linux targets default to PC/SC on. The builder prepares a target-side
static libpcsclite client automatically under the selected toolchain.
  --no-save

Nothing is shared between device builds except the source tree. Each device
gets its own build/<device>/ directory and its own saved state.
TXT
}

bool(){ case "${1:-}" in 1|on|yes|true|ON|YES|TRUE) echo 1;; 0|off|no|false|OFF|NO|FALSE) echo 0;; *) return 1;; esac; }

find_device(){
  local wanted="$1" row name
  for row in "${DEVICES[@]}"; do
    IFS='|' read -r name _ <<< "$row"
    if [[ "$name" == "$wanted" ]]; then printf '%s\n' "$row"; return 0; fi
  done
  return 1
}

find_toolchain(){
  local wanted="$1" row name
  for row in "${TOOLCHAINS[@]}"; do
    IFS='|' read -r name _ <<< "$row"
    if [[ "$name" == "$wanted" ]]; then printf '%s\n' "$row"; return 0; fi
  done
  return 1
}

state_file(){ printf '%s/%s.conf\n' "$STATE_DIR" "$1"; }

load_catalog_defaults(){
  local row="$1" catalog_name
  IFS='|' read -r catalog_name PLATFORM ARCH DESCRIPTION CONF_DIR PCSC TOOLCHAIN <<< "$row"
  TARGET="$catalog_name"
  RELEASE=1
  CLEAN=1
  JOBS=auto
  CFLAGS_EXTRA=""
  LDFLAGS_EXTRA=""
  CC_EXPLICIT=""
}

has_saved_state(){
  [[ -f "$(state_file "$TARGET")" ]]
}

load_defaults(){
  local row="$1" catalog_name
  load_catalog_defaults "$row"
  IFS='|' read -r catalog_name _ <<< "$row"
  local state
  state="$(state_file "$catalog_name")"
  if [[ -f "$state" ]]; then
    # Generated by this script; values are quoted with printf %q.
    # shellcheck disable=SC1090
    source "$state"
    TARGET="$catalog_name" # identity always comes from the catalog
  fi
  # Windows used to ship with an incorrect absolute default.  Migrate only
  # that exact legacy value; a user-supplied custom directory is preserved.
  if [[ "$catalog_name" == "windows-x64" ]]; then
    case "$CONF_DIR" in
      'C:/ProgramData/TCMG'|'C:\ProgramData\TCMG')
        CONF_DIR="./"
        warn "Migrated Windows config directory to ./"
        ;;
    esac
  fi
}

save_state(){
  [[ "$SAVE" == 1 ]] || return 0
  local f tmp
  f="$(state_file "$TARGET")"; tmp="$f.tmp.$$"
  {
    printf '# TCMG 5.9 device state - generated by build.sh\n'
    printf 'PCSC=%q\n' "$PCSC"
    printf 'RELEASE=%q\n' "$RELEASE"
    printf 'CLEAN=%q\n' "$CLEAN"
    printf 'JOBS=%q\n' "$JOBS"
    printf 'CONF_DIR=%q\n' "$CONF_DIR"
    printf 'CFLAGS_EXTRA=%q\n' "$CFLAGS_EXTRA"
    printf 'LDFLAGS_EXTRA=%q\n' "$LDFLAGS_EXTRA"
    printf 'CC_EXPLICIT=%q\n' "$CC_EXPLICIT"
  } > "$tmp"
  mv -f "$tmp" "$f"
  ok "Saved: .tcmg-build/$(basename "$f")"
}

device_label(){
  case "$1" in
    tubox) printf '%s' 'Tuxbox / Native Linux';;
    generic-linux) printf '%s' 'Linux Generic';;
    dev-box) printf '%s' 'Development Box';;
    vuplus-4k) printf '%s' 'Vu+ 4K';;
    vuplus-4k-legacy) printf '%s' 'Vu+ Legacy';;
    dreambox-dm900) printf '%s' 'Dreambox DM900 / DM920';;
    dreambox-one) printf '%s' 'Dreambox ONE / TWO';;
    dreambox-mipsel) printf '%s' 'Dreambox MIPSel (DM520/7080/820)';;
    dreambox-dm520) printf '%s' 'Dreambox DM520';;
    dreambox-dm7080) printf '%s' 'Dreambox DM7080';;
    dreambox-dm820) printf '%s' 'Dreambox DM820';;
    dreambox-dm800se) printf '%s' 'Dreambox DM800SE';;
    dreambox-dm500hd) printf '%s' 'Dreambox DM500HD';;
    dreambox-dm800) printf '%s' 'Dreambox DM800';;
    dreambox-dm7020hd) printf '%s' 'Dreambox DM7020HD';;
    dreambox-dm8000) printf '%s' 'Dreambox DM8000';;
    bootlin-aarch64-generic) printf '%s' 'Generic AArch64 (Bootlin)';;
    bootlin-armv7-generic) printf '%s' 'Generic ARMv7 (Bootlin)';;
    bootlin-mipsel-generic) printf '%s' 'Generic MIPSel (Bootlin)';;
    bootlin-powerpc-generic) printf '%s' 'Generic PowerPC (Bootlin)';;
    windows-x64) printf '%s' 'Windows x64';;
    macos-native) printf '%s' 'macOS Native';;
    *) printf '%s' "$1";;
  esac
}

show_devices(){
  local i=1 row name label
  echo
  printf '%s\n' "TCMG 5.9 devices"
  line
  for row in "${DEVICES[@]}"; do
    IFS='|' read -r name _ <<< "$row"
    label="$(device_label "$name")"
    printf '  %2d) %s\n' "$i" "$label"
    i=$((i + 1))
  done
  echo
}

show_config(){
  line
  printf '%-20s %s\n' Device "$(device_label "$TARGET")"
  printf '%-20s %s\n' ID "$TARGET"
  printf '%-20s %s\n' Platform "$PLATFORM"
  printf '%-20s %s\n' Architecture "$ARCH"
  printf '%-20s %s\n' 'Config directory' "$CONF_DIR"
  printf '%-20s %s\n' Toolchain "$TOOLCHAIN"
  printf '%-20s %s\n' 'PC/SC' "$PCSC"
  printf '%-20s %s\n' 'Build' "$([[ "$RELEASE" == 1 ]] && echo Release || echo Debug)"
  printf '%-20s %s\n' Clean "$CLEAN"
  printf '%-20s %s\n' Jobs "$JOBS"
  printf '%-20s %s\n' Compiler "${CC_EXPLICIT:-auto}"
  printf '%-20s %s\n' CFLAGS "${CFLAGS_EXTRA:-<none>}"
  printf '%-20s %s\n' LDFLAGS "${LDFLAGS_EXTRA:-<none>}"
  line
}

show_defaults(){
  local row="$1"
  load_catalog_defaults "$row"
  echo
  printf '%s\n' "Default settings — $(device_label "$TARGET")"
  show_config
  if has_saved_state; then
    echo 'Saved configuration: yes (Build will use the saved profile)'
  else
    echo 'Saved configuration: no (Build will use these defaults)'
  fi
}

choose_device(){
  show_devices
  local n row choice total=${#DEVICES[@]}
  read -r -p "Select device [1-$total]: " choice
  [[ "$choice" =~ ^[0-9]+$ ]] || { err 'Invalid device'; exit 1; }
  n=1
  for row in "${DEVICES[@]}"; do
    if [[ "$n" == "$choice" ]]; then
      load_catalog_defaults "$row"
      CHOSEN_ROW="$row"
      return 0
    fi
    n=$((n + 1))
  done
  err 'Invalid device'; exit 1
}

jobs_num(){
  if [[ "$JOBS" == auto ]]; then
    command -v nproc >/dev/null 2>&1 && nproc || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2
  else
    echo "$JOBS"
  fi
}

need(){ command -v "$1" >/dev/null 2>&1 || { err "Missing command: $1"; return 1; }; }

probe_compiler(){
  local cc="$1" probe
  if [[ ! -x "$cc" ]]; then
    err "Compiler is not executable: $cc"
    return 1
  fi
  if ! probe="$($cc -dumpmachine 2>/dev/null)"; then
    err "Compiler exists but cannot execute: $cc"
    if command -v file >/dev/null 2>&1; then file "$cc" >&2 || true; fi
    if command -v readelf >/dev/null 2>&1; then
      readelf -l "$cc" 2>/dev/null | grep -F 'Requesting program interpreter' >&2 || true
    fi
    err "This is usually a relocated/missing SDK loader problem, not a C source error."
    return 1
  fi
  # probe_compiler is called from resolve_compiler via command substitution.
  # Keep diagnostics off stdout so only the compiler path is returned.
  if [[ -n "$probe" ]]; then
    ok "Compiler probe: $probe" >&2
  else
    ok "Compiler executable probe passed" >&2
  fi
}

# Legacy OpenEmbedded SDK compatibility.
# Krogoth-era SDKs may embed an absolute host prefix in both PT_INTERP and GCC's
# internal include/spec paths. Rewriting the SDK with relocate_sdk.py is not
# reliable for these GCC builds: it can make the compiler executable while
# losing headers such as stddef.h. Instead, keep the SDK untouched and create
# a temporary/stable symlink at the prefix embedded by the SDK. This preserves
# the exact paths the original SDK was built with, matching the old builder.
ensure_legacy_sdk_alias(){
  local root="$1" cc="$2" interp legacy_root host_subdir real_root current
  [[ -f "$cc" ]] || return 0
  command -v readelf >/dev/null 2>&1 || return 0
  interp="$(readelf -l "$cc" 2>/dev/null | sed -n 's/.*Requesting program interpreter: \([^]]*\).*/\1/p' | head -n1)"
  [[ -n "$interp" ]] || return 0
  case "$interp" in
    */sysroots/x86_64-*/lib/ld-linux-x86-64.so.2)
      host_subdir="${interp#*/sysroots/}"
      legacy_root="${interp%/sysroots/${host_subdir}}"
      ;;
    *)
      return 0
      ;;
  esac
  [[ -n "$legacy_root" && "$legacy_root" != "/" ]] || return 0
  real_root="$(readlink -f "$root" 2>/dev/null || printf '%s' "$root")"
  if [[ -L "$legacy_root" ]]; then
    current="$(readlink -f "$legacy_root" 2>/dev/null || true)"
    [[ "$current" == "$real_root" ]] && return 0
    rm -f "$legacy_root"
  elif [[ -e "$legacy_root" ]]; then
    # Do not overwrite a real directory owned by the user/system. If it is
    # already the desired SDK, keep it; otherwise fail with a precise message.
    current="$(readlink -f "$legacy_root" 2>/dev/null || true)"
    if [[ "$current" == "$real_root" ]]; then
      return 0
    fi
    err "SDK expects legacy prefix $legacy_root, but it already exists and is not this toolchain"
    err "Remove/rename that directory or use --toolchain-dir with an SDK already installed there"
    return 1
  fi
  mkdir -p "$(dirname "$legacy_root")"
  ln -s "$real_root" "$legacy_root"
  ok "Legacy SDK alias: $legacy_root -> $real_root" >&2
}

normalize_oe_toolchain(){
  local root="$1" prefix="$2" host_bin d cc found
  [[ -d "$root" ]] || return 0
  # Follow top-level SDK symlinks when locating the compiler. Do not rewrite
  # GCC binaries or SDK paths.
  found="$(locate_cross_gcc "$root" "$prefix" 2>/dev/null || true)"
  [[ -n "$found" ]] || return 0
  cc="${found#*|}"
  ensure_legacy_sdk_alias "$root" "$cc" || return 1
  if [[ -n "$prefix" && ! -x "$root/bin/${prefix}gcc" && -d "$root/sysroots" ]]; then
    host_bin=""
    while IFS= read -r -d '' d; do
      if [[ -x "$d/${prefix}gcc" ]]; then
        host_bin="$d"
        break
      fi
    done < <(find -L "$root/sysroots" -type d -path "*/usr/bin/${prefix%?}" -print0 2>/dev/null)
    if [[ -n "$host_bin" ]]; then
      rm -f "$root/bin" 2>/dev/null || true
      ln -s "${host_bin#"$root/"}" "$root/bin" 2>/dev/null || true
    fi
  fi
}

toolchain_cache_root(){ printf '%s/%s\n' "$TOOLCHAIN_DIR" "$1"; }
toolchain_runtime_root(){ printf '%s/extract\n' "$(toolchain_cache_root "$1")"; }

materialize_toolchain(){
  local tc="$1" cache root archive row name desc arch url sha prefix sysrel cflags ldflags
  row="$(find_toolchain "$tc" || true)"; [[ -n "$row" ]] || { err "No managed toolchain for $tc"; return 1; }
  IFS='|' read -r name desc arch url sha prefix sysrel cflags ldflags <<< "$row"
  cache="$(toolchain_cache_root "$tc")"
  archive="$cache/toolchain.archive"
  # Backward compatibility with previously cached SimpleBuild4 .tar.xz files.
  if [[ ! -f "$archive" && -f "$cache/toolchain.tar.xz" ]]; then
    archive="$cache/toolchain.tar.xz"
  fi
  root="$(toolchain_runtime_root "$tc")"
  [[ -f "$archive" ]] || { err "Cached toolchain archive missing: $archive"; return 1; }
  need sha256sum || return 1
  if ! printf '%s  %s\n' "$sha" "$archive" | sha256sum -c - >/dev/null 2>&1; then
    err "Cached toolchain SHA-256 mismatch: $archive"
    return 1
  fi
  mkdir -p "$cache"
  rm -rf "$root"
  mkdir -p "$root"
  case "$url" in
    *.tar.bz2) tar -xjf "$archive" -C "$root" || { err "Extract failed: $tc"; return 1; };;
    *.tar.xz)  tar -xJf "$archive" -C "$root" || { err "Extract failed: $tc"; return 1; };;
    *.tar.gz)  tar -xzf "$archive" -C "$root" || { err "Extract failed: $tc"; return 1; };;
    *)         tar -xf "$archive" -C "$root" || { err "Extract failed: $tc"; return 1; };;
  esac
  normalize_oe_toolchain "$root" "$prefix" || return 1
  printf '%s\n' "$root" > "$cache/root.path"
}


cross_pcsc_root(){
  local root=""
  if [[ "$TOOLCHAIN" == manual ]]; then
    root="$(cat "$STATE_DIR/manual-toolchain-$TARGET.path" 2>/dev/null || true)"
  else
    # PC/SC lives beside the extracted SDK, matching the old working builder.
    # root.path records the active SDK extraction root.
    root="$(cat "$(toolchain_cache_root "$TOOLCHAIN")/root.path" 2>/dev/null || true)"
    [[ -n "$root" ]] || root="$(toolchain_runtime_root "$TOOLCHAIN")"
  fi
  [[ -n "$root" ]] && printf '%s\n' "$root"
}

pcsc_header_probe(){
  local cc="$1"
  local flags="${2:-}"
  printf '#include <PCSC/winscard.h>\n#include <PCSC/wintypes.h>\nint tcmg_pcsc_probe;\n' |
    "$cc" $TC_CFLAGS $flags -E -x c - >/dev/null 2>&1
}

existing_cross_pcsc(){
  local tcroot="${1:-$(cross_pcsc_root)}" sysroot="${2:-${TC_SYSROOT:-}}" inc lib
  PCSC_CFLAGS=""
  PCSC_LIBS=""
  if [[ -f "$tcroot/pcsc-libs/include/PCSC/winscard.h" &&
        -f "$tcroot/pcsc-libs/include/PCSC/wintypes.h" &&
        -f "$tcroot/pcsc-libs/include/PCSC/pcsclite.h" &&
        -f "$tcroot/pcsc-libs/lib/libpcsclite.a" ]]; then
    PCSC_CFLAGS="-I$tcroot/pcsc-libs/include -I$tcroot/pcsc-libs/include/PCSC"
    PCSC_LIBS="-L$tcroot/pcsc-libs/lib -lpcsclite -ldl"
    return 0
  fi
  if [[ -n "$sysroot" ]]; then
    for inc in "$sysroot/usr/include" "$sysroot/include"; do
      [[ -f "$inc/PCSC/winscard.h" ]] || continue
      [[ -f "$inc/PCSC/wintypes.h" ]] || continue
      [[ -f "$inc/PCSC/pcsclite.h" ]] || continue
      for lib in "$sysroot/usr/lib" "$sysroot/usr/lib64" "$sysroot/lib" "$sysroot/lib64"; do
        if [[ -f "$lib/libpcsclite.a" || -f "$lib/libpcsclite.so" ]]; then
          PCSC_CFLAGS="-I$inc -I$inc/PCSC"
          PCSC_LIBS="-L$lib -lpcsclite -ldl"
          return 0
        fi
      done
    done
  fi
  return 1
}

prepare_cross_pcsc(){
  # PC/SC preparation is intentionally part of the one and only build entry
  # point (build.sh).  No second build helper is required anymore.
  local cc="$1" jobs="$2" tcroot prefix cache build src_root archive cc_base host
  local cflags ldflags lib candidate h
  tcroot="$(cross_pcsc_root)"
  [[ -n "$tcroot" && -d "$tcroot" ]] || { err "PCSC: toolchain root is missing"; return 1; }

  prefix="$tcroot/pcsc-libs"
  cache="$tcroot/pcsc-cache"
  build="$tcroot/pcsc-build"
  src_root="$build/src"
  archive="$cache/PCSC-1.9.5.tar.gz"
  mkdir -p "$prefix" "$cache" "$build"

  if [[ -f "$prefix/include/PCSC/winscard.h" &&
        -f "$prefix/include/PCSC/wintypes.h" &&
        -f "$prefix/include/PCSC/pcsclite.h" &&
        -f "$prefix/lib/libpcsclite.a" ]]; then
    PCSC_CFLAGS="-I$prefix/include -I$prefix/include/PCSC"
    PCSC_LIBS="-L$prefix/lib -lpcsclite -ldl"
    ok "PC/SC ready: $prefix"
    return 0
  fi

  need curl || return 1
  need tar || return 1
  need make || return 1

  if [[ -f "$archive" ]] && tar -tzf "$archive" >/dev/null 2>&1; then
    ok "PC/SC source cache: PCSC 1.9.5"
  else
    # Migrate a previously cached xz toolchain before downloading again.
    legacy_archive="$cache/toolchain.tar.xz"
    if [[ "$url" == *.tar.xz && -f "$legacy_archive" ]] && printf '%s  %s\n' "$sha" "$legacy_archive" | sha256sum -c - >/dev/null 2>&1; then
      mv -f "$legacy_archive" "$archive"
      ok "Using verified legacy toolchain archive: $tc"
    else
      rm -f "$archive"
      say "  Downloading: $tc"
      say "  URL: $url"
      curl -fL --retry 3 --connect-timeout 20 --max-time 1800 -o "$archive" "$url" || { rm -f "$archive"; err "Download failed: $tc"; return 1; }
      printf '%s  %s\n' "$sha" "$archive" | sha256sum -c - || { rm -f "$archive"; err "SHA-256 mismatch: $tc"; return 1; }
    fi
  fi
  if [[ ! -f "$archive" ]]; then
    err "Toolchain archive not available: $tc"
    return 1
  fi
  if false; then
    echo "PC/SC: downloading SimpleBuild4 source PCSC 1.9.5"
    curl -fL --retry 3 --connect-timeout 20 --max-time 1800 \
      -o "$archive" \
      "https://github.com/LudovicRousseau/PCSC/archive/refs/tags/1.9.5.tar.gz" || {
        err "PCSC source download failed"
        return 1
      }
    tar -tzf "$archive" >/dev/null 2>&1 || { err "downloaded PCSC source archive is invalid"; return 1; }
  fi

  rm -rf "$src_root"
  mkdir -p "$src_root"
  tar -xzf "$archive" -C "$src_root" --strip-components=1 || { err "cannot extract pcsc-lite source"; return 1; }

  cc_base="$(basename "$cc")"
  host="${cc_base%-gcc}"
  [[ -n "$host" && "$host" != "$cc_base" ]] || { err "cannot derive PC/SC target from compiler: $cc"; return 1; }

  export CC="$cc"
  export CXX="${CC%-gcc}-g++"
  export AR="${CC%-gcc}-ar"
  export RANLIB="${CC%-gcc}-ranlib"
  export STRIP="${CC%-gcc}-strip"

  cflags="${PCSC_BUILD_CFLAGS:-${TC_CFLAGS:-}}"
  ldflags="${PCSC_BUILD_LDFLAGS:-${TC_LDFLAGS:-}}"
  if [[ -z "$cflags" && -n "$TC_SYSROOT" && -d "$TC_SYSROOT" ]]; then
    cflags="--sysroot=$TC_SYSROOT"
  fi
  if [[ -z "$ldflags" && -n "$TC_SYSROOT" && -d "$TC_SYSROOT" ]]; then
    ldflags="--sysroot=$TC_SYSROOT"
  fi

  if !(
    cd "$src_root"
    if [[ -x ./bootstrap ]]; then
      ./bootstrap >"$build/bootstrap.log" 2>&1
    else
      command -v autoreconf >/dev/null 2>&1 || exit 1
      autoreconf -fi >"$build/autoreconf.log" 2>&1
    fi
    ./configure \
      --build="$(./config.guess 2>/dev/null || printf '%s' x86_64-pc-linux-gnu)" \
      --host="$host" \
      --prefix=/usr \
      --enable-static \
      --disable-shared \
      --disable-libudev \
      --disable-libsystemd \
      --disable-polkit \
      --disable-libusb \
      CFLAGS="$cflags" \
      LDFLAGS="$ldflags" >"$build/configure.log" 2>&1
    make -C src -j"$jobs" libpcsclite.la >"$build/make.log" 2>&1
  ); then
    [[ -f "$build/bootstrap.log" ]] && tail -n 60 "$build/bootstrap.log" >&2 || true
    [[ -f "$build/autoreconf.log" ]] && tail -n 60 "$build/autoreconf.log" >&2 || true
    [[ -f "$build/configure.log" ]] && tail -n 120 "$build/configure.log" >&2 || true
    [[ -f "$build/make.log" ]] && tail -n 160 "$build/make.log" >&2 || true
    err "PCSC client build failed"
    return 1
  fi

  lib=""
  for candidate in "$src_root/src/.libs/libpcsclite.a" "$src_root/.libs/libpcsclite.a"; do
    if [[ -f "$candidate" ]]; then lib="$candidate"; break; fi
  done
  [[ -n "$lib" ]] || { err "PCSC build produced no libpcsclite.a"; return 1; }

  mkdir -p "$prefix/include/PCSC" "$prefix/lib/pkgconfig"
  for h in "$src_root"/src/PCSC/*.h; do
    [[ -f "$h" ]] || continue
    cp -f "$h" "$prefix/include/PCSC/"
  done
  cp -f "$lib" "$prefix/lib/libpcsclite.a"

  [[ -f "$prefix/include/PCSC/winscard.h" &&
     -f "$prefix/include/PCSC/wintypes.h" &&
     -f "$prefix/include/PCSC/pcsclite.h" ]] || { err "PC/SC headers were not installed"; return 1; }

  cat >"$prefix/lib/pkgconfig/libpcsclite.pc" <<EOF
prefix=$prefix
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: libpcsclite
Description: PC/SC Lite client library
Version: 1.9.5
Cflags: -I\${includedir}
Libs: -L\${libdir} -lpcsclite
Libs.private: -pthread -ldl
EOF

  rm -rf "$build"
  PCSC_CFLAGS="-I$prefix/include -I$prefix/include/PCSC"
  PCSC_LIBS="-L$prefix/lib -lpcsclite -ldl"
  echo "PC/SC: ready at $prefix"
}

# Locates a cross-gcc under a toolchain root and prints "prefix|path".
# OE/Yocto SDKs (what our managed toolchains are) keep the real compiler
# deep under sysroots/x86_64-*/usr/bin/<target>/, with a *symlinked*
# top-level bin/ pointing at it -- `find` without -L doesn't follow that
# symlink, so a plain search comes back empty even though the compiler is
# right there on disk. `-L` (follow symlinks) is what actually finds it.
# We still try the catalog's declared prefix first (fast, exact), then
# fall back to any *-gcc under a bin/ dir and trust whatever we found
# instead of the catalog, in case a future toolchain build renames it.
locate_cross_gcc(){
  local root="$1" prefer_prefix="${2:-}" cc=""
  if [[ -n "$prefer_prefix" ]]; then
    [[ -x "$root/bin/${prefer_prefix}gcc" ]] && cc="$root/bin/${prefer_prefix}gcc"
    if [[ -z "$cc" && -d "$root/sysroots" ]]; then
      while IFS= read -r -d '' d; do
        if [[ -x "$d/${prefer_prefix}gcc" ]]; then cc="$d/${prefer_prefix}gcc"; break; fi
      done < <(find -L "$root/sysroots" -type d -path "*/usr/bin/${prefer_prefix%?}" -print0 2>/dev/null)
    fi
    if [[ -z "$cc" ]]; then
      cc="$(find -L "$root" -type f -path '*/bin/'"$prefer_prefix"'gcc' -perm -111 -print -quit 2>/dev/null || true)"
    fi
  fi
  if [[ -z "$cc" ]]; then
    cc="$(find -L "$root" -type f -path '*/bin/*-gcc' -perm -111 -print -quit 2>/dev/null || true)"
  fi
  [[ -n "$cc" ]] || return 1
  local base prefix
  base="$(basename "$cc")"
  prefix="${base%gcc}"
  printf '%s|%s\n' "$prefix" "$cc"
}

# Returns: tc_dir|cc|sysroot
find_installed_toolchain(){
  local tc="$1" cache root row name desc arch url sha prefix sysrel cflags ldflags cc sysroot found
  row="$(find_toolchain "$tc" || true)"; [[ -n "$row" ]] || return 1
  IFS='|' read -r name desc arch url sha prefix sysrel cflags ldflags <<< "$row"
  cache="$(toolchain_cache_root "$tc")"
  root="$(cat "$cache/root.path" 2>/dev/null || true)"
  if [[ -z "$root" || ! -d "$root" ]]; then
    root="$(toolchain_runtime_root "$tc")"
    [[ -f "$cache/toolchain.archive" || -f "$cache/toolchain.tar.xz" ]] || return 1
    materialize_toolchain "$tc" >/dev/null || return 1
  fi
  normalize_oe_toolchain "$root" "$prefix" >/dev/null || return 1
  found="$(locate_cross_gcc "$root" "$prefix" || true)"; [[ -n "$found" ]] || return 1
  cc="${found#*|}"
  if [[ -d "$root/$sysrel" ]]; then
    sysroot="$root/$sysrel"
  else
    sysroot="$($cc -print-sysroot 2>/dev/null || true)"
  fi
  [[ -n "$sysroot" && -d "$sysroot" ]] || return 1
  printf '%s|%s|%s\n' "$root" "$cc" "$sysroot"
}

fetch_toolchain(){
  local tc="$1" row name desc arch url sha prefix sysrel cflags ldflags cache archive
  row="$(find_toolchain "$tc" || true)"; [[ -n "$row" ]] || { err "No managed toolchain for $tc"; return 1; }
  IFS='|' read -r name desc arch url sha prefix sysrel cflags ldflags <<< "$row"
  [[ -n "$url" && -n "$sha" ]] || { err "$tc has no managed download. Use --toolchain-dir for this device."; return 1; }
  need curl || return 1
  need sha256sum || return 1
  need tar || return 1
  cache="$(toolchain_cache_root "$tc")"
  mkdir -p "$cache"
  archive="$cache/toolchain.archive"
  if [[ -f "$archive" ]] && printf '%s  %s\n' "$sha" "$archive" | sha256sum -c - >/dev/null 2>&1; then
    ok "Using verified toolchain archive: $tc"
  else
    rm -f "$archive"
  fi
  materialize_toolchain "$tc" || return 1
  local info cc
  info="$(find_installed_toolchain "$tc")" || return 1
  cc="$(cut -d'|' -f2 <<< "$info")"
  printf '%s\n' "$cc" > "$cache/compiler.path"
  ok "Toolchain ready: $tc"
  ok "Compiler: $cc"
}

manual_toolchain(){
  local dir="$1" cc
  [[ -d "$dir" ]] || { err "Toolchain directory not found: $dir"; return 1; }
  printf '%s\n' "$dir" > "$STATE_DIR/manual-toolchain-$TARGET.path"
  cc="$(find -L "$dir" -type f -name '*-gcc' -perm -111 -print -quit 2>/dev/null || true)"
  [[ -n "$cc" ]] || { err "No cross gcc found under $dir (searched following symlinks too)"; return 1; }
  printf '%s\n' "$cc" > "$STATE_DIR/manual-toolchain-$TARGET.compiler"
  ok "Using manual toolchain: $dir"
  ok "Compiler: $cc"
}

resolve_compiler(){
  local tc_info tcdir cc sysroot
  if [[ -n "$CC_EXPLICIT" ]]; then
    cc="$CC_EXPLICIT"
  elif [[ "$TOOLCHAIN" == native ]]; then
    case "$PLATFORM" in
      linux) cc="${CC:-gcc}";;
      windows) cc="x86_64-w64-mingw32-gcc";;
      macos) cc="cc";;
    esac
  elif [[ "$TOOLCHAIN" == manual ]]; then
    cc="$(cat "$STATE_DIR/manual-toolchain-$TARGET.compiler" 2>/dev/null || true)"
    [[ -n "$cc" ]] || {
      err "No manual toolchain selected for $TARGET. Use:"
      echo "  ./build.sh config $TARGET --toolchain-dir /path/to/sdk" >&2
      return 1
    }
  else
    tc_info="$(find_installed_toolchain "$TOOLCHAIN" || true)"
    if [[ -z "$tc_info" ]]; then
      echo "  Toolchain '$TOOLCHAIN' is not installed; downloading it now." >&2
      fetch_toolchain "$TOOLCHAIN" >&2 || return 1
      tc_info="$(find_installed_toolchain "$TOOLCHAIN" || true)"
    fi
    [[ -n "$tc_info" ]] || { err "Could not resolve managed toolchain '$TOOLCHAIN'"; return 1; }
    IFS='|' read -r tcdir cc sysroot <<< "$tc_info"
  fi
  if [[ "$cc" == */* ]]; then
    [[ -x "$cc" ]] || { err "Compiler not executable: $cc"; return 1; }
  else
    command -v "$cc" >/dev/null 2>&1 || { err "Compiler not found: $cc"; return 1; }
    cc="$(command -v "$cc")"
  fi
  if [[ "$TOOLCHAIN" != native || "$PLATFORM" == linux ]]; then
    probe_compiler "$cc" || return 1
  fi
  printf '%s\n' "$cc"
}

validate_arch(){
  local cc="$1" target
  [[ "$PLATFORM" == linux ]] || return 0
  if [[ "$TOOLCHAIN" == native ]]; then
    return 0
  elif [[ "$TOOLCHAIN" == manual ]]; then
    : # fall through to the runtime probe below -- a --toolchain-dir path
      # has no catalog entry to trust, so this is the only check we have.
  else
    # Managed toolchain: the catalog already says what arch this is.
    # Don't re-probe via `$cc -dumpmachine` -- several OE/Yocto SDK
    # cross-gcc's only report their target correctly once the SDK's
    # environment-setup-* script has been sourced, and fail this probe
    # (empty output) even though the compiler itself works fine once
    # --sysroot is passed (which toolchain_flags() already does).
    ok "Compiler target: $ARCH (from toolchain catalog: $TOOLCHAIN)"
    return 0
  fi
  target="$($cc -dumpmachine 2>/dev/null || true)"
  case "$ARCH" in
    armv7) [[ "$target" == arm*-* || "$target" == arm*linux* ]] || { err "Wrong compiler target: $target (need ARMv7)"; return 1; };;
    aarch64) [[ "$target" == aarch64-* || "$target" == aarch64* ]] || { err "Wrong compiler target: $target (need AArch64)"; return 1; };;
    mipsel) [[ "$target" == mipsel-* || "$target" == mipsel* ]] || { err "Wrong compiler target: $target (need MIPSel)"; return 1; };;
    powerpc) [[ "$target" == powerpc-* || "$target" == powerpc* ]] || { err "Wrong compiler target: $target (need PowerPC)"; return 1; };;
  esac
  ok "Compiler target: $target"
}

arch_runtime_ldflags(){
  case "$ARCH" in
    mipsel) printf '%s' '-latomic';;
    *) printf '%s' '';;
  esac
}

toolchain_flags(){
  TC_CFLAGS=""; TC_LDFLAGS=""; TC_SYSROOT=""; TC_RUNTIME_ROOT=""
  [[ "$TOOLCHAIN" == native || "$TOOLCHAIN" == manual ]] && return 0
  local row name desc arch url sha prefix sysrel cflags ldflags info cc builtin_sysroot
  row="$(find_toolchain "$TOOLCHAIN")"
  IFS='|' read -r name desc arch url sha prefix sysrel cflags ldflags <<< "$row"
  info="$(find_installed_toolchain "$TOOLCHAIN")"
  TC_RUNTIME_ROOT="$(cut -d'|' -f1 <<< "$info")"
  cc="$(cut -d'|' -f2 <<< "$info")"
  TC_CFLAGS="$cflags"
  TC_LDFLAGS="$ldflags"

  # Legacy 32-bit MIPSel GCC emits 64-bit C11 atomics through libatomic.
  # Without this explicit runtime library the linker reports __atomic_*_8.
  local runtime_ldflags
  runtime_ldflags="$(arch_runtime_ldflags)"
  if [[ -n "$runtime_ldflags" ]]; then
    TC_LDFLAGS="${TC_LDFLAGS:+$TC_LDFLAGS }$runtime_ldflags"
  fi

  # Match SimpleBuild4: prefer the compiler's built-in sysroot. Only inject
  # --sysroot when the compiler does not carry a valid built-in sysroot.
  builtin_sysroot="$($cc -print-sysroot 2>/dev/null || true)"
  if [[ -n "$builtin_sysroot" && -d "$builtin_sysroot" ]]; then
    TC_SYSROOT="$builtin_sysroot"
  elif [[ -n "$sysrel" && -d "$TC_RUNTIME_ROOT/$sysrel" ]]; then
    TC_SYSROOT="$TC_RUNTIME_ROOT/$sysrel"
    TC_CFLAGS="${TC_CFLAGS:+$TC_CFLAGS }--sysroot=$TC_SYSROOT"
    TC_LDFLAGS="${TC_LDFLAGS:+$TC_LDFLAGS }--sysroot=$TC_SYSROOT"
  fi
}

pcsc_flag(){
  PCSC_CFLAGS=""
  PCSC_LIBS=""
  PCSC_ENABLED=0
  case "$PCSC" in
    off) return 0;;
    on)
      if [[ "$PLATFORM" == linux ]]; then
        if [[ "$TOOLCHAIN" == native ]]; then
          # Match TCMG 5.7 native-PCSC behavior: use pkg-config when available,
          # but always provide a usable linker fallback for an explicitly
          # requested PCSC build. Do not force a cross-PCSC preparation path
          # onto a native Linux build.
          if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libpcsclite; then
            PCSC_CFLAGS="$(pkg-config --cflags libpcsclite)"
            PCSC_LIBS="$(pkg-config --libs libpcsclite)"
          fi
          [[ -n "$PCSC_LIBS" ]] || PCSC_LIBS="-lpcsclite"
        else
          if ! existing_cross_pcsc "$TC_RUNTIME_ROOT" "$TC_SYSROOT" ||
             ! pcsc_header_probe "$CURRENT_CC" "$PCSC_CFLAGS"; then
            prepare_cross_pcsc "$CURRENT_CC" "$(jobs_num)" || return 1
            pcsc_header_probe "$CURRENT_CC" "$PCSC_CFLAGS" || {
              err "PC/SC headers are not usable after preparing target libpcsclite"
              return 1
            }
          fi
        fi
      fi
      PCSC_ENABLED=1
      return 0;;
    auto)
      if [[ "$PLATFORM" == linux && "$TOOLCHAIN" == native ]]; then
        if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libpcsclite; then
          PCSC_ENABLED=1
        fi
      elif [[ "$PLATFORM" == linux && "$TOOLCHAIN" != native ]]; then
        if existing_cross_pcsc "$TC_RUNTIME_ROOT" "$TC_SYSROOT" &&
           pcsc_header_probe "$CURRENT_CC" "$PCSC_CFLAGS"; then
          PCSC_ENABLED=1
        fi
      elif [[ "$PLATFORM" != linux ]]; then
        PCSC_ENABLED=1
      fi
      return 0;;
    *) err 'PCSC must be auto/on/off'; return 1;;
  esac
}

interactive(){
  local ans
  echo
  line; echo "Edit configuration: $(device_label "$TARGET")"; echo "$DESCRIPTION"; line
  read -r -p "Release build? [Y/n] " ans
  if [[ -z "$ans" || "$ans" =~ ^[Yy]$ ]]; then RELEASE=1; else RELEASE=0; fi
  read -r -p "PCSC [auto/on/off] [$PCSC]: " ans
  if [[ -n "$ans" ]]; then PCSC="$ans"; fi
  read -r -p "Clean before build? [Y/n] " ans
  if [[ -z "$ans" || "$ans" =~ ^[Yy]$ ]]; then CLEAN=1; else CLEAN=0; fi
  read -r -p "Jobs [auto/$JOBS]: " ans
  if [[ -n "$ans" ]]; then JOBS="$ans"; fi
  read -r -p "Config directory [$CONF_DIR]: " ans
  if [[ -n "$ans" ]]; then CONF_DIR="$ans"; fi
  if [[ "$TOOLCHAIN" == manual ]]; then
    read -r -p "Toolchain directory [none]: " ans
    [[ -n "$ans" ]] && manual_toolchain "$ans"
  fi
  read -r -p "Extra CFLAGS [none]: " ans; CFLAGS_EXTRA="$ans"
  read -r -p "Extra LDFLAGS [none]: " ans; LDFLAGS_EXTRA="$ans"
}

interactive_menu(){
  local row="$1" action
  show_defaults "$row"
  read -r -p '1) Build   2) Edit config   [1]: ' action
  case "${action:-1}" in
    1)
      load_defaults "$row"
      save_state
      build_device
      ;;
    2)
      load_defaults "$row"
      interactive
      save_state
      echo
      ok "Configuration saved for $(device_label "$TARGET")"
      show_config
      ;;
    *)
      err 'Invalid selection (choose 1 or 2)'
      return 1
      ;;
  esac
}

build_device(){
  local cc pcsc_enabled jobs build_dir stamp log builtin_inc
  PCSC_CFLAGS=""
  PCSC_LIBS=""
  cc="$(resolve_compiler)" || return 1
  CURRENT_CC="$cc"
  validate_arch "$cc" || return 1
  toolchain_flags
  pcsc_flag || return 1
  pcsc_enabled="$PCSC_ENABLED"

  # Fail early if the legacy SDK's GCC cannot see its own builtin headers.
  # This catches the exact broken-relocation symptom (e.g. stddef.h missing)
  # before make starts dozens of parallel compile jobs.
  if [[ "$PLATFORM" == linux && "$TOOLCHAIN" != native ]]; then
    builtin_inc="$($cc -print-file-name=include 2>/dev/null || true)"
    if [[ -z "$builtin_inc" || "$builtin_inc" == include || ! -f "$builtin_inc/stddef.h" ]]; then
      err "Cross GCC builtin headers are not usable"
      err "Compiler: $cc"
      err "GCC include: ${builtin_inc:-<none>}"
      return 1
    fi
    if ! printf '#include <stddef.h>\nint tcmg_header_probe;\n' | "$cc" $TC_CFLAGS -E -x c - >/dev/null 2>&1; then
      err "Cross GCC cannot preprocess stddef.h with the selected sysroot"
      return 1
    fi
    ok "GCC headers: stddef.h"
    if [[ "$pcsc_enabled" == 1 ]]; then
      if ! printf '#include <PCSC/winscard.h>\nint tcmg_pcsc_probe;\n' | "$cc" $TC_CFLAGS $PCSC_CFLAGS -E -x c - >/dev/null 2>&1; then
        err "PC/SC headers are not usable with the selected cross compiler"
        return 1
      fi
      ok "PC/SC headers: winscard.h"
    fi
  fi
  jobs="$(jobs_num)"
  build_dir="build/$TARGET"
  [[ "$CLEAN" == 1 ]] && make BUILD_DIR="$build_dir" clean >/dev/null 2>&1 || true
  stamp="$(date +%Y%m%d-%H%M%S)"
  log="$LOG_DIR/${TARGET}-${stamp}.log"
  echo
  line
  echo "Building TCMG 5.9 — $TARGET"
  show_config
  echo "Compiler: $cc"
  echo "Output  : $build_dir/tcmg"
  [[ "$pcsc_enabled" == 1 && "$PLATFORM" == linux && "$TOOLCHAIN" != native ]] && {
    echo "PCSC CFLAGS: $PCSC_CFLAGS"
    echo "PCSC LIBS  : $PCSC_LIBS"
  }
  line
  local args=("BUILD_DIR=$build_dir" "RELEASE=$RELEASE" "TCMG_TARGET_OS=$PLATFORM" "TCMG_TARGET_NAME=tcmg" "TCMG_PCSC=$pcsc_enabled" "CC=$cc" "TCMG_CONF_DIR=$CONF_DIR")
  [[ -n "$CFLAGS_EXTRA" ]] && args+=("CFLAGS_EXTRA=$CFLAGS_EXTRA")
  [[ -n "$PCSC_CFLAGS" ]] && args+=("PCSC_CFLAGS=$PCSC_CFLAGS")
  [[ -n "$PCSC_LIBS" ]] && args+=("PCSC_LIBS=$PCSC_LIBS")
  local cflags="$TC_CFLAGS $CFLAGS_EXTRA" ldflags="$TC_LDFLAGS $LDFLAGS_EXTRA"
  [[ -n "$cflags" ]] && args+=("TCMG_ARCH_FLAGS=$cflags")
  [[ -n "$ldflags" ]] && args+=("TCMG_ARCH_LDFLAGS=$ldflags")
  if ! make -j"$jobs" "${args[@]}" >"$log" 2>&1; then
    err "Build failed"
    tail -n 100 "$log" >&2
    echo "Log: $log" >&2
    return 1
  fi
  ok "Build complete: $build_dir/tcmg"
  echo "Log: $log"
}

clean_device(){
  make BUILD_DIR="build/$TARGET" clean
  rm -f "$LOG_DIR/${TARGET}-"*.log 2>/dev/null || true
}

check_env(){
  local bad=0 c
  for c in bash make awk sed grep find; do command -v "$c" >/dev/null 2>&1 || { err "Missing command: $c"; bad=1; }; done
  [[ -f "$ROOT_DIR/Makefile" ]] || { err 'Makefile missing'; bad=1; }
  [[ -f "$ROOT_DIR/src/main.c" ]] || { err 'src/main.c missing'; bad=1; }
  if [[ "$bad" == 0 ]]; then ok 'Builder environment'; fi
  return "$bad"
}

self_test(){
  local tmp="$STATE_DIR/self-test.$$" row name i
  mkdir -p "$tmp"
  trap 'rm -rf "$tmp"' RETURN
  [[ "$(find_device generic-linux)" == generic-linux\|linux\|native* ]] || { err 'device catalog failed'; return 1; }
  [[ "$(find_toolchain vuplus4k_armv7)" == vuplus4k_armv7\|* ]] || { err 'SimpleBuild4 Vu+ toolchain catalog failed'; return 1; }
  [[ "$(find_device dreambox-dm800se)" == dreambox-dm800se\|linux\|mipsel\|* ]] || { err 'Dreambox DM800SE device catalog failed'; return 1; }
  [[ "$(find_device dreambox-dm520)" == dreambox-dm520\|linux\|mipsel\|* ]] || { err 'Dreambox DM520 device catalog failed'; return 1; }
  [[ "$(find_device dreambox-dm7080)" == dreambox-dm7080\|linux\|mipsel\|* ]] || { err 'Dreambox DM7080 device catalog failed'; return 1; }
  [[ "$(find_device dreambox-dm820)" == dreambox-dm820\|linux\|mipsel\|* ]] || { err 'Dreambox DM820 device catalog failed'; return 1; }
  [[ "$(find_toolchain dream_mipsel)" == dream_mipsel\|* ]] || { err 'Dreambox legacy MIPSel toolchain catalog failed'; return 1; }
  [[ "$(find_toolchain bootlin_aarch64_2018)" == bootlin_aarch64_2018\|* ]] || { err 'Bootlin AArch64 toolchain catalog failed'; return 1; }
  [[ "$(find_toolchain bootlin_armv7_2018)" == bootlin_armv7_2018\|* ]] || { err 'Bootlin ARMv7 toolchain catalog failed'; return 1; }
  [[ "$(find_toolchain bootlin_mipsel_2018)" == bootlin_mipsel_2018\|* ]] || { err 'Bootlin MIPSel toolchain catalog failed'; return 1; }
  [[ "$(find_toolchain bootlin_powerpc_2018)" == bootlin_powerpc_2018\|* ]] || { err 'Bootlin PowerPC toolchain catalog failed'; return 1; }
  local tcrow tcname tcdesc tcarch tcurl tcsha tcpfx tcsys tccf tcaf
  for tcname in bootlin_aarch64_2018 bootlin_armv7_2018 bootlin_mipsel_2018 bootlin_powerpc_2018; do
    tcrow="$(find_toolchain "$tcname")"
    IFS='|' read -r tcname tcdesc tcarch tcurl tcsha tcpfx tcsys tccf tcaf <<< "$tcrow"
    [[ "$tcurl" == https://toolchains.bootlin.com/* ]] || { err "Bootlin URL invalid for $tcname"; return 1; }
    [[ "$tcurl" == *.tar.bz2 ]] || { err "Bootlin archive format invalid for $tcname"; return 1; }
    [[ "$tcsha" =~ ^[0-9a-fA-F]{64}$ ]] || { err "Bootlin SHA-256 invalid for $tcname"; return 1; }
  done
  ARCH=powerpc
  [[ "$(arch_runtime_ldflags)" == '' ]] || { err 'PowerPC should not inherit MIPSel -latomic'; return 1; }
  ARCH=mipsel
  [[ "$(arch_runtime_ldflags)" == '-latomic' ]] || { err 'MIPSel runtime linker flags missing -latomic'; return 1; }
  ARCH=native
  [[ -z "$(arch_runtime_ldflags)" ]] || { err 'Native targets should not force -latomic'; return 1; }
  local winrow wname wplatform warch wdesc wconf wpcsc wtc
  winrow="$(find_device windows-x64)"
  IFS='|' read -r wname wplatform warch wdesc wconf wpcsc wtc <<< "$winrow"
  [[ "$wconf" == './' ]] || { err "Windows default config directory is not ./ (got: $wconf)"; return 1; }
  TARGET=generic-linux; PLATFORM=linux; ARCH=native; DESCRIPTION=x; CONF_DIR=/tmp; PCSC=off; TOOLCHAIN=native; RELEASE=1; CLEAN=1; JOBS=1; CFLAGS_EXTRA=; LDFLAGS_EXTRA=; CC_EXPLICIT=;
  SAVE=1; save_state >/dev/null
  [[ -f "$(state_file generic-linux)" ]] || { err 'state persistence failed'; return 1; }
  [[ "$(find "${TOOLCHAIN_DIR}" -maxdepth 2 -type f -name '*.tmp*' 2>/dev/null | wc -l)" == 0 ]] || { err 'temporary toolchain files remain'; return 1; }
  ok 'Self-test passed'; return 0
}

parse_options(){
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --pcsc) PCSC="${2:?missing value for --pcsc}"; shift 2;;
      --release) RELEASE=1; shift;;
      --debug) RELEASE=0; shift;;
      --clean) CLEAN=1; shift;;
      --no-clean) CLEAN=0; shift;;
      --jobs) JOBS="${2:?missing value for --jobs}"; shift 2;;
      --confdir) CONF_DIR="${2:?missing value for --confdir}"; shift 2;;
      --cflags) CFLAGS_EXTRA="${2:?missing value for --cflags}"; shift 2;;
      --ldflags) LDFLAGS_EXTRA="${2:?missing value for --ldflags}"; shift 2;;
      --cc) CC_EXPLICIT="${2:?missing value for --cc}"; shift 2;;
      --toolchain-dir) MANUAL_TC_DIR="${2:?missing value for --toolchain-dir}"; shift 2;;
      --no-save) SAVE=0; shift;;
      --help|-h) usage; exit 0;;
      *) err "Unknown option: $1"; usage; exit 1;;
    esac
  done
}

# ----- dispatch -----
COMMAND="${1:-wizard}"
shift || true
MANUAL_TC_DIR=""
DEVICE_ARG=""

case "$COMMAND" in
  ''|wizard|menu)
    choose_device
    interactive_menu "$CHOSEN_ROW"
    exit $?;;
  list)
    show_devices; exit 0;;
  check|doctor)
    check_env; exit $?;;
  self-test)
    check_env && self_test; exit $?;;
  plan|config|build|toolchain|clean|test|strict)
    if [[ $# -gt 0 && "$1" != --* ]]; then DEVICE_ARG="$1"; shift; fi
    ;;
  help|-h|--help)
    usage; exit 0;;
  *)
    if find_device "$COMMAND" >/dev/null 2>&1; then
      DEVICE_ARG="$COMMAND"
      COMMAND=build
    else
      err "Unknown command/device: $COMMAND"; usage; exit 1
    fi
    ;;
esac

if [[ -z "$DEVICE_ARG" ]]; then
  show_devices
  read -r -p "Select device [1-${#DEVICES[@]}]: " n
  [[ "$n" =~ ^[0-9]+$ ]] || { err 'Invalid device'; exit 1; }
  i=1
  for row in "${DEVICES[@]}"; do
    if [[ "$i" == "$n" ]]; then DEVICE_ARG="${row%%|*}"; break; fi
    i=$((i + 1))
  done
fi

row="$(find_device "$DEVICE_ARG" || true)"
[[ -n "$row" ]] || { err "Unknown device '$DEVICE_ARG'"; exit 1; }
load_defaults "$row"
parse_options "$@"

# Manual SDK path is an override for this device only and is remembered in its state.
if [[ -n "$MANUAL_TC_DIR" ]]; then manual_toolchain "$MANUAL_TC_DIR"; fi

case "$COMMAND" in
  plan)
    show_config;;
  config)
    interactive
    [[ -n "$MANUAL_TC_DIR" ]] && manual_toolchain "$MANUAL_TC_DIR"
    save_state
    show_config;;
  toolchain)
    if [[ "$TOOLCHAIN" == native ]]; then
      ok 'Native compiler - nothing to download'
    elif [[ "$TOOLCHAIN" == manual ]]; then
      [[ -n "$MANUAL_TC_DIR" ]] || { err 'This device needs --toolchain-dir /path/to/sdk'; exit 1; }
    else
      fetch_toolchain "$TOOLCHAIN"
    fi;;
  build)
    save_state
    build_device;;
  clean)
    clean_device;;
  test)
    save_state
    make BUILD_DIR="build/$TARGET" RELEASE="$RELEASE" TCMG_TARGET_OS="$PLATFORM" TCMG_TARGET_NAME=tcmg TCMG_PCSC=0 test;;
  strict)
    save_state
    make BUILD_DIR="build/$TARGET" RELEASE=0 TCMG_TARGET_OS="$PLATFORM" TCMG_TARGET_NAME=tcmg TCMG_PCSC=0 TCMG_STRICT=1;;
esac
