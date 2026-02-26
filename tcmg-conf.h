
#ifndef TCMG_CONF_H_
#define TCMG_CONF_H_

enum e_opt_type
{
	OPT_END    = 0,
	OPT_INT32,
	OPT_INT8,
	OPT_STR,
	OPT_HEX14,    /* 28 hex chars → 14 bytes (DES key) */
	OPT_DATE,     /* "YYYY-MM-DD" → time_t, 0 = never */
};

typedef struct s_cfg_field
{
	const char      *key;
	enum e_opt_type  type;
	size_t           offset;   /* offsetof(struct, member) */
	size_t           str_max;  /* for OPT_STR: sizeof(member) */
	int32_t          def_i;    /* default for INT/BOOL */
	const char      *def_s;    /* default for STR */
	int32_t          lo, hi;   /* range clamp for INT (0,0 = no clamp) */
} S_CFG_FIELD;

#define DEF_OPT_INT32(k, S, f, d, lo, hi) \
	{ k, OPT_INT32, offsetof(S, f), 0, d, NULL, lo, hi }

#define DEF_OPT_INT8(k, S, f, d) \
	{ k, OPT_INT8, offsetof(S, f), 0, d, NULL, 0, 0 }

#define DEF_OPT_STR(k, S, f, d) \
	{ k, OPT_STR, offsetof(S, f), sizeof(((S *)0)->f), 0, d, 0, 0 }

#define DEF_OPT_HEX14(k, S, f) \
	{ k, OPT_HEX14, offsetof(S, f), 14, 0, NULL, 0, 0 }

#define DEF_OPT_DATE(k, S, f) \
	{ k, OPT_DATE, offsetof(S, f), sizeof(time_t), 0, NULL, 0, 0 }

#define DEF_OPT_END \
	{ NULL, OPT_END, 0, 0, 0, NULL, 0, 0 }

extern const S_CFG_FIELD cfg_server_fields[];
extern const S_CFG_FIELD cfg_webif_fields[];
extern const S_CFG_FIELD cfg_account_fields[];


/*
 * cfg_load — parse 'file' into *cfg.
 * Returns true on success.  Does NOT lock acc_lock (caller's responsibility).
 */
bool cfg_load(const char *file, S_CONFIG *cfg);

/*
 * cfg_save — write *cfg to cfg->config_file atomically via .tmp rename.
 * Thread-safe: takes acc_lock for read.
 */
bool cfg_save(const S_CONFIG *cfg);

/*
 * cfg_reload — re-parse file into g_cfg under write-lock.
 * Preserves running webif settings.
 */
bool cfg_reload(const char *file, char *errbuf, size_t errsz);

/* Lookup account by username (caller must hold acc_lock for read). */
S_ACCOUNT *cfg_find_account(const char *user);

/* Allocate a zeroed account and append to cfg->accounts. */
S_ACCOUNT *cfg_account_new(S_CONFIG *cfg);

/* Free all accounts in the list and zero the pointer. */
void cfg_accounts_free(S_CONFIG *cfg);

/*
 * cfg_write_default — create a fully-commented default config file.
 * Called when no config file is found on startup.
 */
bool cfg_write_default(const char *path);

/* Print one-line summary of loaded config. */
void cfg_print(const S_CONFIG *cfg);

const char *cfg_client_name(uint16_t client_id);

#endif /* TCMG_CONF_H_ */
