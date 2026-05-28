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

/* ── Log file ── */
static FILE  *s_log_fp      = NULL;
static char   s_log_path[CFGPATH_LEN] = "";
#define LOG_FILE_MAX_BYTES  (10 * 1024 * 1024)   /* 10 MB rotation */

/* ── User statistics file (inspired by OSCam usrfile / cs_statistics) ── */
static FILE  *s_usr_fp      = NULL;
static char   s_usr_path[CFGPATH_LEN] = "";
#define USR_FILE_MAX_BYTES  (5 * 1024 * 1024)    /* 5 MB rotation  */

void log_set_file(const char *path)
{
	pthread_mutex_lock(&s_log_mtx);
	if (path && *path && strcmp(path, s_log_path) == 0)
	{
		pthread_mutex_unlock(&s_log_mtx);
		return;
	}
	if (s_log_fp) { fclose(s_log_fp); s_log_fp = NULL; }
	s_log_path[0] = '\0';
	if (path && *path)
	{
		s_log_fp = fopen(path, "a");
		if (s_log_fp)
			tcmg_strlcpy(s_log_path, path, sizeof(s_log_path));
	}
	pthread_mutex_unlock(&s_log_mtx);
}

/*
 * log_set_usrfile — open user statistics log (inspired by OSCam usrfile).
 * Each CW hit is written here in OSCam-compatible format.
 * Pass NULL or "" to disable.
 */
void log_set_usrfile(const char *path)
{
	pthread_mutex_lock(&s_log_mtx);
	if (path && *path && strcmp(path, s_usr_path) == 0)
	{
		pthread_mutex_unlock(&s_log_mtx);
		return;
	}
	if (s_usr_fp) { fclose(s_usr_fp); s_usr_fp = NULL; }
	s_usr_path[0] = '\0';
	if (path && *path)
	{
		s_usr_fp = fopen(path, "a");
		if (s_usr_fp)
			tcmg_strlcpy(s_usr_path, path, sizeof(s_usr_path));
	}
	pthread_mutex_unlock(&s_log_mtx);
}

/* ── Log file rotation (called inside s_log_mtx) ── */
static void maybe_rotate_log(void)
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

static void maybe_rotate_usr(void)
{
	if (!s_usr_fp || !s_usr_path[0]) return;
	long pos = ftell(s_usr_fp);
	if (pos < USR_FILE_MAX_BYTES) return;
	fclose(s_usr_fp);
	char rotated[CFGPATH_LEN + 2];
	snprintf(rotated, sizeof(rotated), "%s.1", s_usr_path);
	rename(s_usr_path, rotated);
	s_usr_fp = fopen(s_usr_path, "a");
}

/* ══════════════════════════════════════════════════════════════════════
 * Ring buffer — inspired by OSCam log_history with counter + usr field.
 * Each entry stores:  line text  +  user context (empty for system msgs)
 * ══════════════════════════════════════════════════════════════════════*/
typedef struct s_ring_entry
{
	char    *line;               /* full formatted log line  */
	char     usr[CFGKEY_LEN];   /* username, "" = system    */
} S_RING_ENTRY;

static S_RING_ENTRY s_ring[LOG_RING_MAX];
static int32_t      s_ring_head  = 0;   /* next write slot          */
static int32_t      s_ring_total = 0;   /* ever-increasing counter  */

/* Thread-local user context — set by net.c per-connection thread */
static __thread char t_log_user[CFGKEY_LEN] = "";

void log_set_user(const char *user)
{
	if (user && *user)
		tcmg_strlcpy(t_log_user, user, sizeof(t_log_user));
	else
		t_log_user[0] = '\0';
}

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

	/* main log file */
	if (s_log_fp)
	{
		maybe_rotate_log();
		if (s_log_fp)
		{
			fputs(line, s_log_fp);
			fputc('\n', s_log_fp);
			fflush(s_log_fp);
		}
	}

	/* Ring buffer — store line + user context (like OSCam log_history) */
	free(s_ring[s_ring_head].line);
	s_ring[s_ring_head].line = strdup(line);
	tcmg_strlcpy(s_ring[s_ring_head].usr, t_log_user,
	             sizeof(s_ring[s_ring_head].usr));
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

	if (n > 512) n = 512;
	char hex[512 * 3 + 1];
	for (i = 0; i < n; i++)
	{
		pos += snprintf(hex + pos, sizeof(hex) - pos,
		                "%02X%s", buf[i], i < n - 1 ? " " : "");
	}
	hex[pos] = '\0';

	char body[2048];
	snprintf(body, sizeof(body), "%s: %s", prefix, hex);
	emit(mod, body);
}

/*
 * log_ecm_raw -- compact hex dump of raw ECM payload (D_ECM only).
 */
void log_ecm_raw(const uint8_t *data, int32_t len)
{
	char line[512];
	int32_t i, pos;

	if (!(D_ECM & g_dblevel)) return;

	snprintf(line, sizeof(line), "ECM  length=%02X", len);
	emit("ecm", line);

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
 * log_cw_result -- log CW result + write to user statistics file.
 * The usrfile format is inspired by OSCam cs_statistics() so that
 * external log parsers (e.g. streamboard tools) can read both.
 *
 * usrfile line format (tab-separated):
 *   YYYY/MM/DD HH:MM:SS  user  ip  caid  sid  hit|miss  ms  channel
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

	/* ── Write to user statistics file (inspired by OSCam usrfile) ── */
	if (s_usr_fp && user && *user)
	{
		char ts[24];
		now_str(ts, sizeof(ts));
		pthread_mutex_lock(&s_log_mtx);
		maybe_rotate_usr();
		if (s_usr_fp)
		{
			fprintf(s_usr_fp, "%s\t%s\t%04X\t%04X\t%s\t%d\t%s\n",
			        ts,
			        user,
			        caid, sid,
			        hit ? "hit" : "miss",
			        ms,
			        ch ? ch : "");
			fflush(s_usr_fp);
		}
		pthread_mutex_unlock(&s_log_mtx);
	}
}

void   log_ecm_set(int8_t on) { s_ecm_log = on ? 1 : 0; }
int8_t log_ecm_get(void)      { return s_ecm_log; }


/*
 * log_ring_since — copy entries with serial id >= from_id.
 * Inspired by OSCam's log_history iterator with counter field.
 * Returns count; *out_next = id to pass on next call.
 *
 * out_lines: caller-allocated array of at least max_lines char* pointers.
 * out_users: caller-allocated array of at least max_lines char* pointers.
 * Caller must free() each returned string.
 */
int32_t log_ring_since(int32_t from_id, char **out_lines, char **out_users,
                        int32_t max_lines, int32_t *out_next)
{
	int32_t count = 0;

	pthread_mutex_lock(&s_log_mtx);

	int32_t oldest = s_ring_total - LOG_RING_MAX;
	if (oldest < 0) oldest = 0;
	if (from_id < oldest) from_id = oldest;

	int32_t i;
	for (i = from_id; i < s_ring_total && count < max_lines; i++)
	{
		int32_t slot = i % LOG_RING_MAX;
		const char *src  = s_ring[slot].line ? s_ring[slot].line : "";
		const char *usr  = s_ring[slot].usr;

		out_lines[count] = strdup(src);
		if (!out_lines[count]) break;

		if (out_users)
		{
			out_users[count] = strdup(usr[0] ? usr : "");
			if (!out_users[count])
			{
				free(out_lines[count]);
				break;
			}
		}
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
