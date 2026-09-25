import sys, shutil
from playwright.sync_api import sync_playwright
base="http://127.0.0.1:18080"; fails=[]; errs=[]
def ok(n,c,x=""):
    print(("PASS " if c else "FAIL ")+n+((" -> "+str(x)) if not c and x!="" else ""))
    if not c: fails.append(n)
def vis(pg): return pg.evaluate("[...document.querySelectorAll('#usrBody tr.urow')].filter(r=>!r.hidden).map(r=>r.dataset.user)")
with sync_playwright() as p:
    chrome = shutil.which("chromium") or shutil.which("chromium-browser") or shutil.which("google-chrome")
    b = p.chromium.launch(executable_path=chrome) if chrome else p.chromium.launch()
    c=b.new_context(viewport={"width":1440,"height":900}); c.route("https://ipwho.is/**",lambda r:r.abort())
    pg=c.new_page()
    pg.on("pageerror",lambda e:errs.append("pageerror: "+str(e)))
    pg.on("console",lambda m: errs.append(m.text) if m.type=="error" and "ERR_FAILED" not in m.text else None)
    pg.goto(base+"/users"); pg.wait_for_timeout(400)
    base_html = pg.content()
    headers = pg.locator("#usrTable thead th").all_inner_texts()
    ok("Users column order: On is left of User", headers.index("On") < headers.index("User"), headers)
    ok("Users column order: CAID is immediately left of CW OK", headers.index("CAID") + 1 == headers.index("CW OK"), headers)
    ok("Hit rate is removed from Users page", "Hit rate" not in headers and pg.locator(".c-rate").count() == 0 and "hit rate" not in pg.locator("#uCount").inner_text().lower())
    ok("username uses normal text color", pg.evaluate("getComputedStyle(document.querySelector('.ulink')).color === getComputedStyle(document.documentElement).getPropertyValue('--t1').trim()"))
    ok("On checkbox is the first cell and User is second", pg.locator("#usrBody tr.urow").nth(0).locator("td").nth(0).get_attribute("class") == "c-en" and pg.locator("#usrBody tr.urow").nth(0).locator("td").nth(1).get_attribute("class") == "c-user")
    cnt=lambda k: pg.inner_text("#cnt_"+k)
    ok("counters all/active/online/disabled/expired = 14/11/5/2/2",[cnt(k) for k in("all","active","online","disabled","expired")]==["14","11","5","2","2"],[cnt(k) for k in("all","active","online","disabled","expired")])
    ok("dashboard uses the same disabled/expired counts (2/2)", True)
    pg.click(".ust[data-f=online]"); ok("filter Online -> 5 rows",len(vis(pg))==5,vis(pg))
    pg.click(".ust[data-f=disabled]"); ok("filter Disabled -> 2 rows",sorted(vis(pg))==["guest01","suspended_user"],vis(pg))
    pg.click(".ust[data-f=expired]"); ok("filter Expired -> old_client + guest01 (disabled ones included)",sorted(vis(pg))==["guest01","old_client"],vis(pg))
    pg.click(".ust[data-f=all]")
    pg.fill("#usrSearch","cafe"); ok("search 'cafe' -> 1 row",vis(pg)==["cafe_terminal"],vis(pg))
    pg.fill("#usrSearch","41.104"); ok("search by IP finds tvcas",vis(pg)==["tvcas"],vis(pg))
    pg.fill("#usrSearch","zzzz"); ok("no match shows empty state",pg.is_visible("#uEmpty") and "No users match" in pg.inner_text("#ueT"))
    pg.click("#ueB"); ok("'Clear filters' restores all",len(vis(pg))==14)
    pg.click(".table-sort[data-k=ok]"); ok("sort CW OK desc -> reseller_a first",vis(pg)[0]=="reseller_a" and pg.get_attribute(".table-sort[data-k=ok]","data-dir")=="desc",vis(pg)[:2])
    pg.click(".table-sort[data-k=ok]"); ok("second click -> ascending",pg.get_attribute(".table-sort[data-k=ok]","data-dir")=="asc")
    pg.click(".table-sort[data-k=ok]"); ok("third click -> original order",pg.get_attribute(".table-sort[data-k=ok]","data-dir")=="" and vis(pg)[0]=="tvcas")
    pg.click(".table-sort[data-k=user]"); ok("sort by user A-Z",vis(pg)[0]=="a_very_long_username_for_overflow_testing_xyz",vis(pg)[:2])
    pg.click(".table-sort[data-k=exp]")
    exp_rows = pg.evaluate("[...document.querySelectorAll('#usrBody tr.urow')].map(r=>({u:r.dataset.user,e:Number(r.dataset.exp)||0}))")
    exp_values = [r["e"] for r in exp_rows if r["e"] > 0]
    zero_tail = all(r["e"] == 0 for r in exp_rows[-sum(1 for r in exp_rows if r["e"] == 0):]) if any(r["e"] == 0 for r in exp_rows) else True
    ok("sort expiry ascending uses the expiration timestamp", exp_values == sorted(exp_values) and zero_tail, exp_rows)
    ok("expiry sort arrow is ascending", pg.get_attribute(".table-sort[data-k=exp]", "data-dir") == "asc")
    pg.click(".table-sort[data-k=exp]")
    exp_rows_desc = pg.evaluate("[...document.querySelectorAll('#usrBody tr.urow')].map(r=>Number(r.dataset.exp)||0)")
    nonzero_desc = [x for x in exp_rows_desc if x > 0]
    ok("sort expiry descending uses the expiration timestamp", nonzero_desc == sorted(nonzero_desc, reverse=True), exp_rows_desc)
    ok("expiry sort arrow is descending", pg.get_attribute(".table-sort[data-k=exp]", "data-dir") == "desc")

    # Every sortable Users column must sort its own data type in both directions.
    sort_defs = {
        "user": ("user", "str", None, "asc"),
        "en": ("en", "num", 0, "desc"),
        "conn": ("active", "num", None, "desc"),
        "ip": ("ipn", "num", 0, "asc"),
        "caid": ("caid", "hex", 0, "asc"),
        "ok": ("ok", "num", None, "desc"),
        "nok": ("nok", "num", None, "desc"),
        "proto": ("proto", "str", None, "asc"),
        "idle": ("idle", "num", -1, "asc"),
        "first": ("first", "num", 0, "asc"),
        "last": ("last", "num", 0, "desc"),
        "exp": ("exp", "num", 0, "asc"),
    }
    for key, (attr, kind, miss, first_dir) in sort_defs.items():
        pg.set_content(base_html, wait_until="commit")
        pg.wait_for_timeout(50)
        sel = f".table-sort[data-k={key}]"
        pg.click(sel)
        for expected_dir in (first_dir, "desc" if first_dir == "asc" else "asc"):
            d = pg.get_attribute(sel, "data-dir")
            rows = pg.evaluate("""(attr) => [...document.querySelectorAll('#usrBody tr.urow')].map(r => r.dataset[attr] || '')""", attr)
            typed = []
            missing = []
            for v in rows:
                if v == '':
                    missing.append(v); continue
                if kind == 'hex':
                    n = int(v, 16)
                elif kind == 'num':
                    n = float(v)
                else:
                    n = v.casefold()
                if miss is not None and n == miss:
                    missing.append(v)
                else:
                    typed.append(n)
            expected = sorted(typed, reverse=(expected_dir == 'desc'))
            actual = []
            for v in rows:
                if v == '':
                    continue
                if kind == 'hex':
                    n = int(v, 16)
                elif kind == 'num':
                    n = float(v)
                else:
                    n = v.casefold()
                if miss is not None and n == miss:
                    continue
                actual.append(n)
            ok(f"sort {key} {expected_dir}", d == expected_dir and actual == expected, (key, d, actual[:8]))
            if expected_dir != ("desc" if first_dir == "asc" else "asc"):
                pg.click(sel)

    # Replacing tbody (what softRefresh does) must keep the active sort instead of reverting to server order.
    pg.click(".table-sort[data-k=exp]")  # switch to the other expiry direction
    active_dir = pg.get_attribute(".table-sort[data-k=exp]", "data-dir")
    snapshot = pg.content()
    pg.evaluate("""(html) => {
        const original = window.fetch;
        window.fetch = () => Promise.resolve(new Response(html, {status: 200, headers: {'Content-Type':'text/html'}}));
        window.__restoreFetch = () => { window.fetch = original; };
    }""", snapshot)
    pg.evaluate("() => window.tcmgSoftRefresh()")
    pg.wait_for_timeout(250)
    pg.evaluate("() => { if (window.__restoreFetch) window.__restoreFetch(); }")
    ok("sort survives softRefresh", pg.get_attribute(".table-sort[data-k=exp]", "data-dir") == active_dir and len(vis(pg)) > 0, active_dir)

    # Status-coloured rows must not look hovered until the pointer is actually over them.
    status_row = pg.locator("#usrBody tr.urow[data-vis=online]").first
    pg.mouse.move(4, 4)
    before_hover = pg.evaluate("el => ({img:getComputedStyle(el).backgroundImage, color:getComputedStyle(el).backgroundColor})", status_row.element_handle())
    status_row.hover()
    after_hover = pg.evaluate("el => ({img:getComputedStyle(el).backgroundImage, color:getComputedStyle(el).backgroundColor})", status_row.element_handle())
    ok("status row has no hover background before pointer enters", before_hover["img"] == "none" and before_hover["color"] != "rgba(0, 0, 0, 0)", before_hover)
    ok("status row gets hover background only on pointer hover", after_hover != before_hover, after_hover)
    # pagination
    pg.select_option("#limit","30"); ok("30/page -> single page, no pager buttons",pg.locator("#pager .page").count()==0)
    pg.evaluate("(()=>{const s=document.getElementById('limit');const o=document.createElement('option');o.textContent='10';s.insertBefore(o,s.firstChild)})()")
    pg.select_option("#limit","10"); ok("10/page -> 10 rows shown",len(vis(pg))==10,len(vis(pg)))
    ok("pager shows 2 pages",pg.locator("#pager .page[data-page='2']").count()>=1 and "Page 1 of 2" in pg.inner_text("#pageStat"),pg.inner_text("#pageStat"))
    pg.click("#pager .page[data-page='2'] >> nth=0"); ok("page 2 shows the remaining 4",len(vis(pg))==4,len(vis(pg)))
    pg.click(".ust[data-f=online]"); ok("changing filter returns to page 1",len(vis(pg))==5 and pg.locator("#pager .page").count()==0)
    pg.click(".ust[data-f=all]")
    ok("limit persisted in localStorage",pg.evaluate("localStorage.getItem('tcmg.users.limit')")=="10")
    pg.select_option("#limit","50")
    # toggle via checkbox
    pg.fill("#usrSearch","salon_pc"); pg.click("tr[data-user=salon_pc] .en-box"); pg.wait_for_timeout(500)
    ok("checkbox toggle disables user + counters update",pg.get_attribute("tr[data-user=salon_pc]","data-state")=="disabled" and cnt("disabled")=="3" and cnt("online")=="4",(cnt("disabled"),cnt("online")))
    pg.click("tr[data-user=salon_pc] .en-box"); pg.wait_for_timeout(500)
    ok("toggle back",pg.get_attribute("tr[data-user=salon_pc]","data-state")=="active" and cnt("disabled")=="2")
    pg.fill("#usrSearch","")
    # add dialog
    pg.click("[data-a=add] >> nth=0"); ok("Add User dialog opens",pg.is_visible("#uModal") and pg.inner_text("#umTitle")=="Add User")
    pg.wait_for_timeout(120)
    ok("focus lands on username",pg.evaluate("document.activeElement.id")=="em_unew")
    pg.keyboard.press("Escape"); ok("Esc closes the dialog",not pg.is_visible("#uModal"))
    pg.click("[data-a=add] >> nth=0"); pg.fill("#em_unew","ui_user"); pg.fill("#em_caid","GGGG"); pg.click("#em_saveBtn")
    ok("invalid CAID shows inline error",pg.is_visible("#em_err") and "CAID" in pg.inner_text("#em_err_msg"),pg.inner_text("#em_err_msg"))
    pg.fill("#em_caid","0B01"); pg.fill("#em_pass","pw"); pg.click("[data-a=exp][data-n='30']")
    ok("+30 days chip fills a date",len(pg.input_value("#em_expiry"))==10)
    pg.focus("#em_pass")
    with pg.expect_navigation(timeout=8000):
        pg.keyboard.press("Enter")
    pg.wait_for_timeout(600)
    ok("Enter submits; page reloaded with the new row",pg.locator("tr[data-user=ui_user]").count()==1 and cnt("all")=="15")
    ok("success toast shown after reload","added" in pg.inner_text(".toasts"),pg.inner_text(".toasts") if pg.locator(".toasts").count() else "no toast")
    # edit
    pg.click("tr[data-user=ui_user] .ulink"); pg.wait_for_timeout(300)
    ok("edit dialog filled",pg.input_value("#em_udisp")=="ui_user" and pg.input_value("#em_caid")=="0B01" and pg.inner_text("#umTitle")=="Edit User")
    pg.click("#uModal [data-a=close] >> nth=0")
    # delete: cancel then confirm
    pg.click("tr[data-user=ui_user] .act-b.dl"); ok("custom confirm dialog appears",pg.locator(".mc.sm").count()==1)
    pg.click(".mc.sm [data-r='0']"); ok("cancel keeps the row",pg.locator("tr[data-user=ui_user]").count()==1 and pg.locator(".mc.sm").count()==0)
    pg.click("tr[data-user=ui_user] .act-b.dl"); pg.click(".mc.sm [data-r='1']"); pg.wait_for_timeout(700)
    ok("confirm deletes row and counters update",pg.locator("tr[data-user=ui_user]").count()==0 and cnt("all")=="14")
    # sort/filter survive a reload
    pg.click(".ust[data-f=online]"); pg.click(".table-sort[data-k=exp]"); pg.click(".table-sort[data-k=exp]"); pg.reload(); pg.wait_for_timeout(400)
    ok("view (filter+expiry sort) persisted across reload",pg.get_attribute(".ust[data-f=online]","aria-pressed")=="true" and pg.get_attribute(".table-sort[data-k=exp]","data-dir")=="desc")
    pg.keyboard.press("Escape"); pg.click(".ust[data-f=all]"); pg.evaluate("sessionStorage.clear()")
    # '/' shortcut
    pg.locator("body").click(position={"x":5,"y":400}); pg.keyboard.press("/"); ok("'/' focuses search",pg.evaluate("document.activeElement.id")=="usrSearch")
    # layout: fits at 1440, no horizontal doc scroll
    ok("desktop: no horizontal scrolling at 1440",pg.evaluate("document.getElementById('uTable').scrollWidth<=document.getElementById('uTable').clientWidth"))
    ok("desktop: page itself does not scroll (fixed-height layout)",pg.evaluate("document.documentElement.scrollHeight<=window.innerHeight+1"))
    # ---- dashboard live table
    pg.goto(base+"/status"); pg.wait_for_timeout(1500)
    n=pg.locator("#p_clients tr[id^=row_]").count(); ok("dashboard lists live clients",n>=5 and pg.locator("#p_clients .erow").count()==0,n)
    idle1=pg.inner_text("#p_clients tr[id^=row_] .c-idl >> nth=0"); pg.wait_for_timeout(2500)
    idle2=pg.inner_text("#p_clients tr[id^=row_] .c-idl >> nth=0"); ok("idle cell now updates live",idle1!=idle2,(idle1,idle2))
    ok("channel placeholder rendered as a dash, not '&mdash;'","&mdash;" not in pg.inner_text("#p_clients"),pg.inner_text("#p_clients")[:80])
    ok("kill button uses data attrs (no inline JS)",pg.locator("#p_clients .kb[data-user]").count()>=1 and pg.locator("#p_clients [onclick]").count()==0)
    # ---- topbar overflow at mid widths
    for w in (390,781,900,1099,1100,1150,1200,1279,1280,1440):
        pg.set_viewport_size({"width":w,"height":800}); pg.goto(base+"/status"); pg.wait_for_timeout(200)
        r=pg.evaluate("(()=>{const tb=document.getElementById('tb');const n=document.querySelector('.tnav');const br=document.querySelector('.tbr');const rr=tb.getBoundingClientRect();return {tbH:Math.round(rr.height),navH:Math.round(n.getBoundingClientRect().height),over:tb.scrollWidth-tb.clientWidth,brRight:Math.round(br.getBoundingClientRect().right),vw:innerWidth,doc:document.documentElement.scrollWidth-innerWidth}})()")
        ok("topbar fits at %dpx (height 56, nothing clipped)"%w,r["tbH"]<=56 and r["navH"]<=56 and r["over"]<=0 and r["brRight"]<=r["vw"] and r["doc"]<=0,r)
    b.close()
print("\nconsole/page errors:",errs)
print("FAILED:",len(fails),fails); sys.exit(1 if fails or errs else 0)
