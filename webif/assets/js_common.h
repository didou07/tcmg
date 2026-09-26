#ifndef TCMG_WEBIF_JS_COMMON_H_
#define TCMG_WEBIF_JS_COMMON_H_

#define TCMG_JS \
	"\n" \
	"function _theme_apply(pref) {\n" \
	"  if (pref !== 'light') pref = 'dark';\n" \
	"  var r = document.documentElement;\n" \
	"  r.setAttribute('data-theme', pref);\n" \
	"  r.setAttribute('data-tpref', pref);\n" \
	"  var b = document.getElementById('thBtn');\n" \
	"  if (b) b.title = 'Theme: ' + pref + ' (click to change)';\n" \
	"}\n" \
	"\n" \
	"function _theme_cycle() {\n" \
	"  var cur = document.documentElement.getAttribute('data-tpref') || 'dark';\n" \
	"  var next = cur === 'dark' ? 'light' : 'dark';\n" \
	"  try { localStorage.setItem('tcmg_theme', next); } catch (e) {}\n" \
	"  _theme_apply(next);\n" \
	"}\n" \
	"\n" \
	"document.addEventListener('DOMContentLoaded', function() {\n" \
	"  _theme_apply(document.documentElement.getAttribute('data-tpref') || 'dark');\n" \
	"});\n" \
	"\n" \
	"(function(){" \
	"function mkq(text){" \
	"var q=document.createElement('button');q.type='button';q.className='qtip';q.textContent='?';" \
	"q.title=text;q.setAttribute('aria-label',text);return q;}" \
	"function run(){" \
	"document.querySelectorAll('.cfg-sub').forEach(function(el){" \
	"var t=(el.textContent||'').replace(/\\s+/g,' ').trim();if(!t)return;" \
	"var q=mkq(t),p=el.parentNode,ttl=el.previousElementSibling;" \
	"if(ttl)ttl.appendChild(q);else p.insertBefore(q,el);el.remove();});" \
	"document.querySelectorAll('.cfg-help,.fhint').forEach(function(el){" \
	"var t=(el.textContent||'').replace(/\\s+/g,' ').trim();if(!t){el.remove();return;}" \
	"var q=mkq(t);if(el.id)q.id=el.id;el.replaceWith(q);});" \
	"document.querySelectorAll('.cfg-intro').forEach(function(el){el.remove();});" \
	"}" \
	"if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',run);else run();" \
	"})();" \
	"\n" \
	"document.addEventListener('DOMContentLoaded', function() {\n" \
	"  document.querySelectorAll('.tnav a').forEach(function(a) {\n" \
	"    a.addEventListener('click', function() {\n" \
	"      var nav = document.querySelector('.tnav');\n" \
	"      if (nav) nav.classList.remove('open');\n" \
	"    });\n" \
	"  });\n" \
	"});\n" \
	"\n" \
	"var ACCENTS = [\n" \
	"  { id: 'blue',   p: '#147bd1', p2: '#106fbe' },\n" \
	"  { id: 'purple', p: '#7c3aed', p2: '#6d28d9' },\n" \
	"  { id: 'teal',   p: '#0e9488', p2: '#0c7e73' },\n" \
	"  { id: 'green',  p: '#16a34a', p2: '#128a3e' },\n" \
	"  { id: 'rose',   p: '#db2777', p2: '#be185d' },\n" \
	"  { id: 'amber',  p: '#d97706', p2: '#b96204' }\n" \
	"];\n" \
	"function _accentRgba(hex, a) {\n" \
	"  var n = parseInt(hex.slice(1), 16);\n" \
	"  return 'rgba(' + ((n >> 16) & 255) + ',' + ((n >> 8) & 255) + ',' + (n & 255) + ',' + a + ')';\n" \
	"}\n" \
	"function applyAccent(id, persist) {\n" \
	"  var preset = null;\n" \
	"  for (var i = 0; i < ACCENTS.length; i++) if (ACCENTS[i].id === id) preset = ACCENTS[i];\n" \
	"  if (!preset) preset = ACCENTS[0];\n" \
	"  var r = document.documentElement.style;\n" \
	"  r.setProperty('--p', preset.p);\n" \
	"  r.setProperty('--p2', preset.p2);\n" \
	"  r.setProperty('--ps', _accentRgba(preset.p, .10));\n" \
	"  r.setProperty('--pg', _accentRgba(preset.p, .22));\n" \
	"  if (persist !== false) { try { localStorage.setItem('tcmg_accent', preset.id); } catch (e) {} }\n" \
	"  document.querySelectorAll('.ac-swatch').forEach(function (b) {\n" \
	"    b.classList.toggle('cur', b.dataset.accent === preset.id);\n" \
	"  });\n" \
	"}\n" \
	"document.addEventListener('DOMContentLoaded', function () {\n" \
	"  var btn = document.getElementById('acBtn'), pop = document.getElementById('acPop');\n" \
	"  if (!btn || !pop) return;\n" \
	"  pop.innerHTML = ACCENTS.map(function (p) {\n" \
	"    return \"<button type='button' class='ac-swatch' data-accent='\" + p.id + \"'\" +\n" \
	"      \" style='background:\" + p.p + \"' title='Accent color' aria-label='Accent color'\" +\n" \
	"      \" role='menuitemradio'></button>\";\n" \
	"  }).join('');\n" \
	"  var saved = 'blue';\n" \
	"  try { saved = localStorage.getItem('tcmg_accent') || 'blue'; } catch (e) {}\n" \
	"  applyAccent(saved, false);\n" \
	"  btn.addEventListener('click', function (e) {\n" \
	"    e.stopPropagation();\n" \
	"    pop.hidden = !pop.hidden;\n" \
	"    btn.setAttribute('aria-expanded', String(!pop.hidden));\n" \
	"  });\n" \
	"  pop.addEventListener('click', function (e) {\n" \
	"    var sw = e.target.closest ? e.target.closest('.ac-swatch') : null;\n" \
	"    if (!sw) return;\n" \
	"    applyAccent(sw.dataset.accent);\n" \
	"  });\n" \
	"  document.addEventListener('click', function (e) {\n" \
	"    if (!pop.hidden && !e.target.closest('#acp')) {\n" \
	"      pop.hidden = true;\n" \
	"      btn.setAttribute('aria-expanded', 'false');\n" \
	"    }\n" \
	"  });\n" \
	"});\n" \
	"\n" \
	"var _pm = (function() {\n" \
	"  var srv = Number(window.TCMG_WEB_POLL) || 0;\n" \
	"  if (srv <= 0) return 0;\n" \
	"  var stored = parseInt(sessionStorage.tcmg_poll);\n" \
	"  var v = (stored >= 1 && stored <= 99) ? stored : srv;\n" \
	"  var el = document.getElementById('ps_');\n" \
	"  if (el) el.value = v;\n" \
	"  return v * 1000;\n" \
	"})();\n" \
	"\n" \
	"var _pit = null, _busy = false, _ut = 0, _ut_tmr = null;\n" \
	"\n" \
	"var _cc_cache = {};\n" \
	"var _cc_waiters = {};\n" \
	"\n" \
	"function _is_public_ipv4(ip) {\n" \
	"  var m = String(ip).match(/^(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$/);\n" \
	"  if (!m) return false;\n" \
	"  var a = m.slice(1).map(Number);\n" \
	"  if (a.some(function(n) { return n < 0 || n > 255; })) return false;\n" \
	"  if (a[0] === 10 || a[0] === 127 || a[0] === 0 || a[0] >= 224) return false;\n" \
	"  if (a[0] === 192 && a[1] === 168) return false;\n" \
	"  if (a[0] === 172 && a[1] >= 16 && a[1] <= 31) return false;\n" \
	"  if (a[0] === 169 && a[1] === 254) return false;\n" \
	"  return true;\n" \
	"}\n" \
	"\n" \
	"function _country_flag(cc) {\n" \
	"  cc = String(cc || '').toUpperCase();\n" \
	"  if (!/^[A-Z]{2}$/.test(cc)) return '';\n" \
	"  return String.fromCodePoint(cc.charCodeAt(0) + 127397, cc.charCodeAt(1) + 127397);\n" \
	"}\n" \
	"\n" \
	"function _geo_providers(ip) {\n" \
	"  var q = encodeURIComponent(ip);\n" \
	"  return [\n" \
	"    { url: 'https://ipwho.is/' + q,\n" \
	"      parse: function (d) { return (d && d.success !== false && d.country_code) ? { code: d.country_code, country: d.country } : null; } },\n" \
	"    { url: 'https://ipapi.co/' + q + '/json/',\n" \
	"      parse: function (d) { return (d && !d.error && d.country_code) ? { code: d.country_code, country: d.country_name } : null; } },\n" \
	"    { url: 'https://geolocation-db.com/json/' + q,\n" \
	"      parse: function (d) { return (d && d.country_code && d.country_code !== 'Not found') ? { code: d.country_code, country: d.country_name } : null; } }\n" \
	"  ];\n" \
	"}\n" \
	"\n" \
	"function _fetch_json_timeout(url, ms) {\n" \
	"  var ctrl = (typeof AbortController !== 'undefined') ? new AbortController() : null;\n" \
	"  var t = ctrl ? setTimeout(function () { ctrl.abort(); }, ms) : null;\n" \
	"  return fetch(url, { cache: 'force-cache', signal: ctrl ? ctrl.signal : undefined })\n" \
	"    .then(function (r) { if (t) clearTimeout(t); return r.ok ? r.json() : null; })\n" \
	"    .catch(function () { if (t) clearTimeout(t); return null; });\n" \
	"}\n" \
	"\n" \
	"function _geo_try(providers, i) {\n" \
	"  if (i >= providers.length) return Promise.resolve(null);\n" \
	"  return _fetch_json_timeout(providers[i].url, 4000).then(function (d) {\n" \
	"    var r = d ? providers[i].parse(d) : null;\n" \
	"    return r || _geo_try(providers, i + 1);\n" \
	"  });\n" \
	"}\n" \
	"\n" \
	"function toast(message, kind) {\n" \
	"  var box = document.querySelector('.toasts');\n" \
	"  if (!box) {\n" \
	"    box = document.createElement('div');\n" \
	"    box.className = 'toasts';\n" \
	"    box.setAttribute('role', 'status');\n" \
	"    box.setAttribute('aria-live', 'polite');\n" \
	"    document.body.appendChild(box);\n" \
	"  }\n" \
	"  var item = document.createElement('div');\n" \
	"  item.className = 'toast ' + (kind || '');\n" \
	"  item.textContent = message;\n" \
	"  box.appendChild(item);\n" \
	"  setTimeout(function () {\n" \
	"    item.style.transition = 'opacity .25s';\n" \
	"    item.style.opacity = '0';\n" \
	"    setTimeout(function () { if (item.parentNode) item.parentNode.removeChild(item); }, 260);\n" \
	"  }, kind === 'err' ? 5000 : 2600);\n" \
	"}\n" \
	"\n" \
	"function tcmg_api(url, opt) {\n" \
	"  opt = opt || {};\n" \
	"  if (!opt.credentials) opt.credentials = 'same-origin';\n" \
	"  if (!opt.cache) opt.cache = 'no-store';\n" \
	"  return fetch(url, opt).then(function (r) {\n" \
	"    if (r.status === 401) {\n" \
	"      window.location.href = '/login';\n" \
	"      var e = new Error('unauthorized'); e.auth = true;\n" \
	"      throw e;\n" \
	"    }\n" \
	"    return r.text().then(function (body) {\n" \
	"      var data = null;\n" \
	"      if (body) { try { data = JSON.parse(body); } catch (ignore) {} }\n" \
	"      if (!r.ok) {\n" \
	"        var err = new Error(data && data.msg ? data.msg : ('HTTP ' + r.status));\n" \
	"        err.status = r.status; err.data = data;\n" \
	"        throw err;\n" \
	"      }\n" \
	"      return data || {};\n" \
	"    });\n" \
	"  });\n" \
	"}\n" \
	"\n" \
	"function tcmg_html(url, opt) {\n" \
	"  opt = opt || {};\n" \
	"  if (!opt.credentials) opt.credentials = 'same-origin';\n" \
	"  if (!opt.cache) opt.cache = 'no-store';\n" \
	"  return fetch(url, opt).then(function (r) {\n" \
	"    if (r.status === 401) { window.location.href = '/login'; throw new Error('unauthorized'); }\n" \
	"    if (!r.ok) throw new Error('HTTP ' + r.status);\n" \
	"    return r.text();\n" \
	"  });\n" \
	"}\n" \
	"\n" \
	"function _flag_img_html(code) {\n" \
	"  var cc = String(code || '').toLowerCase();\n" \
	"  if (!/^[a-z]{2}$/.test(cc)) return '';\n" \
	"  return '<img src=\"https://flagcdn.com/16x12/' + cc + '.png\" width=\"16\" height=\"12\" alt=\"\" loading=\"lazy\" style=\"vertical-align:middle;border-radius:2px;display:inline-block\">';\n" \
	"}\n" \
	"\n" \
	"function _load_country(ip, el, nameEl) {\n" \
	"  if (!_is_public_ipv4(ip)) return;\n" \
	"  function apply(c) {\n" \
	"    if (el) {\n" \
	"      var img = _flag_img_html(c.code);\n" \
	"      if (img) el.innerHTML = img; else el.textContent = c.flag || c.code || '';\n" \
	"      el.title = c.country || c.code;\n" \
	"    }\n" \
	"    if (nameEl) nameEl.textContent = c.country || c.code || '';\n" \
	"  }\n" \
	"  if (Object.prototype.hasOwnProperty.call(_cc_cache, ip)) {\n" \
	"    if (_cc_cache[ip].code || _cc_cache[ip].flag) {\n" \
	"      apply(_cc_cache[ip]);\n" \
	"    } else if (_cc_cache[ip].pending) {\n" \
	"      if (!_cc_waiters[ip]) _cc_waiters[ip] = [];\n" \
	"      _cc_waiters[ip].push({ el: el, nameEl: nameEl });\n" \
	"    }\n" \
	"    return;\n" \
	"  }\n" \
	"  _cc_cache[ip] = { pending: true };\n" \
	"  _cc_waiters[ip] = [{ el: el, nameEl: nameEl }];\n" \
	"  _geo_try(_geo_providers(ip), 0)\n" \
	"    .then(function (r) {\n" \
	"      var waiters = _cc_waiters[ip] || [];\n" \
	"      delete _cc_waiters[ip];\n" \
	"      if (!r) { delete _cc_cache[ip]; return; }\n" \
	"      var flag = _country_flag(r.code);\n" \
	"      _cc_cache[ip] = { code: r.code, country: r.country || r.code, flag: flag };\n" \
	"      waiters.forEach(function (w) {\n" \
	"        if (w.el) {\n" \
	"          var img = _flag_img_html(_cc_cache[ip].code);\n" \
	"          if (img) w.el.innerHTML = img; else w.el.textContent = flag || _cc_cache[ip].code || '';\n" \
	"          w.el.title = _cc_cache[ip].country;\n" \
	"        }\n" \
	"        if (w.nameEl) w.nameEl.textContent = _cc_cache[ip].country;\n" \
	"      });\n" \
	"    })\n" \
	"    .catch(function () { delete _cc_cache[ip]; delete _cc_waiters[ip]; });\n" \
	"}\n" \
	"\n" \
	"function _schedule_poll(ms) {\n" \
	"  if (_pm <= 0) return;\n" \
	"  if (_pit) clearTimeout(_pit);\n" \
	"  var delay = Math.max(25, ms | 0);\n" \
	"  _pit = setTimeout(function () {\n" \
	"    _pit = null;\n" \
	"    if (document.hidden) return;\n" \
	"    _poll();\n" \
	"  }, delay);\n" \
	"}\n" \
	"\n" \
	"function _ap(d) {\n" \
	"  if (_pm === 0) return;\n" \
	"  var el = document.getElementById('ps_');\n" \
	"  var v = Math.max(1, Math.min(99, parseInt(el.value) || 5) + d);\n" \
	"  el.value = v;\n" \
	"  _pm = v * 1000;\n" \
	"  try { sessionStorage.setItem('tcmg_poll', String(v)); } catch (e) {}\n" \
	"  if (!_busy) _schedule_poll(_pm);\n" \
	"}\n" \
	"\n" \
	"function _fmt_up(s) {\n" \
	"  var h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sc = s % 60;\n" \
	"  return (h > 0 ? String(h).padStart(2, '0') + 'h ' : '')\n" \
	"    + String(m).padStart(2, '0') + 'm '\n" \
	"    + String(sc).padStart(2, '0') + 's';\n" \
	"}\n" \
	"\n" \
	"function _esc(s) {\n" \
	"  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')\n" \
	"                  .replace(/\"/g, '&quot;').replace(/'/g, '&#39;');\n" \
	"}\n" \
	"\n" \
	"function _fmt(n) {\n" \
	"  return n >= 1e6 ? (n / 1e6).toFixed(1) + 'M'\n" \
	"       : n >= 1e3 ? (n / 1e3).toFixed(1) + 'K'\n" \
	"       : String(n);\n" \
	"}\n" \
	"\n" \
	"function _anim(id, v) {\n" \
	"  var e = document.getElementById(id);\n" \
	"  if (!e) return;\n" \
	"  if (e.textContent === String(v)) return;\n" \
	"  e.textContent = v;\n" \
	"  e.classList.remove('cnt-up');\n" \
	"  void e.offsetWidth;\n" \
	"  e.classList.add('cnt-up');\n" \
	"}\n" \
	"\n" \
	"function _upd_topbar(d) {\n" \
	"  var e = document.getElementById('tb_conn');\n" \
	"  if (e) e.textContent = d.active_connections;\n" \
	"}\n" \
	"\n" \
	"function _uptime_tick() {\n" \
	"  _ut_tmr = null;\n" \
	"  if (document.hidden) return;\n" \
	"  _ut++;\n" \
	"  var e = document.getElementById('p_up');\n" \
	"  if (e) e.textContent = _fmt_up(_ut);\n" \
	"  _ut_tmr = setTimeout(_uptime_tick, 1000);\n" \
	"}\n" \
	"function _upd_status(d) {\n" \
	"  _ut = d.uptime_s | 0;\n" \
	"  if (!_ut_tmr && !document.hidden) _ut_tmr = setTimeout(_uptime_tick, 1000);\n" \
	"  var eu = document.getElementById('p_up');\n" \
	"  if (eu) eu.textContent = _fmt_up(_ut);\n" \
	"\n" \
	"  _anim('p_conn', d.active_connections);\n" \
	"  _anim('p_acc',  d.accounts);\n" \
	"  _anim('p_hit',  _fmt(d.cw_found));\n" \
	"  _anim('p_miss', _fmt(d.cw_not));\n" \
	"  _anim('p_ban',  d.banned_ips);\n" \
	"  _anim('p_ecm',  _fmt(d.ecm_total));\n" \
	"\n" \
	"  var hr = document.getElementById('p_hr');\n" \
	"  if (hr) hr.textContent = d.hit_rate_pct.toFixed(1) + '%';\n" \
	"\n" \
	"  var hb = document.getElementById('p_hbf');\n" \
	"  if (hb) hb.style.width = d.hit_rate_pct.toFixed(0) + '%';\n" \
	"\n" \
	"  var tb = document.getElementById('p_clients');\n" \
	"  if (!tb) return;\n" \
	"\n" \
	"  if (!tb._kb) {\n" \
	"    tb._kb = 1;\n" \
	"    tb.addEventListener('click', function(e) {\n" \
	"      var b = e.target.closest ? e.target.closest('.kb') : null;\n" \
	"      if (b) _kill(b.getAttribute('data-tid'), b.getAttribute('data-user'));\n" \
	"    });\n" \
	"  }\n" \
	"\n" \
	"  var rows = Array.from(tb.querySelectorAll('tr[id^=\"row_\"]'));\n" \
	"  var empty = tb.querySelector('.erow');\n" \
	"\n" \
	"  if (!d.clients || !d.clients.length) {\n" \
	"    rows.forEach(function(r) { if (r.parentNode) r.parentNode.removeChild(r); });\n" \
	"    if (!empty) tb.innerHTML = '<tr class=\"erow\"><td colspan=\"9\">No active connections</td></tr>';\n" \
	"    return;\n" \
	"  }\n" \
	"  if (empty) empty.parentNode.removeChild(empty);     \n" \
	"\n" \
	"  var ids = {};\n" \
	"  d.clients.forEach(function(cl) { ids[cl.thread_id] = 1; });\n" \
	"\n" \
	"  rows.forEach(function(r) {\n" \
	"    var tid = r.id.slice(4);\n" \
	"    if (!ids[tid] && r.getAttribute('data-gone') !== '1') {\n" \
	"      r.setAttribute('data-gone', '1');\n" \
	"      r.style.opacity = '.4';\n" \
	"      setTimeout(function() { if (r.parentNode) r.parentNode.removeChild(r); }, 800);\n" \
	"    }\n" \
	"  });\n" \
	"\n" \
	"  d.clients.forEach(function(cl) {\n" \
	"    var existing = document.getElementById('row_' + cl.thread_id);\n" \
	"    if (existing) {\n" \
	"\n" \
	"      _cell(existing, 'c-caid', cl.caid);\n" \
	"      _cell(existing, 'c-sid', cl.sid);\n" \
	"      _cell(existing, 'c-ch', cl.channel || '\\u2014');\n" \
	"      _cell(existing, 'c-con', cl.connected);\n" \
	"      _cell(existing, 'c-idl', cl.idle);\n" \
	"      _load_country(cl.ip, existing.querySelector('.flagcol'));\n" \
	"      return;\n" \
	"    }\n" \
	"    var tr = document.createElement('tr');\n" \
	"    tr.className = 'nw';\n" \
	"    tr.id = 'row_' + cl.thread_id;\n" \
	"    tr.innerHTML =\n" \
	"      '<td><span class=\"flagcol\" aria-label=\"\"></span></td>'\n" \
	"      + '<td class=\"bold\">' + _esc(cl.user) + '</td>'\n" \
	"      + '<td class=\"mono\">' + _esc(cl.ip) + '</td>'\n" \
	"      + '<td class=\"mono\"><span class=\"badge bbl c-caid\">' + _esc(cl.caid) + '</span></td>'\n" \
	"      + '<td class=\"mono c-sid\">' + _esc(cl.sid) + '</td>'\n" \
	"      + '<td class=\"c-ch\">' + (cl.channel ? _esc(cl.channel) : '&mdash;') + '</td>'\n" \
	"      + '<td class=\"mono tm c-con\">' + _esc(cl.connected) + '</td>'\n" \
	"      + '<td class=\"mono tm c-idl\">' + _esc(cl.idle) + '</td>'\n" \
	"      + '<td><button class=\"kb\" data-tid=\"' + _esc(cl.thread_id) + '\" data-user=\"' + _esc(cl.user)\n" \
	"        + '\" title=\"Disconnect\" aria-label=\"Disconnect\"><svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4\"/><polyline points=\"16 17 21 12 16 7\"/><line x1=\"21\" y1=\"12\" x2=\"9\" y2=\"12\"/></svg></button></td>';\n" \
	"    tb.appendChild(tr);\n" \
	"    _load_country(cl.ip, tr.querySelector('.flagcol'));\n" \
	"  });\n" \
	"}\n" \
	"\n" \
	"function _cell(row, cls, val) {\n" \
	"  var e = row.querySelector('.' + cls);\n" \
	"  val = String(val);\n" \
	"  if (e && e.textContent !== val) e.textContent = val;\n" \
	"}\n" \
	"\n" \
	"function _kill(tid, user) {\n" \
	"  if (!confirm('Disconnect ' + user + '?')) return;\n" \
	"  tcmg_api('/api/client/kill?tid=' + encodeURIComponent(tid) + '&user=' + encodeURIComponent(user), {method: 'POST'})\n" \
	"    .catch(function(e) { if (!(e && e.auth)) toast('Disconnect failed', 'err'); });\n" \
	"  var r = document.getElementById('row_' + tid);\n" \
	"  if (r) {\n" \
	"    r.style.opacity = '.4';\n" \
	"    setTimeout(function() { if (r.parentNode) r.parentNode.removeChild(r); }, 800);\n" \
	"  }\n" \
	"}\n" \
	"\n" \
	"function _poll() {\n" \
	"  if (_busy || document.hidden) return;\n" \
	"  _busy = true;\n" \
	"\n" \
	"  var endpoint = '/api/status?lite=1';\n" \
	"  if (document.getElementById('p_clients')) {\n" \
	"    endpoint = '/api/status';\n" \
	"  } else if (document.getElementById('usrBody')) {\n" \
	"    endpoint = '/api/userstats';\n" \
	"  }\n" \
	"\n" \
	"  tcmg_api(endpoint)\n" \
	"    .then(function(d) {\n" \
	"      if (!d) return;\n" \
	"      var hook = window.tcmg_poll_apply;\n" \
	"      if (typeof hook === 'function') return Promise.resolve(hook(d));\n" \
	"      _upd_topbar(d);\n" \
	"      if (document.getElementById('p_clients')) _upd_status(d);\n" \
	"    })\n" \
	"    .then(function() {\n" \
	"      _busy = false;\n" \
	"      _schedule_poll(_pm);\n" \
	"    })\n" \
	"    .catch(function(e) {\n" \
	"      _busy = false;\n" \
	"      if (e && e.auth) return;\n" \
	"      _schedule_poll(Math.max(15000, _pm));\n" \
	"    });\n" \
	"}\n" \
	"\n" \
	"document.addEventListener('DOMContentLoaded', function() {\n" \
	"  _poll();\n" \
	"});\n" \
	"\n" \
	"document.addEventListener('visibilitychange', function() {\n" \
	"  if (!document.hidden) {\n" \
	"    if (!_busy && _pm > 0) _schedule_poll(50);\n" \
	"    if (document.getElementById('p_up') && !_ut_tmr) _ut_tmr = setTimeout(_uptime_tick, 1000);\n" \
	"  } else if (_ut_tmr) {\n" \
	"    clearTimeout(_ut_tmr); _ut_tmr = null;\n" \
	"  }\n" \
	"});"

#endif
