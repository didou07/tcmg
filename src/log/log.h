#ifndef TCMG_LOG_H_
#define TCMG_LOG_H_

#include "../core/compat.h"
#include "../core/log_types.h"

#define D_WIRE      0x0001
#define D_ECM       0x0002
#define D_EMU       0x0004
#define D_NEWCAMD   0x0008
#define D_CCCAM     0x0010
#define D_HTTP      0x0020
#define D_CONN      0x0040
#define D_READER    0x0080
#define D_ALL       0xFFFF

#define MAX_DEBUG_LEVELS 8

extern const S_DBLEVEL_NAME g_dblevel_names[MAX_DEBUG_LEVELS];
extern _Atomic uint16_t     g_dblevel;

#ifndef MODULE_LOG_PREFIX
#  define MODULE_LOG_PREFIX NULL
#endif

void tcmg_log_txt(const char *mod, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void tcmg_log_force_txt(const char *mod, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void tcmg_log_hex(const char *mod, const uint8_t *buf, int32_t n, const char *fmt, ...) __attribute__((format(printf, 4, 5)));

#define tcmg_log(...)                    tcmg_log_txt(MODULE_LOG_PREFIX, __VA_ARGS__)
#define tcmg_log_force(...)              tcmg_log_force_txt(MODULE_LOG_PREFIX, __VA_ARGS__)
#define tcmg_log_dbg(mask, ...)           do { if ((mask) & g_dblevel) tcmg_log_txt(MODULE_LOG_PREFIX, __VA_ARGS__); } while (0)
#define tcmg_dump(buf, n, ...)            tcmg_log_hex(MODULE_LOG_PREFIX, (buf), (n), __VA_ARGS__)
#define tcmg_dump_dbg(mask, buf, n, ...)  do { if ((mask) & g_dblevel) tcmg_log_hex(MODULE_LOG_PREFIX, (buf), (n), __VA_ARGS__); } while (0)

typedef enum {
	LOG_TYPE_SYSTEM = 's',
	LOG_TYPE_CLIENT = 'c',
	LOG_TYPE_WEBIF  = 'w',
} E_LOG_TYPE;

void    log_init(void);
void    log_flush(void);
void    log_shutdown(void);
void    log_reopen(void);

void    log_set_file(const char *path);
void    log_set_usrfile(const char *path);
void    log_set_user(const char *user);
void    log_set_type(E_LOG_TYPE type);

void    log_ecm_set(int8_t on);
int8_t  log_ecm_get(void);

void    log_ecm_raw(uint16_t caid, uint16_t sid, const uint8_t *data, int32_t len);
void    log_cw_result(uint16_t caid, uint16_t sid, int32_t len,
                      const uint8_t *cw, bool hit, bool from_cache,
                      int32_t ms, const char *user);

int32_t log_ring_total(void);
typedef int (*log_ring_iter_cb)(int32_t id, const char *line, const char *usr, void *ctx);
int32_t log_ring_foreach(int32_t from_id, int32_t max_lines, log_ring_iter_cb cb, void *ctx, int32_t *out_next);

#endif
