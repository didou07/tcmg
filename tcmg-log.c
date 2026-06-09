#define MODULE_LOG_PREFIX "log"
#include "globals.h"
#include <stdatomic.h>

volatile uint16_t g_dblevel = 0;

const S_DBLEVEL_NAME g_dblevel_names[MAX_DEBUG_LEVELS] = {
	{ D_WIRE,    "wire"    },
	{ D_ECM,     "ecm"     },
	{ D_EMU,     "emu"     },
	{ D_NEWCAMD, "newcamd" },
	{ D_CCCAM,   "cccam"   },
	{ D_HTTP,    "http"    },
	{ D_CONN,    "conn"    },
};

#define LOG_FILE_MAX_BYTES  (10u * 1024u * 1024u)
#define USR_FILE_MAX_BYTES  (5u  * 1024u * 1024u)
#define LOG_LINE_MAX        1280
#define LOG_BODY_MAX        1024
#define LOG_HEX_INPUT_CAP   512
#define LOG_PREFIX_MAX      12
#define WQ_SIZE             8192

typedef struct {
	char   line[LOG_LINE_MAX];
	char   usr[CFGKEY_LEN];
	char   usr_line[LOG_LINE_MAX];
	int8_t has_usr_line;
} S_WQ_ENTRY;

static S_WQ_ENTRY       s_wq[WQ_SIZE];
static _Atomic int32_t  s_wq_head    = 0;
static _Atomic int32_t  s_wq_tail    = 0;
static pthread_mutex_t  s_wq_mtx     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   s_wq_cond    = PTHREAD_COND_INITIALIZER;
static pthread_cond_t   s_wq_empty   = PTHREAD_COND_INITIALIZER;
static pthread_t        s_wq_tid;
static _Atomic int8_t   s_wq_running = 0;

typedef struct {
	char    line[LOG_LINE_MAX];
	char    usr[CFGKEY_LEN];
	int32_t id;
} S_RING_ENTRY;

static S_RING_ENTRY    s_ring[LOG_RING_MAX];
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
static int8_t s_pending_reopen          = 0;

static int8_t s_ecm_log = 1;

static __thread char    t_log_user[CFGKEY_LEN] = "";
static __thread char    t_log_type              = 's';
static __thread char    t_last_line[LOG_LINE_MAX] = "";
static __thread int32_t t_dup_count             = 0;

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
	fputs("-- log file rotated --\n", *fp);
	fflush(*fp);
	fclose(*fp);
	char prev[CFGPATH_LEN + 8];
	snprintf(prev, sizeof(prev), "%s.prev", path);
	rename(path, prev);
	*fp = fopen(path, "a");
	if (*fp) {
		char ts[24];
		ts_now(ts, sizeof(ts));
		fprintf(*fp, "-- log continued at %s --\n", ts);
	}
}

static void ring_push(const char *line, const char *usr)
{
	pthread_mutex_lock(&s_ring_mtx);
	S_RING_ENTRY *e = &s_ring[s_ring_head % LOG_RING_MAX];
	tcmg_strlcpy(e->line, line, sizeof(e->line));
	tcmg_strlcpy(e->usr,  usr ? usr : "", sizeof(e->usr));
	e->id = s_ring_total;
	s_ring_head = (s_ring_head + 1) % LOG_RING_MAX;
	s_ring_total++;
	pthread_mutex_unlock(&s_ring_mtx);
}

static void apply_pending_files(void)
{
	pthread_mutex_lock(&s_file_mtx);

	if (s_pending_reopen) {
		if (s_log_fp) {
			fputs("-- log file re-opened --\n", s_log_fp);
			fflush(s_log_fp);
			fclose(s_log_fp);
			s_log_fp = NULL;
		}
		if (s_log_path[0]) {
			s_log_fp = fopen(s_log_path, "a");
			if (s_log_fp) {
				char ts[24];
				ts_now(ts, sizeof(ts));
				fprintf(s_log_fp, "-- log re-opened at %s --\n", ts);
			}
		}
		s_pending_reopen = 0;
	}

	if (s_pending_log_set) {
		if (s_log_fp) { fclose(s_log_fp); s_log_fp = NULL; }
		s_log_path[0] = '\0';
		if (s_pending_log[0]) {
			s_log_fp = fopen(s_pending_log, "a");
			if (s_log_fp) {
				tcmg_strlcpy(s_log_path, s_pending_log, sizeof(s_log_path));
				char ts[24];
				ts_now(ts, sizeof(ts));
				fprintf(s_log_fp, "\n>> tcmg << log started at %s\n", ts);
				fflush(s_log_fp);
			}
		}
		s_pending_log_set = 0;
	}

	if (s_pending_usr_set) {
		if (s_usr_fp) { fclose(s_usr_fp); s_usr_fp = NULL; }
		s_usr_path[0] = '\0';
		if (s_pending_usr[0]) {
			s_usr_fp = fopen(s_pending_usr, "a");
			if (s_usr_fp)
				tcmg_strlcpy(s_usr_path, s_pending_usr, sizeof(s_usr_path));
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
		int32_t     slot = tail % WQ_SIZE;
		S_WQ_ENTRY *e    = &s_wq[slot];
		const char *line = e->line;

		fputs(line, stdout);
		fputc('\n', stdout);

		if (s_log_fp) {
			rotate_if_needed(&s_log_fp, s_log_path, LOG_FILE_MAX_BYTES);
			if (s_log_fp) {
				fputs(line, s_log_fp);
				fputc('\n', s_log_fp);
			}
		}

		ring_push(line, e->usr);

		if (e->has_usr_line && s_usr_fp) {
			rotate_if_needed(&s_usr_fp, s_usr_path, USR_FILE_MAX_BYTES);
			if (s_usr_fp) {
				fputs(e->usr_line, s_usr_fp);
				fputc('\n', s_usr_fp);
			}
		}

		e->line[0]      = '\0';
		e->usr[0]       = '\0';
		e->usr_line[0]  = '\0';
		e->has_usr_line = 0;

		tail++;
		atomic_store_explicit(&s_wq_tail, tail, memory_order_release);
		head = atomic_load_explicit(&s_wq_head, memory_order_acquire);
	}

	fflush(stdout);
	if (s_log_fp) fflush(s_log_fp);
	if (s_usr_fp) fflush(s_usr_fp);

	pthread_mutex_lock(&s_wq_mtx);
	pthread_cond_broadcast(&s_wq_empty);
	pthread_mutex_unlock(&s_wq_mtx);
}

static void *writer_thread(void *arg)
{
	(void)arg;
	while (atomic_load_explicit(&s_wq_running, memory_order_acquire)) {
		pthread_mutex_lock(&s_wq_mtx);
		while (
			atomic_load_explicit(&s_wq_tail,    memory_order_relaxed) ==
			atomic_load_explicit(&s_wq_head,    memory_order_relaxed) &&
			atomic_load_explicit(&s_wq_running, memory_order_relaxed))
		{
			pthread_cond_wait(&s_wq_cond, &s_wq_mtx);
		}
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
	atomic_store(&s_wq_running, 1);
	pthread_create(&s_wq_tid, NULL, writer_thread, NULL);
}

void log_flush(void)
{
	pthread_mutex_lock(&s_wq_mtx);
	while (atomic_load_explicit(&s_wq_head, memory_order_relaxed) !=
	       atomic_load_explicit(&s_wq_tail, memory_order_relaxed))
		pthread_cond_wait(&s_wq_empty, &s_wq_mtx);
	pthread_mutex_unlock(&s_wq_mtx);
}

void log_shutdown(void)
{
	tcmg_log("%s", ">> " TCMG_BANNER " << shutting down");
	log_flush();
	atomic_store_explicit(&s_wq_running, 0, memory_order_release);
	pthread_mutex_lock(&s_wq_mtx);
	pthread_cond_signal(&s_wq_cond);
	pthread_mutex_unlock(&s_wq_mtx);
	pthread_join(s_wq_tid, NULL);
	if (s_log_fp) { fclose(s_log_fp); s_log_fp = NULL; }
	if (s_usr_fp) { fclose(s_usr_fp); s_usr_fp = NULL; }
}

void log_reopen(void)
{
	pthread_mutex_lock(&s_file_mtx);
	s_pending_reopen = 1;
	pthread_mutex_unlock(&s_file_mtx);
	pthread_mutex_lock(&s_wq_mtx);
	pthread_cond_signal(&s_wq_cond);
	pthread_mutex_unlock(&s_wq_mtx);
}

static void wq_push(const char *line, const char *usr, const char *usr_line)
{
	pthread_mutex_lock(&s_wq_mtx);
	int32_t head = atomic_load_explicit(&s_wq_head, memory_order_relaxed);
	int32_t tail = atomic_load_explicit(&s_wq_tail, memory_order_acquire);

	if (((head + 1) % WQ_SIZE) == (tail % WQ_SIZE)) {
		pthread_mutex_unlock(&s_wq_mtx);
		fputs("[TCMG] log queue full -- line dropped\n", stderr);
		return;
	}

	int32_t     slot = head % WQ_SIZE;
	S_WQ_ENTRY *e    = &s_wq[slot];

	tcmg_strlcpy(e->line, line, sizeof(e->line));
	tcmg_strlcpy(e->usr,  usr ? usr : "", sizeof(e->usr));

	if (usr_line && usr_line[0]) {
		tcmg_strlcpy(e->usr_line, usr_line, sizeof(e->usr_line));
		e->has_usr_line = 1;
	} else {
		e->usr_line[0]  = '\0';
		e->has_usr_line = 0;
	}

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
		char modbuf[LOG_PREFIX_MAX + 2];
		snprintf(modbuf, sizeof(modbuf), "(%s)", mod);
		snprintf(line, sizeof(line), "%s %08X %c %10s %s",
		         ts, tid, t_log_type, modbuf, body);
	} else {
		snprintf(line, sizeof(line), "%s %08X %c            %s",
		         ts, tid, t_log_type, body);
	}

	if (strcmp(line, t_last_line) == 0) {
		t_dup_count++;
		return;
	}

	if (t_dup_count > 0) {
		char dup[LOG_LINE_MAX];
		snprintf(dup, sizeof(dup),
		         "%s %08X %c            -- last line repeated %d time(s) --",
		         ts, tid, t_log_type, t_dup_count);
		t_dup_count = 0;
		if (atomic_load_explicit(&s_wq_running, memory_order_acquire))
			wq_push(dup, t_log_user, NULL);
		else {
			fputs(dup, stdout); fputc('\n', stdout); fflush(stdout);
			ring_push(dup, t_log_user);
		}
	}

	tcmg_strlcpy(t_last_line, line, sizeof(t_last_line));

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
	if (user && *user)
		tcmg_strlcpy(t_log_user, user, sizeof(t_log_user));
	else
		t_log_user[0] = '\0';
}

void log_set_type(E_LOG_TYPE type)
{
	t_log_type = (char)type;
}

void tcmg_log_txt(const char *mod, const char *fmt, ...)
{
	char    body[LOG_BODY_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(body, sizeof(body), fmt, ap);
	va_end(ap);
	emit_line(mod, body);
}

static void hex_row(const uint8_t *buf, int32_t offset, int32_t count, char *row)
{
	int32_t rp = 0;
	int32_t j;

	for (j = 0; j < count; j++) {
	static const char hex_ch[] = "0123456789ABCDEF";
		uint8_t b = buf[offset + j];
		row[rp++] = hex_ch[b >> 4];
		row[rp++] = hex_ch[b & 0xF];
		row[rp++] = ' ';
	}
	while (j < 16) {
		row[rp++] = ' ';
		row[rp++] = ' ';
		row[rp++] = ' ';
		j++;
	}
	while (rp > 0 && row[rp - 1] == ' ') rp--;
	row[rp] = '\0';
}

void tcmg_log_hex(const char *mod, const uint8_t *buf, int32_t n,
                  const char *fmt, ...)
{
	char    prefix[256];
	va_list ap;
	char    firstline[320];
	int32_t i;

	va_start(ap, fmt);
	vsnprintf(prefix, sizeof(prefix), fmt, ap);
	va_end(ap);

	if (n > LOG_HEX_INPUT_CAP) n = LOG_HEX_INPUT_CAP;

	if (n == 0) {
		char body[300];
		int32_t rp = 0;
		const char *p = prefix;
		while (*p && rp < 295) body[rp++] = *p++;
		body[rp++] = ':'; body[rp++] = ' ';
		body[rp++] = '('; body[rp++] = 'e';
		body[rp++] = 'm'; body[rp++] = 'p';
		body[rp++] = 't'; body[rp++] = 'y';
		body[rp++] = ')'; body[rp] = '\0';
		emit_line(mod, body);
		return;
	}

	{
		int32_t rp = 0;
		const char *p = prefix;
		char  tmp[16];
		int32_t nlen = 0;
		int32_t v = n;
		while (*p && rp < 295) firstline[rp++] = *p++;
		firstline[rp++] = ' ';
		firstline[rp++] = '(';
		do {
			tmp[nlen++] = (char)('0' + v % 10);
			v /= 10;
		} while (v && nlen < 15);
		while (nlen > 0) firstline[rp++] = tmp[--nlen];
		firstline[rp++] = ' ';
		firstline[rp++] = 'b';
		firstline[rp++] = 'y';
		firstline[rp++] = 't';
		firstline[rp++] = 'e';
		firstline[rp++] = 's';
		firstline[rp++] = ')';
		firstline[rp++] = ':';
		firstline[rp] = '\0';
	}
	emit_line(mod, firstline);

	for (i = 0; i < n; i += 16) {
		int32_t count = (i + 16 < n) ? 16 : (n - i);
		char    row[56];
		hex_row(buf, i, count, row);
		emit_line(NULL, row);
	}
}

void log_ecm_raw(uint16_t caid, uint16_t sid, const uint8_t *data, int32_t len)
{
	int32_t cap;
	int32_t i;

	if (!(D_ECM & g_dblevel)) return;

	{
		char header[64];
		snprintf(header, sizeof(header), "caid=%04X sid=%04X len=%d", caid, sid, len);
		emit_line("ecm", header);
	}

	cap = (len > 256) ? 256 : len;
	for (i = 0; i < cap; i += 16) {
		int32_t count = (i + 16 < cap) ? 16 : (cap - i);
		char    row[64];
		hex_row(data, i, count, row);
		emit_line(NULL, row);
	}

	if (len > 256) {
		char trunc[40];
		snprintf(trunc, sizeof(trunc), "  ... (%d bytes truncated)", len - 256);
		emit_line(NULL, trunc);
	}
}

void log_cw_result(uint16_t caid, uint16_t sid, int32_t len,
                   const uint8_t *cw, bool hit, int32_t ms, const char *user)
{
	char body[512];
	char usr_line[LOG_LINE_MAX];
	char ts[24];
	char line[LOG_LINE_MAX];
	uint32_t tid;

	if (!s_ecm_log) return;

	{
		char ch_buf[SRVID_NAME_MAX];
		srvid_lookup_copy(caid, sid, ch_buf, sizeof(ch_buf));
		const char *ch = ch_buf[0] ? ch_buf : NULL;

		char cw_str[33] = "";
		if (hit && cw) {
			static const char hx[] = "0123456789ABCDEF";
			for (int32_t i = 0; i < 16; i++) {
				cw_str[i * 2]     = hx[cw[i] >> 4];
				cw_str[i * 2 + 1] = hx[cw[i] & 0xF];
			}
			cw_str[32] = '\0';
		}

		if (hit) {
			if (ch)
				snprintf(body, sizeof(body),
				         "(%04X:%04X:%02X:%s): found (%d ms) by %s  [%s]",
				         caid, sid, (int)len, cw_str, ms,
				         user ? user : "?", ch);
			else
				snprintf(body, sizeof(body),
				         "(%04X:%04X:%02X:%s): found (%d ms) by %s",
				         caid, sid, (int)len, cw_str, ms,
				         user ? user : "?");
		} else {
			snprintf(body, sizeof(body),
			         "(%04X:%04X:%02X): not found (%d ms)",
			         caid, sid, (int)len, ms);
		}

		usr_line[0] = '\0';
		if (user && *user) {
			ts_now(ts, sizeof(ts));
			snprintf(usr_line, sizeof(usr_line),
			         "%s\t%s\t%04X\t%04X\t%s\t%d\t%02X\t%s",
			         ts, user, caid, sid,
			         hit ? "hit" : "miss",
			         ms, (int)len,
			         ch ? ch : "");
		}
	}

	tid = (uint32_t)(uintptr_t)pthread_self();
	ts_now(ts, sizeof(ts));
	snprintf(line, sizeof(line), "%s %08X %c      (ecm) %s",
	         ts, tid, t_log_type, body);

	if (atomic_load_explicit(&s_wq_running, memory_order_acquire))
		wq_push(line, user ? user : "",
		        usr_line[0] ? usr_line : NULL);
	else {
		fputs(line, stdout); fputc('\n', stdout); fflush(stdout);
		ring_push(line, user ? user : "");
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
		int32_t     slot = i % LOG_RING_MAX;
		const char *src  = s_ring[slot].line;
		const char *usr  = s_ring[slot].usr;

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
