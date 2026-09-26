#define MODULE_LOG_PREFIX "conf"
#include "config_internal.h"
#include "stats/account_stats.h"

static void free_account_list(S_ACCOUNT *head)
{
    while (head) {
        S_ACCOUNT *next = head->next;
        account_stats_destroy(&head->stats);
        pthread_mutex_destroy(&head->as_mtx);
        secure_zero(head, sizeof(*head));
        free(head);
        head = next;
    }
}
S_ACCOUNT *cfg_account_new(S_CONFIG *cfg)
{
    S_ACCOUNT *a;
    if (!cfg) return NULL;
    a = (S_ACCOUNT *)calloc(1, sizeof(*a));
    if (!a) return NULL;
    a->enabled = 1;
    a->caid = 0;
    a->group = 1;
    a->groups[0] = 1;
    a->ngroups = 1;
    a->sched_day_from = -1;
    a->as_max_sids = 1;
    a->as_max_ecm = 0;
    a->as_ecm_window_s = 60;
    a->as_channel_timeout_s = 15;
    a->as_switch_delay_s = 1;
    if (!account_stats_init(&a->stats)) {
        free(a);
        return NULL;
    }
    if (pthread_mutex_init(&a->as_mtx, NULL) != 0) {
        account_stats_destroy(&a->stats);
        free(a);
        return NULL;
    }
    atomic_init(&a->active, 0);
    atomic_init(&a->refs, 0);

    if (!cfg->accounts) cfg->accounts = a;
    else {
        S_ACCOUNT *tail = cfg->accounts;
        while (tail->next) tail = tail->next;
        tail->next = a;
    }
    cfg->naccounts++;
    return a;
}
void cfg_accounts_free(S_CONFIG *cfg)
{
    if (!cfg) return;
    free_account_list(cfg->accounts);
    cfg->accounts = NULL;
    cfg->naccounts = 0;
}
bool cfg_parse_users(const char *path,S_CONFIG*c,char*err,size_t esz)
{
    FILE*f=fopen(path,"r");char line[4096],sec[64]="";char key[128],val[CFGVAL_LEN];int ln=0;S_ACCOUNT*a=NULL;
    if(!f){if(errno==ENOENT)return true;snprintf(err,esz,"%s: %s",path,strerror(errno));return false;}
    while(fgets(line,sizeof(line),f)){
        ln++;if(!strchr(line,'\n')&&!feof(f)){snprintf(err,esz,"%s:%d: line too long",path,ln);fclose(f);return false;}cfg_str_trim(line);if(!line[0]||line[0]=='#')continue;
        if(line[0]=='['){char*r=strrchr(line,']');if(!r||r==line+1||r[1])goto badline;*r='\0';if(strlen(line+1)>=sizeof(sec))goto badline;tcmg_strlcpy(sec,line+1,sizeof(sec));if(strcasecmp(sec,"account")){snprintf(err,esz,"%s:%d: only [account] sections are allowed",path,ln);fclose(f);return false;}a=cfg_account_new(c);if(!a){snprintf(err,esz,"%s:%d: out of memory",path,ln);fclose(f);return false;}continue;}
        if(!a){snprintf(err,esz,"%s:%d: key before [account]",path,ln);fclose(f);return false;}char*eq=strchr(line,'=');if(!eq)goto badline;*eq='\0';cfg_str_trim(line);cfg_strip_inline_comment(eq+1);if(!line[0]||strlen(line)>=sizeof(key)||strlen(eq+1)>=sizeof(val))goto badline;tcmg_strlcpy(key,line,sizeof(key));tcmg_strlcpy(val,eq+1,sizeof(val));long v;int b;
        if(!strcasecmp(key,"user")){if(!val[0]||strlen(val)>=sizeof(a->user))goto badval;tcmg_strlcpy(a->user,val,sizeof(a->user));}
        else if(!strcasecmp(key,"pwd")||!strcasecmp(key,"password")){if(strlen(val)>=sizeof(a->pass))goto badval;tcmg_strlcpy(a->pass,val,sizeof(a->pass));}
        else if(!strcasecmp(key,"enabled")){if(!cfg_parse_bool(val,&b))goto badval;a->enabled=b;}
        else if(!strcasecmp(key,"disabled")){if(!cfg_parse_bool(val,&b))goto badval;a->enabled=!b;}
        else if(!strcasecmp(key,"group")){if(!cfg_parse_group_list(val,a->groups,&a->ngroups))goto badval;a->group=a->groups[0];}
        else if(!strcasecmp(key,"caid")){uint16_t list[MAX_CAIDS_PER_ACC];int32_t n;if(!cfg_parse_u16_list(val,list,&n,MAX_CAIDS_PER_ACC))goto badval;a->caid=n?list[0]:0;a->ncaids=0;for(int i=1;i<n;i++)a->caids[a->ncaids++]=list[i];}
        else if(!strcasecmp(key,"ip_whitelist")){if(!cfg_parse_ipv4_list(val,a->ip_whitelist,&a->nwhitelist))goto badval;}
        else if(!strcasecmp(key,"sid_whitelist")){if(!cfg_parse_u16_list(val,a->sid_whitelist,&a->nsid_whitelist,MAX_SID_WHITELIST))goto badval;}
        else if(!strcasecmp(key,"max_connections")){if(!cfg_parse_long_range(val,0,9999,&v))goto badval;a->max_connections=v;}
        else if(!strcasecmp(key,"max_idle")){if(!cfg_parse_long_range(val,0,86400,&v))goto badval;a->max_idle=v;}
        else if(!strcasecmp(key,"expiration")){if(!cfg_parse_date(val,&a->expirationdate))goto badval;}
        else if(!strcasecmp(key,"schedule")){if(!cfg_parse_schedule(val,a))goto badval;}
        else if(!strcasecmp(key,"anti_share")){if(!cfg_parse_bool(val,&b))goto badval;a->anti_share=b;}
        else if(!strcasecmp(key,"as_max_sids")){if(!cfg_parse_long_range(val,1,32,&v))goto badval;a->as_max_sids=v;}
        else if(!strcasecmp(key,"as_max_ecm")){if(!cfg_parse_long_range(val,0,100000,&v))goto badval;a->as_max_ecm=v;}
        else if(!strcasecmp(key,"as_ecm_window_s")){if(!cfg_parse_long_range(val,1,3600,&v))goto badval;a->as_ecm_window_s=v;}
        else if(!strcasecmp(key,"as_channel_timeout_s")){if(!cfg_parse_long_range(val,1,3600,&v))goto badval;a->as_channel_timeout_s=v;}
        else if(!strcasecmp(key,"as_switch_delay_s")){if(!cfg_parse_long_range(val,0,30,&v))goto badval;a->as_switch_delay_s=v;}
        else if(!strcasecmp(key,"as_max_ecm_min")){if(!cfg_parse_long_range(val,0,100000,&v))goto badval;a->as_max_ecm=v;a->as_ecm_window_s=60;}
        else if(!strcasecmp(key,"ecmkey")){if(a->nkeys>=MAX_ECMKEYS_PER_ACC)goto badval;S_ECMKEY ek;if(!cfg_parse_ecm_key(val,a->caid,&ek))goto badval;bool found=false;for(int i=0;i<a->nkeys;i++)if(a->keys[i].caid==ek.caid){a->keys[i]=ek;found=true;break;}if(!found)a->keys[a->nkeys++]=ek;}
        else goto badkey;
    }
    fclose(f);
    for(a=c->accounts;a;a=a->next){if(!a->user[0]){snprintf(err,esz,"%s: account without user",path);return false;}}
    return true;
badkey:snprintf(err,esz,"%s:%d: unknown key '%s'",path,ln,key);fclose(f);return false;
badval:snprintf(err,esz,"%s:%d: invalid value for '%s'",path,ln,key);fclose(f);return false;
badline:snprintf(err,esz,"%s:%d: invalid line",path,ln);fclose(f);return false;
}
