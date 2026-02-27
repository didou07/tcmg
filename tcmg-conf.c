#define MODULE_LOG_PREFIX "conf"
#include "tcmg-globals.h"

const S_CFG_FIELD cfg_server_fields[] =
{
	DEF_OPT_INT32("PORT",           S_CONFIG, port,           15050, 1, 65535),
	DEF_OPT_INT32("SOCKET_TIMEOUT", S_CONFIG, sock_timeout,   30,    5, 600  ),
	DEF_OPT_INT8 ("ECM_LOG",        S_CONFIG, ecm_log,        1              ),
	DEF_OPT_HEX14("DES_KEY",        S_CONFIG, des_key                        ),
	DEF_OPT_STR  ("LOGFILE",        S_CONFIG, logfile,        ""             ),
	DEF_OPT_END
};

const S_CFG_FIELD cfg_webif_fields[] =
{
	DEF_OPT_INT8 ("ENABLED",  S_CONFIG, webif_enabled,  1                ),
	DEF_OPT_INT32("PORT",     S_CONFIG, webif_port,     8080, 1, 65535   ),
	DEF_OPT_INT32("REFRESH",  S_CONFIG, webif_refresh,  30,   0, 3600    ),
	DEF_OPT_STR  ("USER",     S_CONFIG, webif_user,     "admin"          ),
	DEF_OPT_STR  ("PWD",      S_CONFIG, webif_pass,     "admin123"       ),
	DEF_OPT_STR  ("BINDADDR", S_CONFIG, webif_bindaddr, ""               ),
	DEF_OPT_END
};

const S_CFG_FIELD cfg_account_fields[] =
{
	DEF_OPT_STR  ("user",    S_ACCOUNT, user,         ""),
	DEF_OPT_STR  ("pwd",     S_ACCOUNT, pass,         ""),
	DEF_OPT_INT32("group",   S_ACCOUNT, group,        1, 1, 65535),
	DEF_OPT_INT8 ("enabled", S_ACCOUNT, enabled,      1),
	DEF_OPT_INT8 ("fakecw",      S_ACCOUNT, use_fake_cw,     0),
	DEF_OPT_INT32("max_connections", S_ACCOUNT, max_connections, 0, 0, 9999),
	DEF_OPT_INT32("max_idle",        S_ACCOUNT, max_idle,        0, 0, 86400),
	DEF_OPT_DATE ("expiration",  S_ACCOUNT, expirationdate    ),
	DEF_OPT_STR  ("schedule",    S_ACCOUNT, schedule,    ""   ),
	DEF_OPT_END
};

static void str_trim(char *s)
{
	char *p = s, *q;
	while (isspace((unsigned char)*p)) p++;
	if (p != s) memmove(s, p, strlen(p) + 1);
	q = s + strlen(s) - 1;
	while (q >= s && isspace((unsigned char)*q)) *q-- = '\0';
}

static int32_t safe_atoi(const char *v, int32_t def, int32_t lo, int32_t hi)
{
	char *end; long r;
	if (!v || !*v) return def;
	errno = 0;
	r = strtol(v, &end, 10);
	if (errno || end == v || *end) return def;
	if (r < lo) r = lo;
	if (r > hi && lo != hi) r = hi;
	return (int32_t)r;
}

static void field_apply_defaults(const S_CFG_FIELD *tbl, void *base)
{
	const S_CFG_FIELD *f;
	for (f = tbl; f->type != OPT_END; f++)
	{
		char *p = (char *)base + f->offset;
		switch (f->type)
		{
		case OPT_INT32: { int32_t v = f->def_i; memcpy(p, &v, 4); break; }
		case OPT_INT8:  { int8_t  v = (int8_t)f->def_i; *p = v; break; }
		case OPT_STR:   strncpy(p, f->def_s ? f->def_s : "", f->str_max - 1); break;
		case OPT_HEX14: memset(p, 0, 14); break;
		case OPT_DATE:  { time_t z=0; memcpy(p,&z,sizeof(time_t)); } break;
		default: break;
		}
	}
}

static bool field_parse_kv(const S_CFG_FIELD *tbl,
                             const char *key, const char *val, void *base)
{
	const S_CFG_FIELD *f;
	for (f = tbl; f->type != OPT_END; f++)
	{
		if (strcasecmp(key, f->key) != 0) continue;
		char *p = (char *)base + f->offset;
		switch (f->type)
		{
		case OPT_INT32:
		{
			int32_t v = safe_atoi(val, f->def_i, f->lo, f->hi);
			memcpy(p, &v, 4);
			return true;
		}
		case OPT_INT8:
		{
			int8_t v = (int8_t)safe_atoi(val, f->def_i, 0, 1);
			*p = v;
			return true;
		}
		case OPT_STR:
			strncpy(p, val ? val : (f->def_s ? f->def_s : ""), f->str_max - 1);
			return true;
		case OPT_HEX14:
		{
			size_t n = val ? strlen(val) : 0;
			if (n < 28) { memset(p, 0, 14); return true; }
			int i;
			for (i = 0; i < 14; i++)
			{
				unsigned b = 0;
				sscanf(val + i * 2, "%02X", &b);
				p[i] = (uint8_t)b;
			}
			return true;
		}
		case OPT_DATE:
		{
			/* Accept "YYYY-MM-DD" or "0" / "" for never */
			time_t t = 0;
			if (val && strlen(val) >= 10)
			{
				int yr=0, mo=0, dy=0;
				if (sscanf(val, "%d-%d-%d", &yr, &mo, &dy) == 3 && yr > 1970)
				{
					struct tm tm; memset(&tm, 0, sizeof(tm));
					tm.tm_year = yr - 1900; tm.tm_mon = mo - 1; tm.tm_mday = dy;
					t = mktime(&tm);
					if (t < 0) t = 0;
				}
			}
			memcpy(p, &t, sizeof(time_t));
			return true;
		}
		default: break;
		}
	}
	return false;
}

static void field_write(FILE *f, const S_CFG_FIELD *tbl, const void *base)
{
	const S_CFG_FIELD *fl;
	for (fl = tbl; fl->type != OPT_END; fl++)
	{
		const char *p = (const char *)base + fl->offset;
		int pad = 20 - (int)strlen(fl->key);
		if (pad < 1) pad = 1;
		fprintf(f, "%s%*s= ", fl->key, pad, "");
		switch (fl->type)
		{
		case OPT_INT32: { int32_t v; memcpy(&v, p, 4); fprintf(f, "%d\n", v); break; }
		case OPT_INT8:  { int8_t  v = *p;               fprintf(f, "%d\n", (int)v); break; }
		case OPT_STR:   fprintf(f, "%s\n", p); break;
		case OPT_HEX14:
			{ int i; for (i = 0; i < 14; i++) fprintf(f, "%02X", (uint8_t)p[i]); fputc('\n', f); break; }
		case OPT_DATE:
			{ time_t t; memcpy(&t, p, sizeof(time_t));
			  if (t > 0) { struct tm tdm; localtime_r(&t, &tdm);
			    fprintf(f, "%04d-%02d-%02d\n", tdm.tm_year+1900, tdm.tm_mon+1, tdm.tm_mday);
			  } else { fprintf(f, "0\n"); } break; }
		default: break;
		}
	}
}

static void parse_caid_list(const char *v, S_ACCOUNT *a)
{
	char buf[256], *tok, *save;
	strncpy(buf, v, sizeof(buf) - 1);
	a->ncaids = 0;
	tok = strtok_r(buf, ",", &save);
	bool first = true;
	while (tok)
	{
		str_trim(tok);
		unsigned c = 0;
		if (sscanf(tok, "%04X", &c) == 1)
		{
			if (first) { a->caid = (uint16_t)c; first = false; }
			else if (a->ncaids < MAX_CAIDS_PER_ACC)
				a->caids[a->ncaids++] = (uint16_t)c;
		}
		tok = strtok_r(NULL, ",", &save);
	}
}

/* "CAID=KEY0KEY1" or just "KEY0KEY1" */
static bool parse_ecmkey(const char *v, uint16_t def_caid, S_ECMKEY *out)
{
	const char *kh = v;
	out->caid = def_caid;
	if (strlen(v) > 5 && v[4] == '=')
	{
		unsigned c = 0;
		sscanf(v, "%04X", &c);
		out->caid = (uint16_t)c;
		kh = v + 5;
	}
	if (strlen(kh) != 64) return false;
	int i;
	for (i = 0; i < 16; i++) { unsigned b=0; sscanf(kh+i*2,      "%02X",&b); out->key0[i]=(uint8_t)b; }
	for (i = 0; i < 16; i++) { unsigned b=0; sscanf(kh+32+i*2,   "%02X",&b); out->key1[i]=(uint8_t)b; }
	return true;
}

/* Parse schedule "MON-FRI 08:00-22:00" into S_ACCOUNT fields.
 * Day names: MON=0 TUE=1 WED=2 THU=3 FRI=4 SAT=5 SUN=6
 * On parse failure leaves sched_day_from = -1 (always allow). */
static void parse_schedule(const char *v, S_ACCOUNT *a)
{
	static const char *daynames[] = { "MON","TUE","WED","THU","FRI","SAT","SUN" };
	a->sched_day_from = -1;  /* sentinel: not set */
	if (!v || !*v) return;

	char buf[64];
	strncpy(buf, v, sizeof(buf) - 1);
	/* Expect: "DAY[-DAY] HH:MM-HH:MM" */
	char *space = strchr(buf, ' ');
	if (!space) return;
	*space = '\0';
	const char *daypart  = buf;
	const char *timepart = space + 1;

	/* Parse day range */
	char d1[4] = "", d2[4] = "";
	char *dash = strchr(daypart, '-');
	if (dash) {
		strncpy(d1, daypart, (size_t)(dash - daypart) < 3 ? (size_t)(dash - daypart) : 3);
		strncpy(d2, dash + 1, 3);
	} else {
		strncpy(d1, daypart, 3);
		strncpy(d2, daypart, 3);
	}
	int from = -1, to = -1;
	for (int i = 0; i < 7; i++) {
		if (strcasecmp(d1, daynames[i]) == 0) from = i;
		if (strcasecmp(d2, daynames[i]) == 0) to   = i;
	}
	if (from < 0 || to < 0) return;

	/* Parse time range "HH:MM-HH:MM" */
	int h1 = 0, m1 = 0, h2 = 0, m2 = 0;
	if (sscanf(timepart, "%d:%d-%d:%d", &h1, &m1, &h2, &m2) != 4) return;

	a->sched_day_from  = (int8_t)from;
	a->sched_day_to    = (int8_t)to;
	a->sched_hhmm_from = (int16_t)(h1 * 100 + m1);
	a->sched_hhmm_to   = (int16_t)(h2 * 100 + m2);
}

/* Parse sid_whitelist "0064,00C8,1234" comma-separated hex SIDs */
static void parse_sid_whitelist(const char *v, S_ACCOUNT *a)
{
	char buf[512], *tok, *save;
	strncpy(buf, v, sizeof(buf) - 1);
	a->nsid_whitelist = 0;
	tok = strtok_r(buf, ",", &save);
	while (tok && a->nsid_whitelist < MAX_SID_WHITELIST)
	{
		str_trim(tok);
		unsigned s = 0;
		if (sscanf(tok, "%04X", &s) == 1)
			a->sid_whitelist[a->nsid_whitelist++] = (uint16_t)s;
		tok = strtok_r(NULL, ",", &save);
	}
}

S_ACCOUNT *cfg_account_new(S_CONFIG *cfg)
{
	S_ACCOUNT *a = (S_ACCOUNT *)calloc(1, sizeof(S_ACCOUNT));
	if (!a) return NULL;
	field_apply_defaults(cfg_account_fields, a);
	a->caid          = 0x0B00;
	a->sched_day_from = -1;   /* schedule not set */
	/* Append to tail */
	if (!cfg->accounts)
		cfg->accounts = a;
	else
	{
		S_ACCOUNT *tail = cfg->accounts;
		while (tail->next) tail = tail->next;
		tail->next = a;
	}
	cfg->naccounts++;
	return a;
}

void cfg_accounts_free(S_CONFIG *cfg)
{
	S_ACCOUNT *a = cfg->accounts;
	while (a)
	{
		S_ACCOUNT *next = a->next;
		secure_zero(a, sizeof(*a));
		free(a);
		a = next;
	}
	cfg->accounts  = NULL;
	cfg->naccounts = 0;
}

bool cfg_load(const char *file, S_CONFIG *cfg)
{
	FILE *f = fopen(file, "r");
	if (!f) return false;

	strncpy(cfg->config_file, file, CFGPATH_LEN - 1);
	field_apply_defaults(cfg_server_fields, cfg);
	field_apply_defaults(cfg_webif_fields,  cfg);

	enum { SEC_NONE, SEC_SERVER, SEC_WEBIF, SEC_ACCOUNT } sec = SEC_NONE;
	S_ACCOUNT *acc = NULL;
	char line[CFGVAL_LEN * 2];

	while (fgets(line, sizeof(line), f))
	{
		str_trim(line);
		if (!line[0] || line[0] == '#') continue;

		if (strcmp(line, "[server]")  == 0) { sec = SEC_SERVER;  acc = NULL; continue; }
		if (strcmp(line, "[webif]")   == 0) { sec = SEC_WEBIF;   acc = NULL; continue; }
		if (strcmp(line, "[account]") == 0)
		{
			sec = SEC_ACCOUNT;
			acc = cfg_account_new(cfg);
			continue;
		}

		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		char *k = line, *v = eq + 1;
		/* strip inline comments: VALUE = something  # comment */
		char *comment = strchr(v, '#');
		if (comment) *comment = '\0';
		str_trim(k); str_trim(v);

		switch (sec)
		{
		case SEC_SERVER:
			if (!field_parse_kv(cfg_server_fields, k, v, cfg))
				tcmg_log("unknown [server] key: %s", k);
			break;
		case SEC_WEBIF:
			if (!field_parse_kv(cfg_webif_fields, k, v, cfg))
				tcmg_log("unknown [webif] key: %s", k);
			break;
		case SEC_ACCOUNT:
			if (!acc) break;
			if (field_parse_kv(cfg_account_fields, k, v, acc))
			{
				/* Re-parse schedule whenever the key is updated */
				if (strcasecmp(k, "schedule") == 0)
					parse_schedule(acc->schedule, acc);
				break;
			}
			if (strcasecmp(k, "caid") == 0)
			{
				if (strchr(v, ','))
					parse_caid_list(v, acc);
				else
				{
					unsigned c = 0;
					if (sscanf(v, "%04X", &c) == 1) acc->caid = (uint16_t)c;
				}
			}
			else if (strcasecmp(k, "ip_whitelist") == 0)
			{
				char tmp[512], *tok, *save;
				strncpy(tmp, v, sizeof(tmp) - 1);
				tok = strtok_r(tmp, ",", &save);
				while (tok && acc->nwhitelist < MAX_IP_WHITELIST)
				{
					str_trim(tok);
					if (*tok) strncpy(acc->ip_whitelist[acc->nwhitelist++], tok, MAXIPLEN - 1);
					tok = strtok_r(NULL, ",", &save);
				}
			}
			else if (strcasecmp(k, "sid_whitelist") == 0)
			{
				parse_sid_whitelist(v, acc);
			}
			else if (strcasecmp(k, "ecmkey") == 0)
			{
				if (acc->nkeys < MAX_ECMKEYS_PER_ACC)
				{
					S_ECMKEY ek = {0};
					if (parse_ecmkey(v, acc->caid, &ek))
					{
						/* Replace existing key for same CAID */
						int i; bool found = false;
						for (i = 0; i < acc->nkeys; i++)
							if (acc->keys[i].caid == ek.caid)
							{ acc->keys[i] = ek; found = true; break; }
						if (!found) acc->keys[acc->nkeys++] = ek;
					}
				}
			}
			break;
		default: break;
		}
	}
	fclose(f);
	return true;
}

bool cfg_save(const S_CONFIG *cfg)
{
	if (!cfg->config_file[0]) return false;

	char tmppath[CFGPATH_LEN + 4];
	snprintf(tmppath, sizeof(tmppath), "%s.tmp", cfg->config_file);

	/* Step 1: write everything to .tmp */
	FILE *f = fopen(tmppath, "w");
	if (!f)
	{
		tcmg_log("cannot create %s (errno=%d %s)", tmppath, errno, strerror(errno));
		return false;
	}

	time_t now = time(NULL); struct tm ti; char ts[32];
	localtime_r(&now, &ti);
	strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &ti);
	fprintf(f, "# tcmg config -- saved %s\n\n", ts);

	fprintf(f, "[server]\n"); field_write(f, cfg_server_fields, cfg); fputc('\n', f);
	fprintf(f, "[webif]\n");  field_write(f, cfg_webif_fields,  cfg); fputc('\n', f);

	pthread_rwlock_rdlock((pthread_rwlock_t *)&cfg->acc_lock);
	{
		const S_ACCOUNT *a;
		for (a = cfg->accounts; a; a = a->next)
		{
			int i;
			fprintf(f, "[account]\n");
			field_write(f, cfg_account_fields, a);

			fprintf(f, "caid                = %04X", a->caid);
			for (i = 0; i < a->ncaids; i++) fprintf(f, ",%04X", a->caids[i]);
			fputc('\n', f);

			if (a->nwhitelist > 0)
			{
				fprintf(f, "ip_whitelist        = ");
				for (i = 0; i < a->nwhitelist; i++)
				{
					if (i) fputc(',', f);
					fputs(a->ip_whitelist[i], f);
				}
				fputc('\n', f);
			}

			if (a->nsid_whitelist > 0)
			{
				fprintf(f, "sid_whitelist       = ");
				for (i = 0; i < a->nsid_whitelist; i++)
				{
					if (i) fputc(',', f);
					fprintf(f, "%04X", a->sid_whitelist[i]);
				}
				fputc('\n', f);
			}

			for (i = 0; i < a->nkeys; i++)
			{
				char hex[65]; int j;
				for (j = 0; j < 16; j++) snprintf(hex + j*2,    3, "%02X", a->keys[i].key0[j]);
				for (j = 0; j < 16; j++) snprintf(hex + 32+j*2, 3, "%02X", a->keys[i].key1[j]);
				hex[64] = '\0';
				fprintf(f, "ecmkey              = %04X=%s\n", a->keys[i].caid, hex);
			}
			fputc('\n', f);
		}
	}
	pthread_rwlock_unlock((pthread_rwlock_t *)&cfg->acc_lock);
	fclose(f);

	/* Step 2: copy .tmp → final file
	 * Use fopen(dest,"w") like OSCam's file_copy -- rename() fails on Windows
	 * when the destination already exists.                                   */
	FILE *src = fopen(tmppath, "r");
	if (!src)
	{
		tcmg_log("cannot re-open %s (errno=%d %s)", tmppath, errno, strerror(errno));
		remove(tmppath);
		return false;
	}
	FILE *dst = fopen(cfg->config_file, "w");
	if (!dst)
	{
		tcmg_log("cannot write %s (errno=%d %s)", cfg->config_file, errno, strerror(errno));
		fclose(src);
		remove(tmppath);
		return false;
	}
	char copybuf[4096]; size_t n;
	while ((n = fread(copybuf, 1, sizeof(copybuf), src)) > 0)
		fwrite(copybuf, 1, n, dst);
	fclose(src);
	fclose(dst);
	remove(tmppath);

	tcmg_log("saved %s", cfg->config_file);
	return true;
}


bool cfg_write_default(const char *path)
{
	FILE *f = fopen(path, "w");
	if (!f)
	{
		tcmg_log("cfg_write_default: cannot create %s (errno=%d %s)",
		         path, errno, strerror(errno));
		return false;
	}

	fprintf(f,
	"# tcmg -- default configuration\n"
	"# Generated automatically. Edit and restart to apply changes.\n"
	"\n"
	"[server]\n"
	"PORT                = 15050          # Listening port for card-sharing clients\n"
	"DES_KEY             = 0102030405060708091011121314  # 14-byte DES key (28 hex chars)\n"
	"SOCKET_TIMEOUT      = 30             # Client socket timeout in seconds (5-600)\n"
	"ECM_LOG             = 1              # Log ECM requests: 1=on 0=off\n"
	"# LOGFILE           = /var/log/tcmg.log   # Log to file (empty = stdout only; rotates at 10 MB)\n"
	"\n"
	"[webif]\n"
	"ENABLED             = 1              # Enable web interface: 1=on 0=off\n"
	"PORT                = 8080           # Web interface port\n"
	"USER                = admin          # Web interface username (empty = no auth)\n"
	"PWD                 = admin123       # Web interface password\n"
	"BINDADDR            =                # Bind address (empty = all interfaces)\n"
	"# REFRESH           = 30             # Auto-refresh status page every N seconds (0=off)\n"
	"\n"
	"# ── Accounts ──────────────────────────────────────────────────────────────\n"
	"# Each [account] block defines one client. All commented keys are optional.\n"
	"\n"
	"[account]\n"
	"user                = tvcas          # Login username\n"
	"pwd                 = 1234           # Login password\n"
	"group               = 1             # Group number (1-65535)\n"
	"enabled             = 1             # 1=active  0=disabled\n"
	"fakecw              = 0             # Send fake CW instead of real: 1=on 0=off\n"
	"caid                = 0B00,0B01     # Allowed CAIDs (comma-separated hex)\n"
	"ecmkey              = 0B00=9F3C17A2B5D0481E6A7B92F4C8E05D13A1B9E4F276C3058D4ACF19B08273DE5F\n"
	"ecmkey              = 0B01=A9688E271BA149BE1D3A1D84BC2BD1E920626B61C8CBB5CDBA361F44FAF750D6\n"
	"# max_connections   = 2             # Max simultaneous logins (0=unlimited)\n"
	"# max_idle          = 120           # Kick after N seconds with no ECM (0=off)\n"
	"# expiration        = 2026-12-31    # Account expiry date in YYYY-MM-DD (0=never)\n"
	"# schedule          = MON-FRI 08:00-22:00  # Allowed timeframe (empty=always)\n"
	"# sid_whitelist     = 0064,00C8,1234        # Allowed Service IDs (empty=all)\n"
	"# ip_whitelist      = 192.168.1.0,10.0.0.1 # Allowed source IPs (empty=all)\n"
	"\n"
	"[account]\n"
	"user                = test\n"
	"pwd                 = 1234\n"
	"group               = 1\n"
	"enabled             = 1\n"
	"fakecw              = 1\n"
	"max_connections     = 0\n"
	"max_idle            = 0\n"
	"expiration          = 0\n"
	"schedule            =\n"
	"caid                = 0604\n"
	);

	fclose(f);
	tcmg_log("created default config: %s", path);
	return true;
}

bool cfg_reload(const char *file, char *errbuf, size_t errsz)
{
	FILE *t;
	if (!file || !*file) { snprintf(errbuf, errsz, "empty path"); return false; }
	t = fopen(file, "r");
	if (!t) { snprintf(errbuf, errsz, "file not found: %s", file); return false; }
	fclose(t);

	S_CONFIG ncfg;
	memset(&ncfg, 0, sizeof(ncfg));
	pthread_rwlock_init(&ncfg.acc_lock, NULL);
	pthread_mutex_init(&ncfg.ban_lock, NULL);

	if (!cfg_load(file, &ncfg))
	{
		snprintf(errbuf, errsz, "parse error: %s", file);
		cfg_accounts_free(&ncfg);
		return false;
	}

	/* Preserve live webif state (running server stays on same port/bind) */
	ncfg.webif_enabled  = g_cfg.webif_enabled;
	ncfg.webif_port     = g_cfg.webif_port;
	strncpy(ncfg.webif_bindaddr, g_cfg.webif_bindaddr, MAXIPLEN - 1);

	pthread_rwlock_wrlock(&g_cfg.acc_lock);
	cfg_accounts_free(&g_cfg);
	g_cfg.accounts    = ncfg.accounts;  ncfg.accounts  = NULL;
	g_cfg.naccounts   = ncfg.naccounts;
	g_cfg.port        = ncfg.port;
	g_cfg.sock_timeout= ncfg.sock_timeout;
	memcpy(g_cfg.des_key, ncfg.des_key, 14);
	g_cfg.ecm_log     = ncfg.ecm_log;
	g_cfg.webif_refresh = ncfg.webif_refresh;
	strncpy(g_cfg.logfile,    ncfg.logfile,    CFGPATH_LEN - 1);
	strncpy(g_cfg.webif_user, ncfg.webif_user, CFGKEY_LEN - 1);
	strncpy(g_cfg.webif_pass, ncfg.webif_pass, CFGKEY_LEN - 1);
	strncpy(g_cfg.config_file, file, CFGPATH_LEN - 1);
	pthread_rwlock_unlock(&g_cfg.acc_lock);

	log_ecm_set(g_cfg.ecm_log);
	/* Re-open log file if it changed */
	log_set_file(g_cfg.logfile[0] ? g_cfg.logfile : NULL);
	tcmg_log("reloaded: %s (%d accounts)", file, g_cfg.naccounts);
	return true;
}

S_ACCOUNT *cfg_find_account(const char *user)
{
	S_ACCOUNT *a;
	for (a = g_cfg.accounts; a; a = a->next)
		if (strcmp(a->user, user) == 0)
			return a;
	return NULL;
}

void cfg_print(const S_CONFIG *cfg)
{
	int32_t dis = 0;
	const S_ACCOUNT *a;
	for (a = cfg->accounts; a; a = a->next)
	{
		if (!a->enabled) dis++;
		tcmg_log_dbg(D_CONF, "account: user=%-16s caid=%04X enabled=%d fakecw=%d",
		             a->user, a->caid, a->enabled, a->use_fake_cw);
	}
	tcmg_log("loaded %d account(s) (%d disabled)", cfg->naccounts, dis);
}

const char *cfg_client_name(uint16_t id)
{
	static const struct { uint16_t id; const char *name; } tbl[] =
	{
		{0x0665,"rq-sssp-CS"}, {0x0666,"rqcamd"},   {0x414C,"AlexCS"},
		{0x4333,"camd3"},      {0x4343,"CCcam"},     {0x4453,"DiabloCam"},
		{0x4543,"eyetvCamd"}, {0x4765,"Octagon"},    {0x6502,"Tvheadend"},
		{0x6576,"evocamd"},   {0x6D63,"mpcs"},        {0x6D67,"mgcamd"},
		{0x6E73,"NewCS"},     {0x7264,"radegast"},    {0x7363,"Scam"},
		{0x7878,"tsdecrypt"}, {0x8888,"oscam"},       {0x9911,"ACamd"},
		{0, NULL}
	};
	int i;
	for (i = 0; tbl[i].name; i++)
		if (tbl[i].id == id) return tbl[i].name;
	return "unknown";
}
