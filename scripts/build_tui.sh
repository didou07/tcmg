#!/usr/bin/env bash

TUI_ACTIVE=0
TUI_STTY=''

declare -a TUI_FAMILIES=()
declare -A TUI_FAMILY_ROWS=()
declare -A TUI_FAMILY_COUNTS=()
declare -a TUI_ROWS=()

_tui_family_for_row(){
  local row="$1" name platform arch description conf pcsc tc s
  IFS='|' read -r name platform arch description conf pcsc tc <<< "$row"
  s="${name,,} ${description,,}"

  case "$name" in
    platform-*|profile-*) printf '%s\n' 'Build Profiles'; return ;;
  esac

  case "$s" in
    *windows*|*macos*) printf '%s\n' 'Windows / macOS'; return ;;
    *dreambox*|*dreamone*|*dreamtwo*) printf '%s\n' 'Dreambox'; return ;;
    *vuplus*|*'vu+'*|vuuno*|vuzero*|vuduo*|vusolo*|vuultimo*|vusolo4k*) printf '%s\n' 'Vu+'; return ;;
    *formuler*) printf '%s\n' 'Formuler'; return ;;
    gb*|*gigablue*) printf '%s\n' 'GigaBlue'; return ;;
    et*|e3hd|e4hd|*xtrend*) printf '%s\n' 'Xtrend / ET'; return ;;
    h[0-9]*|hd[0-9]*|hzero|*zgemma*) printf '%s\n' 'Zgemma / H-Series'; return ;;
    *mutant*|*'ax/mutant'*|*' ax '*) printf '%s\n' 'Mutant / AX'; return ;;
    *edision*|osmini*|osmega|osmio*|*'os mio'*) printf '%s\n' 'Edision'; return ;;
    *qnap*) printf '%s\n' 'QNAP'; return ;;
    *synology*) printf '%s\n' 'Synology'; return ;;
    *raspberry*|rpi-*) printf '%s\n' 'Raspberry Pi'; return ;;
    *fritz*|*'fritz!box'*) printf '%s\n' 'FRITZ!Box'; return ;;
    *ubiquiti*|*ubnt*) printf '%s\n' 'Ubiquiti'; return ;;
    *openwrt*) printf '%s\n' 'OpenWrt / Routers'; return ;;
    *openembedded*|*'oe20'*|*'openpli'*) printf '%s\n' 'OpenEmbedded / OpenPLi'; return ;;
    *coolstream*|*cool2*) printf '%s\n' 'COOLSTREAM'; return ;;
    *dbox*|*ppcold*) printf '%s\n' 'DBox'; return ;;
    *sh4*) printf '%s\n' 'SH4'; return ;;
    *native*|tubox|generic-*|bootlin-*) printf '%s\n' 'Generic / Development'; return ;;
  esac

  case "$arch" in
    arm|armv5|armv6|armv7|aarch64) printf '%s\n' 'Generic ARM / AArch64'; return ;;
    mips|mipsel|mips32el|mips64) printf '%s\n' 'Generic MIPS'; return ;;
    powerpc) printf '%s\n' 'Generic PowerPC'; return ;;
    csky) printf '%s\n' 'C-SKY'; return ;;
    *) printf '%s\n' 'Other STB'; return ;;
  esac
}

_tui_build_families(){
  local row fam
  TUI_FAMILIES=()
  TUI_FAMILY_ROWS=()
  TUI_FAMILY_COUNTS=()
  for row in "${DEVICES[@]}"; do
    fam="$(_tui_family_for_row "$row")"
    if [[ -z "${TUI_FAMILY_COUNTS[$fam]+x}" ]]; then
      TUI_FAMILIES+=("$fam")
      TUI_FAMILY_COUNTS[$fam]=0
      TUI_FAMILY_ROWS[$fam]=''
    fi
    TUI_FAMILY_COUNTS[$fam]=$((TUI_FAMILY_COUNTS[$fam]+1))
    TUI_FAMILY_ROWS[$fam]+="$row"$'\n'
  done
}

_tui_lines(){
  local n="$(tput lines 2>/dev/null || printf '24')"
  [[ "$n" =~ ^[0-9]+$ ]] || n=24
  (( n < 16 )) && n=16
  printf '%s' "$n"
}

_tui_cols(){
  local n="$(tput cols 2>/dev/null || printf '90')"
  [[ "$n" =~ ^[0-9]+$ ]] || n=90
  (( n < 70 )) && n=70
  printf '%s' "$n"
}

_tui_trim(){
  local text="$1" width="$2"
  (( width < 8 )) && width=8
  if (( ${#text} > width )); then
    printf '%.*s...' "$((width-3))" "$text"
  else
    printf '%s' "$text"
  fi
}

_tui_start(){
  [[ "$TUI_ACTIVE" == 1 ]] && return 0
  TUI_STTY="$(stty -g 2>/dev/null || true)"
  stty -echo -icanon min 1 time 0 2>/dev/null || return 1
  printf '\033[?25l'
  TUI_ACTIVE=1
}

_tui_stop(){
  [[ "$TUI_ACTIVE" == 1 ]] || return 0
  printf '\033[?25h\033[0m\n'
  [[ -n "$TUI_STTY" ]] && stty "$TUI_STTY" 2>/dev/null || stty sane 2>/dev/null || true
  TUI_ACTIVE=0
}

_tui_pause(){
  local prompt="${1:-Press Enter to return to the TUI...}"
  _tui_stop
  printf '%s' "$prompt"
  read -r _
  _tui_start
}

_tui_key(){
  local k rest
  IFS= read -rsN1 k || return 1
  if [[ "$k" == $'\e' ]]; then
    rest=''
    IFS= read -rsN2 -t 0.05 rest || true
    k+="$rest"
  fi
  case "$k" in
    $'\e[A'|k) printf 'up' ;;
    $'\e[B'|j) printf 'down' ;;
    $'\e[C'|l) printf 'right' ;;
    $'\e[D'|h) printf 'left' ;;
    $'\e[H'|g) printf 'home' ;;
    $'\e[F'|G) printf 'end' ;;
    $'\e[5~') printf 'pageup' ;;
    $'\e[6~') printf 'pagedown' ;;
    $'\e') printf 'back' ;;
    $'\n'|$'\r') printf 'enter' ;;
    q|Q) printf 'quit' ;;
    b|B) printf 'back' ;;
    *) printf '%s' "$k" ;;
  esac
}

_tui_header(){
  local title="$1" detail="${2:-}"
  printf '\033[H\033[2J'
  printf ' TCMG %s  |  build TUI\n' "$VERSION"
  printf ' %s\n' "$title"
  [[ -n "$detail" ]] && printf ' %s\n' "$detail"
  printf '%s\n' '------------------------------------------------------------'
}

_tui_family_menu(){
  _tui_build_families
  _tui_start || return 1
  local pos=0 total=${#TUI_FAMILIES[@]} key lines max_rows start end i fam mark
  while :; do
    lines="$(_tui_lines)"
    max_rows=$((lines-8)); (( max_rows < 5 )) && max_rows=5
    start=0
    (( pos >= max_rows )) && start=$((pos-max_rows+1))
    end=$((start+max_rows)); (( end > total )) && end=$total

    _tui_header 'Select device family' "$total families / ${#DEVICES[@]} profiles"
    for ((i=start;i<end;i++)); do
      fam="${TUI_FAMILIES[$i]}"
      mark=' '
      [[ "$i" == "$pos" ]] && mark='>'
      printf ' %s %-28s %4d devices\n' "$mark" "$fam" "${TUI_FAMILY_COUNTS[$fam]}"
    done
    printf '\n ↑↓ select   Enter open   PgUp/PgDn   Home/End   q quit\n'

    key="$(_tui_key)"
    case "$key" in
      up) ((pos>0)) && pos=$((pos-1));;
      down) ((pos<total-1)) && pos=$((pos+1));;
      home) pos=0;; end) pos=$((total-1));;
      pageup) pos=$((pos-max_rows)); ((pos<0)) && pos=0;;
      pagedown) pos=$((pos+max_rows)); ((pos>total-1)) && pos=$((total-1));;
      enter) TUI_RESULT="${TUI_FAMILIES[$pos]}"; return 0;;
      back|quit) TUI_RESULT=''; return 1;;
    esac
  done
}

_tui_device_menu(){
  local family="$1"
  mapfile -t TUI_ROWS < <(printf '%s' "${TUI_FAMILY_ROWS[$family]}" | sed '/^$/d')
  local total=${#TUI_ROWS[@]}
  (( total > 0 )) || return 1
  local pos=0 key lines max_rows start end i name platform arch desc conf pcsc tc mark width
  while :; do
    lines="$(_tui_lines)"
    max_rows=$((lines-13)); (( max_rows < 4 )) && max_rows=4
    start=0
    (( pos >= max_rows )) && start=$((pos-max_rows+1))
    end=$((start+max_rows)); (( end > total )) && end=$total
    IFS='|' read -r name platform arch desc conf pcsc tc <<< "${TUI_ROWS[$pos]}"
    width=$(( $(_tui_cols) - 18 )); (( width < 24 )) && width=24

    _tui_header "$family" "$total devices"
    printf ' Selected: %s\n' "$desc"
    printf ' ID=%s  arch=%s  pcsc=%s\n\n' "$name" "$arch" "$pcsc"
    for ((i=start;i<end;i++)); do
      IFS='|' read -r name platform arch desc conf pcsc tc <<< "${TUI_ROWS[$i]}"
      mark=' '
      [[ "$i" == "$pos" ]] && mark='>'
      printf ' %s %3d  %s\n' "$mark" "$((i+1))" "$(_tui_trim "$desc" "$width")"
    done
    printf '\n ↑↓ select   Enter actions   b back   q quit\n'

    key="$(_tui_key)"
    case "$key" in
      up) ((pos>0)) && pos=$((pos-1));;
      down) ((pos<total-1)) && pos=$((pos+1));;
      home) pos=0;; end) pos=$((total-1));;
      pageup) pos=$((pos-max_rows)); ((pos<0)) && pos=0;;
      pagedown) pos=$((pos+max_rows)); ((pos>total-1)) && pos=$((total-1));;
      enter) TUI_RESULT="${TUI_ROWS[$pos]}"; return 0;;
      back) return 2;;
      quit) TUI_RESULT=''; return 1;;
    esac
  done
}

_tui_action_menu(){
  local row="$1" action=0 key name platform arch desc conf pcsc tc
  IFS='|' read -r name platform arch desc conf pcsc tc <<< "$row"
  local -a actions=('Build' 'Edit configuration' 'Toolchain' 'Clean build' 'Back')
  local n=${#actions[@]} i mark
  while :; do
    _tui_header "$desc" "Device: $name"
    printf ' platform   %s\n arch        %s\n config      %s\n pcsc        %s\n toolchain   %s\n\n' "$platform" "$arch" "$conf" "$pcsc" "$tc"
    for ((i=0;i<n;i++)); do
      mark=' '
      [[ "$i" == "$action" ]] && mark='>'
      printf ' %s %d  %s\n' "$mark" "$((i+1))" "${actions[$i]}"
    done
    printf '\n ↑↓ select   Enter run   b back   q quit\n'
    key="$(_tui_key)"
    case "$key" in
      up) ((action>0)) && action=$((action-1));;
      down) ((action<n-1)) && action=$((action+1));;
      home) action=0;; end) action=$((n-1));;
      enter)
        case "$action" in
          0) TUI_RESULT='build';;
          1) TUI_RESULT='config';;
          2) TUI_RESULT='toolchain';;
          3) TUI_RESULT='clean';;
          4) TUI_RESULT='back';;
        esac
        return 0;;
      back) TUI_RESULT='back'; return 0;;
      quit) TUI_RESULT='quit'; return 0;;
    esac
  done
}

run_tui(){
  local family row action rc
  while :; do
    _tui_family_menu || { _tui_stop; return 0; }
    family="$TUI_RESULT"
    while :; do
      if _tui_device_menu "$family"; then
        rc=0
      else
        rc=$?
      fi
      [[ "$rc" == 1 ]] && { _tui_stop; return 0; }
      [[ "$rc" == 2 ]] && break
      row="$TUI_RESULT"
      while :; do
        _tui_action_menu "$row"
        action="$TUI_RESULT"
        case "$action" in
          back) break;;
          quit) _tui_stop; return 0;;
          build)
            load_defaults "$row"
            save_state
            _tui_stop
            if build_device; then
              rc=0
            else
              rc=$?
            fi
            printf '\n'
            _tui_pause
            [[ "$rc" != 0 ]] && return "$rc"
            ;;
          config)
            load_defaults "$row"
            _tui_stop
            if interactive; then
              rc=0
            else
              rc=$?
            fi
            save_state
            _tui_start || return 1
            ;;
          toolchain)
            load_defaults "$row"
            _tui_stop
            if [[ "$TOOLCHAIN" == native ]]; then
              ok 'Native compiler - nothing to download'
              rc=0
            elif [[ "$TOOLCHAIN" == manual ]]; then
              err 'Manual toolchain: use --toolchain-dir /path/to/sdk'
              rc=1
            else
              if fetch_toolchain "$TOOLCHAIN"; then
                rc=0
              else
                rc=$?
              fi
            fi
            _tui_pause
            [[ "$rc" != 0 ]] && return "$rc"
            ;;
          clean)
            load_defaults "$row"
            _tui_stop
            if clean_device; then
              rc=0
            else
              rc=$?
            fi
            _tui_pause
            [[ "$rc" != 0 ]] && return "$rc"
            ;;
        esac
      done
    done
  done
}
