
#define MODULE_LOG_PREFIX "emu"
#include "tcmg-globals.h"

#ifdef TCMG_OS_WINDOWS
/* Windows: use QueryPerformanceCounter for monotonic time */
static struct timespec now_mono(void)
{
	struct timespec t;
	LARGE_INTEGER freq, cnt;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&cnt);
	t.tv_sec  = (time_t)(cnt.QuadPart / freq.QuadPart);
	t.tv_nsec = (long)((cnt.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
	return t;
}
#else
static struct timespec now_mono(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t;
}
#endif



static int32_t elapsed_ms(struct timespec t0)
{
	struct timespec now = now_mono();
	return (int32_t)((now.tv_sec  - t0.tv_sec)  * 1000 +
	                 (now.tv_nsec - t0.tv_nsec) / 1000000);
}

void emu_init(void)
{
	crypt_init();
}

static void gen_fake_cw(uint8_t *cw)
{
	csprng(cw, CW_LEN);
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
			return true;
		}
	}
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
	if (len < 7) return EMU_NOT_SUPPORTED;

	uint8_t kidx = ecm[0] & 1;
	uint8_t slen = ecm[4] - 2;
	uint8_t nano = ecm[5];
	const uint8_t *sdata = ecm + 7;

	if (slen != 48 || nano != 0x64) return EMU_NOT_SUPPORTED;

	uint8_t key[16];
	if (!key_lookup(acc, caid, kidx, key)) return EMU_KEY_NOT_FOUND;

	uint8_t dec[48];
	memcpy(dec, sdata, slen);

	/* Triple-DES in EDE2 mode, one block at a time (8 bytes) */
	int i;
	for (i = 0; i < slen; i += 8)
	{
		crypt_des_dec(key,     dec + i, dec + i);
		crypt_des_enc(key + 8, dec + i, dec + i);
		crypt_des_dec(key,     dec + i, dec + i);
	}

	/* Checksum: last byte must equal sum of all preceding bytes */
	if (dec[slen - 1] != csum8(dec, slen - 1))
	{
		secure_zero(key, sizeof(key));
		secure_zero(dec, sizeof(dec));
		return EMU_CHECKSUM_ERROR;
	}

	memcpy(cw + 8, dec + 4,  8);
	memcpy(cw,     dec + 12, 8);

	secure_zero(key, sizeof(key));
	secure_zero(dec, sizeof(dec));
	return EMU_OK;
}

int32_t emu_process(uint16_t caid, uint16_t sid,
                    const uint8_t *ecm, int32_t ecm_len,
                    uint8_t *cw, const S_ECM_CTX *ctx)
{
	struct timespec t0 = now_mono();
	int32_t res = EMU_NOT_SUPPORTED;
	bool    hit = false;

	log_ecm_raw(ecm, ecm_len);

	if (!ctx->account) goto done;

	/* Fake CW mode -- always "hit" */
	if (ctx->account->use_fake_cw)
	{
		gen_fake_cw(cw);
		hit = true;
		res = EMU_OK;
		tcmg_log_dbg(D_ECM, "CAID=%04X SID=%04X fake_cw → generated", caid, sid);
		goto done;
	}

	/* Check if this account has a key for caid */
	{
		bool has_key = false; int i;
		for (i = 0; i < ctx->account->nkeys; i++)
			if (ctx->account->keys[i].caid == caid)
			{ has_key = true; break; }

		/* Also attempt decode for any 0x0Bxx CAID even without explicit key */
		if (has_key || (caid & 0xFF00) == 0x0B00)
			res = tcmg_decode(caid, ecm, ecm_len, cw, ctx->account);
	}

	hit = (res == EMU_OK);
	tcmg_log_dbg(D_ECM, "CAID=%04X SID=%04X decode → %s (res=%d)",
	             caid, sid, hit ? "OK" : "FAIL", res);

done:
	{
		int32_t ms = elapsed_ms(t0);
		log_cw_result(caid, sid, ecm_len, cw, hit, ms, ctx->user);
		if (ctx->account)
		{
			/* Use a mutex for 64-bit stat updates: __sync_fetch_and_add_8 is
			 * not available on 32-bit MIPS, SH4, or PowerPC SPE targets.
			 * Stats are updated infrequently (once per ECM) so the lock
			 * overhead is negligible. */
			static pthread_mutex_t s_stat_mtx = PTHREAD_MUTEX_INITIALIZER;
			pthread_mutex_lock(&s_stat_mtx);
			ctx->account->ecm_total++;
			if (hit)
			{
				ctx->account->cw_found++;
				ctx->account->cw_time_total_ms += (int64_t)ms;
			}
			else
			{
				ctx->account->cw_not++;
			}
			pthread_mutex_unlock(&s_stat_mtx);
		}
	}
	if (!hit) secure_zero(cw, CW_LEN);
	return res;
}
