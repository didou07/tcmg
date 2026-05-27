#define MODULE_LOG_PREFIX "webif"
#include "tcmg-globals.h"
#include "tcmg-crypto.h"
#include "tcmg-log.h"

#ifndef TCMG_OS_WINDOWS
#  include <netdb.h>
#  include <sys/select.h>
#endif

#include "tcmg-webif-internal.h"


/* =
 *  LOGIN PAGE
 * = */
void send_login_page(int fd, int failed)
{
	int bsz=8192, pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=buf_printf(&buf,&bsz,pos,
		"<!DOCTYPE html><html lang='en'><head>"
		"<meta charset='UTF-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>TCMG &mdash; Login</title>"
		"<style>%s</style>"
		"</head><body>",
		CSS);

	pos=buf_printf(&buf,&bsz,pos,
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

	if(failed)
		pos=buf_printf(&buf,&bsz,pos,
			"<div class='le'>"
			"<svg width='14' height='14' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<circle cx='12' cy='12' r='10'/>"
			"<line x1='12' y1='8' x2='12' y2='12'/>"
			"<line x1='12' y1='16' x2='12.01' y2='16'/>"
			"</svg>"
			"Invalid credentials &mdash; please try again."
			"</div>");

	pos=buf_printf(&buf,&bsz,pos,
		"<form method='POST' action='/login'>"
		"<div class='fg'>"
		"  <label class='fl'>USERNAME</label>"
		"  <input class='fi' type='text' name='u' placeholder='Enter username' autofocus autocomplete='username'>"
		"</div>"
		"<div class='fg'>"
		"  <label class='fl'>PASSWORD</label>"
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

	send_response(fd,200,"OK","text/html",buf,pos);
	free(buf);
}

/* =
 *  STATS helpers
 * = */
void handle_reset_stats(void)
{
	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a) {
		a->ecm_total = 0; a->cw_found = 0; a->cw_not = 0;
		a->cw_time_total_ms = 0;
		a = a->next;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	tcmg_log("all user stats reset");
}

S_STATS aggregate_stats(void)
{
	S_STATS s = {0, 0, 0};
	time_t now = time(NULL);
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	for (const S_ACCOUNT *a = g_cfg.accounts; a; a = a->next)
		{ s.cw_found += a->cw_found; s.cw_not += a->cw_not; }
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	pthread_mutex_lock(&g_cfg.ban_lock);
	for (const S_BAN_ENTRY *b = g_cfg.bans; b; b = b->next)
		if (now < b->until) s.nbans++;
	pthread_mutex_unlock(&g_cfg.ban_lock);
	return s;
}

/* =
 *  STATUS PAGE
 * = */
void send_page_status(int fd)
{
	int bsz=32768, pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=emit_header(&buf,&bsz,pos,"Dashboard","status");

	time_t now=time(NULL);
	char upstr[32];
	format_uptime(now-g_start_time, upstr, sizeof(upstr));

	S_STATS st=aggregate_stats();
	int64_t cw_found=st.cw_found, cw_not=st.cw_not;
	int nbans=st.nbans, naccounts;
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	naccounts=g_cfg.naccounts;
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	int64_t ecm_total=cw_found+cw_not;
	double  hitrate=ecm_total>0 ? (double)cw_found*100.0/(double)ecm_total : 0.0;
	char    hrstr[16]="0.0%";
	if(ecm_total>0) snprintf(hrstr,sizeof(hrstr),"%.1f%%",hitrate);

	/* Page header */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='ph'>"
		"  <div>"
		"    <div class='pt'>Dashboard</div>"
		"    <div class='ps'>Live server statistics &mdash; auto-refreshing</div>"
		"  </div>"
		"  <div class='ha'>"
		"    <a href='#' onclick=\"if(confirm('Reset all stats?')){fetch('/api/resetstats').then(()=>{location.reload();});}return false\""
		"       class='btn bg sm'>"
		"      <svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"        <polyline points='23 4 23 10 17 10'/>"
		"        <path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
		"      </svg>Reset Stats"
		"    </a>"
		"    <a href='#' onclick=\"fetch('/api/reload').then(()=>{this.textContent='\u2713 Done';this.disabled=true;});return false\""
		"       class='btn bp sm'>"
		"      <svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"        <polyline points='1 4 1 10 7 10'/>"
		"        <path d='M3.51 15a9 9 0 1 0 .49-4.95'/>"
		"      </svg>Reload Config"
		"    </a>"
		"  </div>"
		"</div>");

	/* Stat cards */
	pos=buf_printf(&buf,&bsz,pos,"<div class='cg'>");

	/* Uptime */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='sc bl'>"
		"<div class='si_'>" ICO_CLOCK "</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>Uptime</div>"
		"  <div class='sv mono' id='p_up'>%s</div>"
		"  <div class='sd'>since start</div>"
		"</div><div class='sg'></div></div>", upstr);

	/* Connections */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='sc %s'>"
		"<div class='si_'>" ICO_USERS2 "</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>Connections</div>"
		"  <div class='sv' id='p_conn'>%d</div>"
		"  <div class='sd'>of <span id='p_acc'>%d</span> accounts</div>"
		"</div><div class='sg'></div></div>",
		g_active_conns>0?"gr":"bl",
		g_active_conns, naccounts);

	/* ECM Total */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='sc vi'>"
		"<div class='si_'>" ICO_ZAP "</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>ECM Total</div>"
		"  <div class='sv' id='p_ecm'>%lld</div>"
		"  <div class='sd'>requests processed</div>"
		"</div><div class='sg'></div></div>", (long long)ecm_total);

	/* CW Found */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='sc gr'>"
		"<div class='si_'>" ICO_CHECK "</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>CW Found</div>"
		"  <div class='sv' id='p_hit'>%lld</div>"
		"  <div class='sd'>cache hits</div>"
		"</div><div class='sg'></div></div>", (long long)cw_found);

	/* CW Miss */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='sc %s'>"
		"<div class='si_'>" ICO_X "</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>CW Miss</div>"
		"  <div class='sv' id='p_miss'>%lld</div>"
		"  <div class='sd'>not found</div>"
		"</div><div class='sg'></div></div>",
		cw_not>0?"re":"bl", (long long)cw_not);

	/* Hit Rate */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='sc cy'>"
		"<div class='si_'>" ICO_PERCENT "</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>Hit Rate</div>"
		"  <div class='sv' id='p_hr'>%s</div>"
		"  <div class='sd' style='display:flex;align-items:center;gap:6px;margin-top:4px'>"
		"    <div class='hbw' style='width:100px'><div class='hbf' id='p_hbf' style='width:%.0f%%'></div></div>"
		"  </div>"
		"</div><div class='sg'></div></div>",
		hrstr, hitrate);

	/* Banned */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='sc %s'>"
		"<div class='si_'>" ICO_SHIELD "</div>"
		"<div class='sb_'>"
		"  <div class='sl_'>Banned IPs</div>"
		"  <div class='sv' id='p_ban'>%d</div>"
		"  <div class='sd'><a href='/failban' style='color:inherit;opacity:.7;font-size:11px'>view list &rarr;</a></div>"
		"</div><div class='sg'></div></div>",
		nbans>0?"or":"bl", nbans);

	pos=buf_printf(&buf,&bsz,pos,"</div>"); /* /cg */

	/* Active connections table */
	pos=buf_printf(&buf,&bsz,pos,
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
		"<tbody id='p_clients'>");

	pthread_mutex_lock(&g_clients_mtx);
	int shown=0;
	for(int i=0;i<MAX_ACTIVE_CLIENTS;i++)
	{
		S_CLIENT *cl=g_clients[i];
		if(!cl||!cl->account)continue;
		char conn_str[32], idle_str[32];
		format_uptime(now-cl->connect_time, conn_str, sizeof(conn_str));
		format_uptime(now-cl->account->last_seen, idle_str, sizeof(idle_str));
		char init[3]={0};
		init[0]=(cl->user[0]>='a'&&cl->user[0]<='z')?(cl->user[0]-32):cl->user[0];
		init[1]=(cl->user[1]>='a'&&cl->user[1]<='z')?(cl->user[1]-32):cl->user[1];
		pos=buf_printf(&buf,&bsz,pos,
			"<tr id='row_%u'>"
			"<td><div class='flex gap8'><span class='av'>%.2s</span><span class='bold'>%s</span></div></td>"
			"<td class='mono'>%s</td>"
			"<td class='mono'><span class='badge bbl'>%04X</span></td>"
			"<td class='mono'>%04X</td>"
			"<td>%s</td>"
			"<td class='mono tm'>%s</td>"
			"<td class='mono tm'>%s</td>"
			"<td><button class='kb' onclick=\"if(confirm('Disconnect %s?')){"
			"fetch('/status?kill=%u&user=%s');"
			"var r=document.getElementById('row_%u');if(r){r.style.opacity='.3';setTimeout(()=>r.remove(),400);}"
			"}\" title='Disconnect'>"
			"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<line x1='18' y1='6' x2='6' y2='18'/><line x1='6' y1='6' x2='18' y2='18'/>"
			"</svg></button></td>"
			"</tr>",
			cl->thread_id,
			init, cl->user, cl->ip,
			cl->last_caid, cl->last_srvid,
			cl->last_channel[0]?cl->last_channel:"<span class='tm'>&mdash;</span>",
			conn_str, idle_str,
			cl->user, cl->thread_id, cl->user, cl->thread_id);
		shown++;
	}
	pthread_mutex_unlock(&g_clients_mtx);

	if(!shown)
		pos=buf_printf(&buf,&bsz,pos,
			"<tr class='erow'><td colspan='8'>"
			"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'"
			" style='vertical-align:-3px;margin-right:6px;opacity:.35'>"
			"<circle cx='12' cy='12' r='10'/><line x1='8' y1='12' x2='16' y2='12'/>"
			"</svg>No active connections"
			"</td></tr>");

	pos=buf_printf(&buf,&bsz,pos,"</tbody></table></div>");
	pos=emit_footer(&buf,&bsz,pos);
	send_response(fd,200,"OK","text/html",buf,pos);
	free(buf);
}

/* =
 *  USERS PAGE
 * = */
void send_page_users(int fd)
{
	int bsz=65536, pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=emit_header(&buf,&bsz,pos,"Users","users");

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	int naccs=g_cfg.naccounts;
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	pos=buf_printf(&buf,&bsz,pos,
		"<div class='ph'>"
		"  <div>"
		"    <div class='pt'>Users</div>"
		"    <div class='ps'>%d account%s configured</div>"
		"  </div>"
		"</div>",
		naccs, naccs==1?"":"s");

	pos=buf_printf(&buf,&bsz,pos,
		"<div class='tw'><table>"
		"<thead><tr>"
		"<th>User</th><th>CAID</th><th>Status</th>"
		"<th>Active</th><th>Max</th>"
		"<th>CW Hit</th><th>CW Miss</th><th>Hit %%</th><th>Avg ms</th>"
		"<th>Hit Bar</th><th>First Login</th><th>Last Seen</th><th>Expiry</th>"
		"</tr></thead><tbody>");

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	S_ACCOUNT *a=g_cfg.accounts;
	int row=0;
	while(a)
	{
		char last[32],first_l[32],expiry[128];
		format_time((time_t)a->last_seen,   last,    sizeof(last));
		format_time((time_t)a->first_login, first_l, sizeof(first_l));

		if(a->expirationdate>0){
			format_time(a->expirationdate,expiry,sizeof(expiry));
			if(time(NULL)>a->expirationdate)
				snprintf(expiry,sizeof(expiry),"<span class='badge bban'>EXPIRED</span>");
		} else snprintf(expiry,sizeof(expiry),"<span class='tm'>&mdash;</span>");

		int64_t tot=a->cw_found+a->cw_not;
		double  hr=tot>0?(double)a->cw_found*100.0/(double)tot:-1.0;
		char hrstr[16]="&mdash;";
		if(hr>=0) snprintf(hrstr,sizeof(hrstr),"%.1f%%",hr);

		char avgstr[16]="&mdash;";
		if(a->cw_found>0)
			snprintf(avgstr,sizeof(avgstr),"%lld",(long long)(a->cw_time_total_ms/a->cw_found));

		const char *st_badge=a->enabled
			?"<span class='badge bon'>&#9679;&nbsp;on</span>"
			:"<span class='badge boff'>&#9679;&nbsp;off</span>";

		/* Avatar initials */
		char init[3]={0};
		init[0]=(a->user[0]>='a'&&a->user[0]<='z')?(a->user[0]-32):a->user[0];
		init[1]=(a->user[1]>='a'&&a->user[1]<='z')?(a->user[1]-32):a->user[1];

		pos=buf_printf(&buf,&bsz,pos,
			"<tr>"
			"<td><div class='flex gap8'><span class='av'>%.2s</span><span class='bold'>%s</span></div></td>"
			"<td class='mono'><span class='badge bbl'>%04X</span></td>"
			"<td>%s</td>"
			"<td class='mono'>%d</td>"
			"<td class='mono tm'>%lld</td>"
			"<td class='mono tg'>%lld</td>"
			"<td class='mono %s'>%lld</td>"
			"<td class='mono'>%s</td>"
			"<td class='mono tm'>%s</td>"
			"<td><div class='hbw'><div class='hbf' style='width:%.0f%%'></div></div></td>"
			"<td class='mono tm' style='font-size:11px'>%s</td>"
			"<td class='mono tm' style='font-size:11px'>%s</td>"
			"<td style='font-size:12px'>%s</td>"
			"</tr>",
			init, a->user,
			a->caid,
			st_badge,
			(int)a->active,
			(long long)a->max_connections,
			(long long)a->cw_found,
			a->cw_not>0?"tr":"", (long long)a->cw_not,
			hrstr, avgstr,
			hr>=0?hr:0.0,
			first_l, last, expiry);
		a=a->next;
		row++;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if(!row)
		pos=buf_printf(&buf,&bsz,pos,
			"<tr class='erow'><td colspan='13'>"
			"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'"
			" style='vertical-align:-3px;margin-right:6px;opacity:.35'>"
			"<path d='M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2'/><circle cx='9' cy='7' r='4'/>"
			"</svg>No accounts configured"
			"</td></tr>");

	pos=buf_printf(&buf,&bsz,pos,"</tbody></table></div>");
	pos=emit_footer(&buf,&bsz,pos);
	send_response(fd,200,"OK","text/html",buf,pos);
	free(buf);
}

/* =
 *  FAIL-BAN PAGE
 * = */
void send_page_failban(int fd, const char *qs)
{
	char action[32], clearip[MAXIPLEN];
	get_param(qs,"action",action,sizeof(action));
	get_param(qs,"ip",    clearip,sizeof(clearip));

	if(strcmp(action,"clear")==0 && clearip[0]){
		pthread_mutex_lock(&g_cfg.ban_lock);
		S_BAN_ENTRY *b=g_cfg.bans;
		while(b){if(strcmp(b->ip,clearip)==0)b->until=0;b=b->next;}
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("cleared ban for %s",clearip);
	} else if(strcmp(action,"clearall")==0){
		pthread_mutex_lock(&g_cfg.ban_lock);
		S_BAN_ENTRY *b=g_cfg.bans;
		while(b){b->until=0;b=b->next;}
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("cleared all bans");
	}

	int bsz=16384,pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=emit_header(&buf,&bsz,pos,"Fail-Ban","failban");

	int total_bans=0;
	time_t now=time(NULL);
	pthread_mutex_lock(&g_cfg.ban_lock);
	for(S_BAN_ENTRY *b=g_cfg.bans;b;b=b->next)
		if(b->until>now)total_bans++;
	pthread_mutex_unlock(&g_cfg.ban_lock);

	pos=buf_printf(&buf,&bsz,pos,
		"<div class='ph'>"
		"  <div>"
		"    <div class='pt'>Fail-Ban</div>"
		"    <div class='ps'>%s</div>"
		"  </div>"
		"  %s"
		"</div>",
		total_bans>0
			? "<span class='badge bban' style='font-size:12px'>&nbsp;"
			  "<svg width='11' height='11' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2.5' style='vertical-align:-1px'>"
			  "<circle cx='12' cy='12' r='10'/><line x1='4.93' y1='4.93' x2='19.07' y2='19.07'/></svg>"
			  "&nbsp;</span> Active bans"
			: "No active bans",
		total_bans>0
			? "<a href='/failban?action=clearall' class='btn bd_ sm'>"
			  "<svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			  "<polyline points='3 6 5 6 21 6'/>"
			  "<path d='M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2'/>"
			  "</svg>Clear All</a>"
			: "");

	pos=buf_printf(&buf,&bsz,pos,
		"<div class='tw'><table>"
		"<thead><tr>"
		"<th>IP Address</th><th>Fail Count</th>"
		"<th>Expires At</th><th>Remaining</th><th>Action</th>"
		"</tr></thead><tbody>");

	int shown=0;
	pthread_mutex_lock(&g_cfg.ban_lock);
	S_BAN_ENTRY *b=g_cfg.bans;
	while(b){
		if(b->until>now){
			char exp[32];
			struct tm tm_s;
			localtime_r(&b->until,&tm_s);
			strftime(exp,sizeof(exp),"%H:%M:%S",&tm_s);
			long secs_left=(long)(b->until-now);
			pos=buf_printf(&buf,&bsz,pos,
				"<tr>"
				"<td class='mono bold'>%s</td>"
				"<td><span class='badge bban'>%d fails</span></td>"
				"<td class='mono tm'>%s</td>"
				"<td class='mono to' id='cd_%s'>%lds</td>"
				"<td><a href='/failban?action=clear&ip=%s' class='btn bg sm'>"
				"<svg width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
				"<circle cx='12' cy='12' r='10'/><line x1='4.93' y1='4.93' x2='19.07' y2='19.07'/>"
				"</svg>Unban</a></td>"
				"</tr>",
				b->ip, b->fails, exp,
				b->ip, secs_left,
				b->ip);
			shown++;
		}
		b=b->next;
	}
	pthread_mutex_unlock(&g_cfg.ban_lock);

	if(!shown)
		pos=buf_printf(&buf,&bsz,pos,
			"<tr class='erow'><td colspan='5'>"
			"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'"
			" style='vertical-align:-3px;margin-right:6px;color:var(--gr)'>"
			"<path d='M22 11.08V12a10 10 0 1 1-5.93-9.14'/>"
			"<polyline points='22 4 12 14.01 9 11.01'/>"
			"</svg><span class='tg'>All clear</span> &mdash; no active bans"
			"</td></tr>");

	pos=buf_printf(&buf,&bsz,pos,
		"</tbody></table></div>"
		/* Countdown JS */
		"<script>"
		"(function(){"
		"  var els=document.querySelectorAll('[id^=\"cd_\"]');"
		"  setInterval(function(){"
		"    els.forEach(function(el){"
		"      var v=parseInt(el.textContent);if(v>0){el.textContent=(v-1)+'s';}else{el.textContent='expired';}"
		"    });"
		"  },1000);"
		"})();"
		"</script>");

	pos=emit_footer(&buf,&bsz,pos);
	send_response(fd,200,"OK","text/html",buf,pos);
	free(buf);
}

/* =
 *  CONFIG PAGE
 * = */
void send_page_config(int fd)
{
	int bsz=65536,pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=emit_header(&buf,&bsz,pos,"Config","config");

	char cfgpath[CFGPATH_LEN+16];
	snprintf(cfgpath,sizeof(cfgpath),"%s/" TCMG_CFG_FILE,g_cfgdir);

	FILE *fp=fopen(cfgpath,"r");
	char filebuf[16384]="";
	int  filelen=0, truncated=0;
	if(fp){
		filelen=(int)fread(filebuf,1,sizeof(filebuf)-1,fp);
		if(filelen<0)filelen=0;
		filebuf[filelen]='\0';
		if(!feof(fp))truncated=1;
		fclose(fp);
	}

	char *escaped=(char *)malloc(filelen*6+64);
	if(!escaped){free(buf);return;}
	int ei=0;
	for(int i=0;filebuf[i];i++){
		if     (filebuf[i]=='<'){memcpy(escaped+ei,"&lt;", 4);ei+=4;}
		else if(filebuf[i]=='>'){memcpy(escaped+ei,"&gt;", 4);ei+=4;}
		else if(filebuf[i]=='&'){memcpy(escaped+ei,"&amp;",5);ei+=5;}
		else                      escaped[ei++]=filebuf[i];
	}
	escaped[ei]='\0';

	pos=buf_printf(&buf,&bsz,pos,
		"<div class='ph'>"
		"  <div>"
		"    <div class='pt'>Config Editor</div>"
		"    <div class='ps'>Edit configuration directly in the browser</div>"
		"  </div>"
		"</div>");

	if(truncated)
		pos=buf_printf(&buf,&bsz,pos,
			"<div class='ib2' style='color:var(--or2);border-color:var(--ors)'>"
			"<svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<path d='M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z'/>"
			"<line x1='12' y1='9' x2='12' y2='13'/><line x1='12' y1='17' x2='12.01' y2='17'/>"
			"</svg>"
			"Config exceeds 16 KB &mdash; content is truncated. Edit the file directly on disk."
			"</div>");

	pos=buf_printf(&buf,&bsz,pos,
		"<form method='post' action='/config_save'>"
		"<div class='card'>"
		"<div class='et'>"
		"  <span class='ef'>"
		"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
		"      <path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
		"      <polyline points='14 2 14 8 20 8'/>"
		"    </svg>%s"
		"  </span>"
		"  <button type='button' class='btn bg sm' onclick='reloadPage()'>Reload</button>"
		"  <button type='submit' class='btn bp sm' id='saveBtn'>"
		"    <svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"      <path d='M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z'/>"
		"      <polyline points='17 21 17 13 7 13 7 21'/><polyline points='7 3 7 8 15 8'/>"
		"    </svg>Save &amp; Reload"
		"  </button>"
		"</div>"
		"<div class='ew'>"
		"  <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"    <path d='M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z'/>"
		"    <line x1='12' y1='9' x2='12' y2='13'/><line x1='12' y1='17' x2='12.01' y2='17'/>"
		"  </svg>"
		"  Server running &mdash; a restart may be required to apply some changes"
		"</div>"
		"<textarea class='ea' name='cfg' spellcheck='false' id='cfgArea' oninput='onEdit()'>%s</textarea>"
		"<div class='ef2'>"
		"  <span class='es'><span class='ok' id='edSt'>&#9679; Saved</span></span>"
		"  <span style='font-family:var(--mono);font-size:11px;color:var(--t2)' id='edCur'>Ln 1, Col 1</span>"
		"</div>"
		"</div>"
		"</form>"
		"<script>"
		"var _dirty=false;"
		"function onEdit(){"
		"  if(!_dirty){_dirty=true;"
		"  document.getElementById('edSt').innerHTML='<span style=\"color:var(--or2)\">&#9679; Unsaved changes</span>';}"
		"}"
		"function reloadPage(){if(_dirty&&!confirm('Discard changes?'))return;location.reload();}"
		"var ta=document.getElementById('cfgArea');"
		"function updCursor(){"
		"  var t=ta.value.substr(0,ta.selectionStart);"
		"  var ln=t.split('\n').length;"
		"  var col=t.split('\n').pop().length+1;"
		"  document.getElementById('edCur').textContent='Ln '+ln+', Col '+col;"
		"}"
		"ta.addEventListener('keyup',updCursor);"
		"ta.addEventListener('click',updCursor);"
		"/* Ctrl+S shortcut */"
		"document.addEventListener('keydown',function(e){"
		"  if((e.ctrlKey||e.metaKey)&&e.key==='s'){"
		"    e.preventDefault();"
		"    document.querySelector('form').submit();"
		"  }"
		"});"
		"</script>",
		cfgpath, escaped);
	free(escaped);

	pos=emit_footer(&buf,&bsz,pos);
	send_response(fd,200,"OK","text/html",buf,pos);
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
	char cfgpath[CFGPATH_LEN + 16];
	snprintf(cfgpath, sizeof(cfgpath), "%s/" TCMG_CFG_FILE, g_cfgdir);
	char tmppath[CFGPATH_LEN + 20];
	snprintf(tmppath, sizeof(tmppath), "%s/" TCMG_CFG_FILE ".tmp", g_cfgdir);
	FILE *fp = fopen(tmppath, "w");
	if (!fp) {
		const char *e = "<html><body><h1>Cannot write temp file</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	fputs(newcfg, fp); fclose(fp);
	S_CONFIG parsed;
	memset(&parsed, 0, sizeof(parsed));
	pthread_rwlock_init(&parsed.acc_lock, NULL);
	pthread_mutex_init(&parsed.ban_lock, NULL);
	if (!cfg_load(tmppath, &parsed)) {
		remove(tmppath); cfg_accounts_free(&parsed);
		const char *e = "<html><body><h1>Config parse error -- not saved</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}
	remove(tmppath);
	tcmg_strlcpy(parsed.config_file, cfgpath, CFGPATH_LEN);
	/* Backup */
	char bakpath[CFGPATH_LEN + 20];
	snprintf(bakpath, sizeof(bakpath), "%s/" TCMG_CFG_FILE ".bak", g_cfgdir);
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
	if (!cfg_save(&parsed)) {
		cfg_accounts_free(&parsed);
		const char *e = "<html><body><h1>Cannot write config</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	cfg_accounts_free(&parsed);
	tcmg_log("config saved successfully, reloading...");
	g_reload_cfg = 1;
	send_redirect(fd, "/config");
}

/* =
 *  LIVE LOG PAGE
 * = */
void send_page_livelog(int fd)
{
	int bsz=32768,pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=emit_header(&buf,&bsz,pos,"Live Log","livelog");

	pos=buf_printf(&buf,&bsz,pos,
		"<div class='ph'>"
		"  <div>"
		"    <div class='pt'>Live Log</div>"
		"    <div class='ps'>Real-time server output stream</div>"
		"  </div>"
		"</div>");

	/* Debug toggles */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='db'>"
		"<span style='font-size:10px;font-weight:700;color:var(--t2);"
		"text-transform:uppercase;letter-spacing:.12em;margin-right:6px'>Debug</span>");

	char masks_arr[256]; int ma=0;
	for(int i=0;i<MAX_DEBUG_LEVELS;i++){
		uint16_t m=g_dblevel_names[i].mask;
		int on=!!(g_dblevel&m);
		pos=buf_printf(&buf,&bsz,pos,
			"<a id='db%u' href='#' class='dt%s' onclick='toggleDbg(%u);return false;'"
			" title='mask 0x%04X'>%s</a>",
			m,on?" on":"",m,m,g_dblevel_names[i].name);
		ma+=snprintf(masks_arr+ma,sizeof(masks_arr)-ma,"%s%u",i?",":"",m);
	}
	int all_on=(g_dblevel==D_ALL);
	pos=buf_printf(&buf,&bsz,pos,
		"<a id='dbALL' href='#' class='dt%s' onclick='toggleAll();return false;'>ALL</a>"
		"<span class='dm'>mask: <span id='dbmask'>0x%04X</span></span>"
		"</div>",
		all_on?" on":"", g_dblevel);

	/* Controls row */
	pos=buf_printf(&buf,&bsz,pos,
		"<div class='lc'>"
		"<button class='btn bg sm' onclick='clearLog()'>"
		"<svg width='13' height='13' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"<polyline points='3 6 5 6 21 6'/>"
		"<path d='M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2'/>"
		"</svg>Clear"
		"</button>"
		"<input class='ls' id='filter' placeholder='Search log...' oninput='applyFilter()'>"
		"<label class='flex gap8' style='font-size:12px;color:var(--t1)'>"
		"<input type='checkbox' id='asc' checked>&nbsp;Auto-scroll"
		"</label>"
		"<label class='flex gap8' style='font-size:12px;color:var(--t1)'>"
		"<input type='checkbox' id='paused'>&nbsp;Pause"
		"</label>"
		"<span style='margin-left:auto;font-size:11px;color:var(--t2);display:flex;align-items:center;gap:6px'>"
		"Lines: <select class='lsel' id='maxlines'>"
		"<option value='200' selected>200</option>"
		"<option value='500'>500</option>"
		"<option value='1000'>1000</option>"
		"</select></span>"
		"</div>");

	pos=buf_printf(&buf,&bsz,pos,
		"<div id='lw' onmouseenter='hovered=1' onmouseleave='hovered=0'>"
		"<pre id='lp'></pre>"
		"</div>");

	int ring_now=log_ring_total();

	pos=buf_printf(&buf,&bsz,pos,
		"<script>"
		"var lastid=Math.max(0,%d-200);"
		"var curmask=%u;"
		"var hovered=0;"
		"var masks=[%s];"
		"var filterStr='';"

		"function updateDbgUI(){"
		"  document.getElementById('dbmask').textContent="
		"    '0x'+curmask.toString(16).toUpperCase().padStart(4,'0');"
		"  masks.forEach(function(m){"
		"    var el=document.getElementById('db'+m);if(!el)return;"
		"    el.className='dt'+(curmask&m?' on':'');"
		"  });"
		"  var a=document.getElementById('dbALL');"
		"  a.className='dt'+(curmask===65535?' on':'');"
		"}"
		"function toggleDbg(m){curmask^=m;updateDbgUI();poll();}"
		"function toggleAll(){curmask=(curmask===65535)?0:65535;updateDbgUI();poll();}"

		"function applyFilter(){"
		"  filterStr=document.getElementById('filter').value.toLowerCase();"
		"  var spans=document.getElementById('lp').children;"
		"  for(var i=0;i<spans.length;i++){"
		"    var txt=spans[i].textContent.toLowerCase();"
		"    spans[i].style.display=(!filterStr||txt.includes(filterStr))?'':'none';"
		"  }"
		"}"

		"function clearLog(){"
		"  document.getElementById('lp').innerHTML='';"
		"  fetch('/logpoll?since=999999999&debug='+curmask)"
		"    .then(r=>r.json())"
		"    .then(d=>{if(d.next!==undefined)lastid=d.next;});"
		"}"

		/* Colour mapping */
		"var CM={"
		"  hit:'#4ade80',miss:'#f87171',"
		"  webif:'#60a5fa',ban:'#fb923c',"
		"  net:'#c084fc',proto:'#22d3ee',"
		"  emu:'#86efac',conf:'#fde68a',"
		"  err:'#f87171',warn:'#fb923c'"
		"};"
		"function colorLine(l){"
		"  var ll=l.toLowerCase();"
		"  if(ll.includes('[hit]'))  return CM.hit;"
		"  if(ll.includes('[miss]')) return CM.miss;"
		"  if(ll.includes('(webif'))return CM.webif;"
		"  if(ll.includes('(ban'))  return CM.ban;"
		"  if(ll.includes('(net'))  return CM.net;"
		"  if(ll.includes('(proto'))return CM.proto;"
		"  if(ll.includes('(emu')) return CM.emu;"
		"  if(ll.includes('(conf'))return CM.conf;"
		"  if(ll.includes('error'))return CM.err;"
		"  if(ll.includes('warn')) return CM.warn;"
		"  return null;"
		"}"

		"function appendLines(lines){"
		"  var pre=document.getElementById('lp');"
		"  var maxl=parseInt(document.getElementById('maxlines').value)||200;"
		"  lines.forEach(function(line){"
		"    var span=document.createElement('span');"
		"    var c=colorLine(line);"
		"    if(c)span.style.color=c;"
		"    span.textContent=line+'\n';"
		"    if(filterStr&&!line.toLowerCase().includes(filterStr))"
		"      span.style.display='none';"
		"    pre.appendChild(span);"
		"  });"
		"  while(pre.children.length>maxl)pre.removeChild(pre.firstChild);"
		"  var w=document.getElementById('lw');"
		"  if(!hovered&&document.getElementById('asc').checked)"
		"    w.scrollTop=w.scrollHeight;"
		"}"

		"function poll(){"
		"  if(document.getElementById('paused').checked)return;"
		"  fetch('/logpoll?since='+lastid+'&debug='+curmask)"
		"    .then(r=>r.json())"
		"    .then(d=>{"
		"      if(d.debug!==undefined&&d.debug!==curmask){curmask=d.debug;updateDbgUI();}"
		"      if(d.next!==undefined)lastid=d.next;"
		"      if(d.lines&&d.lines.length)appendLines(d.lines);"
		"    }).catch(()=>{});"
		"}"
		"updateDbgUI();"
		"setInterval(poll,1000);poll();"
		"</script>",
		ring_now, g_dblevel, masks_arr);

	pos=emit_footer(&buf,&bsz,pos);
	send_response(fd,200,"OK","text/html",buf,pos);
	free(buf);
}

/* =
 *  LOG POLL API
 * = */
void send_logpoll(int fd, const char *qs)
{
	char dbg_s[16], since_s[32];
	get_param(qs, "debug", dbg_s,  sizeof(dbg_s));
	get_param(qs, "since", since_s, sizeof(since_s));

	if (dbg_s[0])
	{
		long v = strtol(dbg_s, NULL, 0);
		if (v >= 0 && v <= 0xFFFF && (uint16_t)v != g_dblevel)
		{
			g_dblevel = (uint16_t)v;
			tcmg_log_dbg(D_WEBIF, "livelog debug_level → %u", g_dblevel);
		}
	}

	int32_t from_id = since_s[0] ? (int32_t)atoi(since_s) : 0;
	if (from_id < 0) from_id = 0;

	char *lines[WEB_MAX_LINES_POLL];
	int32_t next_id;
	int32_t count = log_ring_since(from_id, lines, WEB_MAX_LINES_POLL, &next_id);

	int bsz = count * 256 + 256, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = buf_printf(&buf, &bsz, pos,
		"{\"debug\":%u,\"next\":%d,\"lines\":[",
		g_dblevel, next_id);

	for (int i = 0; i < count; i++)
	{
		char escaped[512];
		int  ei = 0;
		for (const char *p = lines[i]; *p && ei < (int)sizeof(escaped) - 4; p++)
		{
			if (*p == '"'  || *p == '\\') escaped[ei++] = '\\';
			if (*p == '\n' || *p == '\r') continue;
			escaped[ei++] = *p;
		}
		escaped[ei] = '\0';
		free(lines[i]);
		pos = buf_printf(&buf, &bsz, pos, "%s\"%s\"", i ? "," : "", escaped);
	}
	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

/* =
 *  API /status  (JSON)
 * = */
void send_api_status(int fd)
{
	int bsz = 16384, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	time_t now = time(NULL);
	char upstr[32];
	format_uptime(now - g_start_time, upstr, sizeof(upstr));

	S_STATS st = aggregate_stats();
	int64_t cw_found = st.cw_found, cw_not = st.cw_not;
	int nbans = st.nbans;
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	int naccounts = g_cfg.naccounts;
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	int64_t ecm_total = cw_found + cw_not;
	double  hitrate   = ecm_total > 0 ? (double)cw_found * 100.0 / (double)ecm_total : 0.0;

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
		TCMG_VERSION,
		TCMG_BUILD_TIME,
		(long)(now - g_start_time),
		upstr,
		g_cfg.port,
		g_active_conns,
		naccounts,
		nbans,
		(long long)cw_found,
		(long long)cw_not,
		(long long)ecm_total,
		hitrate,
		g_dblevel);

	pthread_mutex_lock(&g_clients_mtx);
	bool first = true;
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
	{
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->account) continue;
		char conn_str[32], idle_str[32];
		format_uptime(now - cl->connect_time, conn_str, sizeof(conn_str));
		format_uptime(now - cl->account->last_seen, idle_str, sizeof(idle_str));
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

/* =
 *  SHUTDOWN PAGE
 * = */
void send_page_shutdown(int fd, const char *qs)
{
	char confirm[8];
	get_param(qs,"confirm",confirm,sizeof(confirm));

	int bsz=8192,pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=emit_header(&buf,&bsz,pos,"Shutdown","shutdown");

	if(strcmp(confirm,"yes")==0){
		tcmg_log("shutdown requested via webif");
		g_running=0;
		pos=buf_printf(&buf,&bsz,pos,
			"<div class='done-card'>"
			"<div class='dico danger'>"
			"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"</svg></div>"
			"<h2>Server Stopped</h2>"
			"<p>TCMG has been shut down.<br>All connections were dropped.</p>"
			"</div>");
	} else {
		pos=buf_printf(&buf,&bsz,pos,
			"<div class='dlg'>"
			"<div class='dico danger'>"
			"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"</svg></div>"
			"<h2>Shutdown Server?</h2>"
			"<p>All active connections will be dropped immediately.<br>"
			"The process will exit and will <strong>not</strong> restart.</p>"
			"<div class='da'>"
			"<a href='/shutdown?confirm=yes' class='btn bd_'>"
			"<svg width='14' height='14' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"</svg>Confirm Shutdown</a>"
			"<a href='/status' class='btn bg'>Cancel</a>"
			"</div>"
			"</div>");
	}

	pos=emit_footer(&buf,&bsz,pos);
	send_response(fd,200,"OK","text/html",buf,pos);
	free(buf);
}

/* =
 *  RESTART PAGE
 * = */
void send_page_restart(int fd, const char *qs)
{
	char confirm[8];
	get_param(qs,"confirm",confirm,sizeof(confirm));

	int bsz=8192,pos=0;
	char *buf=(char *)malloc(bsz);
	if(!buf)return;

	pos=emit_header(&buf,&bsz,pos,"Restart","restart");

	if(strcmp(confirm,"yes")==0){
		tcmg_log("restart requested via webif");
		g_restart=1;
		g_running=0;
		pos=buf_printf(&buf,&bsz,pos,
			"<div class='done-card'>"
			"<div class='dico info'>"
			"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"<polyline points='23 4 23 10 17 10'/>"
			"<path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
			"</svg></div>"
			"<h2>Restarting&hellip;</h2>"
			"<p>Config will be reloaded.<br>Redirecting when back online&hellip;</p>"
			"<div class='spill' style='display:inline-flex;margin-top:6px'>"
			"<div class='pulse sm'></div>&nbsp;Waiting for server&hellip;</div>"
			"<script>"
			"setTimeout(function(){"
			"  var t=setInterval(function(){"
			"    fetch('/api/status',{cache:'no-store'})"
			"      .then(function(){clearInterval(t);location.href='/status';})"
			"      .catch(function(){});"
			"  },1500);"
			"},3500);"
			"</script>"
			"</div>");
	} else {
		pos=buf_printf(&buf,&bsz,pos,
			"<div class='dlg'>"
			"<div class='dico info'>"
			"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"<polyline points='23 4 23 10 17 10'/>"
			"<path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
			"</svg></div>"
			"<h2>Restart Server?</h2>"
			"<p>Active connections will be dropped.<br>"
			"Configuration will be reloaded on restart.</p>"
			"<div class='da'>"
			"<a href='/restart?confirm=yes' class='btn bp'>"
			"<svg width='14' height='14' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<polyline points='23 4 23 10 17 10'/>"
			"<path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
			"</svg>Confirm Restart</a>"
			"<a href='/status' class='btn bg'>Cancel</a>"
			"</div>"
			"</div>");
	}

	pos=emit_footer(&buf,&bsz,pos);
	send_response(fd,200,"OK","text/html",buf,pos);
	free(buf);
}

void handle_request(int fd, const char *client_ip)
{
	char req[WEB_BUF_SIZE];
	int  rlen = 0;

#ifdef TCMG_OS_WINDOWS
	DWORD tv = (DWORD)(WEB_READ_TIMEOUT_S * 1000);
#else
	struct timeval tv = { WEB_READ_TIMEOUT_S, 0 };
#endif
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, SO_CAST(&tv), sizeof(tv));

	while (rlen < (int)sizeof(req) - 1)
	{
		int n = (int)recv(fd, RECV_CAST(req + rlen), sizeof(req) - 1 - rlen, 0);
		if (n <= 0) break;
		rlen += n;
		req[rlen] = '\0';
		if (strstr(req, "\r\n\r\n")) break;
	}
	if (rlen < 10) return;

	char method[8] = {0}, uri[512] = {0};
	sscanf(req, "%7s %511s", method, uri);

	char path[512];
	char *qs = NULL;
	tcmg_strlcpy(path, uri, sizeof(path));
	char *qmark = strchr(path, '?');
	if (qmark) { *qmark = '\0'; qs = qmark + 1; }

	if (strcmp(path, "/logpoll") != 0)
		tcmg_log_dbg(D_WEBIF, "%s %s", method, uri);

	/* Auth */
	int authed = 0;
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) authed = 1;

	if (!authed) {
		char *cp = strstr(req, "\r\nCookie:");
		if (!cp) cp = strstr(req, "\nCookie:");
		if (cp) {
			cp = strchr(cp, ':'); if (cp) cp++;
			while (cp && *cp == ' ') cp++;
			char tok[WEB_SESSION_LEN + 1];
			const char *sess = cookie_get_session(cp, tok, sizeof(tok));
			if (sess && session_check(sess)) authed = 1;
		}
	}
	if (!authed) {
		char *ap = strstr(req, "\r\nAuthorization:");
		if (!ap) ap = strstr(req, "\nAuthorization:");
		if (ap) {
			ap = strchr(ap, ':'); if (ap) ap++;
			while (ap && *ap == ' ') ap++;
			if (check_auth(ap)) authed = 1;
		}
	}

	/* POST /login */
	if (strcmp(path, "/login") == 0 && strcmp(method, "POST") == 0)
	{
		char *body_start = strstr(req, "\r\n\r\n");
		if (body_start) body_start += 4;
		char u[128] = {0}, p2[128] = {0};
		form_get(body_start, "u", u, sizeof(u));
		form_get(body_start, "p", p2, sizeof(p2));
		int ok = (g_cfg.webif_user[0] == '\0' && g_cfg.webif_pass[0] == '\0') ||
		         (ct_streq(u, g_cfg.webif_user) && ct_streq(p2, g_cfg.webif_pass));
		if (ok) {
			char token[WEB_SESSION_LEN + 1];
			session_create(token);
			tcmg_log_dbg(D_WEBIF, "login OK for '%s' from %s", u, client_ip);
			send_redirect_with_cookie(fd, "/status", token);
		} else {
			tcmg_log("login FAIL for '%s' from %s", u, client_ip);
			send_login_page(fd, 1);
		}
		return;
	}

	if (!authed) {
		if (strcmp(path, "/login") == 0) send_login_page(fd, 0);
		else send_redirect(fd, "/login");
		return;
	}

	/* Route */
	if (strcmp(path, "/") == 0 || strcmp(path, "/login") == 0)
		send_redirect(fd, "/status");
	else if (strcmp(path, "/status") == 0) {
		char killstr[16];
		get_param(qs ? qs : "", "kill", killstr, sizeof(killstr));
		if (killstr[0]) {
			uint32_t tid = (uint32_t)strtoul(killstr, NULL, 10);
			char killed_user[64] = "";
			get_param(qs ? qs : "", "user", killed_user, sizeof(killed_user));
			client_kill_by_tid(tid);
			tcmg_log("disconnect user '%s' tid=%u (by webif)",
			         killed_user[0] ? killed_user : "?", tid);
		}
		send_page_status(fd);
	}
	else if (strcmp(path, "/users")   == 0) send_page_users(fd);
	else if (strcmp(path, "/failban") == 0) send_page_failban(fd, qs ? qs : "");
	else if (strcmp(path, "/config")  == 0) send_page_config(fd);
	else if (strcmp(path, "/config_save") == 0 && strcmp(method, "POST") == 0)
	{
		const char *body_start = strstr(req, "\r\n\r\n");
		if (body_start) body_start += 4;
		int body_len = body_start ? (int)(req + rlen - body_start) : 0;
		char post_extra[16384] = "";
		if (body_start && body_len < (int)sizeof(post_extra) - 1)
		{
			memcpy(post_extra, body_start, body_len);
			char *clh = strstr(req, "Content-Length:");
			if (!clh) clh = strstr(req, "content-length:");
			if (clh) {
				int clen = atoi(clh + 15);
				while (body_len < clen && body_len < (int)sizeof(post_extra) - 1) {
					int n = (int)recv(fd, RECV_CAST(post_extra + body_len),
					                  sizeof(post_extra) - 1 - body_len, 0);
					if (n <= 0) break;
					body_len += n;
					post_extra[body_len] = '\0';
				}
			}
		}
		handle_config_save(fd, post_extra);
	}
	else if (strcmp(path, "/livelog")  == 0) send_page_livelog(fd);
	else if (strcmp(path, "/logpoll")  == 0) send_logpoll(fd, qs ? qs : "");
	else if (strcmp(path, "/restart")  == 0) send_page_restart(fd, qs ? qs : "");
	else if (strcmp(path, "/shutdown") == 0) send_page_shutdown(fd, qs ? qs : "");
	else if (strcmp(path, "/tvcas")    == 0) send_page_tvcas(fd);
	else if (strcmp(path, "/api/status") == 0) send_api_status(fd);
	else if (strcmp(path, "/api/reload") == 0) {
		g_reload_cfg = 1;
		const char *j = "{\"ok\":true,\"msg\":\"reload scheduled\"}";
		send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
	}
	else if (strcmp(path, "/api/restart") == 0) {
		tcmg_log("restart requested via API");
		g_restart = 1;
		g_running = 0;
		const char *j = "{\"ok\":true,\"msg\":\"restart initiated\"}";
		send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
	}
	else if (strcmp(path, "/api/resetstats") == 0) {
		handle_reset_stats();
		const char *j = "{\"ok\":true,\"msg\":\"stats reset\"}";
		send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
	}
	else {
		const char *msg = "<html><body style='background:#090d14;color:#e8f0fe;font-family:monospace;"
		                  "display:flex;align-items:center;justify-content:center;height:100vh'>"
		                  "<div><h1 style='color:#3b82f6'>404</h1><p>Not Found</p>"
		                  "<a href='/status' style='color:#60a5fa'>← Back to Status</a></div></body></html>";
		send_response(fd, 404, "Not Found", "text/html", msg, (int)strlen(msg));
	}
}

