#ifndef TCMG_CLIENT_PROTOCOL_STATE_H_
#define TCMG_CLIENT_PROTOCOL_STATE_H_

#include "constants.h"
#include <stdint.h>

typedef struct {
    uint8_t session_key[14];
    int8_t is_mgcamd;
    uint8_t key1[8];
    uint8_t key2[8];
    uint16_t proto;
    uint16_t msgid;
    int8_t client_mode;
    uint8_t header[8];
    uint8_t recv_buf[NC_MSG_MAX];
    uint8_t send_buf[NC_MSG_MAX + 64];
} S_CLIENT_NEWCAMD_STATE;

typedef struct {
    uint8_t ucrc[4];
    uint8_t key[16];
} S_CLIENT_CS378X_STATE;

typedef struct {
    char name[12];
    union {
        S_CLIENT_NEWCAMD_STATE newcamd;
        S_CLIENT_CS378X_STATE cs378x;
    } wire;
} S_CLIENT_PROTOCOL_STATE;

#endif
