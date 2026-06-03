#ifndef TCMG_LOG_H_
#define TCMG_LOG_H_

#define D_NET     1u
#define D_CLIENT  2u
#define D_ECM     4u
#define D_PROTO   8u
#define D_CONF    16u
#define D_WEBIF   32u
#define D_BAN     64u
#define D_RELOAD  128u
#define D_SRVID   256u
#define D_ALL     65535u
#define MAX_DEBUG_LEVELS 9

typedef struct { uint16_t mask; const char *name; } S_DBLEVEL_NAME;
extern const S_DBLEVEL_NAME g_dblevel_names[MAX_DEBUG_LEVELS];
extern volatile uint16_t    g_dblevel;

#ifndef MODULE_LOG_PREFIX
#  define MODULE_LOG_PREFIX NULL
#endif

void tcmg_log_txt(const char *mod, const char *fmt, ...) __attribute__((format(printf,2,3)));
void tcmg_log_hex(const char *mod, const uint8_t *buf, int32_t n, const char *fmt, ...) __attribute__((format(printf,4,5)));

#define tcmg_log(fmt,...)            tcmg_log_txt(MODULE_LOG_PREFIX, fmt, ##__VA_ARGS__)
#define tcmg_log_dbg(mask,fmt,...)   do { if ((mask)&g_dblevel) tcmg_log_txt(MODULE_LOG_PREFIX,fmt,##__VA_ARGS__); } while(0)
#define tcmg_dump(buf,n,fmt,...)     tcmg_log_hex(MODULE_LOG_PREFIX,buf,n,fmt,##__VA_ARGS__)
#define tcmg_dump_dbg(m,buf,n,fmt,...) do { if ((m)&g_dblevel) tcmg_log_hex(MODULE_LOG_PREFIX,buf,n,fmt,##__VA_ARGS__); } while(0)

void    log_ecm_raw(uint16_t caid, uint16_t sid, const uint8_t *data, int32_t len);
void    log_cw_result(uint16_t caid, uint16_t sid, int32_t len,
                      const uint8_t *cw, bool hit, int32_t ms, const char *user);
void    log_init(void);
void    log_flush(void);
void    log_shutdown(void);
void    log_ecm_set(int8_t on);
int8_t  log_ecm_get(void);
typedef enum {
    LOG_TYPE_SYSTEM = 's',
    LOG_TYPE_CLIENT = 'c',
    LOG_TYPE_WEBIF  = 'w',
} E_LOG_TYPE;

void    log_set_file(const char *path);
void    log_set_usrfile(const char *path);
void    log_set_user(const char *user);
void    log_set_type(E_LOG_TYPE type);
int32_t log_ring_since(int32_t from_id, char **out_lines, char **out_users,
                       int32_t max, int32_t *out_next);
int32_t log_ring_total(void);

#endif /* TCMG_LOG_H_ */
