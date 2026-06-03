/* tcmg.js — shared WebIF runtime
 *
 * NOTE: This file contains printf format specifiers (%d, %%) because it is
 * embedded into the HTML response via buf_printf() in webif_layout.c.
 * Do NOT remove the %% sequences — they produce literal % in HTML output.
 * The %d near the top is replaced at runtime with the configured poll interval.
 *
 * Sections:
 *   1. Mobile nav toggle
 *   2. Poll interval control
 *   3. Utility helpers (format, escape, animate)
 *   4. Topbar updater
 *   5. Status page updater (client table + stat cards)
 *   6. Kill-client handler
 *   7. Poll loop
 */

/* 1. Mobile nav toggle */
document.querySelectorAll('.tnav a').forEach(function(a) {
  a.addEventListener('click', function() {
    document.querySelector('.tnav').classList.remove('open');
  });
});

/* 2. Poll interval control — initial value comes from server config */
var _pm = (function() {
  var srv = %d;
  var stored = parseInt(sessionStorage.tcmg_poll);
  var v = (stored >= 1 && stored <= 99) ? stored : srv;
  var el = document.getElementById('ps_');
  if (el) el.value = v;
  return v * 1000;
})();

var _pit = null, _busy = false, _ut = 0, _ut_tmr = null;

function _ap(d) {
  var el = document.getElementById('ps_');
  var v = Math.max(1, Math.min(99, parseInt(el.value) || 5) + d);
  el.value = v;
  _pm = v * 1000;
  sessionStorage.tcmg_poll = v;
  if (_pit) { clearInterval(_pit); _pit = setInterval(_poll, _pm); }
}

/* 3. Utility helpers */
function _fmt_up(s) {
  var h = Math.floor(s / 3600), m = Math.floor((s %% 3600) / 60), sc = s %% 60;
  return (h > 0 ? String(h).padStart(2, '0') + 'h ' : '')
    + String(m).padStart(2, '0') + 'm '
    + String(sc).padStart(2, '0') + 's';
}

function _esc(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function _fmt(n) {
  return n >= 1e6 ? (n / 1e6).toFixed(1) + 'M'
       : n >= 1e3 ? (n / 1e3).toFixed(1) + 'K'
       : String(n);
}

function _anim(id, v) {
  var e = document.getElementById(id);
  if (!e) return;
  if (e.textContent === String(v)) return;
  e.textContent = v;
  e.classList.remove('cnt-up');
  void e.offsetWidth;
  e.classList.add('cnt-up');
}

/* 4. Topbar updater */
function _upd_topbar(d) {
  var e = document.getElementById('tb_conn');
  if (e) e.textContent = d.active_connections;
}

/* 5. Status page updater */
function _upd_status(d) {
  _ut = d.uptime_s | 0;
  if (!_ut_tmr) {
    _ut_tmr = setInterval(function() {
      _ut++;
      var e = document.getElementById('p_up');
      if (e) e.textContent = _fmt_up(_ut);
    }, 1000);
  }
  var eu = document.getElementById('p_up');
  if (eu) eu.textContent = _fmt_up(_ut);

  _anim('p_conn', d.active_connections);
  _anim('p_acc',  d.accounts);
  _anim('p_hit',  _fmt(d.cw_found));
  _anim('p_miss', _fmt(d.cw_not));
  _anim('p_ban',  d.banned_ips);
  _anim('p_ecm',  _fmt(d.ecm_total));

  var hr = document.getElementById('p_hr');
  if (hr) hr.textContent = d.hit_rate_pct.toFixed(1) + '%%';

  var hb = document.getElementById('p_hbf');
  if (hb) hb.style.width = d.hit_rate_pct.toFixed(0) + '%%';

  var tb = document.getElementById('p_clients');
  if (!tb) return;

  if (!d.clients || !d.clients.length) {
    tb.innerHTML = '<tr class="erow"><td colspan="8">No active connections</td></tr>';
    return;
  }

  /* Build index of live thread IDs */
  var ids = {};
  d.clients.forEach(function(cl) { ids[cl.thread_id] = 1; });

  /* Fade out rows for disconnected clients */
  Array.from(tb.querySelectorAll('tr[id^="row_"]')).forEach(function(r) {
    var tid = r.id.slice(4);
    if (!ids[tid] && r.style.opacity !== '.4') {
      r.style.opacity = '.4';
      setTimeout(function() { if (r.parentNode) r.parentNode.removeChild(r); }, 800);
    }
  });

  /* Add rows for new clients */
  d.clients.forEach(function(cl) {
    if (document.getElementById('row_' + cl.thread_id)) return;
    var tr = document.createElement('tr');
    tr.className = 'nw';
    tr.id = 'row_' + cl.thread_id;
    tr.innerHTML =
      '<td class="bold">' + _esc(cl.user) + '</td>'
      + '<td class="mono">' + _esc(cl.ip) + '</td>'
      + '<td class="mono"><span class="badge bbl">' + _esc(cl.caid) + '</span></td>'
      + '<td class="mono">' + _esc(cl.sid) + '</td>'
      + '<td>' + _esc(cl.channel || '&mdash;') + '</td>'
      + '<td class="mono tm">' + _esc(cl.connected) + '</td>'
      + '<td class="mono tm">' + _esc(cl.idle) + '</td>'
      + '<td><button class="kb" onclick="_kill(' + cl.thread_id
        + ',\'' + _esc(cl.user) + '\')" title="Disconnect">&#x2715;</button></td>';
    tb.appendChild(tr);
  });
}

/* 6. Kill-client handler */
function _kill(tid, user) {
  if (!confirm('Disconnect ' + user + '?')) return;
  fetch('/status?kill=' + tid + '&user=' + encodeURIComponent(user));
  var r = document.getElementById('row_' + tid);
  if (r) {
    r.style.opacity = '.4';
    setTimeout(function() { if (r.parentNode) r.parentNode.removeChild(r); }, 800);
  }
}

/* 7. Poll loop */
function _poll() {
  if (_busy) return;
  _busy = true;
  fetch('/api/status', { cache: 'no-store' })
    .then(function(r) {
      if (r.status === 401) { window.location.href = '/login'; return null; }
      if (!r.ok) return null;
      return r.json();
    })
    .then(function(d) {
      _busy = false;
      if (!d) return;
      _upd_topbar(d);
      if (document.getElementById('p_clients')) _upd_status(d);
    })
    .catch(function() { _busy = false; });
}

document.addEventListener('DOMContentLoaded', function() {
  _poll();
  _pit = setInterval(_poll, _pm);
});
