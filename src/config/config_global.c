#define MODULE_LOG_PREFIX "conf"
#include "config_internal.h"

void cfg_default_runtime(S_CONFIG *c)
{
    static const uint8_t ncd_key[14] = {
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x10,0x11,0x12,0x13,0x14
    };
    memset(c->readers, 0, sizeof(c->readers));
    c->nreaders = 0;
    c->accounts = NULL;
    c->naccounts = 0;
    c->sock_timeout = 30;
    c->server_keepalive = 20;
    c->server_keepalive_misses = 3;
    c->ecm_log = 1;
    c->scheduled_restart_enabled = 1;
    c->scheduled_restart_minutes = 240;
    c->cccam_port = 12050;
    c->cccam_bindaddr[0] = '\0';
    c->cs378x_port = 0;
    c->cs378x_bindaddr[0] = '\0';
    c->newcamd_port = 15050;
    c->newcamd_bindaddr[0] = '\0';
    memcpy(c->newcamd_key, ncd_key, sizeof(c->newcamd_key));
    c->newcamd_keepalive = 0;
    c->newcamd_mgclient = 0;
    c->webif_enabled = 1;
    c->webif_port = 8080;
    c->webif_refresh = 1;
    c->failban_enabled = 1;
    c->failban_max_fails = BAN_MAX_FAILS;
    c->failban_ban_secs = BAN_SECS;
}

bool cfg_global_cb(const char *sec,const S_KV*k,void*ctx,char*err,size_t esz)
{
    S_CONFIG*c=ctx;long v;int b;
    if(!*sec){snprintf(err,esz,"line %d: key outside section",k->line);return false;}
    if(!strcasecmp(sec,"global")){if(!strcasecmp(k->key,"scheduled_restart")){if(!cfg_parse_bool(k->value,&b))goto bad;c->scheduled_restart_enabled=b;return true;}if(!strcasecmp(k->key,"scheduled_restart_time")){int hh=-1,mm=-1;if(sscanf(k->value,"%d:%d",&hh,&mm)!=2||hh<0||hh>23||mm<0||mm>59||strlen(k->value)!=5||k->value[2] != ':')goto bad;c->scheduled_restart_minutes=hh*60+mm;return true;}if(!strcasecmp(k->key,"socket_timeout")){if(!cfg_parse_long_range(k->value,1,600,&v))goto bad;c->sock_timeout=v;return true;}if(!strcasecmp(k->key,"server_keepalive")){if(!cfg_parse_long_range(k->value,0,3600,&v))goto bad;c->server_keepalive=v;return true;}if(!strcasecmp(k->key,"server_keepalive_misses")){if(!cfg_parse_long_range(k->value,1,20,&v))goto bad;c->server_keepalive_misses=v;return true;}if(!strcasecmp(k->key,"ecm_log")){if(!cfg_parse_bool(k->value,&b))goto bad;c->ecm_log=b;return true;}if(!strcasecmp(k->key,"logfile")){if(strlen(k->value)>=sizeof(c->logfile))goto bad;tcmg_strlcpy(c->logfile,k->value,sizeof(c->logfile));return true;}if(!strcasecmp(k->key,"usrfile")){if(strlen(k->value)>=sizeof(c->usrfile))goto bad;tcmg_strlcpy(c->usrfile,k->value,sizeof(c->usrfile));return true;}goto unknown;}
    if(!strcasecmp(sec,"webif")){if(!strcasecmp(k->key,"enabled")){if(!cfg_parse_bool(k->value,&b))goto bad;c->webif_enabled=b;return true;}if(!strcasecmp(k->key,"port")){if(!cfg_parse_long_range(k->value,0,65535,&v))goto bad;c->webif_port=v;return true;}if(!strcasecmp(k->key,"refresh")){if(!cfg_parse_long_range(k->value,0,3600,&v))goto bad;c->webif_refresh=v;return true;}if(!strcasecmp(k->key,"user")){if(strlen(k->value)>=sizeof(c->webif_user))goto bad;tcmg_strlcpy(c->webif_user,k->value,sizeof(c->webif_user));return true;}if(!strcasecmp(k->key,"password")){if(strlen(k->value)>=sizeof(c->webif_pass))goto bad;tcmg_strlcpy(c->webif_pass,k->value,sizeof(c->webif_pass));return true;}if(!strcasecmp(k->key,"bindaddr")){if(k->value[0]&&!cfg_parse_bindaddr(k->value))goto bad;tcmg_strlcpy(c->webif_bindaddr,k->value,sizeof(c->webif_bindaddr));return true;}goto unknown;}
    if(!strcasecmp(sec,"newcamd")){if(!strcasecmp(k->key,"port")){if(!cfg_parse_long_range(k->value,0,65535,&v))goto bad;c->newcamd_port=v;return true;}if(!strcasecmp(k->key,"bindaddr")){if(k->value[0]&&!cfg_parse_bindaddr(k->value))goto bad;tcmg_strlcpy(c->newcamd_bindaddr,k->value,sizeof(c->newcamd_bindaddr));return true;}if(!strcasecmp(k->key,"key")){if(!cfg_parse_hex_bytes(k->value,c->newcamd_key,14))goto bad;return true;}if(!strcasecmp(k->key,"keepalive")){if(!cfg_parse_bool(k->value,&b))goto bad;c->newcamd_keepalive=b;return true;}if(!strcasecmp(k->key,"mode")){if(strcasecmp(k->value,"auto")&&strcasecmp(k->value,"newcamd")&&strcasecmp(k->value,"mgcamd"))goto bad;c->newcamd_mgclient=!strcasecmp(k->value,"mgcamd");return true;}goto unknown;}
    if(!strcasecmp(sec,"cccam")){if(!strcasecmp(k->key,"port")){if(!cfg_parse_long_range(k->value,0,65535,&v))goto bad;c->cccam_port=v;return true;}if(!strcasecmp(k->key,"bindaddr")){if(k->value[0]&&!cfg_parse_bindaddr(k->value))goto bad;tcmg_strlcpy(c->cccam_bindaddr,k->value,sizeof(c->cccam_bindaddr));return true;}goto unknown;}
    if(!strcasecmp(sec,"cs378x")){if(!strcasecmp(k->key,"port")){if(!cfg_parse_long_range(k->value,0,65535,&v))goto bad;c->cs378x_port=v;return true;}if(!strcasecmp(k->key,"bindaddr")){if(k->value[0]&&!cfg_parse_bindaddr(k->value))goto bad;tcmg_strlcpy(c->cs378x_bindaddr,k->value,sizeof(c->cs378x_bindaddr));return true;}goto unknown;}
    if(!strcasecmp(sec,"failban")){if(!strcasecmp(k->key,"enabled")){if(!cfg_parse_bool(k->value,&b))goto bad;c->failban_enabled=b;return true;}if(!strcasecmp(k->key,"allowlist")){if(strlen(k->value)>=sizeof(c->failban_allowlist))goto bad;tcmg_strlcpy(c->failban_allowlist,k->value,sizeof(c->failban_allowlist));return true;}if(!strcasecmp(k->key,"max_fails")){if(!cfg_parse_long_range(k->value,1,1000,&v))goto bad;c->failban_max_fails=v;return true;}if(!strcasecmp(k->key,"ban_secs")){if(!cfg_parse_long_range(k->value,10,604800,&v))goto bad;c->failban_ban_secs=v;return true;}goto unknown;}
    snprintf(err,esz,"line %d: unknown section [%s]",k->line,sec);return false;
unknown:snprintf(err,esz,"line %d: unknown key '%s' in [%s]",k->line,k->key,sec);return false;
bad:snprintf(err,esz,"line %d: invalid value for '%s'",k->line,k->key);return false;
}

bool cfg_parse_bindaddr(const char *s)
{
    struct in_addr a;
    return s && inet_pton(AF_INET,s,&a)==1;
}

