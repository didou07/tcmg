#include "tcmg-srvid2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

/*
 * tcmg-srvid2.c  —  srvid2 format only
 *
 * File format (ncam.srvid2):
 *   SID:CAID1[,CAID2,...][|name|type||provider]
 *
 * Example:
 *   07D1:09B5,0603,0627|beIN SPORTS 1|TV||beIN SPORTS
 *
 * Hash key: (caid << 16) | sid
 * One entry per CAID/SID pair — all CAIDs map to the same name.
 */

#define SRVID_BUCKETS      16384
#define SRVID_MASK         (SRVID_BUCKETS - 1)
#define SRVID_NAME_MAX     80
#define MAX_CAIDS_PER_LINE 16

typedef struct {
	uint32_t key;                    /* (caid << 16) | sid, 0 = empty */
	char     name[SRVID_NAME_MAX];
} S_SRVID_ENTRY;

typedef struct {
	S_SRVID_ENTRY *tbl;
	int            count;
} S_SRVID_TABLE;

static S_SRVID_TABLE  *g_srvid     = NULL;
static pthread_mutex_t g_srvid_mtx = PTHREAD_MUTEX_INITIALIZER;

/* ── Hash ─────────────────────────────────────────────────── */
static inline uint32_t hash_key(uint32_t k)
{
	k ^= k >> 16;
	k *= 0x45d9f3bU;
	k ^= k >> 16;
	return k & SRVID_MASK;
}

/* ── Insert (no lock) ────────────────────────────────────── */
static void tbl_insert(S_SRVID_ENTRY *tbl, uint32_t key, const char *name)
{
	uint32_t h = hash_key(key);
	for (int i = 0; i < SRVID_BUCKETS; i++)
	{
		uint32_t idx = (h + i) & SRVID_MASK;
		if (!tbl[idx].key)
		{
			tbl[idx].key = key;
			strncpy(tbl[idx].name, name, SRVID_NAME_MAX - 1);
			tbl[idx].name[SRVID_NAME_MAX - 1] = '\0';
			return;
		}
		if (tbl[idx].key == key)
			return;  /* duplicate — keep first */
	}
}

/* ── Trim in-place ───────────────────────────────────────── */
static void trim(char *s)
{
	char *p = s;
	while (isspace((unsigned char)*p)) p++;
	if (p != s) memmove(s, p, strlen(p) + 1);
	char *q = s + strlen(s) - 1;
	while (q >= s && isspace((unsigned char)*q)) *q-- = '\0';
}

/* ── Load / Reload ───────────────────────────────────────── */
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

		/* ── Split key-part from payload at first '|' ── */
		char *pipe = strchr(line, '|');
		if (!pipe) continue;
		*pipe++ = '\0';
		/* payload: "name|type||provider" — name is first token */
		char *name_end = strchr(pipe, '|');
		if (name_end) *name_end = '\0';
		trim(pipe);
		if (!*pipe) continue;

		/* ── Split SID from CAID list at ':' ── */
		char *colon = strchr(line, ':');
		if (!colon) continue;
		*colon++ = '\0';

		trim(line);
		unsigned sid_u = 0;
		if (sscanf(line, "%X", &sid_u) != 1 || !sid_u) continue;
		uint16_t sid = (uint16_t)sid_u;

		/* ── Parse comma-separated CAID list ── */
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

	/* Atomic swap */
	pthread_mutex_lock(&g_srvid_mtx);
	S_SRVID_TABLE *old = g_srvid;
	g_srvid = newtbl;
	pthread_mutex_unlock(&g_srvid_mtx);

	if (old) { free(old->tbl); free(old); }
	return newtbl->count;
}

/* ── Lookup ──────────────────────────────────────────────── */
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

/* ── Free (call on shutdown) ─────────────────────────────── */
void srvid_free(void)
{
	pthread_mutex_lock(&g_srvid_mtx);
	if (g_srvid) { free(g_srvid->tbl); free(g_srvid); g_srvid = NULL; }
	pthread_mutex_unlock(&g_srvid_mtx);
}
