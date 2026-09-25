#define MODULE_LOG_PREFIX "webif"
#include "../../src/pcsc/pcsc.h"
#include "../../src/core/utils.h"
#include "../internal/proto.h"
#include "../service/service.h"

static void hex_encode(const uint8_t *src, uint32_t len, char *dst, int dstsz)
{
    int pos = 0;
    for (uint32_t i = 0; i < len && pos + 2 < dstsz; i++)
        pos += snprintf(dst + pos, (size_t)(dstsz - pos), "%02X", src[i]);
    dst[pos] = '\0';
}

void send_api_pcsc_readers(int fd)
{
    int bsz = 16384, pos = 0;
    char *buf = malloc((size_t)bsz);
    if (!buf) return;

    int enabled = webif_pcsc_enabled();

    pos = buf_printf(&buf, &bsz, pos,
        "{\"enabled\":%d,\"available\":%d,\"count\":%d,\"readers\":[",
        enabled, pcsc_available(), pcsc_reader_count());

    bool first = true;
    for (int i = 0; i < TCMG_PCSC_MAX_READERS; i++) {
        S_PCSC_READER r;
        if (pcsc_reader_get(i, &r) != 0) break;

        char name[TCMG_PCSC_READER_NAME_MAX * 2];
        char atr[TCMG_PCSC_ATR_MAX * 2 + 1];
        json_escape(r.name, name, sizeof(name));
        hex_encode(r.atr, r.atr_len, atr, sizeof(atr));

        const char *proto = "unknown";
        if (r.protocol == 1) proto = "T=0";
        else if (r.protocol == 2) proto = "T=1";

        pos = buf_printf(&buf, &bsz, pos,
            "%s{\"name\":\"%s\",\"present\":%d,\"atr\":\"%s\",\"protocol\":\"%s\"}",
            first ? "" : ",", name, r.present ? 1 : 0, atr, proto);
        first = false;
    }

    pos = buf_printf(&buf, &bsz, pos, "]}");
    send_response(fd, 200, "OK", "application/json", buf, pos);
    free(buf);
}
