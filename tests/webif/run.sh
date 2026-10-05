#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="${TMPDIR:-/tmp}/tcmg-webif-ui-$$"
CONF_SET=0
if [ -n "${CONF+x}" ]; then CONF_SET=1; fi
CONF="${CONF:-$TMP/config}"
H="${HARNESS:-$ROOT/build/tcmg}"
case "$H" in
  /*) ;;
  *) H="$ROOT/$H" ;;
esac
if [ ! -x "$H" ]; then
  echo "WebIF test binary not found/executable: $H" >&2
  exit 2
fi
created_conf=0
if [ ! -f "$CONF/tcmg.conf" ]; then
  if [ "$CONF_SET" -eq 1 ]; then
    echo "WebIF test config not found: $CONF/tcmg.conf" >&2
    exit 2
  fi
  mkdir -p "$CONF"
  cat > "$CONF/tcmg.conf" <<CFG
[global]
socket_timeout = 5
server_keepalive = 0
server_keepalive_misses = 3
ecm_log = 0
logfile =
usrfile =
[webif]
enabled = 1
port = 18080
refresh = 1
user =
password =
bindaddr = 127.0.0.1
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
  cat > "$CONF/tcmg.users" <<'USR'
[account]
user = client
pwd = one
enabled = 1
group = 1
caid = 0B00
expiration = 0
USR
  : > "$CONF/tcmg.readers"
  created_conf=1
fi
cleanup() {
  kill "$HP" 2>/dev/null || true
  wait "$HP" 2>/dev/null || true
  [ "$created_conf" -eq 1 ] && rm -rf "$CONF" "$TMP" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
setsid "$H" -c "$CONF" > /tmp/tcmg-webif-ui.log 2>&1 &
HP=$!
for i in $(seq 1 60); do
  if curl -fsS -o /dev/null http://127.0.0.1:18080/login; then break; fi
  if ! kill -0 "$HP" 2>/dev/null; then
    cat /tmp/tcmg-webif-ui.log >&2 || true
    exit 1
  fi
  sleep 0.2
done
if ! curl -fsS -o /dev/null http://127.0.0.1:18080/login; then
  cat /tmp/tcmg-webif-ui.log >&2 || true
  exit 1
fi
export TCMG_TEST_DIR="$CONF"
python3 "$@"
if grep -q 'ERROR: AddressSanitizer\|runtime error' /tmp/tcmg-webif-ui.log 2>/dev/null; then
  grep -m3 -A6 'ERROR: AddressSanitizer\|runtime error' /tmp/tcmg-webif-ui.log >&2 || true
  exit 1
fi
