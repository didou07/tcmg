#define MODULE_LOG_PREFIX "webif"
#include "../../src/log/log.h"
#include "../internal/proto.h"

typedef struct {
	char **buf;
	int *bsz;
	int pos;
	int failed;
	int json;
} s_log_emit_ctx;

static int emit_livelog_line(int32_t id, const char *line, const char *usr, void *ctx)
{
	(void)usr;
	s_log_emit_ctx *c = (s_log_emit_ctx *)ctx;
	char esc[8192];
	html_escape(line, esc, (int)sizeof(esc));
	c->pos = buf_printf(c->buf, c->bsz, c->pos,
		"<span data-r=\"%s\">%s</span>", esc, esc);
	if (c->pos < 0) c->failed = 1;
	return c->failed ? -1 : 0;
}

static int emit_logpoll_line(int32_t id, const char *line, const char *usr, void *ctx)
{
	s_log_emit_ctx *c = (s_log_emit_ctx *)ctx;
	char esc_line[8192], esc_usr[512];
	json_escape(line, esc_line, sizeof(esc_line));
	json_escape(usr && usr[0] ? usr : "", esc_usr, sizeof(esc_usr));
	c->pos = buf_printf(c->buf, c->bsz, c->pos,
		"%s{\"id\":%d,\"usr\":\"%s\",\"line\":\"%s\"}",
		c->pos > 0 && (*c->buf)[c->pos - 1] != '[' ? "," : "", id, esc_usr, esc_line);
	if (c->pos < 0) c->failed = 1;
	return c->failed ? -1 : 0;
}

void send_page_livelog(int fd)
{
	PAGE_INIT(16384)
	pos = emit_header(&buf, &bsz, pos, "Live Log", "livelog");
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card' style='margin-bottom:12px'><div class='ll-ch'><span class='ct'>"
		"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'><path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/><polyline points='14 2 14 8 20 8'/></svg>Live Log"
		"</span><div class='ll-hdr-right'><div class='ll-dbrow'><span class='ll-label'>Debug</span>");
	for (int i = 0; i < MAX_DEBUG_LEVELS; i++) {
		uint16_t m = g_dblevel_names[i].mask;
		pos = buf_printf(&buf, &bsz, pos,
			"<a id='db%u' href='#' class='dt%s' onclick='toggleDbg(%u);return false;' title='0x%04X'>%s</a>",
			m, (g_dblevel & m) ? " on" : "", m, m, g_dblevel_names[i].name);
	}
	pos = buf_printf(&buf, &bsz, pos,
		"<a id='dbALL' href='#' class='dt%s' onclick='toggleAll();return false;'>ALL</a>"
		"<div class='ll-vsep'></div><div class='ll-meta'><span class='ll-dot'></span>mask:<span id='dbmask' class='ll-mask'>0x%04X</span>"
		"&nbsp;|&nbsp;<span id='linecnt' class='ll-mask'>0</span> lines&nbsp;|&nbsp;<span id='llstate' class='ll-mask'>Live</span>"
		"</div></div></div></div><div class='cb' style='padding:10px 18px'><div class='ll-toolbar2'>"
		"<input class='ls' id='filter' name='log_filter' type='search' autocomplete='off' autocorrect='off' autocapitalize='off' spellcheck='false' inputmode='search' placeholder='Filter&#8230;' oninput='applyFilter()' title='Regex, e.g.: hit|miss' aria-label='Log filter'>"
		"<div class='ll-sep'></div><button class='btn bg sm' onclick='clearLog()'>" ICO_TRASH "&nbsp;Clear</button>"
		"<button class='btn bg sm' onclick='prepSave()'>&#128190;&nbsp;Save</button><div class='ll-sep'></div>"
		"<label class='ll-chk'><input type='checkbox' id='asc' checked>&nbsp;Scroll</label><label class='ll-chk'><input type='checkbox' id='paused'>&nbsp;Pause</label>"
		"<div class='ll-sep'></div><span class='ll-label'>Lines: 200 max</span></div></div></div>",
		(g_dblevel == D_ALL) ? " on" : "", (unsigned)g_dblevel);
	int32_t total = log_ring_total();
	int32_t from_id = total > WEB_MAX_LINES_POLL ? total - WEB_MAX_LINES_POLL : 0;
	pos = buf_printf(&buf, &bsz, pos, "<div id='lw' data-next='%d' data-mask='%u'><pre id='lp'>", total, (unsigned)g_dblevel);
	s_log_emit_ctx c = { &buf, &bsz, pos, 0, 0 };
	int32_t next_id = total;
	log_ring_foreach(from_id, WEB_MAX_LINES_POLL, emit_livelog_line, &c, &next_id);
	pos = c.pos;
	if (c.failed) { free(buf); send_json_error(fd, 503, "Service Unavailable", "out of memory"); return; }
	pos = buf_printf(&buf, &bsz, pos, "</pre></div><script src='/assets/livelog.js?v=%s-20260926' defer></script>", TCMG_VERSION);
	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}

void send_logpoll(int fd, const char *qs)
{
	char dbg_s[16] = "", since_s[32] = "";
	get_param(qs, "debug", dbg_s, sizeof(dbg_s));
	get_param(qs, "since", since_s, sizeof(since_s));
	if (dbg_s[0]) {
		char *end = NULL;
		errno = 0;
		long v = strtol(dbg_s, &end, 0);
		if (errno == 0 && *end == '\0' && v >= 0 && v <= 0xFFFF) g_dblevel = (uint16_t)v;
	}
	int32_t from_id = 0;
	if (since_s[0]) {
		char *end = NULL;
		errno = 0;
		long v = strtol(since_s, &end, 10);
		if (errno == 0 && end && *end == '\0' && v > 0 && v <= INT32_MAX) from_id = (int32_t)v;
	}
	int bsz = 16384, pos = 0;
	char *buf = (char *)malloc((size_t)bsz);
	if (!buf) { send_json_error(fd, 503, "Service Unavailable", "out of memory"); return; }
	pos = buf_printf(&buf, &bsz, pos, "{\"debug\":%u,\"next\":0,\"lines\":[", (unsigned)g_dblevel);
	s_log_emit_ctx c = { &buf, &bsz, pos, 0, 1 };
	int32_t next_id = from_id;
	log_ring_foreach(from_id, WEB_MAX_LINES_POLL, emit_logpoll_line, &c, &next_id);
	pos = c.pos;
	if (c.failed) { free(buf); send_json_error(fd, 503, "Service Unavailable", "out of memory"); return; }
	pos = buf_printf(&buf, &bsz, pos, "],\"next\":%d}", next_id);
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}
