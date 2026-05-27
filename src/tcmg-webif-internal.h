#ifndef TCMG_WEBIF_INTERNAL_H_
#define TCMG_WEBIF_INTERNAL_H_

/* CSS stylesheet (defined in tcmg-webif-layout.c) */
extern const char CSS[];

/* Shared constants */
#define WEB_SERVER_NAME     "tcmg/" TCMG_VERSION
#define WEB_READ_TIMEOUT_S  10
#define WEB_BUF_SIZE        8192
#define WEB_MAX_LINES_POLL  200
#define WEB_SESSION_TIMEOUT 3600
#define WEB_SESSION_LEN     32
#define WEB_MAX_SESSIONS    16

/* Session management */
typedef struct {
	char   token[WEB_SESSION_LEN + 1];
	time_t expires;
} s_session;

extern s_session       s_sessions[WEB_MAX_SESSIONS];
extern pthread_mutex_t s_sess_lock;

void        session_gen_token(char *out);
void        session_create(char *token_out);
int         session_check(const char *token);
const char *cookie_get_session(const char *cookie_hdr, char *buf, int bufsz);

/* Dynamic response buffer */
int buf_printf(char **dst, int *dstsz, int pos, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

/* URL / form helpers */
void url_decode(char *s);
void get_param(const char *qs, const char *key, char *out, int outsz);
void form_get(const char *body, const char *key, char *out, int outsz);

/* HTTP response helpers */
void send_headers_ex(int fd, int code, const char *reason,
                     const char *ctype, int length, const char *set_cookie);
void send_response_ex(int fd, int code, const char *reason,
                      const char *ctype, const char *body, int blen,
                      const char *set_cookie);
void send_response(int fd, int code, const char *reason,
                   const char *ctype, const char *body, int blen);
void send_redirect(int fd, const char *location);
void send_redirect_with_cookie(int fd, const char *location, const char *token);

/* Base64 */
void b64_encode(const char *in, int ilen, char *out, int outsz);

/* Auth */
int  check_auth(const char *auth_header);

/* Layout */
typedef struct { const char *id; const char *href; const char *icon; const char *label; } NavItem;
int  emit_header(char **buf, int *bsz, int pos, const char *title, const char *active);
int  emit_footer(char **buf, int *bsz, int pos);

/* Stats */
typedef struct { int64_t cw_found; int64_t cw_not; int nbans; } S_STATS;
S_STATS aggregate_stats(void);
void    handle_reset_stats(void);

/* Page handlers */
void send_login_page(int fd, int failed);
void send_page_status(int fd);
void send_page_users(int fd);
void send_page_failban(int fd, const char *qs);
void send_page_config(int fd);
void handle_config_save(int fd, const char *post_body);
void send_page_livelog(int fd);
void send_logpoll(int fd, const char *qs);
void send_api_status(int fd);
void send_page_shutdown(int fd, const char *qs);
void send_page_restart(int fd, const char *qs);
void send_page_tvcas(int fd);

/* Request handler */
void handle_request(int fd, const char *client_ip);

/* Server state */
extern int8_t    s_webif_running;
extern int       s_webif_sock;


/* Stat card SVG icon macros (shared between layout and pages) */
/* Stat card icons */
#define ICO_CLOCK \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<circle cx='12' cy='12' r='10'/>"\
"<polyline points='12 6 12 12 16 14'/>"\
"</svg>"

#define ICO_USERS2 \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2'/>"\
"<circle cx='9' cy='7' r='4'/>"\
"<path d='M23 21v-2a4 4 0 0 0-3-3.87'/><path d='M16 3.13a4 4 0 0 1 0 7.75'/>"\
"</svg>"

#define ICO_ZAP \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<polygon points='13 2 3 14 12 14 11 22 21 10 12 10 13 2'/>"\
"</svg>"

#define ICO_CHECK \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<polyline points='20 6 9 17 4 12'/>"\
"</svg>"

#define ICO_X \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<line x1='18' y1='6' x2='6' y2='18'/><line x1='6' y1='6' x2='18' y2='18'/>"\
"</svg>"

#define ICO_SHIELD \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z'/>"\
"</svg>"

#define ICO_KEY \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M21 2l-2 2m-7.61 7.61a5.5 5.5 0 1 1-7.778 7.778 5.5 5.5 0 0 1 7.777-7.777zm0 0L15.5 7.5m0 0l3 3L22 7l-3-3m-3.5 3.5L19 4'/>"\
"</svg>"

#define ICO_PERCENT \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<line x1='19' y1='5' x2='5' y2='19'/>"\
"<circle cx='6.5' cy='6.5' r='2.5'/>"\
"<circle cx='17.5' cy='17.5' r='2.5'/>"\
"</svg>"

#define ICO_FILE \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"\
"<polyline points='14 2 14 8 20 8'/>"\
"</svg>"

#define ICO_POWER \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/>"\
"<line x1='12' y1='2' x2='12' y2='12'/>"\
"</svg>"

#define ICO_WARN \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z'/>"\
"<line x1='12' y1='9' x2='12' y2='13'/>"\
"<line x1='12' y1='17' x2='12.01' y2='17'/>"\
"</svg>"

#define ICO_SAVE \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z'/>"\
"<polyline points='17 21 17 13 7 13 7 21'/>"\
"<polyline points='7 3 7 8 15 8'/>"\
"</svg>"

#define ICO_KILLBTN \
"<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>" \
"<line x1='18' y1='6' x2='6' y2='18'/><line x1='6' y1='6' x2='18' y2='18'/>" \
"</svg>"


#endif /* TCMG_WEBIF_INTERNAL_H_ */
