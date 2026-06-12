#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

void send_page_livelog(int fd)
{
	PAGE_INIT(32768)

	pos = emit_header(&buf, &bsz, pos, "Live Log", "livelog");

	char masks_js[256];
	int  ma = 0;

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card' style='margin-bottom:12px'>"
		"<div class='ll-ch'>"
		"  <span class='ct'>"
		"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
		"      <path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
		"      <polyline points='14 2 14 8 20 8'/>"
		"    </svg>Live Log"
		"  </span>"
		"  <div class='ll-hdr-right'>");

	pos = buf_printf(&buf, &bsz, pos,
		"    <div class='ll-dbrow'>"
		"      <span class='ll-label'>Debug</span>");

	for (int i = 0; i < MAX_DEBUG_LEVELS; i++) {
		uint16_t m  = g_dblevel_names[i].mask;
		int      on = !!(g_dblevel & m);
		pos = buf_printf(&buf, &bsz, pos,
			"<a id='db%u' href='#' class='dt%s' onclick='toggleDbg(%u);return false;'"
			" title='0x%04X'>%s</a>",
			m, on ? " on" : "", m, m, g_dblevel_names[i].name);
		ma += snprintf(masks_js + ma, (int)sizeof(masks_js) - ma, "%s%u", i ? "," : "", m);
	}
	pos = buf_printf(&buf, &bsz, pos,
		"      <a id='dbALL' href='#' class='dt%s' onclick='toggleAll();return false;'>ALL</a>"
		"      <div class='ll-vsep'></div>"
		"      <div class='ll-meta'>"
		"        <span class='ll-dot'></span>"
		"        mask:<span id='dbmask' class='ll-mask'>0x%04X</span>"
		"        &nbsp;|&nbsp;"
		"        <span id='linecnt' class='ll-mask'>0</span> lines"
		"      </div>"
		"    </div>"
		"  </div>"
		"</div>"
		"<div class='cb' style='padding:10px 18px'>",
		(g_dblevel == D_ALL) ? " on" : "",
		g_dblevel);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='ll-toolbar2'>"
		"  <input class='ls' id='filter' placeholder='Filter&#8230;' oninput='applyFilter()'"
		"   title='Regex, e.g.: hit|miss'>"
		"  <div class='ll-sep'></div>"
		"  <button class='btn bg sm' onclick='clearLog()'>" ICO_TRASH "&nbsp;Clear</button>"
		"  <a id='savelog' download='tcmg.log'>"
		"    <button class='btn bg sm' onclick='prepSave()'>&#128190;&nbsp;Save</button>"
		"  </a>"
		"  <div class='ll-sep'></div>"
		"  <label class='ll-chk'><input type='checkbox' id='asc' checked>&nbsp;Scroll</label>"
		"  <label class='ll-chk'><input type='checkbox' id='paused'>&nbsp;Pause</label>"
		"  <div class='ll-sep'></div>"
		"  <span class='ll-label'>Lines</span>"
		"  <select class='lsel' id='maxlines'>"
		"    <option value='200' selected>200</option>"
		"    <option value='500'>500</option>"
		"    <option value='1000'>1000</option>"
		"    <option value='2000'>2000</option>"
		"  </select>"
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
		"var lastid=%d,curmask=%u,hovered=0,_pollBusy=0;"
		"var masks=[%s],filterRe=null,visCount=0;",
		(ring_now > 200) ? ring_now - 200 : 0,
		(unsigned)g_dblevel,
		masks_js);

	pos = buf_printf(&buf, &bsz, pos,
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
		"    .then(function(r){return r.ok?r.json():null;})"
		"    .then(function(d){"
		"      if(!d)return;"
		"      if(typeof d.next==='number')lastid=d.next;"
		"      if(d.lines&&d.lines.length)appendLines(d.lines);"
		"    }).catch(function(){});"
		"}");

	pos = buf_printf(&buf, &bsz, pos,
		"function setLineCnt(v){"
		"  visCount=v;"
		"  var lc=document.getElementById('linecnt');if(lc)lc.textContent=v;"
		"}"
		"function applyFilter(){"
		"  var s=document.getElementById('filter').value;"
		"  try{filterRe=s?new RegExp(s,'i'):null;}catch(e){filterRe=null;}"
		"  var spans=document.getElementById('lp').children,vis=0;"
		"  for(var i=0;i<spans.length;i++){"
		"    var show=!filterRe||filterRe.test(spans[i].getAttribute('data-r')||'');"
		"    spans[i].style.display=show?'':'none';"
		"    if(show)vis++;"
		"  }"
		"  setLineCnt(vis);"
		"}"
		"function clearLog(){"
		"  var pre=document.getElementById('lp');"
		"  pre.textContent='';"
		"  setLineCnt(0);"
		"  fetch('/logpoll?since=999999999&debug='+curmask,{credentials:'same-origin'})"
		"    .then(function(r){return r.ok?r.json():null;})"
		"    .then(function(d){if(d&&typeof d.next==='number')lastid=d.next;})"
		"    .catch(function(){});"
		"}"
		"function prepSave(){"
		"  var spans=document.getElementById('lp').children,txt='';"
		"  for(var i=0;i<spans.length;i++){"
		"    if(spans[i].style.display==='none')continue;"
		"    txt+=(spans[i].getAttribute('data-r')||'')+'\\n';"
		"  }"
		"  var a=document.getElementById('savelog');"
		"  if(a)a.href=URL.createObjectURL(new Blob([txt],{type:'text/plain'}));"
		"}");

	pos = buf_printf(&buf, &bsz, pos,
		"var LC_HIT   =/found \\(|: found/i;"
		"var LC_MISS  =/not found|: miss/i;"
		"var LC_ERR   =/fatal|forced exit|failed:|\\berror\\b/i;"
		"var LC_WARN  =/\\bwarn\\b|rejected|repeated/i;"
		"var LC_DOWN  =/shutting down|shutdown/i;"
		"var LC_START =/started|listening|>> tcmg <</i;"
		"var LC_RELOAD=/\\breload(ed)?\\b|re-opened/i;"
		"var LC_BAN   =/\\bban\\b|login failed/i;"
		"var LC_WEBIF =/\\bwebif\\b|\\bhttp\\b|\\blogin\\b/i;"
		"var LC_NET   =/\\(net\\)|\\bconnect\\b|disconnect/i;"
		"var LC_EMU   =/\\(emu\\)|\\bemu:/i;"
		"var LC_DBG   =/0x[0-9a-f]+|\\bdebug\\b/i;"
		"function colorLine(s){"
		"  if(LC_HIT.test(s))    return '#4ade80';"
		"  if(LC_MISS.test(s))   return '#f87171';"
		"  if(LC_ERR.test(s))    return '#ff6b6b';"
		"  if(LC_WARN.test(s))   return '#fbbf24';"
		"  if(LC_DOWN.test(s))   return '#c084fc';"
		"  if(LC_START.test(s))  return '#22d3ee';"
		"  if(LC_RELOAD.test(s)) return '#60a5fa';"
		"  if(LC_BAN.test(s))    return '#fb923c';"
		"  if(LC_WEBIF.test(s))  return '#818cf8';"
		"  if(LC_NET.test(s))    return '#a78bfa';"
		"  if(LC_EMU.test(s))    return '#67e8f9';"
		"  if(LC_DBG.test(s))    return '#64748b';"
		"  return null;"
		"}"
		"function appendLines(entries){"
		"  var pre=document.getElementById('lp');if(!pre)return;"
		"  var maxl=parseInt((document.getElementById('maxlines')||{value:'200'}).value)||200;"
		"  var frag=document.createDocumentFragment();"
		"  var added=0,addedVis=0;"
		"  for(var i=0;i<entries.length;i++){"
		"    var e=entries[i];"
		"    var line=typeof e==='string'?e:((e.line||'')+'');"
		"    var col=colorLine(line);"
		"    var span=document.createElement('span');"
		"    if(col)span.style.color=col;"
		"    span.setAttribute('data-r',line);"
		"    var show=!filterRe||filterRe.test(line);"
		"    if(!show)span.style.display='none';"
		"    span.appendChild(document.createTextNode(line));"
		"    frag.appendChild(span);"
		"    added++;"
		"    if(show)addedVis++;"
		"  }"
		"  pre.appendChild(frag);"
		"  var overflow=pre.children.length-maxl;"
		"  if(overflow>0){"
		"    var removed=0,removedVis=0,ch=pre.firstChild;"
		"    while(ch&&removed<overflow){"
		"      var nx=ch.nextSibling;"
		"      if(ch.style&&ch.style.display!=='none')removedVis++;"
		"      pre.removeChild(ch);"
		"      ch=nx;removed++;"
		"    }"
		"    setLineCnt(visCount+addedVis-removedVis);"
		"  } else {"
		"    setLineCnt(visCount+addedVis);"
		"  }"
		"  var w=document.getElementById('lw');"
		"  if(w&&!hovered&&addedVis>0"
		"     &&document.getElementById('asc')&&document.getElementById('asc').checked)"
		"    w.scrollTop=w.scrollHeight;"
		"}");

	pos = buf_printf(&buf, &bsz, pos,
		"function poll(){"
		"  if(_pollBusy)return;"
		"  if(document.getElementById('paused')&&document.getElementById('paused').checked)return;"
		"  _pollBusy=1;"
		"  fetch('/logpoll?since='+lastid+'&debug='+curmask,{credentials:'same-origin'})"
		"    .then(function(r){"
		"      if(r.status===401||r.status===302){window.location.href='/login';return null;}"
		"      return r.ok?r.json():null;"
		"    })"
		"    .then(function(d){"
		"      _pollBusy=0;"
		"      if(!d)return;"
		"      if(typeof d.debug==='number'&&d.debug!==curmask){curmask=d.debug;updateDbgUI();}"
		"      if(typeof d.next==='number')lastid=d.next;"
		"      if(d.lines&&d.lines.length)appendLines(d.lines);"
		"    }).catch(function(){_pollBusy=0;});"
		"}"
		"updateDbgUI();"
		"setInterval(poll,1000);"
		"poll();"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
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
				uint16_t old = g_dblevel;
				g_dblevel = nv;
				tcmg_log_dbg(D_HTTP, "debug level changed: 0x%04X -> 0x%04X",
				             (unsigned)old, (unsigned)g_dblevel);
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
		char esc_line[8192];
		char esc_usr[512];
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

