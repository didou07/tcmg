#define MODULE_LOG_PREFIX "webif"
#include "../globals.h"
#include "internal/proto.h"

S_SERVER_STATS collect_stats(void)
{
	S_SERVER_STATS s;
	memset(&s, 0, sizeof(s));
	time_t now = time(NULL);

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	s.naccounts = g_cfg.naccounts;
	for (const S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		s.cw_found += a->cw_found;
		s.cw_not   += a->cw_not;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	pthread_mutex_lock(&g_cfg.ban_lock);
	for (int _bi = 0; _bi < BAN_BUCKETS; _bi++)
		for (const S_BAN_ENTRY *b = g_cfg.ban_table[_bi]; b; b = b->next)
			if (now < b->until) s.nbans++;
	pthread_mutex_unlock(&g_cfg.ban_lock);

	s.ecm_total    = s.cw_found + s.cw_not;
	s.hit_rate     = s.ecm_total > 0
	               ? (double)s.cw_found * 100.0 / (double)s.ecm_total
	               : 0.0;
	s.active_conns = g_active_conns;
	s.uptime_s     = now - g_start_time;
	format_uptime(s.uptime_s, s.uptime_str, sizeof(s.uptime_str));
	return s;
}

void handle_reset_stats(void)
{
	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	for (S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) {
		pthread_mutex_lock(&a->stat_mtx);
		a->ecm_total        = 0;
		a->cw_found         = 0;
		a->cw_not           = 0;
		a->cw_time_total_ms = 0;
		a->cw_time_min_ms   = 0;
		a->cw_time_max_ms   = 0;
		a->first_login      = 0;
		a->last_seen        = 0;
		pthread_mutex_unlock(&a->stat_mtx);
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	tcmg_log("%s", "webif: all user stats reset");
}
