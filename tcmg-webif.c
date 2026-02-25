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
 *  NEW CSS — DreamBox OpenWebif inspired, fully modernised
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
/* ── Sidebar bottom ── */
".sidebar-footer{margin-top:auto;padding:10px 14px;border-top:1px solid var(--border)}"
".uptime-badge{"
"  display:flex;align-items:center;gap:8px;"
"  background:var(--bg2);border:1px solid var(--border);"
"  border-radius:6px;padding:7px 10px;"
"}"
".pulse-dot{"
"  width:7px;height:7px;border-radius:50%;"
"  background:var(--green);"
"  box-shadow:0 0 0 0 rgba(34,197,94,.4);"
"  animation:pulse 2s infinite;"
"  flex-shrink:0;"
"}"
"@keyframes pulse{"
"  0%{box-shadow:0 0 0 0 rgba(34,197,94,.4)}"
"  70%{box-shadow:0 0 0 7px rgba(34,197,94,0)}"
"  100%{box-shadow:0 0 0 0 rgba(34,197,94,0)}"
"}"
".uptime-text{font-size:11px;color:var(--text1);font-family:var(--font-mono)}"
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
 *  PAGE STRUCTURE — sidebar layout
 * ═══════════════════════════════════════════════════════════════════════════ */

/* SVG icons (inline, no deps) */
#define ICO_STATUS  "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path d='M2 10a8 8 0 1116 0A8 8 0 012 10zm8-5a1 1 0 00-1 1v4a1 1 0 00.553.894l3 1.5a1 1 0 10.894-1.788L11 9.382V6a1 1 0 00-1-1z'/></svg>"
#define ICO_USERS   "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path d='M9 6a3 3 0 110 6 3 3 0 010-6zM17 6a3 3 0 110 6 3 3 0 010-6zM12.93 17c.046-.327.07-.66.07-1a6.97 6.97 0 00-1.5-4.33A5 5 0 0119 16v1h-6.07zM6 11a5 5 0 015 5v1H1v-1a5 5 0 015-5z'/></svg>"
#define ICO_BAN     "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M13.477 14.89A6 6 0 015.11 6.524l8.367 8.368zm1.414-1.414L6.524 5.11a6 6 0 018.367 8.367zM18 10a8 8 0 11-16 0 8 8 0 0116 0z' clip-rule='evenodd'/></svg>"
#define ICO_LOG     "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M3 4a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 4a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 4a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1zm0 4a1 1 0 011-1h4a1 1 0 110 2H4a1 1 0 01-1-1z' clip-rule='evenodd'/></svg>"
#define ICO_CFG     "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M11.49 3.17c-.38-1.56-2.6-1.56-2.98 0a1.532 1.532 0 01-2.286.948c-1.372-.836-2.942.734-2.106 2.106.54.886.061 2.042-.947 2.287-1.561.379-1.561 2.6 0 2.978a1.532 1.532 0 01.947 2.287c-.836 1.372.734 2.942 2.106 2.106a1.532 1.532 0 012.287.947c.379 1.561 2.6 1.561 2.978 0a1.533 1.533 0 012.287-.947c1.372.836 2.942-.734 2.106-2.106a1.533 1.533 0 01.947-2.287c1.561-.379 1.561-2.6 0-2.978a1.532 1.532 0 01-.947-2.287c.836-1.372-.734-2.942-2.106-2.106a1.532 1.532 0 01-2.287-.947zM10 13a3 3 0 100-6 3 3 0 000 6z' clip-rule='evenodd'/></svg>"
#define ICO_STOP    "<svg class='nav-icon' viewBox='0 0 20 20' fill='currentColor'><path fill-rule='evenodd' d='M10 18a8 8 0 100-16 8 8 0 000 16zM8 7a1 1 0 00-1 1v4a1 1 0 001 1h4a1 1 0 001-1V8a1 1 0 00-1-1H8z' clip-rule='evenodd'/></svg>"

/* ── Nav item type (shared across all nav groups) ── */
typedef struct { const char *id; const char *href; const char *icon; const char *label; } NavItem;

static int emit_header(char **buf, int *bsz, int pos,
                        const char *title, const char *active)
{
	int is_status = (strcmp(active, "status") == 0);
	int poll_ms   = (is_status && g_cfg.webif_refresh > 0)
	                ? (g_cfg.webif_refresh * 1000) : 5000;

	char upstr[32];
	format_uptime(time(NULL) - g_start_time, upstr, sizeof(upstr));

	pos = buf_printf(buf, bsz, pos,
		"<!DOCTYPE html><html lang='en'><head>"
		"<meta charset='UTF-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>tcmg — %s</title>"
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
		{ "config",   "/config",   ICO_CFG,    "Config"   },
		{ "shutdown", "/shutdown", ICO_STOP,   "Shutdown" },
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

	pos = buf_printf(buf, bsz, pos,
		"</nav>"
		"<div class='sidebar-footer'>"
		"  <div class='uptime-badge'>"
		"    <div class='pulse-dot'></div>"
		"    <div class='uptime-text' id='sb_up'>%s</div>"
		"  </div>"
		"</div>"
		"</div>",
		upstr);

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
		"    <span class='topbar-badge' id='tb_hitrate'>—</span>"
		"  </div>"
		"</div>"
		"<div id='content'>",
		title, srv_addr,
		g_active_conns);

	/* Sidebar collapse JS */
	pos = buf_printf(buf, bsz, pos,
		"<script>"
		"function toggleSidebar(){"
		"  var s=document.getElementById('sidebar');"
		"  var m=document.getElementById('main');"
		"  s.classList.toggle('collapsed');"
		"  m.classList.toggle('expanded');"
		"}"
		"</script>");

	/* Status auto-poll JS (only on status page) */
	if (is_status)
	{
		pos = buf_printf(buf, bsz, pos,
			"<script>"
			"var _pm=%d,_pl=false;"
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
			"  var hr=d.hit_rate_pct?d.hit_rate_pct.toFixed(1)+'%%':'—';"
			"  set('p_hrate',hr);"
			"  set('tb_hitrate',hr);"
			"  var bar=document.getElementById('p_hbar');"
			"  if(bar)bar.style.width=(d.hit_rate_pct||0)+'%%';"
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
			"      +'<td><button class=\"kill-btn\" onclick=\"_kill('+cl.thread_id+',\\''+_esc(cl.user)+'\\')\">&#10005;</button></td>'"
			"      +'</tr>';"
			"  });"
			"  tb.innerHTML=rows||'<tr class=\"empty-row\"><td colspan=\"8\">No active connections</td></tr>';"
			"}"
			"document.addEventListener('DOMContentLoaded',function(){setTimeout(_poll,_pm);});"
			"</script>",
			poll_ms);
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
		"display:flex;align-items:center;justify-content:space-between;"
		"font-size:11px;color:var(--text2)'>"
		"<span>tcmg <span style='color:var(--text1)'>" TCMG_VERSION "</span>"
		" &bull; built <span style='color:var(--text1)'>" TCMG_BUILD_TIME "</span></span>"
		"<span>debug <span class='mono' style='color:var(--accent2)'>0x%04X</span></span>"
		"</div>"
		"</div>"  /* #main */
		"</body></html>",
		g_dblevel);
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
		"<title>tcmg — Login</title>"
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
		"<div style='margin-top:16px;font-size:11px;color:var(--text2);text-align:center'>"
		"Conditional Access Management Gateway"
		"</div>"
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
	int nactive = 0, naccounts;
	pthread_rwlock_rdlock(&g_cfg.acc_lock);
	naccounts = g_cfg.naccounts;
	for (const S_ACCOUNT *a = g_cfg.accounts; a; a = a->next) nactive += a->active;
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
		"<div class='card-sub'>" TCMG_BUILD_TIME "</div>"
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

	/* Hit Rate */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='card %s' style='min-width:200px'>"
		"<div class='card-label'>Hit Rate</div>"
		"<div class='card-value %s' id='p_hrate'>%s</div>"
		"<div class='hitbar-wrap' style='margin-top:8px'>"
		"<div class='hitbar-fill' id='p_hbar' style='width:%.0f%%'></div>"
		"</div></div>",
		hitrate >= 80 ? "green" : hitrate >= 50 ? "" : "red",
		hitrate >= 80 ? "green" : hitrate >= 50 ? "" : "red",
		hrstr, hitrate);

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
		"    <a href='/status?action=resetstats' class='btn btn-ghost btn-sm'>↺ Reset Stats</a>"
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
			"<tr>"
			"<td><span class='bold'>%s</span></td>"
			"<td class='mono'>%s</td>"
			"<td class='mono'><span class='badge badge-blue'>%04X</span></td>"
			"<td class='mono'>%04X</td>"
			"<td>%s</td>"
			"<td class='mono text-muted'>%s</td>"
			"<td class='mono text-muted'>%s</td>"
			"<td><button class='kill-btn' onclick='if(confirm(\"Disconnect %s?\"))location=\"/status?kill=%u\"'>&#10005;</button></td>"
			"</tr>",
			cl->user, cl->ip,
			cl->last_caid, cl->last_srvid,
			cl->last_channel[0] ? cl->last_channel : "<span class='text-muted'>—</span>",
			conn_str, idle_str,
			cl->user, cl->thread_id);
		shown++;
	}
	pthread_mutex_unlock(&g_clients_mtx);

	if (!shown)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='empty-row'><td colspan='8'>No active connections</td></tr>");

	pos = buf_printf(&buf, &bsz, pos, "</tbody></table></div>");

	/* ── Server info ── */
	pos = buf_printf(&buf, &bsz, pos,
		"<div class='section-hdr'><div class='section-title'>Server Info</div></div>"
		"<div class='tbl-wrap'><table><tbody>"
		"<tr><td class='text-muted' style='width:160px'>Version</td>"
		"<td class='mono'>" TCMG_BANNER "</td></tr>"
		"<tr><td class='text-muted'>Port</td>"
		"<td class='mono'>%d</td></tr>"
		"<tr><td class='text-muted'>Config</td>"
		"<td class='mono'>%s</td></tr>"
		"<tr><td class='text-muted'>Debug Mask</td>"
		"<td class='mono'><span class='badge badge-blue'>0x%04X</span></td></tr>"
		"</tbody></table></div>",
		g_cfg.port, g_cfg.config_file, g_dblevel);

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
static void send_page_config(int fd, const char *qs)
{
	(void)qs;
	int bsz = 65536, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = emit_header(&buf, &bsz, pos, "Config", "config");

	char cfgpath[CFGPATH_LEN + 16];
	snprintf(cfgpath, sizeof(cfgpath), "%s/config.cfg", g_cfgdir);

	FILE *fp = fopen(cfgpath, "r");
	char  filebuf[16384] = "";
	int   filelen = 0;
	if (fp) {
		filelen = (int)fread(filebuf, 1, sizeof(filebuf) - 1, fp);
		if (filelen < 0) filelen = 0;
		filebuf[filelen] = '\0';
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
		"  <div class='section-title'>config.cfg</div>"
		"  <span class='text-muted' style='font-size:11px;font-family:var(--font-mono)'>%s</span>"
		"</div>"
		"<div class='info-box'>"
		"Edit and save to apply changes. A backup is created as <span class='mono'>config.cfg.bak</span> automatically."
		"</div>"
		"<form method='post' action='/config_save'>"
		"<textarea class='cfg-editor' name='cfg' spellcheck='false'>%s</textarea>"
		"<div class='flex gap-8 mb-20' style='margin-top:12px'>"
		"<button type='submit' class='btn btn-primary'>💾 Save &amp; Reload</button>"
		"<a href='/config' class='btn btn-ghost'>Discard</a>"
		"</div>"
		"</form>",
		cfgpath, escaped);
	free(escaped);

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

static void handle_config_save(int fd, const char *post_body, int post_len)
{
	char newcfg[16384] = "";
	form_get(post_body, "cfg", newcfg, sizeof(newcfg));
	if (!newcfg[0]) {
		const char *e = "<html><body><h1>Empty config rejected</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}
	char cfgpath[CFGPATH_LEN + 16];
	snprintf(cfgpath, sizeof(cfgpath), "%s/config.cfg", g_cfgdir);
	char tmppath[CFGPATH_LEN + 20];
	snprintf(tmppath, sizeof(tmppath), "%s/config.cfg.tmp", g_cfgdir);
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
		const char *e = "<html><body><h1>Config parse error — not saved</h1></body></html>";
		send_response(fd, 400, "Bad Request", "text/html", e, (int)strlen(e));
		return;
	}
	remove(tmppath);
	strncpy(parsed.config_file, cfgpath, CFGPATH_LEN - 1);
	/* Backup */
	char bakpath[CFGPATH_LEN + 20];
	snprintf(bakpath, sizeof(bakpath), "%s/config.cfg.bak", g_cfgdir);
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
	tcmg_log("config saved+normalized, triggering reload");
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
			" title='0x%04X'>%s</a>",
			m, on ? " on" : "", m, m, g_dblevel_names[i].name);
		ma += snprintf(masks_arr + ma, sizeof(masks_arr) - ma,
		               "%s%u", i ? "," : "", m);
	}
	int all_on = (g_dblevel == D_ALL);
	pos = buf_printf(&buf, &bsz, pos,
		"<a id='dbALL' href='#' class='dbg-tag%s' onclick='toggleAll();return false;'>ALL</a>"
		"<span class='dbg-mask'>mask: <span id='dbmask'>0x%04X</span></span>"
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
			tcmg_log_dbg(D_WEBIF, "livelog debug_level → 0x%04X", g_dblevel);
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
		"\"debug_mask\":\"0x%04X\","
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
			"border-radius:10px;padding:28px 32px;text-align:center'>"
			"<div style='font-size:32px;margin-bottom:12px'>⏹</div>"
			"<div style='font-size:16px;font-weight:600;color:var(--red2);margin-bottom:8px'>"
			"Shutdown Initiated</div>"
			"<div style='color:var(--text2)'>The server is stopping. You can close this window.</div>"
			"</div>");
	}
	else
	{
		pos = buf_printf(&buf, &bsz, pos,
			"<div style='background:var(--bg2);border:1px solid var(--border);"
			"border-radius:10px;padding:32px;max-width:440px'>"
			"<div style='font-size:18px;font-weight:600;margin-bottom:8px'>"
			"⚠ Shutdown tcmg?</div>"
			"<div style='color:var(--text2);margin-bottom:24px;line-height:1.6'>"
			"This will stop the server immediately. All active connections will be dropped."
			"</div>"
			"<div class='flex gap-8'>"
			"<a href='/shutdown?confirm=yes' class='btn btn-danger'>✕ Confirm Shutdown</a>"
			"<a href='/status' class='btn btn-ghost'>Cancel</a>"
			"</div>"
			"</div>");
	}

	pos = emit_footer(&buf, &bsz, pos);
	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HTTP REQUEST HANDLER  (unchanged routing logic)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void handle_request(int fd)
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
			tcmg_log_dbg(D_WEBIF, "login OK for '%s'", u);
			send_redirect_with_cookie(fd, "/status", token);
		} else {
			tcmg_log_dbg(D_WEBIF, "login FAIL for '%s'", u);
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
		char killstr[16], action2[32];
		get_param(qs ? qs : "", "kill",   killstr, sizeof(killstr));
		get_param(qs ? qs : "", "action", action2, sizeof(action2));
		if (killstr[0]) {
			uint32_t tid = (uint32_t)strtoul(killstr, NULL, 10);
			client_kill_by_tid(tid);
			tcmg_log("disconnect tid=%u", tid);
		}
		if (strcmp(action2, "resetstats") == 0) handle_reset_stats();
		send_page_status(fd);
	}
	else if (strcmp(path, "/users")   == 0) send_page_users(fd);
	else if (strcmp(path, "/failban") == 0) send_page_failban(fd, qs ? qs : "");
	else if (strcmp(path, "/config")  == 0) send_page_config(fd, qs ? qs : "");
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
		handle_config_save(fd, post_extra, body_len);
	}
	else if (strcmp(path, "/livelog")  == 0) send_page_livelog(fd);
	else if (strcmp(path, "/logpoll")  == 0) send_logpoll(fd, qs ? qs : "");
	else if (strcmp(path, "/shutdown") == 0) send_page_shutdown(fd, qs ? qs : "");
	else if (strcmp(path, "/api/status") == 0) send_api_status(fd);
	else if (strcmp(path, "/api/reload") == 0) {
		g_reload_cfg = 1;
		const char *j = "{\"ok\":true,\"msg\":\"reload scheduled\"}";
		send_response(fd, 200, "OK", "application/json", j, (int)strlen(j));
	}
	else if (strcmp(path, "/api/normalize") == 0) {
		g_normalize_cfg = 1;
		const char *j = "{\"ok\":true,\"msg\":\"normalize scheduled\"}";
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

		handle_request(cfd);
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