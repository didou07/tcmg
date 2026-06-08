#define MODULE_LOG_PREFIX "webif"
#include "../globals.h"
#include "webif-internal.h"
#include "webif_assets.h"

#define ICO_LOGO \
 "<svg width='16' height='16' viewBox='0 0 24 24' fill='none'>" \
 "<path d='M12 2L2 7l10 5 10-5-10-5z' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>" \
 "<path d='M2 17l10 5 10-5' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>" \
 "<path d='M2 12l10 5 10-5' stroke='var(--cy)' stroke-width='1.8' stroke-linejoin='round'/>" \
 "</svg>"

#define ICO_MENU \
 "<svg width='16' height='16' viewBox='0 0 24 24' fill='none'" \
 " stroke='currentColor' stroke-width='1.8'>" \
 "<line x1='3' y1='6' x2='21' y2='6'/>" \
 "<line x1='3' y1='12' x2='21' y2='12'/>" \
 "<line x1='3' y1='18' x2='21' y2='18'/></svg>"

int emit_header(char **buf, int *bsz, int pos,
                const char *title, const char *active)
{

    const char *nav_active = active;
    if (strcmp(active, "restart")  == 0 ||
        strcmp(active, "shutdown") == 0)
        nav_active = "power";

    pos = buf_printf(buf, bsz, pos,
        "<!DOCTYPE html><html lang='en'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TCMG &mdash; %s</title>"
        "<style>%s</style>"
        "</head><body class='pg-%s'>",
        title, TCMG_CSS, active);

    char srv_addr[64];
    snprintf(srv_addr, sizeof(srv_addr), "%s:%d",
             g_cfg.webif_bindaddr[0] ? g_cfg.webif_bindaddr : "0.0.0.0",
             g_cfg.webif_port);

    pos = buf_printf(buf, bsz, pos,
        "<nav id='tb'>"
        "<div class='lo'>"
        "  <div class='li'>" ICO_LOGO "</div>"
        "  <span class='lt'>TCMG</span>"
        "  <span class='lv'>" TCMG_VERSION "</span>"
        "</div>"
        "<button id='mnuBtn' "
        "onclick='document.querySelector(\".tnav\").classList.toggle(\"open\")'>"
        ICO_MENU
        "</button>"
        "<div class='tnav'>");

    typedef struct { int sep; const char *id, *href, *icon, *label; } t_nav;

    static const char s_ico_status[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<polyline points='22 12 18 12 15 21 9 3 6 12 2 12'/></svg>";
    static const char s_ico_log[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"
        "<polyline points='14 2 14 8 20 8'/>"
        "<line x1='8' y1='13' x2='16' y2='13'/><line x1='8' y1='17' x2='16' y2='17'/></svg>";
    static const char s_ico_users[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2'/>"
        "<circle cx='9' cy='7' r='4'/>"
        "<path d='M23 21v-2a4 4 0 0 0-3-3.87'/><path d='M16 3.13a4 4 0 0 1 0 7.75'/></svg>";
    static const char s_ico_ban[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<circle cx='12' cy='12' r='10'/>"
        "<line x1='4.93' y1='4.93' x2='19.07' y2='19.07'/></svg>";
    static const char s_ico_cfg[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<circle cx='12' cy='12' r='3'/>"
        "<path d='M19.07 4.93a10 10 0 0 1 0 14.14M4.93 4.93a10 10 0 0 0 0 14.14'/></svg>";
    static const char s_ico_power[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/>"
        "<line x1='12' y1='2' x2='12' y2='12'/></svg>";
    static const char s_ico_tvcas[] =
        "<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
        "<path d='M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z'/></svg>";

    static const t_nav nav[] = {
        {0, "status",  "/status",  s_ico_status, "Dashboard"},
        {0, "livelog", "/livelog", s_ico_log,    "Live Log"},
        {1, NULL, NULL, NULL, NULL},
        {0, "users",   "/users",   s_ico_users,  "Users"},
        {0, "failban", "/failban", s_ico_ban,    "Fail-Ban"},
        {1, NULL, NULL, NULL, NULL},
        {0, "config",  "/config",  s_ico_cfg,    "Config"},
        {0, "tvcas",   "/tvcas",   s_ico_tvcas,  "TVCAS"},
        {1, NULL, NULL, NULL, NULL},
        {0, "power",   "/power",   s_ico_power,  "Power"},
        {2, NULL, NULL, NULL, NULL},
    };

    for (int i = 0; nav[i].sep != 2; i++) {
        if (nav[i].sep == 1) {
            if (nav[i + 1].sep == 0)
                pos = buf_printf(buf, bsz, pos, "<div class='sep'></div>");
            continue;
        }
        const char *cls = (strcmp(nav[i].id, nav_active) == 0) ? " act" : "";
        pos = buf_printf(buf, bsz, pos,
            "<a href='%s' class='%s'>%s%s</a>",
            nav[i].href, cls, nav[i].icon, nav[i].label);
    }

    int refresh = g_cfg.webif_refresh > 0 ? g_cfg.webif_refresh : 5;

    pos = buf_printf(buf, bsz, pos,
        "</div>"
        "<div class='tbr'>"
        "  <div class='spill'>"
        "    <div class='pulse sm'></div>"
        "    <span id='tb_conn'>%d</span>&nbsp;online"
        "  </div>"
        "  <span class='chip' id='tb_addr'>%s</span>"
        "  <div class='pc'>"
        "    <label>AUTO</label>"
        "    <button onclick='_ap(-1)'>&#8722;</button>"
        "    <input id='ps_' type='text' value='%d' readonly>"
        "    <button onclick='_ap(1)'>+</button>"
        "  </div>"
        "</div>"
        "</nav>"
        "<div id='mn'><div id='ct'>",
        g_active_conns, srv_addr, refresh);

    pos = buf_printf(buf, bsz, pos,
        "<script>" TCMG_JS "</script>",
        refresh);

    return pos;
}

int emit_footer(char **buf, int *bsz, int pos)
{
    return buf_printf(buf, bsz, pos,
        "</div>"
        "</div>"
        "<footer style='"
        "padding:10px 22px;border-top:1px solid var(--bd);"
        "display:flex;align-items:center;justify-content:center;"
        "font-size:11px;color:var(--t2);font-family:var(--mono)'>"
        "TCMG <span style='color:var(--p);margin:0 4px'>" TCMG_VERSION "</span>"
        "&bull; built <span style='color:var(--t1);margin-left:4px'>"
        TCMG_BUILD_TIME
        "</span></footer>"
        "</body></html>");
}
