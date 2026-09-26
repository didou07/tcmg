#include "serial/serial.h"
#include "reader/protocol.h"
#include "config/config.h"
#include "config/config_internal.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char ports[TCMG_SERIAL_MAX_PORTS][TCMG_SERIAL_PORT_LEN];
    size_t n = serial_list_ports(ports, TCMG_SERIAL_MAX_PORTS);
    if (n > TCMG_SERIAL_MAX_PORTS) return 1;
    for (size_t i = 0; i < n; i++) {
        if (!ports[i][0]) return 2;
    }
    if (!reader_protocol_find("serial")) return 3;
    S_CONFIG cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.readers[0].in_use = 1;
    cfg.readers[0].enabled = 1;
    snprintf(cfg.readers[0].label, sizeof(cfg.readers[0].label), "serial1");
    snprintf(cfg.readers[0].protocol, sizeof(cfg.readers[0].protocol), "serial");
    snprintf(cfg.readers[0].device, sizeof(cfg.readers[0].device), "/dev/ttyUSB0");
    cfg.readers[0].do_ecm = 1;
    cfg.readers[0].poll_ms = 250;
    cfg.readers[0].ecm_whitelist = 0x37;
    cfg.readers[0].ngroups = 1;
    cfg.readers[0].groups[0] = 1;
    char err[256] = "";
    if (!cfg_validate(&cfg, err, sizeof(err))) {
        fprintf(stderr, "FAIL: serial config rejected: %s\n", err);
        return 4;
    }
    puts("SERIAL: PASS");
    return 0;
}
