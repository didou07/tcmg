#define MODULE_LOG_PREFIX "reader"
#include "reader_signature.h"
#include "../crypto/crypto.h"
#include "../core/utils.h"

void reader_session_signature(const S_READER *reader, char *out, size_t out_len)
{
    if (!reader || !out || out_len == 0) return;
    out[0] = '\0';

    char blob[1536];
    const int n = snprintf(blob, sizeof(blob), "%s\x1F%s\x1F%s\x1F%s\x1F%d",
                           reader->device, reader->user, reader->password,
                           reader->protocol, reader->inactivitytimeout);
    if (n < 0 || (size_t)n >= sizeof(blob) || out_len < 41) {
        secure_zero(blob, sizeof(blob));
        return;
    }

    uint8_t hash[20];
    sha1_hash((const uint8_t *)blob, (size_t)n, hash);
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < sizeof(hash); i++) {
        out[i * 2] = hex[hash[i] >> 4];
        out[i * 2 + 1] = hex[hash[i] & 0x0F];
    }
    out[40] = '\0';
    secure_zero(hash, sizeof(hash));
    secure_zero(blob, sizeof(blob));
}
