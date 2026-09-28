import os, shutil, requests, re
from playwright.sync_api import sync_playwright

BASE = os.environ.get("TCMG_WEBIF_URL", "http://127.0.0.1:18080")
fails = []

def ok(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + ((" -> " + str(detail)) if not cond else ""))
    if not cond: fails.append(name)

def inline_page(path):
    html = requests.get(BASE + path, timeout=10).text
    css = requests.get(BASE + "/assets/app.css", timeout=10).text
    assets = {name: requests.get(BASE + "/assets/" + name, timeout=10).text for name in ("app.js", "users.js", "readers.js")}
    html = re.sub(r"<link[^>]+href=['\"]/assets/app\.css[^>]*>", lambda m: "<style>" + css + "</style>", html)
    for name, body in assets.items():
        html = re.sub(r"<script[^>]+src=['\"]/assets/" + re.escape(name) + r"(?:\?[^'\"]*)?['\"][^>]*></script>", lambda m: "<script>" + body + "</script>", html)
    return html

html = inline_page("/users")
with sync_playwright() as p:
    exe = shutil.which("chromium") or shutil.which("chromium-browser") or shutil.which("google-chrome")
    b = p.chromium.launch(executable_path=exe) if exe else p.chromium.launch()
    page = b.new_page(viewport={"width": 1440, "height": 900})
    page.set_default_timeout(3000)
    page.set_content(html, wait_until="commit")
    page.wait_for_timeout(120)
    page.mouse.move(2, 2)
    headers = page.locator("#usrTable thead th").all_inner_texts()
    ok("On before User", headers.index("On") < headers.index("User"), headers)
    ok("CAID immediately before CW OK", headers.index("CAID") + 1 == headers.index("CW OK"), headers)
    ok("Hit rate absent", "Hit rate" not in headers and "uCount" not in page.content())

    mapping = {
        "user": ("user", "str", None, "asc"), "en": ("en", "num", 0, "desc"),
        "conn": ("active", "num", None, "desc"), "ip": ("ipn", "num", 0, "asc"),
        "caid": ("caid", "hex", 0, "asc"), "ok": ("ok", "num", None, "desc"),
        "nok": ("nok", "num", None, "desc"), "proto": ("proto", "str", None, "asc"),
        "idle": ("idle", "num", -1, "asc"), "last60": ("last60", "num", 0, "desc"),
        "last": ("last", "num", 0, "desc"), "exp": ("exp", "num", 0, "asc"),
    }
    for key, (attr, kind, miss, d0) in mapping.items():
        page.set_content(html, wait_until="commit")
        page.wait_for_timeout(80)
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

    for state in ("stale", "expired", "disabled"):
        page.set_content(html, wait_until="commit")
        page.wait_for_timeout(80)
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


# FailBan UI: compact 4-cell card and no Total Fails/per-ban fails/expiry text.
system_c = (ROOT / "webif/pages/system.c").read_text()
assert "Total Fails" not in system_c
assert "b->fails" not in system_c
assert "Expires At" not in system_c
assert "fbc-country" in system_c and "fbc-left" in system_c

# ECM result log classification: whitelist rejection is distinct from not-found.
log_js = (ROOT / "webif/assets/js_livelog.h").read_text()
assert "LC_REJECT" in log_js and "#f0973f" in log_js
log_c = (ROOT / "src/log/log.c").read_text()
assert "LC_ECM_REJECTED" in log_c and "ANSI_ORANGE" in log_c
