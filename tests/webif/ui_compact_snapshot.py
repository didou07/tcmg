#!/usr/bin/env python3
from pathlib import Path
import re
from asset_utils import embedded_asset

root = Path(__file__).resolve().parents[2]
css = embedded_asset(root, 'css.h', 'TCMG_CSS')
core = (root / 'webif/core.c').read_text()
config = (root / 'webif/pages/config.c').read_text()
power = (root / 'webif/pages/power.c').read_text()
files = (root / 'webif/pages/config.c').read_text()
livelog = (root / 'webif/pages/files.c').read_text()
readers = (root / 'webif/pages/readers.c').read_text()
readers_js = embedded_asset(root, 'js_readers.h', 'TCMG_READERS_JS')
users = (root / 'webif/pages/users.c').read_text()
users_js = embedded_asset(root, 'js_users.h', 'TCMG_USERS_JS')

checks = []
def check(name, cond):
    checks.append((name, bool(cond)))

check('Users uses Last 60s, not First login', 'First login' not in users and 'Last 60s' in users and 'last_60s' in users_js and 'last60' in users_js)
check('Readers uses shared SVG toggle', 'ICON("i-toggle")' in readers and '#i-toggle' in readers_js)
check('Config has tabbed panels', "class='cfg-tabs'" in config and 'cfgPanel-security' in config and 'function cfgTab' in config)
check('Files has four tabs', "class='file-tabs'" in files and files.count("class='ctab") == 4)
check('Live Log has collapsible settings', "class='ll-settings'" in livelog and 'Show settings' in livelog and "id='lw'" in livelog)

check('Users has no pagination footer', "id='uCount'" not in users and "id='limit'" not in users and 'per page' not in users)
check('Readers bottom totals removed', 'rCount' not in readers and 'rViewStat' not in readers and 'rSync' not in readers)
check('Users expiry status dot removed', 'sdot' not in users and 'sdot.' not in css)
check('Dashboard kill icon uses user-x', 'i-user-x' in embedded_asset(root, 'icons.h', 'GLOBAL_ICON_SPRITE') and 'i-user-x' in embedded_asset(root, 'js_common.h', 'TCMG_JS'))
check('Dashboard CAID is plain', 'badge bbl c-caid' not in embedded_asset(root, 'js_common.h', 'TCMG_JS'))
check('FailBan duration is human-readable', 'fmt_ban_duration' in (root / 'webif/pages/system.c').read_text())
check('FailBan duration max is one week', 'max=\'604800\'' in config and 'Math.min(604800' in config)

check('Mobile menu wiring is singular', core.count("id='mnuBtn'") == 1 and core.count("id='mobile-nav'") == 1 and core.count('function toggleMobileNav') == 1)

check('portrait mode has no width gate', '@media(orientation:portrait){' in css and not re.search(r'@media\([^)]*(?:max|min)-width', css))
check('landscape keeps desktop base without width/pointer gate', 'orientation:landscape' in css and not re.search(r'@media\([^)]*(?:max|min)-width|pointer:coarse', css))
check('no legacy mobile patch markers', not re.search(r'6\.0\.[23]|chocolate-box|mobile card pass|status-card system', css))
check('single Users card root rule', len(re.findall(r'body\.pg-users \.ut \.urow\{display:grid', css)) == 1)
check('single Readers card root rule', len(re.findall(r'body\.pg-readers \.rt \.rrow\{display:grid', css)) == 1)
check('single Dashboard session card root rule', len(re.findall(r'body\.pg-status #p_clients \.nw\{display:grid', css)) == 1)

check('Users use a 2-column value grid', "grid-template-columns:repeat(2,minmax(0,1fr));grid-template-rows:40px minmax(48px,auto) minmax(48px,auto);grid-template-areas:'user user' 'ip country' 'ok exp'" in css)
check('Readers use a 3-column value grid', "grid-template-columns:repeat(3,minmax(0,1fr));grid-template-rows:40px minmax(48px,auto) minmax(48px,auto);grid-template-areas:'label label label' 'proto dev group' 'caid ok nok'" in css)
check('Dashboard sessions use a 3-column value grid', "grid-template-columns:repeat(3,minmax(0,1fr));grid-template-rows:40px minmax(48px,auto) minmax(48px,auto);grid-template-areas:'user user user' 'ip caid sid' 'ch con idle'" in css)
check('Users hide nonessential cells', all(x in css for x in ('c-conn','c-caid','c-nok','c-last60','c-proto','c-idle','c-last')))
check('Users keep toggle and delete', 'body.pg-users .ut .act-b.ed,body.pg-users .ut .act-b.rs{display:none!important}' in css and 'body.pg-users .ut .act-b.dl{display:flex!important' in css)
check('Readers keep state and actions', 'body.pg-readers .rt .c-rstate' in css and 'body.pg-readers .rt .c-btn' in css and 'body.pg-readers .rt .act-b.ed' in css)
check('Card colors follow state', all(x in css for x in ("body.pg-users .ut .urow[data-active='1']", "body.pg-users .ut .urow[data-expd='1']", "body.pg-users .ut .urow[data-en='0']", "body.pg-readers .rt .rrow[data-active='1']", "body.pg-readers .rt .rrow[data-enabled='0']")))

check('Files editor uses viewport height', 'body.pg-files #fileArea' in css and 'height:calc(100vh - 210px)' in css)
check('Config fields are one-per-row on phone', 'body.pg-config .cfg-field{display:grid!important;grid-template-columns:1fr!important' in css)
check('FailBan rows become cards', 'body.pg-failban #fbBody .fbrow{display:grid!important' in css and 'body.pg-failban .tw thead{display:none}' in css)
check('TVCAS results stack', 'body.pg-tvcas .tv-tbl tr{display:block' in css and 'body.pg-tvcas .tv-split{display:flex;flex-direction:column}' in css)
check('Live Log controls stay below the log', 'body.pg-livelog .ll-controls{display:grid' in css and 'body.pg-livelog .ll-settings-body' in css)
check('Modals stack on narrow screens', '.reader-topgrid,.reader-cccam-grid,.reader-pcsc-grid,.reader-routing-grid,.user-g3,.user-bottom-grid,.g2{grid-template-columns:1fr!important}' in css and '.reader-modal .mf,.user-modal .mf{flex-direction:column' in css)
check('Mobile menu overlay is below menu', 'body.nav-open #mn::before{content:\'\';position:fixed;z-index:1035' in css and '.tnav{display:none;position:fixed' in css and 'z-index:1065' in css)
check('Desktop modal base remains intact', '.reader-modal{width:640px;max-width:calc(100vw - 24px)}' in css)
check('Desktop Users page keeps document scrolling enabled', 'body.pg-users{height:auto;min-height:100vh;overflow-x:clip;overflow-y:visible;padding-top:var(--tbh)}' in css)
check('Desktop Readers table is capped and centered', '#rTable{flex:0 1 auto;width:920px;max-width:100%;min-width:0;min-height:0;height:auto;max-height:none;margin:0 auto' in css)
check('Readers desktop table fills capped wrapper', '#rTable table{width:100%;max-width:none;min-width:700px;table-layout:fixed}' in css)
check('Hidden Users and Readers rows remain hidden in card mode', 'body.pg-users .ut .urow[hidden],body.pg-readers .rt .rrow[hidden]{display:none!important}' in css)
check('Mobile Readers table does not reserve empty viewport space', 'body.pg-readers #rTable{flex:none;width:100%;max-width:100%;min-width:0;max-height:none;height:auto' in css)

failed = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(('PASS ' if ok else 'FAIL ') + name)
print('FAILED:', len(failed), failed)
raise SystemExit(1 if failed else 0)
