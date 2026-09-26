#define MODULE_LOG_PREFIX "webif"
#include "../../src/log/log.h"
#include "../internal/proto.h"

void send_page_livelog(int fd)
{
    PAGE_INIT(32768)

    pos = emit_header(&buf, &bsz, pos, "Live Log", "livelog");

    pos = buf_printf(&buf, &bsz, pos,
        "<div class='card' style='margin-bottom:12px'>"
        "<div class='ll-ch'>"
        "  <span class='ct'>"
        "    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "      <path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
        "      <polyline points='14 2 14 8 20 8'/>"
        "    </svg>Live Log"
        "  </span>"
        "  <div class='ll-hdr-right'>"
        "    <div class='ll-dbrow'><span class='ll-label'>Debug</span>");

    for (int i = 0; i < MAX_DEBUG_LEVELS; i++) {
        uint16_t m = g_dblevel_names[i].mask;
        int on = !!(g_dblevel & m);
        pos = buf_printf(&buf, &bsz, pos,
            "<a id='db%u' href='#' class='dt%s' onclick='toggleDbg(%u);return false;' title='0x%04X'>%s</a>",
            m, on ? " on" : "", m, m, g_dblevel_names[i].name);
    }

    pos = buf_printf(&buf, &bsz, pos,
        "<a id='dbALL' href='#' class='dt%s' onclick='toggleAll();return false;'>ALL</a>"
        "<div class='ll-vsep'></div>"
        "<div class='ll-meta'><span class='ll-dot'></span>mask:<span id='dbmask' class='ll-mask'>0x%04X</span>"
        "&nbsp;|&nbsp;<span id='linecnt' class='ll-mask'>0</span> lines"
        "&nbsp;|&nbsp;<span id='llstate' class='ll-mask'>Live</span></div>"
        "</div></div></div>"
        "<div class='cb' style='padding:10px 18px'>"
        "<div class='ll-toolbar2'>"
        "<input class='ls' id='filter' name='log_filter' type='search' autocomplete='off' autocorrect='off' autocapitalize='off' spellcheck='false' inputmode='search' placeholder='Filter&#8230;' oninput='applyFilter()' title='Regex, e.g.: hit|miss' aria-label='Log filter'>"
        "<div class='ll-sep'></div>"
        "<button class='btn bg sm' onclick='clearLog()'>" ICO_TRASH "&nbsp;Clear</button>"
        "<button class='btn bg sm' onclick='prepSave()'>&#128190;&nbsp;Save</button>"
        "<div class='ll-sep'></div>"
        "<label class='ll-chk'><input type='checkbox' id='asc' checked>&nbsp;Scroll</label>"
        "<label class='ll-chk'><input type='checkbox' id='paused'>&nbsp;Pause</label>"
        "<div class='ll-sep'></div>"
        "<span class='ll-label' title='Capped to limit memory/DOM use'>Lines: 200 max</span>"
        "</div></div></div>",
        (g_dblevel == D_ALL) ? " on" : "", (unsigned)g_dblevel);

    char *lines[WEB_MAX_LINES_POLL];
    char *users[WEB_MAX_LINES_POLL];
    int32_t next_id = 0;
    int32_t total = log_ring_total();
    int32_t from_id = total > WEB_MAX_LINES_POLL ? total - WEB_MAX_LINES_POLL : 0;
    int32_t count = log_ring_since(from_id, lines, users, WEB_MAX_LINES_POLL, &next_id);

    pos = buf_printf(&buf, &bsz, pos,
        "<div id='lw' data-next='%d' data-mask='%u'><pre id='lp'>",
        next_id, (unsigned)g_dblevel);
    for (int i = 0; i < count; i++) {
        char esc[8192];
        html_escape(lines[i], esc, (int)sizeof(esc));
        pos = buf_printf(&buf, &bsz, pos, "<span data-r=\"%s\">%s</span>", esc, esc);
        free(lines[i]);
        free(users[i]);
    }
    pos = buf_printf(&buf, &bsz, pos,
        "</pre></div><script src='/assets/livelog.js?v=%s-20260926' defer></script>", TCMG_VERSION);

    pos = emit_footer(&buf, &bsz, pos);
    PAGE_SEND_AND_FREE(fd);
}

void send_logpoll(int fd, const char *qs)
{
	char dbg_s[16]   = "";
	char since_s[32] = "";
	get_param(qs, "debug", dbg_s,   sizeof(dbg_s));
	get_param(qs, "since", since_s, sizeof(since_s));

	if (dbg_s[0]) {
		char    *end;
		errno = 0;
		long     v = strtol(dbg_s, &end, 0);
		if (errno == 0 && *end == '\0' && v >= 0 && v <= 0xFFFF) {
			uint16_t nv = (uint16_t)v;
			if (nv != g_dblevel) {
				uint16_t old = g_dblevel;
				g_dblevel = nv;
				tcmg_log_dbg(D_HTTP, "debug level changed: 0x%04X -> 0x%04X",
				             (unsigned)old, (unsigned)g_dblevel);
			}
		}
	}

	int32_t from_id = 0;
	if (since_s[0]) {
		char *end = NULL;
		errno = 0;
		long v = strtol(since_s, &end, 10);
		if (errno == 0 && end && *end == '\0' && v > 0 && v <= INT32_MAX) from_id = (int32_t)v;
	}

	char    *lines[WEB_MAX_LINES_POLL];
	char    *users[WEB_MAX_LINES_POLL];
	int32_t  next_id = 0;
	int32_t  count   = log_ring_since(from_id, lines, users,
	                                   WEB_MAX_LINES_POLL, &next_id);

	int   bsz = (count * 2800) + 512;
	char *buf = (char *)malloc((size_t)(bsz < 512 ? 512 : bsz));
	if (!buf) {
		for (int i = 0; i < count; i++) { free(lines[i]); free(users[i]); }
		send_json_error(fd, 503, "Service Unavailable", "out of memory");
		return;
	}

	int pos = 0;
	pos = buf_printf(&buf, &bsz, pos,
		"{\"debug\":%u,\"next\":%d,\"lines\":[",
		(unsigned)g_dblevel, next_id);

	for (int i = 0; i < count; i++) {
		char esc_line[8192];
		char esc_usr[512];
		json_escape(lines[i],              esc_line, sizeof(esc_line));
		json_escape(users[i][0] ? users[i] : "", esc_usr,  sizeof(esc_usr));
		free(lines[i]);
		free(users[i]);
		pos = buf_printf(&buf, &bsz, pos,
			"%s{\"id\":%d,\"usr\":\"%s\",\"line\":\"%s\"}",
			i ? "," : "",
			from_id + i,
			esc_usr,
			esc_line);
	}
	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

