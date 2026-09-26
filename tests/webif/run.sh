#!/bin/bash
# usage: [HARNESS=/path/to/tharness] [CONF=/tmp/tcu] run.sh <python-script> [args...]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CONF=${CONF:-/tmp/tcu}
H=${HARNESS:-"$ROOT/build/tharness"}
case "$H" in
  /*) ;;
  *) H="$ROOT/$H" ;;
esac
[ -x "$H" ] || { echo "Harness not found/executable: $H" >&2; exit 2; }
[ -f "$CONF/tcmg.conf.orig" ] && cp "$CONF/tcmg.conf.orig" "$CONF/tcmg.conf" || cp "$CONF/tcmg.conf" "$CONF/tcmg.conf.orig"
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=0}
setsid "$H" "$CONF" > /tmp/harness.log 2>&1 &
HP=$!
cleanup() { kill "$HP" 2>/dev/null || true; wait "$HP" 2>/dev/null || true; }
trap cleanup EXIT
for i in $(seq 1 40); do
  if curl -s -o /dev/null http://127.0.0.1:18080/login; then break; fi
  sleep 0.2
done
python3 "$@"
echo "--- sanitizer reports in harness log: $(grep -c 'ERROR: AddressSanitizer\|runtime error' /tmp/harness.log || true)"
grep -m3 -A6 'ERROR: AddressSanitizer\|runtime error' /tmp/harness.log | head -30 || true
