#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 - "$ROOT" <<'PY'
from pathlib import Path
import re, sys
root=Path(sys.argv[1])
checks={
    "src/serial/serial.c": "conax_ecm",
    "src/internal/internal.c": "internal_conax_ecm",
    "src/pcsc/pcsc.c": "pcsc_conax_ecm_locked",
}
for rel, fn in checks.items():
    text=(root/rel).read_text()
    start=text.find(fn)
    if start < 0:
        raise SystemExit(f"missing {fn} in {rel}")
    body=text[start:]
    old_pos=body.find("if (!allow_ca)")
    ca_pos=body.find("0xDD, 0xCA")
    if ca_pos < 0:
        ca_pos=body.find("0xDD,0xCA")
    if old_pos < 0 or ca_pos < 0 or old_pos > ca_pos:
        raise SystemExit(f"Old ECM CA guard missing in {rel}")
    if not re.search(r"reader_old_ecm_due\([^;\n]+", text):
        raise SystemExit(f"Old ECM scheduler missing in {rel}")
print("OLD_ECM_SOURCE_SMOKE: PASS")
PY
