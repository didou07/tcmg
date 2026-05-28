
#ifndef TCMG_LOG_H_
#define TCMG_LOG_H_

#define D_NET      1u      /* raw socket recv/send bytes          */
#define D_CLIENT   2u      /* client connect / auth / disconnect  */
#define D_ECM      4u      /* ECM request + CW result + emulator  */
#define D_PROTO    8u      /* Newcamd frame encode/decode         */
#define D_CONF     16u     /* config load / save / reload         */
#define D_WEBIF    32u     /* HTTP request/response               */
#define D_BAN      64u     /* fail-ban events                     */
#define D_ALL      65535u  /* enable all categories               */

#define MAX_DEBUG_LEVELS  7  /* number of named levels              */

typedef struct { uint16_t mask; const char *name; } S_DBLEVEL_NAME;
extern const S_DBLEVEL_NAME g_dblevel_names[MAX_DEBUG_LEVELS];

extern volatile uint16_t g_dblevel;

#ifndef MODULE_LOG_PREFIX
#  define MODULE_LOG_PREFIX NULL
#endif

void tcmg_log_txt(const char *mod, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

void tcmg_log_hex(const char *mod, const uint8_t *buf, int32_t n,
                  const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));


#define tcmg_log(fmt, ...) \
	tcmg_log_txt(MODULE_LOG_PREFIX, fmt, ##__VA_ARGS__)

#define tcmg_dump(buf, n, fmt, ...) \
	tcmg_log_hex(MODULE_LOG_PREFIX, buf, n, fmt, ##__VA_ARGS__)

#define tcmg_log_dbg(mask, fmt, ...) \
	do { \
		if ((mask) & g_dblevel) \
			tcmg_log_txt(MODULE_LOG_PREFIX, fmt, ##__VA_ARGS__); \
	} while(0)

#define tcmg_dump_dbg(mask, buf, n, fmt, ...) \
	do { \
		if ((mask) & g_dblevel) \
			tcmg_log_hex(MODULE_LOG_PREFIX, buf, n, fmt, ##__VA_ARGS__); \
	} while(0)

void log_ecm_raw(const uint8_t *data, int32_t len);
void log_cw_result(uint16_t caid, uint16_t sid, int32_t len,
                   const uint8_t *cw, bool hit, int32_t ms, const char *user);
void   log_ecm_set(int8_t on);
int8_t log_ecm_get(void);

/*
 * log_set_file — open a log file for writing (in addition to stdout).
 * Pass NULL or "" to disable file logging.
 * File is rotated (renamed to .1) when it exceeds 10 MB.
 */
void log_set_file(const char *path);

/*
 * log_set_usrfile — open user statistics file (inspired by OSCam usrfile).
 * Each CW result is written here in tab-separated format.
 * Pass NULL or "" to disable. File is rotated at 5 MB.
 */
void log_set_usrfile(const char *path);

/*
 * log_set_user — set per-thread user context for log ring entries.
 * Call from the client thread after authentication.
 * Pass NULL or "" to clear (system/server messages).
 */
void log_set_user(const char *user);

int32_t log_ring_since(int32_t from_id, char **out_lines, char **out_users,
                        int32_t max_lines, int32_t *out_next);
int32_t log_ring_total(void);

#endif /* TCMG_LOG_H_ */
