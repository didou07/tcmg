#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

void send_page_config(int fd)
{
	PAGE_INIT(32768)

	pos = emit_header(&buf, &bsz, pos, "Config", "config");

	char key_hex[29];
	for (int i = 0; i < 14; i++)
		snprintf(key_hex + i * 2, 3, "%02X", g_cfg.newcamd_key[i]);
	key_hex[28] = '\0';

	char esc_nm_bind[256], esc_logfile[512], esc_wi_bind[256], esc_wi_user[256], esc_wi_pass[256];
	html_escape(g_cfg.newcamd_bindaddr, esc_nm_bind,  sizeof(esc_nm_bind));
	html_escape(g_cfg.logfile,          esc_logfile,  sizeof(esc_logfile));
	html_escape(g_cfg.webif_bindaddr,   esc_wi_bind,  sizeof(esc_wi_bind));
	html_escape(g_cfg.webif_user,       esc_wi_user,  sizeof(esc_wi_user));
	html_escape(g_cfg.webif_pass,       esc_wi_pass,  sizeof(esc_wi_pass));

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='cfgMsg' style='display:none;margin-bottom:14px'></div>"
		"<div style='max-width:640px;margin:0 auto'>"

		"<div class='card' style='margin-bottom:14px'>"
		"  <div class='ct' style='margin-bottom:14px'>&#9656;&nbsp;Newcamd / CCcam</div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>NEWCAMD PORT</label>"
		"    <input class='fi mono' id='cf_newcamd_port' type='number' min='1' max='65535'"
		"     style='flex:1' value='%d'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>NEWCAMD BINDADDR</label>"
		"    <input class='fi mono' id='cf_newcamd_bindaddr' placeholder='0.0.0.0'"
		"     style='flex:1' value='%s'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>NEWCAMD KEY</label>"
		"    <input class='fi mono' id='cf_newcamd_key' maxlength='28' spellcheck='false'"
		"     placeholder='28 hex chars' style='flex:1' value='%s'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>CCCAM PORT</label>"
		"    <input class='fi mono' id='cf_cccam_port' type='number' min='0' max='65535'"
		"     placeholder='0 = disabled' style='flex:1' value='%d'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>SOCK TIMEOUT (s)</label>"
		"    <input class='fi mono' id='cf_sock_timeout' type='number' min='5' max='600'"
		"     style='flex:1' value='%d'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:12px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>LOG FILE</label>"
		"    <input class='fi mono' id='cf_logfile' placeholder='leave empty to disable'"
		"     style='flex:1' value='%s'></div>"

		"  <div style='display:flex;gap:24px;flex-wrap:wrap;padding:10px 12px;"
		"background:var(--s1);border-radius:6px;border:1px solid var(--bd)'>"
		"    <label style='display:flex;align-items:center;gap:8px;cursor:pointer;"
		"font-size:12px;font-weight:700;color:var(--t2);text-transform:uppercase;letter-spacing:.08em'>"
		"      <input type='checkbox' id='cf_keepalive'%s> Keepalive</label>"
		"    <label style='display:flex;align-items:center;gap:8px;cursor:pointer;"
		"font-size:12px;font-weight:700;color:var(--t2);text-transform:uppercase;letter-spacing:.08em'>"
		"      <input type='checkbox' id='cf_mgclient'%s> MGcamd Mode</label>"
		"    <label style='display:flex;align-items:center;gap:8px;cursor:pointer;"
		"font-size:12px;font-weight:700;color:var(--t2);text-transform:uppercase;letter-spacing:.08em'>"
		"      <input type='checkbox' id='cf_ecm_log'%s> ECM Log</label>"
		"  </div>"
		"</div>",
		g_cfg.newcamd_port,
		esc_nm_bind,
		key_hex,
		g_cfg.cccam_port,
		g_cfg.sock_timeout,
		esc_logfile,
		g_cfg.newcamd_keepalive ? " checked" : "",
		g_cfg.newcamd_mgclient  ? " checked" : "",
		g_cfg.ecm_log           ? " checked" : "");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card' style='margin-bottom:14px'>"
		"  <div class='ct' style='margin-bottom:14px'>&#9656;&nbsp;Web Interface</div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>PORT</label>"
		"    <input class='fi mono' id='cf_webif_port' type='number' min='1' max='65535'"
		"     style='flex:1' value='%d'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>BINDADDR</label>"
		"    <input class='fi mono' id='cf_webif_bindaddr' placeholder='0.0.0.0'"
		"     style='flex:1' value='%s'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>ADMIN USER</label>"
		"    <input class='fi mono' id='cf_webif_user' style='flex:1'"
		"     autocomplete='off' value='%s'></div>"

		"  <div style='display:flex;align-items:center;gap:12px;margin-bottom:9px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>ADMIN PASSWORD</label>"
		"    <input class='fi mono' id='cf_webif_pass' type='text' style='flex:1'"
		"     autocomplete='off' value='%s'></div>"

		"  <div style='display:flex;align-items:center;gap:12px'>"
		"    <label class='fld' style='min-width:160px;margin:0'>AUTO-REFRESH (s)</label>"
		"    <input class='fi mono' id='cf_webif_refresh' type='number' min='0' max='3600'"
		"     placeholder='0 = off' style='flex:1' value='%d'></div>"
		"</div>"

		"<div id='cfgErrBox' style='display:none' class='le'>"
		"  <svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"    <circle cx='12' cy='12' r='10'/><line x1='12' y1='8' x2='12' y2='12'/>"
		"    <line x1='12' y1='16' x2='12.01' y2='16'/></svg>"
		"  <span id='cfgErrMsg'>Error</span>"
		"</div>"
		"<div style='display:flex;justify-content:center;margin-bottom:24px'>"
		"  <button class='btn bp' onclick='saveCfg()' style='min-width:180px;justify-content:center'>"
		ICO_SAVE "&nbsp;Save &amp; Reload</button>"
		"</div>"
		"</div>",
		g_cfg.webif_port,
		esc_wi_bind,
		esc_wi_user,
		esc_wi_pass,
		g_cfg.webif_refresh);

	pos = buf_printf(&buf, &bsz, pos,
		"<script>"
		"function v(id){return document.getElementById(id);}"
		"function saveCfg(){"
		"  var p=new URLSearchParams();"
		"  p.set('newcamd_port',     v('cf_newcamd_port').value);"
		"  p.set('newcamd_bindaddr', v('cf_newcamd_bindaddr').value);"
		"  p.set('newcamd_key',      v('cf_newcamd_key').value);"
		"  p.set('newcamd_keepalive',v('cf_keepalive').checked?'1':'0');"
		"  p.set('newcamd_mgclient', v('cf_mgclient').checked?'1':'0');"
		"  p.set('cccam_port',       v('cf_cccam_port').value);"
		"  p.set('sock_timeout',     v('cf_sock_timeout').value);"
		"  p.set('ecm_log',          v('cf_ecm_log').checked?'1':'0');"
		"  p.set('logfile',          v('cf_logfile').value);"
		"  p.set('webif_port',       v('cf_webif_port').value);"
		"  p.set('webif_bindaddr',   v('cf_webif_bindaddr').value);"
		"  p.set('webif_user',       v('cf_webif_user').value);"
		"  p.set('webif_pass',       v('cf_webif_pass').value);"
		"  p.set('webif_refresh',    v('cf_webif_refresh').value);"
		"  fetch('/api/config/save',{method:'POST',body:p.toString(),"
		"    headers:{'Content-Type':'application/x-www-form-urlencoded'}})"
		"  .then(function(r){return r.json();})"
		"  .then(function(d){"
		"    v('cfgErrBox').style.display='none';"
		"    if(d.ok){"
		"      var m=v('cfgMsg');"
		"      m.style.cssText='display:flex;align-items:center;gap:8px;"
		"padding:10px 14px;border-radius:6px;border:1px solid var(--tg);"
		"color:var(--tg);background:rgba(34,197,94,.07);margin-bottom:14px';"
		"      m.innerHTML='&#10003;&nbsp;Config saved &mdash; reload triggered';"
		"      setTimeout(function(){m.style.display='none';},4000);"
		"    } else {"
		"      v('cfgErrMsg').textContent=d.msg||'Error';"
		"      v('cfgErrBox').style.display='flex';"
		"    }"
		"  })"
		"  .catch(function(e){"
		"    v('cfgErrMsg').textContent='Request failed: '+e;"
		"    v('cfgErrBox').style.display='flex';"
		"  });"
		"}"
		"document.addEventListener('keydown',function(e){"
		"  if((e.ctrlKey||e.metaKey)&&e.key==='s'){e.preventDefault();saveCfg();}"
		"});"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}
void send_page_files(int fd)
{
	PAGE_INIT(131072)

	pos = emit_header(&buf, &bsz, pos, "Files", "files");

	char cfgpath[CFGPATH_LEN], srvpath[CFGPATH_LEN];
	tcmg_build_path(cfgpath, sizeof(cfgpath), g_cfgdir, TCMG_CFG_FILE);
	tcmg_build_path(srvpath, sizeof(srvpath), g_cfgdir, TCMG_SRVID_FILE);

	int  cfg_trunc = 0, srv_trunc = 0;
	char *cfgesc = file_read_escaped(cfgpath, 32768, &cfg_trunc);
	char *srvesc = file_read_escaped(srvpath, 32768, &srv_trunc);
	if (!cfgesc || !srvesc) { free(cfgesc); free(srvesc); free(buf); return; }

	pos = buf_printf(&buf, &bsz, pos,
		"<div style='display:flex;gap:0;margin-bottom:16px;position:sticky;top:var(--tbh);"
		"z-index:10;justify-content:center;background:var(--bg);padding-top:8px;padding-bottom:2px'>"
		"  <button class='ctab act' id='tab_fil_conf' onclick=\"switchFile('conf')\">tcmg.conf</button>"
		"  <button class='ctab'     id='tab_fil_srv'  onclick=\"switchFile('srv')\">tcmg.srvid2</button>"
		"</div>");

	if (cfg_trunc)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='ib2' style='color:var(--or2);border-color:var(--ors);margin-bottom:8px'>"
			ICO_WARN " File exceeds 32 KB &mdash; truncated.</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='pnl_fil_conf'>"
		"<div class='card'>"
		"<div class='et' style='position:sticky;top:calc(var(--tbh)+42px);z-index:9;background:var(--s2)'>"
		"  <span class='ef'>" ICO_FILE "&nbsp;%s</span>"
		"  <span id='fil_conf_st' style='margin-left:auto;font-size:11px;color:var(--t3)'></span>"
		"</div>"
		"<div id='fil_conf_warn' class='ew' style='display:none'>"
		"  " ICO_WARN " Config saved &mdash; reload triggered"
		"</div>"
		"<textarea class='ea' name='cfg' spellcheck='false' id='cfgArea'"
		" oninput=\"markDirty('conf')\">%s</textarea>"
		"<div class='ef2'>"
		"  <button class='btn bp sm' onclick='saveFile(\"conf\")'>" ICO_SAVE "&nbsp;Save &amp; Reload</button>"
		"</div></div></div>",
		cfgpath, cfgesc);
	free(cfgesc);

	if (srv_trunc)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='ib2' style='color:var(--or2);border-color:var(--ors);margin-bottom:8px'>"
			ICO_WARN " File exceeds 32 KB &mdash; truncated.</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='pnl_fil_srv' style='display:none'>"
		"<div class='card'>"
		"<div class='et' style='position:sticky;top:calc(var(--tbh)+42px);z-index:9;background:var(--s2)'>"
		"  <span class='ef'>" ICO_FILE "&nbsp;%s</span>"
		"  <span id='fil_srv_st' style='margin-left:auto;font-size:11px;color:var(--t3)'></span>"
		"</div>"
		"<textarea class='ea' name='srvid2' spellcheck='false' id='srvArea'"
		" oninput=\"markDirty('srv')\" style='height:520px'>%s</textarea>"
		"<div class='ef2'>"
		"  <button class='btn bp sm' onclick='saveFile(\"srv\")'>" ICO_SAVE "&nbsp;Save</button>"
		"</div></div></div>",
		srvpath, srvesc);
	free(srvesc);

	pos = buf_printf(&buf, &bsz, pos,
		"<script>"
		"var _dirty={conf:false,srv:false};"

		"function switchFile(f){"
		"  document.getElementById('pnl_fil_conf').style.display=f==='conf'?'':'none';"
		"  document.getElementById('pnl_fil_srv').style.display=f==='srv'?'':'none';"
		"  document.getElementById('tab_fil_conf').className='ctab'+(f==='conf'?' act':'');"
		"  document.getElementById('tab_fil_srv').className='ctab'+(f==='srv'?' act':'');"
		"}"

		"function markDirty(f){"
		"  if(!_dirty[f]){"
		"    _dirty[f]=true;"
		"    var st=document.getElementById('fil_'+f+'_st');"
		"    if(st)st.innerHTML='<span style=\"color:var(--or2)\">&#9679; Unsaved</span>';"
		"  }"
		"}"

		"function showFileSaved(f,withReload){"
		"  _dirty[f]=false;"
		"  var st=document.getElementById('fil_'+f+'_st');"
		"  if(st)st.innerHTML='<span style=\"color:var(--tg)\">&#10003; Saved</span>';"
		"  if(withReload){"
		"    var w=document.getElementById('fil_conf_warn');"
		"    if(w)w.style.display='flex';"
		"  }"
		"}"

		"function showFileErr(f,msg){"
		"  var st=document.getElementById('fil_'+f+'_st');"
		"  if(st)st.innerHTML='<span style=\"color:var(--tr)\">&#10007; '+msg+'</span>';"
		"}"

		"function saveFile(f){"
		"  var p=new URLSearchParams();"
		"  p.set('file',f);"
		"  p.set('content',document.getElementById(f==='conf'?'cfgArea':'srvArea').value);"
		"  fetch('/api/config/file/save',{method:'POST',body:p.toString(),"
		"    headers:{'Content-Type':'application/x-www-form-urlencoded'}})"
		"  .then(function(r){return r.ok?r.json():null;})"
		"  .then(function(d){"
		"    if(!d){showFileErr(f,'Request failed');return;}"
		"    if(d.ok){showFileSaved(f,f==='conf');}"
		"    else{showFileErr(f,d.msg||'Error');}"
		"  })"
		"  .catch(function(e){showFileErr(f,''+e);});"
		"}"

		"document.addEventListener('keydown',function(e){"
		"  if((e.ctrlKey||e.metaKey)&&e.key==='s'){"
		"    e.preventDefault();"
		"    var confVis=document.getElementById('pnl_fil_conf').style.display!=='none';"
		"    saveFile(confVis?'conf':'srv');"
		"  }"
		"});"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}

