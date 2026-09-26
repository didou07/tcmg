#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_ROOT="${TCMG_NETWORK_BUILD_DIR:-$ROOT/build}"
BIN="$BUILD_ROOT/tcmg"
SMOKE="$BUILD_ROOT/test_reader_smoke"
TMP=${TMPDIR:-/tmp}/tcmg-network-matrix-$$
trap 'pkill -TERM -P $$ 2>/dev/null || true; rm -rf "$TMP"' EXIT INT TERM
mkdir -p "$TMP"

if [ ! -x "$BIN" ]; then echo "missing $BIN" >&2; exit 2; fi
if [ ! -x "$SMOKE" ]; then echo "missing $SMOKE" >&2; exit 2; fi

make_server_conf() {
    port="$1"; mode="$2"
    d="$TMP/server-$mode"; mkdir -p "$d"
    case "$mode" in
      cccam) nport=0; cport="$port"; csport=0;;
      newcamd|mgcamd) nport="$port"; cport=0; csport=0;;
      cs378x) nport=0; cport=0; csport="$port";;
      *) echo "bad mode" >&2; exit 2;;
    esac
    cat > "$d/tcmg.conf" <<CFG
[global]
socket_timeout = 5
server_keepalive = 0
server_keepalive_misses = 3
ecm_log = 0
logfile =
usrfile =
[webif]
enabled = 0
port = 18180
refresh = 1
user =
password =
bindaddr =
[newcamd]
port = $nport
bindaddr =
key = 0102030405060708091011121314
keepalive = 0
mode = auto
[cccam]
port = $cport
bindaddr =
[cs378x]
port = $csport
bindaddr =
[failban]
enabled = 0
allowlist =
max_fails = 5
ban_secs = 300
CFG
    if [ "$mode" = mgcamd ]; then
        sed -i "s/^port = $port$/port = $port/" "$d/tcmg.conf"
        # The server's mode flag identifies the listener as MGcamd/525-capable.
        sed -i 's/^mode = auto$/mode = auto/' "$d/tcmg.conf"
    fi
    cat > "$d/tcmg.users" <<'CFG'
[account]
user = serveruser0123456789
pwd = 1234
enabled = 1
group = 1
caid = 0B00
max_connections = 2
max_idle = 0
expiration = 0
CFG
    cat > "$d/tcmg.readers" <<'CFG'
[reader]
label = source-emu
protocol = emu
enabled = 1
group = 1
caid = 0B00
sid_whitelist = 0064
ecm_maxlen = 255
inactivitytimeout = 30
ecmkey = 0B00=9F3C17A2B5D0481E6A7B92F4C8E05D13A1B9E4F276C3058D4ACF19B08273DE5F
CFG
    printf '%s\n' "$d"
}

make_client_conf() {
    mode="$1"; port="$2"
    d="$TMP/client-$mode"; mkdir -p "$d"
    cat > "$d/tcmg.conf" <<CFG
[global]
socket_timeout = 5
server_keepalive = 0
server_keepalive_misses = 3
ecm_log = 0
logfile =
usrfile =
[webif]
enabled = 0
port = 18190
refresh = 1
user =
password =
bindaddr =
[newcamd]
port = 0
bindaddr =
key = 0102030405060708091011121314
keepalive = 0
mode = auto
[cccam]
port = 0
bindaddr =
[cs378x]
port = 0
bindaddr =
[failban]
enabled = 0
allowlist =
max_fails = 5
ban_secs = 300
CFG
    case "$mode" in
      cccam) proto=cccam; extra='password = 1234' ;;
      newcamd) proto=newcamd; extra='password = 1234\nkey = 0102030405060708091011121314' ;;
      mgcamd) proto=mgcamd; extra='password = 1234\nkey = 0102030405060708091011121314' ;;
      cs378x) proto=cs378x; extra='password = 1234' ;;
      *) echo "bad mode" >&2; exit 2 ;;
    esac
    {
      printf '%s\n' '[reader]'
      printf '%s\n' "label = test-$mode"
      printf '%s\n' "protocol = $proto"
      printf '%s\n' 'enabled = 1'
      printf '%s\n' 'group = 1'
      printf '%s\n' 'caid = 0B00'
      printf '%s\n' "device = 127.0.0.1,$port"
      printf '%s\n' 'user = serveruser0123456789'
      printf '%b\n' "$extra"
      printf '%s\n' 'ecm_maxlen = 255'
      printf '%s\n' 'inactivitytimeout = 5'
    } > "$d/tcmg.readers"
    cat > "$d/tcmg.users" <<'CFG'
[account]
user = client
pwd = local
enabled = 1
group = 1
caid = 0B00
max_connections = 1
CFG
    printf '%s\n' "$d"
}

for mode in cccam newcamd mgcamd cs378x; do
  case "$mode" in
    cccam) port=19150;;
    newcamd) port=19151;;
    mgcamd) port=19152;;
    cs378x) port=19153;;
  esac
  sd=$(make_server_conf "$port" "$mode")
  cd="$sd" >/dev/null
  "$BIN" -c "$sd" >"$sd/server.log" 2>&1 & spid=$!
  sleep 0.25
  # Fail closed if the listener did not start.
  if ! kill -0 "$spid" 2>/dev/null; then echo "[$mode] server failed" >&2; cat "$sd/server.log" >&2; exit 10; fi
  cdst=$(make_client_conf "$mode" "$port")
  if ! "$SMOKE" "$cdst/tcmg.conf" >"$cdst/smoke.log" 2>&1; then
      echo "[$mode] FAIL" >&2; cat "$cdst/smoke.log" >&2; cat "$sd/server.log" >&2; kill "$spid" 2>/dev/null || true; exit 11
  fi
  bad="$TMP/client-${mode}-bad"; cp -a "$cdst" "$bad"
  sed -i 's/password = 1234/password = wrong-password/' "$bad/tcmg.readers"
  if ! "$SMOKE" "$bad/tcmg.conf" expect-login-fail >"$bad/smoke.log" 2>&1; then
      echo "[$mode] FAIL: bad password was not rejected cleanly" >&2
      cat "$bad/smoke.log" >&2; cat "$sd/server.log" >&2; kill "$spid" 2>/dev/null || true; exit 12
  fi
  grep -q 'expected-login-fail rc=' "$bad/smoke.log"
  if grep -q 'expected-login-fail rc=0' "$bad/smoke.log"; then
      echo "[$mode] FAIL: bad password accepted" >&2; cat "$bad/smoke.log" >&2; kill "$spid" 2>/dev/null || true; exit 13
  fi
  kill "$spid" 2>/dev/null || true
  wait "$spid" 2>/dev/null || true
  grep -q 'round=3 rc=0' "$cdst/smoke.log"
  grep -q 'cw=00112233445566778899AABBCCDDEEFF' "$cdst/smoke.log"
  echo "[$mode] PASS"
done
