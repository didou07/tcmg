#define MODULE_LOG_PREFIX "webif"
#include "tcmg-globals.h"
#include "tcmg-crypto.h"
#include "tcmg-log.h"
#ifndef TCMG_OS_WINDOWS
#  include <netdb.h>
#  include <sys/select.h>
#endif
#include "tcmg-webif-internal.h"

/* ═══════════════════════════════════════════════════════════════
   TCMG WebIF  ·  VOID Theme  v4.4
   Navy #090d14  ·  Blue #3b82f6  ·  Space Grotesk + JetBrains Mono
═══════════════════════════════════════════════════════════════ */
const char CSS[] =
"@import url('https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;700&family=Space+Grotesk:wght@300;400;500;600;700&display=swap');"
"*{box-sizing:border-box;margin:0;padding:0}"
"html{scroll-behavior:smooth}"
"a{text-decoration:none;color:inherit}"
"button{cursor:pointer;font-family:inherit;border:none;background:none}"
"ul,ol{list-style:none}"

/* ── Tokens ── */
":root{"
"--bg:#090d14;--s1:#0e1421;--s2:#141c2e;--s3:#1a2340;--s4:#202b47;"
"--bd:#1e2d47;--bd2:#253554;"
"--p:#3b82f6;--p2:#2563eb;--ps:rgba(59,130,246,.10);--pg:rgba(59,130,246,.22);"
"--vi:#8b5cf6;--vis:rgba(139,92,246,.10);--vig:rgba(139,92,246,.22);"
"--cy:#06b6d4;--cys:rgba(6,182,212,.10);--cyg:rgba(6,182,212,.22);"
"--gr:#22c55e;--grs:rgba(34,197,94,.10);--grg:rgba(34,197,94,.22);"
"--re:#ef4444;--res:rgba(239,68,68,.10);"
"--or:#f97316;--or2:#fb923c;--ors:rgba(249,115,22,.10);"
"--t0:#e8f0fe;--t1:#94a3b8;--t2:#4b6584;"
"--sans:'Space Grotesk',sans-serif;--mono:'JetBrains Mono',monospace;"
"--r:10px;--rsm:6px;--rxs:4px;"
"--sbw:240px;--tbh:60px;"
"--ease:cubic-bezier(.4,0,.2,1);"
"}"
"body{background:var(--bg);color:var(--t0);font-family:var(--sans);font-size:14px;line-height:1.5;-webkit-font-smoothing:antialiased;}"

/* ── Overlay ── */
"#ov{display:none;position:fixed;inset:0;background:rgba(0,0,0,.55);backdrop-filter:blur(3px);z-index:1040}"
"#ov.show{display:block}"

/* ══ SIDEBAR ══ */
"#sb{width:var(--sbw);height:100vh;background:var(--s1);border-right:1px solid var(--bd);position:fixed;top:0;left:0;display:flex;flex-direction:column;z-index:1050;padding-top:var(--tbh);transition:width .28s var(--ease);overflow:hidden}"
"#sb.col{width:60px}"

/* Logo */
".lo{position:absolute;top:0;left:0;width:100%;height:var(--tbh);display:flex;align-items:center;gap:10px;padding:0 17px;border-bottom:1px solid var(--bd);background:var(--s1);white-space:nowrap;overflow:hidden}"
".li{flex-shrink:0;width:32px;height:32px;background:var(--ps);border:1px solid var(--pg);border-radius:8px;display:grid;place-items:center}"
".li svg{width:18px;height:18px}"
".lt{font-weight:700;font-size:15px;letter-spacing:.06em;color:var(--t0)}"
".lv{margin-left:auto;flex-shrink:0;font-family:var(--mono);font-size:10px;color:var(--p);background:var(--ps);border:1px solid var(--pg);padding:2px 7px;border-radius:var(--rxs)}"
"#sb.col .lt,#sb.col .lv{display:none}"

/* Nav */
".ngl{font-size:10px;font-weight:600;letter-spacing:.12em;text-transform:uppercase;color:var(--t2);padding:16px 17px 4px;white-space:nowrap;overflow:hidden}"
"#sb.col .ngl{display:none}"
"nav ul{padding:2px 10px}"
"nav a{display:flex;align-items:center;gap:12px;padding:8px 10px;border-radius:var(--rsm);color:var(--t1);font-size:13.5px;font-weight:500;white-space:nowrap;overflow:hidden;border:1px solid transparent;transition:background .18s,color .18s,border-color .18s;margin-bottom:1px}"
"nav a:hover{background:var(--s3);color:var(--t0)}"
"nav a.act{background:var(--ps);color:var(--p);border-color:var(--pg)}"
".ni{width:18px;height:18px;flex-shrink:0;opacity:.75}"
"nav a.act .ni,nav a:hover .ni{opacity:1}"
"#sb.col nav a{padding:10px;justify-content:center}"
"#sb.col .nl{display:none}"

/* Srv footer */
".sf{margin-top:auto;display:flex;align-items:center;gap:10px;padding:14px 17px;border-top:1px solid var(--bd);white-space:nowrap;overflow:hidden}"
"#sb.col .sf{justify-content:center}"
"#sb.col .si{display:none}"
".si{display:flex;flex-direction:column;gap:1px}"
".sl{font-size:10px;font-weight:700;letter-spacing:.08em;color:var(--gr)}"
".su{font-family:var(--mono);font-size:12px;color:var(--t1)}"

/* Pulse */
".pulse{width:8px;height:8px;border-radius:50%;background:var(--gr);flex-shrink:0;animation:pa 2s ease-in-out infinite}"
".pulse.sm{width:6px;height:6px}"
"@keyframes pa{0%{box-shadow:0 0 0 0 rgba(34,197,94,.5)}70%{box-shadow:0 0 0 6px rgba(34,197,94,0)}100%{box-shadow:0 0 0 0 rgba(34,197,94,0)}}"

"@media(max-width:900px){#sb{left:calc(-1 * var(--sbw))}#sb.mob{left:0}#tb,#mn{margin-left:0!important}}"

/* ══ TOPBAR ══ */
"#tb{position:fixed;top:0;left:var(--sbw);right:0;height:var(--tbh);background:var(--s1);border-bottom:1px solid var(--bd);display:flex;align-items:center;justify-content:space-between;padding:0 20px;z-index:1030;transition:left .28s var(--ease)}"
"#tb.full{left:60px}"
".tbl,.tbr{display:flex;align-items:center;gap:8px}"

/* Icon btn */
".ib{width:36px;height:36px;border-radius:var(--rsm);background:var(--s2);border:1px solid var(--bd);display:grid;place-items:center;color:var(--t1);transition:all .18s;cursor:pointer}"
".ib svg{width:18px;height:18px}"
".ib:hover{background:var(--s3);color:var(--t0);border-color:var(--pg)}"

/* Breadcrumb */
".bc{display:flex;align-items:center;gap:6px;font-size:13px}"
".bcr{color:var(--t1)}.bcs{color:var(--bd2)}.bcc{color:var(--t0);font-weight:600}"

/* Status pill */
".spill{display:flex;align-items:center;gap:6px;background:var(--grs);border:1px solid rgba(34,197,94,.2);border-radius:20px;padding:4px 12px;font-size:12px;color:var(--gr);font-weight:500}"

/* Chip */
".chip{font-size:11px;font-family:var(--mono);background:var(--s2);border:1px solid var(--bd);border-radius:var(--rxs);padding:2px 8px;color:var(--t1)}"

/* Poll ctrl */
".pc{display:flex;align-items:center;gap:3px;background:var(--s2);border:1px solid var(--bd);border-radius:var(--rsm);padding:3px 7px;font-size:10px;font-family:var(--mono);color:var(--t2)}"
".pc label{white-space:nowrap;letter-spacing:.05em}"
".pc input{width:28px;background:none;border:none;outline:none;color:var(--t0);font-family:var(--mono);font-size:12px;text-align:center}"
".pc button{color:var(--t2);font-size:13px;line-height:1;padding:0 2px;border-radius:3px}"
".pc button:hover{color:var(--t0);background:var(--s3)}"

/* ══ MAIN ══ */
"#mn{margin-left:var(--sbw);margin-top:var(--tbh);padding:22px 22px 30px;min-height:calc(100vh - var(--tbh));transition:margin-left .28s var(--ease)}"
"#mn.full{margin-left:60px}"

/* Page header */
".ph{display:flex;align-items:flex-start;justify-content:space-between;flex-wrap:wrap;gap:12px;margin-bottom:22px}"
".pt{font-size:22px;font-weight:700;letter-spacing:-.01em;color:var(--t0)}"
".ps{color:var(--t1);font-size:13px;margin-top:3px}"
".ha{display:flex;gap:8px;align-items:center}"

/* ══ BUTTONS ══ */
".btn{display:inline-flex;align-items:center;gap:6px;padding:8px 16px;border-radius:var(--rsm);font-family:var(--sans);font-size:13px;font-weight:600;letter-spacing:.02em;transition:all .18s;cursor:pointer;border:1px solid transparent}"
".btn svg{width:14px;height:14px;flex-shrink:0}"
".bp{background:var(--p);color:#fff;border-color:var(--p)}"
".bp:hover{background:var(--p2);box-shadow:0 0 0 3px var(--pg)}"
".bg{background:var(--s2);color:var(--t1);border-color:var(--bd)}"
".bg:hover{background:var(--s3);color:var(--t0)}"
".bd_{background:var(--res);color:var(--re);border-color:rgba(239,68,68,.3)}"
".bd_:hover{background:rgba(239,68,68,.2)}"
".btn.sm{padding:5px 11px;font-size:12px}"

/* ══ STAT CARDS ══ */
".cg{display:grid;grid-template-columns:repeat(auto-fill,minmax(195px,1fr));gap:14px;margin-bottom:22px}"
".sc{background:var(--s1);border:1px solid var(--bd);border-radius:var(--r);padding:16px;display:flex;align-items:flex-start;gap:14px;position:relative;overflow:hidden;transition:border-color .22s,transform .18s,box-shadow .22s;animation:fu .35s var(--ease) both}"
".sc:hover{transform:translateY(-2px)}"

/* Colour variants */
".sc.bl{border-color:rgba(59,130,246,.28);background:rgba(59,130,246,.04)}"
".sc.bl:hover{border-color:var(--p);box-shadow:0 0 32px rgba(59,130,246,.14)}"
".sc.gr{border-color:rgba(34,197,94,.28);background:rgba(34,197,94,.04)}"
".sc.gr:hover{border-color:var(--gr);box-shadow:0 0 32px rgba(34,197,94,.14)}"
".sc.vi{border-color:rgba(139,92,246,.28);background:rgba(139,92,246,.04)}"
".sc.vi:hover{border-color:var(--vi);box-shadow:0 0 32px rgba(139,92,246,.14)}"
".sc.cy{border-color:rgba(6,182,212,.28);background:rgba(6,182,212,.04)}"
".sc.cy:hover{border-color:var(--cy);box-shadow:0 0 32px rgba(6,182,212,.14)}"
".sc.re{border-color:rgba(239,68,68,.28);background:rgba(239,68,68,.04)}"
".sc.re:hover{border-color:var(--re);box-shadow:0 0 32px rgba(239,68,68,.14)}"
".sc.or{border-color:rgba(249,115,22,.28);background:rgba(249,115,22,.04)}"
".sc.or:hover{border-color:var(--or);box-shadow:0 0 32px rgba(249,115,22,.14)}"

/* Icon shape */
".si_{width:44px;height:44px;border-radius:9px;display:grid;place-items:center;flex-shrink:0}"
".si_ svg{width:22px;height:22px}"
".bl .si_{background:var(--ps);color:var(--p)}"
".gr .si_{background:var(--grs);color:var(--gr)}"
".vi .si_{background:var(--vis);color:var(--vi)}"
".cy .si_{background:var(--cys);color:var(--cy)}"
".re .si_{background:var(--res);color:var(--re)}"
".or .si_{background:var(--ors);color:var(--or)}"

/* Card body */
".sb_{display:flex;flex-direction:column;gap:3px;min-width:0;flex:1}"
".sl_{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.10em;color:var(--t1)}"
".sv{font-size:22px;font-weight:700;color:var(--t0);font-variant-numeric:tabular-nums;transition:color .3s}"
".sv.mono{font-family:var(--mono);font-size:16px}"
".sd{font-size:11px;font-family:var(--mono);color:var(--t1)}"
".bl .sd{color:var(--p)}.gr .sd{color:var(--gr)}.vi .sd{color:var(--vi)}.cy .sd{color:var(--cy)}"

/* Top accent bar on hover */
".sc::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;opacity:0;transition:opacity .22s;border-radius:var(--r) var(--r) 0 0}"
".sc:hover::before{opacity:1}"
".bl::before{background:linear-gradient(90deg,var(--p),var(--cy))}"
".gr::before{background:linear-gradient(90deg,var(--gr),var(--cy))}"
".vi::before{background:linear-gradient(90deg,var(--vi),var(--p))}"
".cy::before{background:linear-gradient(90deg,var(--cy),var(--vi))}"
".re::before{background:linear-gradient(90deg,var(--re),var(--or))}"
".or::before{background:linear-gradient(90deg,var(--or),var(--re))}"

/* Glow blob */
".sg{position:absolute;width:70px;height:70px;border-radius:50%;right:-15px;top:-15px;opacity:.12;filter:blur(18px);pointer-events:none}"
".bl .sg{background:var(--p)}.gr .sg{background:var(--gr)}.vi .sg{background:var(--vi)}"
".cy .sg{background:var(--cy)}.re .sg{background:var(--re)}.or .sg{background:var(--or)}"

/* Stagger */
".sc:nth-child(1){animation-delay:.04s}.sc:nth-child(2){animation-delay:.08s}"
".sc:nth-child(3){animation-delay:.12s}.sc:nth-child(4){animation-delay:.16s}"
".sc:nth-child(5){animation-delay:.20s}.sc:nth-child(6){animation-delay:.24s}"
".sc:nth-child(7){animation-delay:.28s}"

/* ══ CARD (generic) ══ */
".card{background:var(--s1);border:1px solid var(--bd);border-radius:var(--r);transition:border-color .2s;animation:fu .35s var(--ease) .12s both}"
".card:hover{border-color:var(--pg)}"
".ch{display:flex;align-items:center;justify-content:space-between;padding:13px 18px;border-bottom:1px solid var(--bd);background:var(--s2);border-radius:var(--r) var(--r) 0 0}"
".ct{font-size:14px;font-weight:700;color:var(--t0);display:flex;align-items:center;gap:8px}"
".ct svg{width:16px;height:16px;color:var(--p);flex-shrink:0}"
".cb{padding:16px 18px}"

/* Section header */
".shd{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;margin-top:4px}"
".stl{font-size:14px;font-weight:700;color:var(--t0);display:flex;align-items:center;gap:8px}"
".stl::before{content:'';display:inline-block;width:3px;height:14px;background:var(--p);border-radius:2px}"

/* ══ TABLES ══ */
".tw{border:1px solid var(--bd);border-radius:var(--r);overflow:hidden;margin-bottom:16px}"
"table{width:100%;border-collapse:collapse;font-size:13px}"
"thead tr{background:var(--s2)}"
"th{padding:9px 14px;text-align:left;font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.10em;color:var(--t2);border-bottom:1px solid var(--bd);white-space:nowrap}"
"td{padding:10px 14px;border-bottom:1px solid var(--bd);color:var(--t0)}"
"tbody tr:last-child td{border-bottom:none}"
"tbody tr:hover{background:var(--s3)}"
"tbody tr.fl{animation:rf .5s ease}"
"@keyframes rf{from{background:rgba(59,130,246,.18)}to{background:transparent}}"
".mono{font-family:var(--mono);font-size:12px}"
".bold{font-weight:600}"

/* User avatar */
".av{width:30px;height:30px;border-radius:7px;display:inline-flex;align-items:center;justify-content:center;font-size:11px;font-weight:700;letter-spacing:.04em;flex-shrink:0;background:var(--ps);color:var(--p);border:1px solid var(--pg)}"

/* ══ BADGES ══ */
".badge{display:inline-flex;align-items:center;gap:4px;padding:2px 9px;border-radius:var(--rxs);font-size:11px;font-weight:700;font-family:var(--mono);letter-spacing:.04em}"
".bon{background:var(--grs);color:var(--gr);border:1px solid rgba(34,197,94,.22)}"
".boff{background:var(--res);color:var(--re);border:1px solid rgba(239,68,68,.22)}"
".bban{background:var(--ors);color:var(--or2);border:1px solid rgba(249,115,22,.22)}"
".bbl{background:var(--ps);color:var(--p);border:1px solid var(--pg)}"
".bcy{background:var(--cys);color:var(--cy);border:1px solid var(--cyg)}"

/* Kill btn */
".kb{display:inline-flex;align-items:center;padding:4px 7px;border-radius:var(--rxs);color:var(--re);opacity:.4;transition:opacity .15s,background .15s}"
".kb:hover{opacity:1;background:var(--res)}"
".kb svg{width:13px;height:13px}"

/* Hit bar */
".hbw{background:var(--s3);border-radius:4px;height:5px;width:80px;overflow:hidden}"
".hbf{height:100%;border-radius:4px;background:linear-gradient(90deg,var(--gr),var(--cy));transition:width .4s}"

/* ══ LOG TERMINAL ══ */
".lc{display:flex;align-items:center;gap:8px;margin-bottom:10px;flex-wrap:wrap}"
".ls{background:var(--s2);border:1px solid var(--bd2);color:var(--t0);border-radius:var(--rsm);padding:5px 10px;font-family:var(--mono);font-size:12px;width:200px;outline:none}"
".ls:focus{border-color:var(--pg)}"
"select.lsel{background:var(--s2);color:var(--t1);border:1px solid var(--bd2);border-radius:var(--rsm);padding:5px 8px;font-size:12px}"
"#lw{background:#040810;border:1px solid var(--bd);border-radius:var(--r);height:480px;overflow:auto;padding:14px 16px;position:relative}"
"#lw::before{content:'LIVE';position:absolute;top:10px;right:12px;font-size:9px;font-family:var(--mono);font-weight:700;letter-spacing:.12em;color:var(--gr);opacity:.5}"
"#lp{margin:0;font-family:var(--mono);font-size:12.5px;line-height:1.9;white-space:pre;color:var(--t1)}"
".lok{color:#4ade80;font-weight:700}"
".lwarn{color:var(--or2);font-weight:700}"
".lerr{color:var(--re);font-weight:700}"
".lnet{color:#c084fc;font-weight:700}"
".lwebif{color:#60a5fa;font-weight:700}"
".lban{color:var(--or2);font-weight:700}"
".lt2{color:var(--t2)}"

/* Debug tags */
".db{background:var(--s2);border:1px solid var(--bd);border-radius:var(--rsm);padding:8px 12px;margin-bottom:10px;display:flex;flex-wrap:wrap;align-items:center;gap:5px}"
".dt{display:inline-flex;align-items:center;padding:3px 10px;border-radius:var(--rxs);font-size:11px;font-family:var(--mono);font-weight:500;cursor:pointer;border:1px solid var(--bd);color:var(--t2);transition:all .15s;user-select:none}"
".dt.on{background:var(--ps);border-color:var(--pg);color:var(--p)}"
".dt:hover{border-color:var(--pg);color:var(--p)}"
".dm{font-size:11px;color:var(--t2);font-family:var(--mono);margin-left:auto}"

/* ══ CONFIG EDITOR ══ */
".et{display:flex;align-items:center;gap:10px;padding:10px 16px;border-bottom:1px solid var(--bd);background:var(--s2);border-radius:var(--r) var(--r) 0 0}"
".ef{font-family:var(--mono);font-size:12px;color:var(--p);flex:1;display:flex;align-items:center;gap:8px}"
".ef svg{width:14px;height:14px;opacity:.6}"
".ew{background:var(--ors);border-top:1px solid rgba(249,115,22,.25);padding:7px 16px;font-family:var(--mono);font-size:12px;color:var(--or2);display:flex;align-items:center;gap:6px}"
".ew svg{width:13px;height:13px;flex-shrink:0}"
".ea{font-family:var(--mono);font-size:13px;line-height:1.9;color:#a5d6a7;background:#040810;border:none;outline:none;width:100%;min-height:390px;padding:14px 16px;resize:vertical}"
".ef2{display:flex;align-items:center;gap:10px;padding:8px 16px;border-top:1px solid var(--bd);background:var(--s2);border-radius:0 0 var(--r) var(--r)}"
".es{font-family:var(--mono);font-size:11px;color:var(--t1);flex:1}"
".es .ok{color:var(--gr)}"

/* ══ LOGIN ══ */
".lb{min-height:100vh;display:flex;align-items:center;justify-content:center;background:var(--bg);background-image:radial-gradient(ellipse at 20% 50%,rgba(59,130,246,.06) 0%,transparent 60%),radial-gradient(ellipse at 80% 20%,rgba(6,182,212,.06) 0%,transparent 60%)}"
".lcard{background:var(--s2);border:1px solid var(--bd);border-radius:14px;padding:30px 36px;width:350px;box-shadow:0 28px 70px rgba(0,0,0,.55)}"
".ll{display:flex;align-items:center;gap:12px;margin-bottom:24px}"
".lli{width:46px;height:46px;background:var(--ps);border:1px solid var(--pg);border-radius:11px;display:grid;place-items:center;flex-shrink:0}"
".lli svg{width:26px;height:26px}"
".llt{font-size:20px;font-weight:700;color:var(--t0)}"
".llv{font-size:11px;color:var(--t1);font-family:var(--mono);margin-top:2px}"
".fl{display:block;font-size:11px;font-weight:600;color:var(--t1);letter-spacing:.05em;margin-bottom:5px}"
".fi{width:100%;padding:9px 12px;background:var(--s1);border:1px solid var(--bd);color:var(--t0);border-radius:var(--rsm);font-size:13px;font-family:var(--sans);transition:border-color .18s;outline:none}"
".fi:focus{border-color:var(--pg)}"
".fg{margin-bottom:14px}"
".le{display:flex;align-items:center;gap:8px;background:var(--res);border:1px solid rgba(239,68,68,.28);border-radius:var(--rsm);padding:9px 12px;color:var(--re);font-size:12px;margin-bottom:16px}"
".le svg{width:14px;height:14px;flex-shrink:0}"

/* ══ DIALOGS (restart/shutdown) ══ */
".dlg{background:var(--s2);border:1px solid var(--bd);border-radius:14px;padding:32px;max-width:400px}"
".dico{width:60px;height:60px;border-radius:14px;display:grid;place-items:center;margin:0 auto 18px;flex-shrink:0}"
".dico svg{width:30px;height:30px}"
".dico.danger{background:var(--res);color:var(--re)}"
".dico.info{background:var(--ps);color:var(--p)}"
".dico.warn{background:var(--ors);color:var(--or)}"
".dlg h2{font-size:17px;font-weight:700;color:var(--t0);text-align:center;margin-bottom:8px}"
".dlg p{color:var(--t1);font-size:13px;text-align:center;margin-bottom:22px;line-height:1.6}"
".da{display:flex;gap:10px;justify-content:center}"

/* Confirm card (done state) */
".done-card{background:var(--s2);border:1px solid var(--bd);border-radius:14px;padding:32px;max-width:400px;text-align:center}"

/* ══ MISC ══ */
".ib2{background:var(--s2);border:1px solid var(--bd);border-radius:var(--rsm);padding:10px 14px;margin-bottom:12px;font-size:12px;color:var(--t1)}"
".ib2 svg{width:13px;height:13px;vertical-align:-2px;margin-right:4px}"
".tg{color:var(--gr)}.tr{color:var(--re)}.to{color:var(--or2)}.tb{color:var(--p)}.tv{color:var(--vi)}.tc{color:var(--cy)}.tm{color:var(--t1)}"
".flex{display:flex;align-items:center}.gap8{gap:8px}.gap10{gap:10px}"
".mb16{margin-bottom:16px}.mb10{margin-bottom:10px}"
"a.danger{color:var(--re)}"
"hr{border:none;border-top:1px solid var(--bd);margin:14px 0}"
".erow td{text-align:center;color:var(--t1);padding:22px}"
"input[type=checkbox]{accent-color:var(--p)}"
"label{cursor:pointer}"

/* Tooltip */
".tip{position:relative}"
".tipt{display:none;position:absolute;bottom:calc(100% + 6px);left:50%;transform:translateX(-50%);background:var(--s4);border:1px solid var(--bd2);border-radius:5px;padding:4px 8px;font-size:11px;color:var(--t0);white-space:nowrap;z-index:300;pointer-events:none}"
".tip:hover .tipt{display:block}"

/* Animations */
"@keyframes fu{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}"
"@keyframes cnt{from{opacity:0;transform:scale(.85)}to{opacity:1;transform:scale(1)}}"
".cnt-up{animation:cnt .4s var(--ease)}"

/* Scrollbar */
"::-webkit-scrollbar{width:5px;height:5px}"
"::-webkit-scrollbar-track{background:transparent}"
"::-webkit-scrollbar-thumb{background:var(--bd2);border-radius:4px}"
"::-webkit-scrollbar-thumb:hover{background:var(--bd)}";

/* ═══ ICONS ═══ */
#define ICO_LOGO \
"<svg width='18' height='18' viewBox='0 0 24 24' fill='none'>"\
"<path d='M12 2L2 7l10 5 10-5-10-5z' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>"\
"<path d='M2 17l10 5 10-5' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>"\
"<path d='M2 12l10 5 10-5' stroke='var(--cy)' stroke-width='1.8' stroke-linejoin='round'/>"\
"</svg>"

#define ICO_MENU \
"<svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<line x1='3' y1='6' x2='21' y2='6'/><line x1='3' y1='12' x2='21' y2='12'/><line x1='3' y1='18' x2='21' y2='18'/>"\
"</svg>"

/* Nav icons */
#define N_STATUS \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<polyline points='22 12 18 12 15 21 9 3 6 12 2 12'/>"\
"</svg>"

#define N_LOG \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z'/>"\
"<polyline points='14 2 14 8 20 8'/>"\
"<line x1='8' y1='13' x2='16' y2='13'/><line x1='8' y1='17' x2='16' y2='17'/>"\
"</svg>"

#define N_USERS \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2'/>"\
"<circle cx='9' cy='7' r='4'/>"\
"<path d='M23 21v-2a4 4 0 0 0-3-3.87'/><path d='M16 3.13a4 4 0 0 1 0 7.75'/>"\
"</svg>"

#define N_BAN \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<circle cx='12' cy='12' r='10'/><line x1='4.93' y1='4.93' x2='19.07' y2='19.07'/>"\
"</svg>"

#define N_CFG \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<circle cx='12' cy='12' r='3'/>"\
"<path d='M19.07 4.93a10 10 0 0 1 0 14.14M4.93 4.93a10 10 0 0 0 0 14.14'/>"\
"</svg>"

#define N_RESTART \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<polyline points='23 4 23 10 17 10'/>"\
"<path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"\
"</svg>"

#define N_STOP \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<circle cx='12' cy='12' r='10'/><rect x='9' y='9' width='6' height='6'/>"\
"</svg>"

#define N_TVCAS \
"<svg class='ni' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"\
"<path d='M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z'/>"\
"</svg>"

/* Stat card icon macros moved to tcmg-webif-internal.h */

/* ═══════════════════════════════════════════════════════════════
   emit_header
═══════════════════════════════════════════════════════════════ */
int emit_header(char **buf, int *bsz, int pos,
                const char *title, const char *active)
{
	int is_status = (strcmp(active,"status")==0);
	char upstr[32];
	format_uptime(time(NULL)-g_start_time, upstr, sizeof(upstr));

	pos = buf_printf(buf,bsz,pos,
		"<!DOCTYPE html><html lang='en'><head>"
		"<meta charset='UTF-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>TCMG &mdash; %s</title>"
		"<style>%s</style>"
		"</head><body>",
		title, CSS);

	pos = buf_printf(buf,bsz,pos,"<div id='ov'></div>");

	/* ── SIDEBAR ── */
	pos = buf_printf(buf,bsz,pos,
		"<aside id='sb'>"
		"<div class='lo'>"
		"  <div class='li'>" ICO_LOGO "</div>"
		"  <span class='lt'>TCMG</span>"
		"  <span class='lv'>" TCMG_VERSION "</span>"
		"</div>");

	static const NavItem mon[]={
		{"status", "/status",  N_STATUS, "Dashboard"},
		{"livelog","/livelog", N_LOG,    "Live Log"},
		{NULL,NULL,NULL,NULL}
	};
	static const NavItem acc[]={
		{"users",  "/users",   N_USERS,  "Users"},
		{"failban","/failban", N_BAN,    "Fail-Ban"},
		{NULL,NULL,NULL,NULL}
	};
	static const NavItem sys[]={
		{"config",  "/config",   N_CFG,    "Config"},
		{"restart", "/restart",  N_RESTART,"Restart"},
		{"shutdown","/shutdown", N_STOP,   "Shutdown"},
		{NULL,NULL,NULL,NULL}
	};
	static const NavItem tls[]={
		{"tvcas","/tvcas",N_TVCAS,"TVCAS Tool"},
		{NULL,NULL,NULL,NULL}
	};
	static const struct{const char*label;const NavItem*items;}groups[]={
		{"Monitor",mon},{"Accounts",acc},{"System",sys},{"Tools",tls},{NULL,NULL}
	};
	for(int g=0;groups[g].label;g++){
		pos=buf_printf(buf,bsz,pos,"<div class='ngl'>%s</div><nav><ul>",groups[g].label);
		for(int i=0;groups[g].items[i].id;i++){
			const char *cls=strcmp(groups[g].items[i].id,active)==0?" act":"";
			pos=buf_printf(buf,bsz,pos,
				"<li><a href='%s' class='%s'>%s<span class='nl'>%s</span></a></li>",
				groups[g].items[i].href,cls,
				groups[g].items[i].icon,
				groups[g].items[i].label);
		}
		pos=buf_printf(buf,bsz,pos,"</ul></nav>");
	}

	pos=buf_printf(buf,bsz,pos,
		"<div class='sf'>"
		"  <div class='pulse'></div>"
		"  <div class='si'>"
		"    <span class='sl'>RUNNING</span>"
		"    <span class='su' id='sb_up'>%s</span>"
		"  </div>"
		"</div></aside>",upstr);

	/* ── TOPBAR ── */
	char srv_addr[64];
	snprintf(srv_addr,sizeof(srv_addr),"%s:%d",
	         g_cfg.webif_bindaddr[0]?g_cfg.webif_bindaddr:"0.0.0.0",
	         g_cfg.webif_port);

	pos=buf_printf(buf,bsz,pos,
		"<div id='mn'>"
		"<nav id='tb'>"
		"  <div class='tbl'>"
		"    <button class='ib' id='tgBtn' title='Toggle sidebar'>" ICO_MENU "</button>"
		"    <button class='ib' id='mbBtn' title='Menu'>" ICO_MENU "</button>"
		"    <div class='bc'>"
		"      <span class='bcr'>TCMG</span>"
		"      <span class='bcs'>/</span>"
		"      <span class='bcc'>%s</span>"
		"    </div>"
		"  </div>"
		"  <div class='tbr'>"
		"    <div class='spill'><div class='pulse sm'></div><span id='tb_conn'>%d</span>&nbsp;online</div>"
		"    <span class='chip' id='tb_addr'>%s</span>"
		"    <div class='pc'><label>AUTO</label>"
		"      <button onclick='_ap(-1)'>&#8722;</button>"
		"      <input id='ps_' type='text' value='%d' readonly>"
		"      <button onclick='_ap(1)'>+</button>"
		"    </div>"
		"  </div>"
		"</nav>"
		"<div id='ct' style='padding:22px'>",
		title,
		g_active_conns, srv_addr,
		g_cfg.webif_refresh>0?g_cfg.webif_refresh:5);

	/* ── JS ── */
	pos=buf_printf(buf,bsz,pos,
		"<script>"
		/* Sidebar */
		"(function(){"
		"  var sb=document.getElementById('sb');"
		"  var mn=document.getElementById('mn');"
		"  var tb=document.getElementById('tb');"
		"  var ov=document.getElementById('ov');"
		"  var tg=document.getElementById('tgBtn');"
		"  var mb=document.getElementById('mbBtn');"
		"  if(sessionStorage.tcmg_sb==='1'){sb.classList.add('col');mn.classList.add('full');tb.classList.add('full');}"
		"  if(tg)tg.addEventListener('click',function(){"
		"    sb.classList.toggle('col');mn.classList.toggle('full');tb.classList.toggle('full');"
		"    sessionStorage.tcmg_sb=sb.classList.contains('col')?'1':'0';"
		"  });"
		"  if(mb)mb.addEventListener('click',function(){sb.classList.add('mob');if(ov)ov.classList.add('show');});"
		"  if(ov)ov.addEventListener('click',function(){sb.classList.remove('mob');ov.classList.remove('show');});"
		"})();"
		/* Poll */
		"var _pm=(function(){"
		"  var v=parseInt(sessionStorage.tcmg_poll)||%d;"
		"  document.getElementById('ps_').value=v;return v*1000;"
		"})();"
		"function _ap(d){"
		"  var el=document.getElementById('ps_');"
		"  var v=Math.max(1,Math.min(99,parseInt(el.value)||5)+d);"
		"  el.value=v;_pm=v*1000;sessionStorage.tcmg_poll=v;"
		"}"
		/* Topbar live update */
		"(function _tp(){"
		"  fetch('/api/status',{cache:'no-store'})"
		"    .then(function(r){return r.json();})"
		"    .then(function(d){"
		"      var e;"
		"      e=document.getElementById('sb_up');  if(e)e.textContent=d.uptime_str;"
		"      e=document.getElementById('tb_conn');if(e)e.textContent=d.active_connections;"
		"      setTimeout(_tp,_pm);"
		"    }).catch(function(){setTimeout(_tp,_pm*3);});"
		"})();"
		"</script>",
		g_cfg.webif_refresh>0?g_cfg.webif_refresh:5);

	/* Status page live-poll JS */
	if(is_status){
		pos=buf_printf(buf,bsz,pos,
			"<script>"
			"var _pl=false;"
			"function _esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
			"function _fmt(n){return n>=1e6?(n/1e6).toFixed(1)+'M':n>=1e3?(n/1e3).toFixed(1)+'K':n}"
			"function _anim(id,v){"
			"  var e=document.getElementById(id);if(!e)return;"
			"  e.textContent=v;e.classList.remove('cnt-up');"
			"  void e.offsetWidth;e.classList.add('cnt-up');"
			"}"
			"function _poll(){"
			"  if(_pl){setTimeout(_poll,_pm);return;}"
			"  _pl=true;"
			"  fetch('/api/status',{cache:'no-store'})"
			"    .then(r=>r.json())"
			"    .then(d=>{_pl=false;_upd(d);setTimeout(_poll,_pm);})"
			"    .catch(()=>{_pl=false;setTimeout(_poll,_pm*3);});"
			"}"
			"function _kill(tid,user){"
			"  if(!confirm('Disconnect '+user+'?'))return;"
			"  fetch('/status?kill='+tid+'&user='+encodeURIComponent(user));"
			"  var r=document.getElementById('row_'+tid);if(r){r.style.opacity='.4';setTimeout(()=>r.remove(),500);}"
			"}"
			"function _upd(d){"
			"  _anim('p_up',d.uptime_str);"
			"  _anim('p_conn',d.active_connections);"
			"  _anim('p_acc',d.accounts);"
			"  _anim('p_hit',_fmt(d.cw_found));"
			"  _anim('p_miss',_fmt(d.cw_not));"
			"  _anim('p_ban',d.banned_ips);"
			"  _anim('p_ecm',_fmt(d.ecm_total));"
			"  var e=document.getElementById('p_hr');"
			"  if(e)e.textContent=d.hit_rate_pct.toFixed(1)+'%%';"
			"  /* hit bar */ var hb=document.getElementById('p_hbf');"
			"  if(hb)hb.style.width=d.hit_rate_pct.toFixed(0)+'%%';"
			"  document.getElementById('sb_up').textContent=d.uptime_str;"
			"  document.getElementById('tb_conn').textContent=d.active_connections;"
			"  var tb=document.getElementById('p_clients');"
			"  if(!tb)return;"
			"  if(!d.clients||!d.clients.length){"
			"    tb.innerHTML=\"<tr class='erow'><td colspan='8'>\""
			"      +\"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8' style='vertical-align:-3px;margin-right:6px;opacity:.4'><circle cx='12' cy='12' r='10'/><line x1='8' y1='12' x2='16' y2='12'/></svg>\""
			"      +\"No active connections</td></tr>\";"
			"    return;"
			"  }"
			"  var rows='';"
			"  d.clients.forEach(function(cl){"
			"    var init=(cl.user||'?').slice(0,2).toUpperCase();"
			"    rows+='<tr class=\"fl\" id=\"row_'+cl.thread_id+'\">'"
			"      +'<td><div class=\"flex gap8\"><span class=\"av\">'+init+'</span><span class=\"bold\">'+_esc(cl.user)+'</span></div></td>'"
			"      +'<td class=\"mono\">'+_esc(cl.ip)+'</td>'"
			"      +'<td class=\"mono\"><span class=\"badge bbl\">'+_esc(cl.caid)+'</span></td>'"
			"      +'<td class=\"mono\">'+_esc(cl.sid)+'</td>'"
			"      +'<td>'+_esc(cl.channel||'<span class=\"tm\">&mdash;</span>')+'</td>'"
			"      +'<td class=\"mono tm\">'+_esc(cl.connected)+'</td>'"
			"      +'<td class=\"mono tm\">'+_esc(cl.idle)+'</td>'"
			"      +'<td><button class=\"kb\" onclick=\"_kill('+cl.thread_id+',\\''+_esc(cl.user)+'\\')\""
			"       title=\"Disconnect " ICO_KILLBTN "\">" ICO_KILLBTN "</button></td>'"
			"      +'</tr>';"
			"  });"
			"  tb.innerHTML=rows;"
			"}"
			"document.addEventListener('DOMContentLoaded',function(){setTimeout(_poll,_pm);});"
			"</script>");
	}
	return pos;
}

int emit_footer(char **buf, int *bsz, int pos)
{
	return buf_printf(buf,bsz,pos,
		"</div>"   /* #ct */
		"<footer style='padding:10px 22px;border-top:1px solid var(--bd);"
		"display:flex;align-items:center;font-size:11px;color:var(--t2);font-family:var(--mono);'>"
		"TCMG <span style='color:var(--p);margin:0 4px'>" TCMG_VERSION "</span>"
		"&bull; built <span style='color:var(--t1);margin-left:4px'>" TCMG_BUILD_TIME "</span>"
		"</footer>"
		"</div></body></html>");
}
