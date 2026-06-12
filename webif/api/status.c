#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

void send_api_status(int fd)
{
	int   bsz = 16384, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	S_SERVER_STATS st  = collect_stats();
	time_t         now = time(NULL);

	pos = buf_printf(&buf, &bsz, pos,
		"{"
		"\"version\":\"%s\","
		"\"build\":\"%s\","
		"\"uptime_s\":%ld,"
		"\"uptime_str\":\"%s\","
		"\"newcamd_port\":%d,"
		"\"cccam_port\":%d,"
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
		g_cfg.newcamd_port, g_cfg.cccam_port, st.active_conns,
		st.naccounts, st.nbans,
		(long long)st.cw_found, (long long)st.cw_not, (long long)st.ecm_total,
		st.hit_rate, g_dblevel);

	pthread_mutex_lock(&g_clients_mtx);
	bool first = true;
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++) {
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->account) continue;
		char conn_str[32], idle_str[32];
		char esc_user[256], esc_ip[128], esc_proto[64], esc_chan[256];
		format_uptime(now - cl->connect_time,         conn_str, sizeof(conn_str));
		format_uptime(now - cl->account->last_seen,   idle_str, sizeof(idle_str));
		json_escape(cl->user,                              esc_user,  sizeof(esc_user));
		json_escape(cl->ip,                               esc_ip,    sizeof(esc_ip));
		json_escape(cl->proto,                            esc_proto, sizeof(esc_proto));
		json_escape(cl->last_channel[0] ? cl->last_channel : "", esc_chan, sizeof(esc_chan));
		pos = buf_printf(&buf, &bsz, pos,
			"%s{"
			"\"user\":\"%s\","
			"\"ip\":\"%s\","
			"\"proto\":\"%s\","
			"\"caid\":\"%04X\","
			"\"sid\":\"%04X\","
			"\"channel\":\"%s\","
			"\"connected\":\"%s\","
			"\"idle\":\"%s\","
			"\"thread_id\":%u"
			"}",
			first ? "" : ",",
			esc_user, esc_ip, esc_proto,
			cl->last_caid, cl->last_srvid,
			esc_chan,
			conn_str, idle_str,
			cl->thread_id);
		first = false;
	}
	pthread_mutex_unlock(&g_clients_mtx);

	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}
