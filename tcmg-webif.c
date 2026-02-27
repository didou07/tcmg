#define MODULE_LOG_PREFIX "webif"
#include "tcmg-globals.h"
#include "tcmg-crypto.h"
#include "tcmg-log.h"

#if !defined(_WIN32) && !defined(_WIN64) && !defined(__MINGW32__) && !defined(__MINGW64__)
#  include <netdb.h>
#  include <sys/select.h>
#endif

#define WEB_SERVER_NAME     "tcmg/" TCMG_VERSION
#define WEB_READ_TIMEOUT_S  10
#define WEB_BUF_SIZE        8192
#define WEB_MAX_LINES_POLL  200
#define WEB_SESSION_TIMEOUT 3600
#define WEB_SESSION_LEN     32
#define WEB_MAX_SESSIONS    16

typedef struct {
	char   token[WEB_SESSION_LEN + 1];
	time_t expires;
} s_session;

static s_session       s_sessions[WEB_MAX_SESSIONS];
static pthread_mutex_t s_sess_lock = PTHREAD_MUTEX_INITIALIZER;

static void session_gen_token(char *out)
{
	uint8_t rnd[16];
	csprng(rnd, sizeof(rnd));
	for (int i = 0; i < 16; i++)
		snprintf(out + i*2, 3, "%02x", rnd[i]);
	out[WEB_SESSION_LEN] = '\0';
}

static void session_create(char *token_out)
{
	session_gen_token(token_out);
	time_t now = time(NULL);
	pthread_mutex_lock(&s_sess_lock);
	int slot = 0;
	time_t oldest = s_sessions[0].expires;
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires <= now) { slot = i; break; }
		if (s_sessions[i].expires < oldest) { oldest = s_sessions[i].expires; slot = i; }
	}
	strncpy(s_sessions[slot].token, token_out, WEB_SESSION_LEN);
	s_sessions[slot].token[WEB_SESSION_LEN] = '\0';
	s_sessions[slot].expires = now + WEB_SESSION_TIMEOUT;
	pthread_mutex_unlock(&s_sess_lock);
}

static int session_check(const char *token)
{
	if (!token || strlen(token) != WEB_SESSION_LEN) return 0;
	time_t now = time(NULL);
	int ok = 0;
	pthread_mutex_lock(&s_sess_lock);
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires > now &&
		    strncmp(s_sessions[i].token, token, WEB_SESSION_LEN) == 0) {
			s_sessions[i].expires = now + WEB_SESSION_TIMEOUT;
			ok = 1; break;
		}
	}
	pthread_mutex_unlock(&s_sess_lock);
	return ok;
}

static const char *cookie_get_session(const char *cookie_hdr, char *buf, int bufsz)
{
	if (!cookie_hdr) return NULL;
	const char *key = "tcmg_session=";
	const char *p = strstr(cookie_hdr, key);
	if (!p) return NULL;
	p += strlen(key);
	int i = 0;
	while (p[i] && p[i] != ';' && p[i] != '\r' && p[i] != '\n' && i < bufsz - 1)
	{
		buf[i] = p[i];
		i++;
	}
	buf[i] = '\0';
	return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  NEW CSS -- DreamBox OpenWebif inspired, fully modernised
 * ═══════════════════════════════════════════════════════════════════════════ */
static const char CSS[] =
/* ── Reset + Font ── */
"@import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;600&family=IBM+Plex+Sans:wght@400;500;600;700&display=swap');"
"*{box-sizing:border-box;margin:0;padding:0}"
":root{"
"  --bg0:#090d14;"
"  --bg1:#0e1421;"
"  --bg2:#141c2e;"
"  --bg3:#1a2338;"
"  --bg4:#20293f;"
"  --border:#1e2d47;"
"  --border2:#253554;"
"  --accent:#3b82f6;"
"  --accent2:#60a5fa;"
"  --accent3:#93c5fd;"
"  --green:#22c55e;"
"  --green2:#4ade80;"
"  --red:#ef4444;"
"  --red2:#f87171;"
"  --yellow:#f59e0b;"
"  --yellow2:#fbbf24;"
"  --purple:#8b5cf6;"
"  --cyan:#06b6d4;"
"  --text0:#e8f0fe;"
"  --text1:#94a3b8;"
"  --text2:#4b6584;"
"  --font-ui:'IBM Plex Sans',sans-serif;"
"  --font-mono:'JetBrains Mono',monospace;"
"}"
"body{"
"  background:var(--bg0);"
"  color:var(--text0);"
"  font-family:var(--font-ui);"
"  font-size:14px;"
"  display:flex;"
"  min-height:100vh;"
"  overflow-x:hidden;"
"}"
/* ── Sidebar ── */
"#sidebar{"
"  width:220px;"
"  min-height:100vh;"
"  background:var(--bg1);"
"  border-right:1px solid var(--border);"
"  display:flex;"
"  flex-direction:column;"
"  position:fixed;"
"  top:0;left:0;bottom:0;"
"  z-index:100;"
"  transition:width 0.25s;"
"}"
"#sidebar.collapsed{width:54px}"
"#sidebar.collapsed .nav-label{display:none}"
"#sidebar.collapsed .logo-text{display:none}"
"#sidebar.collapsed .logo-ver{display:none}"
"#sidebar.collapsed .nav-group-label{display:none}"
/* ── Logo area ── */
".logo-area{"
"  padding:12px 14px 10px;"
"  border-bottom:1px solid var(--border);"
"  display:flex;align-items:center;gap:10px;"
"}"
".logo-icon{"
"  width:32px;height:32px;flex-shrink:0;"
"  background:linear-gradient(135deg,var(--accent),var(--cyan));"
"  border-radius:8px;"
"  display:flex;align-items:center;justify-content:center;"
"  font-size:16px;font-weight:700;color:#fff;font-family:var(--font-mono);"
"}"
".logo-text{font-size:15px;font-weight:700;color:var(--text0);letter-spacing:0.5px}"
".logo-ver{font-size:10px;color:var(--text2);font-family:var(--font-mono);margin-top:1px}"
/* ── Nav groups ── */
".nav-group-label{"
"  padding:10px 14px 3px;"
"  font-size:10px;font-weight:600;"
"  text-transform:uppercase;letter-spacing:1.5px;"
"  color:var(--text2);"
"}"
"nav a{"
"  display:flex;align-items:center;gap:10px;"
"  padding:7px 14px;"
"  color:var(--text1);"
"  text-decoration:none;"
"  font-size:13px;font-weight:500;"
"  border-left:3px solid transparent;"
"  transition:all 0.15s;"
"  white-space:nowrap;"
"  overflow:hidden;"
"}"
"nav a:hover{background:var(--bg2);color:var(--text0);border-left-color:var(--border2)}"
"nav a.active{"
"  background:linear-gradient(90deg,rgba(59,130,246,.15),transparent);"
"  color:var(--accent2);"
"  border-left-color:var(--accent);"
"}"
".nav-icon{width:18px;height:18px;flex-shrink:0;opacity:0.75}"
"nav a.active .nav-icon,nav a:hover .nav-icon{opacity:1}"
/* ── Main area ── */
"#main{"
"  margin-left:220px;"
"  flex:1;"
"  display:flex;flex-direction:column;"
"  min-height:100vh;"
"  transition:margin-left 0.25s;"
"}"
"#main.expanded{margin-left:54px}"
/* ── Top header ── */
"#topbar{"
"  height:44px;"
"  background:var(--bg1);"
"  border-bottom:1px solid var(--border);"
"  display:flex;align-items:center;"
"  padding:0 20px;"
"  gap:14px;"
"  position:sticky;top:0;z-index:50;"
"}"
".topbar-title{"
"  font-size:15px;font-weight:600;color:var(--text0);"
"  display:flex;align-items:center;gap:8px;"
"}"
".topbar-badge{"
"  font-size:11px;background:var(--bg3);border:1px solid var(--border2);"
"  border-radius:4px;padding:2px 7px;"
"  color:var(--text1);font-family:var(--font-mono);"
"}"
".topbar-right{margin-left:auto;display:flex;align-items:center;gap:10px}"
".poll-ctrl{display:flex;align-items:center;gap:3px;background:var(--bg3);"
"  border:1px solid var(--border2);border-radius:5px;padding:2px 5px;}"
".poll-ctrl label{font-size:10px;color:var(--text2);white-space:nowrap;margin-right:2px}"
".poll-ctrl input{width:32px;background:none;border:none;color:var(--text1);"
"  font-family:var(--font-mono);font-size:12px;text-align:center;outline:none;}"
".poll-ctrl button{background:none;border:none;cursor:pointer;color:var(--text2);"
"  font-size:13px;line-height:1;padding:0 2px;border-radius:3px;}"
".poll-ctrl button:hover{color:var(--text0);background:var(--bg4)}"
".status-pill{"
"  display:flex;align-items:center;gap:6px;"
"  background:rgba(34,197,94,.1);border:1px solid rgba(34,197,94,.25);"
"  border-radius:20px;padding:4px 10px;"
"  font-size:12px;color:var(--green);font-weight:500;"
"}"
"#collapse-btn{"
"  background:none;border:none;cursor:pointer;color:var(--text2);"
"  padding:6px;border-radius:5px;transition:all .15s;"
"}"
"#collapse-btn:hover{background:var(--bg2);color:var(--text0)}"
/* ── Content area ── */
"#content{padding:16px 20px;flex:1}"
/* ── Stat cards grid ── */
".cards-grid{"
"  display:grid;"
"  grid-template-columns:repeat(auto-fill,minmax(160px,1fr));"
"  gap:10px;margin-bottom:16px;"
"}"
".card{"
"  background:var(--bg2);"
"  border:1px solid var(--border);"
"  border-radius:8px;"
"  padding:12px 14px;"
"  position:relative;overflow:hidden;"
"  transition:border-color .2s,transform .15s;"
"}"
".card:hover{border-color:var(--border2);transform:translateY(-1px)}"
".card::before{"
"  content:'';"
"  position:absolute;top:0;left:0;right:0;height:2px;"
"  background:linear-gradient(90deg,var(--accent),var(--cyan));"
"  opacity:0;"
"  transition:opacity .2s;"
"}"
".card:hover::before{opacity:1}"
".card.green::before{background:linear-gradient(90deg,var(--green),var(--cyan));opacity:1}"
".card.red::before{background:linear-gradient(90deg,var(--red),var(--yellow));opacity:1}"
".card.blue::before{background:linear-gradient(90deg,var(--accent),var(--purple));opacity:1}"
".card.yellow::before{background:linear-gradient(90deg,var(--yellow),var(--red));opacity:1}"
".card-label{"
"  font-size:11px;font-weight:600;"
"  text-transform:uppercase;letter-spacing:1.2px;"
"  color:var(--text2);margin-bottom:8px;"
"}"
".card-value{"
"  font-size:22px;font-weight:700;"
"  font-family:var(--font-mono);"
"  color:var(--text0);line-height:1;"
"}"
".card-value.green{color:var(--green2)}"
".card-value.red{color:var(--red2)}"
".card-value.blue{color:var(--accent2)}"
".card-value.yellow{color:var(--yellow2)}"
".card-sub{font-size:11px;color:var(--text2);margin-top:3px}"
".card-icon{"
"  position:absolute;right:14px;top:14px;"
"  width:32px;height:32px;opacity:0.12;"
"}"
/* ── Section headers ── */
".section-hdr{"
"  display:flex;align-items:center;justify-content:space-between;"
"  margin-bottom:8px;margin-top:2px;"
"}"
".section-title{"
"  font-size:13px;font-weight:600;"
"  color:var(--text0);letter-spacing:0.3px;"
"  display:flex;align-items:center;gap:8px;"
"}"
".section-title::before{"
"  content:'';"
"  display:inline-block;width:3px;height:14px;"
"  background:var(--accent);"
"  border-radius:2px;"
"}"
/* ── Tables ── */
".tbl-wrap{border:1px solid var(--border);border-radius:8px;overflow:hidden;margin-bottom:14px}"
"table{width:100%;border-collapse:collapse;font-size:13px}"
"thead tr{background:var(--bg3)}"
"th{"
"  padding:7px 12px;"
"  text-align:left;"
"  font-size:11px;font-weight:600;"
"  text-transform:uppercase;letter-spacing:1px;"
"  color:var(--text2);"
"  border-bottom:1px solid var(--border);"
"  white-space:nowrap;"
"}"
"td{"
"  padding:8px 12px;"
"  border-bottom:1px solid var(--border);"
"  color:var(--text0);"
"}"
"tbody tr:last-child td{border-bottom:none}"
"tbody tr:hover{background:var(--bg3)}"
"tbody tr.animated{animation:row-flash .4s ease}"
"@keyframes row-flash{from{background:rgba(59,130,246,.15)}to{background:transparent}}"
".mono{font-family:var(--font-mono);font-size:12px}"
".bold{font-weight:600}"
/* ── Badges ── */
".badge{"
"  display:inline-flex;align-items:center;gap:4px;"
"  padding:2px 8px;border-radius:4px;"
"  font-size:11px;font-weight:600;font-family:var(--font-mono);"
"}"
".badge-on{background:rgba(34,197,94,.15);color:var(--green2);border:1px solid rgba(34,197,94,.3)}"
".badge-off{background:rgba(239,68,68,.15);color:var(--red2);border:1px solid rgba(239,68,68,.3)}"
".badge-ban{background:rgba(245,158,11,.15);color:var(--yellow2);border:1px solid rgba(245,158,11,.3)}"
".badge-blue{background:rgba(59,130,246,.15);color:var(--accent2);border:1px solid rgba(59,130,246,.3)}"
/* ── Buttons ── */
".btn{"
"  display:inline-flex;align-items:center;gap:6px;"
"  padding:7px 14px;border-radius:6px;"
"  font-size:12px;font-weight:600;"
"  cursor:pointer;border:1px solid transparent;"
"  font-family:var(--font-ui);"
"  transition:all .15s;text-decoration:none;"
"}"
".btn-primary{"
"  background:var(--accent);color:#fff;"
"  border-color:var(--accent);"
"}"
".btn-primary:hover{background:#2563eb;border-color:#2563eb}"
".btn-ghost{"
"  background:var(--bg3);color:var(--text1);"
"  border-color:var(--border2);"
"}"
".btn-ghost:hover{background:var(--bg4);color:var(--text0)}"
".btn-danger{background:rgba(239,68,68,.15);color:var(--red2);border-color:rgba(239,68,68,.3)}"
".btn-danger:hover{background:rgba(239,68,68,.25)}"
".btn-sm{padding:4px 10px;font-size:11px}"
/* ── Kill button ── */
".kill-btn{"
"  display:inline-flex;align-items:center;"
"  color:var(--red2);opacity:0.5;"
"  cursor:pointer;background:none;border:none;"
"  font-size:15px;padding:3px 6px;"
"  border-radius:4px;transition:all .15s;"
"}"
".kill-btn:hover{opacity:1;background:rgba(239,68,68,.15)}"
/* ── Log viewer ── */
"#logwrap{"
"  background:#060a10;"
"  border:1px solid var(--border);"
"  border-radius:8px;"
"  height:440px;overflow:auto;"
"  padding:12px;"
"}"
"#logpre{"
"  margin:0;"
"  font-family:var(--font-mono);"
"  font-size:12px;"
"  line-height:1.7;"
"  white-space:pre;"
"}"
".log-ctrl{"
"  display:flex;align-items:center;gap:8px;"
"  margin-bottom:8px;flex-wrap:wrap;"
"}"
".log-search{"
"  background:var(--bg2);border:1px solid var(--border2);"
"  color:var(--text0);border-radius:6px;"
"  padding:5px 10px;font-size:12px;"
"  font-family:var(--font-mono);width:200px;"
"}"
".log-search:focus{outline:none;border-color:var(--accent)}"
"select.log-sel{"
"  background:var(--bg2);color:var(--text1);"
"  border:1px solid var(--border2);border-radius:6px;"
"  padding:5px 8px;font-size:12px;cursor:pointer;"
"}"
/* ── Debug toggles ── */
".dbg-bar{"
"  background:var(--bg2);border:1px solid var(--border);"
"  border-radius:7px;padding:8px 12px;"
"  margin-bottom:8px;"
"  display:flex;flex-wrap:wrap;align-items:center;gap:5px;"
"}"
".dbg-tag{"
"  display:inline-flex;align-items:center;"
"  padding:3px 10px;border-radius:4px;"
"  font-size:11px;font-family:var(--font-mono);font-weight:500;"
"  cursor:pointer;border:1px solid var(--border2);"
"  color:var(--text2);background:transparent;"
"  transition:all .15s;user-select:none;text-decoration:none;"
"}"
".dbg-tag.on{"
"  background:rgba(59,130,246,.15);"
"  border-color:rgba(59,130,246,.4);"
"  color:var(--accent2);"
"}"
".dbg-tag:hover{border-color:var(--accent);color:var(--accent2)}"
".dbg-mask{font-size:11px;color:var(--text2);font-family:var(--font-mono);margin-left:auto}"
/* ── Config editor ── */
".cfg-editor{"
"  width:100%;height:360px;"
"  background:#060a10;color:#c8e6c9;"
"  font-family:var(--font-mono);font-size:13px;"
"  border:1px solid var(--border);border-radius:8px;"
"  padding:12px;resize:vertical;"
"  line-height:1.6;"
"}"
".cfg-editor:focus{outline:none;border-color:var(--accent)}"
/* ── Progress bar ── */
".hitbar-wrap{background:var(--bg3);border-radius:4px;height:5px;width:80px;overflow:hidden}"
".hitbar-fill{height:100%;border-radius:4px;background:linear-gradient(90deg,var(--green),var(--cyan));transition:width .4s}"
/* ── Login page ── */
".login-bg{"
"  min-height:100vh;width:100%;display:flex;"
"  align-items:center;justify-content:center;"
"  background:var(--bg0);"
"  background-image:radial-gradient(ellipse at 20% 50%,rgba(59,130,246,.05) 0%,transparent 60%),"
"  radial-gradient(ellipse at 80% 20%,rgba(6,182,212,.05) 0%,transparent 60%);"
"}"
".login-card{"
"  background:var(--bg2);"
"  border:1px solid var(--border);"
"  border-radius:12px;"
"  padding:28px 36px;"
"  width:340px;"
"  box-shadow:0 20px 50px rgba(0,0,0,.5);"
"}"
".login-logo{"
"  display:flex;align-items:center;gap:12px;margin-bottom:20px;"
"}"
".login-logo-icon{"
"  width:42px;height:42px;"
"  background:linear-gradient(135deg,var(--accent),var(--cyan));"
"  border-radius:10px;"
"  display:flex;align-items:center;justify-content:center;"
"  font-size:20px;font-weight:700;color:#fff;font-family:var(--font-mono);"
"}"
".login-logo-text{font-size:20px;font-weight:700}"
".login-logo-ver{font-size:11px;color:var(--text2);font-family:var(--font-mono)}"
".form-label{font-size:11px;font-weight:600;color:var(--text2);letter-spacing:0.5px;margin-bottom:5px;display:block}"
".form-input{"
"  width:100%;padding:9px 12px;"
"  background:var(--bg1);border:1px solid var(--border2);"
"  color:var(--text0);border-radius:7px;"
"  font-size:13px;font-family:var(--font-ui);"
"  transition:border-color .15s;"
"}"
".form-input:focus{outline:none;border-color:var(--accent)}"
".form-group{margin-bottom:12px}"
".login-err{"
"  display:flex;align-items:center;gap:8px;"
"  background:rgba(239,68,68,.12);border:1px solid rgba(239,68,68,.3);"
"  border-radius:7px;padding:9px 12px;"
"  color:var(--red2);font-size:12px;margin-bottom:16px;"
"}"
/* ── Tooltip ── */
".tooltip{position:relative}"
".tooltip-tip{"
"  display:none;position:absolute;bottom:calc(100% + 6px);left:50%;"
"  transform:translateX(-50%);"
"  background:var(--bg4);border:1px solid var(--border2);"
"  border-radius:5px;padding:4px 8px;"
"  font-size:11px;color:var(--text0);"
"  white-space:nowrap;z-index:200;"
"}"
".tooltip:hover .tooltip-tip{display:block}"
/* ── Misc ── */
".text-green{color:var(--green2)}"
".text-red{color:var(--red2)}"
".text-yellow{color:var(--yellow2)}"
".text-blue{color:var(--accent2)}"
".text-muted{color:var(--text2)}"
".flex{display:flex;align-items:center}"
".gap-8{gap:8px}"
".gap-10{gap:10px}"
".mb-20{margin-bottom:14px}"
".mb-10{margin-bottom:8px}"
"a.danger{color:var(--red2)}"
"hr{border:none;border-top:1px solid var(--border);margin:14px 0}"
".empty-row td{text-align:center;color:var(--text2);padding:18px}"
".pulse-sm{width:6px!important;height:6px!important}"
/* ── Info box ── */
".info-box{"
"  background:var(--bg2);border:1px solid var(--border);"
"  border-radius:8px;padding:10px 14px;"
"  margin-bottom:12px;font-size:12px;color:var(--text2);"
"}"
".card-value.sm{font-size:16px}"
"input[type=checkbox]{accent-color:var(--accent)}"
"label{cursor:pointer}";

static pthread_t  s_webif_tid;
static int8_t     s_webif_running = 0;
static int        s_webif_sock    = -1;

static int buf_printf(char **dst, int *dstsz, int pos, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

static int buf_printf(char **dst, int *dstsz, int pos, const char *fmt, ...)
{
	va_list ap;
	int needed;
	va_start(ap, fmt);
	needed = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (pos + needed + 1 >= *dstsz)
	{
		int newsz = pos + needed + 8192;
		char *nb  = (char *)realloc(*dst, newsz);
		if (!nb) return pos;
		*dst   = nb;
		*dstsz = newsz;
	}
	va_start(ap, fmt);
	vsnprintf(*dst + pos, *dstsz - pos, fmt, ap);
	va_end(ap);
	return pos + needed;
}

static void url_decode(char *s)
{
	char *r = s, *w = s;
	while (*r)
	{
		if (*r == '%' && r[1] && r[2])
		{
			char h[3] = { r[1], r[2], 0 };
			*w++ = (char)strtol(h, NULL, 16);
			r += 3;
		}
		else if (*r == '+') { *w++ = ' '; r++; }
		else                { *w++ = *r++; }
	}
	*w = '\0';
}

static void get_param(const char *qs, const char *key, char *out, int outsz)
{
	out[0] = '\0';
	if (!qs) return;
	int klen = (int)strlen(key);
	const char *p = qs;
	while (*p)
	{
		if (strncmp(p, key, klen) == 0 && p[klen] == '=')
		{
			p += klen + 1;
			int i = 0;
			while (*p && *p != '&' && i < outsz - 1)
				out[i++] = *p++;
			out[i] = '\0';
			url_decode(out);
			return;
		}
		while (*p && *p != '&') p++;
		if (*p == '&') p++;
	}
}

static void form_get(const char *body, const char *key, char *out, int outsz)
{
	out[0] = '\0';
	if (!body) return;
	char needle[64];
	snprintf(needle, sizeof(needle), "%s=", key);
	const char *p = strstr(body, needle);
	if (!p) return;
	p += strlen(needle);
	int i = 0;
	while (*p && *p != '&' && i < outsz - 1)
	{
		if (*p == '+') { out[i++] = ' '; p++; }
		else if (*p == '%' && p[1] && p[2])
		{
			char hex[3] = { p[1], p[2], 0 };
			out[i++] = (char)strtol(hex, NULL, 16);
			p += 3;
		}
		else out[i++] = *p++;
	}
	out[i] = '\0';
}

static void send_headers_ex(int fd, int code, const char *reason,
                             const char *ctype, int length,
                             const char *set_cookie)
{
	char hdr[768];
	time_t now = time(NULL);
	char date_str[64];
	struct tm tm_s;
	gmtime_r(&now, &tm_s);
	strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &tm_s);
	char cookie_line[256] = "";
	if (set_cookie && set_cookie[0])
		snprintf(cookie_line, sizeof(cookie_line),
		         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Strict\r\n",
		         set_cookie);
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 %d %s\r\n"
	         "Server: %s\r\n"
	         "Date: %s\r\n"
	         "Content-Type: %s\r\n"
	         "Content-Length: %d\r\n"
	         "Cache-Control: no-store, no-cache\r\n"
	         "%s"
	         "Connection: close\r\n"
	         "\r\n",
	         code, reason,
	         WEB_SERVER_NAME,
	         date_str,
	         ctype,
	         length,
	         cookie_line);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}

static void send_response_ex(int fd, int code, const char *reason,
                              const char *ctype, const char *body, int blen,
                              const char *set_cookie)
{
	send_headers_ex(fd, code, reason, ctype, blen, set_cookie);
	if (body && blen > 0)
		send(fd, SO_CAST(body), blen, MSG_NOSIGNAL);
}

static void send_response(int fd, int code, const char *reason,
                            const char *ctype, const char *body, int blen)
{
	send_response_ex(fd, code, reason, ctype, body, blen, NULL);
}

static void send_redirect(int fd, const char *location)
{
	char hdr[256];
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n"
	         "Connection: close\r\n\r\n", location);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}

static void send_redirect_with_cookie(int fd, const char *location, const char *token)
{
	char hdr[512];
	snprintf(hdr, sizeof(hdr),
	         "HTTP/1.1 302 Found\r\nLocation: %s\r\n"
	         "Set-Cookie: tcmg_session=%s; Path=/; HttpOnly; SameSite=Strict\r\n"
	         "Content-Length: 0\r\nConnection: close\r\n\r\n",
	         location, token);
	send(fd, SO_CAST(hdr), (int)strlen(hdr), MSG_NOSIGNAL);
}

/* ── base64 ── */
static const char B64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode(const char *in, int ilen, char *out, int outsz)
{
	int i = 0, o = 0;
	uint8_t *s = (uint8_t *)in;
	while (i < ilen && o + 4 < outsz)
	{
		int rem = ilen - i;
		uint32_t v = ((uint32_t)s[i] << 16)
		           | (rem > 1 ? (uint32_t)s[i+1] << 8 : 0)
		           | (rem > 2 ? (uint32_t)s[i+2]      : 0);
		out[o++] = B64[(v >> 18) & 0x3F];
		out[o++] = B64[(v >> 12) & 0x3F];
		out[o++] = rem > 1 ? B64[(v >> 6) & 0x3F] : '=';
		out[o++] = rem > 2 ? B64[v & 0x3F]        : '=';
		i += 3;
	}
	out[o] = '\0';
}

static int check_auth(const char *auth_header)
{
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) return 1;
	if (!auth_header) return 0;
	const char *p = strstr(auth_header, "Basic ");
	if (!p) return 0;
	p += 6;
	char got[512];
	int  glen = 0;
	while (*p && *p != '\r' && *p != '\n' && *p != ' ' && glen < (int)sizeof(got) - 1)
		got[glen++] = *p++;
	got[glen] = '\0';
	char creds[256];
	snprintf(creds, sizeof(creds), "%s:%s", g_cfg.webif_user, g_cfg.webif_pass);
	char expected[512];
	b64_encode(creds, (int)strlen(creds), expected, sizeof(expected));
	return ct_streq(got, expected) ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  PAGE STRUCTURE -- sidebar layout
 * ═══════════════════════════════════════════════════════════════════════════ */

/* SVG icons (inline, no deps) */
#define ICO_STATUS  "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path d='M2 10a8 8 0 1116 0A8 8 0 012 10zm8-5a1 1 0 00-1 1v4a1 1 0 00.553.894l3 1.5a1 1 0 10.894-1.788L11 9.382V6a1 1 0 00-1-1z'/></svg>"
#define ICO_USERS   "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path d='M9 6a3 3 0 110 6 3 3 0 010-6zM17 6a3 3 0 110 6 3 3 0 010-6zM12.93 17c.046-.327.07-.66.07-1a6.97 6.97 0 00-1.5-4.33A5 5 0 0119 16v1h-6.07zM6 11a5 5 0 015 5v1H1v-1a5 5 0 015-5z'/></svg>"
#define ICO_BAN     "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M13.477 14.89A6 6 0 015.11 6.524l8.367 8.368zm1.414-1.414L6.524 5.11a6 6 0 018.367 8.367zM18 10a8 8 0 11-16 0 8 8 0 0116 0z' clip-rule='evenodd'/></svg>"
#define ICO_LOG     "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M3 4a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 4a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 4a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 4a1 1 0 011-1h4a1 1 0 110 2H4a1 1 0 01-1-1z' clip-rule='evenodd'/></svg>"
#define ICO_CFG     "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M11.49 3.17c-.38-1.56-2.6-1.56-2.98 0a1.532 1.532 0 01-2.286.948c-1.372-.836-2.942.734-2.106 2.106.54.886.061 2.042-.947 2.287-1.561.379-1.561 2.6 0 2.978a1.532 1.532 0 01.947 2.287c-.836 1.372.734 2.942 2.106 2.106a1.532 1.532 0 012.287.947c.379 1.561 2.6 1.561 2.978 0a1.533 1.533 0 012.287-.947c1.372.836 2.942-.734 2.106-2.106a1.533 1.533 0 01.947-2.287c1.561-.379 1.561-2.6 0-2.978a1.532 1.532 0 01-.947-2.287c.836-1.372-.734-2.942-2.106-2.106a1.532 1.532 0 01-2.287-.947zM10 13a3 3 0 100-6 3 3 0 000 6z' clip-rule='evenodd'/></svg>"
#define ICO_STOP    "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M10 18a8 8 0 100-16 8 8 0 000 16zM8 7a1 1 0 00-1 1v4a1 1 0 001 1h4a1 1 0 001-1V8a1 1 0 00-1-1H8z' clip-rule='evenodd'/></svg>"
#define ICO_RESTART "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M4 2a1 1 0 011 1v2.101a7.002 7.002 0 0111.601 2.566 1 1 0 11-1.885.666A5.002 5.002 0 005.999 7H9a1 1 0 010 2H4a1 1 0 01-1-1V3a1 1 0 011-1zm.008 9.057a1 1 0 011.276.61A5.002 5.002 0 0014.001 13H11a1 1 0 110-2h5a1 1 0 011 1v5a1 1 0 11-2 0v-2.101a7.002 7.002 0 01-11.601-2.566 1 1 0 01.61-1.276z' clip-rule='evenodd'/></svg>"
#define ICO_TVCAS   "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M2.166 4.999A11.954 11.954 0 0010 1.944 11.954 11.954 0 0017.834 5c.11.65.166 1.32.166 2.001 0 5.225-3.34 9.67-8 11.317C5.34 16.67 2 12.225 2 7c0-.682.057-1.35.166-2.001zm11.541 3.708a1 1 0 00-1.414-1.414L9 10.586 7.707 9.293a1 1 0 00-1.414 1.414l2 2a1 1 0 001.414 0l4-4z' clip-rule='evenodd'/></svg>"

/* ── Nav item type (shared across all nav groups) ── */
typedef struct { const char *id; const char *href; const char *icon; const char *label; } NavItem;

static int emit_header(char **buf, int *bsz, int pos,
                        const char *title, const char *active)
{
	int is_status = (strcmp(active, "status") == 0);

	char upstr[32];
	format_uptime(time(NULL) - g_start_time, upstr, sizeof(upstr));

	pos = buf_printf(buf, bsz, pos,
		"<!DOCTYPE html><html lang='en'><head>"
		"<meta charset='UTF-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>tcmg -- %s</title>"
		"<style>%s</style>"
		"</head><body>",
		title, CSS);

	/* ── Sidebar ── */
	pos = buf_printf(buf, bsz, pos,
		"<div id='sidebar'>"
		"  <div class='logo-area'>"
		"    <div class='logo-icon'>tc</div>"
		"    <div>"
		"      <div class='logo-text'>tcmg</div>"
		"      <div class='logo-ver'>" TCMG_VERSION "</div>"
		"    </div>"
		"  </div>"
		"  <div class='nav-group-label'>Monitor</div>"
		"  <nav>");

	static const NavItem pages[] = {
		{ "status",   "/status",   ICO_STATUS, "Status"   },
		{ "livelog",  "/livelog",  ICO_LOG,    "Live Log" },
		{ NULL, NULL, NULL, NULL }
	};
	static const NavItem pages2[] = {
		{ "users",    "/users",    ICO_USERS,  "Users"    },
		{ "failban",  "/failban",  ICO_BAN,    "Fail-Ban" },
		{ NULL, NULL, NULL, NULL }
	};
	static const NavItem pages3[] = {
		{ "config",   "/config",   ICO_CFG,     "Config"   },
		{ "restart",  "/restart",  ICO_RESTART, "Restart"  },
		{ "shutdown", "/shutdown", ICO_STOP,    "Shutdown" },
		{ NULL, NULL, NULL, NULL }
	};
	static const NavItem pages4[] = {
		{ "tvcas",    "/tvcas",    ICO_TVCAS,  "TVCAS Tool" },
		{ NULL, NULL, NULL, NULL }
	};

	for (int i = 0; pages[i].id; i++) {
		const char *cls = (strcmp(pages[i].id, active) == 0) ? " active" : "";
		pos = buf_printf(buf, bsz, pos,
			"<a href='%s' class='%s'>%s<span class='nav-label'>%s</span></a>",
			pages[i].href, cls, pages[i].icon, pages[i].label);
	}

	pos = buf_printf(buf, bsz, pos, "</nav><div class='nav-group-label'>Accounts</div><nav>");
	for (int i = 0; pages2[i].id; i++) {
		const char *cls = (strcmp(pages2[i].id, active) == 0) ? " active" : "";
		pos = buf_printf(buf, bsz, pos,
			"<a href='%s' class='%s'>%s<span class='nav-label'>%s</span></a>",
			pages2[i].href, cls, pages2[i].icon, pages2[i].label);
	}

	pos = buf_printf(buf, bsz, pos, "</nav><div class='nav-group-label'>System</div><nav>");
	for (int i = 0; pages3[i].id; i++) {
		const char *cls = (strcmp(pages3[i].id, active) == 0) ? " active" : "";
		pos = buf_printf(buf, bsz, pos,
			"<a href='%s' class='%s'>%s<span class='nav-label'>%s</span></a>",
			pages3[i].href, cls, pages3[i].icon, pages3[i].label);
	}

	pos = buf_printf(buf, bsz, pos, "</nav><div class='nav-group-label'>Tools</div><nav>");
	for (int i = 0; pages4[i].id; i++) {
		const char *cls = (strcmp(pages4[i].id, active) == 0) ? " active" : "";
		pos = buf_printf(buf, bsz, pos,
			"<a href='%s' class='%s'>%s<span class='nav-label'>%s</span></a>",
			pages4[i].href, cls, pages4[i].icon, pages4[i].label);
	}

	pos = buf_printf(buf, bsz, pos,
		"</nav>"
		"</div>");

	char srv_addr[64];
	snprintf(srv_addr, sizeof(srv_addr), "%s:%d",
	         g_cfg.webif_bindaddr[0] ? g_cfg.webif_bindaddr : "0.0.0.0",
	         g_cfg.webif_port);

	/* ── Main ── */
	pos = buf_printf(buf, bsz, pos,
		"<div id='main'>"
		"<div id='topbar'>"
		"  <button id='collapse-btn' onclick='toggleSidebar()' title='Toggle sidebar'>"
		"    <svg width='18' height='18' viewBox='0 0 20 20' fill='currentColor'>"
		"    <path fill-rule='evenodd' d='M3 5a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 5a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 5a1 1 0 011-1h6a1 1 0 110 2H4a1 1 0 01-1-1z' clip-rule='evenodd'/></svg>"
		"  </button>"
		"  <div class='topbar-title'>"
		"    %s"
		"    <span class='topbar-badge'>%s</span>"
		"  </div>"
		"  <div class='topbar-right'>"
		"    <div class='status-pill'>"
		"      <div class='pulse-dot pulse-sm'></div>"
		"      <span id='tb_conns'>%d</span> online"
		"    </div>"
		"    <span class='topbar-badge' id='sb_up'>%s</span>"
		"    <div class='poll-ctrl' title='Auto-refresh interval (seconds)'>"
		"      <label>REFRESH</label>"
		"      <button onclick='_adjPoll(-1)'>&#8722;</button>"
		"      <input id='poll_sec' type='text' value='%d' readonly>"
		"      <button onclick='_adjPoll(1)'>+</button>"
		"    </div>"
		"  </div>"
		"</div>"
		"<div id='content'>",
		title, srv_addr,
		g_active_conns, upstr,
		g_cfg.webif_refresh > 0 ? g_cfg.webif_refresh : 5);

	/* Sidebar collapse JS + global poll control + topbar live update (all pages) */
	pos = buf_printf(buf, bsz, pos,
		"<script>"
		"(function(){"
		"  var s=document.getElementById('sidebar');"
		"  var m=document.getElementById('main');"
		"  if(sessionStorage.tcmg_sb=='1'){s.classList.add('collapsed');m.classList.add('expanded');}"
		"})();"
		"function toggleSidebar(){"
		"  var s=document.getElementById('sidebar');"
		"  var m=document.getElementById('main');"
		"  s.classList.toggle('collapsed');"
		"  m.classList.toggle('expanded');"
		"  sessionStorage.tcmg_sb=s.classList.contains('collapsed')?'1':'0';"
		"}"
		"var _pm=(function(){"
		"  var v=parseInt(sessionStorage.tcmg_poll)||%d;"
		"  document.getElementById('poll_sec').value=v;"
		"  return v*1000;"
		"})();"
		"function _adjPoll(d){"
		"  var el=document.getElementById('poll_sec');"
		"  var v=Math.max(1,Math.min(99,parseInt(el.value)||5)+d);"
		"  el.value=v;"
		"  _pm=v*1000;"
		"  sessionStorage.tcmg_poll=v;"
		"}"
		/* topbar updater: runs on ALL pages */
		"(function _tbpoll(){"
		"  fetch('/api/status',{cache:'no-store'})"
		"    .then(function(r){return r.json();})"
		"    .then(function(d){"
		"      var e;"
		"      e=document.getElementById('sb_up');    if(e)e.textContent=d.uptime_str;"
		"      e=document.getElementById('tb_conns'); if(e)e.textContent=d.active_connections;"
		"      setTimeout(_tbpoll,_pm);"
		"    })"
		"    .catch(function(){setTimeout(_tbpoll,_pm*3);});"
		"})();"
		"</script>",
		g_cfg.webif_refresh > 0 ? g_cfg.webif_refresh : 5);

	/* Status auto-poll JS (only on status page) */
	if (is_status)
	{
		pos = buf_printf(buf, bsz, pos,
			"<script>"
			"var _pl=false;"
			"function _esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
			"function _poll(){"
			"  if(_pl){setTimeout(_poll,_pm);return;}"
			"  _pl=true;"
			"  fetch('/api/status',{cache:'no-store'})"
			"    .then(r=>r.json())"
			"    .then(d=>{"
			"      _pl=false;_updateStatus(d);"
			"      setTimeout(_poll,_pm);"
			"    })"
			"    .catch(()=>{_pl=false;setTimeout(_poll,_pm*3);});"
			"}"
			"function _kill(tid,user){if(confirm('Disconnect '+user+'?'))location='/status?kill='+tid;}"
			"function _numFmt(n){return n>=1e6?(n/1e6).toFixed(1)+'M':n>=1e3?(n/1e3).toFixed(1)+'K':n}"
			"function _updateStatus(d){"
			"  var set=function(id,v){var e=document.getElementById(id);if(e)e.textContent=v;};"
			"  set('p_up',d.uptime_str);"
			"  set('sb_up',d.uptime_str);"
			"  set('p_conn',d.active_connections);"
			"  set('p_acc',d.accounts);"
			"  set('p_hit',_numFmt(d.cw_found));"
			"  set('p_miss',_numFmt(d.cw_not));"
			"  set('p_ban',d.banned_ips);"
			"  set('p_ecm',_numFmt(d.ecm_total));"
			"  set('tb_conns',d.active_connections);"
			"  var tb=document.getElementById('p_clients');"
			"  if(!tb)return;"
			"  var rows='';"
			"  (d.clients||[]).forEach(function(cl){"
			"    rows+='<tr>'"
			"      +'<td><span class=\"bold\">'+_esc(cl.user)+'</span></td>'"
			"      +'<td class=\"mono\">'+_esc(cl.ip)+'</td>'"
			"      +'<td class=\"mono\"><span class=\"badge badge-blue\">'+_esc(cl.caid)+'</span></td>'"
			"      +'<td class=\"mono\">'+_esc(cl.sid)+'</td>'"
			"      +'<td>'+_esc(cl.channel||'—')+'</td>'"
			"      +'<td class=\"mono text-muted\">'+_esc(cl.connected)+'</td>'"
			"      +'<td class=\"mono text-muted\">'+_esc(cl.idle)+'</td>'"
			"      +'<td><button class=\"kill-btn\" onclick=\"_kill('+cl.thread_id+',\\''+_esc(cl.user)+'\\')\" title=\"Disconnect\"><svg width=\"13\" height=\"13\" viewBox=\"0 0 20 20\" fill=\"currentColor\"><path fill-rule=\"evenodd\" d=\"M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z\" clip-rule=\"evenodd\"/></svg></button></td>'"
			"      +'</tr>';"
			"  });"
			"  tb.innerHTML=rows||'<tr class=\"empty-row\"><td colspan=\"8\">No active connections</td></tr>';"
			"}"
			"document.addEventListener('DOMContentLoaded',function(){setTimeout(_poll,_pm);});"
			"</script>");
	}

	return pos;
}

static int emit_footer(char **buf, int *bsz, int pos)
{
	char upstr[32];
	format_uptime(time(NULL) - g_start_time, upstr, sizeof(upstr));
	return buf_printf(buf, bsz, pos,
		"</div>"  /* #content */
		"<div style='padding:8px 20px;border-top:1px solid var(--border);"
		"display:flex;align-items:center;"
		"font-size:11px;color:var(--text2)'>"
		"<span>tcmg <span style='color:var(--text1)'>" TCMG_VERSION "</span>"
		" &bull; built <span style='color:var(--text1)'>" TCMG_BUILD_TIME "</span></span>"
		"</div>"
		"</div>"  /* #main */
		"</body></html>");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  LOGIN PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_login_page(int fd, int failed)
{
	int bsz = 8192, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = buf_printf(&buf, &bsz, pos,
		"<!DOCTYPE html><html lang='en'><head>"
		"<meta charset='UTF-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>tcmg -- Login</title>"
		"<style>%s</style>"
		"</head><body style='background:var(--bg0)'>",
		CSS);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='login-bg'>"
		"<div class='login-card'>"
		"  <div class='login-logo'>"
		"    <div class='login-logo-icon'>tc</div>"
		"    <div>"
		"      <div style='font-size:20px;font-weight:700'>tcmg</div>"
		"      <div style='font-size:11px;color:var(--text2);font-family:var(--font-mono)'>"
		TCMG_VERSION
		"      </div>"
		"    </div>"
		"  </div>");

	if (failed)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='login-err'>"
			"<svg width='14' height='14' viewBox='0 0 20 20' fill='currentColor'>"
			"<path fill-rule='evenodd' d='M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z' clip-rule='evenodd'/></svg>"
			"Invalid credentials. Please try again."
			"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<form method='POST' action='/login'>"
		"<div class='form-group'>"
		"  <label class='form-label'>USERNAME</label>"
		"  <input class='form-input' type='text' name='u' placeholder='Username' autofocus autocomplete='username'>"
		"</div>"
		"<div class='form-group'>"
		"  <label class='form-label'>PASSWORD</label>"
		"  <input class='form-input' type='password' name='p' placeholder='Password' autocomplete='current-password'>"
		"</div>"
		"<button type='submit' class='btn btn-primary' style='width:100%%;justify-content:center;padding:10px'>"
		"Sign In</button>"
		"</form>"
		"</div></div>"
		"</body></html>");

	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  STATS helpers
 * ═══════════════════════════════════════════════════════════════════════════ */
static void handle_reset_stats(void)
{
	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	while (a) {
		a->ecm_total = 0; a->cw_found = 0; a->cw_not = 0;
		a->cw_time_total_ms = 0;
		a = a->next;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	tcmg_log("all user stats reset");
}

typedef struct { int64_t cw_found; int64_t cw_not; int nbans; } S_STATS;
static S_STATS aggregate_stats(void)
{
	S_STATS s = {0, 0, 0};
	time_t now = time(NULL);
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	for (const S_ACCOUNT *a = g_cfg.accounts; a; a = a->next)
		{ s.cw_found += a->cw_found; s.cw_not += a->cw_not; }
	pthread_rwlock_unlock(&g_cfg.acc_lock);
	pthread_mutex_lock(&g_cfg.ban_lock);
	for (const S_BAN_ENTRY *b = g_cfg.bans; b; b = b->next)
		if (now < b->until) s.nbans++;
	pthread_mutex_unlock(&g_cfg.ban_lock);
	return s;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  STATUS PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_status(int fd)
{
	int bsz = 32768, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Status", "status");

	time_t now = time(NULL);
	char upstr[32];
	format_uptime(now - g_start_time, upstr, sizeof(upstr));

	S_STATS st  = aggregate_stats();
	int64_t cw_found = st.cw_found, cw_not = st.cw_not;
	int nbans = st.nbans;
	int naccounts;
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	naccounts = g_cfg.naccounts;
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	int64_t ecm_total = cw_found + cw_not;
	double  hitrate   = ecm_total > 0 ? (double)cw_found * 100.0 / (double)ecm_total : 0.0;
	char    hrstr[16] = "—";
	if (ecm_total > 0) snprintf(hrstr, sizeof(hrstr), "%.1f%%", hitrate);

	/* ── Stat cards ── */
	pos = buf_printf(&buf, &bsz, pos, "<div class='cards-grid'>");

	/* Uptime */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card blue'>"
		"<div class='card-label'>Uptime</div>"
		"<div class='card-value blue sm' id='p_up'>%s</div>"
		"</div>", upstr);

	/* Connections */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card %s'>"
		"<div class='card-label'>Connections</div>"
		"<div class='card-value %s' id='p_conn'>%d</div>"
		"<div class='card-sub'>of %d accounts</div>"
		"</div>",
		g_active_conns > 0 ? "green" : "",
		g_active_conns > 0 ? "green" : "",
		g_active_conns, naccounts);

	/* ECM Total */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card'>"
		"<div class='card-label'>ECM Total</div>"
		"<div class='card-value' id='p_ecm'>%lld</div>"
		"<div class='card-sub'>requests processed</div>"
		"</div>", (long long)ecm_total);

	/* CW Found */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card green'>"
		"<div class='card-label'>CW Found</div>"
		"<div class='card-value green' id='p_hit'>%lld</div>"
		"<div class='card-sub'>cache hits</div>"
		"</div>", (long long)cw_found);

	/* CW Miss */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card %s'>"
		"<div class='card-label'>CW Miss</div>"
		"<div class='card-value %s' id='p_miss'>%lld</div>"
		"<div class='card-sub'>not found</div>"
		"</div>",
		cw_not > 0 ? "red" : "",
		cw_not > 0 ? "red" : "",
		(long long)cw_not);


	/* Bans */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card %s'>"
		"<div class='card-label'>Banned IPs</div>"
		"<div class='card-value %s' id='p_ban'>%d</div>"
		"<div class='card-sub'><a href='/failban' style='color:var(--text2);font-size:11px'>view all →</a></div>"
		"</div>",
		nbans > 0 ? "yellow" : "",
		nbans > 0 ? "yellow" : "",
		nbans);

	/* Accounts */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card'>"
		"<div class='card-label'>Accounts</div>"
		"<div class='card-value' id='p_acc'>%d</div>"
		"<div class='card-sub'><a href='/users' style='color:var(--text2);font-size:11px'>manage →</a></div>"
		"</div>", naccounts);

	pos = buf_printf(&buf, &bsz, pos, "</div>"); /* /cards-grid */

	/* ── Active connections table ── */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='section-hdr'>"
		"  <div class='section-title'>Active Connections</div>"
		"  <div class='flex gap-8'>"
		"    <a href='#' onclick=\"if(confirm('Reset all stats?')){fetch('/api/resetstats').then(()=>location.reload());}return false\" class='btn btn-ghost btn-sm'>↺ Reset Stats</a>"
		"    <a href='#' onclick=\"fetch('/api/reload');this.textContent='✓ Done';return false\" class='btn btn-ghost btn-sm'>⟳ Reload Config</a>"
		"  </div>"
		"</div>"
		"<div class='tbl-wrap'>"
		"<table>"
		"<thead><tr>"
		"<th>User</th><th>IP Address</th><th>CAID</th><th>SID</th>"
		"<th>Channel</th><th>Connected</th><th>Idle</th><th></th>"
		"</tr></thead>"
		"<tbody id='p_clients'>");

	pthread_mutex_lock(&g_clients_mtx);
	int shown = 0;
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
	{
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->account) continue;
		char conn_str[32], idle_str[32];
		format_uptime(now - cl->connect_time, conn_str, sizeof(conn_str));
		format_uptime(now - cl->account->last_seen, idle_str, sizeof(idle_str));
		pos = buf_printf(&buf, &bsz, pos,
			"<tr id='row_%u'>"
			"<td><span class='bold'>%s</span></td>"
			"<td class='mono'>%s</td>"
			"<td class='mono'><span class='badge badge-blue'>%04X</span></td>"
			"<td class='mono'>%04X</td>"
			"<td>%s</td>"
			"<td class='mono text-muted'>%s</td>"
			"<td class='mono text-muted'>%s</td>"
			"<td><button class='kill-btn' onclick=\"if(confirm('Disconnect %s?')){"
			"fetch('/status?kill=%u&user=%s');"
			"var r=document.getElementById('row_%u');if(r)r.remove();"
			"}\"  title='Disconnect'><svg width='13' height='13' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z' clip-rule='evenodd'/></svg></button></td>"
			"</tr>",
			cl->thread_id,
			cl->user, cl->ip,
			cl->last_caid, cl->last_srvid,
			cl->last_channel[0] ? cl->last_channel : "<span class='text-muted'>—</span>",
			conn_str, idle_str,
			cl->user, cl->thread_id, cl->user, cl->thread_id);
		shown++;
	}
	pthread_mutex_unlock(&g_clients_mtx);

	if (!shown)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='empty-row'><td colspan='8'>No active connections</td></tr>");

	pos = buf_printf(&buf, &bsz, pos, "</tbody></table></div>");



	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  USERS PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_users(int fd)
{
	int bsz = 65536, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Users", "users");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='section-hdr'>"
		"  <div class='section-title'>Account Management</div>"
		"</div>"
		"<div class='tbl-wrap'><table>"
		"<thead><tr>"
		"<th>Username</th><th>CAID</th><th>Status</th>"
		"<th>Active</th><th>Max</th>"
		"<th>CW Hit</th><th>CW Miss</th><th>Hit %%</th><th>Avg ms</th>"
		"<th>Hit Bar</th>"
		"<th>First Login</th><th>Last Seen</th><th>Expiry</th>"
		"</tr></thead><tbody>");

	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	S_ACCOUNT *a = g_cfg.accounts;
	int row = 0;
	while (a)
	{
		char last[32], first_l[32], expiry[128];
		format_time((time_t)a->last_seen,   last,    sizeof(last));
		format_time((time_t)a->first_login, first_l, sizeof(first_l));

		if (a->expirationdate > 0)
		{
			format_time(a->expirationdate, expiry, sizeof(expiry));
			if (time(NULL) > a->expirationdate)
				snprintf(expiry, sizeof(expiry), "<span class='badge badge-ban'>EXPIRED</span>");
		}
		else snprintf(expiry, sizeof(expiry), "<span class='text-muted'>—</span>");

		int64_t tot = a->cw_found + a->cw_not;
		double  hr  = tot > 0 ? (double)a->cw_found * 100.0 / (double)tot : -1.0;
		char    hrstr[16] = "—";
		if (hr >= 0) snprintf(hrstr, sizeof(hrstr), "%.1f%%", hr);

		char avgstr[16] = "—";
		if (a->cw_found > 0)
			snprintf(avgstr, sizeof(avgstr), "%lld",
			         (long long)(a->cw_time_total_ms / a->cw_found));

		const char *st_badge = a->enabled
			? "<span class='badge badge-on'>on</span>"
			: "<span class='badge badge-off'>off</span>";

		const char *miss_cls = a->cw_not > 0 ? "text-red" : "";

		pos = buf_printf(&buf, &bsz, pos,
			"<tr>"
			"<td><span class='bold'>%s</span></td>"
			"<td class='mono'><span class='badge badge-blue'>%04X</span></td>"
			"<td>%s</td>"
			"<td class='mono'>%d</td>"
			"<td class='mono text-muted'>%lld</td>"
			"<td class='mono text-green'>%lld</td>"
			"<td class='mono %s'>%lld</td>"
			"<td class='mono'>%s</td>"
			"<td class='mono text-muted'>%s</td>"
			"<td><div class='hitbar-wrap'><div class='hitbar-fill' style='width:%.0f%%'></div></div></td>"
			"<td class='mono text-muted' style='font-size:11px'>%s</td>"
			"<td class='mono text-muted' style='font-size:11px'>%s</td>"
			"<td style='font-size:12px'>%s</td>"
			"</tr>",
			a->user,
			a->caid,
			st_badge,
			(int)a->active,
			(long long)a->max_connections,
			(long long)a->cw_found,
			miss_cls, (long long)a->cw_not,
			hrstr, avgstr,
			hr >= 0 ? hr : 0.0,
			first_l, last, expiry);
		a = a->next;
		row++;
	}
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	if (!row)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='empty-row'><td colspan='13'>No accounts configured</td></tr>");

	pos = buf_printf(&buf, &bsz, pos, "</tbody></table></div>");
	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  FAIL-BAN PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_failban(int fd, const char *qs)
{
	char action[32], clearip[MAXIPLEN];
	get_param(qs, "action", action, sizeof(action));
	get_param(qs, "ip",     clearip, sizeof(clearip));

	if (strcmp(action, "clear") == 0 && clearip[0])
	{
		pthread_mutex_lock(&g_cfg.ban_lock);
		S_BAN_ENTRY *b = g_cfg.bans;
		while (b) { if (strcmp(b->ip, clearip) == 0) b->until = 0; b = b->next; }
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("cleared ban for %s", clearip);
	}
	else if (strcmp(action, "clearall") == 0)
	{
		pthread_mutex_lock(&g_cfg.ban_lock);
		S_BAN_ENTRY *b = g_cfg.bans;
		while (b) { b->until = 0; b = b->next; }
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("cleared all bans");
	}

	int bsz = 16384, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Fail-Ban", "failban");

	/* Count active bans */
	int total_bans = 0;
	time_t now = time(NULL);
	pthread_mutex_lock(&g_cfg.ban_lock);
	for (S_BAN_ENTRY *b = g_cfg.bans; b; b = b->next)
		if (b->until > now) total_bans++;
	pthread_mutex_unlock(&g_cfg.ban_lock);

	{
		char ban_badge[64] = "";
		if (total_bans > 0)
			snprintf(ban_badge, sizeof(ban_badge),
			         "<span class='badge badge-ban' style='margin-left:8px'>%d active</span>",
			         total_bans);
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='section-hdr'>"
			"  <div class='section-title'>Fail-Ban Manager %s</div>"
			"  %s"
			"</div>",
			ban_badge,
			total_bans > 0
				? "<a href='/failban?action=clearall' class='btn btn-danger btn-sm'>🗑 Clear All</a>"
				: "");
	}

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='tbl-wrap'><table>"
		"<thead><tr>"
		"<th>IP Address</th><th>Fail Count</th>"
		"<th>Expires At</th><th>Remaining</th><th>Action</th>"
		"</tr></thead><tbody>");

	int shown = 0;
	pthread_mutex_lock(&g_cfg.ban_lock);
	S_BAN_ENTRY *b = g_cfg.bans;
	while (b)
	{
		if (b->until > now)
		{
			char exp[32];
			struct tm tm_s;
			localtime_r(&b->until, &tm_s);
			strftime(exp, sizeof(exp), "%H:%M:%S", &tm_s);
			long rem = (long)(b->until - now);
			pos = buf_printf(&buf, &bsz, pos,
				"<tr>"
				"<td class='mono bold'>%s</td>"
				"<td><span class='badge badge-ban'>%d fails</span></td>"
				"<td class='mono text-muted'>%s</td>"
				"<td class='mono text-yellow'>%lds</td>"
				"<td><a href='/failban?action=clear&ip=%s' class='btn btn-ghost btn-sm'>Unban</a></td>"
				"</tr>",
				b->ip, b->fails, exp, rem, b->ip);
			shown++;
		}
		b = b->next;
	}
	pthread_mutex_unlock(&g_cfg.ban_lock);

	if (!shown)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='empty-row'><td colspan='5'>"
			"<span class='text-green'>✓</span> No active bans"
			"</td></tr>");

	pos = buf_printf(&buf, &bsz, pos, "</tbody></table></div>");
	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  CONFIG PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_config(int fd)
{
	int bsz = 65536, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Config", "config");

	char cfgpath[CFGPATH_LEN + 16];
	snprintf(cfgpath, sizeof(cfgpath), "%s/" TCMG_CFG_FILE, g_cfgdir);

	FILE *fp = fopen(cfgpath, "r");
	char  filebuf[16384] = "";
	int   filelen = 0;
	int   truncated = 0;
	if (fp) {
		filelen = (int)fread(filebuf, 1, sizeof(filebuf) - 1, fp);
		if (filelen < 0) filelen = 0;
		filebuf[filelen] = '\0';
		if (!feof(fp)) truncated = 1;
		fclose(fp);
	}

	/* HTML-escape */
	char *escaped = (char *)malloc(filelen * 6 + 64);
	if (!escaped) { free(buf); return; }
	int ei = 0;
	for (int i = 0; filebuf[i]; i++)
	{
		if      (filebuf[i] == '<') { memcpy(escaped+ei, "&lt;",  4); ei+=4; }
		else if (filebuf[i] == '>') { memcpy(escaped+ei, "&gt;",  4); ei+=4; }
		else if (filebuf[i] == '&') { memcpy(escaped+ei, "&amp;", 5); ei+=5; }
		else                         escaped[ei++] = filebuf[i];
	}
	escaped[ei] = '\0';

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='section-hdr'>"
		"  <div class='section-title'>tcmg.conf</div>"
		"  <span class='text-muted' style='font-size:11px;font-family:var(--font-mono)'>%s</span>"
		"</div>"
		"%s"
		"<div class='info-box'>"
		"Edit and save to apply changes. A backup is created as <span class='mono'>tcmg.conf.bak</span> automatically."
		"</div>"
		"<form method='post' action='/config_save'>"
		"<textarea class='cfg-editor' name='cfg' spellcheck='false'>%s</textarea>"
		"<div class='flex gap-8 mb-20' style='margin-top:12px'>"
		"<button type='submit' class='btn btn-primary'>💾 Save &amp; Reload</button>"
		"</div>"
		"</form>",
		cfgpath,
		truncated ? "<div class='info-box' style='color:var(--yellow2);border-color:rgba(245,158,11,.3)'>"
		            "⚠ Config file exceeds 16 KB -- displayed content is truncated. Edit the file directly."
		            "</div>" : "",
		escaped);
	free(escaped);

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

static void handle_config_save(int fd, const char *post_body)
{
	char newcfg[16384] = "";
	form_get(post_body, "cfg", newcfg, sizeof(newcfg));
	if (!newcfg[0]) {
		const char *e = "<html><body><h1>Empty config rejected</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}
	char cfgpath[CFGPATH_LEN + 16];
	snprintf(cfgpath, sizeof(cfgpath), "%s/" TCMG_CFG_FILE, g_cfgdir);
	char tmppath[CFGPATH_LEN + 20];
	snprintf(tmppath, sizeof(tmppath), "%s/" TCMG_CFG_FILE ".tmp", g_cfgdir);
	FILE *fp = fopen(tmppath, "w");
	if (!fp) {
		const char *e = "<html><body><h1>Cannot write temp file</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	fputs(newcfg, fp); fclose(fp);
	S_CONFIG parsed;
	memset(&parsed, 0, sizeof(parsed));
	pthread_rwlock_init(&parsed.acc_lock, NULL);
	pthread_mutex_init(&parsed.ban_lock, NULL);
	if (!cfg_load(tmppath, &parsed)) {
		remove(tmppath); cfg_accounts_free(&parsed);
		const char *e = "<html><body><h1>Config parse error -- not saved</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}
	remove(tmppath);
	strncpy(parsed.config_file, cfgpath, CFGPATH_LEN - 1);
	/* Backup */
	char bakpath[CFGPATH_LEN + 20];
	snprintf(bakpath, sizeof(bakpath), "%s/" TCMG_CFG_FILE ".bak", g_cfgdir);
	FILE *src = fopen(cfgpath, "r");
	if (src) {
		FILE *dst = fopen(bakpath, "w");
		if (dst) {
			char tmp[512]; int n;
			while ((n = (int)fread(tmp, 1, sizeof(tmp), src)) > 0)
				fwrite(tmp, 1, n, dst);
			fclose(dst);
		}
		fclose(src);
	}
	if (!cfg_save(&parsed)) {
		cfg_accounts_free(&parsed);
		const char *e = "<html><body><h1>Cannot write config</h1></body></html>";
		send_response(fd, 500, "Internal Error", "text/html", e, (int)strlen(e));
		return;
	}
	cfg_accounts_free(&parsed);
	tcmg_log("config saved successfully, reloading...");
	g_reload_cfg = 1;
	send_redirect(fd, "/config");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  LIVE LOG PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_livelog(int fd)
{
	int bsz = 32768, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Live Log", "livelog");

	/* Debug toggles */
	pos = buf_printf(&buf, &bsz, pos, "<div class='dbg-bar'>"
		"<span style='font-size:11px;font-weight:600;color:var(--text2);"
		"text-transform:uppercase;letter-spacing:1px;margin-right:6px'>Debug</span>");

	char masks_arr[256];
	int  ma = 0;
	for (int i = 0; i < MAX_DEBUG_LEVELS; i++)
	{
		uint16_t m   = g_dblevel_names[i].mask;
		int      on  = !!(g_dblevel & m);
		pos = buf_printf(&buf, &bsz, pos,
			"<a id='db%u' href='#' class='dbg-tag%s' onclick='toggleDbg(%u);return false;'"
			" title='%u'>%s</a>",
			m, on ? " on" : "", m, m, g_dblevel_names[i].name);
		ma += snprintf(masks_arr + ma, sizeof(masks_arr) - ma,
		               "%s%u", i ? "," : "", m);
	}
	int all_on = (g_dblevel == D_ALL);
	pos = buf_printf(&buf, &bsz, pos,
		"<a id='dbALL' href='#' class='dbg-tag%s' onclick='toggleAll();return false;'>ALL</a>"
		"<span class='dbg-mask'>mask: <span id='dbmask'>%u</span></span>"
		"</div>",
		all_on ? " on" : "", g_dblevel);

	/* Controls */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='log-ctrl'>"
		"<button class='btn btn-ghost btn-sm' onclick='clearLog()'>✕ Clear</button>"
		"<input class='log-search' id='filter' placeholder='Filter...' oninput='applyFilter()'>"
		"<label class='flex gap-8' style='font-size:12px'>"
		"<input type='checkbox' id='asc' checked> Auto-scroll"
		"</label>"
		"<label class='flex gap-8' style='font-size:12px'>"
		"<input type='checkbox' id='paused'> Pause"
		"</label>"
		"<span style='margin-left:auto;font-size:11px;color:var(--text2)'>Lines:"
		"<select class='log-sel' id='maxlines'>"
		"<option value='200' selected>200</option>"
		"<option value='500'>500</option>"
		"<option value='1000'>1000</option>"
		"</select></span>"
		"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div id='logwrap' onmouseenter='hovered=1' onmouseleave='hovered=0'>"
		"<pre id='logpre'></pre>"
		"</div>");

	int ring_now = log_ring_total();

	pos = buf_printf(&buf, &bsz, pos,
		"<script>"
		"var lastid=Math.max(0,%d-200);"
		"var curmask=%u;"
		"var hovered=0;"
		"var masks=[%s];"
		"var filterStr='';"

		"function updateDbgUI(){"
		"  document.getElementById('dbmask').textContent="
		"    '0x'+curmask.toString(16).toUpperCase().padStart(4,'0');"
		"  masks.forEach(function(m){"
		"    var el=document.getElementById('db'+m);"
		"    if(!el)return;"
		"    el.className='dbg-tag'+(curmask&m?' on':'');"
		"  });"
		"  var a=document.getElementById('dbALL');"
		"  a.className='dbg-tag'+(curmask===65535?' on':'');"
		"}"
		"function toggleDbg(m){curmask^=m;updateDbgUI();poll();}"
		"function toggleAll(){curmask=(curmask===65535)?0:65535;updateDbgUI();poll();}"

		"function applyFilter(){"
		"  filterStr=document.getElementById('filter').value.toLowerCase();"
		"  var spans=document.getElementById('logpre').children;"
		"  for(var i=0;i<spans.length;i++){"
		"    spans[i].style.display=(!filterStr||spans[i].textContent.toLowerCase().includes(filterStr))?'':'none';"
		"  }"
		"}"

		"function clearLog(){"
		"  document.getElementById('logpre').innerHTML='';"
		"  fetch('/logpoll?since=999999999&debug='+curmask)"
		"    .then(r=>r.json())"
		"    .then(d=>{if(d.next!==undefined)lastid=d.next;});"
		"}"

		"var COLOR_MAP={"
		"  'cw':{hit:'#4ade80',miss:'#f87171'},"
		"  'webif':'#60a5fa','ban':'#fbbf24',"
		"  'net':'#c084fc','proto':'#22d3ee',"
		"  'emu':'#86efac','conf':'#fde68a',"
		"  'error':'#f87171','warn':'#fbbf24'"
		"};"
		"function colorLine(l){"
		"  var ll=l.toLowerCase();"
		"  if(ll.includes('[hit]'))  return COLOR_MAP.cw.hit;"
		"  if(ll.includes('[miss]')) return COLOR_MAP.cw.miss;"
		"  if(ll.includes('(webif')) return COLOR_MAP.webif;"
		"  if(ll.includes('(ban'))   return COLOR_MAP.ban;"
		"  if(ll.includes('(net'))   return COLOR_MAP.net;"
		"  if(ll.includes('(proto')) return COLOR_MAP.proto;"
		"  if(ll.includes('(emu'))   return COLOR_MAP.emu;"
		"  if(ll.includes('(conf'))  return COLOR_MAP.conf;"
		"  if(ll.includes('error'))  return COLOR_MAP.error;"
		"  if(ll.includes('warn'))   return '#fbbf24';"
		"  return null;"
		"}"

		"function appendLines(lines){"
		"  var pre=document.getElementById('logpre');"
		"  var maxl=parseInt(document.getElementById('maxlines').value)||200;"
		"  lines.forEach(function(line){"
		"    var span=document.createElement('span');"
		"    var c=colorLine(line);"
		"    if(c)span.style.color=c;"
		"    span.textContent=line+'\\n';"
		"    if(filterStr&&!line.toLowerCase().includes(filterStr))"
		"      span.style.display='none';"
		"    pre.appendChild(span);"
		"  });"
		"  while(pre.children.length>maxl)"
		"    pre.removeChild(pre.firstChild);"
		"  var w=document.getElementById('logwrap');"
		"  if(!hovered&&document.getElementById('asc').checked)"
		"    w.scrollTop=w.scrollHeight;"
		"}"

		"function poll(){"
		"  if(document.getElementById('paused').checked)return;"
		"  fetch('/logpoll?since='+lastid+'&debug='+curmask)"
		"    .then(r=>r.json())"
		"    .then(d=>{"
		"      if(d.debug!==undefined&&d.debug!==curmask){curmask=d.debug;updateDbgUI();}"
		"      if(d.next!==undefined)lastid=d.next;"
		"      if(d.lines&&d.lines.length)appendLines(d.lines);"
		"    })"
		"    .catch(()=>{});"
		"}"
		"updateDbgUI();"
		"setInterval(poll,1000);poll();"
		"</script>",
		ring_now, g_dblevel, masks_arr);

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  LOG POLL API
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_logpoll(int fd, const char *qs)
{
	char dbg_s[16], since_s[32];
	get_param(qs, "debug", dbg_s,  sizeof(dbg_s));
	get_param(qs, "since", since_s, sizeof(since_s));

	if (dbg_s[0])
	{
		long v = strtol(dbg_s, NULL, 0);
		if (v >= 0 && v <= 0xFFFF && (uint16_t)v != g_dblevel)
		{
			g_dblevel = (uint16_t)v;
			tcmg_log_dbg(D_WEBIF, "livelog debug_level → %u", g_dblevel);
		}
	}

	int32_t from_id = since_s[0] ? (int32_t)atoi(since_s) : 0;
	if (from_id < 0) from_id = 0;

	char *lines[WEB_MAX_LINES_POLL];
	int32_t next_id;
	int32_t count = log_ring_since(from_id, lines, WEB_MAX_LINES_POLL, &next_id);

	int bsz = count * 256 + 256, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = buf_printf(&buf, &bsz, pos,
		"{\"debug\":%u,\"next\":%d,\"lines\":[",
		g_dblevel, next_id);

	for (int i = 0; i < count; i++)
	{
		char escaped[512];
		int  ei = 0;
		for (const char *p = lines[i]; *p && ei < (int)sizeof(escaped) - 4; p++)
		{
			if (*p == '"'  || *p == '\\') escaped[ei++] = '\\';
			if (*p == '\n' || *p == '\r') continue;
			escaped[ei++] = *p;
		}
		escaped[ei] = '\0';
		free(lines[i]);
		pos = buf_printf(&buf, &bsz, pos, "%s\"%s\"", i ? "," : "", escaped);
	}
	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  API /status  (JSON)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_api_status(int fd)
{
	int bsz = 16384, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	time_t now = time(NULL);
	char upstr[32];
	format_uptime(now - g_start_time, upstr, sizeof(upstr));

	S_STATS st = aggregate_stats();
	int64_t cw_found = st.cw_found, cw_not = st.cw_not;
	int nbans = st.nbans;
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	int naccounts = g_cfg.naccounts;
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	int64_t ecm_total = cw_found + cw_not;
	double  hitrate   = ecm_total > 0 ? (double)cw_found * 100.0 / (double)ecm_total : 0.0;

	pos = buf_printf(&buf, &bsz, pos,
		"{"
		"\"version\":\"%s\","
		"\"build\":\"%s\","
		"\"uptime_s\":%ld,"
		"\"uptime_str\":\"%s\","
		"\"port\":%d,"
		"\"active_connections\":%d,"
		"\"accounts\":%d,"
		"\"banned_ips\":%d,"
		"\"cw_found\":%lld,"
		"\"cw_not\":%lld,"
		"\"ecm_total\":%lld,"
		"\"hit_rate_pct\":%.1f,"
		"\"debug_mask\":%u,"
		"\"clients\":[",
		TCMG_VERSION,
		TCMG_BUILD_TIME,
		(long)(now - g_start_time),
		upstr,
		g_cfg.port,
		g_active_conns,
		naccounts,
		nbans,
		(long long)cw_found,
		(long long)cw_not,
		(long long)ecm_total,
		hitrate,
		g_dblevel);

	pthread_mutex_lock(&g_clients_mtx);
	bool first = true;
	for (int i = 0; i < MAX_ACTIVE_CLIENTS; i++)
	{
		S_CLIENT *cl = g_clients[i];
		if (!cl || !cl->account) continue;
		char conn_str[32], idle_str[32];
		format_uptime(now - cl->connect_time, conn_str, sizeof(conn_str));
		format_uptime(now - cl->account->last_seen, idle_str, sizeof(idle_str));
		pos = buf_printf(&buf, &bsz, pos,
			"%s{"
			"\"user\":\"%s\","
			"\"ip\":\"%s\","
			"\"caid\":\"%04X\","
			"\"sid\":\"%04X\","
			"\"channel\":\"%s\","
			"\"connected\":\"%s\","
			"\"idle\":\"%s\","
			"\"thread_id\":%u"
			"}",
			first ? "" : ",",
			cl->user, cl->ip,
			cl->last_caid, cl->last_srvid,
			cl->last_channel[0] ? cl->last_channel : "",
			conn_str, idle_str,
			cl->thread_id);
		first = false;
	}
	pthread_mutex_unlock(&g_clients_mtx);

	pos = buf_printf(&buf, &bsz, pos, "]}");
	send_response(fd, 200, "OK", "application/json", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SHUTDOWN PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_shutdown(int fd, const char *qs)
{
	char confirm[8];
	get_param(qs, "confirm", confirm, sizeof(confirm));

	int bsz = 8192, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Shutdown", "shutdown");

	if (strcmp(confirm, "yes") == 0)
	{
		tcmg_log("shutdown requested via webif");
		g_running = 0;
		pos = buf_printf(&buf, &bsz, pos,
			"<div style='background:rgba(239,68,68,.1);border:1px solid rgba(239,68,68,.3);"
			"border-radius:10px;padding:28px 32px;text-align:center;max-width:380px'>"
			"<div style='font-size:28px;margin-bottom:10px'>⏹</div>"
			"<div style='font-size:15px;font-weight:600;color:var(--red2)'>Shutdown Initiated</div>"
			"<div style='color:var(--text2);margin-top:6px;font-size:13px'>Server is stopping.</div>"
			"</div>");
	}
	else
	{
		pos = buf_printf(&buf, &bsz, pos,
			"<div style='background:var(--bg2);border:1px solid var(--border);"
			"border-radius:10px;padding:28px 32px;max-width:360px'>"
			"<div style='font-size:16px;font-weight:600;margin-bottom:10px'>⚠ Shutdown tcmg?</div>"
			"<div style='color:var(--text2);margin-bottom:20px;font-size:13px'>"
			"All active connections will be dropped.</div>"
			"<div class='flex gap-8'>"
			"<a href='/shutdown?confirm=yes' class='btn btn-danger'><svg width='13' height='13' viewBox='0 0 20 20' fill='currentColor' style='margin-right:5px;vertical-align:-2px'><path fill-rule='evenodd' d='M10 2a1 1 0 011 1v6a1 1 0 11-2 0V3a1 1 0 011-1zm3.293 2.293a1 1 0 011.414 1.414 7 7 0 11-9.414 0 1 1 0 011.414-1.414 5 5 0 106.586 0z' clip-rule='evenodd'/></svg>Confirm Shutdown</a>"
			"<a href='/status' class='btn btn-ghost'>Cancel</a>"
			"</div>"
			"</div>");
	}

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RESTART PAGE
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_restart(int fd, const char *qs)
{
	char confirm[8];
	get_param(qs, "confirm", confirm, sizeof(confirm));

	int bsz = 8192, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Restart", "restart");

	if (strcmp(confirm, "yes") == 0)
	{
		tcmg_log("restart requested via webif");
		g_restart = 1;
		g_running = 0;
		pos = buf_printf(&buf, &bsz, pos,
			"<div style='background:rgba(59,130,246,.1);border:1px solid rgba(59,130,246,.3);"
			"border-radius:10px;padding:28px 32px;text-align:center;max-width:380px'>"
			"<div style='font-size:28px;margin-bottom:10px'>🔄</div>"
			"<div style='font-size:15px;font-weight:600;color:var(--accent2)'>Restart Initiated</div>"
			"<div style='color:var(--text2);margin-top:6px;font-size:13px;margin-bottom:16px'>"
			"Redirecting when back online...</div>"
			"<script>"
			"setTimeout(function(){"
			"  var t=setInterval(function(){"
			"    fetch('/api/status',{cache:'no-store'})"
			"      .then(function(){clearInterval(t);location.href='/status';})"
			"      .catch(function(){});"
			"  },1500);"
			"},3000);"
			"</script>"
			"<div class='status-pill' style='display:inline-flex'>"
			"<div class='pulse-dot pulse-sm'></div>Waiting...</div>"
			"</div>");
	}
	else
	{
		pos = buf_printf(&buf, &bsz, pos,
			"<div style='background:var(--bg2);border:1px solid var(--border);"
			"border-radius:10px;padding:28px 32px;max-width:360px'>"
			"<div style='font-size:16px;font-weight:600;margin-bottom:10px'>🔄 Restart tcmg?</div>"
			"<div style='color:var(--text2);margin-bottom:20px;font-size:13px'>"
			"Active connections will be dropped. Config reloaded on startup.</div>"
			"<div class='flex gap-8'>"
			"<a href='/restart?confirm=yes' class='btn btn-primary'>🔄 Confirm Restart</a>"
			"<a href='/status' class='btn btn-ghost'>Cancel</a>"
			"</div>"
			"</div>");
	}

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TVCAS TOOL PAGE  (pure-JS DES/3DES + TVCAS key transform, no ext libs)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void send_page_tvcas(int fd)
{
	int bsz = 32768, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "TVCAS Tool", "tvcas");

	/* ── Page styles ── */
	pos = buf_printf(&buf, &bsz, pos,
		"<style>"
		".tv-tabs{display:flex;gap:4px;margin-bottom:16px;"
		"border-bottom:1px solid var(--border);padding-bottom:0}"
		".tv-tab{padding:8px 22px;cursor:pointer;font-size:13px;font-weight:500;"
		"color:var(--text2);border-bottom:2px solid transparent;margin-bottom:-1px;"
		"transition:all .15s;background:none;border-top:none;border-left:none;border-right:none}"
		".tv-tab.active{color:var(--accent2);border-bottom-color:var(--accent)}"
		".tv-tab:hover{color:var(--text0)}"
		".tv-panel{display:none}.tv-panel.active{display:block}"
		".tv-card{background:var(--bg2);border:1px solid var(--border);"
		"border-radius:8px;padding:16px;margin-bottom:12px}"
		".tv-lbl{font-size:12px;color:var(--text2);margin-bottom:5px;"
		"margin-top:10px;font-weight:500}"
		".tv-lbl:first-child{margin-top:0}"
		".tv-inp{width:100%%;background:var(--bg3);border:1px solid var(--border2);"
		"border-radius:5px;color:var(--text0);font-family:var(--font-mono);"
		"font-size:12px;padding:7px 10px;outline:none;transition:border-color .15s}"
		".tv-inp:focus{border-color:var(--accent)}"
		"textarea.tv-inp{resize:vertical;min-height:50px}"
		".tv-btn{background:var(--accent);border:none;border-radius:5px;color:#fff;"
		"font-size:13px;font-weight:600;padding:8px 22px;cursor:pointer;"
		"margin-top:10px;transition:background .15s}"
		".tv-btn:hover{background:var(--accent2)}"
		".tv-res{background:var(--bg2);border:1px solid var(--border);"
		"border-radius:8px;overflow:hidden;min-height:44px}"
		".tv-res-empty{padding:14px;font-size:12px;color:var(--text2);"
		"font-family:var(--font-mono)}"
		".tv-tbl{width:100%%;border-collapse:collapse}"
		".tv-tbl tr{border-bottom:1px solid var(--border)}"
		".tv-tbl tr:last-child{border-bottom:none}"
		".tv-tbl td{padding:9px 14px;vertical-align:middle;font-family:var(--font-mono);font-size:12px}"
		".tv-tbl td.tk{color:var(--text2);font-size:11px;font-weight:600;"
		"text-transform:uppercase;letter-spacing:.4px;white-space:nowrap;"
		"width:120px;border-right:1px solid var(--border);background:var(--bg3)}"
		".tv-tbl td.tv{color:var(--text0);padding-left:16px;word-break:break-all}"
		".tv-tbl tr.tv-sh>td{background:var(--bg4);color:var(--accent);font-size:10px;"
		"font-weight:700;letter-spacing:1.5px;text-transform:uppercase;"
		"padding:6px 14px;border-right:none}"
		/* split layout */
		".tv-split{display:flex;gap:0;border-bottom:1px solid var(--border)}"
		".tv-split-box{flex:1;border-right:1px solid var(--border)}"
		".tv-split-box:last-child{border-right:none}"
		".tv-split-hdr{font-size:10px;font-weight:700;letter-spacing:1.5px;"
		"text-transform:uppercase;color:var(--accent);background:var(--bg4);"
		"padding:6px 14px;border-bottom:1px solid var(--border)}"
		".tv-cw-val{font-family:var(--font-mono);font-size:12px;font-weight:600;"
		"color:var(--cyan);word-break:break-all;letter-spacing:.3px}"
		".tv-ts{color:var(--text0);white-space:nowrap}"
		".tv-rgrp{display:flex;align-items:center;gap:12px;margin-bottom:10px}"
		".tv-rgrp label{display:flex;align-items:center;gap:5px;"
		"cursor:pointer;font-size:13px;color:var(--text1)}"
		".tv-ok{color:var(--green2)}.tv-er{color:var(--red2)}"
		".tv-hi{color:var(--cyan)}.tv-dim{color:var(--text2)}"
		"</style>");

	/* ── Tabs ── */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='tv-tabs'>"
		"<button class='tv-tab active' onclick='tvTab(0)'>&#128275; ECM Decryptor</button>"
		"<button class='tv-tab' onclick='tvTab(1)'>&#128260; Key Converter</button>"
		"</div>");

	/* ── Panel 0: ECM Decryptor ── */
	pos = buf_printf(&buf, &bsz, pos,
		"<div id='tvp0' class='tv-panel active'>"
		"<div class='tv-card'>"
		"<div class='tv-rgrp'>"
		"<span style='font-size:12px;color:var(--text2);font-weight:500'>Version:</span>"
		"<label><input type='radio' name='ecm_v' value='3' onchange='tvVC()'>TVCAS3</label>"
		"<label><input type='radio' name='ecm_v' value='4' onchange='tvVC()' checked>TVCAS4</label>"
		"</div>"
		"<div class='tv-lbl'>ECM (110 hex chars -- header 80 or 81 selects the key):</div>"
		"<textarea id='ecm_in' class='tv-inp' rows='2'"
		" placeholder='80... or 81... (110 hex chars)'></textarea>"
		"<div id='k3r' style='display:none'>"
		"<div class='tv-lbl'>TVCAS3 Key (32 or 64 hex chars):</div>"
		"<input id='k3in' class='tv-inp' type='text' placeholder='TVCAS3 key hex...'>"
		"</div>"
		"<div id='k4r'>"
		"<div class='tv-lbl'>TVCAS4 Key (64 hex chars):</div>"
		"<input id='k4in' class='tv-inp' type='text' placeholder='TVCAS4 key hex...'>"
		"</div>"
		"<button class='tv-btn' onclick='tvDec()'>Decrypt ECM</button>"
		"</div>"
		"<div class='tv-card'>"
		"<div class='tv-lbl'>Result</div>"
		"<div id='ecm_res' class='tv-res'>"
		"<div class='tv-res-empty'>—</div>"
		"</div>"
		"</div>"
		"</div>");

	/* ── Panel 1: Key Converter ── */
	pos = buf_printf(&buf, &bsz, pos,
		"<div id='tvp1' class='tv-panel'>"
		"<div class='tv-card'>"
		"<div class='tv-rgrp'>"
		"<span style='font-size:12px;color:var(--text2);font-weight:500'>Direction:</span>"
		"<label><input type='radio' name='cv_d' value='3to4'"
		" onchange='tvDC()' checked>3 &#8594; 4</label>"
		"<label><input type='radio' name='cv_d' value='4to3'"
		" onchange='tvDC()'>4 &#8594; 3</label>"
		"</div>"
		"<div id='cv_il' class='tv-lbl'>TVCAS3 Key (32 or 64 hex chars):</div>"
		"<input id='cv_in' class='tv-inp' type='text' placeholder='Key hex...'>"
		"<button class='tv-btn' onclick='tvConv()'>Convert</button>"
		"</div>"
		"<div class='tv-card'>"
		"<div id='cv_ol' class='tv-lbl'>TVCAS4 Key</div>"
		"<div id='cv_res' class='tv-res'>"
		"<div class='tv-res-empty'>—</div>"
		"</div>"
		"</div>"
		"</div>");

	/* ── JavaScript ── */
	pos = buf_printf(&buf, &bsz, pos, "<script>\n");

	/* CRYPT_TABLE */
	pos = buf_printf(&buf, &bsz, pos,
		"const CT=new Uint8Array(["
		"0xDA,0x26,0xE8,0x72,0x11,0x52,0x3E,0x46,0x32,0xFF,0x8C,0x1E,0xA7,0xBE,0x2C,0x29,"
		"0x5F,0x86,0x7E,0x75,0x0A,0x08,0xA5,0x21,0x61,0xFB,0x7A,0x58,0x60,0xF7,0x81,0x4F,"
		"0xE4,0xFC,0xDF,0xB1,0xBB,0x6A,0x02,0xB3,0x0B,0x6E,0x5D,0x5C,0xD5,0xCF,0xCA,0x2A,"
		"0x14,0xB7,0x90,0xF3,0xD9,0x37,0x3A,0x59,0x44,0x69,0xC9,0x78,0x30,0x16,0x39,0x9A,"
		"0x0D,0x05,0x1F,0x8B,0x5E,0xEE,0x1B,0xC4,0x76,0x43,0xBD,0xEB,0x42,0xEF,0xF9,0xD0,"
		"0x4D,0xE3,0xF4,0x57,0x56,0xA3,0x0F,0xA6,0x50,0xFD,0xDE,0xD2,0x80,0x4C,0xD3,0xCB,"
		"0xF8,0x49,0x8F,0x22,0x71,0x84,0x33,0xE0,0x47,0xC2,0x93,0xBC,0x7C,0x3B,0x9C,0x7D,"
		"0xEC,0xC3,0xF1,0x89,0xCE,0x98,0xA2,0xE1,0xC1,0xF2,0x27,0x12,0x01,0xEA,0xE5,0x9B,"
		"0x25,0x87,0x96,0x7B,0x34,0x45,0xAD,0xD1,0xB5,0xDB,0x83,0x55,0xB0,0x9E,0x19,0xD7,"
		"0x17,0xC6,0x35,0xD8,0xF0,0xAE,0xD4,0x2B,0x1D,0xA0,0x99,0x8A,0x15,0x00,0xAF,0x2D,"
		"0x09,0xA8,0xF5,0x6C,0xA1,0x63,0x67,0x51,0x3C,0xB2,0xC0,0xED,0x94,0x03,0x6F,0xBA,"
		"0x3F,0x4E,0x62,0x92,0x85,0xDD,0xAB,0xFE,0x10,0x2E,0x68,0x65,0xE7,0x04,0xF6,0x0C,"
		"0x20,0x1C,0xA9,0x53,0x40,0x77,0x2F,0xA4,0xFA,0x6D,0x73,0x28,0xE2,0xCD,0x79,0xC8,"
		"0x97,0x66,0x8E,0x82,0x74,0x06,0xC7,0x88,0x1A,0x4A,0x6B,0xCC,0x41,0xE9,0x9D,0xB8,"
		"0x23,0x9F,0x3D,0xBF,0x8D,0x95,0xC5,0x13,0xB9,0x24,0x5A,0xDC,0x64,0x18,0x38,0x91,"
		"0x7F,0x5B,0x70,0x54,0x07,0xB6,0x4B,0x0E,0x36,0xAC,0x31,0xE6,0xD6,0x48,0xAA,0xB4]);\n");

	/* DES implementation (BigInt-based, portable) */
	pos = buf_printf(&buf, &bsz, pos,
		"const DES=(()=>{\n"
		"const IP=[58,50,42,34,26,18,10,2,60,52,44,36,28,20,12,4,62,54,46,38,30,22,14,6,"
		"64,56,48,40,32,24,16,8,57,49,41,33,25,17,9,1,59,51,43,35,27,19,11,3,"
		"61,53,45,37,29,21,13,5,63,55,47,39,31,23,15,7];\n"
		"const FP=[40,8,48,16,56,24,64,32,39,7,47,15,55,23,63,31,38,6,46,14,54,22,62,30,"
		"37,5,45,13,53,21,61,29,36,4,44,12,52,20,60,28,35,3,43,11,51,19,59,27,"
		"34,2,42,10,50,18,58,26,33,1,41,9,49,17,57,25];\n"
		"const EE=[32,1,2,3,4,5,4,5,6,7,8,9,8,9,10,11,12,13,12,13,14,15,16,17,"
		"16,17,18,19,20,21,20,21,22,23,24,25,24,25,26,27,28,29,28,29,30,31,32,1];\n"
		"const PP=[16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,"
		"2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25];\n"
		"const PC1=[57,49,41,33,25,17,9,1,58,50,42,34,26,18,10,2,59,51,43,35,27,19,11,3,"
		"60,52,44,36,63,55,47,39,31,23,15,7,62,54,46,38,30,22,14,6,"
		"61,53,45,37,29,21,13,5,28,20,12,4];\n"
		"const PC2=[14,17,11,24,1,5,3,28,15,6,21,10,23,19,12,4,26,8,16,7,27,20,13,2,"
		"41,52,31,37,47,55,30,40,51,45,33,48,44,49,39,56,34,53,46,42,50,36,29,32];\n"
		"const SH=[1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1];\n");

	pos = buf_printf(&buf, &bsz, pos,
		"const SB=["
		"[14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7,0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8,"
		"4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0,15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13],"
		"[15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10,3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5,"
		"0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15,13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9],"
		"[10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8,13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1,"
		"13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7,1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12],"
		"[7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15,13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9,"
		"10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4,3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14],"
		"[2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9,14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6,"
		"4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14,11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3],"
		"[12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11,10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8,"
		"9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6,4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13],"
		"[4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1,13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6,"
		"1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2,6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12],"
		"[13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7,1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2,"
		"7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8,2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11]];\n");

	/* DES permute, subkeys, F-function, block */
	pos = buf_printf(&buf, &bsz, pos,
		"function prm(v,t,n){"
		"let o=0n;const l=t.length;"
		"for(let i=0;i<l;i++){const b=(v>>BigInt(n-t[i]))&1n;"
		"if(b)o|=(1n<<BigInt(l-1-i));}return o;}\n"

		"function skeys(k8){"
		"let k=0n;for(let i=0;i<8;i++)k=(k<<8n)|BigInt(k8[i]);"
		"let cd=prm(k,PC1,64);"
		"let C=cd>>28n,D=cd&0xFFFFFFFn;"
		"const sk=[];"
		"for(let r=0;r<16;r++){"
		"for(let s=0;s<SH[r];s++){"
		"C=((C<<1n)|(C>>27n))&0xFFFFFFFn;"
		"D=((D<<1n)|(D>>27n))&0xFFFFFFFn;}"
		"sk.push(prm((C<<28n)|D,PC2,56));}"
		"return sk;}\n"

		"function ff(R,sk){"
		"let exp=prm(BigInt(R>>>0),EE,32)^sk;"
		"let out=0n;"
		"for(let i=0;i<8;i++){"
		"const b6=Number((exp>>BigInt(42-i*6))&0x3Fn);"
		"const row=((b6&0x20)>>4)|(b6&1),col=(b6>>1)&0xF;"
		"out|=BigInt(SB[i][row*16+col])<<BigInt(28-i*4);}"
		"return Number(prm(out,PP,32)&0xFFFFFFFFn);}\n"

		"function desBlk(k8,b8,dec){"
		"const sk=skeys(k8);"
		"let v=0n;for(let i=0;i<8;i++)v=(v<<8n)|BigInt(b8[i]);"
		"v=prm(v,IP,64);"
		"let L=Number((v>>32n)&0xFFFFFFFFn),R=Number(v&0xFFFFFFFFn);"
		"for(let i=0;i<16;i++){const t=R;"
		"R=(L^ff(R,dec?sk[15-i]:sk[i]))>>>0;L=t;}"
		"const fp=prm((BigInt(R>>>0)<<32n)|BigInt(L>>>0),FP,64);"
		"const r=new Uint8Array(8);"
		"for(let i=0;i<8;i++)r[i]=Number((fp>>BigInt(56-i*8))&0xFFn);"
		"return r;}\n"

		"return{e:(k,b)=>desBlk(k,b,false),d:(k,b)=>desBlk(k,b,true)};"
		"})();\n");

	/* 3DES ECB decrypt -- PyCryptodome compatible: D_K1(E_K2(D_K3(C))) */
	pos = buf_printf(&buf, &bsz, pos,
		"function tdesD(k24,data){"
		"const k1=k24.slice(0,8),k2=k24.slice(8,16),k3=k24.slice(16,24);"
		"const out=new Uint8Array(data.length);"
		"for(let i=0;i<data.length;i+=8){"
		"let b=data.slice(i,i+8);"
		"b=DES.d(k3,b);b=DES.e(k2,b);b=DES.d(k1,b);out.set(b,i);}"
		"return out;}\n");

	/* rotate 8-byte buffer */
	pos = buf_printf(&buf, &bsz, pos,
		"function rot8(buf,left){"
		"const b=new Uint8Array(buf);"
		"if(left){let t1=b[7];for(let k=0;k<8;k++){const t2=t1;t1=b[k];b[k]=((b[k]<<1)|(t2>>7))&0xFF;}}"
		"else{let t1=b[0];for(let k=7;k>=0;k--){const t2=t1;t1=b[k];b[k]=((b[k]>>1)|(t2<<7))&0xFF;}}"
		"return b;}\n");

	/* TVCAS session key transform */
	pos = buf_printf(&buf, &bsz, pos,
		"function tvKT(key8,enc){"
		"const key=new Uint8Array(key8);"
		"let bk=new Uint8Array([0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88]);"
		"if(enc){"
		"for(let i=0;i<8;i++)bk=rot8(bk,true);"
		"for(let i1=7;i1>=0;i1--){"
		"bk=rot8(bk,false);"
		"for(let i2=7;i2>=0;i2--){"
		"const ok7=key[6];"
		"const t1=CT[ok7^bk[i2]^i1];"
		"const s=[...key.slice(0,6)];"
		"key[0]=key[7]^t1;"
		"for(let j=1;j<7;j++)key[j]=s[j-1];"
		"key[6]^=t1;key[7]=ok7;"
		"}}"
		"}else{"
		"for(let i1=0;i1<8;i1++){"
		"for(let i2=0;i2<8;i2++){"
		"const t1=CT[key[7]^bk[i2]^i1],t2=key[0];"
		"for(let j=0;j<6;j++)key[j]=key[j+1];"
		"key[5]^=t1;key[6]=key[7];key[7]=t1^t2;}"
		"bk=rot8(bk,true);}}"
		"return key;}\n");

	/* hex utils */
	pos = buf_printf(&buf, &bsz, pos,
		"function h2b(h){const b=new Uint8Array(h.length/2);"
		"for(let i=0;i<b.length;i++)b[i]=parseInt(h.slice(i*2,i*2+2),16);return b;}\n"
		"function b2h(b){return Array.from(b).map(x=>x.toString(16).padStart(2,'0')).join('').toUpperCase();}\n");

	/* convert key */
	pos = buf_printf(&buf, &bsz, pos,
		"function convKey(hex,to4){"
		"const kb=h2b(hex),res=new Uint8Array(kb.length);"
		"for(let i=0;i<kb.length;i+=8)res.set(tvKT(kb.slice(i,i+8),to4),i);"
		"return b2h(res);}\n");

	/* decrypt ECM */
	pos = buf_printf(&buf, &bsz, pos,
		"function decEcm(ecmH,keyH){"
		"ecmH=ecmH.replace(/[\\s]/g,'');"
		"if(ecmH.length!==110)return{err:'ECM must be 110 hex chars (got '+ecmH.length+')'};"
		"const hdr=ecmH.slice(0,2).toUpperCase();"
		"if(hdr!=='80'&&hdr!=='81')return{err:'Invalid ECM header (expected 80 or 81, got '+hdr+')'};"
		"const par=parseInt(hdr,16);"
		"const ed=h2b(ecmH.slice(14)),kr=h2b(keyH);"
		"if(ed.length!==48)return{err:'ECM payload must be 48 bytes, got '+ed.length};"
		"if(kr.length!==32)return{err:'Key must be 32 bytes, got '+kr.length};"
		"const kd=new Uint8Array(32);"
		"for(let i=0;i<32;i+=8)kd.set(tvKT(kr.slice(i,i+8),false),i);"
		"const off=par===0x81?16:0;"
		"const dk=new Uint8Array(24);"
		"dk.set(kd.slice(off,off+16));dk.set(kd.slice(off,off+8),16);"
		"const dec=tdesD(dk,ed);"
		"let sum=0;for(let i=0;i<47;i++)sum=(sum+dec[i])&0xFF;"
		"const ts=((dec[0]<<24)|(dec[1]<<16)|(dec[2]<<8)|dec[3])>>>0;"
		"const ac=((dec[20]<<24)|(dec[21]<<16)|(dec[22]<<8)|dec[23])>>>0;"
		"const cw=new Uint8Array(16);cw.set(dec.slice(12,20));cw.set(dec.slice(4,12),8);"
		"const d=new Date(ts*1000);"
		"const ts2=d.getUTCFullYear()+'-'+"
		"String(d.getUTCMonth()+1).padStart(2,'0')+'-'+"
		"String(d.getUTCDate()).padStart(2,'0')+' '+"
		"String(d.getUTCHours()).padStart(2,'0')+':'+"
		"String(d.getUTCMinutes()).padStart(2,'0')+':'+"
		"String(d.getUTCSeconds()).padStart(2,'0')+' UTC';"
		"return{csC:sum,csS:dec[47],csOk:sum===dec[47],par:hdr,"
		"ts:ts2,ac:ac.toString(16).toUpperCase().padStart(8,'0'),cw:b2h(cw)};}\n");

	/* UI logic + sessionStorage persistence + aligned output */
	pos = buf_printf(&buf, &bsz, pos,
		"/* ── sessionStorage helpers ── */\n"
		"function tvSave(){"
		"const v=document.querySelector('input[name=ecm_v]:checked').value;"
		"sessionStorage.setItem('tv_ecm',document.getElementById('ecm_in').value);"
		"sessionStorage.setItem('tv_k3',document.getElementById('k3in').value);"
		"sessionStorage.setItem('tv_k4',document.getElementById('k4in').value);"
		"sessionStorage.setItem('tv_ecmv',v);"
		"sessionStorage.setItem('tv_cvk',document.getElementById('cv_in').value);"
		"const d=document.querySelector('input[name=cv_d]:checked');"
		"if(d)sessionStorage.setItem('tv_cvd',d.value);"
		"sessionStorage.setItem('tv_tab',document.querySelector('.tv-tab.active')?[].indexOf.call(document.querySelectorAll('.tv-tab'),document.querySelector('.tv-tab.active')):'0');}\n"

		"function tvLoad(){"
		"const ecm=sessionStorage.getItem('tv_ecm');"
		"const k3=sessionStorage.getItem('tv_k3');"
		"const k4=sessionStorage.getItem('tv_k4');"
		"const ev=sessionStorage.getItem('tv_ecmv');"
		"const cvk=sessionStorage.getItem('tv_cvk');"
		"const cvd=sessionStorage.getItem('tv_cvd');"
		"const tab=parseInt(sessionStorage.getItem('tv_tab')||'0');"
		"if(ecm)document.getElementById('ecm_in').value=ecm;"
		"if(k3)document.getElementById('k3in').value=k3;"
		"if(k4)document.getElementById('k4in').value=k4;"
		"if(ev){const r=document.querySelector('input[name=ecm_v][value=\"'+ev+'\"]');if(r)r.checked=true;}"
		"if(cvk)document.getElementById('cv_in').value=cvk;"
		"if(cvd){const r=document.querySelector('input[name=cv_d][value=\"'+cvd+'\"]');if(r)r.checked=true;}"
		"tvVC();tvDC();if(tab)tvTab(tab);}\n"

		"function tvTab(n){"
		"document.querySelectorAll('.tv-tab').forEach((t,i)=>t.classList.toggle('active',i===n));"
		"document.querySelectorAll('.tv-panel').forEach((p,i)=>p.classList.toggle('active',i===n));"
		"sessionStorage.setItem('tv_tab',n);}\n"

		"function tvVC(){"
		"const v=document.querySelector('input[name=ecm_v]:checked').value;"
		"document.getElementById('k3r').style.display=v==='3'?'':'none';"
		"document.getElementById('k4r').style.display=v==='4'?'':'none';"
		"tvSave();}\n"

		"function tvDC(){"
		"const d=document.querySelector('input[name=cv_d]:checked').value;"
		"document.getElementById('cv_il').textContent=d==='3to4'?"
		"'TVCAS3 Key (32 or 64 hex chars):':'TVCAS4 Key (64 hex chars):';"
		"document.getElementById('cv_ol').textContent=d==='3to4'?'TVCAS4 Key':'TVCAS3 Key';"
		"tvSave();}\n"

		"function sr(id,html){document.getElementById(id).innerHTML=html;}\n"
		"function row(k,v){return '<tr><td class=tk>'+k+'</td><td class=tv>'+v+'</td></tr>';}\n"

		"function tvDec(){"
		"tvSave();"
		"try{"
		"const ecm=document.getElementById('ecm_in').value.trim().replace(/[\\s]+/g,'');"
		"if(!ecm)return sr('ecm_res','<div class=tv-res-empty><span class=tv-er>Missing ECM</span></div>');"
		"if(ecm.length!==110)"
		"return sr('ecm_res','<div class=tv-res-empty><span class=tv-er>ECM must be 110 hex chars (got '+ecm.length+')</span></div>');"
		"const ver=document.querySelector('input[name=ecm_v]:checked').value;"
		"let k4;"
		"if(ver==='3'){"
		"const k3=document.getElementById('k3in').value.trim().replace(/[\\s]+/g,'');"
		"if(!k3)return sr('ecm_res','<div class=tv-res-empty><span class=tv-er>Missing TVCAS3 Key</span></div>');"
		"if(k3.length!==32&&k3.length!==64)"
		"return sr('ecm_res','<div class=tv-res-empty><span class=tv-er>Key must be 32 or 64 hex chars</span></div>');"
		"k4=convKey(k3,true);"
		"}else{"
		"k4=document.getElementById('k4in').value.trim().replace(/[\\s]+/g,'');"
		"if(!k4)return sr('ecm_res','<div class=tv-res-empty><span class=tv-er>Missing TVCAS4 Key</span></div>');"
		"if(k4.length!==64)"
		"return sr('ecm_res','<div class=tv-res-empty><span class=tv-er>Key must be 64 hex chars</span></div>');}"
		"const r=decEcm(ecm,k4);"
		"if(r.err)return sr('ecm_res','<div class=tv-res-empty><span class=tv-er>'+r.err+'</span></div>');"
		"const csC=r.csC.toString(16).toUpperCase().padStart(2,'0');"
		"const csS=r.csS.toString(16).toUpperCase().padStart(2,'0');"
		"const ok=r.csOk?'<span class=tv-ok>YES</span>':'<span class=tv-er>NO</span>';"
		"const cc=r.csOk?'tv-ok':'tv-er';"
		/* Two tables side by side, then CW full width below */
		"sr('ecm_res',"
		"'<div class=tv-split>'"
		/* LEFT: Checksum */
		"+'<div class=tv-split-box>'"
		"+'<div class=tv-split-hdr>Checksum</div>'"
		"+'<table class=tv-tbl>'"
		"+row('Calculated','<span class='+cc+'>'+csC+'</span>')"
		"+row('Stored','<span class='+cc+'>'+csS+'</span>')"
		"+row('Valid',ok)"
		"+'</table></div>'"
		/* RIGHT: Output */
		"+'<div class=tv-split-box>'"
		"+'<div class=tv-split-hdr>Output</div>'"
		"+'<table class=tv-tbl>'"
		"+row('Timestamp','<span class=tv-ts>'+r.ts+'</span>')"
		"+row('Access','<span class=tv-hi>'+r.ac+'</span>')"
		"+row('CW','<span class=tv-cw-val>'+r.cw+'</span>')"
		"+'</table></div>'"
		"+'</div>'"
		");"
		"}catch(e){sr('ecm_res','<div class=tv-res-empty><span class=tv-er>'+e.message+'</span></div>');}}\n"

		"function tvConv(){"
		"tvSave();"
		"try{"
		"const k=document.getElementById('cv_in').value.trim().replace(/[\\s]+/g,'');"
		"if(!k)return sr('cv_res','<div class=tv-res-empty><span class=tv-er>Missing key</span></div>');"
		"if(k.length!==32&&k.length!==64)"
		"return sr('cv_res','<div class=tv-res-empty><span class=tv-er>Invalid length (must be 32 or 64 hex chars)</span></div>');"
		"const d=document.querySelector('input[name=cv_d]:checked').value;"
		"const lbl=d==='3to4'?'TVCAS4 Key':'TVCAS3 Key';"
		"sr('cv_res',"
		"'<div class=tv-split-hdr>'+lbl+'</div>'"
		"+'<table class=tv-tbl>'"
		"+row('Key','<span class=tv-cw-val>'+convKey(k,d==='3to4')+'</span>')"
		"+'</table>'"
		");"
		"}catch(e){sr('cv_res','<div class=tv-res-empty><span class=tv-er>'+e.message+'</span></div>');}}\n"

		/* Add input listeners for auto-save */
		"['ecm_in','k3in','k4in','cv_in'].forEach(id=>{"
		"const el=document.getElementById(id);"
		"if(el)el.addEventListener('input',tvSave);});\n"

		"document.addEventListener('DOMContentLoaded',tvLoad);\n"

		"</script>");

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HTTP REQUEST HANDLER  (unchanged routing logic)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void handle_request(int fd, const char *client_ip)
{
	char req[WEB_BUF_SIZE];
	int  rlen = 0;

#if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
	DWORD tv = (DWORD)(WEB_READ_TIMEOUT_S * 1000);
#else
	struct timeval tv = { WEB_READ_TIMEOUT_S, 0 };
#endif
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, SO_CAST(&tv), sizeof(tv));

	while (rlen < (int)sizeof(req) - 1)
	{
		int n = (int)recv(fd, RECV_CAST(req + rlen), sizeof(req) - 1 - rlen, 0);
		if (n <= 0) break;
		rlen += n;
		req[rlen] = '\0';
		if (strstr(req, "\r\n\r\n")) break;
	}
	if (rlen < 10) return;

	char method[8] = {0}, uri[512] = {0};
	sscanf(req, "%7s %511s", method, uri);

	char path[512];
	char *qs = NULL;
	strncpy(path, uri, sizeof(path) - 1);
	path[sizeof(path)-1] = '\0';
	char *qmark = strchr(path, '?');
	if (qmark) { *qmark = '\0'; qs = qmark + 1; }

	if (strcmp(path, "/logpoll") != 0)
		tcmg_log_dbg(D_WEBIF, "%s %s", method, uri);

	/* Auth */
	int authed = 0;
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) authed = 1;

	if (!authed) {
		char *cp = strstr(req, "\r\nCookie:");
		if (!cp) cp = strstr(req, "\nCookie:");
		if (cp) {
			cp = strchr(cp, ':'); if (cp) cp++;
			while (cp && *cp == ' ') cp++;
			char tok[WEB_SESSION_LEN + 1];
			const char *sess = cookie_get_session(cp, tok, sizeof(tok));
			if (sess && session_check(sess)) authed = 1;
		}
	}
	if (!authed) {
		char *ap = strstr(req, "\r\nAuthorization:");
		if (!ap) ap = strstr(req, "\nAuthorization:");
		if (ap) {
			ap = strchr(ap, ':'); if (ap) ap++;
			while (ap && *ap == ' ') ap++;
			if (check_auth(ap)) authed = 1;
		}
	}

	/* POST /login */
	if (strcmp(path, "/login") == 0 && strcmp(method, "POST") == 0)
	{
		char *body_start = strstr(req, "\r\n\r\n");
		if (body_start) body_start += 4;
		char u[128] = {0}, p2[128] = {0};
		form_get(body_start, "u", u, sizeof(u));
		form_get(body_start, "p", p2, sizeof(p2));
		int ok = (g_cfg.webif_user[0] == '\0' && g_cfg.webif_pass[0] == '\0') ||
		         (ct_streq(u, g_cfg.webif_user) && ct_streq(p2, g_cfg.webif_pass));
		if (ok) {
			char token[WEB_SESSION_LEN + 1];
			session_create(token);
			tcmg_log_dbg(D_WEBIF, "login OK for '%s' from %s", u, client_ip);
			send_redirect_with_cookie(fd, "/status", token);
		} else {
			tcmg_log("login FAIL for '%s' from %s", u, client_ip);
			send_login_page(fd, 1);
		}
		return;
	}

	if (!authed) {
		if (strcmp(path, "/login") == 0) send_login_page(fd, 0);
		else send_redirect(fd, "/login");
		return;
	}

	/* Route */
	if (strcmp(path, "/") == 0 || strcmp(path, "/login") == 0)
		send_redirect(fd, "/status");
	else if (strcmp(path, "/status") == 0) {
		char killstr[16];
		get_param(qs ? qs : "", "kill", killstr, sizeof(killstr));
		if (killstr[0]) {
			uint32_t tid = (uint32_t)strtoul(killstr, NULL, 10);
			char killed_user[64] = "";
			get_param(qs ? qs : "", "user", killed_user, sizeof(killed_user));
			client_kill_by_tid(tid);
			tcmg_log("disconnect user '%s' tid=%u (by webif)",
			         killed_user[0] ? killed_user : "?", tid);
		}
		send_page_status(fd);
	}
	else if (strcmp(path, "/users")   == 0) send_page_users(fd);
	else if (strcmp(path, "/failban") == 0) send_page_failban(fd, qs ? qs : "");
	else if (strcmp(path, "/config")  == 0) send_page_config(fd);
	else if (strcmp(path, "/config_save") == 0 && strcmp(method, "POST") == 0)
	{
		const char *body_start = strstr(req, "\r\n\r\n");
		if (body_start) body_start += 4;
		int body_len = body_start ? (int)(req + rlen - body_start) : 0;
		char post_extra[16384] = "";
		if (body_start && body_len < (int)sizeof(post_extra) - 1)
		{
			memcpy(post_extra, body_start, body_len);
			char *clh = strstr(req, "Content-Length:");
			if (!clh) clh = strstr(req, "content-length:");
			if (clh) {
				int clen = atoi(clh + 15);
				while (body_len < clen && body_len < (int)sizeof(post_extra) - 1) {
					int n = (int)recv(fd, RECV_CAST(post_extra + body_len),
					                  sizeof(post_extra) - 1 - body_len, 0);
					if (n <= 0) break;
					body_len += n;
					post_extra[body_len] = '\0';
				}
			}
		}
		handle_config_save(fd, post_extra);
	}
	else if (strcmp(path, "/livelog")  == 0) send_page_livelog(fd);
	else if (strcmp(path, "/logpoll")  == 0) send_logpoll(fd, qs ? qs : "");
	else if (strcmp(path, "/restart")  == 0) send_page_restart(fd, qs ? qs : "");
	else if (strcmp(path, "/shutdown") == 0) send_page_shutdown(fd, qs ? qs : "");
	else if (strcmp(path, "/tvcas")    == 0) send_page_tvcas(fd);
	else if (strcmp(path, "/api/status") == 0) send_api_status(fd);
	else if (strcmp(path, "/api/reload") == 0) {
		g_reload_cfg = 1;
		const char *j = "{\"ok\":true,\"msg\":\"reload scheduled\"}";
		send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
	}
	else if (strcmp(path, "/api/restart") == 0) {
		tcmg_log("restart requested via API");
		g_restart = 1;
		g_running = 0;
		const char *j = "{\"ok\":true,\"msg\":\"restart initiated\"}";
		send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
	}
	else if (strcmp(path, "/api/resetstats") == 0) {
		handle_reset_stats();
		const char *j = "{\"ok\":true,\"msg\":\"stats reset\"}";
		send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
	}
	else {
		const char *msg = "<html><body style='background:#090d14;color:#e8f0fe;font-family:monospace;"
		                  "display:flex;align-items:center;justify-content:center;height:100vh'>"
		                  "<div><h1 style='color:#3b82f6'>404</h1><p>Not Found</p>"
		                  "<a href='/status' style='color:#60a5fa'>← Back to Status</a></div></body></html>";
		send_response(fd, 404, "Not Found", "text/html", msg, (int)strlen(msg));
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  SERVER THREAD + START/STOP  (unchanged)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void *http_server_thread(void *arg)
{
	(void)arg;
	tcmg_log("listening http %s:%d",
	         g_cfg.webif_bindaddr[0] ? g_cfg.webif_bindaddr : "0.0.0.0",
	         g_cfg.webif_port);

	while (s_webif_running)
	{
		struct sockaddr_in ca;
		socklen_t clen = sizeof(ca);
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s_webif_sock, &rfds);
		struct timeval tv = { 1, 0 };
		int rc = select(s_webif_sock + 1, &rfds, NULL, NULL, &tv);
		if (rc <= 0) continue;

		int cfd = accept(s_webif_sock, (struct sockaddr *)&ca, &clen);
		if (cfd < 0) { if (s_webif_running) tcmg_log_dbg(D_WEBIF, "accept() errno=%d", errno); continue; }

		char client_ip[MAXIPLEN];
		inet_ntop(AF_INET, &ca.sin_addr, client_ip, sizeof(client_ip));

		char peek[128] = "";
		recv(cfd, RECV_CAST(peek), sizeof(peek)-1, MSG_PEEK);
		int is_poll = (strstr(peek, "GET /logpoll") != NULL);
		if (!is_poll) tcmg_log_dbg(D_WEBIF, "HTTP connection from %s", client_ip);

		handle_request(cfd, client_ip);
		close(cfd);
	}
	tcmg_log("stopped");
	return NULL;
}

int32_t webif_start(void)
{
	int opt;
	struct sockaddr_in sa;
	if (!g_cfg.webif_enabled) { tcmg_log("disabled in config"); return -1; }

	s_webif_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s_webif_sock < 0) { tcmg_log("socket() failed: %s", strerror(errno)); return -1; }

	opt = 1;
	setsockopt(s_webif_sock, SOL_SOCKET, SO_REUSEADDR, SO_CAST(&opt), sizeof(opt));

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port   = htons((uint16_t)g_cfg.webif_port);
	if (g_cfg.webif_bindaddr[0])
		inet_pton(AF_INET, g_cfg.webif_bindaddr, &sa.sin_addr);
	else
		sa.sin_addr.s_addr = INADDR_ANY;

	if (bind(s_webif_sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		tcmg_log("bind() port %d failed: %s", g_cfg.webif_port, strerror(errno));
		close(s_webif_sock); s_webif_sock = -1; return -1;
	}
	if (listen(s_webif_sock, 16) < 0) {
		tcmg_log("listen() failed: %s", strerror(errno));
		close(s_webif_sock); s_webif_sock = -1; return -1;
	}

	s_webif_running = 1;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setstacksize(&attr, 256 * 1024);

	if (pthread_create(&s_webif_tid, &attr, http_server_thread, NULL) != 0) {
		tcmg_log("pthread_create failed");
		s_webif_running = 0;
		close(s_webif_sock); s_webif_sock = -1;
		pthread_attr_destroy(&attr);
		return -1;
	}
	pthread_attr_destroy(&attr);
	return 0;
}

void webif_stop(void)
{
	if (!s_webif_running) return;
	s_webif_running = 0;
	if (s_webif_sock >= 0) { close(s_webif_sock); s_webif_sock = -1; }
	pthread_join(s_webif_tid, NULL);
}
