#define MODULE_LOG_PREFIX "webif"
#include "../service/service.h"
#include "../../src/core/utils.h"
#include "../internal/proto.h"

static void reader_groups_text(const S_WEBIF_READER_VIEW *r, char *out, size_t sz)
{
	out[0] = 0;
	tcmg_strlcpy(out, r->groups, sz);
}

static void reader_caid_text(const S_WEBIF_READER_VIEW *r, char *out, size_t sz)
{
    tcmg_strlcpy(out, r->caids, sz);
    if (!strcasecmp(r->protocol, "emu") && r->ecmkeys[0]) {
        char tmp[WEBIF_TEXT_8192];
        tcmg_strlcpy(tmp, r->ecmkeys, sizeof(tmp));
        char *save = NULL, *line = strtok_r(tmp, "\r\n", &save);
        while (line) {
            char *eq = strchr(line, '=');
            if (eq && eq != line) {
                char caid[8];
                size_t n = (size_t)(eq - line); if (n >= sizeof(caid)) n = sizeof(caid) - 1;
                memcpy(caid, line, n); caid[n] = 0;
                if (!strstr(out, caid)) {
                    if (out[0]) tcmg_strlcat(out, ",", sz);
                    tcmg_strlcat(out, caid, sz);
                }
            }
            line = strtok_r(NULL, "\r\n", &save);
        }
    }
}

void send_page_readers(int fd)
{
	PAGE_INIT(65536)
	pos = emit_header(&buf, &bsz, pos, "Readers", "readers");

	int n = webif_reader_count();
	S_WEBIF_READER_VIEW *reader = calloc(1, sizeof(*reader));
	if (!reader) {
		send_json_error(fd, 503, "Service Unavailable", "out of memory");
		return;
	}
	pos = buf_printf(&buf, &bsz, pos,
		"<section class='utoolbar' aria-label='Readers toolbar'>"
		"<div class='tgrp'>"
		"<button type='button' class='tool ust act' aria-pressed='true'>Readers <span class='n'>%d</span></button>"
		"</div>"
		"<div class='tgrp'>"
		"<button type='button' class='tool' onclick='location.reload()'>Refresh</button>"
		"<button type='button' class='tool pri' onclick='openReader(-1)'>Add Reader</button>"
		"</div></section>", n);

	pos = buf_printf(&buf, &bsz, pos,
		"<section class='tw cm'>"
		"<table><thead><tr>"
		"<th>Label</th><th>On</th><th>Protocol</th><th>Device / Reader</th>"
		"<th>Groups</th><th>CAID</th><th>Actions</th>"
		"</tr></thead><tbody>");

	int snapshot_n = 0;
	for (int idx = 0; idx < MAX_READERS; idx++) {
		if (!webif_reader_get(idx, reader)) continue;
		snapshot_n++;
		const S_WEBIF_READER_VIEW *r = reader;
		char l[256], p[128], d[512], graw[256], craw[256], g[256], c[256];
		html_escape(r->label, l, sizeof(l));
		html_escape(r->protocol, p, sizeof(p));
		html_escape(r->device, d, sizeof(d));
		reader_groups_text(r, graw, sizeof(graw));
		reader_caid_text(r, craw, sizeof(craw));
		html_escape(graw, g, sizeof(g));
		html_escape(craw, c, sizeof(c));
		pos = buf_printf(&buf, &bsz, pos,
			"<tr>"
			"<td class='mono'>%s</td>"
			"<td>%s</td>"
			"<td><span class='badge bbl'>%s</span></td>"
			"<td class='mono'>%s</td>"
			"<td class='mono'>%s</td>"
			"<td class='mono'>%s</td>"
			"<td><button type='button' class='tool sm' onclick='openReader(%d)'>Edit</button> "
			"<button type='button' class='tool sm danger' onclick='deleteReader(%d)'>Delete</button></td>"
			"</tr>",
			l, r->enabled ? "Yes" : "No", p,
			d[0] ? d : "&mdash;", g[0] ? g : "&mdash;", c[0] ? c : "&mdash;", r->index, r->index);
	}

	if (n == 0)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='erow'><td colspan='7'>No readers configured.</td></tr>");

	pos = buf_printf(&buf, &bsz, pos,
		"</tbody></table></section>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='mo' id='rModal' hidden>"
		"<div class='mc reader-modal' role='dialog' aria-modal='true' aria-labelledby='rTitle'>"
		"<div class='mh'><h2 id='rTitle'>Reader</h2>"
		"<button type='button' class='ib' onclick='closeReader()' aria-label='Close'>×</button></div>"
		"<div class='mb'><input type='hidden' id='rIndex' value='-1'>"


		"<div class='reader-topgrid'>"
		"<div class='fg'><label class='fld' for='rLabel'>Label</label>"
		"<input class='fi' id='rLabel' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rProtocol'>Protocol</label>"
		"<select class='fi' id='rProtocol'>"
		"<option value='cccam'>CCcam</option>"
		"<option value='newcamd'>Newcamd</option>"
		"<option value='mgcamd'>MGcamd</option>"
		"<option value='cs378x'>CS378X</option>"
		"<option value='emu'>EMU</option>"
		"<option value='pcsc'>PCSC</option>"
		"<option value='internal'>Internal</option>"
		"</select></div>"
		"<label class='reader-enable'><span>Enabled</span><input id='rEnabled' type='checkbox' checked></label>"
		"</div>"

		"<div class='shd'><div class='stl'>Protocol</div></div>"

		"<div id='rCccamFields'>"
		"<div class='reader-cccam-grid'>"
		"<div class='fg'><label class='fld' for='rCccamDevice'>Server</label>"
		"<input class='fi mono' id='rCccamDevice' placeholder='host,port' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rUser'>Username</label>"
		"<input class='fi' id='rUser' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rPassword'>Password</label>"
		"<input class='fi' id='rPassword' type='password' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rTimeout'>Timeout (s)</label>"
		"<input class='fi mono' id='rTimeout' type='number' min='1' max='600'></div>"
		"<div class='fg' id='rKeyWrap' hidden><label class='fld' for='rKey'>DES Key</label>"
		"<input class='fi mono' id='rKey' maxlength='28' spellcheck='false' placeholder='28 hex chars' autocomplete='off'></div>"
		"</div></div>"

		"<div id='rPcscFields' hidden>"
		"<div class='reader-pcsc-grid'>"
		"<div class='fg'><label class='fld' for='rPcscDevice'>PCSC reader</label>"
		"<input class='fi mono' id='rPcscDevice' placeholder='reader name or index' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rPoll'>Poll (ms)</label>"
		"<input class='fi mono' id='rPoll' type='number' min='50' max='10000'></div>"
		"<div class='fg'><label class='fld' for='rFast'>Fast reset (s)</label>"
		"<input class='fi mono' id='rFast' type='number' min='0' max='86400'></div>"
		"<label class='reader-check'><span>DO_ECM</span><input id='rDoEcm' type='checkbox'></label>"
		"</div></div>"

		"<div id='rInternalFields' hidden>"
		"<div class='reader-pcsc-grid'>"
		"<div class='fg'><label class='fld' for='rInternalDevice'>SCI device</label>"
		"<input class='fi mono' id='rInternalDevice' placeholder='/dev/sci0' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rInternalPoll'>Poll (ms)</label>"
		"<input class='fi mono' id='rInternalPoll' type='number' min='50' max='10000'></div>"
		"<div class='fg'><label class='fld' for='rInternalFast'>Fast reset (s)</label>"
		"<input class='fi mono' id='rInternalFast' type='number' min='0' max='86400'></div>"
		"<label class='reader-check'><span>DO_ECM</span><input id='rInternalDoEcm' type='checkbox'></label>"
		"</div></div>"

		"<div id='rEmuFields' hidden>"
		"<div class='fg'><label class='fld' for='rKeys'>ECM keys</label>"
		"<textarea class='ea mono' id='rKeys' style='height:108px;min-height:108px' spellcheck='false' "
		"placeholder='0B00=64 hex characters'></textarea></div>"
		"</div>"

		"<div class='shd'><div class='stl'>Routing</div></div>"
		"<div class='reader-routing-grid'>"
		"<div class='fg' id='rCaidWrap'><label class='fld' for='rCaid'>CAID</label>"
		"<input class='fi mono' id='rCaid' placeholder='0B02,0B07' autocomplete='off' spellcheck='false'></div>"
		"<div class='fg'><label class='fld' for='rSid'>SID whitelist</label>"
		"<input class='fi mono' id='rSid' placeholder='0064,00C8,1234' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rWl'>ECM whitelist</label>"
		"<input class='fi mono' id='rWl' maxlength='2' placeholder='37' autocomplete='off'></div>"
		"<div class='fg'><label class='fld' for='rGroup'>Group</label>"
		"<input class='fi mono' id='rGroup' placeholder='1,5' autocomplete='off'></div>"
		"</div>"
		"<div class='fhint' id='rCaidHint' hidden>EMU: leave CAID empty to use the CAIDs of the ECM keys above. "
		"Several CAIDs are separated by commas.</div>"

		"<div id='rErr' class='le' hidden><span id='rErrMsg'></span></div>"
		"</div>"
		"<div class='mf'><button type='button' class='btn bg' onclick='closeReader()'>Cancel</button>"
		"<button type='button' class='btn bp' id='rSave' onclick='saveReader()'>Save</button></div>"
		"</div></div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<script>"
		"function $(i){return document.getElementById(i)}"
		"function rerr(m){$('rErrMsg').textContent=m;$('rErr').hidden=false}"
		"function clearErr(){$('rErr').hidden=true}"
		"function keyCaids(){var o=[],m,re=/^\\s*([0-9A-Fa-f]{1,4})\\s*=/gm,t=$('rKeys').value;"
		"while((m=re.exec(t))){var c=('0000'+m[1].toUpperCase()).slice(-4);if(o.indexOf(c)<0)o.push(c)}return o}"
		"function syncCaidPlaceholder(){var e=$('rProtocol').value==='emu',k=keyCaids();"
		"$('rCaid').placeholder=e?(k.length?k.join(',')+'  (from ECM keys)':'from ECM keys'):'0B02,0B07'}"
		"function caidList(s){var t=s.split(/[\\s,;]+/).filter(Boolean),o=[];"
		"for(var i=0;i<t.length;i++){if(!/^[0-9A-Fa-f]{1,4}$/.test(t[i]))return null;"
		"var c=('0000'+t[i].toUpperCase()).slice(-4);if(o.indexOf(c)<0)o.push(c)}return o}"
		"function syncReaderProtocol(){"
		"var p=$('rProtocol').value,i=p==='internal';"
		"var m=p==='mgcamd',c=(p==='cccam'),n=(p==='newcamd'),x=(p==='cs378x'),s=p==='pcsc',e=p==='emu',auth=(c||m||n||x),net=(auth);"
		"$('rCccamFields').hidden=!(c||n||x);$('rPcscFields').hidden=!s;$('rInternalFields').hidden=!i;$('rEmuFields').hidden=!e;"
		"$('rKeyWrap').hidden=!(m||n);$('rCaidHint').hidden=!e;syncCaidPlaceholder();"
		"$('rUser').disabled=!auth;$('rPassword').disabled=!auth;$('rTimeout').disabled=!net;$('rKey').disabled=!(m||n);"
		"$('rCccamDevice').disabled=!net;$('rPcscDevice').disabled=!s;$('rInternalDevice').disabled=!i;"
		"$('rPoll').disabled=!s;$('rFast').disabled=!s;$('rDoEcm').disabled=!s;$('rInternalPoll').disabled=!i;$('rInternalFast').disabled=!i;$('rInternalDoEcm').disabled=!i;$('rKeys').disabled=!e;"
		"}"
		"function openReader(i){"
		"clearErr();"
		"if(i<0){"
		"$('rIndex').value='-1';$('rTitle').textContent='Add Reader';$('rLabel').value='';$('rProtocol').value='cccam';"
		"$('rCccamDevice').value='';$('rPcscDevice').value='';$('rInternalDevice').value='/dev/sci0';$('rUser').value='';$('rPassword').value='';$('rKey').value='';$('rTimeout').value='30';"
		"$('rGroup').value='1';$('rCaid').value='';$('rSid').value='';$('rWl').value='37';$('rPoll').value='250';$('rFast').value='0';"
		"$('rInternalPoll').value='250';$('rInternalFast').value='0';"
		"$('rEnabled').checked=true;$('rDoEcm').checked=true;$('rInternalDoEcm').checked=true;$('rKeys').value='';"
		"syncReaderProtocol();$('rModal').hidden=false;document.body.classList.add('mo-open');return;}"
		"fetch('/api/reader/get?index='+i,{cache:'no-store'}).then(function(x){return x.json()}).then(function(d){"
		"if(!d.ok){rerr(d.msg||'Reader not found');return;}"
		"$('rIndex').value=d.index;$('rTitle').textContent='Edit Reader';$('rLabel').value=d.label||'';$('rProtocol').value=(d.protocol||'emu').toLowerCase();"
		"$('rCccamDevice').value=d.device||'';$('rPcscDevice').value=d.device||'';$('rInternalDevice').value=d.device||'';$('rUser').value=d.user||'';$('rPassword').value=d.password||'';$('rKey').value=d.key||'';"
		"$('rTimeout').value=d.inactivitytimeout||30;$('rGroup').value=d.group||'1';$('rCaid').value=d.caid||'';$('rSid').value=d.sid_whitelist||'';"
		"$('rWl').value=d.ecmwhitelist||'37';$('rPoll').value=d.POLL_MS||250;$('rFast').value=d.FAST_RESET||0;"
		"$('rInternalPoll').value=d.POLL_MS||250;$('rInternalFast').value=d.FAST_RESET||0;$('rInternalDoEcm').checked=!!d.DO_ECM;"
		"$('rEnabled').checked=!!d.enabled;$('rDoEcm').checked=!!d.DO_ECM;$('rKeys').value=d.ecmkeys||'';"
		"syncReaderProtocol();$('rModal').hidden=false;document.body.classList.add('mo-open');"
		"}).catch(function(){rerr('Request failed')})}"
		"function closeReader(){$('rModal').hidden=true;document.body.classList.remove('mo-open')}"
		"function saveReader(){"
		"clearErr();var proto=$('rProtocol').value,p=new URLSearchParams();"
		"p.set('index',$('rIndex').value);p.set('label',$('rLabel').value.trim());p.set('protocol',proto);"
		"var isSrv=(proto==='cccam'||proto==='mgcamd'||proto==='newcamd'||proto==='cs378x');"
		"p.set('device',proto==='pcsc'?$('rPcscDevice').value.trim():(proto==='internal'?$('rInternalDevice').value.trim():(isSrv?$('rCccamDevice').value.trim():'')));"
		"p.set('user',isSrv?$('rUser').value:'');p.set('password',isSrv?$('rPassword').value:'');"
		"p.set('key',(proto==='mgcamd'||proto==='newcamd')?$('rKey').value.trim():'');"
		"p.set('inactivitytimeout',isSrv?$('rTimeout').value:'30');"
		"var cl=caidList($('rCaid').value);"
		"if(!cl){rerr('CAID must be hex values of 1-4 digits separated by commas, e.g. 0B00,0B01');$('rCaid').focus();return}"
		"if(cl.length>8){rerr('At most 8 CAIDs per reader');$('rCaid').focus();return}"
		"p.set('group',$('rGroup').value.trim());p.set('caid',cl.join(','));"
		"p.set('sid_whitelist',$('rSid').value.trim());p.set('ecmwhitelist',$('rWl').value.trim());"
		"p.set('ecmkeys',proto==='emu'?$('rKeys').value:'');"
		"p.set('DO_ECM',proto==='pcsc'?($('rDoEcm').checked?'1':'0'):(proto==='internal'?($('rInternalDoEcm').checked?'1':'0'):'1'));"
		"p.set('FAST_RESET',proto==='pcsc'?$('rFast').value:(proto==='internal'?$('rInternalFast').value:'0'));p.set('POLL_MS',proto==='pcsc'?$('rPoll').value:(proto==='internal'?$('rInternalPoll').value:'250'));"
		"p.set('enabled',$('rEnabled').checked?'1':'0');"
		"var b=$('rSave');b.disabled=true;fetch('/api/reader/save',{method:'POST',body:p.toString(),headers:{'Content-Type':'application/x-www-form-urlencoded'}})"
		".then(function(x){return x.json()}).then(function(d){if(d.ok)location.reload();else rerr(d.msg||'Save failed')})"
		".catch(function(){rerr('Request failed')}).finally(function(){b.disabled=false})"
		"}"
		"function deleteReader(i){if(!confirm('Delete this reader?'))return;fetch('/api/reader/delete?index='+i).then(function(x){return x.json()}).then(function(d){if(d.ok)location.reload();else alert(d.msg||'Delete failed')}).catch(function(){alert('Request failed')})}"
		"$('rProtocol').addEventListener('change',syncReaderProtocol);$('rKeys').addEventListener('input',syncCaidPlaceholder);"
		"document.addEventListener('keydown',function(e){if(e.key==='Escape'&&!$('rModal').hidden)closeReader()});"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
	free(reader);
}
