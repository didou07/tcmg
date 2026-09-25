#ifndef TCMG_WEBIF_JS_USERS_H_
#define TCMG_WEBIF_JS_USERS_H_

#define TCMG_USERS_JS \
	"\n" \
	"(function () {\n" \
	"  'use strict';\n" \
	"\n" \
	"  var body = document.getElementById('usrBody');\n" \
	"  if (!body) return;\n" \
	"\n" \
	"  function $(id) { return document.getElementById(id); }\n" \
	"  function rowsAll() { return Array.prototype.slice.call(body.querySelectorAll('tr.urow')); }\n" \
	"  function fmtN(n) { return String(Math.round(n)).replace(/\\B(?=(\\d{3})+(?!\\d))/g, ','); }\n" \
	"  function pad2(n) { return (n < 10 ? '0' : '') + n; }\n" \
	"  function ymd(d) { return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate()); }\n" \
	"\n" \
	"  \n" \
	"\n" \
	"  function api(url, opt) {\n" \
	"    return fetch(url, opt).then(function (r) {\n" \
	"      if (r.status === 401) { location.href = '/login'; throw new Error('unauthorized'); }\n" \
	"      return r.json();\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function netErr(e) {\n" \
	"    if (e && e.message === 'unauthorized') return;\n" \
	"    toast('Request failed \\u2014 check the connection', 'err');\n" \
	"  }\n" \
	"\n" \
	"  var toastBox = null;\n" \
	"  function toast(msg, kind) {\n" \
	"    if (!toastBox) {\n" \
	"      toastBox = document.createElement('div');\n" \
	"      toastBox.className = 'toasts';\n" \
	"      toastBox.setAttribute('role', 'status');\n" \
	"      toastBox.setAttribute('aria-live', 'polite');\n" \
	"      document.body.appendChild(toastBox);\n" \
	"    }\n" \
	"    var t = document.createElement('div');\n" \
	"    t.className = 'toast ' + (kind || '');\n" \
	"    t.textContent = msg;\n" \
	"    toastBox.appendChild(t);\n" \
	"    setTimeout(function () {\n" \
	"      t.style.transition = 'opacity .25s';\n" \
	"      t.style.opacity = '0';\n" \
	"      setTimeout(function () { if (t.parentNode) t.parentNode.removeChild(t); }, 260);\n" \
	"    }, kind === 'err' ? 5000 : 2600);\n" \
	"  }\n" \
	"\n" \
	"  function flash(msg) { try { sessionStorage.setItem('tcmg_u_flash', msg); } catch (e) {} }\n" \
	"\n" \
	"  \n" \
	"  var stack = [];\n" \
	"  function lock() { document.body.classList.toggle('mo-open', stack.length > 0); }\n" \
	"\n" \
	"  function focusables(root) {\n" \
	"    return Array.prototype.slice.call(root.querySelectorAll(\n" \
	"      'button:not([disabled]),input:not([disabled]):not([type=hidden]),select,textarea,[tabindex]:not([tabindex=\"-1\"])'\n" \
	"    )).filter(function (el) { return !el.closest('[hidden]') && el.offsetParent !== null; });\n" \
	"  }\n" \
	"\n" \
	"  function pushModal(el, close, opener) {\n" \
	"    stack.push({ el: el, close: close, opener: opener || null });\n" \
	"    lock();\n" \
	"  }\n" \
	"  function popModal(el) {\n" \
	"    for (var i = stack.length - 1; i >= 0; i--) {\n" \
	"      if (stack[i].el === el) {\n" \
	"        var op = stack[i].opener;\n" \
	"        stack.splice(i, 1);\n" \
	"        lock();\n" \
	"        if (op && document.body.contains(op)) op.focus();\n" \
	"        return;\n" \
	"      }\n" \
	"    }\n" \
	"  }\n" \
	"\n" \
	"  document.addEventListener('keydown', function (e) {\n" \
	"    var top = stack[stack.length - 1];\n" \
	"    if (top) {\n" \
	"      if (e.key === 'Escape') { e.preventDefault(); top.close(); return; }\n" \
	"      if (e.key === 'Tab') {\n" \
	"        var f = focusables(top.el);\n" \
	"        if (!f.length) return;\n" \
	"        var first = f[0], last = f[f.length - 1];\n" \
	"        if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }\n" \
	"        else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }\n" \
	"      }\n" \
	"      return;\n" \
	"    }\n" \
	"    if (e.key === '/' && !/^(INPUT|TEXTAREA|SELECT)$/.test((document.activeElement || {}).tagName || '')) {\n" \
	"      e.preventDefault();\n" \
	"      $('usrSearch').focus();\n" \
	"    }\n" \
	"  });\n" \
	"\n" \
	"  \n" \
	"  function ask(o) {\n" \
	"    return new Promise(function (resolve) {\n" \
	"      var ov = document.createElement('div');\n" \
	"      ov.className = 'mo';\n" \
	"      var dlg = document.createElement('div');\n" \
	"      dlg.className = 'mc sm';\n" \
	"      dlg.setAttribute('role', 'alertdialog');\n" \
	"      dlg.setAttribute('aria-modal', 'true');\n" \
	"      var tone = o.danger ? 'danger' : 'warn';\n" \
	"      dlg.innerHTML =\n" \
	"        '<div class=\"cf\"><div class=\"cfi ' + tone + '\"><svg class=\"i\" viewBox=\"0 0 24 24\"><use href=\"#' +\n" \
	"        (o.danger ? 'i-trash' : 'i-alert') + '\"/></svg></div><div><div class=\"cft\"></div><div class=\"cfm\"></div></div></div>' +\n" \
	"        '<div class=\"mf\"><button type=\"button\" class=\"btn bg\" data-r=\"0\">Cancel</button>' +\n" \
	"        '<button type=\"button\" class=\"btn ' + (o.danger ? 'bdz' : 'bp') + '\" data-r=\"1\"></button></div>';\n" \
	"      dlg.querySelector('.cft').textContent = o.title;\n" \
	"      dlg.querySelector('.cfm').textContent = o.msg;\n" \
	"      dlg.querySelector('[data-r=\"1\"]').textContent = o.ok;\n" \
	"      ov.appendChild(dlg);\n" \
	"      document.body.appendChild(ov);\n" \
	"\n" \
	"      var opener = document.activeElement;\n" \
	"      function done(v) {\n" \
	"        popModal(ov);\n" \
	"        if (ov.parentNode) ov.parentNode.removeChild(ov);\n" \
	"        resolve(v);\n" \
	"      }\n" \
	"      ov.addEventListener('mousedown', function (e) { if (e.target === ov) done(false); });\n" \
	"      dlg.addEventListener('click', function (e) {\n" \
	"        var b = e.target.closest('[data-r]');\n" \
	"        if (b) done(b.getAttribute('data-r') === '1');\n" \
	"      });\n" \
	"      pushModal(ov, function () { done(false); }, opener);\n" \
	"      dlg.querySelector('[data-r=\"0\"]').focus();\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  \n" \
	"\n" \
	"  var view = { q: '', f: 'all', k: '', d: 1, p: 1 };\n" \
	"  var limit = 50;\n" \
	"  try { var lv = parseInt(localStorage.getItem('tcmg.users.limit'), 10); if (lv >= 10 && lv <= 500) limit = lv; } catch (e) {}\n" \
	"  var VKEY = 'tcmg_u_view';\n" \
	"\n" \
	"  function saveView() { try { sessionStorage.setItem(VKEY, JSON.stringify(view)); } catch (e) {} }\n" \
	"  function loadView() {\n" \
	"    try {\n" \
	"      var v = JSON.parse(sessionStorage.getItem(VKEY) || 'null');\n" \
	"      if (v && typeof v === 'object') {\n" \
	"        view.q = String(v.q || '');\n" \
	"        view.f = /^(all|active|online|disabled|expired)$/.test(v.f) ? v.f : 'all';\n" \
	"        view.k = KEYS[v.k] ? v.k : '';\n" \
	"        view.d = v.d === -1 ? -1 : 1;\n" \
	"      }\n" \
	"    } catch (e) {}\n" \
	"  }\n" \
	"\n" \
	"  \n" \
	"\n" \
	"  \n" \
	"  var KEYS = {\n" \
	"    user:  { a: 'user', str: true, d0: 1 },\n" \
	"    en:    { a: 'en', d0: -1 },\n" \
	"    caid:  { a: 'caid', miss: 0, hex: true, d0: 1 },\n" \
	"    conn:  { a: 'active', d0: -1 },\n" \
	"    ok:    { a: 'ok', d0: -1 },\n" \
	"    nok:   { a: 'nok', d0: -1 },\n" \
	"    avg:   { a: 'avg', miss: -1, d0: 1 },\n" \
	"    proto: { a: 'proto', str: true, d0: 1 },\n" \
	"    ip:    { a: 'ipn', miss: 0, d0: 1 },\n" \
	"    idle:  { a: 'idle', miss: -1, d0: 1 },\n" \
	"    first: { a: 'first', miss: 0, d0: 1 },\n" \
	"    last:  { a: 'last', miss: 0, d0: -1 },\n" \
	"    exp:   { a: 'exp', miss: 0, d0: 1, date: true }\n" \
	"  };\n" \
	"\n" \
	"  function matches(tr) {\n" \
	"    var s = tr.dataset;\n" \
	"    if (view.f === 'online') { if (s.online !== '1') return false; }\n" \
	"    else if (view.f === 'disabled') { if (s.en !== '0') return false; }\n" \
	"    else if (view.f === 'expired') { if (s.expd !== '1') return false; }\n" \
	"    else if (view.f === 'active') { if (s.en !== '1' || s.expd === '1') return false; }\n" \
	"    if (view.q && s.q.toLowerCase().indexOf(view.q) < 0) return false;\n" \
	"    return true;\n" \
	"  }\n" \
	"\n" \
	"  function pageBtn(label, page, disabled, cur, title) {\n" \
	"    var b = document.createElement('button');\n" \
	"    b.type = 'button';\n" \
	"    b.className = 'page' + (cur ? ' current' : '');\n" \
	"    b.textContent = label;\n" \
	"    b.title = title || '';\n" \
	"    b.disabled = !!disabled;\n" \
	"    b.setAttribute('data-page', page);\n" \
	"    if (cur) b.setAttribute('aria-current', 'page');\n" \
	"    return b;\n" \
	"  }\n" \
	"\n" \
	"  function renderPager(pages) {\n" \
	"    var box = $('pager');\n" \
	"    box.textContent = '';\n" \
	"    if (pages > 1) {\n" \
	"      var cur = view.p, nums = [], i;\n" \
	"      var start = Math.max(1, cur - 4), end = Math.min(pages, start + 8);\n" \
	"      start = Math.max(1, end - 8);\n" \
	"      box.appendChild(pageBtn('\\u00AB', 1, cur === 1, false, 'First page'));\n" \
	"      box.appendChild(pageBtn('\\u2039', Math.max(1, cur - 1), cur === 1, false, 'Previous page'));\n" \
	"      for (i = start; i <= end; i++) nums.push(i);\n" \
	"      nums.forEach(function (n) { box.appendChild(pageBtn(String(n), n, false, n === cur, 'Page ' + n)); });\n" \
	"      box.appendChild(pageBtn('\\u203A', Math.min(pages, cur + 1), cur === pages, false, 'Next page'));\n" \
	"      box.appendChild(pageBtn('\\u00BB', pages, cur === pages, false, 'Last page'));\n" \
	"    }\n" \
	"    $('pageStat').textContent = pages > 1 ? 'Page ' + view.p + ' of ' + pages : '';\n" \
	"  }\n" \
	"\n" \
	"  \n" \
	"  function applyFilter() {\n" \
	"    var rows = rowsAll(), matched = [];\n" \
	"    rows.forEach(function (r) { if (matches(r)) matched.push(r); });\n" \
	"    var pages = Math.max(1, Math.ceil(matched.length / limit));\n" \
	"    if (view.p > pages) view.p = pages;\n" \
	"    if (view.p < 1) view.p = 1;\n" \
	"    var from = (view.p - 1) * limit, to = from + limit;\n" \
	"    rows.forEach(function (r) { r.hidden = true; });\n" \
	"    matched.forEach(function (r, i) { r.hidden = !(i >= from && i < to); });\n" \
	"\n" \
	"    var total = rows.length;\n" \
	"    var tbl = $('usrTable'), empty = $('uEmpty');\n" \
	"    tbl.hidden = (matched.length === 0);\n" \
	"    empty.hidden = (matched.length !== 0);\n" \
	"    if (matched.length === 0) {\n" \
	"      var none = (total === 0);\n" \
	"      $('ueT').textContent = none ? 'No users yet' : 'No users match';\n" \
	"      $('ueS').textContent = none ? 'Create the first account to let a client connect.'\n" \
	"                                  : 'Try another search term or clear the filter.';\n" \
	"      var b = $('ueB');\n" \
	"      b.setAttribute('data-a', none ? 'add' : 'clear');\n" \
	"      b.textContent = none ? 'Add User' : 'Clear filters';\n" \
	"    }\n" \
	"    renderPager(pages);\n" \
	"    $('limit').value = String(limit);\n" \
	"    document.querySelectorAll('.ust').forEach(function (t) {\n" \
	"      var on = t.getAttribute('data-f') === view.f;\n" \
	"      t.classList.toggle('act', on);\n" \
	"      t.setAttribute('aria-pressed', on ? 'true' : 'false');\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function recount() {\n" \
	"    var rows = rowsAll();\n" \
	"    var c = { all: rows.length, active: 0, online: 0, disabled: 0, expired: 0 };\n" \
	"    var ok = 0, nok = 0;\n" \
	"    rows.forEach(function (r) {\n" \
	"      var d = r.dataset;\n" \
	"      if (d.en === '0') c.disabled++;\n" \
	"      if (d.expd === '1') c.expired++;\n" \
	"      if (d.en === '1' && d.expd !== '1') c.active++;\n" \
	"      if (d.online === '1') c.online++;\n" \
	"      ok += +d.ok;\n" \
	"      nok += +d.nok;\n" \
	"    });\n" \
	"    Object.keys(c).forEach(function (k) {\n" \
	"      var e = $('cnt_' + k);\n" \
	"      if (e) e.textContent = c[k];\n" \
	"    });\n" \
	"    $('uCount').innerHTML =\n" \
	"      '<span><b>' + rows.length + '</b>' + (rows.length === 1 ? 'user' : 'users') + '</span>' +\n" \
	"      '<span><b class=\"tg\">' + fmtN(ok) + '</b>CW OK</span>' +\n" \
	"      '<span><b class=\"' + (nok > 0 ? 'tr' : 'dim') + '\">' + fmtN(nok) + '</b>CW NOK</span>';\n" \
	"  }\n" \
	"\n" \
	"  function sortRows() {\n" \
	"    var def = KEYS[view.k];\n" \
	"    var rows = rowsAll();\n" \
	"    rows.sort(function (a, b) {\n" \
	"      var ai = +a.dataset.i, bi = +b.dataset.i;\n" \
	"      if (!def) return ai - bi;\n" \
	"      var av = a.dataset[def.a] || '', bv = b.dataset[def.a] || '';\n" \
	"      if (def.str) {\n" \
	"        if (!av && bv) return 1;\n" \
	"        if (av && !bv) return -1;\n" \
	"        if (!av && !bv) return ai - bi;\n" \
	"        var al = av.toLowerCase(), bl = bv.toLowerCase();\n" \
	"        var sr = al < bl ? -1 : (al > bl ? 1 : 0);\n" \
	"        return sr ? sr * view.d : ai - bi;\n" \
	"      }\n" \
	"      var x = def.hex ? parseInt(av, 16) : parseFloat(av), y = def.hex ? parseInt(bv, 16) : parseFloat(bv);\n" \
	"      var xm = !isFinite(x) || (def.miss !== undefined && x === def.miss);\n" \
	"      var ym = !isFinite(y) || (def.miss !== undefined && y === def.miss);\n" \
	"      if (xm !== ym) return xm ? 1 : -1;\n" \
	"      if (xm && ym) return ai - bi;\n" \
	"      var nr = (x - y) * view.d;\n" \
	"      return nr ? nr : ai - bi;\n" \
	"    });\n" \
	"    rows.forEach(function (r) { body.appendChild(r); });\n" \
	"\n" \
	"    document.querySelectorAll('#usrTable .table-sort').forEach(function (b) {\n" \
	"      var on = b.getAttribute('data-k') === view.k;\n" \
	"      b.setAttribute('data-dir', on ? (view.d === 1 ? 'asc' : 'desc') : '');\n" \
	"      b.parentNode.setAttribute('aria-sort', on ? (view.d === 1 ? 'ascending' : 'descending') : 'none');\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function refreshAll() { sortRows(); recount(); applyFilter(); }\n" \
	"\n" \
	"  var rowMap = Object.create(null);\n" \
	"  function rebuildRowMap() {\n" \
	"    rowMap = Object.create(null);\n" \
	"    rowsAll().forEach(function (r) { rowMap[r.dataset.user] = r; });\n" \
	"  }\n" \
	"\n" \
	"  function fmtLiveAgo(ts) {\n" \
	"    ts = +ts || 0;\n" \
	"    if (!ts) return 'never';\n" \
	"    var d = Math.max(0, Math.floor(Date.now() / 1000) - ts);\n" \
	"    if (d < 10) return 'just now';\n" \
	"    if (d < 60) return d + 's ago';\n" \
	"    if (d < 3600) return Math.floor(d / 60) + 'm ago';\n" \
	"    if (d < 86400) return Math.floor(d / 3600) + 'h ago';\n" \
	"    if (d < 86400 * 45) return Math.floor(d / 86400) + 'd ago';\n" \
	"    if (d < 86400 * 365) return Math.floor(d / (86400 * 30)) + 'mo ago';\n" \
	"    return Math.floor(d / (86400 * 365)) + 'y ago';\n" \
	"  }\n" \
	"\n" \
	"  function fmtLiveIdle(s) {\n" \
	"    s = +s;\n" \
	"    if (!isFinite(s) || s < 0) return '\\u2014';\n" \
	"    if (s < 60) return Math.floor(s) + 's';\n" \
	"    if (s < 3600) return Math.floor(s / 60) + 'm ' + String(Math.floor(s % 60)).padStart(2, '0') + 's';\n" \
	"    if (s < 86400) return Math.floor(s / 3600) + 'h ' + String(Math.floor((s % 3600) / 60)).padStart(2, '0') + 'm';\n" \
	"    return Math.floor(s / 86400) + 'd ' + Math.floor((s % 86400) / 3600) + 'h';\n" \
	"  }\n" \
	"\n" \
	"  function ipNum(ip) {\n" \
	"    var m = String(ip || '').match(/^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$/);\n" \
	"    if (!m) return 0;\n" \
	"    return ((+m[1] & 255) * 16777216) + ((+m[2] & 255) * 65536) + ((+m[3] & 255) * 256) + (+m[4] & 255);\n" \
	"  }\n" \
	"\n" \
	"  function setCellText(row, cls, value) {\n" \
	"    var e = row.querySelector('.' + cls);\n" \
	"    if (e) e.textContent = value;\n" \
	"    return e;\n" \
	"  }\n" \
	"\n" \
	"  function applyLiveUser(u) {\n" \
	"    var row = rowMap[u.user];\n" \
	"    if (!row) return false;\n" \
	"\n" \
	"    var active = +u.active || 0;\n" \
	"    var ok = +u.ok || 0, nok = +u.nok || 0, avg = +u.avg;\n" \
	"    var ip = String(u.ip || ''), proto = String(u.proto || ''), caid = String(u.caid || '');\n" \
	"    var online = active > 0 ? 1 : 0;\n" \
	"    var expd = row.dataset.expd === '1';\n" \
	"    var en = row.dataset.en === '1';\n" \
	"    var stale = online && (+u.idle >= 30);\n" \
	"\n" \
	"    row.dataset.online = String(online);\n" \
	"    row.dataset.active = String(active);\n" \
	"    row.dataset.caid = caid || '0';\n" \
	"    row.dataset.ok = String(ok);\n" \
	"    row.dataset.nok = String(nok);\n" \
	"    row.dataset.avg = isFinite(avg) ? String(avg) : '-1';\n" \
	"    row.dataset.proto = proto;\n" \
	"    row.dataset.ipn = String(ipNum(ip));\n" \
	"    row.dataset.idle = String((u.idle === null || u.idle === undefined) ? -1 : +u.idle);\n" \
	"    row.dataset.last = String(+u.last_seen || 0);\n" \
	"    row.dataset.vis = expd ? 'expired' : !en ? 'disabled' : stale ? 'stale' : online ? 'online' : '';\n" \
	"    row.dataset.q = row.dataset.user + ' ' + caid + ' ' + (row.dataset.allowed || '') + ' ' + ip + ' ' + proto;\n" \
	"\n" \
	"    var c = row.querySelector('.c-conn');\n" \
	"    if (c) {\n" \
	"      var cn = c.querySelector('.cn');\n" \
	"      var max = c.querySelector('.dim');\n" \
	"      var maxn = max ? max.textContent : '\\u221e';\n" \
	"      if (cn) {\n" \
	"        cn.textContent = String(active);\n" \
	"        cn.classList.toggle('on', active > 0);\n" \
	"        var maxVal = parseInt(maxn, 10);\n" \
	"        cn.classList.toggle('full', isFinite(maxVal) && maxVal > 0 && active >= maxVal);\n" \
	"      }\n" \
	"    }\n" \
	"\n" \
	"    var ipEl = row.querySelector('.c-ip');\n" \
	"    var flagEl = row.querySelector('.uipflag');\n" \
	"    if (ipEl) {\n" \
	"      ipEl.textContent = ip || '\\u2014';\n" \
	"      ipEl.classList.toggle('dim', !ip);\n" \
	"    }\n" \
	"    if (flagEl && ip) {\n" \
	"      var oldIp = flagEl.getAttribute('data-ip') || '';\n" \
	"      flagEl.setAttribute('data-ip', ip);\n" \
	"      if (ip !== oldIp && typeof _load_country === 'function') _load_country(ip, flagEl);\n" \
	"    }\n" \
	"\n" \
	"    var cc = row.querySelector('.c-caid');\n" \
	"    if (cc) {\n" \
	"      cc.classList.toggle('dim', !caid);\n" \
	"      cc.textContent = caid || '\\u2014';\n" \
	"      var sid = String(u.sid || '');\n" \
	"      var ch = String(u.channel || '');\n" \
	"      cc.title = caid ? 'Now watching: ' + (ch || 'unknown channel') + (sid ? ' (SID ' + sid + ')' : '') + '\\nAllowed CAIDs: ' + (row.dataset.allowed || 'none') : (online ? 'Online, no ECM received yet' : 'Offline') + '\\nAllowed CAIDs: ' + (row.dataset.allowed || 'none');\n" \
	"    }\n" \
	"    setCellText(row, 'c-ok', fmtN(ok));\n" \
	"    var nokEl = setCellText(row, 'c-nok', fmtN(nok));\n" \
	"    if (nokEl) nokEl.classList.toggle('tr', nok > 0), nokEl.classList.toggle('dim', nok <= 0);\n" \
	"\n" \
	"    var p = row.querySelector('.c-proto');\n" \
	"    if (p) {\n" \
	"      var badge = p.querySelector('.badge');\n" \
	"      p.classList.toggle('dim', !proto);\n" \
	"      if (badge) badge.textContent = proto || '\\u2014';\n" \
	"      else p.textContent = proto || '\\u2014';\n" \
	"    }\n" \
	"    setCellText(row, 'c-idle', fmtLiveIdle(u.idle));\n" \
	"    var last = setCellText(row, 'c-last', fmtLiveAgo(u.last_seen));\n" \
	"    if (last) last.title = (+u.last_seen > 0) ? new Date(+u.last_seen * 1000).toLocaleString() : '';\n" \
	"\n" \
	"    var cb = row.querySelector('.en-box');\n" \
	"    if (cb) cb.checked = en;\n" \
	"    return true;\n" \
	"  }\n" \
	"\n" \
	"  window.tcmg_poll_apply = function (d) {\n" \
	"    if (!d) return;\n" \
	"    _upd_topbar(d);\n" \
	"    if (!Array.isArray(d.users)) return;\n" \
	"    rebuildRowMap();\n" \
	"\n" \
	"    var seen = Object.create(null), structural = (d.users.length !== rowsAll().length);\n" \
	"    for (var i = 0; i < d.users.length; i++) {\n" \
	"      var u = d.users[i];\n" \
	"      seen[u.user] = 1;\n" \
	"      if (!applyLiveUser(u)) structural = true;\n" \
	"    }\n" \
	"    if (!structural) {\n" \
	"      rowsAll().forEach(function (r) { if (!seen[r.dataset.user]) structural = true; });\n" \
	"    }\n" \
	"    if (structural) {\n" \
	"      return softRefresh().then(function () { rebuildRowMap(); });\n" \
	"    }\n" \
	"    recount();\n" \
	"    applyFilter();\n" \
	"  };\n" \
	"\n" \
	"  function softRefresh() {\n" \
	"    return fetch(location.pathname + location.search, { credentials: 'same-origin', cache: 'no-store' })\n" \
	"      .then(function (r) {\n" \
	"        if (r.status === 401) { location.href = '/login'; return null; }\n" \
	"        return r.text();\n" \
	"      })\n" \
	"      .then(function (html) {\n" \
	"        if (!html) return;\n" \
	"        var doc = new DOMParser().parseFromString(html, 'text/html');\n" \
	"        var nb = doc.getElementById('usrBody');\n" \
	"        var cb = document.getElementById('usrBody');\n" \
	"        if (nb && cb) cb.innerHTML = nb.innerHTML;\n" \
	"        Array.prototype.forEach.call(document.querySelectorAll('.uipflag[data-ip]'), function (el) {\n" \
	"          if (typeof _load_country !== 'function') return;\n" \
	"          _load_country(el.getAttribute('data-ip'), el);\n" \
	"        });\n" \
	"        refreshAll();\n" \
	"        rebuildRowMap();\n" \
	"      })\n" \
	"      .catch(function () { location.reload(); });\n" \
	"  }\n" \
	"\n" \
	"  window.tcmgSoftRefresh = softRefresh;\n" \
	"\n" \
	"  \n" \
	"  function setEnabled(tr, en) {\n" \
	"    tr.dataset.en = en ? '1' : '0';\n" \
	"    tr.dataset.state = !en ? 'disabled' : (tr.dataset.expd === '1' ? 'expired' : 'active');\n" \
	"    tr.dataset.vis = !en ? 'disabled' : (tr.dataset.expd === '1' ? 'expired' : '');\n" \
	"    var cb = tr.querySelector('.en-box');\n" \
	"    if (cb) cb.checked = en;\n" \
	"    if (!en) tr.dataset.online = '0';   \n" \
	"  }\n" \
	"\n" \
	"  function toggleUser(tr, cb) {\n" \
	"    var want = cb.checked;\n" \
	"    cb.disabled = true;\n" \
	"    api('/api/user/toggle?user=' + encodeURIComponent(tr.dataset.user)).then(function (d) {\n" \
	"      cb.disabled = false;\n" \
	"      if (!d.ok) { cb.checked = !want; toast(d.msg || 'Could not change the account state', 'err'); return; }\n" \
	"      setEnabled(tr, !!d.enabled);\n" \
	"      refreshAll();\n" \
	"      toast(tr.dataset.user + (d.enabled ? ' enabled' : ' disabled'), 'ok');\n" \
	"    }).catch(function (e) { cb.disabled = false; cb.checked = !want; netErr(e); });\n" \
	"  }\n" \
	"\n" \
	"  function deleteUser(tr) {\n" \
	"    var u = tr.dataset.user;\n" \
	"    ask({\n" \
	"      title: 'Delete user?',\n" \
	"      msg: '\\u201C' + u + '\\u201D will be permanently removed from the configuration. This cannot be undone.',\n" \
	"      ok: 'Delete', danger: true\n" \
	"    }).then(function (yes) {\n" \
	"      if (!yes) return;\n" \
	"      api('/api/user/delete?user=' + encodeURIComponent(u)).then(function (d) {\n" \
	"        if (!d.ok) { toast(d.msg || 'Delete failed', 'err'); return; }\n" \
	"        tr.classList.add('gone');\n" \
	"        setTimeout(function () { if (tr.parentNode) tr.parentNode.removeChild(tr); refreshAll(); }, 290);\n" \
	"        toast('Deleted ' + u, 'ok');\n" \
	"      }).catch(netErr);\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  function resetStats(tr) {\n" \
	"    var u = tr.dataset.user;\n" \
	"    ask({\n" \
	"      title: 'Reset statistics?',\n" \
	"      msg: 'ECM counters, latency, first login and last seen of \\u201C' + u + '\\u201D will be set to zero.',\n" \
	"      ok: 'Reset', danger: false\n" \
	"    }).then(function (yes) {\n" \
	"      if (!yes) return;\n" \
	"      api('/api/user/resetstats?user=' + encodeURIComponent(u)).then(function (d) {\n" \
	"        if (!d.ok) { toast(d.msg || 'Reset failed', 'err'); return; }\n" \
	"        flash('Statistics reset for ' + u);\n" \
	"        softRefresh();\n" \
	"      }).catch(netErr);\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  \n" \
	"\n" \
	"  var um = $('uModal');\n" \
	"\n" \
	"  function setSwitch(on) {\n" \
	"    var s = $('em_enabled');\n" \
	"    s.classList.toggle('on', on);\n" \
	"    s.setAttribute('aria-checked', on ? 'true' : 'false');\n" \
	"  }\n" \
	"  function toggleAS(on) {\n" \
	"    var s = $('em_as_enabled');\n" \
	"    s.classList.toggle('on', on);\n" \
	"    s.setAttribute('aria-checked', on ? 'true' : 'false');\n" \
	"    $('em_as_fields').hidden = !on;\n" \
	"  }\n" \
	"  function showErr(msg, focusId) {\n" \
	"    $('em_err_msg').textContent = msg;\n" \
	"    $('em_err').hidden = false;\n" \
	"    if (focusId) $(focusId).focus();\n" \
	"  }\n" \
	"\n" \
	"  function openUM(isAdd, d, opener) {\n" \
	"    $('umTitle').textContent = isAdd ? 'Add User' : 'Edit User';\n" \
	"    $('em_unew_wrap').hidden = !isAdd;\n" \
	"    $('em_udisp_wrap').hidden = isAdd;\n" \
	"    $('em_user').value = isAdd ? '' : d.user;\n" \
	"    $('em_unew').value = '';\n" \
	"    $('em_udisp').value = isAdd ? '' : d.user;\n" \
	"    $('em_pass').value = isAdd ? '' : (d.pass || '');\n" \
	"    $('em_groups').value = isAdd ? '1' : (d.groups || '1');\n" \
	"    $('em_caid').value = isAdd ? '' : (d.caid || '');\n" \
	"    $('em_maxconn').value = isAdd ? '0' : d.max_connections;\n" \
	"    var ex = isAdd ? '' : (d.expiry && d.expiry !== '0' ? d.expiry : '');\n" \
	"    $('em_expiry').value = ex;\n" \
	"    setSwitch(isAdd ? true : !!d.enabled);\n" \
	"    toggleAS(isAdd ? false : !!d.anti_share);\n" \
	"    $('em_as_sids').value = isAdd ? '1' : (d.as_max_sids || 1);\n" \
	"    $('em_as_ecm').value = isAdd ? '0' : (d.as_max_ecm || 0);\n" \
	"    $('em_as_window').value = isAdd ? '60' : (d.as_ecm_window_s || 60);\n" \
	"    $('em_as_timeout').value = isAdd ? '15' : (d.as_channel_timeout_s || 15);\n" \
	"    $('em_as_delay').value = isAdd ? '1' : (d.as_switch_delay_s || 0);\n" \
	"    $('em_err').hidden = true;\n" \
	"    var sb = $('em_saveBtn');\n" \
	"    sb.disabled = false;\n" \
	"    sb.textContent = isAdd ? 'Add User' : 'Save';\n" \
	"    um.hidden = false;\n" \
	"    pushModal(um, closeUM, opener);\n" \
	"    setTimeout(function () { $(isAdd ? 'em_unew' : 'em_pass').focus(); }, 30);\n" \
	"  }\n" \
	"\n" \
	"  function closeUM() {\n" \
	"    if (um.hidden) return;\n" \
	"    um.hidden = true;\n" \
	"    popModal(um);\n" \
	"  }\n" \
	"\n" \
	"  function editUser(tr, opener) {\n" \
	"    api('/api/user/get?user=' + encodeURIComponent(tr.dataset.user)).then(function (d) {\n" \
	"      if (!d.ok) { toast(d.msg || 'User not found', 'err'); return; }\n" \
	"      openUM(false, d, opener);\n" \
	"    }).catch(netErr);\n" \
	"  }\n" \
	"\n" \
	"  function addDays(n) {\n" \
	"    var inp = $('em_expiry');\n" \
	"    if (n === 0) { inp.value = ''; return; }\n" \
	"    var base = new Date(); base.setHours(0, 0, 0, 0);\n" \
	"    var m = /^(\\d{4})-(\\d{2})-(\\d{2})$/.exec(inp.value);\n" \
	"    if (m) {\n" \
	"      var cur = new Date(+m[1], +m[2] - 1, +m[3]);\n" \
	"      if (cur > base) base = cur;          \n" \
	"    }\n" \
	"    base.setDate(base.getDate() + n);\n" \
	"    inp.value = ymd(base);\n" \
	"  }\n" \
	"\n" \
	"  function parseCaids(s) {\n" \
	"    var t = s.split(/[\\s,;]+/).filter(Boolean), out = [], i, c;\n" \
	"    for (i = 0; i < t.length; i++) {\n" \
	"      if (!/^[0-9a-fA-F]{1,4}$/.test(t[i])) return null;\n" \
	"      c = ('0000' + t[i].toUpperCase()).slice(-4);\n" \
	"      if (out.indexOf(c) < 0) out.push(c);\n" \
	"    }\n" \
	"    return out;\n" \
	"  }\n" \
	"\n" \
	"  function saveUM() {\n" \
	"    var isAdd = !$('em_user').value;\n" \
	"    var username = isAdd ? $('em_unew').value.trim() : $('em_user').value;\n" \
	"    var groups = $('em_groups').value.trim();\n" \
	"    var caids = parseCaids($('em_caid').value);\n" \
	"    var maxc = $('em_maxconn').value.trim();\n" \
	"    var exp = $('em_expiry').value.trim();\n" \
	"    var asSids = $('em_as_sids').value.trim();\n" \
	"    var asEcm = $('em_as_ecm').value.trim();\n" \
	"    var asWindow = $('em_as_window').value.trim();\n" \
	"    var asTimeout = $('em_as_timeout').value.trim();\n" \
	"    var asDelay = $('em_as_delay').value.trim();\n" \
	"\n" \
	"    if (!username) return showErr('Username is required', 'em_unew');\n" \
	"    if (!/^\\d+(,\\d+)*$/.test(groups)) return showErr('Reader groups must be comma-separated numbers, e.g. 1,5', 'em_groups');\n" \
	"    if (!caids) return showErr('CAIDs must be hex values of 1\\u20134 digits separated by commas, e.g. 0B00,0B01', 'em_caid');\n" \
	"    if (caids.length > 9) return showErr('At most 9 CAIDs per user', 'em_caid');\n" \
	"    if (maxc && !/^\\d+$/.test(maxc)) return showErr('Max connections must be a whole number (0 = unlimited)', 'em_maxconn');\n" \
	"    if (exp && exp !== '0' && !/^\\d{4}-\\d{2}-\\d{2}$/.test(exp)) return showErr('Expiry must look like 2030-12-31', 'em_expiry');\n" \
	"    if ($('em_as_enabled').classList.contains('on')) {\n" \
	"      if (!/^\\d+$/.test(asSids) || +asSids < 1) return showErr('Max active channels must be 1 or more', 'em_as_sids');\n" \
	"      if (!/^\\d+$/.test(asEcm)) return showErr('Max ECM requests must be a whole number (0 = no limit)', 'em_as_ecm');\n" \
	"      if (!/^\\d+$/.test(asWindow) || +asWindow < 1 || +asWindow > 3600) return showErr('ECM rate window must be 1-3600 seconds', 'em_as_window');\n" \
	"      if (!/^\\d+$/.test(asTimeout) || +asTimeout < 1 || +asTimeout > 3600) return showErr('Channel timeout must be 1-3600 seconds', 'em_as_timeout');\n" \
	"      if (!/^\\d+$/.test(asDelay) || +asDelay > 30) return showErr('Channel switch delay must be 0-30 seconds', 'em_as_delay');\n" \
	"    }\n" \
	"    $('em_err').hidden = true;\n" \
	"\n" \
	"    var p = new URLSearchParams();\n" \
	"    p.set('user', username);\n" \
	"    p.set('pass', $('em_pass').value);\n" \
	"    p.set('groups', groups);\n" \
	"    p.set('caid', caids.length ? caids.join(',') : '0');\n" \
	"    p.set('maxconn', maxc || '0');\n" \
	"    p.set('enabled', $('em_enabled').classList.contains('on') ? '1' : '0');\n" \
	"    p.set('expiry', exp || '0');\n" \
	"    p.set('anti_share', $('em_as_enabled').classList.contains('on') ? '1' : '0');\n" \
	"    p.set('as_max_sids', asSids || '1');\n" \
	"    p.set('as_max_ecm', asEcm || '0');\n" \
	"    p.set('as_ecm_window_s', asWindow || '60');\n" \
	"    p.set('as_channel_timeout_s', asTimeout || '15');\n" \
	"    p.set('as_switch_delay_s', asDelay || '0');\n" \
	"\n" \
	"    var sb = $('em_saveBtn');\n" \
	"    sb.disabled = true;\n" \
	"    sb.textContent = 'Saving\\u2026';\n" \
	"    api(isAdd ? '/api/user/add' : '/api/user/save', {\n" \
	"      method: 'POST',\n" \
	"      body: p.toString(),\n" \
	"      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }\n" \
	"    }).then(function (d) {\n" \
	"      if (d.ok) {\n" \
	"        flash(isAdd ? 'User \\u201C' + username + '\\u201D added' : 'Changes saved for \\u201C' + username + '\\u201D');\n" \
	"        closeUM();\n" \
	"        softRefresh();\n" \
	"      } else {\n" \
	"        sb.disabled = false;\n" \
	"        sb.textContent = isAdd ? 'Add User' : 'Save';\n" \
	"        showErr(d.msg || 'Error');\n" \
	"      }\n" \
	"    }).catch(function (e) {\n" \
	"      sb.disabled = false;\n" \
	"      sb.textContent = isAdd ? 'Add User' : 'Save';\n" \
	"      if (!e || e.message !== 'unauthorized') showErr('Request failed \\u2014 check the connection');\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  \n" \
	"\n" \
	"  function setSort(k) {\n" \
	"    if (view.k === k) {\n" \
	"      if (view.d === KEYS[k].d0) view.d = -view.d;\n" \
	"      else { view.k = ''; view.d = 1; }          \n" \
	"    } else {\n" \
	"      view.k = k;\n" \
	"      view.d = KEYS[k].d0;\n" \
	"    }\n" \
	"    view.p = 1;\n" \
	"    saveView();\n" \
	"    sortRows();\n" \
	"    applyFilter();\n" \
	"  }\n" \
	"\n" \
	"  document.addEventListener('click', function (e) {\n" \
	"    var el = e.target.closest('[data-a]');\n" \
	"    if (el) {\n" \
	"      var a = el.getAttribute('data-a');\n" \
	"      var tr = el.closest('tr.urow');\n" \
	"      switch (a) {\n" \
	"        case 'tg':      toggleUser(tr, el); break;\n" \
	"        case 'edit':    editUser(tr, el); break;\n" \
	"        case 'del':     deleteUser(tr); break;\n" \
	"        case 'reset':   resetStats(tr); break;\n" \
	"        case 'add':     openUM(true, null, el); break;\n" \
	"        case 'refresh': softRefresh(); break;\n" \
	"        case 'close':   closeUM(); break;\n" \
	"        case 'save':    saveUM(); break;\n" \
	"        case 'sw':      setSwitch(!el.classList.contains('on')); break;\n" \
	"        case 'sw-as':   toggleAS(!el.classList.contains('on')); break;\n" \
	"        case 'exp':     addDays(parseInt(el.getAttribute('data-n'), 10) || 0); break;\n" \
	"        case 'clear':\n" \
	"          view.q = ''; view.f = 'all'; view.p = 1; $('usrSearch').value = '';\n" \
	"          saveView(); applyFilter();\n" \
	"          break;\n" \
	"      }\n" \
	"      return;\n" \
	"    }\n" \
	"    var tile = e.target.closest('.ust');\n" \
	"    if (tile) { view.f = tile.getAttribute('data-f'); view.p = 1; saveView(); applyFilter(); return; }\n" \
	"    var pg = e.target.closest('.page');\n" \
	"    if (pg && !pg.disabled) { view.p = parseInt(pg.getAttribute('data-page'), 10) || 1; applyFilter(); return; }\n" \
	"    var sb = e.target.closest('.table-sort');\n" \
	"    if (sb) setSort(sb.getAttribute('data-k'));\n" \
	"  });\n" \
	"\n" \
	"  document.addEventListener('keydown', function (e) {\n" \
	"    if (e.key === 'Enter' && !um.hidden && um.contains(e.target) && e.target.tagName === 'INPUT') {\n" \
	"      e.preventDefault();\n" \
	"      saveUM();\n" \
	"    }\n" \
	"  });\n" \
	"\n" \
	"  $('usrSearch').addEventListener('input', function () {\n" \
	"    view.q = this.value.trim().toLowerCase();\n" \
	"    view.p = 1;\n" \
	"    saveView();\n" \
	"    applyFilter();\n" \
	"  });\n" \
	"\n" \
	"  $('limit').addEventListener('change', function () {\n" \
	"    limit = parseInt(this.value, 10) || 50;\n" \
	"    try { localStorage.setItem('tcmg.users.limit', String(limit)); } catch (e) {}\n" \
	"    view.p = 1;\n" \
	"    applyFilter();\n" \
	"  });\n" \
	"\n" \
	"  \n" \
	"  loadView();\n" \
	"  $('usrSearch').value = view.q;\n" \
	"  sortRows();\n" \
	"  refreshAll();\n" \
	"  rebuildRowMap();\n" \
	"\n" \
	"  Array.prototype.forEach.call(document.querySelectorAll('.uipflag[data-ip]'), function (el) {\n" \
	"    if (typeof _load_country !== 'function') return;\n" \
	"    _load_country(el.getAttribute('data-ip'), el);\n" \
	"  });\n" \
	"\n" \
	"  try {\n" \
	"    var fl = sessionStorage.getItem('tcmg_u_flash');\n" \
	"    if (fl) { sessionStorage.removeItem('tcmg_u_flash'); toast(fl, 'ok'); }\n" \
	"  } catch (e) {}\n" \
	"})();"

#endif
