#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
! grep -q "rInternalPoll" "$ROOT/webif/pages/readers.c"
! grep -q "rInternalPoll" "$ROOT/webif/assets/js_readers.h"
! grep -q "protocol === 'internal'.*POLL_MS" "$ROOT/webif/assets/js_readers.h"
printf '%s\n' 'INTERNAL_UI: PASS'
! grep -q "LOCKED" "$ROOT/webif/assets/js_readers.h"
! grep -q ">OPEN<" "$ROOT/webif/pages/readers.c"
[ "$(grep -c "function makeRow(reader)" "$ROOT/webif/assets/js_readers.h")" -eq 1 ]
[ "$(grep -c "function saveReader()" "$ROOT/webif/assets/js_readers.h")" -eq 1 ]
printf "%s\n" "READERS_UI: PASS"
