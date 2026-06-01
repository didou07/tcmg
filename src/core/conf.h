#ifndef TCMG_CONF_H_
#define TCMG_CONF_H_

typedef enum { OPT_END=0, OPT_INT32, OPT_INT8, OPT_STR, OPT_HEX14, OPT_DATE } e_opt_type;

typedef struct {
    const char *key;
    e_opt_type  type;
    size_t      offset;
    size_t      str_max;
    int32_t     def_i;
    const char *def_s;
    int32_t     lo, hi;
} S_CFG_FIELD;

#define DEF_OPT_INT32(k,S,f,d,lo,hi) { k, OPT_INT32, offsetof(S,f), 0,          d, NULL, lo, hi }
#define DEF_OPT_INT8(k,S,f,d)        { k, OPT_INT8,  offsetof(S,f), 0,          d, NULL, 0,  0  }
#define DEF_OPT_STR(k,S,f,d)         { k, OPT_STR,   offsetof(S,f), sizeof(((S*)0)->f), 0, d, 0, 0 }
#define DEF_OPT_HEX14(k,S,f)         { k, OPT_HEX14, offsetof(S,f), 14,         0, NULL, 0,  0  }
#define DEF_OPT_DATE(k,S,f)          { k, OPT_DATE,  offsetof(S,f), sizeof(time_t), 0, NULL, 0, 0 }
#define DEF_OPT_END                  { NULL, OPT_END, 0, 0, 0, NULL, 0, 0 }

extern const S_CFG_FIELD cfg_server_fields[];
extern const S_CFG_FIELD cfg_webif_fields[];
extern const S_CFG_FIELD cfg_account_fields[];

bool        cfg_load(const char *file, S_CONFIG *cfg);
bool        cfg_save(S_CONFIG *cfg);
bool        cfg_reload(const char *file, char *errbuf, size_t errsz);
S_ACCOUNT  *cfg_find_account(const char *user);
S_ACCOUNT  *cfg_account_new(S_CONFIG *cfg);
void        cfg_accounts_free(S_CONFIG *cfg);
bool        cfg_write_default(const char *path);
void        cfg_print(const S_CONFIG *cfg);
const char *cfg_client_name(uint16_t client_id);

#endif /* TCMG_CONF_H_ */
