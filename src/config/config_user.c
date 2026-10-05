#define MODULE_LOG_PREFIX "conf"
#include "config_internal.h"
#include "account/account.h"
#include "stats/account_stats.h"
S_ACCOUNT*cfg_account_new(S_CONFIG*cfg)
{
    S_ACCOUNT*a;
    if(!cfg)return NULL;
    a=(S_ACCOUNT*)calloc(1,sizeof(*a));
    if(!a)return NULL;
    a->enabled=1;
    a->caid=0;
    a->group=1;
    a->groups[0]=1;
    a->ngroups=1;
    a->sched_day_from=-1;
    a->as_max_sids=1;
    a->as_max_ecm=0;
    a->as_ecm_window_s=60;
    a->as_channel_timeout_s=15;
    a->as_switch_delay_s=1;
    if(!account_stats_init(&a->stats)){free(a);return NULL;}
    if(pthread_mutex_init(&a->as_mtx,NULL)!=0){account_stats_destroy(&a->stats);free(a);return NULL;}
    atomic_init(&a->active,0);
    atomic_init(&a->refs,0);
    if(!cfg->accounts)cfg->accounts=a;
    else{
        S_ACCOUNT*tail=cfg->accounts;
        while(tail->next)tail=tail->next;
        tail->next=a;
    }
    cfg->naccounts++;
    return a;
}
void cfg_accounts_free(S_CONFIG*cfg)
{
    if(!cfg)return;
    account_list_free(cfg->accounts);
    cfg->accounts=NULL;
    cfg->naccounts=0;
}
static void cfg_account_remove(S_CONFIG*cfg,S_ACCOUNT*target)
{
    S_ACCOUNT*prev=NULL;
    S_ACCOUNT*a;
    if(!cfg||!target)return;
    for(a=cfg->accounts;a;a=a->next){
        if(a!=target){prev=a;continue;}
        if(prev)prev->next=a->next;
        else cfg->accounts=a->next;
        a->next=NULL;
        account_destroy(a);
        if(cfg->naccounts>0)cfg->naccounts--;
        return;
    }
}
bool cfg_parse_users(const char*path,S_CONFIG*c,char*err,size_t esz)
{
    FILE*f=fopen(path,"r");
    char line[4096],sec[64]="",key[128],val[CFGVAL_LEN];
    int ln=0,section_active=0;
    S_ACCOUNT*a=NULL;
    if(!f){
        if(errno==ENOENT)return true;
        snprintf(err,esz,"%s: %s",path,strerror(errno));
        return false;
    }
    while(fgets(line,sizeof(line),f)){
        char*eq;
        ln++;
        if(!strchr(line,'\n')&&!feof(f)){
            fprintf(stderr,"WARNING: %s line %d line too long ignored\n",path,ln);
            section_active=0;
            continue;
        }
        cfg_str_trim(line);
        if(!line[0]||line[0]=='#')continue;
        if(line[0]=='['){
            char*r=strrchr(line,']');
            section_active=0;
            if(!r||r==line+1||r[1]){fprintf(stderr,"WARNING: %s line %d invalid section ignored\n",path,ln);continue;}
            *r='\0';
            if(strlen(line+1)>=sizeof(sec)){fprintf(stderr,"WARNING: %s line %d section name too long ignored\n",path,ln);continue;}
            tcmg_strlcpy(sec,line+1,sizeof(sec));
            if(strcasecmp(sec,"account")){fprintf(stderr,"WARNING: %s line %d unknown section [%s] ignored\n",path,ln,sec);continue;}
            if(a&&!a->user[0])cfg_account_remove(c,a);
            a=cfg_account_new(c);
            if(!a){snprintf(err,esz,"%s:%d: out of memory",path,ln);fclose(f);return false;}
            section_active=1;
            continue;
        }
        if(!section_active||!a){fprintf(stderr,"WARNING: %s line %d setting outside a valid [account] section ignored\n",path,ln);continue;}
        eq=strchr(line,'=');
        if(!eq){fprintf(stderr,"WARNING: %s line %d invalid line in [account] ignored\n",path,ln);continue;}
        *eq='\0';
        cfg_str_trim(line);
        cfg_strip_inline_comment(eq+1);
        if(!line[0]||strlen(line)>=sizeof(key)){fprintf(stderr,"WARNING: %s line %d invalid key in [account] ignored\n",path,ln);continue;}
        if(strlen(eq+1)>=sizeof(val)){fprintf(stderr,"WARNING: %s line %d value too long for '%s' ignored\n",path,ln,line);continue;}
        tcmg_strlcpy(key,line,sizeof(key));
        tcmg_strlcpy(val,eq+1,sizeof(val));
        int32_t v;
        bool b;
        if(!strcasecmp(key,"user")){if(!val[0]||strlen(val)>=sizeof(a->user)){fprintf(stderr,"WARNING: %s line %d invalid value for 'user' ignored\n",path,ln);continue;}tcmg_strlcpy(a->user,val,sizeof(a->user));}
        else if(!strcasecmp(key,"pwd")||!strcasecmp(key,"password")){if(strlen(val)>=sizeof(a->pass)){fprintf(stderr,"WARNING: %s line %d invalid value for '%s' ignored\n",path,ln,key);continue;}tcmg_strlcpy(a->pass,val,sizeof(a->pass));}
        else if(!strcasecmp(key,"enabled")){if(!cfg_parse_bool(val,&b)){fprintf(stderr,"WARNING: %s line %d invalid value for 'enabled' ignored\n",path,ln);continue;}a->enabled=b;}
        else if(!strcasecmp(key,"disabled")){if(!cfg_parse_bool(val,&b)){fprintf(stderr,"WARNING: %s line %d invalid value for 'disabled' ignored\n",path,ln);continue;}a->enabled=!b;}
        else if(!strcasecmp(key,"group")){if(!cfg_parse_group_list(val,a->groups,&a->ngroups)){fprintf(stderr,"WARNING: %s line %d invalid value for 'group' ignored\n",path,ln);continue;}a->group=a->groups[0];}
        else if(!strcasecmp(key,"caid")){uint16_t list[MAX_CAIDS_PER_ACC];int32_t n;if(!cfg_parse_u16_list(val,list,&n,MAX_CAIDS_PER_ACC)){fprintf(stderr,"WARNING: %s line %d invalid value for 'caid' ignored\n",path,ln);continue;}a->caid=n?list[0]:0;a->ncaids=0;for(int i=1;i<n;i++)a->caids[a->ncaids++]=list[i];}
        else if(!strcasecmp(key,"ident")){if(!cfg_parse_ident_list(val,a->idents,&a->nidents,MAX_IDENT_FILTERS)){fprintf(stderr,"WARNING: %s line %d invalid value for 'ident' ignored\n",path,ln);continue;}}
        else if(!strcasecmp(key,"ip_whitelist")){if(!cfg_parse_ipv4_list(val,a->ip_whitelist,&a->nwhitelist)){fprintf(stderr,"WARNING: %s line %d invalid value for 'ip_whitelist' ignored\n",path,ln);continue;}}
        else if(!strcasecmp(key,"sid_whitelist")){if(!cfg_parse_u16_list(val,a->sid_whitelist,&a->nsid_whitelist,MAX_SID_WHITELIST)){fprintf(stderr,"WARNING: %s line %d invalid value for 'sid_whitelist' ignored\n",path,ln);continue;}}
        else if(!strcasecmp(key,"max_connections")){if(!cfg_parse_i32_range(val,0,9999,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'max_connections' ignored\n",path,ln);continue;}a->max_connections=v;}
        else if(!strcasecmp(key,"max_idle")){if(!cfg_parse_i32_range(val,0,86400,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'max_idle' ignored\n",path,ln);continue;}a->max_idle=v;}
        else if(!strcasecmp(key,"expiration")){if(!cfg_parse_date(val,&a->expirationdate)){fprintf(stderr,"WARNING: %s line %d invalid value for 'expiration' ignored\n",path,ln);continue;}}
        else if(!strcasecmp(key,"schedule")){if(!cfg_parse_schedule(val,a)){fprintf(stderr,"WARNING: %s line %d invalid value for 'schedule' ignored\n",path,ln);continue;}}
        else if(!strcasecmp(key,"anti_share")){if(!cfg_parse_bool(val,&b)){fprintf(stderr,"WARNING: %s line %d invalid value for 'anti_share' ignored\n",path,ln);continue;}a->anti_share=b;}
        else if(!strcasecmp(key,"as_max_sids")){if(!cfg_parse_i32_range(val,1,32,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'as_max_sids' ignored\n",path,ln);continue;}a->as_max_sids=v;}
        else if(!strcasecmp(key,"as_max_ecm")){if(!cfg_parse_i32_range(val,0,100000,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'as_max_ecm' ignored\n",path,ln);continue;}a->as_max_ecm=v;}
        else if(!strcasecmp(key,"as_ecm_window_s")){if(!cfg_parse_i32_range(val,1,3600,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'as_ecm_window_s' ignored\n",path,ln);continue;}a->as_ecm_window_s=v;}
        else if(!strcasecmp(key,"as_channel_timeout_s")){if(!cfg_parse_i32_range(val,1,3600,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'as_channel_timeout_s' ignored\n",path,ln);continue;}a->as_channel_timeout_s=v;}
        else if(!strcasecmp(key,"as_switch_delay_s")){if(!cfg_parse_i32_range(val,0,30,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'as_switch_delay_s' ignored\n",path,ln);continue;}a->as_switch_delay_s=v;}
        else if(!strcasecmp(key,"as_max_ecm_min")){if(!cfg_parse_i32_range(val,0,100000,&v)){fprintf(stderr,"WARNING: %s line %d invalid value for 'as_max_ecm_min' ignored\n",path,ln);continue;}a->as_max_ecm=v;a->as_ecm_window_s=60;}
        else if(!strcasecmp(key,"ecmkey")){if(a->nkeys>=MAX_ECMKEYS_PER_ACC){fprintf(stderr,"WARNING: %s line %d too many ecmkey entries ignored\n",path,ln);continue;}S_ECMKEY ek;if(!cfg_parse_ecm_key(val,a->caid,&ek)){fprintf(stderr,"WARNING: %s line %d invalid value for 'ecmkey' ignored\n",path,ln);continue;}bool found=false;for(int i=0;i<a->nkeys;i++)if(a->keys[i].caid==ek.caid){a->keys[i]=ek;found=true;break;}if(!found)a->keys[a->nkeys++]=ek;}
        else fprintf(stderr,"WARNING: %s line %d section [account] contains unknown setting '%s=%s' ignored\n",path,ln,key,val);
    }
    fclose(f);
    if(a&&!a->user[0])cfg_account_remove(c,a);
    return true;
}
