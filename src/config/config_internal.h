#ifndef TCMG_CONFIG_INTERNAL_H_
#define TCMG_CONFIG_INTERNAL_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "../core/compat.h"
#include "../core/constants.h"
#include "../core/config_types.h"
#include "../core/utils.h"
#include "../log/log.h"
#include "../crypto/crypto.h"
#include "../platform/platform.h"
#include "config.h"
typedef struct {
    char key[128];
    char value[CFGVAL_LEN];
    int line;
} S_KV;
typedef enum {
    CFG_CB_OK = 1,
    CFG_CB_INVALID = 0,
    CFG_CB_SKIP_SECTION = 2
} E_CFG_CB_RESULT;
typedef E_CFG_CB_RESULT (*cfg_kv_callback)(const char *section, const S_KV *kv,
                                             void *ctx, char *err, size_t errsz);
void cfg_default_runtime(S_CONFIG *cfg);
void cfg_str_trim(char *s);
bool cfg_parse_file(const char *path, cfg_kv_callback cb, void *ctx,
                    char *err, size_t errsz);
bool cfg_parse_bool(const char *s, bool *out);
bool cfg_parse_i32_range(const char *s, int32_t lo, int32_t hi, int32_t *out);
bool cfg_parse_hex_bytes(const char *s, uint8_t *out, size_t n);
bool cfg_parse_u16_hex(const char *s, uint16_t *out);
bool cfg_parse_u8_hex(const char *s, uint8_t *out);
bool cfg_parse_group_list(const char *s, int32_t *groups, int32_t *count);
bool cfg_parse_u16_list(const char *s, uint16_t *out, int32_t *count, int32_t maxn);
bool cfg_parse_ident_list(const char *s, S_IDENT_FILTER *out, int32_t *count, int32_t maxn);
bool cfg_parse_ipv4_list(const char *s, char out[][MAXIPLEN], int32_t *count);
bool cfg_parse_ecm_key(const char *s, uint16_t def_caid, S_ECMKEY *out);
bool cfg_parse_date(const char *s, time_t *out);
bool cfg_parse_schedule(const char *s, S_ACCOUNT *account);
void cfg_strip_inline_comment(char *s);
bool cfg_parse_bindaddr(const char *s);
E_CFG_CB_RESULT cfg_global_cb(const char *section, const S_KV *kv, void *ctx,
                              char *err, size_t errsz);
bool cfg_parse_users(const char *path, S_CONFIG *cfg, char *err, size_t errsz);
bool cfg_parse_readers(const char *path, S_CONFIG *cfg, char *err, size_t errsz);
bool cfg_validate(S_CONFIG *cfg, char *err, size_t errsz);
bool cfg_listener_settings_changed(const S_CONFIG *old_cfg,
                                   const S_CONFIG *new_cfg,
                                   char *err, size_t errsz);
void cfg_path_sibling(char *out, size_t outsz, const char *global, const char *name);
bool cfg_appendf(char *buf, size_t cap, size_t *pos, const char *fmt, ...);
bool cfg_write_atomic(const char *path, const char *data);
bool cfg_build_global_text(const S_CONFIG *cfg, char *buf, size_t cap);
bool cfg_build_user_text(const S_CONFIG *cfg, char *buf, size_t cap);
bool cfg_build_server_text(const S_CONFIG *cfg, char *buf, size_t cap);
bool cfg_write_default_if_missing(const char *path, const char *text);
#endif
