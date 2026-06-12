#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

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

void send_page_status(int fd)
{
	PAGE_INIT(32768)

	pos = emit_header(&buf, &bsz, pos, "Dashboard", "status");

	S_SERVER_STATS st = collect_stats();
	char hrstr[16];
	snprintf(hrstr, sizeof(hrstr),
	         st.ecm_total > 0 ? "%.1f%%" : "0.0%%", st.hit_rate);

	char hbf_extra[192];
	snprintf(hbf_extra, sizeof(hbf_extra),
	         "  <div class='hbw' style='margin-top:4px'>"
	         "<div class='hbf' id='p_hbf' style='width:%.0f%%'></div></div>",
	         st.hit_rate);

	int dis_cnt = 0, exp_cnt = 0;
	{
		time_t now = time(NULL);
		pthread_rwlock_rdlock(&g_cfg.acc_lock);
		for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
			if (!a->enabled) dis_cnt++;
			if (a->expirationdate > 0 && now > a->expirationdate) exp_cnt++;
		}
		pthread_rwlock_unlock(&g_cfg.acc_lock);
	}

	pos = buf_printf(&buf, &bsz, pos, "<div class='cg'>");

	pos = emit_stat_card(&buf, &bsz, pos, "bl", ICO_CLOCK,
	    "Uptime", "p_up", st.uptime_str, "since start", NULL);

	{
		char conn_val[16], conn_sub[48];
		snprintf(conn_val, sizeof(conn_val), "%d", st.active_conns);
		snprintf(conn_sub, sizeof(conn_sub), "of <span id='p_acc'>%d</span> accounts", st.naccounts);
		pos = emit_stat_card(&buf, &bsz, pos,
		    st.active_conns > 0 ? "gr" : "bl", ICO_USERS2,
		    "Connections", "p_conn", conn_val, conn_sub, NULL);
	}

	{
		char dis_val[8], exp_val[8];
		snprintf(dis_val, sizeof(dis_val), "%d", dis_cnt);
		snprintf(exp_val, sizeof(exp_val), "%d", exp_cnt);

		pos = emit_stat_card(&buf, &bsz, pos,
		    dis_cnt > 0 ? "or" : "bl", ICO_KEY,
		    "Disabled", "p_dis", dis_val, "accounts", NULL);

		pos = emit_stat_card(&buf, &bsz, pos,
		    exp_cnt > 0 ? "re" : "bl", ICO_WARN,
		    "Expired", "p_exp", exp_val, "accounts expired", NULL);
	}

	{
		char ecm_val[24];
		snprintf(ecm_val, sizeof(ecm_val), "%lld", (long long)st.ecm_total);
		pos = emit_stat_card(&buf, &bsz, pos, "vi", ICO_ZAP,
		    "ECM Total", "p_ecm", ecm_val, "requests", NULL);
	}

	{
		char cw_val[24];
		snprintf(cw_val, sizeof(cw_val), "%lld", (long long)st.cw_found);
		pos = emit_stat_card(&buf, &bsz, pos, "gr", ICO_CHECK,
		    "CW Found", "p_hit", cw_val, "cache hits", NULL);
	}

	{
		char miss_val[24];
		snprintf(miss_val, sizeof(miss_val), "%lld", (long long)st.cw_not);
		pos = emit_stat_card(&buf, &bsz, pos,
		    st.cw_not > 0 ? "re" : "bl", ICO_X,
		    "CW Miss", "p_miss", miss_val, "not found", NULL);
	}

	pos = emit_stat_card(&buf, &bsz, pos, "cy", ICO_PERCENT,
	    "Hit Rate", "p_hr", hrstr, "", hbf_extra);

	{
		char ban_val[8];
		snprintf(ban_val, sizeof(ban_val), "%d", st.nbans);
		pos = emit_stat_card(&buf, &bsz, pos,
		    st.nbans > 0 ? "or" : "bl", ICO_SHIELD,
		    "Banned IPs", "p_ban", ban_val,
		    "<a href='/failban' style='color:inherit;opacity:.7;font-size:11px'>view list &rarr;</a>",
		    NULL);
	}

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='sc bl'>"
		"<div class='si_'>"
		"<svg width='22' height='22' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
		"<polyline points='1 4 1 10 7 10'/>"
		"<path d='M3.51 15a9 9 0 1 0 .49-4.08'/>"
		"</svg></div>"
		"<div class='sb_'>"
		"  <div class='sl_'>Actions</div>"
		"  <div class='sv' style='font-size:13px;font-weight:600'>"
		"    <a href='#' onclick=\"if(confirm('Reset all stats?')){fetch('/api/resetstats').then(()=>location.reload());}return false\""
		"       style='color:inherit;text-decoration:none'>Reset Stats</a>"
		"  </div>"
		"  <div class='sd'>clear counters</div>"
		"</div><div class='sg'></div></div>");

	pos = buf_printf(&buf, &bsz, pos, "</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='shd' style='justify-content:center'>"
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
		"</tbody></table></div>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}
