import requests, os, re, sys
from playwright.sync_api import sync_playwright
BASE='http://127.0.0.1:18080'
S=requests.Session()
r=S.post(BASE+'/login',data={'u':'admin','p':'secret'},timeout=5,allow_redirects=False)
assert r.status_code==302, r.status_code

def inline(html):
    css=S.get(BASE+'/assets/app.css',timeout=5).text
    html=re.sub(r"<link[^>]+href=['\"]?/assets/app\.css[^>]*>", lambda m: '<style>'+css+'</style>', html, count=1)
    for name in ('app','users','readers','config','livelog','files','tvcas','power'):
        if ("/assets/"+name+'.js') in html:
            js=S.get(BASE+'/assets/'+name+'.js',timeout=5).text
            html=re.sub(r"<script src='/assets/"+re.escape(name)+r"\.js\?v=[^']+' defer></script>", lambda m: '<script>'+js+'</script>', html)
            html=re.sub(r"<script[^>]+src=['\"]/assets/"+re.escape(name)+r"\.js[^>]*></script>", lambda m: '<script>'+js+'</script>', html, count=1)
    return html

pages={}
for path in ('/status','/users','/readers'):
    rr=S.get(BASE+path,timeout=5)
    assert rr.status_code==200
    pages[path]=inline(rr.text)

with sync_playwright() as p:
    exe=next((x for x in ('/usr/bin/chromium','/usr/bin/chromium-browser','/usr/bin/google-chrome') if os.path.exists(x)),None)
    b=p.chromium.launch(executable_path=exe)
    for w in (320,390,430):
        pg=b.new_page(viewport={'width':w,'height':844},reduced_motion='reduce')
        pg.set_content(pages['/status'],wait_until='domcontentloaded'); pg.wait_for_timeout(200)
        pg.locator('#mnuBtn').click(); pg.wait_for_timeout(80)
        nav=pg.locator('.tnav.open a').first
        hit=pg.evaluate("""el=>{const r=el.getBoundingClientRect();const e=document.elementFromPoint(r.left+r.width/2,r.top+r.height/2);return !!e&&(e===el||el.contains(e))}""",nav.element_handle())
        assert hit, f'menu link blocked at {w}'
        pg.keyboard.press('Escape')
        print('PASS menu',w)
        pg.close()

    for w in (320,390,430):
        pg=b.new_page(viewport={'width':w,'height':844},reduced_motion='reduce')
        pg.set_content(pages['/users'],wait_until='domcontentloaded'); pg.wait_for_timeout(150)
        if pg.locator('tr.urow').count()==0:
            sample="""<tr class='urow'><td class='c-en'><button class='pw-btn on'></button></td><td class='c-user'><button class='ulink'>alice</button></td><td class='c-conn'>1/2</td><td class='c-ip'>10.0.0.4</td><td class='c-country'><span class='flg'>DZ</span></td><td class='c-caid'>0B00</td><td class='c-ok tg'>124</td><td class='c-nok dim'>2</td><td class='c-proto'><span class='badge'>NC</span></td><td class='c-idle'>4s</td><td class='c-last60 tg'>18</td><td class='c-last'>4s</td><td class='c-exp'>2027-09-01</td><td class='c-btn'><div class='ba'><button class='act-b ed'></button><button class='act-b rs'></button><button class='act-b dl'></button></div></td></tr>"""
            pg.locator('#usrBody').evaluate("(el,h)=>{el.innerHTML=h; const t=document.getElementById('usrTable'); if(t) t.hidden=false}", sample)
        row=pg.locator('tr.urow').first
        assert row.count()==1
        vals=pg.evaluate("""row=>Object.fromEntries(['c-user','c-ip','c-country','c-ok','c-exp','c-en','c-btn','c-conn','c-caid','c-nok','c-last60','c-last'].map(c=>{const e=row.querySelector('.'+c);return [c,e?getComputedStyle(e).display:'missing']}))""",row.element_handle())
        for c in ('c-user','c-ip','c-country','c-ok','c-exp','c-en','c-btn'): assert vals[c] not in ('none','missing'), (w,c,vals)
        for c in ('c-conn','c-caid','c-nok','c-last60','c-last'): assert vals[c]=='none', (w,c,vals)
        dl=pg.locator('tr.urow .act-b.dl').first
        ed=pg.locator('tr.urow .act-b.ed').first
        rs=pg.locator('tr.urow .act-b.rs').first
        assert dl.count() and ed.count() and rs.count()
        assert pg.evaluate("e=>getComputedStyle(e).display!=='none'",dl.element_handle())
        assert pg.evaluate("e=>getComputedStyle(e).display==='none'",ed.element_handle())
        assert pg.evaluate("e=>getComputedStyle(e).display==='none'",rs.element_handle())
        box=row.bounding_box(); assert box and box['width']<=w-16,(w,box)
        pg.screenshot(path=f'/tmp/users6_{w}.png',full_page=True)
        print('PASS users',w,vals)
        pg.close()
    b.close()
print('ALL PASS')
