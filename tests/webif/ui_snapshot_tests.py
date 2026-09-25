import os, shutil, requests
from playwright.sync_api import sync_playwright

BASE = os.environ.get("TCMG_WEBIF_URL", "http://127.0.0.1:18080")
html = requests.get(BASE + "/users", timeout=10).text
fails = []
def ok(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + ((" -> " + str(detail)) if not cond else ""))
    if not cond: fails.append(name)

with sync_playwright() as p:
    exe = shutil.which("chromium") or shutil.which("chromium-browser") or shutil.which("google-chrome")
    b = p.chromium.launch(executable_path=exe) if exe else p.chromium.launch()
    page = b.new_page(viewport={"width": 1440, "height": 900})
    page.set_default_timeout(3000)
    page.set_content(html, wait_until="commit")
    page.mouse.move(2, 2)
    headers = page.locator("#usrTable thead th").all_inner_texts()
    ok("On before User", headers.index("On") < headers.index("User"), headers)
    ok("CAID immediately before CW OK", headers.index("CAID") + 1 == headers.index("CW OK"), headers)
    ok("Hit rate absent", "Hit rate" not in headers and "hit rate" not in page.locator("#uCount").inner_text().lower())

    mapping = {
        "user": ("user", "str", None, "asc"), "en": ("en", "num", 0, "desc"),
        "conn": ("active", "num", None, "desc"), "ip": ("ipn", "num", 0, "asc"),
        "caid": ("caid", "hex", 0, "asc"), "ok": ("ok", "num", None, "desc"),
        "nok": ("nok", "num", None, "desc"), "proto": ("proto", "str", None, "asc"),
        "idle": ("idle", "num", -1, "asc"), "first": ("first", "num", 0, "asc"),
        "last": ("last", "num", 0, "desc"), "exp": ("exp", "num", 0, "asc"),
    }
    base_html = html
    for key, (attr, kind, miss, d0) in mapping.items():
        page.set_content(base_html, wait_until="commit")
        page.mouse.move(2, 2)
        sel = f".table-sort[data-k={key}]"
        page.click(sel)
        dirs = [page.get_attribute(sel, "data-dir")]
        page.click(sel)
        dirs.append(page.get_attribute(sel, "data-dir"))
        rows = page.evaluate("attr => [...document.querySelectorAll('#usrBody tr.urow')].map(r => r.dataset[attr] || '')", attr)
        vals = []
        for v in rows:
            if not v: continue
            x = v.casefold() if kind == "str" else int(v, 16) if kind == "hex" else float(v)
            if miss is not None and x == miss: continue
            vals.append(x)
        second = "desc" if d0 == "asc" else "asc"
        ok(f"sort {key} both directions", dirs == [d0, second] and vals == sorted(vals, reverse=second == "desc"), (dirs, vals[:6]))

    # Use all status states and ensure their highlight appears only after hover.
    for state in ("stale", "expired", "disabled"):
        page.set_content(base_html, wait_until="commit")
        page.mouse.move(2, 2)
        row = page.locator("#usrBody tr.urow").first
        row.evaluate("(el,s) => el.dataset.vis=s", state)
        before = row.evaluate("el => [getComputedStyle(el).backgroundImage, getComputedStyle(el).backgroundColor]")
        before_hovered = row.evaluate("el => el.matches(':hover')")
        row.hover()
        after = row.evaluate("el => [getComputedStyle(el).backgroundImage, getComputedStyle(el).backgroundColor]")
        ok(f"{state} row not hovered until pointer", not before_hovered, before)
        page.mouse.move(2, 2)
        ok(f"{state} hover changes visual only on pointer", after != before, (before, after))

    b.close()

print("FAILED:", len(fails), fails)
raise SystemExit(1 if fails else 0)
