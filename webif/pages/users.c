#define MODULE_LOG_PREFIX "webif"
#include "../service/service.h"
#include "../../src/core/constants.h"
#include "../../src/core/utils.h"
#include "../internal/proto.h"

  
              
  
                                                                             
                                                                            
                                                                        
                                                  
                                                                               
                                                                            
   

                                                                              

static void fmt_int(long long v, char *out, size_t sz)
{
	char tmp[32], o[48];
	int  n = snprintf(tmp, sizeof(tmp), "%lld", v < 0 ? 0 : v), k = 0;
	for (int i = 0; i < n; i++) {
		if (i && (n - i) % 3 == 0) o[k++] = ',';
		o[k++] = tmp[i];
	}
	o[k] = '\0';
	tcmg_strlcpy(out, o, sz);
}

static void fmt_date(time_t t, char *out, size_t sz)
{
	struct tm tm_s;
	localtime_r(&t, &tm_s);
	strftime(out, sz, "%d-%m-%Y", &tm_s);
}

                                                                    
static void fmt_ago(time_t t, time_t now, char *out, size_t sz)
{
	if (t <= 0) { tcmg_strlcpy(out, "never", sz); return; }
	long d = (long)(now - t);
	if (d < 0)            d = 0;
	if (d < 10)           tcmg_strlcpy(out, "just now", sz);
	else if (d < 60)      snprintf(out, sz, "%lds ago", d);
	else if (d < 3600)    snprintf(out, sz, "%ldm ago", d / 60);
	else if (d < 86400)   snprintf(out, sz, "%ldh ago", d / 3600);
	else if (d < 86400L * 45)  snprintf(out, sz, "%ldd ago", d / 86400);
	else if (d < 86400L * 365) snprintf(out, sz, "%ldmo ago", d / (86400L * 30));
	else                  snprintf(out, sz, "%ldy ago", d / (86400L * 365));
}

                                                 
static void fmt_dur(long s, char *out, size_t sz)
{
	if (s < 0) s = 0;
	if (s < 60)         snprintf(out, sz, "%lds", s);
	else if (s < 3600)  snprintf(out, sz, "%ldm %02lds", s / 60, s % 60);
	else if (s < 86400) snprintf(out, sz, "%ldh %02ldm", s / 3600, (s % 3600) / 60);
	else                snprintf(out, sz, "%ldd %ldh", s / 86400, (s % 86400) / 3600);
}

                                                                           
                                                                    
                                                            

                                                                               

void send_page_users(int fd)
{
	PAGE_INIT(65536)

	pos = emit_header(&buf, &bsz, pos, "Users", "users");

	time_t now = time(NULL);

	                                                                           
	int total_u = webif_account_count();
	S_WEBIF_ACCOUNT_VIEW *accounts = total_u > 0 ? calloc((size_t)total_u, sizeof(*accounts)) : NULL;
	int naccounts = accounts ? webif_account_snapshot_all(accounts, (size_t)total_u) : 0;
	int active_u = 0, disabled_u = 0, expired_u = 0, online_u = 0;
	for (int ai = 0; ai < naccounts; ai++) {
		int expd = (accounts[ai].expirationdate > 0 && now > accounts[ai].expirationdate);
		if (!accounts[ai].enabled) disabled_u++;
		if (expd) expired_u++;
		if (accounts[ai].enabled && !expd) active_u++;
		if (accounts[ai].active > 0) online_u++;
	}
	total_u = naccounts;

	                                                           

	                                                                           
	pos = buf_printf(&buf, &bsz, pos,
		"<section class='utoolbar' aria-label='Users toolbar'>"
		"<div class='tgrp' id='uStats'>"
		"<button type='button' class='tool ust act' data-f='all' aria-pressed='true'>All <span class='n' id='cnt_all'>%d</span></button>"
		"<button type='button' class='tool ust' data-f='active' aria-pressed='false'>Active <span class='n' id='cnt_active'>%d</span></button>"
		"<button type='button' class='tool ust' data-f='online' aria-pressed='false'>Online <span class='n' id='cnt_online'>%d</span></button>"
		"<button type='button' class='tool ust' data-f='disabled' aria-pressed='false'>Disabled <span class='n' id='cnt_disabled'>%d</span></button>"
		"<button type='button' class='tool ust' data-f='expired' aria-pressed='false'>Expired <span class='n' id='cnt_expired'>%d</span></button>"
		"</div>"
		"<div class='usrch'>" ICON("i-search")
		"<input id='usrSearch' type='search' placeholder='Search users&hellip;' title='Search user, CAID, IP or protocol (press / )'"
		" autocomplete='off' spellcheck='false' aria-label='Search users'></div>"
		"<div class='tgrp'>"
		"<button type='button' class='tool' data-a='refresh'>" ICON("i-refresh") "Refresh</button>"
		"<button type='button' class='tool pri' data-a='add'>" ICON("i-plus") "Add User</button>"
		"</div></section>",
		total_u, active_u, online_u, disabled_u, expired_u);

	                                                                           
#define TH(cls, key, label) "<th class='" cls "'><button type='button' class='table-sort' data-k='" key "' data-dir=''>" label "<span class='sort-arrow' aria-hidden='true'></span></button></th>"
	pos = buf_printf(&buf, &bsz, pos,
		"<section class='tw cm' id='uTable'><table class='ut' id='usrTable'>"
		"<thead><tr>"
		TH("c-en",   "en",   "On")
		TH("c-user", "user", "User")
		TH("c-conn", "conn", "Conn")
		TH("c-ip",   "ip",   "IP")
		"<th class='c-country'>Country</th>"
		TH("c-caid", "caid", "CAID")
		TH("c-ok",   "ok",   "CW OK")
		TH("c-nok",  "nok",  "CW NOK")
		TH("c-proto","proto","Proto")
		TH("c-idle", "idle", "Idle")
		TH("c-first","first","First login")
		TH("c-last", "last", "Last seen")
		TH("c-exp",  "exp",  "Expiry")
		"<th class='c-btn'>Actions</th>"
		"</tr></thead><tbody id='usrBody'>");
#undef TH

	                                                                           
	S_WEBIF_CLIENT_VIEW *snaps = calloc(MAX_ACTIVE_CLIENTS, sizeof(*snaps));
	int nsnaps = snaps ? webif_client_snapshot_all(snaps, MAX_ACTIVE_CLIENTS) : 0;

	                                                                           
	int row = 0;
	int64_t tot_ok = 0, tot_nok = 0;

	for (int ai = 0; ai < naccounts; ai++, row++) {
		S_WEBIF_ACCOUNT_VIEW *a = &accounts[ai];
		int expired = (a->expirationdate > 0 && now > a->expirationdate);
		const char *state = !a->enabled ? "disabled" : expired ? "expired" : "active";

		int64_t ok = a->cw_found, nok = a->cw_not;
		tot_ok += ok; tot_nok += nok;
		long long avg = ok > 0 ? (long long)(a->cw_time_total_ms / ok) : -1;

		                                        
		int    nsess = 0;
		char   ip_str[MAXIPLEN] = "", proto_raw[12] = "", live_chan[80] = "";
		time_t last_ecm_t = 0, live_t = 0;
		unsigned live_caid = 0, live_sid = 0;
		uint16_t live_all[16];
		int      nlive = 0;
		for (int si = 0; si < nsnaps; si++) {
			if (strcmp(snaps[si].user, a->user) != 0) continue;
			if (!nsess) {
				tcmg_strlcpy(ip_str,    snaps[si].ip,    sizeof(ip_str));
				tcmg_strlcpy(proto_raw, snaps[si].proto, sizeof(proto_raw));
			}
			nsess++;
			if (snaps[si].last_activity > last_ecm_t) last_ecm_t = snaps[si].last_activity;
			if (snaps[si].caid) {
				int dup = 0;
				for (int k = 0; k < nlive; k++) if (live_all[k] == snaps[si].caid) dup = 1;
				if (!dup && nlive < 16) live_all[nlive++] = snaps[si].caid;
				if (snaps[si].last_activity >= live_t) {
					live_t    = snaps[si].last_activity;
					live_caid = snaps[si].caid;
					live_sid  = snaps[si].sid;
					tcmg_strlcpy(live_chan, snaps[si].channel, sizeof(live_chan));
				}
			}
		}

		char allowed[128];
		tcmg_strlcpy(allowed, a->caids, sizeof(allowed));
		long idle_s = (nsess > 0 && last_ecm_t > 0) ? (long)(now - last_ecm_t) : -1;
		if (nsess > 0 && idle_s < 0) idle_s = 0;

		char last_ip[64];
		tcmg_strlcpy(last_ip, a->last_ip, sizeof(last_ip));
		const char *flag_ip = nsess > 0 ? ip_str : last_ip;

		unsigned ipn = 0;
		{ unsigned o[4]; if (sscanf(ip_str, "%u.%u.%u.%u", &o[0], &o[1], &o[2], &o[3]) == 4)
			ipn = ((o[0] & 255u) << 24) | ((o[1] & 255u) << 16) | ((o[2] & 255u) << 8) | (o[3] & 255u); }

		char esc_user[256], esc_ip[64], esc_flag_ip[64], esc_proto[32], q_raw[512], q_esc[1400];
		html_escape(a->user, esc_user, sizeof(esc_user));
		html_escape(ip_str, esc_ip, sizeof(esc_ip));
		html_escape(flag_ip, esc_flag_ip, sizeof(esc_flag_ip));
		html_escape(proto_raw, esc_proto, sizeof(esc_proto));
		char live_s[16] = "";
		if (live_caid) snprintf(live_s, sizeof(live_s), "%04X", live_caid);
		snprintf(q_raw, sizeof(q_raw), "%s %s %s %s %s", a->user, live_s, allowed, ip_str, proto_raw);
		html_escape(q_raw, q_esc, sizeof(q_esc));

		char allowed_esc[256];
		html_escape(allowed, allowed_esc, sizeof(allowed_esc));
		char ok_s[32], nok_s[32];
		fmt_int(ok,  ok_s,  sizeof(ok_s));
		fmt_int(nok, nok_s, sizeof(nok_s));

		                                                                    
                                                                        
                                                                  
                                                                          
		const char *vis = "";
		if (expired) {
			vis = "expired";
		} else if (!a->enabled) {
			vis = "disabled";
		} else if (a->active > 0 && idle_s >= 30) {
			vis = "stale";
		} else if (a->active > 0) {
			vis = "online";
		}

		                                       
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='urow' data-i='%d' data-user='%s' data-state='%s' data-vis='%s' data-en='%d' data-expd='%d'"
			" data-online='%d' data-active='%d' data-caid='%u' data-ok='%lld' data-nok='%lld'"
			" data-avg='%lld' data-proto='%s' data-ipn='%u' data-idle='%ld'"
			" data-first='%lld' data-last='%lld' data-exp='%lld' data-allowed='%s' data-q='%s'>",
			row, esc_user, state, vis, (int)a->enabled, expired,
			a->active > 0 ? 1 : 0, (int)a->active, live_caid,
			(long long)ok, (long long)nok, avg, esc_proto, ipn, idle_s,
			(long long)a->first_login, (long long)a->last_seen, (long long)a->expirationdate, allowed_esc, q_esc);

		pos = buf_printf(&buf, &bsz, pos,
			"<td class='c-en'><input type='checkbox' class='en-box' data-a='tg'%s"
			" title='Enable / disable' aria-label='Account enabled'></td>"
			"<td class='c-user'><button type='button' class='ulink' data-a='edit' title='%s'>%s</button></td>",
			a->enabled ? " checked" : "", esc_user, esc_user);

		                        
		char maxc[16];
		if (a->max_connections <= 0) tcmg_strlcpy(maxc, "&infin;", sizeof(maxc));
		else snprintf(maxc, sizeof(maxc), "%d", (int)a->max_connections);
		int full = (a->max_connections > 0 && a->active >= a->max_connections);
		pos = buf_printf(&buf, &bsz, pos,
			"<td class='c-conn mono' title='Active / maximum connections'><span class='cn%s'>%d</span>"
			"<span class='dim'>/%s</span></td>",
			full ? " full" : a->active > 0 ? " on" : "", (int)a->active, maxc);

		                                      
		if (nsess > 0)
			pos = buf_printf(&buf, &bsz, pos,
				"<td class='c-ip mono'>%s</td>"
				"<td class='c-country'><span class='flg uipflag' data-ip='%s' title=''></span></td>",
				esc_ip, esc_flag_ip);
		else if (esc_flag_ip[0])
			pos = buf_printf(&buf, &bsz, pos,
				"<td class='c-ip dim'>&mdash;</td><td class='c-country'><span class='flg uipflag' data-ip='%s' title='Last known country'></span></td>",
				esc_flag_ip);
		else
			pos = buf_printf(&buf, &bsz, pos,
				"<td class='c-ip dim'>&mdash;</td><td class='c-country dim'>&mdash;</td>");

		                               
		{
			char chan_esc[400], tip[700];
			html_escape(live_chan, chan_esc, sizeof(chan_esc));
			if (live_caid)
				snprintf(tip, sizeof(tip), "Now watching: %s (SID %04X)&#10;Allowed CAIDs: %s",
				         chan_esc[0] ? chan_esc : "unknown channel", live_sid, allowed[0] ? allowed : "none");
			else
				snprintf(tip, sizeof(tip), "%s&#10;Allowed CAIDs: %s",
				         nsess > 0 ? "Online, no ECM received yet" : "Offline", allowed[0] ? allowed : "none");

			if (live_caid) {
				char more[48] = "";
				if (nlive > 1) snprintf(more, sizeof(more), "<span class='more'>+%d</span>", nlive - 1);
				pos = buf_printf(&buf, &bsz, pos,
					"<td class='c-caid mono' title='%s'>%04X%s</td>", tip, live_caid, more);
			} else {
				pos = buf_printf(&buf, &bsz, pos,
					"<td class='c-caid mono dim' title='%s'>&mdash;</td>", tip);
			}
		}

		pos = buf_printf(&buf, &bsz, pos,
			"<td class='c-ok mono tg'>%s</td>"
			"<td class='c-nok mono %s'>%s</td>",
			ok_s, nok > 0 ? "tr" : "dim", nok_s);

		                                  
		if (nsess > 0) {
			char idle_txt[24];
			fmt_dur(idle_s, idle_txt, sizeof(idle_txt));
			pos = buf_printf(&buf, &bsz, pos,
				"<td class='c-proto'><span class='badge bcy'>%s</span></td>"
				"<td class='c-idle mono'>%s</td>",
				esc_proto, idle_txt);
		} else {
			pos = buf_printf(&buf, &bsz, pos,
				"<td class='c-proto dim'>&mdash;</td><td class='c-idle dim'>&mdash;</td>");
		}

		                             
		{
			char seen[40], seen_full[32], first_d[32];
			fmt_ago(a->last_seen, now, seen, sizeof(seen));
			format_time((time_t)a->last_seen, seen_full, sizeof(seen_full));
			if (a->first_login > 0) {
				fmt_date((time_t)a->first_login, first_d, sizeof(first_d));
				pos = buf_printf(&buf, &bsz, pos, "<td class='c-first mono'>%s</td>", first_d);
			} else {
				pos = buf_printf(&buf, &bsz, pos, "<td class='c-first dim'>&mdash;</td>");
			}
			if (a->last_seen > 0)
				pos = buf_printf(&buf, &bsz, pos, "<td class='c-last' title='%s'>%s</td>", seen_full, seen);
			else
				pos = buf_printf(&buf, &bsz, pos, "<td class='c-last dim'>&mdash;</td>");
		}

		                                                                    
		if (a->expirationdate > 0) {
			char ed[16], sub[48];
			const char *dot;
			fmt_date(a->expirationdate, ed, sizeof(ed));
			if (expired) {
				char ago[24];
				fmt_ago(a->expirationdate, now, ago, sizeof(ago));
				snprintf(sub, sizeof(sub), "Expired %s", ago);
				dot = "expired";
			} else {
				long days = (long)((a->expirationdate - now + 86399) / 86400);
				if (days < 1) days = 1;
				snprintf(sub, sizeof(sub), days == 1 ? "Expires in 1 day" : "Expires in %ld days", days);
				dot = days <= 7 ? "expiring" : "valid";
			}
			pos = buf_printf(&buf, &bsz, pos,
				"<td class='c-exp mono' title='%s'>%s<span class='sdot %s' aria-hidden='true'></span></td>",
				sub, ed, dot);
		} else {
			pos = buf_printf(&buf, &bsz, pos, "<td class='c-exp dim' title='No expiry'>&mdash;</td>");
		}

		             
		pos = buf_printf(&buf, &bsz, pos,
			"<td class='c-btn'><div class='ba'>"
			"<button type='button' class='act-b ed' data-a='edit' title='Edit' aria-label='Edit user'>" ICON("i-edit") "</button>"
			"<button type='button' class='act-b rs' data-a='reset' title='Reset statistics' aria-label='Reset statistics'>" ICON("i-reset") "</button>"
			"<button type='button' class='act-b dl' data-a='del' title='Delete' aria-label='Delete user'>" ICON("i-trash") "</button>"
			"</div></td></tr>");
	}
	free(snaps);
	free(accounts);

	                                                                            
	{
		char ok_s[32], nok_s[32];
		fmt_int(tot_ok,  ok_s,  sizeof(ok_s));
		fmt_int(tot_nok, nok_s, sizeof(nok_s));

		pos = buf_printf(&buf, &bsz, pos,
			"</tbody></table>"
			"<div class='uempty' id='uEmpty' hidden>" ICON("i-users")
			"<b id='ueT'>No users</b><span id='ueS'></span><br>"
			"<button type='button' class='tool pri' id='ueB' data-a='add'>Add User</button></div>"
			"</section>"
			"<footer class='ufoot'>"
			"<div class='ucount' id='uCount'>"
			"<span><b>%d</b>%s</span>"
			"<span><b class='tg'>%s</b>CW OK</span>"
			"<span><b class='%s'>%s</b>CW NOK</span>"
			"</div>"
			"<div class='pager-wrap'><div class='pager' id='pager' aria-label='Pagination'></div>"
			"<span class='pstat' id='pageStat' aria-live='polite'></span></div>"
			"<label class='per-page'><select id='limit' aria-label='Users per page'>"
			"<option>30</option><option>50</option><option>100</option><option>200</option></select>"
			"<span>per page</span></label>"
			"</footer>",
			row, row == 1 ? "user" : "users",
			ok_s,
			tot_nok > 0 ? "tr" : "dim", nok_s);
	}

	                                                                           
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='mo' id='uModal' hidden>"
		"<div class='mc user-modal' role='dialog' aria-modal='true' aria-labelledby='umTitle'>"
		"<div class='mh'><h2 id='umTitle'>Edit User</h2>"
		"<button type='button' class='ib' data-a='close' aria-label='Close'>" ICON("i-x") "</button></div>"
		"<div class='mb'>"
		"<input type='hidden' id='em_user'>"
		"<div class='fg' id='em_udisp_wrap'><label class='fld' for='em_udisp'>Username</label>"
		"<input class='fi mono' id='em_udisp' disabled></div>"
		"<div class='fg' id='em_unew_wrap' hidden><label class='fld' for='em_unew'>Username</label>"
		"<input class='fi mono' id='em_unew' placeholder='new_user' autocomplete='off' spellcheck='false'></div>"
		"<div class='fg'><label class='fld' for='em_pass'>Password</label>"
		"<input class='fi mono' id='em_pass' type='text' autocomplete='off' spellcheck='false'></div>"
		"<div class='fg'><label class='fld' for='em_caid'>CAIDs (hex, comma-separated)</label>"
		"<input class='fi mono' id='em_caid' maxlength='64' placeholder='0B00,0B01,0604' autocomplete='off' spellcheck='false' style='text-transform:uppercase'>"
		"<div class='fhint'>One or more CAIDs, up to 9. Empty = none.</div></div>"
		"<div class='user-g3'>"
		"<div class='fg'><label class='fld' for='em_groups'>Reader groups</label>"
		"<input class='fi mono' id='em_groups' placeholder='1,5' autocomplete='off' spellcheck='false'></div>"
		"<div class='fg'><label class='fld' for='em_maxconn'>Max connections</label>"
		"<input class='fi mono' id='em_maxconn' type='number' min='0' value='0' placeholder='0 = unlimited'></div>"
		"</div>"

		"<div class='fg' style='margin-top:2px'>"
		"<label class='user-enable'><span>Anti-sharing protection</span>"
		"<button type='button' class='sw' id='em_as_enabled' role='switch' aria-checked='false'"
		" aria-label='Anti-sharing protection' data-a='sw-as'><i></i></button></label>"
		"</div>"
		"<div class='user-g3' id='em_as_fields' hidden>"
		"<div class='fg'><label class='fld' for='em_as_sids'>Max active channels</label>"
		"<input class='fi mono' id='em_as_sids' type='number' min='1' max='32' value='1'><div class='fhint'>Active client/channel entries kept by the protection timeout.</div></div>"
		"<div class='fg'><label class='fld' for='em_as_ecm'>Max ECM requests</label>"
		"<input class='fi mono' id='em_as_ecm' type='number' min='0' max='100000' value='0' placeholder='0 = no limit'><div class='fhint'>Incoming ECM requests allowed during the window.</div></div>"
		"<div class='fg'><label class='fld' for='em_as_window'>ECM rate window (seconds)</label>"
		"<input class='fi mono' id='em_as_window' type='number' min='1' max='3600' value='60'></div>"
		"<div class='fg'><label class='fld' for='em_as_timeout'>Active channel timeout (seconds)</label>"
		"<input class='fi mono' id='em_as_timeout' type='number' min='1' max='3600' value='15'></div>"
		"<div class='fg'><label class='fld' for='em_as_delay'>Channel switch delay (seconds)</label>"
		"<input class='fi mono' id='em_as_delay' type='number' min='0' max='30' value='1'></div>"
		"<div class='fhint'>Optional delay applied to the CW returned after a channel switch. This is separate from the rate and channel limits.</div>"
		"</div>"

		"<div class='user-bottom-grid'>"
		"<div class='fg'><label class='fld' for='em_expiry'>Expiry date</label>"
		"<input class='fi mono' id='em_expiry' type='date'>"
		"<div class='chips'>"
		"<button type='button' class='chip2' data-a='exp' data-n='30'>+30 days</button>"
		"<button type='button' class='chip2' data-a='exp' data-n='90'>+90 days</button>"
		"<button type='button' class='chip2' data-a='exp' data-n='365'>+1 year</button>"
		"<button type='button' class='chip2' data-a='exp' data-n='0'>Never</button>"
		"</div></div>"
		"<label class='user-enable'><span>Account enabled</span>"
		"<button type='button' class='sw on' id='em_enabled' role='switch' aria-checked='true' aria-label='Account enabled' data-a='sw'><i></i></button>"
		"</label></div>"

		"<div id='em_err' class='le' hidden>" ICON("i-alert") "<span id='em_err_msg'>Error</span></div>"
		"</div>"
		"<div class='mf'>"
		"<button type='button' class='btn bg' data-a='close'>Cancel</button>"
		"<button type='button' class='btn bp' id='em_saveBtn' data-a='save'>Save</button>"
		"</div></div></div>");

	                                                                              
	pos = buf_printf(&buf, &bsz, pos,
		"<script src='/assets/users.js?v='" TCMG_VERSION " defer></script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}
