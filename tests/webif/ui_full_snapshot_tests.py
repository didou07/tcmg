import os, re, shutil, requests, html as htmlmod
from playwright.sync_api import sync_playwright

BASE = os.environ.get('TCMG_WEBIF_URL', 'http://127.0.0.1:18080')
PAGES = ['/status','/users','/readers','/livelog','/config','/failban','/files','/tvcas','/power']
FAILS=[]
def ok(name, cond, detail=''):
    print(('PASS ' if cond else 'FAIL ') + name + ((' -> ' + str(detail)) if not cond else ''))
    if not cond: FAILS.append(name)

def inline(html, path):
    css_url = BASE + '/assets/app.css'
    css_r = S.get(css_url, timeout=10)
    if css_r.status_code != 200:
        raise RuntimeError(f'asset app.css: HTTP {css_r.status_code}')
    css = css_r.text
    html = re.sub(
        r"<link[^>]+href=['\"](/assets/app\.css(?:\?[^'\"]*)?)['\"][^>]*>",
        lambda m: '<style>' + css + '</style>',
        html, count=1, flags=re.I)
    refs = []
    for src in re.findall(r"<script[^>]+src=['\"](/assets/[^'\"]+)['\"][^>]*></script>", html, flags=re.I):
        src = src.split('?', 1)[0]
        if src not in refs:
            refs.append(src)
    for src in refs:
        r = S.get(BASE + src, timeout=10)
        if r.status_code != 200:
            raise RuntimeError(f'asset {src}: HTTP {r.status_code}')
        body = r.content.decode('utf-8', errors='strict')
        tag_re = re.compile(r"<script[^>]+src=['\"]" + re.escape(src) + r"(?:\?[^'\"]*)?['\"][^>]*></script>", re.I)
        html = tag_re.sub(lambda m: '<script>' + body + '</script>', html, count=1)
    return html


S=requests.Session()
login=S.post(BASE+'/login',data={'u':'admin','p':'secret'},timeout=10,allow_redirects=False)
ok('webif test login', login.status_code==302, login.status_code)

htmls={}
for path in PAGES:
    r=S.get(BASE+path, timeout=10)
    ok(f'HTTP {path}', r.status_code==200 and len(r.text)>500, r.status_code)
    htmls[path]=inline(r.text, path)

with sync_playwright() as p:
    exe=shutil.which('chromium') or shutil.which('chromium-browser') or shutil.which('google-chrome')
    b=p.chromium.launch(executable_path=exe) if exe else p.chromium.launch()
    for path, html in htmls.items():
        for width,height,label in ((1440,900,'desktop'),(390,844,'mobile')):
            page=b.new_page()
            page.set_viewport_size({'width': width, 'height': height})
            page.emulate_media(reduced_motion='reduce')
            errors=[]; page.on('pageerror', lambda e: errors.append(str(e)))
            page.set_content(html, wait_until='commit')
            page.mouse.move(1,1)
            dims=page.evaluate("""() => ({sw:document.documentElement.scrollWidth, cw:document.documentElement.clientWidth,
                dup:[...document.querySelectorAll('[id]')].filter(x=>!x.closest('symbol')).map(x=>x.id).filter((x,i,a)=>a.indexOf(x)!==i),
                bodyH:document.body.scrollHeight, vh:window.innerHeight})""")
            ok(f'{path} {label} no horizontal overflow', dims['sw'] <= dims['cw'] + 2, dims)
            ok(f'{path} {label} no duplicate ids', not dims['dup'], dims['dup'])
            real_errors=[e for e in errors if 'sessionStorage' not in e]; ok(f'{path} {label} no page errors', not real_errors, real_errors)
            if path == '/users':
                page.get_by_role('button', name='Add User').first.click()
                visible=page.locator('#uModal').is_visible()
                box=page.locator('#uModal .mc').bounding_box()
                ok(f'{path} {label} user modal opens', visible)
                ok(f'{path} {label} user modal fits viewport', bool(box and box['width']<=width and box['height']<=height-8), box)
                page.keyboard.press('Escape')
                ok(f'{path} {label} user modal closes with Escape', not page.locator('#uModal').is_visible())
            elif path == '/readers':
                page.get_by_role('button', name='Add Reader').first.click()
                visible=page.locator('#rModal').is_visible()
                box=page.locator('#rModal .mc').bounding_box()
                ok(f'{path} {label} reader modal opens', visible)
                ok(f'{path} {label} reader modal fits viewport', bool(box and box['width']<=width and box['height']<=height-8), box)
                page.keyboard.press('Escape')
                ok(f'{path} {label} reader modal closes with Escape', not page.locator('#rModal').is_visible())
            elif path == '/files':
                tabs=page.locator('.file-tabs .ctab')
                ok(f'{path} {label} four file tabs', tabs.count()==4, tabs.count())
                for i in range(tabs.count()):
                    tabs.nth(i).click()
                    ok(f'{path} {label} tab {i} aria selection', tabs.nth(i).get_attribute('aria-selected')=='true')
                ok(f'{path} {label} reduced motion', page.evaluate("matchMedia('(prefers-reduced-motion: reduce)').matches"))
            page.close()
    b.close()

css_src=S.get(BASE+'/assets/app.css', timeout=10).text
for selector in ('.sc:hover::before', '.sc.bl:hover', '.sc.gr:hover', 'tbody tr:hover', '.cm tbody tr:hover'):
    ok(f'CSS hover selector present: {selector}', selector in css_src)
for bad in ('@media (hover:hover) and (pointer:fine){.sc:hover::before', '@media (hover:hover) and (pointer:fine){tbody tr:hover'):
    pass

print('FAILED:', len(FAILS), FAILS)
raise SystemExit(1 if FAILS else 0)
