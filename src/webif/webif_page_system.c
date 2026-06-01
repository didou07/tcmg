#define MODULE_LOG_PREFIX "webif"
#include "tcmg.h"
#include "webif_internal.h"

/* webif_page_system.c — Fail-Ban, Config, Live Log, Power pages */

void send_page_failban(int fd, const char *qs)
{
	char action[32], clearip[MAXIPLEN];
	get_param(qs, "action", action,  sizeof(action));
	get_param(qs, "ip",     clearip, sizeof(clearip));

	/* Mutate under lock before rendering */
	if (strcmp(action, "clear") == 0 && clearip[0]) {
		pthread_mutex_lock(&g_cfg.ban_lock);
		for (S_BAN_ENTRY *b = g_cfg.bans; b; b = b->next)
			if (strcmp(b->ip, clearip) == 0) b->until = 0;
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("cleared ban for %s", clearip);
	} else if (strcmp(action, "clearall") == 0) {
		pthread_mutex_lock(&g_cfg.ban_lock);
		for (S_BAN_ENTRY *b = g_cfg.bans; b; b = b->next) b->until = 0;
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("%s", "cleared all bans");
	}

	int   bsz = 16384, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Fail-Ban", "failban");

	int    total_bans = 0, total_fails = 0;
	time_t now        = time(NULL);
	pthread_mutex_lock(&g_cfg.ban_lock);
	for (S_BAN_ENTRY *b = g_cfg.bans; b; b = b->next)
		if (b->until > now) { total_bans++; total_fails += b->fails; }
	pthread_mutex_unlock(&g_cfg.ban_lock);


	pos = buf_printf(&buf, &bsz, pos,
		"<div class='ph'>"
		"  <div class='ha'>");
	if (total_bans > 0)
		pos = buf_printf(&buf, &bsz, pos,
			"    <a href='/failban?action=clearall' class='btn bd_ sm'>"
			ICO_TRASH "&nbsp;Clear All</a>");
	pos = buf_printf(&buf, &bsz, pos,
		"    <button class='btn bg sm' onclick='location.reload()'>"
		ICO_RELOAD "&nbsp;Refresh</button>"
		"  </div>"
		"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='sbar'>"
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
		BAN_MAX_FAILS,
		BAN_SECS);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='tw'><table>"
		"<thead><tr>"
		"<th>IP Address</th><th>Fail Count</th>"
		"<th>Expires At</th><th>Remaining</th><th>Action</th>"
		"</tr></thead><tbody>");

	int shown = 0;
	pthread_mutex_lock(&g_cfg.ban_lock);
	for (S_BAN_ENTRY *b = g_cfg.bans; b; b = b->next) {
		if (b->until <= now) continue;
		char exp[32];
		struct tm tm_s;
		localtime_r(&b->until, &tm_s);
		strftime(exp, sizeof(exp), "%H:%M:%S", &tm_s);
		long secs_left = (long)(b->until - now);
		pos = buf_printf(&buf, &bsz, pos,
			"<tr>"
			"<td class='mono bold'>%s</td>"
			"<td><span class='badge bban'>%d fails</span></td>"
			"<td class='mono tm'>%s</td>"
			"<td class='mono to' id='cd_%s'>%lds</td>"
			"<td><a href='/failban?action=clear&ip=%s' class='btn bg sm'>"
			ICO_UNBAN "&nbsp;Unban</a></td>"
			"</tr>",
			b->ip, b->fails, exp, b->ip, secs_left, b->ip);
		shown++;
	}
	pthread_mutex_unlock(&g_cfg.ban_lock);

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
		"  var els=document.querySelectorAll('[id^=\"cd_\"]');"
		"  setInterval(function(){"
		"    els.forEach(function(el){"
		"      var v=parseInt(el.textContent);"
		"      el.textContent=(v>0?v-1:0)+'s';"
		"    });"
		"  },1000);"
		"})();</script>");

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

void send_page_config(int fd)
{
	int   bsz = 65536, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Config", "config");

	/* Read both files — html_escape done once each via file_read_escaped() */
	char cfgpath[CFGPATH_LEN], srvpath[CFGPATH_LEN];
	tcmg_build_path(cfgpath, sizeof(cfgpath), g_cfgdir, TCMG_CFG_FILE);
	tcmg_build_path(srvpath, sizeof(srvpath), g_cfgdir, TCMG_SRVID_FILE);

	int  cfg_trunc = 0, srv_trunc = 0;
	char *escaped = file_read_escaped(cfgpath, 16384, &cfg_trunc);
	char *srvesc  = file_read_escaped(srvpath, 32768, &srv_trunc);

	if (!escaped || !srvesc) {
		free(escaped); free(srvesc); free(buf);
		const char *e = "Out of memory";
		send_response(fd, 500, "Internal Error", "text/plain", e, (int)strlen(e));
		return;
	}

	pos = buf_printf(&buf, &bsz, pos,
		/* Tab bar */
		"<div style='display:flex;gap:0;margin-bottom:-1px;position:sticky;top:var(--tbh);z-index:10;justify-content:center;background:var(--bg);padding-top:8px'>"
		"  <button class='ctab act' id='tab_cfg' onclick=\"switchTab('cfg')\">tcmg.cfg</button>"
		"  <button class='ctab' id='tab_srv'     onclick=\"switchTab('srv')\">tcmg.srvid2</button>"
		"</div>");


	if (cfg_trunc)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='ib2' style='color:var(--or2);border-color:var(--ors)'>"
			ICO_WARN " Config exceeds 16 KB &mdash; truncated. Edit on disk.</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='pnl_cfg'>"
		"<form method='post' action='/config_save'>"
		"<div class='card'>"
		"<div class='et' style='position:sticky;top:calc(var(--tbh) + 42px);z-index:9;background:var(--s2)'>"
		"  <span class='ef'>" ICO_FILE "&nbsp;%s</span>"
		"</div>"
		"<div class='ew' id='cfgWarn' style='display:none'>"
		"  " ICO_WARN " Config saved &mdash; restart may be needed for some changes"
		"</div>"
		"<textarea class='ea' name='cfg' spellcheck='false' id='cfgArea' oninput='onEdit()'>%s</textarea>"
		"<div class='ef2'>"
		"  <button type='submit' class='btn bp sm' id='saveBtn'"
		"    onclick=\"document.getElementById('cfgWarn').style.display='flex'\">"
		"    " ICO_SAVE "&nbsp;Save &amp; Reload"
		"  </button>"
		"</div></div></form></div>",
		cfgpath, escaped);
	free(escaped);


	if (srv_trunc)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='ib2' style='color:var(--or2);border-color:var(--ors)'>"
			ICO_WARN " srvid2 exceeds 32 KB &mdash; truncated. Edit on disk.</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='pnl_srv' style='display:none'>"
		"<form method='post' action='/srvid2_save'>"
		"<div class='card'>"
		"<div class='et' style='position:sticky;top:calc(var(--tbh) + 42px);z-index:9;background:var(--s2)'>"
		"  <span class='ef'>" ICO_FILE "&nbsp;%s</span>"
		"</div>"
		"<textarea class='ea' name='srvid2' spellcheck='false' style='height:520px'>%s</textarea>"
		"<div class='ef2'>"
		"  <button type='submit' class='btn bp sm'>" ICO_SAVE "&nbsp;Save &amp; Reload</button>"
		"</div></div></form></div>",
		srvpath, srvesc);
	free(srvesc);

	pos = buf_printf(&buf, &bsz, pos,
		"<script>"
		"function switchTab(t){"
		"  document.getElementById('pnl_cfg').style.display=t==='cfg'?'':'none';"
		"  document.getElementById('pnl_srv').style.display=t==='srv'?'':'none';"
		"  document.getElementById('tab_cfg').className='ctab'+(t==='cfg'?' act':'');"
		"  document.getElementById('tab_srv').className='ctab'+(t==='srv'?' act':'');"
		"}"
		"var _dirty=false;"
		"function onEdit(){"
		"  if(!_dirty){_dirty=true;"
		"    document.getElementById('edSt').innerHTML="
		"'<span style=\"color:var(--or2)\">&#9679; Unsaved</span>';}"
		"}"
		"function reloadPage(){if(_dirty&&!confirm('Discard changes?'))return;location.reload();}"
		"var ta=document.getElementById('cfgArea');"
		"if(ta){"
		"  function updCursor(){"
		"    var t=ta.value.substr(0,ta.selectionStart);"
		"    var ln=t.split('\\n').length;"
		"    var col=t.split('\\n').pop().length+1;"
		"    document.getElementById('edCur').textContent='Ln '+ln+', Col '+col;"
		"  }"
		"  ta.addEventListener('keyup',updCursor);"
		"  ta.addEventListener('click',updCursor);"
		"}"
		"document.addEventListener('keydown',function(e){"
		"  if((e.ctrlKey||e.metaKey)&&e.key==='s'){"
		"    e.preventDefault();"
		"    if(!document.getElementById('pnl_cfg').style.display){"
		"      document.getElementById('cfgWarn').style.display='flex';"
		"      document.getElementById('cfgArea').closest('form').submit();"
		"    }"
		"  }"
		"});"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

void handle_config_save(int fd, const char *post_body)
{
	char newcfg[16384] = "";
	form_get(post_body, "cfg", newcfg, sizeof(newcfg));
	if (!newcfg[0]) {
		const char *e = "<html><body><h1>Empty config rejected</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}

	char cfgpath[CFGPATH_LEN], tmppath[CFGPATH_LEN + 4], bakpath[CFGPATH_LEN + 4];
	tcmg_build_path(cfgpath,  sizeof(cfgpath),  g_cfgdir, TCMG_CFG_FILE);
	tcmg_build_path(tmppath,  sizeof(tmppath),  g_cfgdir, TCMG_CFG_FILE ".tmp");
	tcmg_build_path(bakpath,  sizeof(bakpath),  g_cfgdir, TCMG_CFG_FILE ".bak");

	FILE *fp = fopen(tmppath, "w");
	if (!fp) {
		const char *e = "<html><body><h1>Cannot write temp file</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	fputs(newcfg, fp); fclose(fp);

	/* Validate before clobbering the live config */
	S_CONFIG parsed;
	memset(&parsed, 0, sizeof(parsed));
	pthread_rwlock_init(&parsed.acc_lock, NULL);
	pthread_mutex_init(&parsed.ban_lock, NULL);
	if (!cfg_load(tmppath, &parsed)) {
		remove(tmppath);
		cfg_accounts_free(&parsed);
		const char *e = "<html><body><h1>Config parse error &mdash; not saved</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}
	remove(tmppath);

	/* Backup current config */
	FILE *src = fopen(cfgpath, "r");
	if (src) {
		FILE *dst = fopen(bakpath, "w");
		if (dst) {
			char tmp[512]; int n;
			while ((n = (int)fread(tmp, 1, sizeof(tmp), src)) > 0)
				fwrite(tmp, 1, n, dst);
			fclose(dst);
		}
		fclose(src);
	}

	tcmg_strlcpy(parsed.config_file, cfgpath, CFGPATH_LEN);
	if (!cfg_save(&parsed)) {
		cfg_accounts_free(&parsed);
		const char *e = "<html><body><h1>Cannot write config</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	cfg_accounts_free(&parsed);
	tcmg_log("%s", "config saved via webif, reloading...");
	g_reload_cfg = 1;
	send_redirect(fd, "/config");
}

void send_page_livelog(int fd)
{
	int   bsz = 32768, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Live Log", "livelog");

	char masks_js[256];
	int  ma = 0;

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card' style='margin-bottom:12px'>"
		"<div class='ch'>"
		"  <span class='ct'>"
		"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
		"      <path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
		"      <polyline points='14 2 14 8 20 8'/>"
		"    </svg>Live Log"
		"  </span>"
		"  <div style='display:flex;align-items:center;gap:6px;font-size:11px;font-family:var(--mono);color:var(--t2)'>"
		"    mask:<span id='dbmask' style='color:var(--p)'>0x%04X</span>"
		"  </div>"
		"</div>"
		"<div class='cb' style='display:flex;flex-direction:column;gap:10px'>",
		g_dblevel);

	pos = buf_printf(&buf, &bsz, pos,
		"<div style='display:flex;flex-wrap:wrap;align-items:center;justify-content:center;gap:6px'>"
		"  <span style='font-size:10px;font-weight:700;color:var(--t2);text-transform:uppercase;letter-spacing:.1em'>Debug</span>");

	for (int i = 0; i < MAX_DEBUG_LEVELS; i++) {
		uint16_t m  = g_dblevel_names[i].mask;
		int      on = !!(g_dblevel & m);
		pos = buf_printf(&buf, &bsz, pos,
			"<a id='db%u' href='#' class='dt%s' onclick='toggleDbg(%u);return false;'"
			" title='mask 0x%04X'>%s</a>",
			m, on ? " on" : "", m, m, g_dblevel_names[i].name);
		ma += snprintf(masks_js + ma, (int)sizeof(masks_js) - ma, "%s%u", i ? "," : "", m);
	}

	pos = buf_printf(&buf, &bsz, pos,
		"  <a id='dbALL' href='#' class='dt%s' onclick='toggleAll();return false;'>ALL</a>"
		"</div>",
		(g_dblevel == D_ALL) ? " on" : "");

	pos = buf_printf(&buf, &bsz, pos,
		"<div style='display:flex;flex-wrap:wrap;align-items:center;justify-content:center;gap:8px'>"
		"  <button class='btn bg sm' onclick='clearLog()'>" ICO_TRASH "&nbsp;Clear</button>"
		"  <a id='savelog' download='tcmg.log'>"
		"    <button class='btn bg sm' onclick='prepSave()'>&#128190;&nbsp;Save</button>"
		"  </a>"
		"  <input class='ls' id='filter' placeholder='Filter (regex)...' oninput='applyFilter()'"
		"   title='Supports regex, e.g.: hit|miss' style='width:180px'>"
		"  <label class='flex gap8' style='font-size:12px;color:var(--t1)'>"
		"    <input type='checkbox' id='asc' checked>&nbsp;Auto-scroll"
		"  </label>"
		"  <label class='flex gap8' style='font-size:12px;color:var(--t1)'>"
		"    <input type='checkbox' id='paused'>&nbsp;Pause"
		"  </label>"
		"  <span style='display:flex;align-items:center;gap:5px;font-size:11px;color:var(--t2)'>"
		"    Lines:<select class='lsel' id='maxlines'>"
		"      <option value='200' selected>200</option>"
		"      <option value='500'>500</option>"
		"      <option value='1000'>1000</option>"
		"      <option value='2000'>2000</option>"
		"    </select>"
		"    <span id='linecnt' style='font-family:var(--mono)'>0</span> shown"
		"  </span>"
		"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div style='display:flex;gap:10px;flex-wrap:wrap;justify-content:center;"
		"font-size:11px;font-family:var(--mono);padding:4px 0'>"
		"  <span style='color:#4ade80'>&#9632; hit</span>"
		"  <span style='color:#f87171'>&#9632; miss</span>"
		"  <span style='color:#60a5fa'>&#9632; webif</span>"
		"  <span style='color:#fb923c'>&#9632; ban</span>"
		"  <span style='color:#c084fc'>&#9632; net</span>"
		"  <span style='color:#22d3ee'>&#9632; proto</span>"
		"  <span style='color:#fde68a'>&#9632; conf</span>"
		"  <span style='color:#f87171'>&#9632; error/warn</span>"
		"</div>"
		"</div>"
		"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='lw' onmouseenter='hovered=1' onmouseleave='hovered=0'>"
		"<pre id='lp'></pre>"
		"</div>");

	int32_t ring_now = log_ring_total();

	pos = buf_printf(&buf, &bsz, pos,
		"<script>"
		"var lastid=%d,curmask=%u,hovered=0;"
		"var masks=[%s],filterRe=null;",
		(ring_now > 200) ? ring_now - 200 : 0,
		(unsigned)g_dblevel,
		masks_js);

	pos = buf_printf(&buf, &bsz, pos,
		/* debug UI */
		"function updateDbgUI(){"
		"  var el=document.getElementById('dbmask');"
		"  if(el)el.textContent='0x'+curmask.toString(16).toUpperCase().padStart(4,'0');"
		"  masks.forEach(function(m){"
		"    var b=document.getElementById('db'+m);"
		"    if(b)b.className='dt'+(curmask&m?' on':'');"
		"  });"
		"  var a=document.getElementById('dbALL');"
		"  if(a)a.className='dt'+(curmask===65535?' on':'');"
		"}"
		"function toggleDbg(m){curmask^=m;updateDbgUI();sendDebug();return false;}"
		"function toggleAll(){curmask=(curmask===65535)?0:65535;updateDbgUI();sendDebug();return false;}"
		"function sendDebug(){"
		"  fetch('/logpoll?since='+lastid+'&debug='+curmask,{credentials:'same-origin'})"
		"    .then(function(r){if(!r.ok)return null;return r.json();})"
		"    .then(function(d){if(d&&d.next!==undefined)lastid=d.next;if(d&&d.lines&&d.lines.length)appendLines(d.lines);})"
		"    .catch(function(){});"
		"}"

		/* filter */
		"function applyFilter(){"
		"  var s=document.getElementById('filter').value;"
		"  try{filterRe=s?new RegExp(s,'i'):null;}catch(e){filterRe=null;}"
		"  var spans=document.getElementById('lp').children,vis=0;"
		"  for(var i=0;i<spans.length;i++){"
		"    var raw=spans[i].getAttribute('data-raw')||'';"
		"    var usr=spans[i].getAttribute('data-usr')||'';"
		"    var show=!filterRe||filterRe.test(raw)||filterRe.test(usr);"
		"    spans[i].style.display=show?'block':'none';"
		"    if(show)vis++;"
		"  }"
		"  var lc=document.getElementById('linecnt');if(lc)lc.textContent=vis;"
		"}"

		/* clear and save */
		"function clearLog(){"
		"  document.getElementById('lp').innerHTML='';"
		"  var lc=document.getElementById('linecnt');if(lc)lc.textContent='0';"
		"  fetch('/logpoll?since=999999999&debug='+curmask,{credentials:'same-origin'})"
		"    .then(function(r){if(!r.ok)return null;return r.json();})"
		"    .then(function(d){if(d&&d.next!==undefined)lastid=d.next;})"
		"    .catch(function(){});"
		"}"
		"function prepSave(){"
		"  var spans=document.getElementById('lp').children,txt='';"
		"  for(var i=0;i<spans.length;i++){"
		"    if(spans[i].style.display==='none')continue;"
		"    var usr=spans[i].getAttribute('data-usr')||'';"
		"    var raw=spans[i].getAttribute('data-raw')||'';"
		"    txt+=(usr?'['+usr+'] ':'')+raw+'\\n';"
		"  }"
		"  var blob=new Blob([txt],{type:'text/plain'});"
		"  var a=document.getElementById('savelog');"
		"  if(a)a.href=URL.createObjectURL(blob);"
		"}"

		/* color map */
		"var CM={"
		"  hit:'#4ade80',miss:'#f87171',webif:'#60a5fa',"
		"  ban:'#fb923c',net:'#c084fc',proto:'#22d3ee',"
		"  conf:'#fde68a',err:'#f87171',warn:'#fb923c'"
		"};"
		"function colorLine(l){"
		"  var s=l.toLowerCase();"
		"  if(s.indexOf('[hit]')>=0)  return CM.hit;"
		"  if(s.indexOf('[miss]')>=0) return CM.miss;"
		"  if(s.indexOf('(webif')>=0) return CM.webif;"
		"  if(s.indexOf('(ban')>=0)   return CM.ban;"
		"  if(s.indexOf('(net')>=0)   return CM.net;"
		"  if(s.indexOf('(proto')>=0) return CM.proto;"
		"  if(s.indexOf('(conf')>=0)  return CM.conf;"
		"  if(s.indexOf('error')>=0)  return CM.err;"
		"  if(s.indexOf('warn')>=0)   return CM.warn;"
		"  return null;"
		"}"

		/* line count */
		"function updateLineCnt(){"
		"  var spans=document.getElementById('lp').children,v=0;"
		"  for(var i=0;i<spans.length;i++)if(spans[i].style.display!=='none')v++;"
		"  var lc=document.getElementById('linecnt');if(lc)lc.textContent=v;"
		"}"

		/* append */
		"function appendLines(entries){"
		"  var pre=document.getElementById('lp');"
		"  if(!pre)return;"
		"  var maxl=parseInt((document.getElementById('maxlines')||{}).value)||200;"
		"  for(var i=0;i<entries.length;i++){"
		"    var e=entries[i];"
		"    var line=(typeof e==='string')?e:(e.line||'')+'';"
		"    var usr=(typeof e==='string')?'':(e.usr||'')+'';"
		"    var span=document.createElement('span');"
		"    span.style.cssText='display:block;white-space:pre;';"
		"    var col=colorLine(line);"
		"    if(col)span.style.color=col;"
		"    span.setAttribute('data-raw',line);"
		"    if(usr)span.setAttribute('data-usr',usr);"
		"    var show=!filterRe||filterRe.test(line)||(usr&&filterRe.test(usr));"
		"    span.style.display=show?'block':'none';"
		"    /* Align hex-only rows (no module tag) to match timestamped lines */"
		"    var isHexRow=/^\\d{4}\\/\\d{2}\\/\\d{2} \\d{2}:\\d{2}:\\d{2} [0-9A-F][0-9A-F]/.test(line);"
		"    if(isHexRow){"
		"      var ts=line.substring(0,19);"
		"      var hex=line.substring(19).trim();"
		"      var tsNode=document.createTextNode(ts+' ');"
		"      var pad=document.createElement('span');"
		"      pad.style.cssText='display:inline-block;width:11ch;';"
		"      var hexNode=document.createTextNode(hex);"
		"      span.appendChild(tsNode);"
		"      span.appendChild(pad);"
		"      span.appendChild(hexNode);"
		"    } else {"
		"      span.appendChild(document.createTextNode(line));"
		"    }"
		"    pre.appendChild(span);"
		"  }"
		"  while(pre.children.length>maxl)pre.removeChild(pre.firstChild);"
		"  updateLineCnt();"
		"  var w=document.getElementById('lw');"
		"  if(w&&!hovered&&document.getElementById('asc')&&document.getElementById('asc').checked)"
		"    w.scrollTop=w.scrollHeight;"
		"}"

		/* poll loop */
		"function poll(){"
		"  if(document.getElementById('paused')&&document.getElementById('paused').checked)return;"
		"  fetch('/logpoll?since='+lastid+'&debug='+curmask,{credentials:'same-origin'})"
		"    .then(function(r){"
		"      if(r.status===401||r.status===302){window.location.href='/login';return null;}"
		"      if(!r.ok)return null;"
		"      return r.json();"
		"    })"
		"    .then(function(d){"
		"      if(!d)return;"
		"      if(typeof d.debug==='number'&&d.debug!==curmask){curmask=d.debug;updateDbgUI();}"
		"      if(typeof d.next==='number')lastid=d.next;"
		"      if(d.lines&&d.lines.length)appendLines(d.lines);"
		"    }).catch(function(){});"
		"}"
		"updateDbgUI();"
		"setInterval(poll,1000);"
		"poll();"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

void send_logpoll(int fd, const char *qs)
{
	char dbg_s[16]   = "";
	char since_s[32] = "";
	get_param(qs, "debug", dbg_s,   sizeof(dbg_s));
	get_param(qs, "since", since_s, sizeof(since_s));

	if (dbg_s[0]) {
		char    *end;
		long     v = strtol(dbg_s, &end, 0);
		if (*end == '\0' && v >= 0 && v <= 0xFFFF) {
			uint16_t nv = (uint16_t)v;
			if (nv != g_dblevel) {
				g_dblevel = nv;
				tcmg_log_dbg(D_WEBIF, "livelog debug_level -> %u", (unsigned)g_dblevel);
			}
		}
	}

	int32_t from_id = 0;
	if (since_s[0]) {
		long v = strtol(since_s, NULL, 10);
		if (v > 0) from_id = (int32_t)v;
	}

	char    *lines[WEB_MAX_LINES_POLL];
	char    *users[WEB_MAX_LINES_POLL];
	int32_t  next_id = 0;
	int32_t  count   = log_ring_since(from_id, lines, users,
	                                   WEB_MAX_LINES_POLL, &next_id);

	int   bsz = (count * 2800) + 512;
	char *buf = (char *)malloc((size_t)(bsz < 512 ? 512 : bsz));
	if (!buf) {
		for (int i = 0; i < count; i++) { free(lines[i]); free(users[i]); }
		return;
	}

	int pos = 0;
	pos = buf_printf(&buf, &bsz, pos,
		"{\"debug\":%u,\"next\":%d,\"lines\":[",
		(unsigned)g_dblevel, next_id);

	for (int i = 0; i < count; i++) {
		char esc_line[2048];
		char esc_usr[128];
		json_escape(lines[i],              esc_line, sizeof(esc_line));
		json_escape(users[i][0] ? users[i] : "", esc_usr,  sizeof(esc_usr));
		free(lines[i]);
		free(users[i]);
		pos = buf_printf(&buf, &bsz, pos,
			"%s{\"id\":%d,\"usr\":\"%s\",\"line\":\"%s\"}",
			i ? "," : "",
			from_id + i,
			esc_usr,
			esc_line);
	}
	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

void send_page_power(int fd, const char *qs)
{
	char action[16], confirm[8];
	get_param(qs, "action",  action,  sizeof(action));
	get_param(qs, "confirm", confirm, sizeof(confirm));

	int   bsz = 12288, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Power", "power");


	if (strcmp(confirm, "yes") == 0 && action[0]) {
		int is_restart = (strcmp(action, "restart") == 0);
		tcmg_log("%s requested via webif", is_restart ? "restart" : "shutdown");
		if (is_restart) g_restart = 1;
		g_running = 0;

		pos = buf_printf(&buf, &bsz, pos,
			"<div class='pg-center'>"
			"<div class='done-card'>"
			"<div class='%s' style='margin:0 auto 16px'>"
			"  <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"    %s"
			"  </svg>"
			"</div>"
			"<h2>%s</h2>"
			"<p style='margin-top:8px'>%s</p>"
			"%s"
			"</div></div>",
			is_restart ? "dico info" : "dico danger",
			is_restart
				? "<polyline points='23 4 23 10 17 10'/><path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
				: "<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>",
			is_restart ? "Restarting&hellip;" : "Server Stopped",
			is_restart
				? "Config will be reloaded. Redirecting when back online&hellip;"
				: "TCMG has been shut down. All connections were dropped.",
			is_restart
				? "<div style='display:flex;justify-content:center;margin-top:14px'>"
				  "<div class='spill'><div class='pulse sm'></div>&nbsp;Waiting for server&hellip;</div>"
				  "</div>"
				  "<script>setTimeout(function(){"
				  "  var t=setInterval(function(){"
				  "    fetch('/api/status',{cache:'no-store'})"
				  "      .then(function(){clearInterval(t);location.href='/status';})"
				  "      .catch(function(){});"
				  "  },1500);"
				  "},3500);</script>"
				: "");
	}

	else if (action[0]) {
		int is_restart = (strcmp(action, "restart") == 0);
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='pg-center'>"
			"<div class='dlg'>"
			"<div class='%s' style='margin:0 auto 18px'>"
			"  <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"    %s"
			"  </svg>"
			"</div>"
			"<h2>%s Server?</h2>"
			"<p>%s</p>"
			"<div class='da'>"
			"  <a href='/power?action=%s&confirm=yes' class='%s'>Confirm %s</a>"
			"  <a href='/power' class='btn bg'>Cancel</a>"
			"</div>"
			"</div></div>",
			is_restart ? "dico info" : "dico danger",
			is_restart
				? "<polyline points='23 4 23 10 17 10'/><path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
				: "<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>",
			is_restart ? "Restart" : "Shutdown",
			is_restart
				? "Active connections will be dropped and configuration reloaded on restart."
				: "All active connections will be dropped immediately. The process will <strong>not</strong> restart.",
			is_restart ? "restart" : "shutdown",
			is_restart ? "btn bp" : "btn bd_",
			is_restart ? "Restart" : "Shutdown");
	}

	else {
		pos = buf_printf(&buf, &bsz, pos,
			"<div style='display:flex;gap:16px;flex-wrap:wrap;justify-content:center;max-width:600px;margin:0 auto'>"

			/* Restart card */
			"<div class='card' style='flex:1;min-width:220px;text-align:center;padding:28px 20px'>"
			"  <div class='dico info' style='margin:0 auto 16px'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"      <polyline points='23 4 23 10 17 10'/>"
			"      <path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
			"    </svg>"
			"  </div>"
			"  <h3 style='font-size:16px;font-weight:700;margin-bottom:8px'>Restart</h3>"
			"  <p style='font-size:12px;color:var(--t1);margin-bottom:18px;line-height:1.6'>"
			"    Drops connections and reloads configuration."
			"  </p>"
			"  <a href='/power?action=restart' class='btn bp' style='width:100%%;justify-content:center'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8' width='14' height='14'>"
			"      <polyline points='23 4 23 10 17 10'/>"
			"      <path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
			"    </svg>"
			"    Restart Server"
			"  </a>"
			"</div>"

			/* Shutdown card */
			"<div class='card' style='flex:1;min-width:220px;text-align:center;padding:28px 20px'>"
			"  <div class='dico danger' style='margin:0 auto 16px'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"      <path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"    </svg>"
			"  </div>"
			"  <h3 style='font-size:16px;font-weight:700;margin-bottom:8px'>Shutdown</h3>"
			"  <p style='font-size:12px;color:var(--t1);margin-bottom:18px;line-height:1.6'>"
			"    Stops all connections. Process will not restart."
			"  </p>"
			"  <a href='/power?action=shutdown' class='btn bd_' style='width:100%%;justify-content:center'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8' width='14' height='14'>"
			"      <path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"    </svg>"
			"    Shutdown Server"
			"  </a>"
			"</div>"

			"</div>");
	}

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* Keep old endpoints as redirects for backward compat */
void send_page_shutdown(int fd, const char *qs) { (void)qs; send_redirect(fd, "/power?action=shutdown"); }
void send_page_restart (int fd, const char *qs) { (void)qs; send_redirect(fd, "/power?action=restart"); }
