#define MODULE_LOG_PREFIX "log"
#include "tcmg.h"
#include <stdatomic.h>

volatile uint16_t g_dblevel = 0;

const S_DBLEVEL_NAME g_dblevel_names[MAX_DEBUG_LEVELS] = {
	{ D_NET,    "net"    },
	{ D_CLIENT, "client" },
	{ D_ECM,    "ecm"    },
	{ D_PROTO,  "proto"  },
	{ D_CONF,   "conf"   },
	{ D_WEBIF,  "webif"  },
	{ D_BAN,    "ban"    },
	{ D_RELOAD, "reload" },
	{ D_SRVID,  "srvid"  },
};

#define LOG_FILE_MAX_BYTES  (10u * 1024u * 1024u)
#define USR_FILE_MAX_BYTES  (5u  * 1024u * 1024u)
#define LOG_LINE_MAX        1280
#define LOG_BODY_MAX        1024
#define LOG_HEX_INPUT_CAP   512
#define WQ_SIZE             4096

typedef struct {
	char  *line;
	char   usr[CFGKEY_LEN];
	char  *usr_line;
} S_WQ_ENTRY;

static S_WQ_ENTRY         s_wq[WQ_SIZE];
static char              *s_wq_pool   = NULL;
static _Atomic  int32_t   s_wq_head   = 0;
static _Atomic  int32_t   s_wq_tail   = 0;
static pthread_mutex_t    s_wq_mtx    = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t     s_wq_cond   = PTHREAD_COND_INITIALIZER;
static pthread_t          s_wq_tid;
static _Atomic  int8_t    s_wq_running = 0;

typedef struct { char *line; char usr[CFGKEY_LEN]; } S_RING_ENTRY;
static S_RING_ENTRY    s_ring[LOG_RING_MAX];
static char           *s_ring_pool  = NULL;
static int32_t         s_ring_head  = 0;
static int32_t         s_ring_total = 0;
static pthread_mutex_t s_ring_mtx   = PTHREAD_MUTEX_INITIALIZER;

static FILE *s_log_fp  = NULL;
static char  s_log_path[CFGPATH_LEN] = "";
static FILE *s_usr_fp  = NULL;
static char  s_usr_path[CFGPATH_LEN] = "";

static pthread_mutex_t s_file_mtx       = PTHREAD_MUTEX_INITIALIZER;
static char  s_pending_log[CFGPATH_LEN] = "";
static char  s_pending_usr[CFGPATH_LEN] = "";
static int8_t s_pending_log_set         = 0;
static int8_t s_pending_usr_set         = 0;

static int8_t s_ecm_log = 1;
static __thread char  t_log_user[CFGKEY_LEN] = "";
static __thread char  t_log_type = 's';

static void ts_now(char *buf, size_t sz)
{
	static __thread time_t s_last_t   = 0;
	static __thread char   s_last[24] = "";
	time_t t = time(NULL);
	if (t != s_last_t) {
		struct tm tm_buf;
		localtime_r(&t, &tm_buf);
		strftime(s_last, sizeof(s_last), "%Y/%m/%d %H:%M:%S", &tm_buf);
		s_last_t = t;
	}
	size_t n = sz < sizeof(s_last) ? sz : sizeof(s_last);
	memcpy(buf, s_last, n);
	if (n > 0) buf[n - 1] = '\0';
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

static void ring_push(const char *line, const char *usr)
{
	pthread_mutex_lock(&s_ring_mtx);
	S_RING_ENTRY *e = &s_ring[s_ring_head];
	tcmg_strlcpy(e->line, line, LOG_LINE_MAX);
	tcmg_strlcpy(e->usr,  usr ? usr : "", sizeof(e->usr));
	s_ring_head = (s_ring_head + 1) % LOG_RING_MAX;
	s_ring_total++;
	pthread_mutex_unlock(&s_ring_mtx);
}

static void apply_pending_files(void)
{
	pthread_mutex_lock(&s_file_mtx);
	if (s_pending_log_set) {
		if (s_log_fp) { fclose(s_log_fp); s_log_fp = NULL; }
		s_log_path[0] = '\0';
		if (s_pending_log[0]) {
			s_log_fp = fopen(s_pending_log, "a");
			if (s_log_fp) tcmg_strlcpy(s_log_path, s_pending_log, sizeof(s_log_path));
		}
		s_pending_log_set = 0;
	}
	if (s_pending_usr_set) {
		if (s_usr_fp) { fclose(s_usr_fp); s_usr_fp = NULL; }
		s_usr_path[0] = '\0';
		if (s_pending_usr[0]) {
			s_usr_fp = fopen(s_pending_usr, "a");
			if (s_usr_fp) tcmg_strlcpy(s_usr_path, s_pending_usr, sizeof(s_usr_path));
		}
		s_pending_usr_set = 0;
	}
	pthread_mutex_unlock(&s_file_mtx);
}

static void writer_drain(void)
{
	int32_t tail = atomic_load_explicit(&s_wq_tail, memory_order_acquire);
	int32_t head = atomic_load_explicit(&s_wq_head, memory_order_acquire);

	while (tail != head) {
		int32_t    slot = tail % WQ_SIZE;
		S_WQ_ENTRY *e   = &s_wq[slot];
		const char *line = e->line;

		fputs(line, stdout);
		fputc('\n', stdout);

		if (s_log_fp) {
			rotate_if_needed(&s_log_fp, s_log_path, LOG_FILE_MAX_BYTES);
			if (s_log_fp) { fputs(line, s_log_fp); fputc('\n', s_log_fp); }
		}

		ring_push(line, e->usr);

		if (e->usr_line && s_usr_fp) {
			rotate_if_needed(&s_usr_fp, s_usr_path, USR_FILE_MAX_BYTES);
			if (s_usr_fp) { fputs(e->usr_line, s_usr_fp); fputc('\n', s_usr_fp); }
		}

		free(e->usr_line);
		e->usr_line = NULL;
		e->line[0]  = '\0';
		e->usr[0]   = '\0';

		tail++;
		atomic_store_explicit(&s_wq_tail, tail, memory_order_release);
		head = atomic_load_explicit(&s_wq_head, memory_order_acquire);
	}
	fflush(stdout);
	if (s_log_fp) fflush(s_log_fp);
	if (s_usr_fp) fflush(s_usr_fp);
}

static void *writer_thread(void *arg)
{
	(void)arg;
	while (atomic_load_explicit(&s_wq_running, memory_order_acquire)) {
		pthread_mutex_lock(&s_wq_mtx);
		while (atomic_load_explicit(&s_wq_tail, memory_order_relaxed) ==
		       atomic_load_explicit(&s_wq_head, memory_order_relaxed) &&
		       atomic_load_explicit(&s_wq_running, memory_order_relaxed))
			pthread_cond_wait(&s_wq_cond, &s_wq_mtx);
		pthread_mutex_unlock(&s_wq_mtx);
		apply_pending_files();
		writer_drain();
	}
	apply_pending_files();
	writer_drain();
	return NULL;
}

void log_init(void)
{
	s_wq_pool = (char *)malloc((size_t)WQ_SIZE * LOG_LINE_MAX);
	if (s_wq_pool) {
		for (int i = 0; i < WQ_SIZE; i++)
			s_wq[i].line = s_wq_pool + (size_t)i * LOG_LINE_MAX;
	}

	s_ring_pool = (char *)malloc((size_t)LOG_RING_MAX * LOG_LINE_MAX);
	if (s_ring_pool) {
		for (int i = 0; i < LOG_RING_MAX; i++)
			s_ring[i].line = s_ring_pool + (size_t)i * LOG_LINE_MAX;
	}

	atomic_store(&s_wq_running, 1);
	pthread_create(&s_wq_tid, NULL, writer_thread, NULL);
}

void log_flush(void)
{
	for (int i = 0; i < 200; i++) {
		int32_t head = atomic_load_explicit(&s_wq_head, memory_order_acquire);
		int32_t tail = atomic_load_explicit(&s_wq_tail, memory_order_acquire);
		if (tail == head) break;
		tcmg_sleep_ms(1);
	}
}

void log_shutdown(void)
{
	atomic_store_explicit(&s_wq_running, 0, memory_order_release);
	pthread_mutex_lock(&s_wq_mtx);
	pthread_cond_signal(&s_wq_cond);
	pthread_mutex_unlock(&s_wq_mtx);
	pthread_join(s_wq_tid, NULL);
	free(s_wq_pool);   s_wq_pool   = NULL;
	free(s_ring_pool); s_ring_pool = NULL;
}

static void wq_push(const char *line, const char *usr, char *usr_line)
{
	pthread_mutex_lock(&s_wq_mtx);
	int32_t head = atomic_load_explicit(&s_wq_head, memory_order_relaxed);
	int32_t tail = atomic_load_explicit(&s_wq_tail, memory_order_acquire);
	if (((head + 1) % WQ_SIZE) == (tail % WQ_SIZE)) {
		pthread_mutex_unlock(&s_wq_mtx);
		free(usr_line);
		return;
	}
	int32_t    slot = head % WQ_SIZE;
	S_WQ_ENTRY *e   = &s_wq[slot];
	free(e->usr_line);
	if (e->line) tcmg_strlcpy(e->line, line, LOG_LINE_MAX);
	tcmg_strlcpy(e->usr, usr ? usr : "", sizeof(e->usr));
	e->usr_line = usr_line;
	atomic_store_explicit(&s_wq_head, head + 1, memory_order_release);
	pthread_cond_signal(&s_wq_cond);
	pthread_mutex_unlock(&s_wq_mtx);
}

static void emit_line(const char *mod, const char *body)
{
	char     ts[24];
	char     line[LOG_LINE_MAX];
	uint32_t tid = (uint32_t)(uintptr_t)pthread_self();

	ts_now(ts, sizeof(ts));
	if (mod) {
		char modbuf[12];
		snprintf(modbuf, sizeof(modbuf), "(%s)", mod);
		snprintf(line, sizeof(line), "%s %08X %c %9s %s",
		         ts, tid, t_log_type, modbuf, body);
	} else {
		snprintf(line, sizeof(line), "%s %08X %c           %s",
		         ts, tid, t_log_type, body);
	}

	if (atomic_load_explicit(&s_wq_running, memory_order_acquire))
		wq_push(line, t_log_user, NULL);
	else {
		fputs(line, stdout); fputc('\n', stdout); fflush(stdout);
		ring_push(line, t_log_user);
	}
}

void log_set_file(const char *path)
{
	pthread_mutex_lock(&s_file_mtx);
	tcmg_strlcpy(s_pending_log, path ? path : "", sizeof(s_pending_log));
	s_pending_log_set = 1;
	pthread_mutex_unlock(&s_file_mtx);
	pthread_mutex_lock(&s_wq_mtx);
	pthread_cond_signal(&s_wq_cond);
	pthread_mutex_unlock(&s_wq_mtx);
}

void log_set_usrfile(const char *path)
{
	pthread_mutex_lock(&s_file_mtx);
	tcmg_strlcpy(s_pending_usr, path ? path : "", sizeof(s_pending_usr));
	s_pending_usr_set = 1;
	pthread_mutex_unlock(&s_file_mtx);
	pthread_mutex_lock(&s_wq_mtx);
	pthread_cond_signal(&s_wq_cond);
	pthread_mutex_unlock(&s_wq_mtx);
}

void log_set_user(const char *user)
{
	if (user && *user) tcmg_strlcpy(t_log_user, user, sizeof(t_log_user));
	else               t_log_user[0] = '\0';
}

void log_set_type(E_LOG_TYPE type)
{
	t_log_type = (char)type;
}

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
		pos += snprintf(hex + pos, (size_t)(sizeof(hex) - (size_t)pos),
		                "%02X%s", buf[i], i < n - 1 ? " " : "");
	hex[pos] = '\0';
	char body[sizeof(hex) + 260];
	snprintf(body, sizeof(body), "%s: %s", prefix, hex);
	emit_line(mod, body);
}

void log_ecm_raw(uint16_t caid, uint16_t sid, const uint8_t *data, int32_t len)
{
	if (!(D_ECM & g_dblevel)) return;

	char first[64];
	snprintf(first, sizeof(first), "CAID=%04X SID=%04X LEN=%02X", caid, sid, len);
	emit_line("ecm", first);

	int32_t cap = len > 128 ? 128 : len;
	for (int32_t i = 0; i < cap; i += 16) {
		char row[16 * 3 + 1];
		int32_t rp = 0;
		int32_t end = (i + 16 < cap) ? i + 16 : cap;
		for (int32_t j = i; j < end; j++)
			rp += snprintf(row + rp, sizeof(row) - (size_t)rp,
			               "%02X%s", data[j], j < end - 1 ? " " : "");
		emit_line(NULL, row);
	}
}

void log_cw_result(uint16_t caid, uint16_t sid, int32_t len,
                   const uint8_t *cw, bool hit, int32_t ms, const char *user)
{
	if (!s_ecm_log) return;

	char ch_buf[SRVID_NAME_MAX];
	srvid_lookup_copy(caid, sid, ch_buf, sizeof(ch_buf));
	const char *ch = ch_buf[0] ? ch_buf : NULL;

	char line[320];
	if (hit) {
		char cw_str[33];
		for (int32_t i = 0; i < 16; i++)
			snprintf(cw_str + i * 2, 3, "%02X", cw[i]);
		cw_str[32] = '\0';

		if (ch)
			snprintf(line, sizeof(line),
			         "(%04X:%04X:%02X:%s): found (%d ms) by %s - %s",
			         caid, sid, (int)len, cw_str, ms, user ? user : "?", ch);
		else
			snprintf(line, sizeof(line),
			         "(%04X:%04X:%02X:%s): found (%d ms) by %s",
			         caid, sid, (int)len, cw_str, ms, user ? user : "?");
	} else {
		snprintf(line, sizeof(line),
		         "(%04X:%04X:%02X): not found (%d ms)",
		         caid, sid, (int)len, ms);
	}

	char *usr_line = NULL;
	if (user && *user) {
		char ts[24]; ts_now(ts, sizeof(ts));
		char tmp[320];
		snprintf(tmp, sizeof(tmp), "%s\t%s\t%04X\t%04X\t%s\t%d\t%02X\t%s",
		         ts, user, caid, sid, hit ? "hit" : "miss", ms, (int)len, ch ? ch : "");
		usr_line = strdup(tmp);
	}

	char ts[24]; ts_now(ts, sizeof(ts));
	uint32_t tid = (uint32_t)(uintptr_t)pthread_self();
	char full[LOG_LINE_MAX];
	snprintf(full, sizeof(full), "%s %08X %c     (ecm) %s", ts, tid, t_log_type, line);

	if (atomic_load_explicit(&s_wq_running, memory_order_acquire))
		wq_push(full, user ? user : "", usr_line);
	else {
		fputs(full, stdout); fputc('\n', stdout); fflush(stdout);
		ring_push(full, user ? user : "");
		free(usr_line);
	}
}

void   log_ecm_set(int8_t on) { s_ecm_log = on ? 1 : 0; }
int8_t log_ecm_get(void)      { return s_ecm_log; }

int32_t log_ring_since(int32_t from_id, char **out_lines, char **out_users,
                        int32_t max_lines, int32_t *out_next)
{
	int32_t count = 0;
	pthread_mutex_lock(&s_ring_mtx);
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
	pthread_mutex_unlock(&s_ring_mtx);
	return count;
}

int32_t log_ring_total(void)
{
	pthread_mutex_lock(&s_ring_mtx);
	int32_t t = s_ring_total;
	pthread_mutex_unlock(&s_ring_mtx);
	return t;
}
