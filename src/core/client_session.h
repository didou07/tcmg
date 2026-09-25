#ifndef TCMG_CLIENT_SESSION_H_
#define TCMG_CLIENT_SESSION_H_

#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    int fd;
    time_t connect_time;
    _Atomic time_t last_activity;
    _Atomic int8_t kill_flag;
} S_CLIENT_SESSION;

#endif
