#define MODULE_LOG_PREFIX "webif"
#include "../service/service.h"
#include "../../src/core/runtime_state.h"
#include "../../src/core/utils.h"
#include "../../src/platform/platform.h"
#include "../internal/proto.h"
#include <sys/stat.h>

#define FILE_TRUNC_BANNER \
	"<div class='ib2' style='color:var(--or2);border-color:var(--ors);margin-bottom:8px'>" \
	ICO_WARN " File is larger than 512 KB &mdash; shown read-only so that a truncated copy " \
	"can never be saved over it. Edit it on disk.</div>"

void send_page_config(int fd)
{
	PAGE_INIT(32768)

	S_WEBIF_CONFIG_VIEW cfg = {0};
	webif_config_snapshot(&cfg);

	pos = emit_header(&buf, &bsz, pos, "Config", "config");

	char key_hex[29];
	for (int i = 0; i < 14; i++)
		snprintf(key_hex + i * 2, 3, "%02X", cfg.newcamd_key[i]);
	key_hex[28] = '\0';

	char esc_nm_bind[256], esc_cs_bind[256], esc_logfile[512], esc_wi_bind[256];
	char esc_wi_user[256], esc_wi_pass[256], esc_failban_allow[CFGVAL_LEN*2];

	html_escape(cfg.newcamd_bindaddr, esc_nm_bind, sizeof(esc_nm_bind));
	html_escape(cfg.cs378x_bindaddr, esc_cs_bind, sizeof(esc_cs_bind));
	html_escape(cfg.logfile, esc_logfile, sizeof(esc_logfile));
	html_escape(cfg.webif_bindaddr, esc_wi_bind, sizeof(esc_wi_bind));
	html_escape(cfg.webif_user, esc_wi_user, sizeof(esc_wi_user));
	html_escape(cfg.webif_pass, esc_wi_pass, sizeof(esc_wi_pass));
		html_escape(cfg.failban_allowlist, esc_failban_allow, sizeof(esc_failban_allow));

	pos = buf_printf(&buf, &bsz, pos,
		"<style>"
		".cfg-page{max-width:1160px;margin:0 auto;padding-bottom:16px}"
		".cfg-intro{margin-bottom:16px;padding:14px 16px;background:var(--s2);"
		"border:1px solid var(--bd);border-radius:var(--r);color:var(--t1);"
		"font-size:12px;line-height:1.6}"
		".cfg-intro strong{color:var(--t0)}"
		".cfg-card{background:var(--s1);border:1px solid var(--bd);border-radius:var(--r);"
		"margin-bottom:14px;overflow:hidden}"
		".cfg-head{padding:14px 18px;background:var(--s2);border-bottom:1px solid var(--bd);"
		"display:flex;align-items:center;justify-content:space-between;gap:14px}"
		".cfg-head-main{min-width:0}"
		".cfg-title{font-size:14px;font-weight:700;color:var(--t0)}"
		".cfg-sub{margin-top:2px;font-size:11px;color:var(--t2)}"
		".cfg-body{padding:16px 18px}"
		".cfg-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px 12px}"
		".cfg-grid.one{grid-template-columns:minmax(0,1fr)}"
		".cfg-field{min-width:0}"
		".cfg-label{display:block;margin-bottom:5px;font-size:11px;font-weight:650;"
		"letter-spacing:.04em;color:var(--t1)}"
		".cfg-input{width:100%%;padding:9px 11px;background:var(--s2);border:1px solid var(--bd2);"
		"border-radius:var(--rsm);color:var(--t0);font-family:var(--sans);font-size:13px;"
		"outline:none;transition:border-color .18s,box-shadow .18s;box-sizing:border-box}"
		".cfg-input.mono{font-family:var(--mono);font-size:12px}"
		".cfg-input[type=number]{width:104px;max-width:100%%;font-family:var(--mono)}"
		".cfg-input[type=time]{width:112px;max-width:100%%;font-family:var(--mono)}"
		".cfg-input[type=date]{width:132px;max-width:100%%}"
		".cfg-input:focus{border-color:var(--p);box-shadow:0 0 0 3px var(--ps)}"
		".cfg-input:disabled{opacity:.48;cursor:not-allowed}"
		".cfg-help{margin-top:4px;font-size:10.5px;color:var(--t2);line-height:1.45}"
		".cfg-toggle-row{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-top:13px}"
		".cfg-toggle{display:inline-flex;align-items:center;gap:7px;padding:8px 10px;"
		"background:var(--s2);border:1px solid var(--bd);border-radius:var(--rsm);"
		"font-size:12px;color:var(--t0);font-weight:600}"
		".cfg-toggle input{margin:0;accent-color:var(--p)}"
		".cfg-divider{height:1px;background:var(--bd);margin:15px 0}"
		".cfg-msg{display:none;align-items:center;gap:8px;margin-bottom:14px;padding:10px 13px;"
		"border-radius:var(--rsm);font-size:12px}"
		".cfg-msg.ok{border:1px solid var(--gr);color:var(--gr);background:var(--grs)}"
		".cfg-msg.err{border:1px solid var(--re);color:var(--re);background:var(--res)}"
		".cfg-save{display:flex;justify-content:center;margin:20px 0 4px}"
		".cfg-save button{min-width:190px;justify-content:center}"
		"@media(min-width:1100px){.cfg-grid{grid-template-columns:repeat(3,minmax(0,1fr))}.cfg-grid.one{grid-template-columns:minmax(0,1fr)}}"
		"@media(max-width:720px){"
		".cfg-page{padding:0 2px 20px}"
		".cfg-grid{grid-template-columns:minmax(0,1fr)}"
		".cfg-body{padding:14px}"
		".cfg-head{padding:13px 14px}"
		".cfg-toggle-row{display:grid;grid-template-columns:minmax(0,1fr)}"
		".cfg-toggle{width:100%%;box-sizing:border-box}"
		"}"
		"</style>"
		"<div class='cfg-page'>"
		"<div id='cfgMsg' class='cfg-msg' role='status' aria-live='polite'></div>"
		"<div class='cfg-intro'>"
		"<strong>Server configuration</strong><br>"
		"Only the settings shown here are changed by this page. Values are validated before they are saved, "
		"and a successful save triggers a configuration reload."
		"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<section class='cfg-card'>"
		"<div class='cfg-head'><div class='cfg-head-main'>"
		"<div class='cfg-title'>Newcamd / MGcamd / CCcam / CS378X</div>"
		"<div class='cfg-sub'>Incoming protocol listeners and common socket options</div>"
		"</div></div>"
		"<div class='cfg-body'>"
		"<div class='cfg-grid'>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_newcamd_port'>Newcamd port</label>"
		"<input class='cfg-input mono' id='cf_newcamd_port' type='number' min='0' max='65535' value='%d'>"
		"<div class='cfg-help'>0 disables the Newcamd listener.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_cccam_port'>CCcam port</label>"
		"<input class='cfg-input mono' id='cf_cccam_port' type='number' min='0' max='65535' value='%d'>"
		"<div class='cfg-help'>0 disables the CCcam listener.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_cs378x_port'>CS378X port</label>"
		"<input class='cfg-input mono' id='cf_cs378x_port' type='number' min='0' max='65535' value='%d'>"
		"<div class='cfg-help'>CS378X/camd35 TCP listener; 0 disables it.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_cs378x_bindaddr'>CS378X bind address</label>"
		"<input class='cfg-input mono' id='cf_cs378x_bindaddr' placeholder='0.0.0.0' value='%s'>"
		"<div class='cfg-help'>Leave empty to listen on all local IPv4 interfaces.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_newcamd_bindaddr'>Newcamd bind address</label>"
		"<input class='cfg-input mono' id='cf_newcamd_bindaddr' placeholder='0.0.0.0' value='%s'>"
		"<div class='cfg-help'>Leave empty to use the default local address.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_sock_timeout'>Socket timeout</label>"
		"<input class='cfg-input mono' id='cf_sock_timeout' type='number' min='5' max='600' value='%d'>"
		"<div class='cfg-help'>Maximum wait for network input when no server heartbeat is due.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_server_keepalive'>Server keepalive</label>"
		"<input class='cfg-input mono' id='cf_server_keepalive' type='number' min='0' max='3600' value='%d'>"
		"<div class='cfg-help'>Server sends an application heartbeat after this many idle seconds. 0 disables it.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_server_keepalive_misses'>Keepalive misses</label>"
		"<input class='cfg-input mono' id='cf_server_keepalive_misses' type='number' min='1' max='20' value='%d'>"
		"<div class='cfg-help'>Disconnect after this many unanswered heartbeats.</div></div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_newcamd_key'>Newcamd DES key</label>"
		"<input class='cfg-input mono' id='cf_newcamd_key' maxlength='28' spellcheck='false' "
		"autocomplete='off' value='%s'>"
		"<div class='cfg-help'>28 hexadecimal characters.</div></div>"

		"</div>"
		"<div class='cfg-toggle-row'>"
		"<label class='cfg-toggle'><input type='checkbox' id='cf_keepalive'%s> Keepalive</label>"
		"<label class='cfg-toggle'><input type='checkbox' id='cf_mgclient'%s> MGcamd client mode</label>"
		"</div>"
		"</div></section>",
		cfg.newcamd_port,
		cfg.cccam_port,
		cfg.cs378x_port,
		esc_cs_bind,
		esc_nm_bind,
		cfg.sock_timeout,
		cfg.server_keepalive,
		cfg.server_keepalive_misses,
		key_hex,
		cfg.newcamd_keepalive ? " checked" : "",
		cfg.newcamd_mgclient ? " checked" : "");

	pos = buf_printf(&buf, &bsz, pos,
		"<section class='cfg-card'>"
		"<div class='cfg-head'><div class='cfg-head-main'>"
		"<div class='cfg-title'>Web Interface</div>"
		"<div class='cfg-sub'>HTTP port, bind address and administrator login</div>"
		"</div></div>"
		"<div class='cfg-body'>"
		"<div class='cfg-grid'>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_webif_port'>Port</label>"
		"<input class='cfg-input mono' id='cf_webif_port' type='number' min='1' max='65535' value='%d'>"
		"</div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_webif_bindaddr'>Bind address</label>"
		"<input class='cfg-input mono' id='cf_webif_bindaddr' placeholder='0.0.0.0' value='%s'>"
		"</div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_webif_user'>Admin username</label>"
		"<input class='cfg-input' id='cf_webif_user' autocomplete='off' value='%s'>"
		"</div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_webif_pass'>Admin password</label>"
		"<input class='cfg-input' id='cf_webif_pass' type='text' autocomplete='off' value='%s'>"
		"</div>"

		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_webif_refresh'>Auto-refresh</label>"
		"<input class='cfg-input mono' id='cf_webif_refresh' type='number' min='0' max='3600' value='%d'>"
		"<div class='cfg-help'>0 disables automatic page refresh.</div></div>"

		"</div>"
		"</div></section>",
		cfg.webif_port,
		esc_wi_bind,
		esc_wi_user,
		esc_wi_pass,
		cfg.webif_refresh);

	pos = buf_printf(&buf, &bsz, pos,
		"<section class='cfg-card'>"
		"<div class='cfg-head'><div class='cfg-head-main'>"
		"<div class='cfg-title'>Logging</div>"
		"<div class='cfg-sub'>ECM logging and log file location</div>"
		"</div></div>"
		"<div class='cfg-body'>"
		"<div class='cfg-grid'>"
		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_logfile'>Log file</label>"
		"<input class='cfg-input mono' id='cf_logfile' placeholder='Leave empty to disable file logging' value='%s'>"
		"</div>"
		"</div>"
		"<div class='cfg-toggle-row'>"
		"<label class='cfg-toggle'><input type='checkbox' id='cf_ecm_log'%s> ECM log</label>"
		"</div>"
		"</div></section>",
		esc_logfile,
		cfg.ecm_log ? " checked" : "");

	pos = buf_printf(&buf, &bsz, pos,
		"<section class='cfg-card'>"
		"<div class='cfg-head'><div class='cfg-head-main'>"
		"<div class='cfg-title'>Security</div>"
		"<div class='cfg-sub'>Temporary protection against repeated failed logins</div>"
		"</div></div>"
		"<div class='cfg-body'>"
		"<div class='cfg-toggle-row'>"
		"<label class='cfg-toggle'><input type='checkbox' id='cf_failban_enabled'%s> Enabled</label>"
		"</div>"
		"<div class='cfg-grid'>"
		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_failban_allowlist'>Allowlist</label>"
		"<input class='cfg-input mono' id='cf_failban_allowlist' value='%s' placeholder='192.168.1.10, 10.0.0.0/24'>"
		"<div class='cfg-help'>These IPs/CIDRs never accumulate failures and are never banned. Separate entries with commas or spaces.</div>"
		"</div>"
		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_failban_max_fails'>Maximum failed attempts</label>"
		"<input class='cfg-input mono' id='cf_failban_max_fails' type='number' min='1' max='1000' value='%d'>"
		"</div>"
		"<div class='cfg-field'>"
		"<label class='cfg-label' for='cf_failban_ban_secs'>Ban duration</label>"
		"<input class='cfg-input mono' id='cf_failban_ban_secs' type='number' min='10' max='604800' value='%d'>"
		"<div class='cfg-help'>Duration in seconds. An IP is blocked after the configured number of failed logins.</div>"
		"</div>"
		"</div>"
		"</div></section>",
		cfg.failban_enabled ? " checked" : "",
		esc_failban_allow,
		cfg.failban_max_fails,
		cfg.failban_ban_secs);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='cfg-save'>"
		"<button type='button' class='tool pri' id='cfgSaveBtn' onclick='saveCfg()'>"
		ICON("i-save") "Save &amp; Reload</button>"
		"</div>"
		"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<script>"
		"function cfgEl(id){return document.getElementById(id);}"
		"function cfgSetMessage(ok,msg){"
		"  var e=cfgEl('cfgMsg');"
		"  e.className='cfg-msg '+(ok?'ok':'err');"
		"  e.textContent=msg;"
		"  e.style.display='flex';"
		"}"
		"function cfgSync(){"
		"}"
		"function cfgHex28(s){return /^[0-9a-fA-F]{28}$/.test(s);}"
		"function cfgValidate(){"
		"  var nk=cfgEl('cf_newcamd_key').value.trim();"
		"  if(!cfgHex28(nk)){cfgSetMessage(false,'Newcamd DES key must contain 28 hexadecimal characters.');cfgEl('cf_newcamd_key').focus();return false;}"
		"  return true;"
		"}"
		"function saveCfg(){"
		"  if(window._cfgSaving||!cfgValidate())return;"
		"  window._cfgSaving=true;"
		"  var b=cfgEl('cfgSaveBtn');"
		"  b.disabled=true;"
		"  b.dataset.cfgOriginal=b.innerHTML;"
		"  b.textContent='Saving...';"
		"  var p=new URLSearchParams();"
		"  p.set('newcamd_port',cfgEl('cf_newcamd_port').value);"
		"  p.set('newcamd_bindaddr',cfgEl('cf_newcamd_bindaddr').value);"
		"  p.set('newcamd_key',cfgEl('cf_newcamd_key').value.trim());"
		"  p.set('newcamd_keepalive',cfgEl('cf_keepalive').checked?'1':'0');"
		"  p.set('newcamd_mgclient',cfgEl('cf_mgclient').checked?'1':'0');"
		"  p.set('cccam_port',cfgEl('cf_cccam_port').value);"
		"  p.set('cs378x_port',cfgEl('cf_cs378x_port').value);"
		"  p.set('cs378x_bindaddr',cfgEl('cf_cs378x_bindaddr').value);"
		"  p.set(\'sock_timeout\',cfgEl(\'cf_sock_timeout\').value);"
		"  p.set(\'server_keepalive\',cfgEl(\'cf_server_keepalive\').value);"
		"  p.set(\'server_keepalive_misses\',cfgEl(\'cf_server_keepalive_misses\').value);"
		"  p.set('ecm_log',cfgEl('cf_ecm_log').checked?'1':'0');"
		"  p.set('logfile',cfgEl('cf_logfile').value);"
		"  p.set('webif_port',cfgEl('cf_webif_port').value);"
		"  p.set('webif_bindaddr',cfgEl('cf_webif_bindaddr').value);"
		"  p.set('webif_user',cfgEl('cf_webif_user').value);"
		"  p.set('webif_pass',cfgEl('cf_webif_pass').value);"
		"  p.set('webif_refresh',cfgEl('cf_webif_refresh').value);"
		"  p.set(\'failban_enabled\',cfgEl(\'cf_failban_enabled\').checked?\'1\':\'0\');"
		"  p.set(\'failban_allowlist\',cfgEl(\'cf_failban_allowlist\').value);"
		"  p.set(\'failban_max_fails\',cfgEl(\'cf_failban_max_fails\').value);"
		"  p.set('failban_ban_secs',cfgEl('cf_failban_ban_secs').value);"
		"  fetch('/api/config/save',{method:'POST',body:p.toString(),headers:{'Content-Type':'application/x-www-form-urlencoded'}})"
		"  .then(function(r){return r.json().catch(function(){return null;});})"
		"  .then(function(d){"
		"    if(d&&d.ok){cfgSetMessage(true,'Configuration saved. Reload has been triggered.');}"
		"    else{cfgSetMessage(false,d&&d.msg?d.msg:'Request failed.');}"
		"  })"
		"  .catch(function(e){cfgSetMessage(false,'Request failed: '+e);})"
		"  .finally(function(){"
		"    window._cfgSaving=false;b.disabled=false;"
		"    b.innerHTML=b.dataset.cfgOriginal||'Save & Reload';"
		"  });"
		"}"
		"document.addEventListener('keydown',function(e){"
		"  if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='s'){e.preventDefault();saveCfg();}"
		"});"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}
void send_page_files(int fd)
{
    PAGE_INIT(65536)
    pos = emit_header(&buf, &bsz, pos, "Files", "files");

    char cfgpath[CFGPATH_LEN];
    tcmg_build_path(cfgpath, sizeof(cfgpath), g_cfgdir, TCMG_CFG_FILE);
    int cfg_trunc = 0;
    size_t cfg_len = 0;
    struct stat cfg_st;
    if (stat(cfgpath, &cfg_st) == 0 && cfg_st.st_size > 0) cfg_len = (size_t)cfg_st.st_size;
    char *cfgesc = file_read_escaped(cfgpath, WEB_FILE_VIEW_MAX, &cfg_trunc);
    if (!cfgesc) {
        free(buf);
        send_json_error(fd, 503, "Service Unavailable", "out of memory");
        return;
    }

    pos = buf_printf(&buf, &bsz, pos,
        "<div class='file-page'>"
        "<div class='file-tabs' role='tablist' aria-label='Files'>"
        "  <button type='button' class='ctab act' id='fileTab-conf' role='tab' aria-selected='true' aria-controls='filePanel' tabindex='0' data-file='conf' onclick='switchFile(\"conf\")'>tcmg.conf</button>"
        "  <button type='button' class='ctab' id='fileTab-usr' role='tab' aria-selected='false' aria-controls='filePanel' tabindex='-1' data-file='usr' onclick='switchFile(\"usr\")'>tcmg.users</button>"
        "  <button type='button' class='ctab' id='fileTab-rdr' role='tab' aria-selected='false' aria-controls='filePanel' tabindex='-1' data-file='rdr' onclick='switchFile(\"rdr\")'>tcmg.readers</button>"
        "  <button type='button' class='ctab' id='fileTab-srv' role='tab' aria-selected='false' aria-controls='filePanel' tabindex='-1' data-file='srv' onclick='switchFile(\"srv\")'>tcmg.srvid2</button>"
        "</div>"
        "<div class='file-card' id='filePanel' role='tabpanel' tabindex='0' aria-labelledby='fileTab-conf'>"
        "%s"
        "<div class='et'><span id='fileName' class='ef'>tcmg.conf</span><span id='fileState' class='file-state'>Saved</span></div>"
        "<div id='fileWarn' class='ew' style='display:%s'></div>"
        "<textarea class='ea' id='fileArea' spellcheck='false' autocomplete='off'%s>%s</textarea>"
        "<div class='ef2 file-actions'>"
        "  <div class='file-info'><span id='fileSize'>%zu bytes</span><span class='file-dot'>·</span><span id='fileMode'>Validated on save</span></div>"
        "  <div class='file-buttons'>"
        "    <button type='button' class='tool sm' onclick='reloadFile()'>Reload</button>"
        "    <button type='button' class='tool sm' onclick='downloadCurrentFile()'>Download</button>"
        "    <button type='button' class='tool pri sm' id='fileSaveBtn' onclick='saveCurrentFile()' %s>" ICON("i-save") "Save</button>"
        "  </div>"
        "</div></div></div>",
        cfg_trunc ? FILE_TRUNC_BANNER : "",
        cfg_trunc ? "flex" : "none",
        cfg_trunc ? " readonly" : "", cfgesc,
        (size_t)cfg_len, cfg_trunc ? "disabled" : "");
    free(cfgesc);

    pos = buf_printf(&buf, &bsz, pos,
        "<script>"
        "var currentFile='conf',dirty=false,busy=false;"
        "var fileNames={conf:'tcmg.conf',usr:'tcmg.users',rdr:'tcmg.readers',srv:'tcmg.srvid2'};"
        "function state(t,c){var e=document.getElementById('fileState');if(e){e.textContent=t;e.className='file-state '+(c||'')}}"
        "function setBusy(v){busy=v;var e=document.getElementById('fileBusy'),b=document.getElementById('fileSaveBtn');if(e)e.hidden=!v;if(b)b.disabled=v||document.getElementById('fileArea').readOnly}"
        "function clearWarn(){var e=document.getElementById('fileWarn');if(e){e.style.display='none';e.textContent=''}}"
        "function warn(msg){var e=document.getElementById('fileWarn');if(e){e.style.display='flex';e.textContent=msg}}"
        "function markDirty(){dirty=true;state('Unsaved','dirty');clearWarn()}"
        "function tabs(f){document.querySelectorAll('.file-tabs .ctab').forEach(function(b){var a=b.dataset.file===f;b.classList.toggle('act',a);b.setAttribute('aria-selected',a?'true':'false');b.tabIndex=a?0:-1});var p=document.getElementById('filePanel');if(p)p.setAttribute('aria-labelledby','fileTab-'+f)}"
        "function switchFile(f){if(busy||f===currentFile)return;if(dirty&&!confirm('Discard unsaved changes?'))return;loadFile(f)}"
        "function reloadFile(){if(dirty&&!confirm('Discard unsaved changes?'))return;loadFile(currentFile)}function downloadCurrentFile(){if(busy)return;var blob=new Blob([document.getElementById('fileArea').value],{type:'text/plain;charset=utf-8'}),u=URL.createObjectURL(blob),a=document.createElement('a');a.href=u;a.download=fileNames[currentFile];document.body.appendChild(a);a.click();a.remove();setTimeout(function(){URL.revokeObjectURL(u)},0)}"
        "function loadFile(f){currentFile=f;dirty=false;tabs(f);clearWarn();setBusy(true);state('Loading','loading');document.getElementById('fileName').textContent=fileNames[f];fetch('/api/config/file/get?file='+encodeURIComponent(f),{cache:'no-store',credentials:'same-origin'}).then(function(r){return r.json().catch(function(){return {ok:false,msg:'HTTP '+r.status}})}).then(function(x){if(!x.ok)throw new Error(x.msg||'Load failed');var a=document.getElementById('fileArea');a.value=x.content||'';a.readOnly=!!x.truncated;document.getElementById('fileSize').textContent=x.size+' bytes'+(x.truncated?' · truncated':'');document.getElementById('fileMode').textContent=x.truncated?'Read only':'Validated on save';document.getElementById('fileSaveBtn').disabled=!!x.truncated;document.getElementById('fileWarn').style.display=x.truncated?'flex':'none';if(x.truncated)document.getElementById('fileWarn').textContent='File is larger than 512 KB and is shown read-only.';state('Saved');}).catch(function(e){warn(String(e));state('Load error','error')}).finally(function(){setBusy(false)})}"
        "function saveCurrentFile(){if(busy)return;var a=document.getElementById('fileArea');if(a.readOnly)return;var p=new URLSearchParams();p.set('file',currentFile);p.set('content',a.value);clearWarn();setBusy(true);state('Saving','loading');fetch('/api/config/file/save',{method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p.toString()}).then(function(r){return r.json().catch(function(){return {ok:false,msg:'HTTP '+r.status}})}).then(function(x){if(!x.ok)throw new Error(x.msg||'Save failed');dirty=false;state('Saved','ok');document.getElementById('fileMode').textContent=currentFile==='srv'?'Reloaded':'Saved and reload queued';if(currentFile==='conf'||currentFile==='usr'||currentFile==='rdr'){warn('Saved. Configuration reload queued. Existing sessions remain active until the new state is applied.');} }).catch(function(e){warn(String(e));state('Save error','error')}).finally(function(){setBusy(false)})}"
        "document.getElementById('fileArea').addEventListener('input',markDirty);document.querySelectorAll('.file-tabs .ctab').forEach(function(b){b.addEventListener('keydown',function(e){var a=[].slice.call(document.querySelectorAll('.file-tabs .ctab')),i=a.indexOf(b),n=i;if(e.key==='ArrowRight'||e.key==='ArrowDown')n=(i+1)%%a.length;else if(e.key==='ArrowLeft'||e.key==='ArrowUp')n=(i+a.length-1)%%a.length;else if(e.key==='Home')n=0;else if(e.key==='End')n=a.length-1;else return;e.preventDefault();a[n].focus();a[n].click()})});"
        "document.addEventListener('keydown',function(e){if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='s'){e.preventDefault();saveCurrentFile()}});"
        "</script>");

    pos = emit_footer(&buf, &bsz, pos);
    PAGE_SEND_AND_FREE(fd);
}
