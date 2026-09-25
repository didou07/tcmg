"""Dashboard with REFRESH=0 and Live Log page (run with CONF=<dir whose tcmg.conf has REFRESH = 0>)."""
import json, sys, urllib.request
from playwright.sync_api import sync_playwright

base = "http://127.0.0.1:18080"
fails, errs = [], []


def ok(name, cond, extra=""):
    print(("PASS " if cond else "FAIL ") + name + ((" -> " + str(extra)) if not cond and extra != "" else ""))
    if not cond:
        fails.append(name)


with sync_playwright() as p:
    b = p.chromium.launch()
    c = b.new_context(viewport={"width": 1440, "height": 900}, accept_downloads=True)
    c.route("https://ipwho.is/**", lambda r: r.abort())
    pg = c.new_page()
    pg.on("pageerror", lambda e: errs.append(str(e)))
    pg.on("console", lambda m: errs.append(m.text) if m.type == "error" and "ERR_FAILED" not in m.text else None)

    pg.goto(base + "/status")
    pg.wait_for_timeout(1500)
    n = pg.locator("#p_clients tr[id^=row_]").count()
    ok("refresh=0: dashboard still fills its connections table (was empty forever)", n >= 5, n)

    pg.goto(base + "/livelog")
    pg.wait_for_timeout(1800)
    for _ in range(6):
        pg.click("#dbALL")
        pg.wait_for_timeout(120)
    pg.wait_for_timeout(2500)
    ui = pg.evaluate("[...document.querySelectorAll('#lp span')].map(s => s.getAttribute('data-r') || '')")
    srv = [l["line"] for l in json.loads(urllib.request.urlopen(base + "/logpoll?since=0").read())["lines"]]
    ok("log view is line-for-line identical to the server ring buffer (%d = %d)" % (len(ui), len(srv)),
       ui == srv, (len(ui), len(srv)))

    with pg.expect_download(timeout=5000) as d:
        pg.click("button:has-text('Save')")
    ok("Save button downloads tcmg.log", d.value.suggested_filename == "tcmg.log", d.value.suggested_filename)
    ok("no <a><button> nesting left", pg.locator("a button").count() == 0)
    b.close()

print("console/page errors:", errs)
print("FAILED:", len(fails), fails)
sys.exit(1 if fails or errs else 0)
