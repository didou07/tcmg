#define MODULE_LOG_PREFIX "log"
#include "tcmg-globals.h"

volatile uint16_t g_dblevel = 0;

const S_DBLEVEL_NAME g_dblevel_names[MAX_DEBUG_LEVELS] = {
	{ D_NET,    "net"    },
	{ D_CLIENT, "client" },
	{ D_ECM,    "ecm"    },
	{ D_PROTO,  "proto"  },
	{ D_CONF,   "conf"   },
	{ D_WEBIF,  "webif"  },
	{ D_BAN,    "ban"    },
};

#define LOG_FILE_MAX_BYTES  (10u * 1024u * 1024u)
#define USR_FILE_MAX_BYTES  (5u  * 1024u * 1024u)
#define LOG_LINE_MAX        1280
#define LOG_BODY_MAX        1024
#define LOG_HEX_INPUT_CAP   512

static pthread_mutex_t s_log_mtx = PTHREAD_MUTEX_INITIALIZER;

static FILE  *s_log_fp              = NULL;
static char   s_log_path[CFGPATH_LEN] = "";
static FILE  *s_usr_fp              = NULL;
static char   s_usr_path[CFGPATH_LEN] = "";

static int8_t s_ecm_log = 1;

typedef struct {
	char    *line;
	char     usr[CFGKEY_LEN];
} S_RING_ENTRY;

static S_RING_ENTRY s_ring[LOG_RING_MAX];
static int32_t      s_ring_head  = 0;
static int32_t      s_ring_total = 0;

static __thread char t_log_user[CFGKEY_LEN] = "";

/* ── internal helpers ── */

static void ts_now(char *buf, size_t sz)
{
	time_t    t = time(NULL);
	struct tm tm_buf;
	localtime_r(&t, &tm_buf);
	strftime(buf, sz, "%Y/%m/%d %H:%M:%S", &tm_buf);
}

static void rotate_if_needed(FILE **fp, const char *path, unsigned long max_bytes)
{
	if (!*fp || !path[0]) return;
	long pos = ftell(*fp);
	if (pos < 0 || (unsigned long)pos < max_bytes) return;
	fclose(*fp);
	char rotated[CFGPATH_LEN + 2];
	snprintf(rotated, sizeof(rotated), "%s.1", path);
	rename(path, rotated);
	*fp = fopen(path, "a");
}

static void ring_push(const char *line)
{
	S_RING_ENTRY *e = &s_ring[s_ring_head];
	free(e->line);
	e->line = strdup(line);
	if (!e->line) e->line = strdup("");
	tcmg_strlcpy(e->usr, t_log_user, sizeof(e->usr));
	s_ring_head = (s_ring_head + 1) % LOG_RING_MAX;
	s_ring_total++;
}

static void emit_line(const char *mod, const char *body)
{
	char ts[24];
	char line[LOG_LINE_MAX];

	ts_now(ts, sizeof(ts));
	if (mod) {
		char modbuf[18];
		snprintf(modbuf, sizeof(modbuf), "(%s)", mod);
		snprintf(line, sizeof(line), "%s %10s %s", ts, modbuf, body);
	} else {
		snprintf(line, sizeof(line), "%s %s", ts, body);
	}

	pthread_mutex_lock(&s_log_mtx);

	fputs(line, stdout);
	fputc('\n', stdout);
	fflush(stdout);

	if (s_log_fp) {
		rotate_if_needed(&s_log_fp, s_log_path, LOG_FILE_MAX_BYTES);
		if (s_log_fp) {
			fputs(line, s_log_fp);
			fputc('\n', s_log_fp);
			fflush(s_log_fp);
		}
	}

	ring_push(line);

	pthread_mutex_unlock(&s_log_mtx);
}

/* ── public file management ── */

void log_set_file(const char *path)
{
	pthread_mutex_lock(&s_log_mtx);
	if (path && *path && strcmp(path, s_log_path) == 0) {
		pthread_mutex_unlock(&s_log_mtx);
		return;
	}
	if (s_log_fp) { fclose(s_log_fp); s_log_fp = NULL; }
	s_log_path[0] = '\0';
	if (path && *path) {
		s_log_fp = fopen(path, "a");
		if (s_log_fp)
			tcmg_strlcpy(s_log_path, path, sizeof(s_log_path));
	}
	pthread_mutex_unlock(&s_log_mtx);
}

void log_set_usrfile(const char *path)
{
	pthread_mutex_lock(&s_log_mtx);
	if (path && *path && strcmp(path, s_usr_path) == 0) {
		pthread_mutex_unlock(&s_log_mtx);
		return;
	}
	if (s_usr_fp) { fclose(s_usr_fp); s_usr_fp = NULL; }
	s_usr_path[0] = '\0';
	if (path && *path) {
		s_usr_fp = fopen(path, "a");
		if (s_usr_fp)
			tcmg_strlcpy(s_usr_path, path, sizeof(s_usr_path));
	}
	pthread_mutex_unlock(&s_log_mtx);
}

/* ── per-thread user context ── */

void log_set_user(const char *user)
{
	if (user && *user)
		tcmg_strlcpy(t_log_user, user, sizeof(t_log_user));
	else
		t_log_user[0] = '\0';
}

/* ── core log functions ── */

void tcmg_log_txt(const char *mod, const char *fmt, ...)
{
	char body[LOG_BODY_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(body, sizeof(body), fmt, ap);
	va_end(ap);
	emit_line(mod, body);
}

void tcmg_log_hex(const char *mod, const uint8_t *buf, int32_t n,
                  const char *fmt, ...)
{
	char prefix[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(prefix, sizeof(prefix), fmt, ap);
	va_end(ap);

	if (n > LOG_HEX_INPUT_CAP) n = LOG_HEX_INPUT_CAP;

	char hex[LOG_HEX_INPUT_CAP * 3 + 1];
	int32_t pos = 0;
	for (int32_t i = 0; i < n; i++)
		pos += snprintf(hex + pos, sizeof(hex) - (size_t)pos,
		                "%02X%s", buf[i], i < n - 1 ? " " : "");
	hex[pos] = '\0';

	char body[1280];
	snprintf(body, sizeof(body), "%s: %s", prefix, hex);
	emit_line(mod, body);
}

/* ── ECM / CW logging ── */

void log_ecm_raw(const uint8_t *data, int32_t len)
{
	if (!(D_ECM & g_dblevel)) return;

	char line[128];
	snprintf(line, sizeof(line), "ECM  length=%02X", len);
	emit_line("ecm", line);

	for (int32_t i = 0; i < len; i += 16) {
		char row[64];
		int32_t rp = 0;
		for (int32_t j = 0; j < 16 && (i + j) < len; j++) {
			if (j) row[rp++] = ' ';
			rp += snprintf(row + rp, sizeof(row) - (size_t)rp,
			               "%02X", data[i + j]);
		}
		row[rp] = '\0';
		emit_line(NULL, row);
	}
}

void log_cw_result(uint16_t caid, uint16_t sid, int32_t len,
                   const uint8_t *cw, bool hit, int32_t ms, const char *user)
{
	if (!s_ecm_log) return;

	const char *ch = srvid_lookup(caid, sid);
	char line[192];

	if (hit) {
		char cw_str[34];
		for (int32_t i = 0; i < 8; i++)
			snprintf(cw_str + i * 2,       3, "%02X", cw[i]);
		cw_str[16] = ' ';
		for (int32_t i = 0; i < 8; i++)
			snprintf(cw_str + 17 + i * 2,  3, "%02X", cw[8 + i]);
		cw_str[33] = '\0';

		if (ch)
			snprintf(line, sizeof(line), "[hit]  %04X:%04X  %s  %dms  %s  %s",
			         caid, sid, cw_str, ms, user ? user : "?", ch);
		else
			snprintf(line, sizeof(line), "[hit]  %04X:%04X:%02X  %s  %dms  %s",
			         caid, sid, (uint8_t)len, cw_str, ms, user ? user : "?");
	} else {
		if (ch)
			snprintf(line, sizeof(line), "[miss] %04X:%04X  %dms  %s  %s",
			         caid, sid, ms, user ? user : "?", ch);
		else
			snprintf(line, sizeof(line), "[miss] %04X:%04X:%02X  %dms  %s",
			         caid, sid, (uint8_t)len, ms, user ? user : "?");
	}
	emit_line("cw", line);

	if (s_usr_fp && user && *user) {
		char ts[24];
		ts_now(ts, sizeof(ts));
		pthread_mutex_lock(&s_log_mtx);
		rotate_if_needed(&s_usr_fp, s_usr_path, USR_FILE_MAX_BYTES);
		if (s_usr_fp) {
			fprintf(s_usr_fp, "%s\t%s\t%04X\t%04X\t%s\t%d\t%s\n",
			        ts, user, caid, sid,
			        hit ? "hit" : "miss", ms,
			        ch ? ch : "");
			fflush(s_usr_fp);
		}
		pthread_mutex_unlock(&s_log_mtx);
	}
}

void   log_ecm_set(int8_t on) { s_ecm_log = on ? 1 : 0; }
int8_t log_ecm_get(void)      { return s_ecm_log; }

/* ── ring buffer API ── */

int32_t log_ring_since(int32_t from_id, char **out_lines, char **out_users,
                        int32_t max_lines, int32_t *out_next)
{
	int32_t count = 0;

	pthread_mutex_lock(&s_log_mtx);

	int32_t oldest = s_ring_total - LOG_RING_MAX;
	if (oldest < 0) oldest = 0;
	if (from_id < oldest) from_id = oldest;

	for (int32_t i = from_id; i < s_ring_total && count < max_lines; i++) {
		int32_t    slot = i % LOG_RING_MAX;
		const char *src = s_ring[slot].line ? s_ring[slot].line : "";
		const char *usr = s_ring[slot].usr;

		out_lines[count] = strdup(src);
		if (!out_lines[count]) break;

		if (out_users) {
			out_users[count] = strdup(usr[0] ? usr : "");
			if (!out_users[count]) { free(out_lines[count]); break; }
		}
		count++;
	}
	*out_next = s_ring_total;

	pthread_mutex_unlock(&s_log_mtx);
	return count;
}

int32_t log_ring_total(void)
{
	pthread_mutex_lock(&s_log_mtx);
	int32_t t = s_ring_total;
	pthread_mutex_unlock(&s_log_mtx);
	return t;
}
