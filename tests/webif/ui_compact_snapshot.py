#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
css = (root / 'webif/assets/css.h').read_text()
core = (root / 'webif/core.c').read_text()
config = (root / 'webif/pages/config.c').read_text()
power = (root / 'webif/pages/power.c').read_text()

checks = []
def check(name, cond):
    checks.append((name, bool(cond)))

check('compact dashboard six columns', 'grid-template-columns:repeat(6,minmax(0,1fr))' in css)
check('dashboard card compact padding', '.sc{padding:10px 11px!important' in css)
check('dashboard value compact', '.sv{font-size:18px!important' in css)
check('global help question marker', '.qtip{' in css and 'textContent=\'?\'' in core)
check('long config intro removed', "document.querySelectorAll('.cfg-intro').forEach(function(el){el.remove();});" in core)
check('config help converted to qtip', "document.querySelectorAll('.cfg-help,.fhint')" in core)
check('config subtitle converted to qtip', "'.cfg-sub'" in core and '.file-sub' not in core)
check('bounded numeric fields compact', '.cfg-input[type=number]{width:104px' in css)
check('bounded webif numeric fields compact', '.fi[type=number]{width:100px' in css)
check('config wide 3-column layout', '@media(min-width:1100px){.cfg-grid{grid-template-columns:repeat(3' in config)
check('power uses compact help markers', "class='qtip' title='Drops active connections" in power and "class='qtip' title='Stops all connections" in power)
check('no giant power card headings', "font-size:16px" not in power)

failed = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(('PASS ' if ok else 'FAIL ') + name)
print('FAILED:', len(failed), failed)
raise SystemExit(1 if failed else 0)
