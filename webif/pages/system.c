#define MODULE_LOG_PREFIX "webif"
#include "../service/service.h"
#include "../../src/core/constants.h"
#include "../../src/core/utils.h"
#include "../../src/log/log.h"
#include "../internal/proto.h"

static int failban_clear(const char *clearip)
{
	if (clearip && clearip[0]) return webif_ban_clear(clearip);
	return webif_ban_clear_all();
}

void handle_api_failban_clear(int fd, const char *qs)
{
	char clearip[MAXIPLEN] = "";
	get_param(qs, "ip", clearip, sizeof(clearip));

	if (!clearip[0]) {
		send_json_error(fd, 400, "Bad Request", "missing ip");
		return;
	}

	int n = failban_clear(clearip);
	tcmg_log("ban cleared for ip=%s", clearip);

	char msg[128];
	snprintf(msg, sizeof(msg), n ? "Unbanned %s" : "%s was not banned", clearip);
	send_json_ok(fd, msg);
}

void handle_api_failban_clearall(int fd)
{
	int n = failban_clear(NULL);
	tcmg_log("%s", "all bans cleared");

	char msg[64];
	snprintf(msg, sizeof(msg), "Cleared %d ban(s)", n);
	send_json_ok(fd, msg);
}

void send_page_failban(int fd, const char *qs)
{
	char action[32], clearip[MAXIPLEN];
	get_param(qs, "action", action,  sizeof(action));
	get_param(qs, "ip",     clearip, sizeof(clearip));

	if (strcmp(action, "clear") == 0 && clearip[0]) {
		failban_clear(clearip);
		tcmg_log("ban cleared for ip=%s", clearip);
	} else if (strcmp(action, "clearall") == 0) {
		failban_clear(NULL);
		tcmg_log("%s", "all bans cleared");
	}

	PAGE_INIT(8192)

	pos = emit_header(&buf, &bsz, pos, "Fail-Ban", "failban");

	S_WEBIF_BAN_STATS bstats;
	webif_ban_snapshot(NULL, 0, &bstats);
	int ban_cap = bstats.active_bans > 0 ? bstats.active_bans : 1;
	S_WEBIF_BAN_VIEW *bans = calloc((size_t)ban_cap, sizeof(*bans));
	int nbans = bans ? webif_ban_snapshot(bans, (size_t)ban_cap, &bstats) : 0;
	int total_bans = bstats.active_bans;
	int total_fails = bstats.total_fails;
	int max_fails = bstats.max_fails;
	int ban_secs = bstats.ban_secs;
	time_t now = time(NULL);

	pos = buf_printf(&buf, &bsz, pos,
		"<section class='utoolbar center' aria-label='Fail-Ban toolbar'>"
		"<div class='tgrp'>"
		"<button type='button' class='tool danger' id='fbClearAll'%s>"
		ICON("i-trash") "Clear All</button>"
		"</div></section>",
		total_bans > 0 ? "" : " hidden");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='sbar' id='fbStats'>"
		"<div class='sbar-item'><div class='sbl'>Active Bans</div>"
		"  <div class='sbv%s'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Total Fails</div>"
		"  <div class='sbv%s sm'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Max Fails</div>"
		"  <div class='sbv sm'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Ban Duration</div>"
		"  <div class='sbv sm'>%ds</div></div>"
		"</div>",
		total_bans > 0 ? " tr" : " tg", total_bans,
		total_fails > 0 ? " to" : "", total_fails,
		max_fails,
		ban_secs);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='tw cm auto'><table>"
		"<thead><tr>"
		"<th>IP Address</th><th>Fail Count</th>"
		"<th>Expires At</th><th>Remaining</th><th>Action</th>"
		"</tr></thead><tbody id='fbBody'>");

	int shown = 0;
	for (int bi = 0; bi < nbans; bi++) {
		S_WEBIF_BAN_VIEW *b = &bans[bi];
		if (b->until <= now) continue;
		char exp[32];
		struct tm tm_s;
		localtime_r(&b->until, &tm_s);
		strftime(exp, sizeof(exp), "%H:%M:%S", &tm_s);
		long secs_left = (long)(b->until - now);
		char esc_ip[128];
		html_escape(b->ip, esc_ip, sizeof(esc_ip));
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='fbrow' data-ip='%s'>"
			"<td class='mono bold'>%s</td>"
			"<td><span class='badge bban'>%d fails</span></td>"
			"<td class='mono tm'>%s</td>"
			"<td class='mono to fbcd' data-left='%ld'>%lds</td>"
			"<td><button type='button' class='tool sm' data-a='unban'>"
			ICON("i-unban") "Unban</button></td>"
			"</tr>",
			esc_ip, esc_ip, b->fails, exp, secs_left, secs_left);
		shown++;
	}
	free(bans);

	if (!shown)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='erow'><td colspan='5'>"
			"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='var(--gr)' stroke-width='1.8'"
			" style='vertical-align:-3px;margin-right:6px'>"
			"<path d='M22 11.08V12a10 10 0 1 1-5.93-9.14'/>"
			"<polyline points='22 4 12 14.01 9 11.01'/>"
			"</svg><span class='tg'>All clear</span> &mdash; no active bans"
			"</td></tr>");

	pos = buf_printf(&buf, &bsz, pos,
		"</tbody></table></div>"
		"<script>(function(){"
		"  var body=document.getElementById('fbBody');"
		"  var busy=false;"

		"  var tbox=null;"
		"  function toast(msg,kind){"
		"    if(!tbox){"
		"      tbox=document.createElement('div');"
		"      tbox.className='toasts';"
		"      tbox.setAttribute('role','status');"
		"      tbox.setAttribute('aria-live','polite');"
		"      document.body.appendChild(tbox);"
		"    }"
		"    var t=document.createElement('div');"
		"    t.className='toast '+(kind||'');"
		"    t.textContent=msg;"
		"    tbox.appendChild(t);"
		"    setTimeout(function(){"
		"      t.style.transition='opacity .25s';t.style.opacity='0';"
		"      setTimeout(function(){if(t.parentNode)t.parentNode.removeChild(t);},260);"
		"    },kind==='err'?5000:2600);"
		"  }"

		"  function tickCountdown(){"
		"    var rows=body.querySelectorAll('.fbcd');"
		"    for(var i=0;i<rows.length;i++){"
		"      var el=rows[i], v=parseInt(el.getAttribute('data-left'),10);"
		"      if(isNaN(v)) v=0;"
		"      if(v>0) v--;"
		"      el.setAttribute('data-left',v);"
		"      el.textContent=v+'s';"
		"      if(v===0&&el.parentNode) el.parentNode.classList.add('dim');"
		"    }"
		"    setTimeout(tickCountdown,1000);"
		"  }"
		"  setTimeout(tickCountdown,1000);"

		"  function softRefresh(){"
		"    if(busy) return Promise.resolve();"
		"    busy=true;"
		"    return tcmg_html('/failban')"
		"      .then(function(r){"
		"        if(r.status===401){location.href='/login';return null;}"
		"        return r.text();"
		"      })"
		"      .then(function(html){"
		"        busy=false;"
		"        if(!html) return;"
		"        var doc=new DOMParser().parseFromString(html,'text/html');"
		"        var nb=doc.getElementById('fbBody');"
		"        if(nb) body.innerHTML=nb.innerHTML;"
		"        var ns=doc.getElementById('fbStats'), cs=document.getElementById('fbStats');"
		"        if(ns&&cs) cs.innerHTML=ns.innerHTML;"
		"        var ca=document.getElementById('fbClearAll');"
		"        if(ca) ca.hidden=!body.querySelector('.fbrow');"
		"      })"
		"      .catch(function(){busy=false;});"
		"  }"
		"  window.tcmgSoftRefresh=softRefresh;"

		"  function post(url,okmsg){"
		"    return tcmg_api(url,{method:'POST'})"
		"      .then(function(d){"
		"        toast(d&&d.msg?d.msg:okmsg,(d&&d.ok===false)?'err':'ok');"
		"        return softRefresh();"
		"      })"
		"      .catch(function(){"
		"        toast('Request failed','err');"
		"      });"
		"  }"

		"  var ca=document.getElementById('fbClearAll');"
		"  if(ca) ca.addEventListener('click',function(){"
		"    if(!confirm('Clear all active bans?')) return;"
		"    post('/api/failban/clearall','Cleared');"
		"  });"

		"  body.addEventListener('click',function(e){"
		"    var btn=e.target.closest?e.target.closest('[data-a=\"unban\"]'):null;"
		"    if(!btn) return;"
		"    var tr=btn.closest('tr');"
		"    if(!tr) return;"
		"    var ip=tr.getAttribute('data-ip');"
		"    post('/api/failban/clear?ip='+encodeURIComponent(ip),'Unbanned '+ip);"
		"  });"
		"})();</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}
