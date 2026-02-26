#define MODULE_LOG_PREFIX "log"
#include "tcmg-globals.h"

volatile uint16_t g_dblevel = 0;

const S_DBLEVEL_NAME g_dblevel_names[MAX_DEBUG_LEVELS] = {
	{ D_NET,     "net"     },
	{ D_CLIENT,  "client"  },
	{ D_ECM,     "ecm"     },
	{ D_PROTO,   "proto"   },
	{ D_CONF,    "conf"    },
	{ D_WEBIF,   "webif"   },
	{ D_BAN,     "ban"     },
};

static int8_t          s_ecm_log  = 1;
static pthread_mutex_t s_log_mtx  = PTHREAD_MUTEX_INITIALIZER;

/* Log file support */
static FILE  *s_log_fp      = NULL;
static char   s_log_path[CFGPATH_LEN] = "";
#define LOG_FILE_MAX_BYTES  (10 * 1024 * 1024)  /* 10 MB rotation threshold */

void log_set_file(const char *path)
{
	pthread_mutex_lock(&s_log_mtx);
	if (s_log_fp) { fclose(s_log_fp); s_log_fp = NULL; }
	s_log_path[0] = '\0';
	if (path && *path)
	{
		s_log_fp = fopen(path, "a");
		if (s_log_fp)
			strncpy(s_log_path, path, sizeof(s_log_path) - 1);
	}
	pthread_mutex_unlock(&s_log_mtx);
}

/* Check & rotate log file (called inside s_log_mtx) */
static void maybe_rotate(void)
{
	if (!s_log_fp || !s_log_path[0]) return;
	long pos = ftell(s_log_fp);
	if (pos < LOG_FILE_MAX_BYTES) return;
	fclose(s_log_fp);
	char rotated[CFGPATH_LEN + 2];
	snprintf(rotated, sizeof(rotated), "%s.1", s_log_path);
	rename(s_log_path, rotated);
	s_log_fp = fopen(s_log_path, "a");
}

/* Circular ring buffer */
static char    *s_ring[LOG_RING_MAX];
static int32_t  s_ring_head  = 0;   /* next write slot */
static int32_t  s_ring_total = 0;   /* ever-increasing serial counter */

static void now_str(char *buf, size_t sz)
{
	time_t    t = time(NULL);
	struct tm ti;
	localtime_r(&t, &ti);
	strftime(buf, sz, "%Y/%m/%d %H:%M:%S", &ti);
}

static void emit(const char *mod, const char *body)
{
	char ts[24];
	char line[1280];

	now_str(ts, sizeof(ts));
	if (mod)
	{
		char modbuf[16];
		snprintf(modbuf, sizeof(modbuf), "(%s)", mod);
		snprintf(line, sizeof(line), "%s %10s %s", ts, modbuf, body);
	}
	else
		snprintf(line, sizeof(line), "%s %s", ts, body);

	pthread_mutex_lock(&s_log_mtx);

	/* stdout */
	puts(line);
	fflush(stdout);

	/* log file (if configured) */
	if (s_log_fp)
	{
		maybe_rotate();
		if (s_log_fp)
		{
			fputs(line, s_log_fp);
			fputc('\n', s_log_fp);
			fflush(s_log_fp);
		}
	}

	/* overwrite oldest slot if ring is full */
	free(s_ring[s_ring_head]);
	s_ring[s_ring_head] = strdup(line);
	s_ring_head = (s_ring_head + 1) % LOG_RING_MAX;
	s_ring_total++;

	pthread_mutex_unlock(&s_log_mtx);
}

void tcmg_log_txt(const char *mod, const char *fmt, ...)
{
	char body[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(body, sizeof(body), fmt, ap);
	va_end(ap);
	emit(mod, body);
}

void tcmg_log_hex(const char *mod, const uint8_t *buf, int32_t n,
                  const char *fmt, ...)
{
	char prefix[256];
	va_list ap;
	int32_t pos = 0, i;

	va_start(ap, fmt);
	vsnprintf(prefix, sizeof(prefix), fmt, ap);
	va_end(ap);

	/* "PREFIX: XX XX XX ..." */
	int32_t hex_max = n * 3 + 4;
	char hex[hex_max];
	for (i = 0; i < n; i++)
	{
		if (pos + 3 >= hex_max) break;
		pos += snprintf(hex + pos, hex_max - pos,
		                "%02X%s", buf[i], i < n - 1 ? " " : "");
	}

	char body[1280];
	snprintf(body, sizeof(body), "%s: %s", prefix, hex);
	emit(mod, body);
}


/*
 * log_ecm_raw — compact hex dump of raw ECM payload (D_ECM only).
 */
void log_ecm_raw(const uint8_t *data, int32_t len)
{
	char line[512];
	int32_t i, pos;

	if (!(D_ECM & g_dblevel)) return;

	/* ── Header line ── */
	snprintf(line, sizeof(line), "ECM  length=%02X", len);
	emit("ecm", line);

	/* ── Continuation rows: 16 bytes each.
	 * emit() with module:    "YYYY/MM/DD HH:MM:SS %10s body"  (col 31)
	 * emit() without module: "YYYY/MM/DD HH:MM:SS body"       (col 20)
	 * Prepend 11 spaces to body so hex columns align with header. ── */
	for (i = 0; i < len; i += 16)
	{
		pos = snprintf(line, sizeof(line), "           "); /* 11 spaces */
		int j;
		for (j = 0; j < 16 && (i + j) < len; j++)
		{
			if (j > 0) line[pos++] = ' ';
			pos += snprintf(line + pos, sizeof(line) - pos,
			                "%02X", data[i + j]);
		}
		line[pos] = '\0';
		emit(NULL, line);
	}
}

/*
 * log_cw_result — always printed when ecm_log=1 (D_ECM not required).
 * Normal mode (0x0000):
 *   (cw      ) [hit]  0B00:0001  28FDEF307A1BFBBA 43F55D8627A87C5C  0ms  user
 * With D_ECM (0x0010): same format, payload already printed by log_ecm_raw.
 */
void log_cw_result(uint16_t caid, uint16_t sid, int32_t len,
                   const uint8_t *cw, bool hit, int32_t ms, const char *user)
{
	char line[128];
	int32_t i;

	if (!s_ecm_log) return;

	const char *ch = srvid_lookup(caid, sid);

	if (hit)
	{
		char cw_str[35];
		for (i = 0; i < 8; i++)
			snprintf(cw_str + i * 2,       3, "%02X", cw[i]);
		cw_str[16] = ' ';
		for (i = 0; i < 8; i++)
			snprintf(cw_str + 17 + i * 2, 3, "%02X", cw[8 + i]);
		cw_str[33] = '\0';
		if (ch)
			snprintf(line, sizeof(line), "[hit]  %04X:%04X  %s  %dms  %s  %s",
			         caid, sid, cw_str, ms, user ? user : "?", ch);
		else
			snprintf(line, sizeof(line), "[hit]  %04X:%04X:%02X  %s  %dms  %s",
			         caid, sid, (uint8_t)len, cw_str, ms, user ? user : "?");
	}
	else
	{
		if (ch)
			snprintf(line, sizeof(line), "[miss] %04X:%04X  %dms  %s  %s",
			         caid, sid, ms, user ? user : "?", ch);
		else
			snprintf(line, sizeof(line), "[miss] %04X:%04X:%02X  %dms  %s",
			         caid, sid, (uint8_t)len, ms, user ? user : "?");
	}
	emit("cw", line);
}

/* Legacy ECM-log switch (maps to D_ECM presence) */
void   log_ecm_set(int8_t on) { s_ecm_log = on ? 1 : 0; }
int8_t log_ecm_get(void)      { return s_ecm_log; }


/*
 * log_ring_since — copy lines with serial id in [from_id, ...).
 * Call with from_id=0 on first poll to get the last LOG_RING_MAX entries.
 * Returns count; *out_next = next id to pass next time.
 * Caller must free() each returned string.
 */
int32_t log_ring_since(int32_t from_id, char **out_lines,
                        int32_t max_lines, int32_t *out_next)
{
	int32_t count = 0;

	pthread_mutex_lock(&s_log_mtx);

	/* oldest serial currently in ring */
	int32_t oldest = s_ring_total - LOG_RING_MAX;
	if (oldest < 0) oldest = 0;
	if (from_id < oldest) from_id = oldest;

	int32_t i;
	for (i = from_id; i < s_ring_total && count < max_lines; i++)
	{
		/* slot for serial i */
		int32_t slot = i % LOG_RING_MAX;
		out_lines[count] = s_ring[slot] ? strdup(s_ring[slot]) : strdup("");
		count++;
	}
	*out_next = s_ring_total;

	pthread_mutex_unlock(&s_log_mtx);
	return count;
}

int32_t log_ring_total(void)
{
	int32_t t;
	pthread_mutex_lock(&s_log_mtx);
	t = s_ring_total;
	pthread_mutex_unlock(&s_log_mtx);
	return t;
}
