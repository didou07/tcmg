#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"


void send_page_users(int fd)
{
	PAGE_INIT(65536)

	pos = emit_header(&buf, &bsz, pos, "Users", "users");

	int total_u = 0, active_u = 0, disabled_u = 0, expired_u = 0, online_u = 0;
	time_t now_u = time(NULL);
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		total_u++;
		if (!a->enabled) disabled_u++;
		else if (a->expirationdate > 0 && now_u > a->expirationdate) expired_u++;
		else active_u++;
		if (a->active > 0) online_u++;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='sbar'>"
		"<div class='sbar-item'><div class='sbl'>Total</div>"
		"  <div class='sbv tb'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Active</div>"
		"  <div class='sbv tg'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Online</div>"
		"  <div class='sbv%s'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Disabled</div>"
		"  <div class='sbv%s'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Expired</div>"
		"  <div class='sbv%s'>%d</div></div>"
		"</div>",
		total_u, active_u,
		online_u   > 0 ? " tg" : "", online_u,
		disabled_u > 0 ? " to" : "", disabled_u,
		expired_u  > 0 ? " tr" : "", expired_u);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='ttb' style='display:flex;align-items:center;gap:8px'>"
		"<input class='tsrch' id='usrSearch' placeholder='Search users...' "
		"oninput='filterUsers()' autocomplete='off'>"
		"<button class='btn bg sm' onclick='location.reload()' style='flex-shrink:0'>"
		ICO_RELOAD "&nbsp;Refresh</button>"
		"<button class='btn bp sm' onclick=\"openAddUser()\" style='flex-shrink:0'>"
		"Add User</button>"
		"</div>");

	S_SERVER_STATS st = collect_stats();
	(void)st;

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='tw'><table id='usrTable'>"
		"<thead><tr>"
		"<th style='width:36px'></th>"
		"<th class='sortable' onclick='sortTable(1)'>User</th>"
		"<th class='sortable' onclick='sortTable(2)'>CAID</th>"
		"<th class='sortable' onclick='sortTable(3)' title='Active connections'>Active</th>"
		"<th>Max</th>"
		"<th>CW OK / NOK</th>"
		"<th class='sortable' onclick='sortTable(6)' title='Hit rate %%'>Rate</th>"
		"<th class='sortable' onclick='sortTable(7)' title='Average ECM response ms'>Avg&nbsp;ms</th>"
		"<th title='Min/Max ECM response ms'>Min/Max</th>"
		"<th class='sortable' onclick='sortTable(9)'>Proto</th>"
		"<th>IP</th>"
		"<th class='sortable' onclick='sortTable(11)'>Idle</th>"
		"<th class='sortable' onclick='sortTable(12)'>First Login</th>"
		"<th class='sortable' onclick='sortTable(13)'>Last Seen</th>"
		"<th>Expiry</th><th></th>"
		"</tr></thead><tbody id='usrBody'>");

	typedef struct { char user[64]; char ip[MAXIPLEN]; char proto[12]; time_t last_ecm; } cl_snap;
	cl_snap *snaps = (cl_snap *)calloc(MAX_ACTIVE_CLIENTS, sizeof(cl_snap));
	int nsnaps = 0;
	if (snaps) {
		pthread_mutex_lock(&g_clients_mtx);
		for (int ci = 0; ci < MAX_ACTIVE_CLIENTS; ci++) {
			S_CLIENT *cl = g_clients[ci];
			if (!cl || !cl->account) continue;
			tcmg_strlcpy(snaps[nsnaps].user,  cl->account->user, 64);
			tcmg_strlcpy(snaps[nsnaps].ip,    cl->ip, MAXIPLEN);
			tcmg_strlcpy(snaps[nsnaps].proto, cl->proto[0] ? cl->proto : "unknown", 12);
			snaps[nsnaps].last_ecm = cl->last_ecm_time;
			nsnaps++;
		}
		pthread_mutex_unlock(&g_clients_mtx);
	}

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	int row = 0;
	int64_t tot_cw_ok = 0, tot_cw_nok = 0;
	for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next, row++) {
		char last[32], expiry[128];
		format_time((time_t)a->last_seen, last, sizeof(last));

		if (a->expirationdate > 0) {
			format_time(a->expirationdate, expiry, sizeof(expiry));
			if (time(NULL) > a->expirationdate)
				snprintf(expiry, sizeof(expiry),
				         "<span class='badge bban'>EXPIRED</span>");
		} else {
			snprintf(expiry, sizeof(expiry), "<span class='tm'>&mdash;</span>");
		}

		int64_t tot = a->cw_found + a->cw_not;
		double  hr  = tot > 0 ? (double)a->cw_found * 100.0 / (double)tot : -1.0;
		char hrstr[16], avgstr[16], minmaxstr[32];
		if (hr >= 0) snprintf(hrstr,  sizeof(hrstr),  "%.1f%%", hr);
		else         tcmg_strlcpy(hrstr, "&mdash;", sizeof(hrstr));
		if (a->cw_found > 0)
			snprintf(avgstr, sizeof(avgstr), "%lld",
			         (long long)(a->cw_time_total_ms / a->cw_found));
		else
			tcmg_strlcpy(avgstr, "&mdash;", sizeof(avgstr));
		if (a->cw_found > 0 && a->cw_time_min_ms > 0)
			snprintf(minmaxstr, sizeof(minmaxstr), "%lld / %lld",
			         (long long)a->cw_time_min_ms,
			         (long long)a->cw_time_max_ms);
		else
			tcmg_strlcpy(minmaxstr, "&mdash;", sizeof(minmaxstr));

		const char *proto_str = "&mdash;";
		char ip_str[MAXIPLEN] = "";
		time_t last_ecm_t = 0;
		char first_login_str[32];
		format_time((time_t)a->first_login, first_login_str, sizeof(first_login_str));

		if (snaps) {
			for (int si = 0; si < nsnaps; si++) {
				if (strcmp(snaps[si].user, a->user) != 0) continue;
				proto_str = snaps[si].proto;
				if (!ip_str[0]) tcmg_strlcpy(ip_str, snaps[si].ip, sizeof(ip_str));
				if (snaps[si].last_ecm > last_ecm_t) last_ecm_t = snaps[si].last_ecm;
			}
		}

		char idle_str[32];
		if (a->active > 0 && last_ecm_t > 0) {
			time_t idle_s = time(NULL) - last_ecm_t;
			if (idle_s < 0) idle_s = 0;
			format_uptime(idle_s, idle_str, sizeof(idle_str));
		} else {
			tcmg_strlcpy(idle_str, "&mdash;", sizeof(idle_str));
		}

		const char *btn_cls = a->enabled ? "pw-btn on" : "pw-btn off";
		char esc_user_attr[256], esc_user_html[256];
		html_escape(a->user, esc_user_attr, sizeof(esc_user_attr));
		tcmg_strlcpy(esc_user_html, esc_user_attr, sizeof(esc_user_html));
		tot_cw_ok  += a->cw_found;
		tot_cw_nok += a->cw_not;

		double bar_w = hr >= 0 ? hr : 0.0;

		char maxconn_str[16];
		if (a->max_connections <= 0)
			tcmg_strlcpy(maxconn_str, "&infin;", sizeof(maxconn_str));
		else
			snprintf(maxconn_str, sizeof(maxconn_str), "%d", (int)a->max_connections);

		pos = buf_printf(&buf, &bsz, pos,
			"<tr data-user='%s'>"
			"<td><button class='%s' id='pw_%.32s' onclick='tgUser(this)' title='Toggle'>"
			"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"</svg></button></td>"
			"<td><div class='flex gap8'>"
			"  <span class='u-link bold' onclick='editUser(this)'>%s</span>"
			"</div></td>"
			"<td class='mono'><span class='badge bbl'>%04X</span></td>"
			"<td class='mono%s'>%d</td>"
			"<td class='mono tm'>%s</td>"
			"<td>"
			"  <div class='msr'>"
			"    <span class='msr-kv'><span class='msr-k'>OK</span>"
			"      <span class='msr-v tg'>%lld</span></span>"
			"    <span class='msr-kv'><span class='msr-k'>NOK</span>"
			"      <span class='msr-v%s'>%lld</span></span>"
			"  </div>"
			"  <div class='hbw' style='margin-top:4px'>"
			"<div class='hbf' style='width:%.0f%%'></div></div>"
			"</td>"
			"<td class='mono'><span class='%s'>%s</span></td>"
			"<td class='mono tm'>%s</td>"
			"<td class='mono tm' style='font-size:11px'>%s</td>"
			"<td class='mono'><span class='%s' style='font-size:11px'>%s</span></td>"
			"<td class='mono' style='font-size:11px;color:var(--t1)'>%s</td>"
			"<td class='mono tm' style='font-size:11px'>%s</td>"
			"<td class='mono tm' style='font-size:11px'>%s</td>"
			"<td class='mono tm' style='font-size:11px'>%s</td>"
			"<td style='font-size:12px'>%s</td>"
			"<td>"
			"  <div style='display:flex;gap:4px'>"
			"    <button class='btn bg sm' onclick='resetStats(this)' title='Reset stats' "
			"      data-u='%s' style='padding:4px 7px;font-size:11px'>&#8635;</button>"
			"    <button class='btn bd_ sm' onclick='delUser(this)' data-u='%s' title='Delete'>"
			ICO_KILLBTN "</button>"
			"  </div>"
			"</td>"
			"</tr>",
			esc_user_attr,
			btn_cls, esc_user_attr, esc_user_html,
			(unsigned int)a->caid,
			a->active > 0 ? " tg bold" : "", (int)a->active,
			maxconn_str,
			(long long)a->cw_found,
			a->cw_not > 0 ? " tr" : "", (long long)a->cw_not,
			bar_w,
			hr > 80.0 ? "tg" : hr >= 0 ? (hr > 50.0 ? "to" : "tr") : "tm",
			hrstr,
			avgstr,
			minmaxstr,

			a->active > 0 ? "badge bcy" : "tm", proto_str,

			ip_str[0] ? ip_str : "&mdash;",

			idle_str,

			first_login_str,

			last, expiry, a->user, a->user);
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	free(snaps);

	if (!row)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='erow'><td colspan='11'>"
			"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'"
			" style='vertical-align:-3px;margin-right:6px;opacity:.35'>"
			"<path d='M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2'/><circle cx='9' cy='7' r='4'/>"
			"</svg>No accounts configured"
			"</td></tr>");

	{
		int64_t tot_all = tot_cw_ok + tot_cw_nok;
		double  tot_hr  = tot_all > 0 ? (double)tot_cw_ok * 100.0 / tot_all : 0.0;
		pos = buf_printf(&buf, &bsz, pos,
			"</tbody>"
			"<tfoot><tr>"
			"<td colspan='5'>"
			"  <span class='tfl'>Totals &mdash; %d users</span>"
			"</td>"
			"<td>"
			"  <span class='tg tfs'>%ld</span>"
			"  <span style='color:var(--t2);margin:0 4px'>/</span>"
			"  <span class='%s tfs'>%ld</span>"
			"</td>"
			"<td><span class='tfs %s'>%.1f%%</span></td>"
			"<td colspan='9'></td>"
			"</tr></tfoot>"
			"</table></div>",
			row,
			(long)tot_cw_ok,
			tot_cw_nok > 0 ? "tr" : "tm", (long)tot_cw_nok,
			tot_hr > 80 ? "tg" : tot_hr > 50 ? "to" : "tr",
			tot_hr);
	}

	pos = buf_printf(&buf, &bsz, pos,

		"<div id='uModal' style='display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);"
		"z-index:2000;align-items:center;justify-content:center'>"
		"<div class='card' style='width:380px;max-width:95vw;padding:24px'>"
		"  <div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:16px'>"
		"    <span id='umTitle' style='font-weight:700;font-size:15px'>Edit User</span>"
		"    <button class='btn bg sm' onclick='closeUM()'>&#10005;</button>"
		"  </div>"
		"  <input type='hidden' id='em_user'>"
		"  <div id='em_udisp_wrap' class='fg'><label class='fld'>USERNAME</label>"
		"    <input class='fi mono' id='em_udisp' disabled style='opacity:.6'></div>"
		"  <div id='em_unew_wrap' class='fg' style='display:none'><label class='fld'>USERNAME</label>"
		"    <input class='fi mono' id='em_unew' placeholder='new_user' autocomplete='off'></div>"
		"  <div class='fg'><label class='fld'>PASSWORD</label>"
		"    <input class='fi mono' id='em_pass' type='text' autocomplete='off'></div>"
		"  <div style='display:grid;grid-template-columns:1fr 1fr;gap:10px'>"
		"    <div class='fg'><label class='fld'>CAID (hex)</label>"
		"      <input class='fi mono' id='em_caid' maxlength='4' placeholder='09B5'></div>"
		"    <div class='fg'><label class='fld'>MAX CONN (0=&infin;)</label>"
		"      <input class='fi mono' id='em_maxconn' type='number' min='0' value='0'></div>"
		"  </div>"
		"  <div class='fg'><label class='fld'>EXPIRY DATE (YYYY-MM-DD, 0=never)</label>"
		"    <input class='fi mono' id='em_expiry' placeholder='2030-12-31'></div>"
		"  <div style='display:flex;align-items:center;gap:10px;margin:12px 0'>"
		"    <label style='font-size:12px;font-weight:700;color:var(--t2);"
		"text-transform:uppercase;letter-spacing:.08em'>Enabled</label>"
		"    <input type='checkbox' id='em_enabled' checked>"
		"  </div>"
		"  <div id='em_err' style='display:none' class='le'>"
		"    <svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"<circle cx='12' cy='12' r='10'/><line x1='12' y1='8' x2='12' y2='12'/>"
		"<line x1='12' y1='16' x2='12.01' y2='16'/></svg>"
		"    <span id='em_err_msg'>Error</span>"
		"  </div>"
		"  <div style='display:flex;gap:8px;margin-top:16px'>"
		"    <button class='btn bp sm' id='em_saveBtn' style='flex:1;justify-content:center'"
		"     onclick='saveUM()'>Save</button>"
		"    <button class='btn bg sm' style='flex:1;justify-content:center'"
		"     onclick='closeUM()'>Cancel</button>"
		"  </div>"
		"</div></div>"
		"<script>"
		"function resetStats(btn){"
		"  var u=btn.dataset.u||btn.closest('tr').dataset.user;"
		"  if(!confirm('Reset stats for '+u+'?'))return;"
		"  fetch('/api/user/resetstats?user='+encodeURIComponent(u))"
		"    .then(r=>r.json())"
		"    .then(d=>{if(d.ok)location.reload();else alert('Error: '+d.msg);});"
		"}"

		"function tgUser(btn){"
		"  var u=btn.closest('tr').dataset.user;"
		"  fetch('/api/user/toggle?user='+encodeURIComponent(u))"
		"    .then(r=>r.json())"
		"    .then(d=>{"
		"      if(!d.ok)return;"
		"      btn.className='pw-btn '+(d.enabled?'on':'off');"
		"    });"
		"}"

		"function editUser(el){"
		"  var u=el.closest('tr').dataset.user;"
		"  fetch('/api/user/get?user='+encodeURIComponent(u))"
		"    .then(r=>r.json())"
		"    .then(d=>{"
		"      document.getElementById('umTitle').textContent='Edit User';"
		"      document.getElementById('em_unew_wrap').style.display='none';"
		"      document.getElementById('em_udisp_wrap').style.display='';"
		"      document.getElementById('em_user').value=d.user;"
		"      document.getElementById('em_udisp').value=d.user;"
		"      document.getElementById('em_pass').value=d.pass||'';"
		"      document.getElementById('em_caid').value=d.caid;"
		"      document.getElementById('em_maxconn').value=d.max_connections;"
		"      document.getElementById('em_enabled').checked=!!d.enabled;"
		"      document.getElementById('em_expiry').value=d.expiry||'';"
		"      document.getElementById('em_err').style.display='none';"
		"      document.getElementById('uModal').style.display='flex';"
		"    });"
		"}"

		"function openAddUser(){"
		"  document.getElementById('umTitle').textContent='Add User';"
		"  document.getElementById('em_unew_wrap').style.display='';"
		"  document.getElementById('em_udisp_wrap').style.display='none';"
		"  document.getElementById('em_user').value='';"
		"  document.getElementById('em_unew').value='';"
		"  document.getElementById('em_pass').value='';"
		"  document.getElementById('em_caid').value='';"
		"  document.getElementById('em_maxconn').value='0';"
		"  document.getElementById('em_enabled').checked=true;"
		"  document.getElementById('em_expiry').value='';"
		"  document.getElementById('em_err').style.display='none';"
		"  document.getElementById('uModal').style.display='flex';"
		"  setTimeout(()=>document.getElementById('em_unew').focus(),80);"
		"}"
		"function closeUM(){document.getElementById('uModal').style.display='none';}"
		"function delUser(btn){"
		"  var u=btn.dataset.u||btn.closest('tr').dataset.user;"
		"  if(!confirm('Delete user '+u+'?\\nThis cannot be undone.'))return;"
		"  fetch('/api/user/delete?user='+encodeURIComponent(u))"
		"    .then(r=>r.json())"
		"    .then(d=>{if(d.ok)location.reload();else alert('Error: '+d.msg);});"
		"}"
		"function saveUM(){"
		"  var isAdd=!document.getElementById('em_user').value;"
		"  var username=isAdd"
		"    ?document.getElementById('em_unew').value.trim()"
		"    :document.getElementById('em_user').value;"
		"  if(!username){document.getElementById('em_err_msg').textContent='Username required';"
		"    document.getElementById('em_err').style.display='flex';return;}"
		"  var p=new URLSearchParams();"
		"  p.set('user',username);"
		"  p.set('pass',document.getElementById('em_pass').value);"
		"  p.set('caid',document.getElementById('em_caid').value||'0');"
		"  p.set('maxconn',document.getElementById('em_maxconn').value||'0');"
		"  p.set('enabled',document.getElementById('em_enabled').checked?'1':'0');"
		"  p.set('expiry',document.getElementById('em_expiry').value||'0');"
		"  var url=isAdd?'/api/user/add':'/api/user/save';"
		"  fetch(url,{method:'POST',body:p.toString(),"
		"    headers:{'Content-Type':'application/x-www-form-urlencoded'}})"
		"    .then(r=>r.json())"
		"    .then(d=>{if(d.ok){closeUM();location.reload();}"
		"      else{document.getElementById('em_err_msg').textContent=d.msg||'Error';"
		"           document.getElementById('em_err').style.display='flex';}});"
		"}"

		"function filterUsers(){"
		"  var q=document.getElementById('usrSearch').value.toLowerCase();"
		"  var rows=document.getElementById('usrBody').rows;"
		"  for(var i=0;i<rows.length;i++){"
		"    var u=(rows[i].getAttribute('data-user')||'').toLowerCase();"
		"    rows[i].style.display=(!q||u.includes(q))?'':'none';"
		"  }"
		"}"

		"var _sortCol=-1,_sortDir=1;"
		"function sortTable(col){"
		"  var tb=document.getElementById('usrTable');"
		"  var ths=tb.querySelectorAll('th');"
		"  ths.forEach(function(t){t.classList.remove('sort-asc','sort-desc');});"
		"  if(_sortCol===col){_sortDir*=-1;}else{_sortCol=col;_sortDir=1;}"
		"  ths[col].classList.add(_sortDir===1?'sort-asc':'sort-desc');"
		"  var body=document.getElementById('usrBody');"
		"  var rows=Array.from(body.rows);"
		"  rows.sort(function(a,b){"
		"    var av=a.cells[col]?a.cells[col].textContent.trim():'';"
		"    var bv=b.cells[col]?b.cells[col].textContent.trim():'';"
		"    var an=parseFloat(av),bn=parseFloat(bv);"
		"    var cmp=isNaN(an)||isNaN(bn)?av.localeCompare(bv):an-bn;"
		"    return cmp*_sortDir;"
		"  });"
		"  rows.forEach(function(r){body.appendChild(r);});"
		"}"
		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}

