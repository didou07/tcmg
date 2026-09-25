#ifndef TCMG_WEBIF_SERVICE_H_
#define TCMG_WEBIF_SERVICE_H_

#include "types.h"
#include <stdbool.h>
#include <stddef.h>

bool webif_config_snapshot(S_WEBIF_CONFIG_VIEW *out);
bool webif_config_apply(const S_WEBIF_CONFIG_PATCH *patch, bool *restart_required);
bool webif_save_config_file(const char *content, size_t len);
bool webif_save_srvid_file(const char *content, size_t len, int *loaded);
bool webif_save_users_file(const char *content, size_t len, char *err, size_t errsz);
bool webif_save_readers_file(const char *content, size_t len, char *err, size_t errsz);

int webif_account_count(void);
int webif_account_snapshot_all(S_WEBIF_ACCOUNT_VIEW *out, size_t cap);
int webif_account_userstats_snapshot_all(S_WEBIF_USER_STATS_VIEW *out, size_t cap);
bool webif_account_get(const char *user, S_WEBIF_ACCOUNT_VIEW *out);
bool webif_account_toggle(const char *user, int *enabled);
bool webif_account_save(const S_WEBIF_ACCOUNT_EDIT *edit);
bool webif_account_add(const S_WEBIF_ACCOUNT_EDIT *edit, int *status_code);
bool webif_account_delete(const char *user);
bool webif_account_reset_stats(const char *user);
void webif_account_reset_all_stats(void);

int webif_reader_count(void);
bool webif_reader_first_free(int *index);
int webif_reader_snapshot_all(S_WEBIF_READER_VIEW *out, size_t cap);
bool webif_reader_get(int index, S_WEBIF_READER_VIEW *out);
bool webif_reader_save(const S_WEBIF_READER_EDIT *edit);
bool webif_reader_delete(int index);

int webif_client_snapshot_all(S_WEBIF_CLIENT_VIEW *out, size_t cap);
int webif_active_connection_count(void);
void webif_client_kill_by_tid(uint32_t tid);
void webif_client_kill_by_user(const char *user);

S_WEBIF_SERVER_STATS webif_server_stats(void);
int webif_pcsc_enabled(void);

int webif_ban_snapshot(S_WEBIF_BAN_VIEW *out, size_t cap, S_WEBIF_BAN_STATS *stats);
int webif_ban_clear(const char *ip);
int webif_ban_clear_all(void);

bool webif_auth_enabled(void);
bool webif_auth_basic_valid(const char *header);
bool webif_credentials_valid(const char *user, const char *pass);
int webif_port(void);
void webif_bindaddr(char *out, size_t sz);
int webif_max_refresh(void);

#endif
