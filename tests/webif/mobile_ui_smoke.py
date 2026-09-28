import os, requests, shutil, re
from playwright.sync_api import sync_playwright

BASE = os.environ.get('TCMG_WEBIF_URL', 'http://127.0.0.1:18080')
PAGES = ['/status','/users','/readers','/config','/livelog','/failban','/files','/tvcas','/power']
WIDTHS = (320, 360, 390, 430)
FAILS = []

def ok(name, cond, detail=''):
    print(('PASS ' if cond else 'FAIL ') + name + ((' -> ' + str(detail)) if not cond else ''))
    if not cond:
        FAILS.append(name)

def inline_page(path):
    html = requests.get(BASE + path, timeout=10).text
    css = requests.get(BASE + "/assets/app.css", timeout=10).text
    js_names = ("app.js","livelog.js","users.js","readers.js","config.js","files.js","tvcas.js","power.js")
    html = re.sub(r"<link[^>]+href=['\"]/assets/app\.css[^>]*>", lambda m: "<style>" + css + "</style>", html)
    for name in js_names:
        try:
            body = requests.get(BASE + "/assets/" + name, timeout=10).text
        except Exception:
            continue
        html = re.sub(r"<script[^>]+src=['\"]/assets/" + re.escape(name) + r"(?:\?[^'\"]*)?['\"][^>]*></script>", lambda m: "<script>" + body + "</script>", html)
    return html

def sample_user_row():
    return """<tr class='urow'><td class='c-en'><button class='pw-btn on'></button></td><td class='c-user'><button class='ulink'>alice</button></td><td class='c-conn'>1/2</td><td class='c-ip'>10.0.0.4</td><td class='c-country'>DZ</td><td class='c-caid'>0B00</td><td class='c-ok tg'>124</td><td class='c-nok dim'>2</td><td class='c-proto'><span class='badge'>NC</span></td><td class='c-idle'>4s</td><td class='c-last60 tg'>18</td><td class='c-last'>4s</td><td class='c-exp'>2027-09-01</td><td class='c-btn'><div class='ba'><button class='act-b ed'></button><button class='act-b rs'></button><button class='act-b dl'></button></div></td></tr>"""

def sample_reader_row():
    return """<tr class='rrow'><td class='c-rstate'><button class='pw-btn on'></button></td><td class='c-rlabel'><button class='rlink'>local-reader</button></td><td class='c-rproto'><span class='badge'>PCSC</span></td><td class='c-rdev mono'>/dev/ttyUSB0</td><td class='c-rgroup'>1</td><td class='c-rcaid'>0B00</td><td class='c-rstat-ok'>127</td><td class='c-rstat-nok'>1</td><td class='c-btn'><div class='ba'><button class='act-b ed'></button><button class='act-b dl'></button></div></td></tr>"""

for path in PAGES:
    r = requests.get(BASE + path, timeout=10)
    ok('HTTP ' + path, r.status_code == 200 and len(r.text) > 500, r.status_code)

with sync_playwright() as p:
    exe = shutil.which('chromium') or shutil.which('chromium-browser') or shutil.which('google-chrome')
    b = p.chromium.launch(executable_path=exe) if exe else p.chromium.launch()
    for width in WIDTHS:
        for path in PAGES:
            page = b.new_page(viewport={'width': width, 'height': 844}, reduced_motion='reduce')
            page.set_content(inline_page(path), wait_until='domcontentloaded')
            page.wait_for_timeout(120)
            dims = page.evaluate("""() => ({
                sw: document.documentElement.scrollWidth,
                cw: document.documentElement.clientWidth,
                bw: document.body.scrollWidth
            })""")
            ok(f'{path} {width}px no horizontal overflow', dims['sw'] <= dims['cw'] + 1, dims)

            if path == '/status':
                page.locator('#mnuBtn').click()
                ok(f'{path} {width}px mobile menu opens', 'open' in (page.locator('.tnav').get_attribute('class') or ''))
                ok(f'{path} {width}px nav aria expanded', page.locator('#mnuBtn').get_attribute('aria-expanded') == 'true')
                first_nav = page.locator('.tnav.open a').first
                clickable = page.evaluate("el => { const r=el.getBoundingClientRect(); const x=Math.floor(r.left+r.width/2), y=Math.floor(r.top+r.height/2); const hit=document.elementFromPoint(x,y); return !!hit && (hit===el || el.contains(hit)); }", first_nav.element_handle()) if first_nav.count() else False
                ok(f'{path} {width}px nav links are clickable', clickable)
                page.keyboard.press('Escape')
                ok(f'{path} {width}px mobile menu closes', 'open' not in (page.locator('.tnav').get_attribute('class') or ''))
                ok(f'{path} {width}px nav body unlocks', 'nav-open' not in (page.locator('body').get_attribute('class') or ''))

            if path == '/users':
                page.locator('#uStats').first.click(button='left') if page.locator('#uStats').count() else None
                page.locator('#usrBody').evaluate("(el, html) => { el.innerHTML = html; }", sample_user_row())
                row = page.locator('#usrBody tr.urow').first
                ok(f'{path} {width}px mobile row uses grid card', row.evaluate("el => getComputedStyle(el).display === 'grid'"))
                ok(f'{path} {width}px user row fits viewport', bool((row.bounding_box() or {}).get('width', 0) <= width - 16))
                for cls in ('c-user','c-ip','c-country','c-ok','c-exp','c-en','c-btn'):
                    ok(f'{path} {width}px shows {cls}', page.locator('#usrBody tr.urow ' + '.' + cls).first.evaluate("el => getComputedStyle(el).display !== 'none'"))
                for cls in ('c-conn','c-caid','c-nok','c-proto','c-idle','c-last60','c-last'):
                    ok(f'{path} {width}px hides {cls}', page.locator('#usrBody tr.urow ' + '.' + cls).first.evaluate("el => getComputedStyle(el).display === 'none'"))
                ok(f'{path} {width}px shows delete only', page.locator('#usrBody tr.urow .act-b.dl').first.evaluate("el => getComputedStyle(el).display !== 'none'") and page.locator('#usrBody tr.urow .act-b.ed').first.evaluate("el => getComputedStyle(el).display === 'none'") and page.locator('#usrBody tr.urow .act-b.rs').first.evaluate("el => getComputedStyle(el).display === 'none'"))
                page.locator("[data-a='add']").first.click()
                box = page.locator('#uModal .mc').bounding_box()
                ok(f'{path} {width}px Add User modal fits', bool(box and box['width'] <= width and box['height'] <= 836), box)
                page.keyboard.press('Escape')

            if path == '/readers':
                page.locator('#rBody').evaluate("(el, html) => { el.innerHTML = html; }", sample_reader_row())
                row = page.locator('#rBody tr.rrow').first
                ok(f'{path} {width}px mobile row uses grid card', row.evaluate("el => getComputedStyle(el).display === 'grid'"))
                ok(f'{path} {width}px reader row fits viewport', bool((row.bounding_box() or {}).get('width', 0) <= width - 16))
                page.get_by_role('button', name='Add Reader').first.click()
                box = page.locator('#rModal .mc').bounding_box()
                ok(f'{path} {width}px Add Reader modal fits', bool(box and box['width'] <= width and box['height'] <= 836), box)
                page.keyboard.press('Escape')

            if path == '/files':
                tabs = page.locator('.file-tabs .ctab')
                ok(f'{path} {width}px file tabs present', tabs.count() == 4, tabs.count())
                box = page.locator('.file-tabs').bounding_box()
                ok(f'{path} {width}px file tabs fit', bool(box and box['width'] <= width - 12), box)

            page.close()
    b.close()

print('FAILED:', len(FAILS), FAILS)
raise SystemExit(1 if FAILS else 0)
