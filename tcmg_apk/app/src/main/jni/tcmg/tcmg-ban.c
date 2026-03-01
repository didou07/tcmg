
#define MODULE_LOG_PREFIX "ban"
#include "tcmg-globals.h"

static S_BAN_ENTRY *ban_find_locked(const char *ip)
{
	S_BAN_ENTRY *e;
	for (e = g_cfg.bans; e; e = e->next)
		if (strncmp(e->ip, ip, MAXIPLEN) == 0)
			return e;
	return NULL;
}

/* Prune expired entries -- call under ban_lock. */
static void ban_prune_locked(void)
{
	S_BAN_ENTRY **pp = &g_cfg.bans;
	time_t now = time(NULL);
	while (*pp)
	{
		S_BAN_ENTRY *e = *pp;
		if (e->until > 0 && now >= e->until)
		{
			*pp = e->next;
			free(e);
		}
		else
		{
			pp = &e->next;
		}
	}
}

bool ban_is_banned(const char *ip)
{
	bool    banned = false;
	time_t  now    = time(NULL);

	pthread_mutex_lock(&g_cfg.ban_lock);
	ban_prune_locked();
	S_BAN_ENTRY *e = ban_find_locked(ip);
	if (e && e->until > 0 && now < e->until)
		banned = true;
	pthread_mutex_unlock(&g_cfg.ban_lock);

	return banned;
}

void ban_record_fail(const char *ip)
{
	pthread_mutex_lock(&g_cfg.ban_lock);

	S_BAN_ENTRY *e = ban_find_locked(ip);
	if (!e)
	{
		e = (S_BAN_ENTRY *)calloc(1, sizeof(S_BAN_ENTRY));
		if (!e) { pthread_mutex_unlock(&g_cfg.ban_lock); return; }
		strncpy(e->ip, ip, MAXIPLEN - 1);
		e->next     = g_cfg.bans;
		g_cfg.bans  = e;
	}

	e->fails++;
	tcmg_log_dbg(D_BAN, "%s fail_count=%d/%d", ip, e->fails, BAN_MAX_FAILS);
	if (e->fails >= BAN_MAX_FAILS)
	{
		e->until = time(NULL) + BAN_SECS;
		tcmg_log("banned %s for %ds", ip, BAN_SECS);
	}

	pthread_mutex_unlock(&g_cfg.ban_lock);
}

void ban_record_ok(const char *ip)
{
	pthread_mutex_lock(&g_cfg.ban_lock);

	S_BAN_ENTRY **pp = &g_cfg.bans;
	while (*pp)
	{
		if (strncmp((*pp)->ip, ip, MAXIPLEN) == 0)
		{
			S_BAN_ENTRY *e = *pp;
			*pp = e->next;
			free(e);
			break;
		}
		pp = &(*pp)->next;
	}

	pthread_mutex_unlock(&g_cfg.ban_lock);
}

void ban_free_all(void)
{
	pthread_mutex_lock(&g_cfg.ban_lock);
	S_BAN_ENTRY *e = g_cfg.bans;
	while (e)
	{
		S_BAN_ENTRY *next = e->next;
		free(e);
		e = next;
	}
	g_cfg.bans = NULL;
	pthread_mutex_unlock(&g_cfg.ban_lock);
}
