#define MODULE_LOG_PREFIX "webif"
#include "tcmg-globals.h"
#include "tcmg-crypto.h"
#include "tcmg-log.h"
#include "tcmg-webif-internal.h"

/* ────────────────────────────────────────────────────────────────────────────
 * tcmg-webif-pages.c  —  HTML pages + JSON API handlers
 *
 * OSCam philosophy applied:
 *   · emit_stat_card() eliminates 7× duplicated stat-card markup
 *   · file_read_escaped() (common.c) eliminates 2× duplicated file-read+escape
 *   · send_page_action()  eliminates duplicate Shutdown/Restart pages
 *   · form_get()          now calls url_decode() (no more dual decode logic)
 *   · send_api_status()   JSON correctly wrapped in { }
 *   · send_api_user_get() no longer leaks password hash
 *   · tcmg_build_path()   used for all path construction
 * ────────────────────────────────────────────────────────────────────────────*/

/* ════════════════════════════════════════════════════════════════════════════
 * Private helpers
 * ════════════════════════════════════════════════════════════════════════════*/

/*
 * emit_stat_card — render one card in the dashboard grid.
 *
 *   color  : CSS class suffix ("bl","gr","vi","cy","re","or")
 *   icon   : ICO_* macro string
 *   label  : card header label (ALL-CAPS)
 *   sv_id  : DOM id for the value span (NULL = no id)
 *   value  : initial value text
 *   sub    : small sub-text below value (HTML allowed)
 *   extra  : optional HTML injected after the value div (progress bar etc.),
 *            NULL for none
 */
static int emit_stat_card(char **buf, int *bsz, int pos,
                          const char *color, const char *icon,
                          const char *label, const char *sv_id,
                          const char *value, const char *sub,
                          const char *extra)
{
	pos = buf_printf(buf, bsz, pos,
		"<div class='sc %s'>"
		"<div class='si_'>%s</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>%s</div>"
		"  <div class='sv'%s%s%s>%s</div>"
		"%s"
		"  <div class='sd'>%s</div>"
		"</div><div class='sg'></div></div>",
		color, icon, label,
		sv_id ? " id='" : "", sv_id ? sv_id : "", sv_id ? "'" : "",
		value,
		extra ? extra : "",
		sub);
	return pos;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Login page
 * ════════════════════════════════════════════════════════════════════════════*/
void send_login_page(int fd, int failed)
{
	int   bsz = 8192, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = buf_printf(&buf, &bsz, pos,
		"<!DOCTYPE html><html lang='en'><head>"
		"<meta charset='UTF-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>TCMG &mdash; Login</title>"
		"<style>%s</style>"
		"</head><body>",
		CSS);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='lb'><div class='lcard'>"
		"<div class='ll'>"
		"  <div class='lli'>"
		"    <svg width='26' height='26' viewBox='0 0 24 24' fill='none'>"
		"      <path d='M12 2L2 7l10 5 10-5-10-5z' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>"
		"      <path d='M2 17l10 5 10-5' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>"
		"      <path d='M2 12l10 5 10-5' stroke='var(--cy)' stroke-width='1.8' stroke-linejoin='round'/>"
		"    </svg>"
		"  </div>"
		"  <div>"
		"    <div class='llt'>TCMG</div>"
		"    <div class='llv'>" TCMG_VERSION " &bull; Web Interface</div>"
		"  </div>"
		"</div>");

	if (failed)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='le'>"
			"<svg width='14' height='14' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<circle cx='12' cy='12' r='10'/>"
			"<line x1='12' y1='8' x2='12' y2='12'/>"
			"<line x1='12' y1='16' x2='12.01' y2='16'/>"
			"</svg>"
			"Invalid credentials &mdash; please try again."
			"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<form method='POST' action='/login'>"
		"<div class='fg'>"
		"  <label class='fld'>USERNAME</label>"
		"  <input class='fi' type='text' name='u' placeholder='Enter username' autofocus autocomplete='username'>"
		"</div>"
		"<div class='fg'>"
		"  <label class='fld'>PASSWORD</label>"
		"  <input class='fi' type='password' name='p' placeholder='Enter password' autocomplete='current-password'>"
		"</div>"
		"<button type='submit' class='btn bp' style='width:100%%;justify-content:center;padding:11px;margin-top:4px'>"
		"<svg width='15' height='15' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"<path d='M15 3h4a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-4'/>"
		"<polyline points='10 17 15 12 10 7'/><line x1='15' y1='12' x2='3' y2='12'/>"
		"</svg>"
		"Sign In</button>"
		"</form>"
		"</div></div>"
		"</body></html>");

	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Dashboard (status page)
 * ════════════════════════════════════════════════════════════════════════════*/
void send_page_status(int fd)
{
	int   bsz = 32768, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Dashboard", "status");

	S_SERVER_STATS st = collect_stats();
	char hrstr[16];
	snprintf(hrstr, sizeof(hrstr),
	         st.ecm_total > 0 ? "%.1f%%" : "0.0%%", st.hit_rate);

	/* Page header with action buttons */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='ph'>"
		"  <div class='ha'>"
		"    <a href='#'"
		"       onclick=\"if(confirm('Reset all stats?')){fetch('/api/resetstats').then(()=>location.reload());}return false\""
		"       class='btn bg sm'>" ICO_RESET "&nbsp;Reset Stats</a>"
		"  </div>"
		"</div>");

	/* ── Stat cards grid ── */
	char hbf_extra[192];
	snprintf(hbf_extra, sizeof(hbf_extra),
	         "  <div class='hbw' style='margin-top:4px'>"
	         "<div class='hbf' id='p_hbf' style='width:%.0f%%'></div></div>",
	         st.hit_rate);

	pos = buf_printf(&buf, &bsz, pos, "<div class='cg'>");

	pos = emit_stat_card(&buf, &bsz, pos, "bl", ICO_CLOCK,
	    "Uptime",       "p_up",   st.uptime_str,           "since start",        NULL);

	{
		char conn_val[16], conn_sub[48];
		snprintf(conn_val, sizeof(conn_val), "%d", st.active_conns);
		snprintf(conn_sub, sizeof(conn_sub), "of <span id='p_acc'>%d</span> accounts", st.naccounts);
		pos = emit_stat_card(&buf, &bsz, pos,
		    st.active_conns > 0 ? "gr" : "bl", ICO_USERS2,
		    "Connections", "p_conn", conn_val, conn_sub, NULL);
	}

	{
		char ecm_val[24];
		snprintf(ecm_val, sizeof(ecm_val), "%lld", (long long)st.ecm_total);
		pos = emit_stat_card(&buf, &bsz, pos, "vi", ICO_ZAP,
		    "ECM Total",   "p_ecm",  ecm_val, "requests processed", NULL);
	}

	{
		char cw_val[24];
		snprintf(cw_val, sizeof(cw_val), "%lld", (long long)st.cw_found);
		pos = emit_stat_card(&buf, &bsz, pos, "gr", ICO_CHECK,
		    "CW Found",    "p_hit",  cw_val,  "cache hits",         NULL);
	}

	{
		char miss_val[24];
		snprintf(miss_val, sizeof(miss_val), "%lld", (long long)st.cw_not);
		pos = emit_stat_card(&buf, &bsz, pos,
		    st.cw_not > 0 ? "re" : "bl", ICO_X,
		    "CW Miss",     "p_miss", miss_val, "not found",         NULL);
	}

	pos = emit_stat_card(&buf, &bsz, pos, "cy", ICO_PERCENT,
	    "Hit Rate",    "p_hr",   hrstr, "", hbf_extra);

	{
		char ban_val[8];
		snprintf(ban_val, sizeof(ban_val), "%d", st.nbans);
		pos = emit_stat_card(&buf, &bsz, pos,
		    st.nbans > 0 ? "or" : "bl", ICO_SHIELD,
		    "Banned IPs",  "p_ban",  ban_val,
		    "<a href='/failban' style='color:inherit;opacity:.7;font-size:11px'>view list &rarr;</a>",
		    NULL);
	}

	pos = buf_printf(&buf, &bsz, pos, "</div>"); /* /cg */

	/* ── ECM Breakdown + Users Summary bar (OSCam-style info section) ── */
	{
		int total_users = 0, disabled_u = 0, expired_u = 0;
		time_t now = time(NULL);
		pthread_rwlock_rdlock(&g_cfg.acc_lock);
		for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
			total_users++;
			if (!a->enabled) disabled_u++;
			if (a->expirationdate > 0 && now > a->expirationdate) expired_u++;
		}
		pthread_rwlock_unlock(&g_cfg.acc_lock);

		/* ECM stacked bar */
		double ok_pct  = st.ecm_total > 0 ? (double)st.cw_found * 100.0 / st.ecm_total : 0.0;
		double nok_pct = st.ecm_total > 0 ? (double)st.cw_not   * 100.0 / st.ecm_total : 0.0;

		char ok_pct_s[16], nok_pct_s[16];
		snprintf(ok_pct_s,  sizeof(ok_pct_s),  "%.1f%%", ok_pct);
		snprintf(nok_pct_s, sizeof(nok_pct_s), "%.1f%%", nok_pct);

		pos = buf_printf(&buf, &bsz, pos,
			/* ── Users summary bar ── */
			"<div class='sbar'>"
			"<div class='sbar-item'>"
			"  <div class='sbl'>Total Users</div>"
			"  <div class='sbv tb'>%d</div>"
			"</div>"
			"<div class='sbar-item'>"
			"  <div class='sbl'>Connected</div>"
			"  <div class='sbv%s'>%d</div>"
			"</div>"
			"<div class='sbar-item'>"
			"  <div class='sbl'>Disabled</div>"
			"  <div class='sbv%s'>%d</div>"
			"</div>"
			"<div class='sbar-item'>"
			"  <div class='sbl'>Expired</div>"
			"  <div class='sbv%s'>%d</div>"
			"</div>"
			"<div class='sbar-item'>"
			"  <div class='sbl'>ECM OK</div>"
			"  <div class='sbv tg'>%s</div>"
			"  <div class='ecm-bk'>"
			"    <span class='ecm-ok' style='width:%.0f%%'></span>"
			"    <span class='ecm-nok' style='width:%.0f%%'></span>"
			"  </div>"
			"</div>"
			"<div class='sbar-item'>"
			"  <div class='sbl'>ECM NOK</div>"
			"  <div class='sbv tr'>%s</div>"
			"</div>"
			"<div class='sbar-item'>"
			"  <div class='sbl'>Total ECM</div>"
			"  <div class='sbv sm'>%lld</div>"
			"</div>"
			"</div>",
			total_users,
			st.active_conns > 0 ? " tg" : "", st.active_conns,
			disabled_u > 0 ? " to" : "", disabled_u,
			expired_u > 0  ? " tr" : "", expired_u,
			ok_pct_s,  ok_pct, nok_pct,
			nok_pct_s,
			(long long)st.ecm_total);
	}
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='shd'>"
		"  <div class='stl'>"
		"    <svg width='15' height='15' viewBox='0 0 24 24' fill='none' stroke='var(--p)' stroke-width='1.8' style='flex-shrink:0'>"
		"      <rect x='2' y='3' width='20' height='14' rx='2'/>"
		"      <line x1='8' y1='21' x2='16' y2='21'/><line x1='12' y1='17' x2='12' y2='21'/>"
		"    </svg>Active Connections"
		"  </div>"
		"</div>"
		"<div class='tw'><table>"
		"<thead><tr>"
		"<th>User</th><th>IP Address</th><th>CAID</th>"
		"<th>SID</th><th>Channel</th><th>Connected</th><th>Idle</th><th></th>"
		"</tr></thead>"
		"<tbody id='p_clients'>"
		"<tr class='erow'><td colspan='8'>"
		"<div class='pulse sm' style='display:inline-block;margin-right:8px'></div>Loading&hellip;"
		"</td></tr>"
		"</tbody></table></div>");

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Users page
 * ════════════════════════════════════════════════════════════════════════════*/
void send_page_users(int fd)
{
	int   bsz = 65536, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Users", "users");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='ph'>"
		"  <div class='ha'>"
		"    <button class='btn bg sm' onclick='location.reload()'>"
		ICO_RELOAD "&nbsp;Refresh</button>"
		"    <button class='btn bp sm' onclick=\"openAddUser()\">"
		"<svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"<line x1='12' y1='5' x2='12' y2='19'/><line x1='5' y1='12' x2='19' y2='12'/>"
		"</svg>&nbsp;Add User</button>"
		"  </div>"
		"</div>");

	/* Count totals for summary bar */
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

	/* ── Users summary bar ── */
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

	/* ── Table toolbar: search + Add User button ── */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='ttb'>"
		"<input class='tsrch' id='usrSearch' placeholder='&#128269; Search users...' "
		"oninput='filterUsers()' autocomplete='off'>"
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

	/* snapshot clients once to avoid N mutex locks inside acc_lock */
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
			tcmg_strlcpy(snaps[nsnaps].proto, cl->is_mgcamd ? "mgcamd" : "newcamd", 12);
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

		/* Protocol + IP from active connections */
		const char *proto_str = "&mdash;";
		char ip_str[MAXIPLEN] = "";
		time_t last_ecm_t = 0;
		char first_login_str[32];
		format_time((time_t)a->first_login, first_login_str, sizeof(first_login_str));

		/* Look up account in pre-built snapshot (no extra lock) */
		if (snaps) {
			for (int si = 0; si < nsnaps; si++) {
				if (strcmp(snaps[si].user, a->user) != 0) continue;
				proto_str = snaps[si].proto;
				if (!ip_str[0]) tcmg_strlcpy(ip_str, snaps[si].ip, sizeof(ip_str));
				if (snaps[si].last_ecm > last_ecm_t) last_ecm_t = snaps[si].last_ecm;
			}
		}

		/* Idle time */
		char idle_str[32];
		if (a->active > 0 && last_ecm_t > 0) {
			time_t idle_s = time(NULL) - last_ecm_t;
			if (idle_s < 0) idle_s = 0;
			format_uptime(idle_s, idle_str, sizeof(idle_str));
		} else {
			tcmg_strlcpy(idle_str, "&mdash;", sizeof(idle_str));
		}

		const char *btn_cls = a->enabled ? "pw-btn on" : "pw-btn off";
		tot_cw_ok  += a->cw_found;
		tot_cw_nok += a->cw_not;

		/* hit-bar width for the mini bar under OK/NOK */
		double bar_w = hr >= 0 ? hr : 0.0;

		/* Format max-connections: 0 = unlimited */
		char maxconn_str[16];
		if (a->max_connections <= 0)
			tcmg_strlcpy(maxconn_str, "&infin;", sizeof(maxconn_str));
		else
			snprintf(maxconn_str, sizeof(maxconn_str), "%d", (int)a->max_connections);

		pos = buf_printf(&buf, &bsz, pos,
			"<tr data-user='%s'>"
			"<td><button class='%s' id='pw_%.32s' onclick=\"tgUser('%s',this)\" title='Toggle'>"
			"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"</svg></button></td>"
			"<td><div class='flex gap8'>"
			"  <span class='u-link bold' onclick=\"editUser('%s')\">%s</span>"
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
			"    <button class='btn bg sm' onclick=\"resetStats('%s')\" title='Reset stats' "
			"      style='padding:4px 7px;font-size:11px'>&#8635;</button>"
			"    <button class='btn bd_ sm' onclick=\"delUser('%s')\" title='Delete'>"
			ICO_KILLBTN "</button>"
			"  </div>"
			"</td>"
			"</tr>",
			a->user,
			btn_cls, a->user, a->user,
			a->user, a->user,
			a->caid,
			a->active > 0 ? " tg bold" : "", (int)a->active,
			maxconn_str,
			(long long)a->cw_found,
			a->cw_not > 0 ? " tr" : "", (long long)a->cw_not,
			bar_w,
			hr > 80.0 ? "tg" : hr >= 0 ? (hr > 50.0 ? "to" : "tr") : "tm",
			hrstr,
			avgstr,
			minmaxstr,
			/* proto */
			a->active > 0 ? "badge bcy" : "tm", proto_str,
			/* IP */
			ip_str[0] ? ip_str : "&mdash;",
			/* idle */
			idle_str,
			/* first_login */
			first_login_str,
			/* last_seen */
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

	/* ── tfoot aggregate row ── */
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

	/* Edit/Add user modals + Search/Sort JS */
	pos = buf_printf(&buf, &bsz, pos,
		/* ── Shared modal overlay ── */
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
		"    <input class='fi mono' id='em_pass' type='password' placeholder='leave blank to keep' autocomplete='new-password'></div>"
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
		"function resetStats(u){"
		"  if(!confirm('Reset stats for '+u+'?'))return;"
		"  fetch('/api/user/resetstats?user='+encodeURIComponent(u))"
		"    .then(r=>r.json())"
		"    .then(d=>{if(d.ok)location.reload();else alert('Error: '+d.msg);});"
		"}"
		/* ── Toggle on/off ── */
		"function tgUser(u,btn){"
		"  fetch('/api/user/toggle?user='+encodeURIComponent(u))"
		"    .then(r=>r.json())"
		"    .then(d=>{"
		"      if(!d.ok)return;"
		"      btn.className='pw-btn '+(d.enabled?'on':'off');"
		"    });"
		"}"
		/* ── Edit user ── */
		"function editUser(u){"
		"  fetch('/api/user/get?user='+encodeURIComponent(u))"
		"    .then(r=>r.json())"
		"    .then(d=>{"
		"      document.getElementById('umTitle').textContent='Edit User';"
		"      document.getElementById('em_unew_wrap').style.display='none';"
		"      document.getElementById('em_udisp_wrap').style.display='';"
		"      document.getElementById('em_user').value=d.user;"
		"      document.getElementById('em_udisp').value=d.user;"
		"      document.getElementById('em_pass').value='';"
		"      document.getElementById('em_caid').value=d.caid;"
		"      document.getElementById('em_maxconn').value=d.max_connections;"
		"      document.getElementById('em_enabled').checked=!!d.enabled;"
		"      document.getElementById('em_expiry').value=d.expiry||'';"
		"      document.getElementById('em_err').style.display='none';"
		"      document.getElementById('uModal').style.display='flex';"
		"    });"
		"}"
		/* ── Add user ── */
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
		"function delUser(u){"
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
		/* ── Live search/filter ── */
		"function filterUsers(){"
		"  var q=document.getElementById('usrSearch').value.toLowerCase();"
		"  var rows=document.getElementById('usrBody').rows;"
		"  for(var i=0;i<rows.length;i++){"
		"    var u=(rows[i].getAttribute('data-user')||'').toLowerCase();"
		"    rows[i].style.display=(!q||u.includes(q))?'':'none';"
		"  }"
		"}"
		/* ── Column sort ── */
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
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ════════════════════════════════════════════════════════════════════════════
 * Fail-ban page
 * ════════════════════════════════════════════════════════════════════════════*/
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
		tcmg_log("cleared all bans");
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

	/* ── Summary bar ── */
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

/* ════════════════════════════════════════════════════════════════════════════
 * Config editor page
 * ════════════════════════════════════════════════════════════════════════════*/
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
		"<div style='display:flex;gap:0;margin-bottom:-1px;position:relative;z-index:1'>"
		"  <button class='ctab act' id='tab_cfg' onclick=\"switchTab('cfg')\">tcmg.cfg</button>"
		"  <button class='ctab' id='tab_srv'     onclick=\"switchTab('srv')\">tcmg.srvid2</button>"
		"</div>");

	/* ── tcmg.cfg panel ── */
	if (cfg_trunc)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='ib2' style='color:var(--or2);border-color:var(--ors)'>"
			ICO_WARN " Config exceeds 16 KB &mdash; truncated. Edit on disk.</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='pnl_cfg'>"
		"<form method='post' action='/config_save'>"
		"<div class='card'>"
		"<div class='et'>"
		"  <span class='ef'>" ICO_FILE "&nbsp;%s</span>"
		"  <button type='submit' class='btn bp sm' id='saveBtn'"
		"    onclick=\"document.getElementById('cfgWarn').style.display='flex'\">"
		"    " ICO_SAVE "&nbsp;Save &amp; Reload"
		"  </button>"
		"</div>"
		"<div class='ew' id='cfgWarn' style='display:none'>"
		"  " ICO_WARN " Config saved &mdash; restart may be needed for some changes"
		"</div>"
		"<textarea class='ea' name='cfg' spellcheck='false' id='cfgArea' oninput='onEdit()'>%s</textarea>"
		"<div class='ef2'>"
		"  <span class='es'><span class='ok' id='edSt'>&#9679; Saved</span></span>"
		"  <span style='font-family:var(--mono);font-size:11px;color:var(--t2)' id='edCur'>Ln 1, Col 1</span>"
		"</div></div></form></div>",
		cfgpath, escaped);
	free(escaped);

	/* ── tcmg.srvid2 panel ── */
	if (srv_trunc)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='ib2' style='color:var(--or2);border-color:var(--ors)'>"
			ICO_WARN " srvid2 exceeds 32 KB &mdash; truncated. Edit on disk.</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='pnl_srv' style='display:none'>"
		"<form method='post' action='/srvid2_save'>"
		"<div class='card'>"
		"<div class='et'>"
		"  <span class='ef'>" ICO_FILE "&nbsp;%s</span>"
		"  <button type='submit' class='btn bp sm'>" ICO_SAVE "&nbsp;Save &amp; Reload</button>"
		"</div>"
		"<textarea class='ea' name='srvid2' spellcheck='false' style='height:520px'>%s</textarea>"
		"<div class='ef2'>"
		"  <span style='font-size:11px;color:var(--t2)'>"
		"Format: SID:CAID[,CAID]|name|type||provider</span>"
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
	tcmg_log("config saved via webif, reloading...");
	g_reload_cfg = 1;
	send_redirect(fd, "/config");
}

/* ════════════════════════════════════════════════════════════════════════════
 * Live log page  GET /livelog
 * ════════════════════════════════════════════════════════════════════════════*/
void send_page_livelog(int fd)
{
	int   bsz = 32768, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Live Log", "livelog");

	/* Debug level toggles */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='db'>"
		"<span style='font-size:10px;font-weight:700;color:var(--t2);"
		"text-transform:uppercase;letter-spacing:.12em;margin-right:6px'>Debug</span>"
		"<span style='font-size:10px;color:var(--t2);margin-right:6px;"
		"font-family:var(--mono)' title='Enable debug categories to see verbose output'>"
		"&#9432;&nbsp;click to toggle verbose output</span>");

	char masks_js[256];
	int  ma = 0;
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
		"<a id='dbALL' href='#' class='dt%s' onclick='toggleAll();return false;'>ALL</a>"
		"<span class='dm'>mask: <span id='dbmask'>0x%04X</span></span>"
		"</div>",
		(g_dblevel == D_ALL) ? " on" : "", g_dblevel);

	/* Toolbar */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='lc'>"
		"<button class='btn bg sm' onclick='clearLog()'>" ICO_TRASH "&nbsp;Clear</button>"
		"<a id='savelog' download='tcmg.log'>"
		"  <button class='btn bg sm' onclick='prepSave()'>&#128190;&nbsp;Save</button>"
		"</a>"
		"<input class='ls' id='filter' placeholder='Filter (regex)...' oninput='applyFilter()'"
		" title='Supports regex, e.g.: hit|miss'>"
		"<label class='flex gap8' style='font-size:12px;color:var(--t1)'>"
		"<input type='checkbox' id='asc' checked>&nbsp;Auto-scroll</label>"
		"<label class='flex gap8' style='font-size:12px;color:var(--t1)'>"
		"<input type='checkbox' id='paused'>&nbsp;Pause</label>"
		"<span style='margin-left:auto;font-size:11px;color:var(--t2);"
		"display:flex;align-items:center;gap:6px'>"
		"Lines: <select class='lsel' id='maxlines'>"
		"<option value='200' selected>200</option>"
		"<option value='500'>500</option>"
		"<option value='1000'>1000</option>"
		"<option value='2000'>2000</option>"
		"</select>"
		"&nbsp;<span id='linecnt' style='font-family:var(--mono);color:var(--t2)'>0</span>&nbsp;shown"
		"</span></div>"
		/* Color legend */
		"<div style='display:flex;gap:8px;flex-wrap:wrap;margin-bottom:8px;"
		"font-size:11px;font-family:var(--mono)'>"
		"<span style='color:#4ade80'>&#9632; hit</span>"
		"<span style='color:#f87171'>&#9632; miss</span>"
		"<span style='color:#60a5fa'>&#9632; webif</span>"
		"<span style='color:#fb923c'>&#9632; ban</span>"
		"<span style='color:#c084fc'>&#9632; net</span>"
		"<span style='color:#22d3ee'>&#9632; proto</span>"
		"<span style='color:#fde68a'>&#9632; conf</span>"
		"<span style='color:#f87171'>&#9632; error/warn</span>"
		"</div>"
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
		"    if(usr){"
		"      var b=document.createElement('span');"
		"      b.textContent='['+usr+'] ';"
		"      b.style.cssText='font-size:10px;color:#60a5fa;opacity:.85;font-family:var(--mono);';"
		"      b.title='User: '+usr;"
		"      span.appendChild(b);"
		"    }"
		"    span.appendChild(document.createTextNode(line));"
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

/* ════════════════════════════════════════════════════════════════════════════
 * Log poll API  GET /logpoll
 * ════════════════════════════════════════════════════════════════════════════*/
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

/* ════════════════════════════════════════════════════════════════════════════
 * JSON API  GET /api/status
 * ════════════════════════════════════════════════════════════════════════════*/
void send_api_status(int fd)
{
	int   bsz = 16384, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	S_SERVER_STATS st  = collect_stats();
	time_t         now = time(NULL);

	/* Correctly wrapped in { } */
	pos = buf_printf(&buf, &bsz, pos,
		"{"
		"\"version\":\"%s\","
		"\"build\":\"%s\","
		"\"uptime_s\":%ld,"
		"\"uptime_str\":\"%s\","
		"\"port\":%d,"
		"\"active_connections\":%d,"
		"\"accounts\":%d,"
		"\"banned_ips\":%d,"
		"\"cw_found\":%lld,"
		"\"cw_not\":%lld,"
		"\"ecm_total\":%lld,"
		"\"hit_rate_pct\":%.1f,"
		"\"debug_mask\":%u,"
		"\"clients\":[",
		TCMG_VERSION, TCMG_BUILD_TIME,
		(long)st.uptime_s, st.uptime_str,
		g_cfg.port, st.active_conns,
		st.naccounts, st.nbans,
		(long long)st.cw_found, (long long)st.cw_not, (long long)st.ecm_total,
		st.hit_rate, g_dblevel);

	pthread_mutex_lock(&g_clients_mtx);
	bool first = true;
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) {
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->account) continue;
		char conn_str[32], idle_str[32];
		format_uptime(now - cl->connect_time,         conn_str, sizeof(conn_str));
		format_uptime(now - cl->account->last_seen,   idle_str, sizeof(idle_str));
		pos = buf_printf(&buf, &bsz, pos,
			"%s{"
			"\"user\":\"%s\","
			"\"ip\":\"%s\","
			"\"caid\":\"%04X\","
			"\"sid\":\"%04X\","
			"\"channel\":\"%s\","
			"\"connected\":\"%s\","
			"\"idle\":\"%s\","
			"\"thread_id\":%u"
			"}",
			first ? "" : ",",
			cl->user, cl->ip,
			cl->last_caid, cl->last_srvid,
			cl->last_channel[0] ? cl->last_channel : "",
			conn_str, idle_str,
			cl->thread_id);
		first = false;
	}
	pthread_mutex_unlock(&g_clients_mtx);

	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

/* ════════════════════════════════════════════════════════════════════════════
 * User API
 * ════════════════════════════════════════════════════════════════════════════*/

/* GET /api/user/toggle?user=<name> */
void handle_user_toggle(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	int enabled = -1;
	if (uname[0]) {
		pthread_rwlock_wrlock(&g_cfg.acc_lock);
		for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
			if (strcmp(a->user, uname) == 0) {
				a->enabled = !a->enabled;
				enabled = a->enabled;
				break;
			}
		}
		pthread_rwlock_unlock(&g_cfg.acc_lock);
	}
	if (enabled < 0) {
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	cfg_save(&g_cfg);
	tcmg_log("user '%s' %s via webif", uname, enabled ? "enabled" : "disabled");
	char j[64];
	snprintf(j, sizeof(j), "{\"ok\":true,\"enabled\":%d}", enabled);
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}

/* GET /api/user/get?user=<name>
 * SECURITY: password hash is NEVER returned. */
void send_api_user_get(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	int   bsz = 512, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;

	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		free(buf);
		const char *e = "{\"ok\":false,\"msg\":\"not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	char expiry[24] = "0";
	if (a->expirationdate > 0) {
		struct tm tm_s;
		localtime_r(&a->expirationdate, &tm_s);
		strftime(expiry, sizeof(expiry), "%Y-%m-%d", &tm_s);
	}
	/* pass field omitted intentionally — client sets blank to keep existing */
	pos = buf_printf(&buf, &bsz, pos,
		"{\"ok\":true,\"user\":\"%s\","
		"\"caid\":\"%04X\",\"max_connections\":%d,"
		"\"enabled\":%d,\"expiry\":\"%s\"}",
		a->user, a->caid, (int)a->max_connections,
		(int)a->enabled, expiry);
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

/* POST /api/user/save */
void handle_user_save(int fd, const char *post_body)
{
	char uname[CFGKEY_LEN]="", pass[CFGKEY_LEN]="";
	char caid_s[8]="", maxconn_s[16]="", enabled_s[4]="", expiry_s[16]="";
	form_get(post_body, "user",    uname,     sizeof(uname));
	form_get(post_body, "pass",    pass,      sizeof(pass));
	form_get(post_body, "caid",    caid_s,    sizeof(caid_s));
	form_get(post_body, "maxconn", maxconn_s, sizeof(maxconn_s));
	form_get(post_body, "enabled", enabled_s, sizeof(enabled_s));
	form_get(post_body, "expiry",  expiry_s,  sizeof(expiry_s));

	if (!uname[0]) {
		const char *e = "{\"ok\":false,\"msg\":\"missing user\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
		return;
	}

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}

	if (pass[0])       tcmg_strlcpy(a->pass, pass, sizeof(a->pass));
	if (caid_s[0])     a->caid            = (uint16_t)strtol(caid_s, NULL, 16);
	if (maxconn_s[0])  a->max_connections = atoi(maxconn_s);
	a->enabled = atoi(enabled_s);

	if (expiry_s[0] && strcmp(expiry_s, "0") != 0) {
		struct tm tm_s = {0};
		if (sscanf(expiry_s, "%d-%d-%d",
		           &tm_s.tm_year, &tm_s.tm_mon, &tm_s.tm_mday) == 3) {
			tm_s.tm_year -= 1900;
			tm_s.tm_mon  -= 1;
			a->expirationdate = mktime(&tm_s);
		}
	} else {
		a->expirationdate = 0;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	cfg_save(&g_cfg);
	tcmg_log("user '%s' updated via webif", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}


/* GET /api/user/delete?user=<name> */
void handle_user_delete(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	if (!uname[0]) {
		const char *e = "{\"ok\":false,\"msg\":\"missing user\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
		return;
	}

	int found = 0;
	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT **pp = &g_cfg.accounts;
	while (*pp) {
		if (strcmp((*pp)->user, uname) == 0) {
			S_ACCOUNT *del = *pp;
			*pp = del->next;
			free(del);
			g_cfg.naccounts--;
			found = 1;
			break;
		}
		pp = &(*pp)->next;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!found) {
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	cfg_save(&g_cfg);
	tcmg_log("user '%s' deleted via webif", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}

/* POST /api/user/add — create a new account */
void handle_user_add(int fd, const char *post_body)
{
	char uname[CFGKEY_LEN]="", pass[CFGKEY_LEN]="";
	char caid_s[8]="", maxconn_s[16]="", enabled_s[4]="", expiry_s[16]="";
	form_get(post_body, "user",    uname,     sizeof(uname));
	form_get(post_body, "pass",    pass,      sizeof(pass));
	form_get(post_body, "caid",    caid_s,    sizeof(caid_s));
	form_get(post_body, "maxconn", maxconn_s, sizeof(maxconn_s));
	form_get(post_body, "enabled", enabled_s, sizeof(enabled_s));
	form_get(post_body, "expiry",  expiry_s,  sizeof(expiry_s));

	if (!uname[0]) {
		const char *e = "{\"ok\":false,\"msg\":\"username required\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
		return;
	}

	/* Reject if username already exists */
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		if (strcmp(a->user, uname) == 0) {
			pthread_rwlock_unlock(&g_cfg.acc_lock);
			const char *e = "{\"ok\":false,\"msg\":\"username already exists\"}";
			send_response(fd, 409, "Conflict", "application/json", e, (int)strlen(e));
			return;
		}
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	/* Create new account */
	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = cfg_account_new(&g_cfg);
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		const char *e = "{\"ok\":false,\"msg\":\"out of memory\"}";
		send_response(fd, 500, "Internal Error", "application/json", e, (int)strlen(e));
		return;
	}

	tcmg_strlcpy(a->user, uname, sizeof(a->user));
	if (pass[0])      tcmg_strlcpy(a->pass, pass, sizeof(a->pass));
	if (caid_s[0])    a->caid            = (uint16_t)strtol(caid_s, NULL, 16);
	if (maxconn_s[0]) a->max_connections = atoi(maxconn_s);
	a->enabled = enabled_s[0] ? atoi(enabled_s) : 1;

	if (expiry_s[0] && strcmp(expiry_s, "0") != 0) {
		struct tm tm_s = {0};
		if (sscanf(expiry_s, "%d-%d-%d",
		           &tm_s.tm_year, &tm_s.tm_mon, &tm_s.tm_mday) == 3) {
			tm_s.tm_year -= 1900;
			tm_s.tm_mon  -= 1;
			a->expirationdate = mktime(&tm_s);
		}
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	cfg_save(&g_cfg);
	tcmg_log("user '%s' added via webif", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}

/* GET /api/user/resetstats?user=<name>
 * Inspired by OSCam's per-user statistics reset (resetuserstats action).
 * Resets ECM counters, CW found/not, timing, and min/max ms for one account. */
void handle_user_resetstats(int fd, const char *qs)
{
	char uname[CFGKEY_LEN] = "";
	get_param(qs, "user", uname, sizeof(uname));

	if (!uname[0]) {
		const char *e = "{\"ok\":false,\"msg\":\"missing user\"}";
		send_response(fd, 400, "Bad Request", "application/json", e, (int)strlen(e));
		return;
	}

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a && strcmp(a->user, uname) != 0) a = a->next;
	if (!a) {
		pthread_rwlock_unlock(&g_cfg.acc_lock);
		const char *e = "{\"ok\":false,\"msg\":\"user not found\"}";
		send_response(fd, 404, "Not Found", "application/json", e, (int)strlen(e));
		return;
	}
	a->ecm_total        = 0;
	a->cw_found         = 0;
	a->cw_not           = 0;
	a->cw_time_total_ms = 0;
	a->cw_time_min_ms   = 0;
	a->cw_time_max_ms   = 0;
	a->first_login      = 0;
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	tcmg_log("stats reset for user '%s' via webif", uname);
	const char *j = "{\"ok\":true}";
	send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
}
void handle_srvid2_save(int fd, const char *post_body)
{
	char newcontent[32768] = "";
	form_get(post_body, "srvid2", newcontent, sizeof(newcontent));
	if (!newcontent[0]) {
		const char *e = "<html><body><h1>Empty srvid2 rejected</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}

	char path[CFGPATH_LEN];
	tcmg_build_path(path, sizeof(path), g_cfgdir, TCMG_SRVID_FILE);

	FILE *fp = fopen(path, "w");
	if (!fp) {
		const char *e = "<html><body><h1>Cannot write srvid2</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	fputs(newcontent, fp);
	fclose(fp);
	int n = srvid_load(path);
	tcmg_log("srvid2 saved (%d entries), reloaded", n);
	send_redirect(fd, "/config");
}

/* ════════════════════════════════════════════════════════════════════════════
 * Action pages: Shutdown & Restart  (single implementation)
 *
 * Combined Power page: Restart + Shutdown in one place
 * ════════════════════════════════════════════════════════════════════════════*/
void send_page_power(int fd, const char *qs)
{
	char action[16], confirm[8];
	get_param(qs, "action",  action,  sizeof(action));
	get_param(qs, "confirm", confirm, sizeof(confirm));

	int   bsz = 12288, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Power", "power");

	/* ── Confirmed action ── */
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
	/* ── Pending confirmation ── */
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
	/* ── Main power panel ── */
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
