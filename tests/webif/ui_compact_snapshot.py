#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
css = (root / 'webif/assets/css.h').read_text()
core = (root / 'webif/core.c').read_text()
config = (root / 'webif/pages/config.c').read_text()
power = (root / 'webif/pages/power.c').read_text()
readers = (root / 'webif/pages/readers.c').read_text()
readers_js = (root / 'webif/assets/js_readers.h').read_text()

checks = []
def check(name, cond):
    checks.append((name, bool(cond)))

check('compact dashboard six columns', 'grid-template-columns:repeat(6,minmax(0,1fr))' in css)
check('dashboard card compact padding', '.sc{padding:10px 11px!important' in css)
check('dashboard value compact', '.sv{font-size:18px!important' in css)
check('global qtip styling', '.qtip{' in css)
check('config intro styling present', '.cfg-intro' in config)
check('config help styling present', '.cfg-help' in config)
check('config subtitle styling present', '.cfg-sub' in config)
check('bounded numeric fields compact', '.cfg-input[type=number]{width:104px' in css)
check('bounded webif numeric fields compact', '.fi[type=number]{width:100px' in css)
check('config wide 3-column layout', '@media(min-width:1100px){.cfg-grid{grid-template-columns:repeat(3' in config)
check('power uses compact help markers', "class='qtip' title='Drops active connections" in power and "class='qtip' title='Stops all connections" in power)
check('no giant power card headings', "font-size:16px" not in power)
check('readers show CW OK/CW NOK', 'CW OK' in readers and 'CW NOK' in readers and 'cw_ok' in readers_js and 'cw_nok' in readers_js)
check('active reader row is fully highlighted', ".rrow[data-active='1'] td" in css)
check('active user row is fully highlighted', ".urow[data-active='1'] td" in css)
check('legacy Tuning column removed', 'Tuning' not in readers and 'c-rtune' not in readers_js)

failed = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(('PASS ' if ok else 'FAIL ') + name)
print('FAILED:', len(failed), failed)
raise SystemExit(1 if failed else 0)
