#ifndef TCMG_WEBIF_PROTO_H_
#define TCMG_WEBIF_PROTO_H_

#include "constants.h"
#include "types.h"
#include "../icons.h"

extern const char CSS[];
extern s_session       s_sessions[WEB_MAX_SESSIONS];
extern pthread_mutex_t s_sess_lock;

void        session_gen_token(char *out);
void        session_create(char *token_out);
int         session_check(const char *token);
void        session_invalidate(const char *token);
const char *cookie_get_session(const char *cookie_hdr, char *buf, int bufsz);

int  req_parse(s_http_req *req, int fd, char *raw, int rawlen);
void req_free(s_http_req *req);

int  buf_printf(char **dst, int *dstsz, int pos, const char *fmt, ...)
     __attribute__((format(printf, 4, 5)));
void url_decode(char *s);
void get_param(const char *qs, const char *key, char *out, int outsz);
void form_get(const char *body, const char *key, char *out, int outsz);

void send_headers_ex(int fd, int code, const char *reason,
                     const char *ctype, int length, const char *set_cookie);
void send_response_ex(int fd, int code, const char *reason,
                      const char *ctype, const char *body, int blen,
                      const char *set_cookie);
void send_response(int fd, int code, const char *reason,
                   const char *ctype, const char *body, int blen);
void send_redirect(int fd, const char *location);
void send_redirect_with_cookie(int fd, const char *location, const char *token);
void send_redirect_clear_cookie(int fd, const char *location);

void b64_encode(const char *in, int ilen, char *out, int outsz);
int  check_auth(const char *auth_header);
int  check_credentials(const char *user, const char *pass);

int  html_escape(const char *src, char *dst, int dstsz);
char *html_escape_alloc(const char *src, int maxbytes, int *truncated);
char *file_read_escaped(const char *path, int maxbytes, int *truncated);
int  json_escape(const char *src, char *dst, int dstsz);

S_SERVER_STATS collect_stats(void);
void           handle_reset_stats(void);

int  emit_header(char **buf, int *bsz, int pos, const char *title, const char *active);
int  emit_footer(char **buf, int *bsz, int pos);

void send_login_page(int fd, int failed);
void send_page_status(int fd);
void send_page_users(int fd);
void send_page_failban(int fd, const char *qs);
void send_page_config(int fd);
void send_page_files(int fd);
void send_page_livelog(int fd);
void send_logpoll(int fd, const char *qs);
void send_page_shutdown(int fd, const char *qs);
void send_page_restart(int fd, const char *qs);
void send_page_power(int fd, const char *qs);
void send_page_tvcas(int fd);

void handle_request(int fd, const char *client_ip);

void send_api_status(int fd);
void handle_user_toggle(int fd, const char *qs);
void send_api_user_get(int fd, const char *qs);
void handle_user_save(int fd, const char *body);
void handle_user_add(int fd, const char *body);
void handle_user_delete(int fd, const char *qs);
void handle_user_resetstats(int fd, const char *qs);

void send_api_config_get(int fd);
void handle_api_config_save(int fd, const char *body);
void handle_api_file_save(int fd, const char *body);

void handle_api_reload(int fd);
void handle_api_restart(int fd);
void handle_api_resetstats(int fd);

#include "macros.h"

#endif
