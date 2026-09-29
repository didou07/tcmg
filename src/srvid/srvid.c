#include "srvid.h"
#include "../core/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#define SRVID_MIN_BUCKETS  64
#define MAX_CAIDS_PER_LINE 16

typedef struct {
	uint32_t key;
	uint32_t name_off;
} S_SRVID_ENTRY;

typedef struct {
	S_SRVID_ENTRY *tbl;
	uint32_t       mask;
	char          *pool;
	size_t         pool_len;
	size_t         pool_cap;
	int            count;
} S_SRVID_TABLE;

static S_SRVID_TABLE  *g_srvid     = NULL;
static pthread_mutex_t g_srvid_mtx = PTHREAD_MUTEX_INITIALIZER;

static inline uint32_t hash_key(uint32_t k)
{
	k ^= k >> 16;
	k *= 0x45d9f3bU;
	k ^= k >> 16;
	return k;
}

static void tbl_free(S_SRVID_TABLE *t)
{
	if (!t) return;
	free(t->tbl);
	free(t->pool);
	free(t);
}

static void tbl_insert(S_SRVID_TABLE *t, uint32_t key, uint32_t name_off)
{
	uint32_t h = hash_key(key) & t->mask;
	for (uint32_t i = 0; i <= t->mask; i++)
	{
		uint32_t idx = (h + i) & t->mask;
		if (!t->tbl[idx].key)
		{
			t->tbl[idx].key = key;
			t->tbl[idx].name_off = name_off;
			return;
		}
		if (t->tbl[idx].key == key)
			return;
	}
}

static uint32_t pool_add(S_SRVID_TABLE *t, const char *name)
{
	size_t n = strnlen(name, SRVID_NAME_MAX - 1);
	if (t->pool_len + n + 1 > t->pool_cap)
	{
		size_t ncap = t->pool_cap ? t->pool_cap * 2 : 4096;
		while (ncap < t->pool_len + n + 1) ncap *= 2;
		char *np = (char *)realloc(t->pool, ncap);
		if (!np) return UINT32_MAX;
		t->pool = np;
		t->pool_cap = ncap;
	}
	uint32_t off = (uint32_t)t->pool_len;
	memcpy(t->pool + off, name, n);
	t->pool[off + n] = '\0';
	t->pool_len += n + 1;
	return off;
}

static void trim(char *s)
{
	char *p = s;
	while (isspace((unsigned char)*p)) p++;
	if (p != s) memmove(s, p, strlen(p) + 1);
	char *q = s + strlen(s) - 1;
	while (q >= s && isspace((unsigned char)*q)) *q-- = '\0';
}

int srvid_load(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;

	char line[512];

	size_t pairs = 0;
	while (fgets(line, sizeof(line), f))
	{
		char *colon = strchr(line, ':');
		char *pipe  = strchr(line, '|');
		if (!colon || !pipe || colon > pipe) continue;
		pairs++;
		for (char *c = colon; c < pipe; c++)
			if (*c == ',') pairs++;
	}
	rewind(f);

	uint32_t nb = SRVID_MIN_BUCKETS;
	while ((size_t)nb < pairs * 2 && nb < (1u << 24)) nb <<= 1;

	S_SRVID_TABLE *newtbl = (S_SRVID_TABLE *)calloc(1, sizeof(S_SRVID_TABLE));
	if (!newtbl) { fclose(f); return -1; }
	newtbl->tbl = (S_SRVID_ENTRY *)calloc(nb, sizeof(S_SRVID_ENTRY));
	if (!newtbl->tbl) { tbl_free(newtbl); fclose(f); return -1; }
	newtbl->mask = nb - 1;

	while (fgets(line, sizeof(line), f))
	{
		trim(line);
		if (!line[0] || line[0] == '#') continue;

		char *pipe = strchr(line, '|');
		if (!pipe) continue;
		*pipe++ = '\0';

		char *name_end = strchr(pipe, '|');
		if (name_end) *name_end = '\0';
		trim(pipe);
		if (!*pipe) continue;

		char *colon = strchr(line, ':');
		if (!colon) continue;
		*colon++ = '\0';

		trim(line);
		unsigned sid_u = 0;
		if (sscanf(line, "%X", &sid_u) != 1 || !sid_u) continue;
		uint16_t sid = (uint16_t)sid_u;

		uint32_t name_off = UINT32_MAX;
		char *saveptr = NULL;
		char *tok = strtok_r(colon, ",", &saveptr);
		int ncaid = 0;

		while (tok)
		{
			trim(tok);
			unsigned caid_u = 0;
			if (sscanf(tok, "%X", &caid_u) == 1 && caid_u)
			{
				if (name_off == UINT32_MAX)
				{
					name_off = pool_add(newtbl, pipe);
					if (name_off == UINT32_MAX) break;
				}
				uint32_t key = ((uint32_t)(uint16_t)caid_u << 16) | sid;
				tbl_insert(newtbl, key, name_off);
				ncaid++;
			}
			tok = strtok_r(NULL, ",", &saveptr);
		}

		if (ncaid) newtbl->count++;
	}
	fclose(f);

	if (newtbl->pool && newtbl->pool_len < newtbl->pool_cap)
	{
		char *tp = (char *)realloc(newtbl->pool, newtbl->pool_len);
		if (tp) { newtbl->pool = tp; newtbl->pool_cap = newtbl->pool_len; }
	}

	const int count = newtbl->count;

	pthread_mutex_lock(&g_srvid_mtx);
	S_SRVID_TABLE *old = g_srvid;
	g_srvid = newtbl;
	pthread_mutex_unlock(&g_srvid_mtx);

	tbl_free(old);
	return count;
}

void srvid_lookup_copy(uint16_t caid, uint16_t sid, char *buf, size_t bufsz)
{
	if (!buf || !bufsz) return;
	buf[0] = '\0';
	pthread_mutex_lock(&g_srvid_mtx);
	S_SRVID_TABLE *t = g_srvid;
	if (!t || !sid || !caid) { pthread_mutex_unlock(&g_srvid_mtx); return; }

	uint32_t key = ((uint32_t)caid << 16) | sid;
	uint32_t h   = hash_key(key) & t->mask;

	for (uint32_t i = 0; i <= t->mask; i++)
	{
		uint32_t idx = (h + i) & t->mask;
		if (!t->tbl[idx].key) break;
		if (t->tbl[idx].key == key)
		{
			const char *name = t->pool + t->tbl[idx].name_off;
			size_t n = strlen(name);
			if (n >= bufsz) n = bufsz - 1;
			memcpy(buf, name, n);
			buf[n] = '\0';
			break;
		}
	}
	pthread_mutex_unlock(&g_srvid_mtx);
}

const char *srvid_lookup(uint16_t caid, uint16_t sid)
{
	static __thread char t_name[SRVID_NAME_MAX];
	srvid_lookup_copy(caid, sid, t_name, sizeof(t_name));
	return t_name[0] ? t_name : NULL;
}

int srvid_write_default(const char *path)
{
	FILE *f = fopen(path, "w");
	if (!f) return 0;

	fprintf(f,
	"# tcmg.srvid2 — channel name database\n"
	"# Format: SID:CAID[,CAID2,...]|Channel Name|type||provider\n"
	"# Generated automatically. Add or edit entries as needed.\n"
	"# SID and CAID values are hexadecimal.\n"
	"\n"
	"# ── beIN SPORTS ─────────────────────────────────────────────────────────────\n"
	"0101:0B00,0B01,0B02|beIN SPORTS HD 1|TV||beIN SPORTS\n"
	"0102:0B00,0B01,0B02|beIN SPORTS HD 2|TV||beIN SPORTS\n"
	"0103:0B00,0B01,0B02|beIN SPORTS HD 3|TV||beIN SPORTS\n"
	);

	fclose(f);
	return 1;
}

void srvid_free(void)
{
	pthread_mutex_lock(&g_srvid_mtx);
	if (g_srvid) { tbl_free(g_srvid); g_srvid = NULL; }
	pthread_mutex_unlock(&g_srvid_mtx);
}
