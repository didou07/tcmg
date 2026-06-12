#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

s_session       s_sessions[WEB_MAX_SESSIONS];
pthread_mutex_t s_sess_lock = PTHREAD_MUTEX_INITIALIZER;

void session_gen_token(char *out)
{
	uint8_t rnd[16];
	csprng(rnd, sizeof(rnd));
	for (int i = 0; i < 16; i++)
		snprintf(out + i * 2, 3, "%02x", rnd[i]);
	out[WEB_SESSION_LEN] = '\0';
}

void session_create(char *token_out)
{
	session_gen_token(token_out);
	time_t now = time(NULL);
	pthread_mutex_lock(&s_sess_lock);
	int    slot   = 0;
	time_t oldest = s_sessions[0].expires;
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires <= now) { slot = i; break; }
		if (s_sessions[i].expires < oldest) { oldest = s_sessions[i].expires; slot = i; }
	}
	tcmg_strlcpy(s_sessions[slot].token, token_out, WEB_SESSION_LEN + 1);
	s_sessions[slot].expires   = now + WEB_SESSION_TIMEOUT;
	s_sessions[slot].issued_at = now;
	pthread_mutex_unlock(&s_sess_lock);
}

int session_check(const char *token)
{
	if (!token || strlen(token) != WEB_SESSION_LEN) return 0;
	time_t now = time(NULL);
	int    ok  = 0;
	pthread_mutex_lock(&s_sess_lock);
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (s_sessions[i].expires <= now) continue;
		if (!ct_streq(s_sessions[i].token, token)) continue;
		if (now - s_sessions[i].issued_at > WEB_SESSION_MAX_AGE) break;
		s_sessions[i].expires = now + WEB_SESSION_TIMEOUT;
		ok = 1;
		break;
	}
	pthread_mutex_unlock(&s_sess_lock);
	return ok;
}

const char *cookie_get_session(const char *cookie_hdr, char *buf, int bufsz)
{
	if (!cookie_hdr) return NULL;
	const char *key = "tcmg_session=";
	const char *p   = strstr(cookie_hdr, key);
	if (!p) return NULL;
	p += strlen(key);
	int i = 0;
	while (i < bufsz - 1 && p[i] && p[i] != ';' && p[i] != '\r' && p[i] != '\n') {
		buf[i] = p[i];
		i++;
	}
	buf[i] = '\0';
	return i == WEB_SESSION_LEN ? buf : NULL;
}

void session_invalidate(const char *token)
{
	if (!token || !*token) return;
	pthread_mutex_lock(&s_sess_lock);
	for (int i = 0; i < WEB_MAX_SESSIONS; i++) {
		if (ct_streq(s_sessions[i].token, token)) {
			memset(s_sessions[i].token, 0, WEB_SESSION_LEN + 1);
			s_sessions[i].expires   = 0;
			s_sessions[i].issued_at = 0;
			break;
		}
	}
	pthread_mutex_unlock(&s_sess_lock);
}

static const char B64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void b64_encode(const char *in, int ilen, char *out, int outsz)
{
	const uint8_t *s = (const uint8_t *)in;
	int i = 0, o = 0;
	while (i < ilen && o + 4 < outsz) {
		int      rem = ilen - i;
		uint32_t v   = ((uint32_t)s[i] << 16)
		             | (rem > 1 ? (uint32_t)s[i+1] << 8 : 0)
		             | (rem > 2 ? (uint32_t)s[i+2]      : 0);
		out[o++] = B64[(v >> 18) & 0x3F];
		out[o++] = B64[(v >> 12) & 0x3F];
		out[o++] = rem > 1 ? B64[(v >>  6) & 0x3F] : '=';
		out[o++] = rem > 2 ? B64[ v        & 0x3F] : '=';
		i += 3;
	}
	out[o] = '\0';
}

int check_auth(const char *auth_header)
{
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) return 1;
	if (!auth_header) return 0;
	const char *p = strstr(auth_header, "Basic ");
	if (!p) return 0;
	p += 6;
	char got[512]; int glen = 0;
	while (*p && *p != '\r' && *p != '\n' && *p != ' ' && glen < (int)sizeof(got) - 1)
		got[glen++] = *p++;
	got[glen] = '\0';
	char creds[256];
	snprintf(creds, sizeof(creds), "%s:%s", g_cfg.webif_user, g_cfg.webif_pass);
	char expected[512];
	b64_encode(creds, (int)strlen(creds), expected, sizeof(expected));
	return ct_streq(got, expected) ? 1 : 0;
}

int check_credentials(const char *user, const char *pass)
{
	if (!g_cfg.webif_user[0] && !g_cfg.webif_pass[0]) return 1;
	if (!user || !pass) return 0;
	return (ct_streq(user, g_cfg.webif_user) &&
	        ct_streq(pass, g_cfg.webif_pass)) ? 1 : 0;
}
