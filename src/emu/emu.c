#define MODULE_LOG_PREFIX "emu"
#include "../../globals.h"

void emu_init(void)
{
	crypt_init();
	tcmg_log_dbg(D_EMU, "%s", "initialized");
}

static uint8_t s_fake_prev[8] = {0};
static int8_t  s_fake_half    = 0;
static pthread_mutex_t s_fake_mtx = PTHREAD_MUTEX_INITIALIZER;

static void gen_fake_cw(uint8_t *cw)
{
    uint8_t fresh[8];
    csprng(fresh, 8);
    pthread_mutex_lock(&s_fake_mtx);
    if (s_fake_half == 0) {
        memcpy(cw,     fresh,        8);
        memcpy(cw + 8, s_fake_prev,  8);
        memcpy(s_fake_prev, fresh,   8);
    } else {
        memcpy(cw,     s_fake_prev,  8);
        memcpy(cw + 8, fresh,        8);
        memcpy(s_fake_prev, fresh,   8);
    }
    s_fake_half ^= 1;
    pthread_mutex_unlock(&s_fake_mtx);
}

static bool key_lookup(const S_ACCOUNT *acc, uint16_t caid,
                        uint8_t kidx, uint8_t *key_out)
{
	int i;
	for (i = 0; i < acc->nkeys; i++)
	{
		if (acc->keys[i].caid == caid)
		{
			memcpy(key_out, kidx == 0 ? acc->keys[i].key0 : acc->keys[i].key1, 16);
			tcmg_log_dbg(D_EMU, "key found for caid=%04X kidx=%u slot=%d", caid, kidx, i);
			return true;
		}
	}
	tcmg_log_dbg(D_EMU, "no key for caid=%04X kidx=%u (account has %d key(s))",
	             caid, kidx, acc->nkeys);
	return false;
}

static uint8_t csum8(const uint8_t *d, uint8_t len)
{
	uint8_t s = 0; uint8_t i;
	for (i = 0; i < len; i++) s += d[i];
	return s;
}

static int32_t tcmg_decode(uint16_t caid, const uint8_t *ecm, int32_t len,
                             uint8_t *cw, const S_ACCOUNT *acc)
{
	if (len < 7) {
		tcmg_log_dbg(D_EMU, "caid=%04X ECM too short len=%d expected>=7", caid, len);
		return EMU_NOT_SUPPORTED;
	}

	uint8_t kidx = ecm[0] & 3;

	if (ecm[4] < 2) {
		tcmg_log_dbg(D_EMU, "caid=%04X ECM[4]=%u too small", caid, ecm[4]);
		return EMU_NOT_SUPPORTED;
	}
	uint8_t slen = ecm[4] - 2;
	uint8_t nano = ecm[5];

	if (slen != 48 || nano != 0x64) {
		tcmg_log_dbg(D_EMU, "caid=%04X unsupported format slen=%u nano=0x%02X (expected slen=48 nano=0x64)",
		             caid, slen, nano);
		return EMU_NOT_SUPPORTED;
	}

	if (len < 7 + (int32_t)slen) {
		tcmg_log_dbg(D_EMU, "caid=%04X ECM truncated len=%d need=%d", caid, len, 7 + slen);
		return EMU_NOT_SUPPORTED;
	}
	const uint8_t *sdata = ecm + 7;

	uint8_t key[16];
	if (!key_lookup(acc, caid, kidx, key)) return EMU_KEY_NOT_FOUND;

	uint8_t dec[48];
	memcpy(dec, sdata, slen);

	tcmg_dump_dbg(D_EMU, sdata, slen,
	              "caid=%04X ENC kidx=%u", caid, kidx);

	crypt_ede2_ecb(key, dec, dec, slen, false);

	tcmg_dump_dbg(D_EMU, dec, slen,
	              "caid=%04X DEC kidx=%u", caid, kidx);

	uint8_t expected_csum = csum8(dec, slen - 1);
	if (dec[slen - 1] != expected_csum)
	{
		tcmg_log_dbg(D_EMU, "caid=%04X checksum error: got=0x%02X expected=0x%02X",
		             caid, dec[slen - 1], expected_csum);
		secure_zero(key, sizeof(key));
		secure_zero(dec, sizeof(dec));
		return EMU_CHECKSUM_ERROR;
	}

	memcpy(cw + 8, dec + 4,  8);
	memcpy(cw,     dec + 12, 8);

	tcmg_dump_dbg(D_EMU, cw, CW_LEN,
	              "caid=%04X CW extracted successfully", caid);
	secure_zero(key, sizeof(key));
	secure_zero(dec, sizeof(dec));
	return EMU_OK;
}

int32_t emu_process(uint16_t caid, uint16_t sid,
                    const uint8_t *ecm, int32_t ecm_len,
                    uint8_t *cw, const S_ECM_CTX *ctx)
{
	int64_t t0  = tcmg_mono_ms();
	int32_t res = EMU_NOT_SUPPORTED;
	bool    hit = false;

	tcmg_dump_dbg(D_EMU, ecm, ecm_len,
	              "emu_process user='%s' caid=%04X sid=%04X",
	              ctx->user ? ctx->user : "?", caid, sid);

	if (!ctx->account) {
		tcmg_log_dbg(D_EMU, "no account context for user='%s'",
		             ctx->user ? ctx->user : "?");
		goto done;
	}

	if (ctx->account->use_fake_cw)
	{
		gen_fake_cw(cw);
		hit = true;
		res = EMU_OK;
		tcmg_log_dbg(D_EMU, "FAKE_CW generated for user='%s' caid=%04X sid=%04X",
		             ctx->user, caid, sid);
		goto done;
	}

	{
		bool has_key = false; int i;
		for (i = 0; i < ctx->account->nkeys; i++)
			if (ctx->account->keys[i].caid == caid)
			{ has_key = true; break; }

		if (!has_key && (caid & 0xFF00) != 0x0B00)
		{
			tcmg_log_dbg(D_EMU, "no key: user='%s' caid=%04X sid=%04X nkeys=%d",
			             ctx->user, caid, sid, ctx->account->nkeys);
		}
		else
		{
			res = tcmg_decode(caid, ecm, ecm_len, cw, ctx->account);
		}
	}

	hit = (res == EMU_OK);

done:
	{
		int32_t ms = tcmg_elapsed_ms(t0);
		tcmg_log_dbg(D_EMU, "done user='%s' caid=%04X sid=%04X result=%s time=%dms",
		             ctx->user ? ctx->user : "?", caid, sid,
		             hit ? "FOUND" : (res == EMU_KEY_NOT_FOUND ? "KEY_NOT_FOUND" :
		                              res == EMU_CHECKSUM_ERROR ? "CHECKSUM_ERROR" :
		                              res == EMU_NOT_SUPPORTED  ? "NOT_SUPPORTED"  : "ERROR"),
		             ms);
	}
	if (!hit) secure_zero(cw, CW_LEN);
	return res;
}
