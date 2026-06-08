#include "globals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#define SRVID_BUCKETS      16384
#define SRVID_MASK         (SRVID_BUCKETS - 1)
#define SRVID_NAME_MAX     80
#define MAX_CAIDS_PER_LINE 16

typedef struct {
	uint32_t key;
	char     name[SRVID_NAME_MAX];
} S_SRVID_ENTRY;

typedef struct {
	S_SRVID_ENTRY *tbl;
	int            count;
} S_SRVID_TABLE;

static S_SRVID_TABLE  *g_srvid     = NULL;
static pthread_mutex_t g_srvid_mtx = PTHREAD_MUTEX_INITIALIZER;

static inline uint32_t hash_key(uint32_t k)
{
	k ^= k >> 16;
	k *= 0x45d9f3bU;
	k ^= k >> 16;
	return k & SRVID_MASK;
}

static void tbl_insert(S_SRVID_ENTRY *tbl, uint32_t key, const char *name)
{
	uint32_t h = hash_key(key);
	for (int i = 0; i < SRVID_BUCKETS; i++)
	{
		uint32_t idx = (h + i) & SRVID_MASK;
		if (!tbl[idx].key)
		{
			tbl[idx].key = key;
			tcmg_strlcpy(tbl[idx].name, name, SRVID_NAME_MAX);
			return;
		}
		if (tbl[idx].key == key)
			return;
	}
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

	S_SRVID_TABLE *newtbl = calloc(1, sizeof(S_SRVID_TABLE));
	if (!newtbl) { fclose(f); return -1; }
	newtbl->tbl = calloc(SRVID_BUCKETS, sizeof(S_SRVID_ENTRY));
	if (!newtbl->tbl) { free(newtbl); fclose(f); return -1; }

	char line[512];

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

		char *saveptr = NULL;
		char *tok = strtok_r(colon, ",", &saveptr);
		int ncaid = 0;

		while (tok)
		{
			trim(tok);
			unsigned caid_u = 0;
			if (sscanf(tok, "%X", &caid_u) == 1 && caid_u)
			{
				uint32_t key = ((uint32_t)(uint16_t)caid_u << 16) | sid;
				tbl_insert(newtbl->tbl, key, pipe);
				ncaid++;
			}
			tok = strtok_r(NULL, ",", &saveptr);
		}

		if (ncaid) newtbl->count++;
	}
	fclose(f);

	pthread_mutex_lock(&g_srvid_mtx);
	S_SRVID_TABLE *old = g_srvid;
	g_srvid = newtbl;
	pthread_mutex_unlock(&g_srvid_mtx);

	if (old) { free(old->tbl); free(old); }
	return newtbl->count;
}

const char *srvid_lookup(uint16_t caid, uint16_t sid)
{
	pthread_mutex_lock(&g_srvid_mtx);
	S_SRVID_TABLE *t = g_srvid;
	if (!t || !sid || !caid) { pthread_mutex_unlock(&g_srvid_mtx); return NULL; }

	uint32_t key = ((uint32_t)caid << 16) | sid;
	uint32_t h   = hash_key(key);
	const char *found = NULL;

	for (int i = 0; i < SRVID_BUCKETS; i++)
	{
		uint32_t idx = (h + i) & SRVID_MASK;
		if (!t->tbl[idx].key) break;
		if (t->tbl[idx].key == key) { found = t->tbl[idx].name; break; }
	}
	pthread_mutex_unlock(&g_srvid_mtx);
	return found;
}

void srvid_lookup_copy(uint16_t caid, uint16_t sid, char *buf, size_t bufsz)
{
	buf[0] = '\0';
	if (!bufsz) return;
	pthread_mutex_lock(&g_srvid_mtx);
	S_SRVID_TABLE *t = g_srvid;
	if (!t || !sid || !caid) { pthread_mutex_unlock(&g_srvid_mtx); return; }

	uint32_t key = ((uint32_t)caid << 16) | sid;
	uint32_t h   = hash_key(key);

	for (int i = 0; i < SRVID_BUCKETS; i++)
	{
		uint32_t idx = (h + i) & SRVID_MASK;
		if (!t->tbl[idx].key) break;
		if (t->tbl[idx].key == key)
		{
			size_t n = strlen(t->tbl[idx].name);
			if (n >= bufsz) n = bufsz - 1;
			memcpy(buf, t->tbl[idx].name, n);
			buf[n] = '\0';
			break;
		}
	}
	if (!buf[0]) {  }
	pthread_mutex_unlock(&g_srvid_mtx);
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
	if (g_srvid) { free(g_srvid->tbl); free(g_srvid); g_srvid = NULL; }
	pthread_mutex_unlock(&g_srvid_mtx);
}
