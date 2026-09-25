#ifndef TCMG_WEBIF_CONSTANTS_H_
#define TCMG_WEBIF_CONSTANTS_H_

#include "../../src/core/constants.h"
#define WEB_SERVER_NAME     "tcmg/" TCMG_VERSION
#define WEB_READ_TIMEOUT_S  4
#define WEB_BUF_SIZE        16384
#define WEB_MAX_LINES_POLL  200
#define WEB_SESSION_TIMEOUT  3600
#define WEB_SESSION_MAX_AGE  86400
#define WEB_SESSION_LEN      32
#define WEB_MAX_SESSIONS     64
#define WEB_POST_MAX         (1024 * 1024)                                          
#define WEB_FILE_VIEW_MAX    (512 * 1024)                                                         

                                                                           
                                                                    
                                                                                 
#define WEB_THEME_INIT_JS \
 "(function(){var r=document.documentElement,p='dark',t;" \
 "try{p=localStorage.getItem('tcmg_theme')||'dark'}catch(e){}" \
 "if(p!=='light')p='dark';t=p;" \
 "r.setAttribute('data-theme',t);r.setAttribute('data-tpref',p)})();"

                                                                            
                                                                          
                                                                               
#define WEB_ACCENT_INIT_JS \
 "(function(){var s={blue:['#147bd1','#106fbe'],purple:['#7c3aed','#6d28d9']," \
 "teal:['#0e9488','#0c7e73'],green:['#16a34a','#128a3e'],rose:['#db2777','#be185d']," \
 "amber:['#d97706','#b96204']},id='blue';" \
 "try{id=localStorage.getItem('tcmg_accent')||'blue'}catch(e){}" \
 "var c=s[id]||s.blue,h=document.documentElement.style;" \
 "function rgba(hex,a){var n=parseInt(hex.slice(1),16);" \
 "return 'rgba('+((n>>16)&255)+','+((n>>8)&255)+','+(n&255)+','+a+')'}" \
 "h.setProperty('--p',c[0]);h.setProperty('--p2',c[1]);" \
 "h.setProperty('--ps',rgba(c[0],.10));h.setProperty('--pg',rgba(c[0],.22))})();"

#endif
