                                                                          
  
                                                                            
                                                           
                                                                        
                                                                   
                                                          
                                                                                  
   
#ifndef TCMG_WEBIF_CSS_H_
#define TCMG_WEBIF_CSS_H_

#define TCMG_CSS \
	"*{box-sizing:border-box;margin:0;padding:0}" \
	"html{scroll-behavior:smooth}" \
	"a{text-decoration:none;color:inherit}" \
	"button{cursor:pointer;font-family:inherit;border:none;background:none}" \
	"ul,ol{list-style:none}" \
	":root{--bg:#090d14;--s1:#0e1421;--s2:#141c2e;--s3:#1a2340;--s4:#202b47;--bd:#1e2d47;--bd2:#253554;--p:#3b82f6;--p2:#2563eb;--ps:rgba(59,130,246,.10);--pg:rgba(59,130,246,.22);--vi:#8b5cf6;--vis:rgba(139,92,246,.10);--vig:rgba(139,92,246,.22);--cy:#06b6d4;--cys:rgba(6,182,212,.10);--cyg:rgba(6,182,212,.22);--gr:#22c55e;--grs:rgba(34,197,94,.10);--grg:rgba(34,197,94,.22);--re:#ef4444;--res:rgba(239,68,68,.10);--or:#f97316;--or2:#fb923c;--ors:rgba(249,115,22,.10);--t0:#e8f0fe;--t1:#94a3b8;--t2:#7a91ad;--sans:system-ui,-apple-system,'Segoe UI',Arial,sans-serif;--mono:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono',monospace;--r:10px;--rsm:6px;--rxs:4px;--tbh:56px;--ease:cubic-bezier(.4,0,.2,1);}" \
	"body{background:var(--bg);color:var(--t0);font-family:var(--sans);font-size:14px;line-height:1.5;-webkit-font-smoothing:antialiased;}" \
	"#tb{position:fixed;top:0;left:0;right:0;height:var(--tbh);background:var(--s1);border-bottom:1px solid var(--bd);display:flex;align-items:center;padding:0 18px;z-index:1030;gap:10px;}" \
	".lo{display:flex;align-items:center;gap:9px;margin-right:10px;flex-shrink:0}" \
	".li{width:30px;height:30px;background:var(--ps);border:1px solid var(--pg);border-radius:8px;display:grid;place-items:center;flex-shrink:0}" \
	".li svg{width:16px;height:16px}" \
	".lt{font-weight:700;font-size:14px;letter-spacing:.06em;color:var(--t0)}" \
	".lv{font-family:var(--mono);font-size:10px;color:var(--p);background:var(--ps);border:1px solid var(--pg);padding:1px 6px;border-radius:var(--rxs)}" \
	".tnav{display:flex;align-items:center;justify-content:center;gap:2px;flex:1;flex-wrap:wrap}" \
	".tnav a{display:inline-flex;align-items:center;gap:5px;padding:6px 10px;border-radius:var(--rsm);color:var(--t1);font-size:12.5px;font-weight:500;border:1px solid transparent;transition:background .15s,color .15s,border-color .15s;white-space:nowrap}" \
	".tnav a.act{background:var(--ps);color:var(--p);border-color:var(--pg)}" \
	".tnav .ni{width:14px;height:14px;flex-shrink:0;opacity:.75}" \
	".tnav a.act .ni{opacity:1}" \
	".tnav .sep{width:1px;height:20px;background:var(--bd2);margin:0 4px;flex-shrink:0}" \
	".tbr{display:flex;align-items:center;gap:8px;margin-left:auto;flex-shrink:0}" \
	".spill{display:flex;align-items:center;gap:6px;background:var(--grs);border:1px solid rgba(34,197,94,.2);border-radius:20px;padding:3px 10px;font-size:11px;color:var(--gr);font-weight:500;white-space:nowrap}" \
	".chip{font-size:11px;font-family:var(--mono);background:var(--s2);border:1px solid var(--bd);border-radius:var(--rxs);padding:2px 8px;color:var(--t1)}" \
	".pc{display:flex;align-items:center;gap:3px;background:var(--s2);border:1px solid var(--bd);border-radius:var(--rsm);padding:3px 7px;font-size:10px;font-family:var(--mono);color:var(--t2)}" \
	".pc label{white-space:nowrap;letter-spacing:.05em}" \
	".pc input{width:28px;background:none;border:none;outline:none;color:var(--t0);font-family:var(--mono);font-size:12px;text-align:center}" \
	".pc button{color:var(--t2);font-size:13px;line-height:1;padding:0 2px;border-radius:3px}" \
	"body.pg-config .pc,body.pg-tvcas .pc,body.pg-power .pc{display:none}" \
	".pc.pc-off{display:none}" \
	".pulse{width:8px;height:8px;border-radius:50%;background:var(--gr);flex-shrink:0;animation:pa 2s ease-in-out infinite}" \
	".pulse.sm{width:6px;height:6px}" \
	"@keyframes pa{" \
	"  0%{box-shadow:0 0 0 0 rgba(34,197,94,.5)}" \
	"  70%{box-shadow:0 0 0 6px rgba(34,197,94,0)}" \
	"  100%{box-shadow:0 0 0 0 rgba(34,197,94,0)}" \
	"}" \
	"#mn{margin-top:var(--tbh);min-height:calc(100vh - var(--tbh));display:flex;justify-content:center}" \
	"#ct{width:100%;max-width:1400px;padding:16px 20px 32px}" \
	".ph{display:flex;align-items:center;justify-content:center;flex-wrap:wrap;gap:8px;margin-bottom:16px}" \
	".ha{display:flex;gap:8px;align-items:center}" \
	".btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;padding:8px 16px;border-radius:var(--rsm);font-family:var(--sans);font-size:13px;font-weight:600;letter-spacing:.02em;transition:all .18s;cursor:pointer;border:1px solid transparent}" \
	".btn svg{width:14px;height:14px;flex-shrink:0}" \
	".bp{background:var(--p);color:#fff;border-color:var(--p)}" \
	".bg{background:var(--s2);color:var(--t1);border-color:var(--bd)}" \
	".bd_{background:var(--res);color:var(--re);border-color:rgba(239,68,68,.3)}" \
	".bwarn{background:var(--ors);color:var(--or);border-color:rgba(249,115,22,.3)}" \
	".btn.sm{padding:5px 11px;font-size:12px}" \
	".cg{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:14px;margin-bottom:16px;align-items:stretch}" \
	".sc{background:var(--s1);border:1px solid var(--bd);border-radius:var(--r);padding:16px;display:flex;align-items:flex-start;gap:14px;position:relative;overflow:hidden;transition:border-color .18s;animation:fu .28s var(--ease) both;height:100%}" \
		".sc.bl{border-color:rgba(59,130,246,.28);background:rgba(59,130,246,.04)}" \
	".sc.gr{border-color:rgba(34,197,94,.28);background:rgba(34,197,94,.04)}" \
	".sc.vi{border-color:rgba(139,92,246,.28);background:rgba(139,92,246,.04)}" \
	".sc.cy{border-color:rgba(6,182,212,.28);background:rgba(6,182,212,.04)}" \
	".sc.re{border-color:rgba(239,68,68,.28);background:rgba(239,68,68,.04)}" \
	".sc.or{border-color:rgba(249,115,22,.28);background:rgba(249,115,22,.04)}" \
	".si_{width:44px;height:44px;border-radius:9px;display:grid;place-items:center;flex-shrink:0}" \
	".si_ svg{width:22px;height:22px}" \
	".bl .si_{background:var(--ps);color:var(--p)}" \
	".gr .si_{background:var(--grs);color:var(--gr)}" \
	".vi .si_{background:var(--vis);color:var(--vi)}" \
	".cy .si_{background:var(--cys);color:var(--cy)}" \
	".re .si_{background:var(--res);color:var(--re)}" \
	".or .si_{background:var(--ors);color:var(--or)}" \
	".sb_{display:flex;flex-direction:column;gap:3px;min-width:0;flex:1}" \
	".sl_{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.10em;color:var(--t1)}" \
	".sv{white-space:nowrap;font-size:22px;font-weight:700;color:var(--t0);font-variant-numeric:tabular-nums;transition:color .3s}" \
	".sv.mono{font-family:var(--mono);font-size:16px}" \
	".sd{font-size:11px;font-family:var(--mono);color:var(--t1)}" \
	".bl .sd{color:var(--p)}" \
	".gr .sd{color:var(--gr)}" \
	".vi .sd{color:var(--vi)}" \
	".cy .sd{color:var(--cy)}" \
	".re .sd{color:var(--re)}" \
	".or .sd{color:var(--or)}" \
	".gr .sv{color:var(--gr)}" \
	".re .sv{color:var(--re)}" \
	".or .sv{color:var(--or)}" \
	".vi .sv{color:var(--vi)}" \
	".cy .sv{color:var(--cy)}" \
	".bl .sv{color:var(--p)}" \
	".sc::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;opacity:0;transition:opacity .22s;border-radius:var(--r) var(--r) 0 0}" \
		".bl::before{background:linear-gradient(90deg,var(--p),var(--cy))}" \
	".gr::before{background:linear-gradient(90deg,var(--gr),var(--cy))}" \
	".vi::before{background:linear-gradient(90deg,var(--vi),var(--p))}" \
	".cy::before{background:linear-gradient(90deg,var(--cy),var(--vi))}" \
	".re::before{background:linear-gradient(90deg,var(--re),var(--or))}" \
	".or::before{background:linear-gradient(90deg,var(--or),var(--re))}" \
	".sg{position:absolute;width:70px;height:70px;border-radius:50%;right:-15px;top:-15px;opacity:.12;filter:blur(18px);pointer-events:none}" \
	".bl .sg{background:var(--p)}" \
	".gr .sg{background:var(--gr)}" \
	".vi .sg{background:var(--vi)}" \
	".cy .sg{background:var(--cy)}" \
	".re .sg{background:var(--re)}" \
	".or .sg{background:var(--or)}" \
	".sc:nth-child(1){animation-delay:.04s}" \
	".sc:nth-child(2){animation-delay:.08s}" \
	".sc:nth-child(3){animation-delay:.12s}" \
	".sc:nth-child(4){animation-delay:.16s}" \
	".sc:nth-child(5){animation-delay:.20s}" \
	".sc:nth-child(6){animation-delay:.24s}" \
	".sc:nth-child(7){animation-delay:.28s}" \
	".card{background:var(--s1);border:1px solid var(--bd);border-radius:var(--r);transition:border-color .2s;animation:fu .35s var(--ease) .12s both}" \
		".ch{display:flex;align-items:center;justify-content:space-between;padding:13px 18px;border-bottom:1px solid var(--bd);background:var(--s2);border-radius:var(--r) var(--r) 0 0}" \
	".ct{font-size:14px;font-weight:700;color:var(--t0);display:flex;align-items:center;gap:8px}" \
	".ct svg{width:16px;height:16px;color:var(--p);flex-shrink:0}" \
	".cb{padding:16px 18px}" \
	".shd{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;margin-top:4px}" \
	".stl{font-size:14px;font-weight:700;color:var(--t0);display:flex;align-items:center;gap:8px}" \
	".stl::before{content:'';display:inline-block;width:3px;height:14px;background:var(--p);border-radius:2px}" \
	".tw{border:1px solid var(--bd);border-radius:var(--r);overflow:auto;margin-bottom:16px}" \
	"table{width:100%;border-collapse:collapse;font-size:13px}" \
	"thead tr{background:var(--s2)}" \
	"th{padding:9px 14px;text-align:left;font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.10em;color:var(--t2);border-bottom:1px solid var(--bd);white-space:nowrap}" \
	"td{padding:10px 14px;border-bottom:1px solid var(--bd);color:var(--t0)}" \
	"tbody tr:last-child td{border-bottom:none}" \
		"tbody tr.nw{animation:rf .5s ease}" \
	"@keyframes rf{" \
	"  from{background:rgba(59,130,246,.18)}" \
	"  to{background:transparent}" \
	"}" \
	".mono{font-family:var(--mono);font-size:12px}" \
	".bold{font-weight:600}" \
	".badge{display:inline-flex;align-items:center;gap:4px;padding:2px 9px;border-radius:var(--rxs);font-size:11px;font-weight:700;font-family:var(--mono);letter-spacing:.04em}" \
	".bon{background:var(--grs);color:var(--gr);border:1px solid rgba(34,197,94,.22)}" \
	".boff{background:var(--res);color:var(--re);border:1px solid rgba(239,68,68,.22)}" \
	".bban{background:var(--ors);color:var(--or2);border:1px solid rgba(249,115,22,.22)}" \
	".bbl{background:var(--ps);color:var(--p);border:1px solid var(--pg)}" \
	".bcy{background:var(--cys);color:var(--cy);border:1px solid var(--cyg)}" \
	".kb{display:inline-flex;align-items:center;padding:4px 7px;border-radius:var(--rxs);color:var(--re);opacity:.4;transition:opacity .15s,background .15s}" \
	".kb svg{width:13px;height:13px}" \
	".pw-btn{display:inline-flex;align-items:center;justify-content:center;width:28px;height:28px;border-radius:6px;border:none;cursor:pointer;transition:all .15s;padding:0}" \
	".pw-btn svg{width:15px;height:15px;pointer-events:none}" \
	".pw-btn.on{background:rgba(74,222,128,.15);color:#4ade80}" \
	".pw-btn.off{background:var(--s3);color:var(--t2);opacity:.5}" \
	".u-link{cursor:pointer;color:var(--t1)}" \
	".ctab{padding:7px 16px;font-size:12px;font-weight:600;letter-spacing:.05em;border:1px solid var(--bd);border-bottom:none;border-radius:6px 6px 0 0;background:var(--s2);color:var(--t2);cursor:pointer;transition:all .15s}" \
	".ctab.act{background:var(--ps);color:var(--p);border-color:var(--pg);border-bottom:2px solid var(--p)}" \
	".hbw{background:var(--s3);border-radius:4px;height:5px;width:80px;overflow:hidden}" \
	".hbf{height:100%;border-radius:4px;background:linear-gradient(90deg,var(--gr),var(--cy));transition:width .4s}" \
	".lc{display:flex;align-items:center;justify-content:center;gap:8px;margin-bottom:12px;flex-wrap:wrap}" \
	".ls{background:var(--s2);border:1px solid var(--bd2);color:var(--t0);border-radius:var(--rsm);padding:5px 10px;font-family:var(--mono);font-size:12px;width:220px;outline:none}" \
	".ls:focus{border-color:var(--pg)}" \
	"select.lsel{background:var(--s2);color:var(--t1);border:1px solid var(--bd2);border-radius:var(--rsm);padding:5px 8px;font-size:12px}" \
	"#lw{background:var(--term);border:1px solid var(--bd);border-radius:var(--r);height:calc(100vh - 310px);min-height:320px;overflow:auto;padding:14px 4px 14px 14px;position:relative;scroll-behavior:smooth}" \
	"#lw::before{content:'LIVE';position:absolute;top:10px;right:12px;font-size:9px;font-family:var(--mono);font-weight:700;letter-spacing:.12em;color:var(--gr);opacity:.4;pointer-events:none}" \
	"#lp{margin:0;font-family:var(--mono);font-size:12.5px;line-height:1.85;color:var(--t1)}" \
	"#lp span{display:block;white-space:pre;border-radius:2px;padding:0 4px}" \
	".lok{color:#4ade80;font-weight:700}" \
	".lwarn{color:var(--or2);font-weight:700}" \
	".lerr{color:var(--re);font-weight:700}" \
	".lnet{color:#c084fc;font-weight:700}" \
	".lwebif{color:#60a5fa;font-weight:700}" \
	".lban{color:var(--or2);font-weight:700}" \
	".lt2{color:var(--t2)}" \
	".db{background:var(--s2);border:1px solid var(--bd);border-radius:var(--rsm);padding:8px 12px;margin-bottom:12px;display:flex;flex-wrap:wrap;align-items:center;justify-content:center;gap:5px}" \
	".dt{display:inline-flex;align-items:center;padding:3px 10px;border-radius:var(--rxs);font-size:11px;font-family:var(--mono);font-weight:500;cursor:pointer;border:1px solid var(--bd);color:var(--t2);transition:all .15s;user-select:none}" \
	".dt.on{background:var(--ps);border-color:var(--pg);color:var(--p)}" \
	".dm{font-size:11px;color:var(--t2);font-family:var(--mono)}" \
	".et{display:flex;align-items:center;justify-content:center;gap:10px;padding:10px 16px;border-bottom:1px solid var(--bd);background:var(--s2);border-radius:var(--r) var(--r) 0 0}" \
	".ef{font-family:var(--mono);font-size:12px;color:var(--p);display:flex;align-items:center;gap:8px}" \
	".ef svg{width:14px;height:14px;opacity:.6}" \
	".ew{background:var(--ors);border-top:1px solid rgba(249,115,22,.25);padding:7px 16px;font-family:var(--mono);font-size:12px;color:var(--or2);display:flex;align-items:center;gap:6px}" \
	".ew svg{width:13px;height:13px;flex-shrink:0}" \
	".ea{font-family:var(--mono);font-size:13px;line-height:1.9;color:#a5d6a7;background:#040810;border:none;outline:none;width:100%;min-height:390px;padding:14px 16px;resize:vertical}" \
	".ef2{display:flex;align-items:center;justify-content:center;gap:10px;padding:8px 16px;border-top:1px solid var(--bd);background:var(--s2);border-radius:0 0 var(--r) var(--r)}" \
	".es{font-family:var(--mono);font-size:11px;color:var(--t1);text-align:center}" \
	".es .ok{color:var(--gr)}" \
	".lb{min-height:100vh;display:flex;align-items:center;justify-content:center;background:var(--bg);background-image:radial-gradient(ellipse at 20% 50%,rgba(59,130,246,.06) 0%,transparent 60%),radial-gradient(ellipse at 80% 20%,rgba(6,182,212,.06) 0%,transparent 60%)}" \
	".lcard{background:var(--s2);border:1px solid var(--bd);border-radius:14px;padding:30px 36px;width:350px;box-shadow:var(--shadow-lg)}" \
	".ll{display:flex;align-items:center;gap:12px;margin-bottom:24px}" \
	".lli{width:46px;height:46px;background:var(--ps);border:1px solid var(--pg);border-radius:11px;display:grid;place-items:center;flex-shrink:0}" \
	".lli svg{width:26px;height:26px}" \
	".llt{font-size:20px;font-weight:700;color:var(--t0)}" \
	".llv{font-size:11px;color:var(--t1);font-family:var(--mono);margin-top:2px}" \
	".fld{display:block;font-size:11px;font-weight:600;color:var(--t1);letter-spacing:.05em;margin-bottom:5px}" \
	".fi{width:100%;padding:9px 12px;background:var(--s1);border:1px solid var(--bd);color:var(--t0);border-radius:var(--rsm);font-size:13px;font-family:var(--sans);transition:border-color .18s;outline:none}" \
	".fi:focus{border-color:var(--pg)}" \
	".fg{margin-bottom:14px}" \
	".le{display:flex;align-items:center;gap:8px;background:var(--res);border:1px solid rgba(239,68,68,.28);border-radius:var(--rsm);padding:9px 12px;color:var(--re);font-size:12px;margin-bottom:16px}" \
	".le svg{width:14px;height:14px;flex-shrink:0}" \
	".dlg{background:var(--s2);border:1px solid var(--bd);border-radius:14px;padding:32px;max-width:460px}" \
	".dico{width:60px;height:60px;border-radius:14px;display:grid;place-items:center;margin:0 auto 18px;flex-shrink:0}" \
	".dico svg{width:30px;height:30px}" \
	".dico.danger{background:var(--res);color:var(--re)}" \
	".dico.info{background:var(--ps);color:var(--p)}" \
	".dico.warn{background:var(--ors);color:var(--or)}" \
	".dlg h2{font-size:17px;font-weight:700;color:var(--t0);text-align:center;margin-bottom:8px}" \
	".dlg p{color:var(--t1);font-size:13px;text-align:center;margin-bottom:16px;line-height:1.6}" \
	".da{display:flex;gap:10px;justify-content:center;flex-wrap:wrap}" \
	".done-card{background:var(--s2);border:1px solid var(--bd);border-radius:14px;padding:32px;max-width:400px;text-align:center}" \
	".pg-center{display:flex;justify-content:center;padding-top:20px}" \
	".ib2{background:var(--s2);border:1px solid var(--bd);border-radius:var(--rsm);padding:10px 14px;margin-bottom:12px;font-size:12px;color:var(--t1)}" \
	".ib2 svg{width:13px;height:13px;vertical-align:-2px;margin-right:4px}" \
	".tg{color:var(--gr)}" \
	".tr{color:var(--re)}" \
	".to{color:var(--or2)}" \
	".tb{color:var(--p)}" \
	".tv{color:var(--vi)}" \
	".tc{color:var(--cy)}" \
	".tm{color:var(--t1)}" \
	".flex{display:flex;align-items:center}" \
	".gap8{gap:8px}" \
	".gap10{gap:10px}" \
	".mb16{margin-bottom:16px}" \
	".mb10{margin-bottom:10px}" \
	"a.danger{color:var(--re)}" \
	"hr{border:none;border-top:1px solid var(--bd);margin:14px 0}" \
	".erow td{text-align:center;color:var(--t1);padding:22px}" \
	"input[type=checkbox]{accent-color:var(--p)}" \
	"label{cursor:pointer}" \
	".tip{position:relative}" \
	".tipt{display:none;position:absolute;bottom:calc(100% + 6px);left:50%;transform:translateX(-50%);background:var(--s4);border:1px solid var(--bd2);border-radius:5px;padding:4px 8px;font-size:11px;color:var(--t0);white-space:nowrap;z-index:300;pointer-events:none}" \
	"@keyframes fu{" \
	"  from{opacity:0;transform:translateY(10px)}" \
	"  to{opacity:1;transform:translateY(0)}" \
	"}" \
	"@keyframes cnt{" \
	"  from{opacity:0;transform:scale(.85)}" \
	"  to{opacity:1;transform:scale(1)}" \
	"}" \
	".cnt-up{animation:cnt .4s var(--ease)}" \
	"::-webkit-scrollbar{width:5px;height:5px}" \
	"::-webkit-scrollbar-track{background:transparent}" \
	"::-webkit-scrollbar-thumb{background:var(--bd2);border-radius:4px}" \
	"th.sort-asc::after{content:' \\2191';color:var(--p)}" \
	"th.sort-desc::after{content:' \\2193';color:var(--p)}" \
	".ttb{display:flex;align-items:center;justify-content:center;gap:10px;margin-bottom:12px;flex-wrap:wrap}" \
	".ttb-r{display:flex;gap:8px;align-items:center}" \
	".tsrch{background:var(--s2);border:1px solid var(--bd2);color:var(--t0);border-radius:var(--rsm);padding:6px 12px;font-family:var(--mono);font-size:12px;width:160px;outline:none;transition:border-color .18s}" \
	".tsrch:focus{border-color:var(--pg)}" \
	".tsrch::placeholder{color:var(--t2)}" \
	"tfoot tr{background:var(--s2)}" \
	"tfoot td{padding:9px 14px;font-size:11px;font-weight:700;color:var(--t2);border-top:2px solid var(--bd);letter-spacing:.04em}" \
	"tfoot .tfs{font-size:13px;color:var(--t0)}" \
	"tfoot .tfl{font-size:10px;color:var(--t2);text-transform:uppercase;letter-spacing:.1em}" \
	".sbar{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));border:1px solid var(--bd);border-radius:var(--r);overflow:hidden;margin-bottom:16px;background:var(--s1);max-width:660px;margin-left:auto;margin-right:auto}" \
	".sbar-item{padding:12px 16px;border-right:1px solid var(--bd);display:flex;flex-direction:column;gap:3px;transition:background .18s}" \
	".sbar-item:last-child{border-right:none}" \
	".sbl{font-size:9px;font-weight:700;text-transform:uppercase;letter-spacing:.12em;color:var(--t2)}" \
	".sbv{font-size:18px;font-weight:700;color:var(--t0);font-variant-numeric:tabular-nums}" \
	".sbv.sm{font-size:13px;font-family:var(--mono)}" \
	".sbv.tg{color:var(--gr)}" \
	".sbv.tr{color:var(--re)}" \
	".sbv.tb{color:var(--p)}" \
	".sbv.to{color:var(--or2)}" \
	".row2{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-bottom:16px}" \
	".row2.r7030{grid-template-columns:7fr 3fr}" \
	".row2.r6040{grid-template-columns:6fr 4fr}" \
	"@media(max-width:900px){" \
	"  .row2,.row2.r7030,.row2.r6040{grid-template-columns:1fr}" \
	"}" \
	".ecm-bk{display:flex;gap:2px;height:6px;border-radius:4px;overflow:hidden;margin:6px 0 2px;width:100%;background:var(--s3)}" \
	".ecm-bk span{height:100%;transition:width .4s}" \
	".ecm-ok{background:var(--gr)}" \
	".ecm-nok{background:var(--re)}" \
	".msr{display:flex;flex-wrap:wrap;gap:8px 16px;font-size:12px;font-family:var(--mono)}" \
	".msr-kv{display:flex;gap:5px;align-items:center}" \
	".msr-k{color:var(--t2);font-size:10px;text-transform:uppercase;letter-spacing:.08em;font-family:var(--sans);font-weight:600}" \
	".msr-v{color:var(--t0);font-weight:600}" \
	"#mnuBtn{display:none;width:34px;height:34px;border-radius:var(--rsm);background:var(--s2);border:1px solid var(--bd);place-items:center;color:var(--t1);cursor:pointer;flex-shrink:0}" \
	"#mnuBtn svg{width:16px;height:16px}" \
	"@media(max-width:1099px){" \
	"  #mnuBtn{display:grid}" \
	"  .tnav{display:none;position:fixed;top:var(--tbh);left:0;right:0;bottom:0; background:var(--navbg);z-index:1020;flex-direction:column;padding:14px; overflow-y:auto;gap:4px}" \
	"  .tnav.open{display:flex}" \
	"  .tnav .sep{display:none}" \
	"  .tnav a{font-size:14px;padding:10px 14px}" \
	"  .tbr .chip,.tbr .pc{display:none}" \
	"}" \
	".ll-meta{display:flex;align-items:center;gap:5px;font-size:11px;font-family:var(--mono);color:var(--t2)}" \
	".ll-dot{width:7px;height:7px;border-radius:50%;background:var(--gr);animation:pa 2s ease-in-out infinite;flex-shrink:0}" \
	".ll-mask{color:var(--p)}" \
	".ll-row{display:flex;flex-wrap:wrap;align-items:center;gap:6px;justify-content:center}" \
	".ll-label{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.1em;color:var(--t2);white-space:nowrap}" \
	".ll-sep{width:1px;height:18px;background:var(--bd2);flex-shrink:0}" \
	".ll-chk{display:flex;align-items:center;font-size:12px;color:var(--t1);white-space:nowrap;gap:4px;cursor:pointer}" \
	".ll-toolbar{padding:4px 0}" \
	".ll-ch{display:flex;flex-direction:column;align-items:center;padding:13px 18px;border-bottom:1px solid var(--bd);background:var(--s2);border-radius:var(--r) var(--r) 0 0;gap:8px}" \
	".ll-hdr-right{display:flex;align-items:center;gap:5px;flex-wrap:wrap;justify-content:center}" \
	".ll-dbrow{display:flex;align-items:center;gap:5px;flex-wrap:wrap;justify-content:center}" \
	".ll-vsep{width:1px;height:18px;background:var(--bd2);flex-shrink:0;margin:0 4px}" \
	".ll-toolbar2{display:flex;flex-wrap:wrap;align-items:center;gap:6px;justify-content:center}" \
	".flagcol{display:inline-block;min-width:20px;font-size:17px;line-height:1;text-align:center;vertical-align:-2px}" \
	".flagcol:empty::after{content:'\\2014';color:var(--t3);font-size:12px}" \
	".uipflag{display:inline-block;min-width:24px;margin-right:7px;font-size:18px;line-height:1;text-align:center;vertical-align:-2px}" \
	".uipflag:empty{display:none}" \
	\
	                                     \
	":root{color-scheme:dark;--term:#030b14;--navbg:rgba(9,13,20,.97);--pop:#1a2340;--shadow:none;--shadow-lg:0 18px 42px rgba(0,0,0,.42);--ovl:rgba(3,7,14,.72);--focus:0 0 0 3px var(--pg)}" \
	":root[data-theme=light]{color-scheme:light;--bg:#e8ebef;--s1:#eef1f4;--s2:#e3e7eb;--s3:#d9dee4;--s4:#cfd5dc;--bd:#c5ccd4;--bd2:#aeb7c1;--p:#1f5bdc;--p2:#1a4fc0;--ps:rgba(31,91,220,.09);--pg:rgba(31,91,220,.28);--vi:#6d28d9;--vis:rgba(109,40,217,.09);--vig:rgba(109,40,217,.24);--cy:#0c6a84;--cys:rgba(12,106,132,.10);--cyg:rgba(12,106,132,.26);--gr:#147a3a;--grs:rgba(20,122,58,.10);--grg:rgba(20,122,58,.26);--re:#c81e1e;--res:rgba(200,30,30,.08);--or:#b93a0b;--or2:#a8460a;--ors:rgba(185,58,11,.09);--t0:#182230;--t1:#334155;--t2:#566274;--navbg:rgba(232,235,239,.98);--pop:#f6f7f9;--shadow:0 1px 2px rgba(15,23,42,.07),0 1px 3px rgba(15,23,42,.05);--shadow-lg:0 16px 36px rgba(15,23,42,.13);--ovl:rgba(15,23,42,.42)}" \
	":root[data-theme=light] #lw,:root[data-theme=light] .ea{--t0:#e8f0fe;--t1:#94a3b8;--t2:#7a91ad;--bd:#1e2d47}" \
	"[hidden]{display:none!important}" \
	".site-footer{padding:7px 18px;border-top:1px solid var(--bd);display:flex;align-items:center;justify-content:center;gap:6px;flex-wrap:wrap;font-size:10px;color:var(--t2);font-family:var(--mono);min-height:24px;box-sizing:border-box}" \
	".site-footer-version{color:var(--p)}" \
	".site-footer-sep{opacity:.55}" \
	".qtip{width:17px;height:17px;display:inline-grid;place-items:center;margin-left:5px;padding:0;border:1px solid var(--bd2);border-radius:50%;background:var(--s2);color:var(--t2);font:700 10px/1 var(--sans);vertical-align:1px;cursor:help;flex:0 0 auto}" \
	".qtip-inline{margin-top:0;vertical-align:middle}" \
	".cfg-head .cfg-title{display:flex;align-items:center;gap:4px}" \
	".cfg-sub,.cfg-help,.fhint{font-size:0!important;margin:0!important;line-height:0!important}" \
		".cfg-intro{display:none!important}" \
	".cfg-card{margin-bottom:8px!important}" \
	".cfg-head{padding:9px 12px!important;gap:8px!important}" \
	".cfg-title{font-size:12.5px!important}" \
	".cfg-body{padding:10px 12px!important}" \
	".cfg-grid{gap:8px 10px!important}" \
	".cfg-label{margin-bottom:4px!important;font-size:10px!important}" \
	".cfg-input{padding:7px 8px!important;font-size:12px!important}" \
	".cfg-input[type=number]{width:104px;max-width:100%;font-family:var(--mono)}" \
	".cfg-input[type=date]{width:132px;max-width:100%}" \
	".cfg-toggle-row{margin-top:8px!important;gap:6px!important}" \
	".cfg-toggle{padding:6px 8px!important;font-size:11px!important}" \
	".cfg-save{margin:12px 0 2px!important}" \
	"#ct{max-width:1440px;padding:12px 16px 24px}" \
	".ph{margin-bottom:8px!important;gap:6px!important}" \
	".card .ch{padding:9px 12px!important}" \
	".card .cb{padding:11px 12px!important}" \
	".ct{font-size:12.5px!important}" \
	".stl{font-size:12px!important}" \
	".shd{margin-bottom:8px!important;margin-top:2px!important}" \
	".tw{margin-bottom:10px!important}" \
	"th{padding:7px 10px;font-size:9.5px}" \
	"td{padding:8px 10px;font-size:12px}" \
	".sc{padding:10px 11px!important;gap:10px!important;border-radius:8px!important}" \
	".si_{width:34px!important;height:34px!important;border-radius:7px!important}" \
	".si_ svg{width:18px!important;height:18px!important}" \
	".sl_{font-size:8.5px!important;letter-spacing:.08em!important}" \
	".sv{font-size:18px!important;line-height:1.15!important}" \
	".sv.mono{font-size:13px!important}" \
	".sd{font-size:9.5px!important}" \
	".cg{gap:9px!important;margin-bottom:10px!important;grid-auto-rows:minmax(0,1fr)}" \
	"@media(min-width:1180px){.cg{grid-template-columns:repeat(6,minmax(0,1fr))}}" \
	".mc{border-radius:10px!important;max-height:calc(100vh - 20px)!important}" \
	".mh{padding:10px 12px!important}" \
	".mh h2{font-size:13px!important}" \
	".mb{padding:12px 14px 4px!important}" \
	".mf{padding:10px 14px!important}" \
	".dlg{padding:20px!important;max-width:420px!important;border-radius:10px!important}" \
	".dico{width:42px!important;height:42px!important;margin:0 auto 10px!important;border-radius:10px!important}" \
	".dico svg{width:22px!important;height:22px!important}" \
	".dlg h2{font-size:14px!important;margin-bottom:5px!important}" \
	".dlg p{font-size:11.5px!important;margin-bottom:12px!important;line-height:1.45!important}" \
	".done-card{padding:20px!important;max-width:360px!important;border-radius:10px!important}" \
	".pg-center{padding-top:12px!important}" \
	".reader-modal{width:700px!important}" \
	".user-modal{width:560px!important}" \
	".reader-modal .mb,.user-modal .mb{padding:12px 14px 3px!important}" \
	".reader-modal .mf,.user-modal .mf{padding:10px 14px!important}" \
	".reader-modal .fg,.user-modal .fg{margin-bottom:8px!important}" \
	".fi[type=number]{width:100px;max-width:100%}" \
	".fi[type=date]{width:132px;max-width:100%}" \
	".fi[type=time]{width:108px;max-width:100%}" \
	".reader-modal .shd{margin-top:1px!important;margin-bottom:6px!important}" \
	".reader-modal .shd .stl{font-size:11px!important}" \
		".file-card{border-radius:8px!important}" \
	".file-actions{padding:8px 10px!important}" \
	".file-info{font-size:10px!important}" \
	".file-tabs{gap:2px!important}" \
	"body.pg-power .card{padding:16px 14px!important}" \
	"body.pg-power .dico{margin-bottom:8px!important}" \
	"body.pg-tvcas .tv-card{padding:12px 14px!important;margin-bottom:10px!important}" \
	"body.pg-tvcas .tv-tabs{margin-bottom:10px!important}" \
	":where(a,button,input,select,textarea,summary,[tabindex]):focus-visible{outline:2px solid var(--p);outline-offset:2px}" \
	".fi:focus,.tsrch:focus,.ls:focus{border-color:var(--p);box-shadow:var(--focus)}" \
	".card,.sc,.tw,.sbar,.done-card,.dlg{box-shadow:var(--shadow)}" \
	"svg.i{fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}" \
	"@media(prefers-reduced-motion:reduce){" \
	"  *,*::before,*::after{animation-duration:.01ms!important;animation-iteration-count:1!important;transition-duration:.01ms!important;scroll-behavior:auto!important}" \
	"  .pulse{animation:none!important}" \
	"}" \
	".acp{position:relative}" \
	".acpop{position:absolute;inset-inline-end:0;top:calc(100% + 8px);z-index:60;display:flex;gap:8px;padding:9px;background:var(--s2);border:1px solid var(--bd2);border-radius:var(--rsm);box-shadow:var(--shadow-lg,0 12px 32px rgba(0,0,0,.35))}" \
	".ac-swatch{width:22px;height:22px;border-radius:50%;border:2px solid transparent;cursor:pointer;padding:0;display:inline-flex;flex-shrink:0}" \
	".ac-swatch.cur{border-color:var(--t0)}" \
	".thb{width:34px;height:34px;display:grid;place-items:center;flex-shrink:0;border-radius:var(--rsm);background:var(--s2);border:1px solid var(--bd);color:var(--t1);transition:background .15s,color .15s,border-color .15s}" \
	".thb svg{width:16px;height:16px;display:none}" \
	"[data-tpref=dark] .thb .ti-d,[data-tpref=light] .thb .ti-l{display:block}" \
	".acp .thb svg{display:block;color:var(--p)}" \
	\
	                                                        \
	"#p_up{font-size:19px}" \
	"@media(min-width:1180px){" \
	"  .cg{grid-template-columns:repeat(5,minmax(0,1fr))}" \
	"}" \
	"body.pg-config .card{padding:16px 18px}" \
	\
	                                                                            \
	".sw{position:relative;flex:0 0 auto;width:38px;height:22px;border-radius:999px;background:var(--s4);border:1px solid var(--bd2);transition:background .18s,border-color .18s;padding:0}" \
	".sw i{position:absolute;top:2px;left:2px;width:16px;height:16px;border-radius:50%;background:var(--t1);transition:transform .18s,background .18s}" \
	".sw.on{background:var(--gr);border-color:var(--gr)}" \
	".sw.on i{transform:translateX(16px);background:#fff}" \
	".sw[disabled]{opacity:.5;cursor:progress}" \
	".ib{width:30px;height:30px;display:inline-grid;place-items:center;flex-shrink:0;border-radius:var(--rsm);color:var(--t1);border:1px solid transparent;transition:background .15s,color .15s,border-color .15s}" \
	".ib svg{width:15px;height:15px}" \
	".mo{position:fixed;inset:0;z-index:2000;display:flex;align-items:center;justify-content:center;padding:16px;background:var(--ovl);animation:mfi .15s ease-out}" \
	".mc{width:440px;max-width:100%;max-height:calc(100vh - 28px);display:flex;flex-direction:column;background:var(--s1);border:1px solid var(--bd2);border-radius:14px;box-shadow:var(--shadow-lg);animation:mpop .15s var(--ease)}" \
	".mc.sm{width:400px}" \
	".mh{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:14px 16px 14px 20px;border-bottom:1px solid var(--bd)}" \
	".mh h2{font-size:15px;font-weight:700}" \
	".mb{padding:18px 20px 6px;overflow:auto}" \
	".mf{display:flex;justify-content:flex-end;gap:8px;padding:14px 20px;border-top:1px solid var(--bd);background:var(--s2);border-radius:0 0 14px 14px}" \
	".mc .fld{text-transform:uppercase;font-size:10.5px;letter-spacing:.08em}" \
	".mc .fi{background:var(--bg)}" \
	".fi:disabled{opacity:.6;cursor:not-allowed}" \
	".g2{display:grid;grid-template-columns:1fr 1fr;gap:12px}" \
	".chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}" \
	".chip2{padding:3px 10px;border-radius:999px;border:1px solid var(--bd2);background:var(--s2);color:var(--t1);font-size:11.5px;font-weight:600;transition:all .15s}" \
	".swrow{display:flex;align-items:center;justify-content:space-between;gap:16px;padding:12px 14px;border:1px solid var(--bd);border-radius:var(--rsm);background:var(--s2);margin:2px 0 14px}" \
	".swt{font-weight:600;font-size:13px}" \
	".sws{font-size:11.5px;color:var(--t2);margin-top:2px}" \
	".mc .le{margin:0 0 14px}" \
	".cf{display:flex;gap:14px;padding:20px 20px 18px}" \
	".cfi{width:40px;height:40px;border-radius:10px;display:grid;place-items:center;flex-shrink:0}" \
	".cfi svg{width:20px;height:20px}" \
	".cfi.danger{background:var(--res);color:var(--re)}" \
	".cfi.warn{background:var(--ors);color:var(--or)}" \
	".cft{font-size:15px;font-weight:700;margin-bottom:4px}" \
	".cfm{font-size:13px;color:var(--t1);line-height:1.55;word-break:break-word}" \
	".btn.bdz{background:var(--re);color:#fff;border-color:var(--re)}" \
	".btn[disabled]{opacity:.6;cursor:progress}" \
	"body.mo-open{overflow:hidden}" \
	".toasts{position:fixed;left:50%;bottom:22px;transform:translateX(-50%);z-index:3000;display:flex;flex-direction:column;gap:8px;align-items:center;pointer-events:none;width:max-content;max-width:calc(100vw - 24px)}" \
	".toast{display:flex;align-items:center;gap:9px;padding:10px 14px;background:var(--pop);color:var(--t0);border:1px solid var(--bd2);border-left:3px solid var(--p);border-radius:var(--rsm);box-shadow:var(--shadow-lg);font-size:13px;animation:tin .2s var(--ease)}" \
	".toast.ok{border-left-color:var(--gr)}" \
	".toast.err{border-left-color:var(--re)}" \
	"@keyframes mfi{" \
	"  from{opacity:0}" \
	"  to{opacity:1}" \
	"}" \
	"@keyframes mpop{" \
	"  from{opacity:0;transform:translateY(8px) scale(.98)}" \
	"  to{opacity:1;transform:none}" \
	"}" \
	"@keyframes tin{" \
	"  from{opacity:0;transform:translateY(8px)}" \
	"  to{opacity:1;transform:none}" \
	"}" \
	\
	                                                                   \
	"@media(max-width:1279px){" \
	"  .tbr .chip,.tbr .pc{display:none}" \
	"}" \
	".tnav{min-width:0}" \
	\
	                                                           \
	".tw.cm{background:var(--s1)}" \
	".cm table{table-layout:fixed;font-size:13px}" \
	".cm.auto table{table-layout:auto}" \
	".cm.auto th,.cm.auto td{padding:0 12px}" \
	".cm td.bold{font-weight:700}" \
	".cm th,.cm td{height:30px;padding:0 7px;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;border-bottom:1px solid var(--bd);border-right:1px solid var(--bd)}" \
	".cm th:last-child,.cm td:last-child{border-right:0}" \
	".cm th{position:sticky;top:0;z-index:1;background:var(--s2);color:var(--t2);font-size:11px;font-weight:700;letter-spacing:.01em;text-transform:none}" \
	".cm td{font-weight:500}" \
	".cm tbody tr:nth-child(even){background:rgba(127,140,150,.045)}" \
		".cm tbody tr:last-child td{border-bottom:none}" \
	".cm .erow td{padding:22px}" \
	".table-sort{width:100%;height:30px;padding:0 4px;display:inline-flex;align-items:center;justify-content:center;gap:4px;color:inherit;font:inherit;cursor:pointer;transition:color .15s}" \
		".table-sort .sort-arrow{font-size:10px;line-height:1;opacity:.55}" \
	".table-sort .sort-arrow::after{content:\"\\2195\"}" \
	".table-sort[data-dir=asc] .sort-arrow::after{content:\"\\2191\"}" \
	".table-sort[data-dir=desc] .sort-arrow::after{content:\"\\2193\"}" \
	".table-sort[data-dir=asc] .sort-arrow,.table-sort[data-dir=desc] .sort-arrow{opacity:1;color:var(--p)}" \
	".sdot{display:inline-block;flex:none;width:9px;height:9px;margin-left:7px;border-radius:50%;vertical-align:middle}" \
	".sdot.valid{background:var(--gr)}" \
	".sdot.expiring{background:var(--or)}" \
	".sdot.expired{background:var(--re)}" \
	".sdot.stale{background:var(--or)}" \
	".sdot.online{margin:0 7px 0 0;background:var(--gr);animation:pa 2s ease-in-out infinite}" \
	".urow{transition:background-color .16s ease,box-shadow .16s ease}" \
		".urow[data-vis='online'],.urow[data-vis='stale'],.urow[data-vis='expired'],.urow[data-vis='disabled']{background:transparent}" \
		".urow[data-active='1'] td{background:rgba(74,222,128,.14);color:var(--t0)}" \
	".urow[data-active='1']{box-shadow:inset 4px 0 0 var(--gr),0 0 0 1px rgba(74,222,128,.10)}" \
		".urow[data-vis='stale']{box-shadow:inset 3px 0 0 0 var(--or)}" \
		".urow[data-vis='expired']{box-shadow:inset 3px 0 0 0 var(--re)}" \
		".urow[data-vis='disabled']{box-shadow:inset 3px 0 0 0 var(--bd2)}" \
		"@media (hover:hover) and (pointer:fine){" \
		".urow[data-active='1']:hover td{background:rgba(74,222,128,.18)}" \
		":root[data-theme=light] .urow[data-active='1']:hover{background:linear-gradient(90deg,rgba(20,122,58,.10),rgba(20,122,58,.03) 60%)}" \
		".urow[data-vis='stale']:hover{background:linear-gradient(90deg,rgba(249,115,22,.12),rgba(249,115,22,.03) 60%)}" \
		".urow[data-vis='expired']:hover{background:linear-gradient(90deg,rgba(239,68,68,.12),rgba(239,68,68,.03) 60%)}" \
		".urow[data-vis='disabled']:hover{background:linear-gradient(90deg,rgba(127,140,150,.10),rgba(127,140,150,.025) 60%)}" \
		"}" \
		".tool{height:30px;padding:0 10px;display:inline-flex;align-items:center;justify-content:center;gap:6px;white-space:nowrap;color:var(--t0);background:var(--s1);border:1px solid var(--bd2);border-radius:var(--rsm);box-shadow:var(--shadow);transition:background .15s,border-color .15s,color .15s}" \
	".tool svg{width:14px;height:14px}" \
	".tool.act{color:#fff;background:var(--p);border-color:var(--p)}" \
	".tool .n{min-width:18px;padding:0 5px;font-family:var(--mono);font-size:11px;font-weight:700;line-height:16px;border-radius:9px;background:var(--s3);color:var(--t1)}" \
	".tool.act .n{background:rgba(255,255,255,.22);color:#fff}" \
	".tool.pri{color:#fff;background:var(--p);border-color:var(--p)}" \
	".tool.sm{height:24px;padding:0 8px;font-size:11.5px;gap:4px}" \
	".tool.sm svg{width:12px;height:12px}" \
	".tool.danger{color:var(--tr);border-color:var(--res)}" \
	".tool .g{color:var(--gr)}" \
	".tool .pu{color:var(--vi)}" \
	\
	                                                             \
	"body.pg-readers{height:100vh;overflow:hidden;padding-top:var(--tbh)}" \
	"body.pg-readers footer:not(.ufoot){display:none!important}" \
	"body.pg-readers #mn{height:100%;min-height:0;margin-top:0}" \
	"body.pg-readers #ct{display:flex;flex-direction:column;gap:8px;max-width:none;height:100%;padding:10px 12px 8px}" \
	".rtoolbar{grid-template-columns:minmax(0,1fr) 260px auto}" \
	".rtoolbar #rStats{overflow-x:auto;scrollbar-width:thin;padding-bottom:1px}" \
	"#rTable{flex:1;min-height:0;margin:0;overflow:auto;position:relative}" \
	"#rTable table{min-width:940px}" \
	".rt th[data-k]{padding:0}" \
	".rt .c-rstate{width:92px;white-space:nowrap}" \
	".rt .c-rlabel{width:160px}" \
	".rt .c-rproto{width:95px}" \
	".rt .c-rdev{min-width:220px;max-width:300px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}" \
	".rt .c-rgroup{width:100px}" \
	".rt .c-rcaid{width:110px}" \
	".rt .c-rstat-ok{width:72px;text-align:right}" \
	".rt .c-rstat-nok{width:72px;text-align:right}" \
	".rt .c-btn{width:108px}" \
	".rlink{max-width:100%;padding:4px 2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:var(--t1);font-family:var(--mono);font-size:13px;font-weight:700;border-radius:4px;transition:background .15s}" \
	".rdot{display:inline-block;width:8px;height:8px;margin-right:6px;border-radius:50%;vertical-align:middle;background:var(--t2);box-shadow:0 0 0 3px rgba(148,163,184,.08)}" \
	".rdot.on{background:var(--gr);box-shadow:0 0 0 3px rgba(74,222,128,.10)}" \
	".rdot.off{background:var(--t2)}" \
	".rstate{font-size:11px;color:var(--t2)}" \
	".rrow[data-active='1'] td{background:rgba(74,222,128,.14);color:var(--t0)}" \
	".rrow[data-active='1']{box-shadow:inset 4px 0 0 var(--gr),0 0 0 1px rgba(74,222,128,.10)}" \
	".rrow[data-enabled='0'] td:not(.c-rstate):not(.c-btn):not(.c-rlabel){opacity:.5}" \
	".rrow[data-enabled='0'] .rlink{opacity:.65}" \
	".act-b.on{color:var(--gr);background:rgba(74,222,128,.10);border-color:rgba(74,222,128,.28)}" \
	".act-b.off{color:var(--t2);background:var(--s2)}" \
	"#rEmpty{position:absolute;inset:48px 0 0;display:flex;flex-direction:column;align-items:center;justify-content:center}" \
	"#rEmpty[hidden]{display:none}" \
	"#rEmpty svg{width:34px;height:34px;margin-bottom:8px;color:var(--t2)}" \
	"#rEmpty b{display:block;margin-bottom:4px;font-size:15px;color:var(--t0)}" \
	"#rEmpty span{color:var(--t2);font-size:12px}" \
	"#rEmpty .tool{margin-top:14px}" \
	"@media(max-width:900px){.rtoolbar{grid-template-columns:1fr;justify-items:stretch}.rtoolbar .usrch{max-width:none}.rt{min-width:940px}}" \
	"body.pg-users{height:100vh;overflow:hidden;padding-top:var(--tbh)}" \
	"body.pg-users footer:not(.ufoot){display:none!important}" \
	"body.pg-users #mn{height:100%;min-height:0;margin-top:0}" \
	"body.pg-users #ct{display:flex;flex-direction:column;gap:8px;max-width:none;height:100%;padding:10px 12px 8px}" \
	".utoolbar{display:grid;grid-template-columns:auto 200px auto;align-items:center;justify-content:center;gap:12px}" \
	".utoolbar.center{display:flex;justify-content:center}" \
	".tgrp{display:inline-flex;align-items:center;gap:5px;min-width:0}" \
	".usrch{height:30px;display:flex;align-items:center;gap:6px;padding:0 9px;color:var(--t2);background:var(--s1);border:1px solid var(--bd2);border-radius:999px;box-shadow:var(--shadow);transition:border-color .15s,box-shadow .15s}" \
	".usrch:focus-within{border-color:var(--p);box-shadow:var(--focus)}" \
	".usrch svg{width:14px;height:14px;flex:none}" \
	".usrch input{flex:1;min-width:0;color:var(--t0);background:transparent;border:0;outline:0;font-size:13px}" \
	".usrch input::placeholder{color:var(--t2)}" \
	"#uTable{flex:1;min-height:0;margin:0;overflow:auto;position:relative}" \
	"#uTable table{min-width:1350px}" \
	".ut .c-user{width:140px}" \
	".ut .c-en{width:48px}" \
	".ut .c-caid{width:76px}" \
	".ut .c-conn{width:64px}" \
	".ut .c-ok{width:70px}" \
	".ut .c-nok{width:62px}" \
	".ut .c-proto{width:78px}" \
	".c-proto .badge{padding:1px 6px;font-size:10px}" \
	".ut .c-ip{width:112px}" \
	".ut .c-country{width:44px;text-align:center}" \
	".ut .c-idle{width:64px}" \
	".ut .c-first{width:88px}" \
	".ut .c-last{width:82px}" \
	".ut .c-exp{width:100px}" \
	".ut .c-btn{width:108px}" \
	".ut td.c-user{padding:0 4px}" \
	".ulink{max-width:100%;padding:4px 2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;vertical-align:middle;color:var(--t1);font-family:var(--mono);font-size:13px;font-weight:700;border-radius:4px;transition:background .15s}" \
	".en-box{width:15px;height:15px;vertical-align:middle;cursor:pointer}" \
	".en-box[disabled]{opacity:.5;cursor:progress}" \
	".mono{font-family:var(--mono)}" \
	".cn.on{color:var(--gr);font-weight:700}" \
	".cn.full{color:var(--or);font-weight:700}" \
	".dim{color:var(--t2)}" \
	".flg{margin-right:5px}" \
	".flg:empty{display:none}" \
	".more{margin-left:5px;padding:0 5px;border-radius:9px;background:var(--s3);color:var(--t1);font-size:10px;font-weight:700}" \
	".ba{display:flex;justify-content:center;gap:4px}" \
	".act-b{width:27px;height:27px;padding:0;display:inline-flex;align-items:center;justify-content:center;background:var(--s1);border:1px solid var(--bd2);border-radius:7px;transition:background .15s,transform .1s,box-shadow .15s}" \
	".act-b svg{width:14px;height:14px}" \
	".act-b.ed{color:var(--p);background:var(--ps);border-color:var(--pg)}" \
	".act-b.rs{color:var(--or);background:var(--ors);border-color:rgba(249,115,22,.3)}" \
	".act-b.dl{color:var(--re);background:var(--res);border-color:rgba(239,68,68,.3)}" \
	".urow[data-state=disabled] td:not(.c-en):not(.c-btn):not(.c-user){opacity:.5}" \
	".urow[data-state=disabled] .ulink{opacity:.6}" \
	".uempty{padding:44px 16px;text-align:center;color:var(--t1)}" \
	".uempty svg{width:34px;height:34px;margin-bottom:8px;color:var(--t2)}" \
	".uempty b{display:block;margin-bottom:4px;font-size:15px;color:var(--t0)}" \
	".uempty .tool{margin-top:14px}" \
	".ufoot{display:grid;grid-template-columns:1fr auto 1fr;align-items:center;gap:8px;min-height:31px}" \
	".ucount{display:flex;align-items:center;flex-wrap:wrap;gap:2px 14px;min-width:0;font-size:12px;color:var(--t2)}" \
	".ucount b{margin-right:4px;font-family:var(--mono);font-weight:700;color:var(--t0)}" \
	".pager-wrap{display:flex;align-items:center;justify-content:center;gap:8px;min-width:0}" \
	".pager{display:flex;align-items:center;gap:4px}" \
	".page{min-width:30px;height:30px;padding:0 7px;font-size:12px;color:var(--t0);background:var(--s1);border:1px solid var(--bd2);border-radius:var(--rsm)}" \
	".page:disabled{opacity:.55;cursor:default}" \
	".page.current{color:#fff;background:var(--p);border-color:var(--p)}" \
	".pstat{font-size:12px;color:var(--t2);white-space:nowrap}" \
	".per-page{justify-self:end;display:flex;align-items:center;gap:7px;font-size:12px;color:var(--t2)}" \
	".per-page select{height:30px;padding:0 8px;color:var(--t0);background:var(--s1);border:1px solid var(--bd2);border-radius:var(--rsm)}" \
	"@media(max-width:900px){" \
	"  body.pg-users{height:auto;overflow:auto}" \
	"  body.pg-users #mn{height:auto}" \
	"  body.pg-users #ct{height:auto;min-height:calc(100vh - var(--tbh))}" \
	"  body.pg-users footer:not(.ufoot){display:flex!important}" \
	"  #uTable{flex:none;max-height:none}" \
	"  .utoolbar{grid-template-columns:1fr;justify-items:stretch}" \
	"  .tgrp{flex-wrap:wrap}" \
	"  .tgrp .tool{flex:1 1 auto}" \
	"  .ut td.c-en,.ut th.c-en{position:sticky;left:0;z-index:3;background:var(--s1)}" \
	"  .ut td.c-user,.ut th.c-user{position:sticky;left:48px;z-index:2;background:var(--s1)}" \
	"  .ut th.c-en,.ut th.c-user{background:var(--s2)}" \
	"  .ufoot{grid-template-columns:1fr;justify-items:center;gap:6px}" \
	"  .per-page{justify-self:center}" \
	"}" \
	".reader-modal{width:780px}" \
	".reader-modal .mb{padding:16px 18px 4px;overflow:visible}" \
	".reader-topgrid{display:grid;grid-template-columns:minmax(0,1.6fr) minmax(160px,.8fr) auto;gap:12px;align-items:end}" \
	".reader-enable,.reader-check{display:flex;align-items:center;justify-content:space-between;gap:10px;min-height:38px;margin-bottom:10px;padding:8px 11px;border:1px solid var(--bd);border-radius:var(--rsm);background:var(--s2);font-size:12px;font-weight:600;color:var(--t1)}" \
	".reader-enable input,.reader-check input{width:17px;height:17px;margin:0;accent-color:var(--p)}" \
	".reader-cccam-grid{display:grid;grid-template-columns:2fr 1.2fr 1.2fr .8fr;gap:10px}" \
	".reader-pcsc-grid{display:grid;grid-template-columns:2fr .9fr .9fr .9fr;gap:10px}" \
	".reader-routing-grid{display:grid;grid-template-columns:1fr 1.5fr .8fr 1fr;gap:10px}" \
	".reader-modal .ea{height:108px!important;min-height:108px!important}" \
	".reader-modal .shd{margin-top:2px;margin-bottom:9px}" \
	".reader-modal .fg{margin-bottom:10px}" \
	".reader-modal .mf{padding:12px 18px}" \
	".fhint{margin:0 0 10px;font-size:11.5px;line-height:1.45;color:var(--t2)}" \
	".fg .fhint{margin:6px 0 0}" \
	".user-modal{width:620px}" \
	".user-modal .mb{padding:16px 20px 4px;overflow:auto}" \
	".user-modal .fg{margin-bottom:11px}" \
	".user-g3{display:grid;grid-template-columns:1.2fr 1fr;gap:10px}" \
	".user-bottom-grid{display:grid;grid-template-columns:1.55fr .9fr;gap:12px;align-items:end}" \
	".user-enable{display:flex;align-items:center;justify-content:space-between;gap:12px;min-height:38px;margin-bottom:11px;padding:8px 11px;border:1px solid var(--bd);border-radius:var(--rsm);background:var(--s2);font-size:12px;font-weight:600;color:var(--t1)}" \
	".user-modal .mf{padding:12px 20px}" \
	"@media(max-width:760px){.reader-modal{width:100%}.reader-topgrid,.reader-cccam-grid,.reader-pcsc-grid,.reader-routing-grid,.user-g3,.user-bottom-grid{grid-template-columns:1fr 1fr}.reader-enable{grid-column:1/-1}.reader-modal .mb,.user-modal .mb{overflow:auto}.user-modal{width:100%}}" \
	"@media (hover:hover) and (pointer:fine){" \
	".sc.bl:hover{border-color:var(--p)}" \
	".sc.gr:hover{border-color:var(--gr)}" \
	".sc.vi:hover{border-color:var(--vi)}" \
	".sc.cy:hover{border-color:var(--cy)}" \
	".sc.re:hover{border-color:var(--re)}" \
	".sc.or:hover{border-color:var(--or)}" \
	".sc:hover::before{opacity:1}" \
	".card:hover{border-color:var(--pg)}" \
	"tbody tr:hover{background:var(--s3)}" \
	".cm tbody tr:hover{background:var(--s3)}" \
	".table-sort:hover{color:var(--p)}" \
	"}" \
	".file-page{max-width:1400px;margin:0 auto}" \
						".file-busy{padding:3px 8px;border-radius:999px;background:var(--ps);color:var(--p);border:1px solid var(--pg)}" \
	".file-tabs{display:flex;align-items:flex-end;gap:3px;overflow-x:auto;scrollbar-width:none;padding:0 2px}" \
	".file-tabs::-webkit-scrollbar{display:none}" \
	".file-tabs .ctab{flex:none}" \
	".file-card{background:var(--s1);border:1px solid var(--bd);border-radius:0 var(--r) var(--r) var(--r);overflow:hidden}" \
	".file-state{margin-left:auto;font-size:11px;font-family:var(--mono);color:var(--t2)}" \
	".file-state.ok{color:var(--gr)}" \
	".file-state.dirty{color:var(--or2)}" \
	".file-state.error{color:var(--re)}" \
	".file-state.loading{color:var(--p)}" \
	".file-actions{justify-content:space-between;text-align:left;gap:12px}" \
	".file-info{display:flex;align-items:center;gap:7px;min-width:0;font:11px var(--mono);color:var(--t2);overflow:hidden}" \
	".file-info span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}" \
	".file-dot{color:var(--bd2)!important;overflow:visible!important}" \
	".file-buttons{display:flex;align-items:center;gap:7px;flex:none}" \
	"@media(max-width:720px){.file-actions{align-items:flex-start;flex-direction:column}.file-buttons{width:100%;justify-content:flex-end}.file-buttons .tool{flex:0 0 auto}.file-info{max-width:100%;width:100%}}" \
	"@media (hover:hover) and (pointer:fine){" \
	"::-webkit-scrollbar-thumb:hover{background:var(--bd)}" \
	".tnav a:hover{background:var(--s3);color:var(--t0)}" \
	".tnav a:hover .ni{opacity:1}" \
	".pc button:hover{color:var(--t0);background:var(--s3)}" \
	".bp:hover{background:var(--p2);box-shadow:0 0 0 3px var(--pg)}" \
	".bg:hover{background:var(--s3);color:var(--t0)}" \
	".bd_:hover{background:rgba(239,68,68,.2)}" \
	".bwarn:hover{background:rgba(249,115,22,.2)}" \
	".kb:hover{opacity:1;background:var(--res)}" \
	".pw-btn:hover{opacity:1!important;filter:brightness(1.2)}" \
	".u-link:hover{color:var(--p);text-decoration:underline}" \
	".ctab:not(.act):hover{color:var(--t0);background:var(--s3)}" \
	"#lp span:hover{background:rgba(255,255,255,0.04)}" \
	".dt:hover{border-color:var(--pg);color:var(--p)}" \
	".tip:hover .tipt,.tip:focus-within .tipt{display:block}" \
	".sbar-item:hover{background:var(--s2)}" \
	".ac-swatch:hover{transform:scale(1.1)}" \
	".thb:hover{background:var(--s3);color:var(--t0)}" \
	".acp .thb:hover{border-color:var(--p)}" \
	".ib:hover{background:var(--s3);color:var(--t0);border-color:var(--bd)}" \
	".ib.dng:hover{background:var(--res);color:var(--re);border-color:rgba(239,68,68,.3)}" \
	".chip2:hover{border-color:var(--p);color:var(--p);background:var(--ps)}" \
	".btn.bdz:hover{filter:brightness(1.1);box-shadow:0 0 0 3px var(--res)}" \
	".tool:hover{background:var(--s3)}" \
	".tool.pri:hover{background:var(--p2)}" \
	".tool.danger:hover{background:var(--res)}" \
	".ulink:hover{background:var(--ps)}" \
	".act-b:hover{transform:translateY(-1px);box-shadow:var(--shadow)}" \
	".page:hover{background:var(--s3)}" \
	"}" \
	"body.pg-status .sc{min-height:0}" \
	"body.pg-power .card{padding:16px 14px!important}" \
	"body.pg-power .card .dico{margin-bottom:8px!important}" \
	"body.pg-tvcas .tv-card{padding:12px 14px!important;margin-bottom:10px!important}" \
	"body.pg-tvcas .tv-tabs{margin-bottom:10px!important}" \
	""

#endif                        
