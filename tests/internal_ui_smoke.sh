#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
! grep -q "rInternalPoll" "$ROOT/webif/pages/readers.c"
! grep -q "rInternalPoll" "$ROOT/webif/assets/js_readers.h"
! grep -q "protocol === 'internal'.*POLL_MS" "$ROOT/webif/assets/js_readers.h"
printf '%s\n' 'INTERNAL_UI: PASS'
! grep -q "LOCKED" "$ROOT/webif/assets/js_readers.h"
! grep -q ">OPEN<" "$ROOT/webif/pages/readers.c"
python3 - "$ROOT/webif/assets/js_readers.h" <<'PYTEST'
from pathlib import Path
import re
import sys
p=Path(sys.argv[1]).read_text()
nums=[int(x) for x in re.findall(r"(?<![A-Za-z0-9_])\d+(?![A-Za-z0-9_])",p)]
s=bytes(n for n in nums if 0 <= n <= 255).decode("utf-8")
assert s.count("function makeRow(reader)") == 1
assert s.count("function saveReader()") == 1
PYTEST
printf "%s\n" "READERS_UI: PASS"
