#ifndef TCMG_WEBIF_CSS_H_
#define TCMG_WEBIF_CSS_H_

#define TCMG_CSS \
	"*{box-sizing:border-box;margin:0;padding:0}" \
	"html{scroll-behavior:smooth}" \
	"a{text-decoration:none;color:inherit}" \
	"button{cursor:pointer;font-family:inherit;border:none;background:none}" \
	"ul,ol{list-style:none}" \
	":root{--bg:#08090c;--s4:#33353c;--bd:#383a41;--bd2:#494b53;--p:#38bdf8;--p2:#0ea5e9;--ps:rgba(56,189,248,.12);--pg:rgba(56,189,248,.35);--vi:#e8a23c;--vis:rgba(232,162,60,.13);--vig:rgba(232,162,60,.26);--cy:#c96f47;--cys:rgba(201,111,71,.13);--cyg:rgba(201,111,71,.26);--gr:#4bc97a;--grs:rgba(75,201,122,.13);--grg:rgba(75,201,122,.26);--re:#e5484d;--res:rgba(229,72,77,.13);--or:#f0973f;--or2:#f5ab5c;--ors:rgba(240,151,63,.13);--am:#e8c547;--ams:rgba(232,197,71,.13);--amg:rgba(232,197,71,.30);--pk:#e878a9;--pks:rgba(232,120,169,.13);--pkg:rgba(232,120,169,.28);--te:#35c7b5;--tes:rgba(53,199,181,.13);--teg:rgba(53,199,181,.28);--ind:#8b7cf6;--inds:rgba(139,124,246,.13);--indg:rgba(139,124,246,.28);--sl:#9aa2b1;--sls:rgba(154,162,177,.13);--slg:rgba(154,162,177,.28);--t0:#f2f3f5;--t1:#c1c4ca;--t2:#8b8e96;--sans:system-ui,-apple-system,'Segoe UI',Arial,sans-serif;--mono:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono',monospace;--r:12px;--rsm:8px;--rxs:5px;--tbh:56px;--ease:cubic-bezier(.4,0,.2,1);color-scheme:dark;--term:#08090a;--navbg:rgba(13,14,16,.97);--pop:#202226;--shadow:0 1px 2px rgba(0,0,0,.4),0 2px 6px rgba(0,0,0,.3);--shadow-lg:0 20px 46px rgba(0,0,0,.55);--ovl:rgba(4,5,6,.74);--focus:0 0 0 3px var(--pg);--grad-brand:linear-gradient(135deg,#38bdf8 0%,#0ea5e9 45%,#075985 100%);--grad-p:linear-gradient(135deg,#38bdf8,#0ea5e9);--grad-warm:linear-gradient(135deg,#f0973f,#c96f47);--grad-card:linear-gradient(165deg,rgba(255,255,255,.05),rgba(255,255,255,0) 40%);--noise-op:.05}" \
	"body{background:radial-gradient(circle,rgba(56,189,248,.09) 1px,transparent 1.4px) 0 0/22px 22px,var(--bg);color:var(--t0);font-family:var(--sans);font-size:14px;line-height:1.5;-webkit-font-smoothing:antialiased;min-height:100vh}" \
	"#tb{position:fixed;top:0;left:0;right:0;height:var(--tbh);background:var(--navbg);backdrop-filter:blur(10px);border-bottom:1px solid var(--bd);display:flex;align-items:center;padding:0 18px;z-index:1060;gap:10px;box-shadow:0 1px 0 rgba(255,255,255,.03) inset,0 12px 24px -18px rgba(0,0,0,.6)}" \
	"#tb::after{content:'';position:absolute;left:0;right:0;bottom:-1px;height:1px;background:linear-gradient(90deg,transparent,var(--p) 20%,var(--cy) 50%,var(--vi) 80%,transparent);opacity:.55}" \
	".lo{display:flex;align-items:center;gap:9px;margin-right:10px;flex-shrink:0}" \
	".li{width:32px;height:32px;background:var(--grad-brand);border-radius:9px;display:grid;place-items:center;flex-shrink:0;box-shadow:0 3px 10px -2px rgba(56,189,248,.55),inset 0 1px 0 rgba(255,255,255,.18);position:relative}" \
	".li svg *{stroke:#eafffb!important}" \
	".li svg{width:16px;height:16px}" \
	".lt{font-weight:800;font-size:14.5px;letter-spacing:.07em;color:var(--t0);background:linear-gradient(90deg,var(--t0),var(--t0) 60%,var(--p));-webkit-background-clip:text;background-clip:text}" \
	".lv{font-family:var(--mono);font-size:10px;color:var(--p);background:var(--ps);border:1px solid var(--pg);padding:1px 6px;border-radius:var(--rxs)}" \
	".tnav{display:flex;align-items:center;justify-content:center;gap:2px;flex:1;flex-wrap:wrap}" \
	".tnav a{display:inline-flex;align-items:center;gap:5px;padding:6px 10px;border-radius:var(--rsm);color:var(--t1);font-size:12.5px;font-weight:500;border:1px solid transparent;transition:background .15s,color .15s,border-color .15s;white-space:nowrap}" \
	".tnav a.act{background:linear-gradient(180deg,var(--ps),rgba(56,189,248,.03));color:var(--p);border-color:var(--pg);box-shadow:0 2px 10px -4px rgba(56,189,248,.5),inset 0 1px 0 rgba(255,255,255,.06)}" \
	".tnav a{position:relative}" \
	".tnav a:hover:not(.act){color:var(--t0)}" \
	".tnav .ni{width:15px;height:15px;flex-shrink:0;opacity:.76;fill:none;stroke:currentColor;stroke-width:1.9;stroke-linecap:round;stroke-linejoin:round}" \
	".tnav a.act .ni{opacity:1}" \
	".tnav .sep{width:1px;height:20px;background:var(--bd2);margin:0 4px;flex-shrink:0}" \
	".tbr{display:flex;align-items:center;gap:8px;margin-left:auto;flex-shrink:0}" \
	".spill{display:flex;align-items:center;gap:6px;background:var(--grs);border:1px solid rgba(34,197,94,.2);border-radius:20px;padding:3px 10px;font-size:11px;color:var(--gr);font-weight:500;white-space:nowrap}" \
	".chip{font-size:11px;font-family:var(--mono);border:1px solid var(--bd);border-radius:var(--rxs);padding:2px 8px;color:var(--t1)}" \
	".pc{display:flex;align-items:center;gap:3px;border:1px solid var(--bd);border-radius:var(--rsm);padding:3px 7px;font-size:10px;font-family:var(--mono);color:var(--t2)}" \
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
	".btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;padding:8px 16px;border-radius:10px;font-family:var(--sans);font-size:13px;font-weight:600;letter-spacing:.02em;transition:transform .15s var(--ease),box-shadow .15s,filter .15s,background .15s;cursor:pointer;border:1px solid transparent}" \
	".btn:active{transform:translateY(1px) scale(.98)}" \
	".btn svg{width:14px;height:14px;flex-shrink:0}" \
	".bp{background:rgba(56,189,248,.08);color:var(--p);border:1.5px solid var(--p);box-shadow:0 0 0 rgba(0,0,0,0)}" \
	".bp:hover{background:rgba(56,189,248,.16);box-shadow:0 0 14px -2px rgba(56,189,248,.45)}" \
	".bp:active{background:rgba(56,189,248,.22)}" \
	".bg{color:var(--t1);border-color:var(--bd)}" \
	".bg:hover{color:var(--t0);border-color:var(--bd2)}" \
	".bd_{background:var(--res);color:var(--re);border-color:rgba(239,68,68,.3)}" \
	".bwarn{background:var(--ors);color:var(--or);border-color:rgba(249,115,22,.3)}" \
	".btn.sm{padding:5px 11px;font-size:12px}" \
	".cg{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:10px;margin-bottom:14px;align-items:stretch}" \
	".sc{border:1px solid var(--bd);border-radius:11px;padding:10px 11px;display:flex;align-items:center;gap:10px;position:relative;overflow:hidden;transition:border-color .18s,transform .18s var(--ease),box-shadow .18s;animation:fu .28s var(--ease) both;height:100%;min-height:72px;box-shadow:var(--shadow)}" \
	".sc:hover{transform:translateY(-2px);box-shadow:0 8px 22px -16px rgba(0,0,0,.8)}" \
		".sc.bl{border-color:rgba(56,189,248,.32);}" \
	".sc.gr{border-color:rgba(75,201,122,.32);}" \
	".sc.vi{border-color:rgba(232,162,60,.32);}" \
	".sc.cy{border-color:rgba(201,111,71,.32);}" \
	".sc.re{border-color:rgba(229,72,77,.32);}" \
	".sc.or{border-color:rgba(240,151,63,.32);}" \
	".si_{width:34px;height:34px;border-radius:9px;display:grid;place-items:center;flex-shrink:0;box-shadow:inset 0 1px 0 rgba(255,255,255,.08),0 4px 10px -4px rgba(0,0,0,.5)}" \
	".si_ svg{width:18px;height:18px}" \
	".bl .si_{background:var(--ps);color:var(--p)}" \
	".gr .si_{background:var(--grs);color:var(--gr)}" \
	".vi .si_{background:var(--vis);color:var(--vi)}" \
	".cy .si_{background:var(--cys);color:var(--cy)}" \
	".re .si_{background:var(--res);color:var(--re)}" \
	".or .si_{background:var(--ors);color:var(--or)}" \
	".sb_{display:flex;flex-direction:column;gap:3px;min-width:0;flex:1}" \
	".sl_{font-size:9.5px;font-weight:700;text-transform:uppercase;letter-spacing:.07em;color:var(--t1);line-height:1.2}" \
	".sv{white-space:nowrap;font-size:18px;font-weight:700;color:var(--t0);font-variant-numeric:tabular-nums;transition:color .25s;line-height:1.15}" \
	".sv.mono{font-family:var(--mono);font-size:16px}" \
	".sd{font-size:10px;font-family:var(--sans);color:var(--t1);line-height:1.25}" \
	".bl .sd{color:var(--p)}" \
	".gr .sd{color:var(--gr)}" \
	".vi .sd{color:var(--vi)}" \
	".cy .sd{color:var(--cy)}" \
	".re .sd{color:var(--re)}" \
	".or .sd{color:var(--or)}" \
		".am .si_{background:var(--ams);color:var(--am)}" \
		".am .sd{color:var(--am)}" \
		".am .sv{color:var(--am)}" \
		".pk .si_{background:var(--pks);color:var(--pk)}" \
		".pk .sd{color:var(--pk)}" \
		".pk .sv{color:var(--pk)}" \
		".te .si_{background:var(--tes);color:var(--te)}" \
		".te .sd{color:var(--te)}" \
		".te .sv{color:var(--te)}" \
		".ind .si_{background:var(--inds);color:var(--ind)}" \
		".ind .sd{color:var(--ind)}" \
		".ind .sv{color:var(--ind)}" \
		".sl .si_{background:var(--sls);color:var(--sl)}" \
		".sl .sd{color:var(--sl)}" \
		".sl .sv{color:var(--t0)}" \
		".sc.am{border-color:var(--amg);}" \
		".sc.pk{border-color:var(--pkg);}" \
		".sc.te{border-color:var(--teg);}" \
		".sc.ind{border-color:var(--indg);}" \
		".sc.sl{border-color:var(--slg);}" \
		".am::before{background:linear-gradient(90deg,var(--am),var(--or))}" \
		".pk::before{background:linear-gradient(90deg,var(--pk),var(--re))}" \
		".te::before{background:linear-gradient(90deg,var(--te),var(--cy))}" \
		".ind::before{background:linear-gradient(90deg,var(--ind),var(--p))}" \
		".sl::before{background:linear-gradient(90deg,var(--sl),var(--p))}" \
		".am .sg{background:var(--am)}" \
		".pk .sg{background:var(--pk)}" \
		".te .sg{background:var(--te)}" \
		".ind .sg{background:var(--ind)}" \
		".sl .sg{background:var(--sl)}" \
	".gr .sv{color:var(--gr)}" \
	".re .sv{color:var(--re)}" \
	".or .sv{color:var(--or)}" \
	".vi .sv{color:var(--vi)}" \
	".cy .sv{color:var(--cy)}" \
	".bl .sv{color:var(--p)}" \
	".sc::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;opacity:0;transition:opacity .22s;border-radius:14px 14px 0 0;filter:blur(.2px)}" \
	".sc:hover::before{opacity:1}" \
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
	".sc:nth-child(8){animation-delay:.32s}" \
	".sc:nth-child(9){animation-delay:.36s}" \
	".sc:nth-child(10){animation-delay:.40s}" \
	".card{border:1px solid var(--bd);border-radius:14px;transition:border-color .2s,transform .2s var(--ease),box-shadow .2s;animation:fu .35s var(--ease) .12s both;position:relative;overflow:hidden;box-shadow:var(--shadow)}" \
	".card::before{content:'';position:absolute;inset:0;background:var(--grad-card);pointer-events:none}" \
		".ch{display:flex;align-items:center;justify-content:space-between;padding:13px 18px;border-bottom:1px solid var(--bd);background:rgba(0,0,0,.12);border-radius:14px 14px 0 0}" \
	".ct{font-size:14px;font-weight:700;color:var(--t0);display:flex;align-items:center;gap:8px}" \
	".ct svg{width:16px;height:16px;color:var(--p);flex-shrink:0;filter:drop-shadow(0 0 6px rgba(56,189,248,.6))}" \
	".cb{padding:16px 18px}" \
	".shd{display:flex;align-items:center;justify-content:space-between;margin:2px 0 9px}" \
	".stl{font-size:14px;font-weight:700;color:var(--t0);display:flex;align-items:center;gap:8px}" \
	".stl::before{content:'';display:inline-block;width:4px;height:16px;background:var(--grad-p);border-radius:3px;box-shadow:0 0 8px rgba(56,189,248,.6)}" \
	".tw{border:1px solid var(--bd);border-radius:14px;overflow:auto;margin-bottom:16px;box-shadow:var(--shadow)}" \
	"table{width:100%;border-collapse:collapse;font-size:13px}" \
	"thead tr{background:rgba(0,0,0,.14)}" \
	"th{padding:9px 12px;text-align:left;font-size:9.5px;font-weight:700;text-transform:uppercase;letter-spacing:.075em;color:var(--t2);border-bottom:1px solid var(--bd);white-space:nowrap;vertical-align:middle}" \
	"td{padding:8px 12px;border-bottom:1px solid var(--bd);color:var(--t0);vertical-align:middle}" \
	"tbody tr:last-child td{border-bottom:none}" \
	"tbody tr{transition:background .15s,box-shadow .15s}" \
	"tbody tr:hover{background:rgba(56,189,248,.06);box-shadow:inset 3px 0 0 var(--p)}" \
	"tbody tr:nth-child(even){background:rgba(255,255,255,.012)}" \
		"tbody tr.nw{animation:rf .5s ease}" \
	"@keyframes rf{" \
	"  from{background:rgba(56,189,248,.20)}" \
	"  to{background:transparent}" \
	"}" \
	".mono{font-family:var(--mono);font-size:12px}" \
	".bold{font-weight:600}" \
	".badge{display:inline-flex;align-items:center;gap:5px;padding:3px 10px;border-radius:999px;font-size:11px;font-weight:700;font-family:var(--mono);letter-spacing:.04em}" \
	".badge::before{content:'';width:5px;height:5px;border-radius:50%;background:currentColor;box-shadow:0 0 6px currentColor}" \
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
	".pw-btn.off{color:var(--t2);opacity:.5}" \
	".pw-btn.off svg{transform:scaleX(-1)}" \
	".u-link{cursor:pointer;color:var(--t1)}" \
	".ctab{padding:7px 16px;font-size:12px;font-weight:600;letter-spacing:.05em;border:1px solid var(--bd);border-bottom:none;border-radius:6px 6px 0 0;color:var(--t2);cursor:pointer;transition:all .15s}" \
	".ctab.act{background:var(--ps);color:var(--p);border-color:var(--pg);border-bottom:2px solid var(--p)}" \
	".hbw{border-radius:4px;height:5px;width:80px;overflow:hidden}" \
	".hbf{height:100%;border-radius:4px;background:linear-gradient(90deg,var(--gr),var(--cy));transition:width .4s}" \
	".lc{display:flex;align-items:center;justify-content:center;gap:8px;margin-bottom:12px;flex-wrap:wrap}" \
	".ls{border:1px solid var(--bd2);color:var(--t0);border-radius:var(--rsm);padding:5px 10px;font-family:var(--mono);font-size:12px;width:220px;outline:none}" \
	".ls:focus{border-color:var(--pg)}" \
	"select.lsel{color:var(--t1);border:1px solid var(--bd2);border-radius:var(--rsm);padding:5px 8px;font-size:12px}" \
	"#lw{background:var(--term);border:1px solid var(--bd);border-radius:10px;height:calc(100vh - 245px);min-height:340px;overflow:auto;padding:14px 4px 14px 14px;position:relative;scroll-behavior:smooth}" \
	"#lw::before{content:'LIVE';position:absolute;top:10px;right:12px;font-size:9px;font-family:var(--mono);font-weight:700;letter-spacing:.12em;color:var(--gr);opacity:.4;pointer-events:none}" \
	"#lp{margin:0;font-family:var(--mono);font-size:12.5px;line-height:1.85;color:var(--t1)}" \
	"#lp span{display:block;white-space:pre;border-radius:2px;padding:0 4px}" \
	".lok{color:#4ade80;font-weight:700}" \
	".lwarn{color:var(--or2);font-weight:700}" \
	".lerr{color:var(--re);font-weight:700}" \
	".lnet{color:#c96f47;font-weight:700}" \
	".lwebif{color:#1fb8a3;font-weight:700}" \
	".lban{color:var(--or2);font-weight:700}" \
	".lt2{color:var(--t2)}" \
	".db{border:1px solid var(--bd);border-radius:var(--rsm);padding:8px 12px;margin-bottom:12px;display:flex;flex-wrap:wrap;align-items:center;justify-content:center;gap:5px}" \
	".dt{display:inline-flex;align-items:center;padding:3px 10px;border-radius:var(--rxs);font-size:11px;font-family:var(--mono);font-weight:500;cursor:pointer;border:1px solid var(--bd);color:var(--t2);transition:all .15s;user-select:none}" \
	".dt.on{background:var(--ps);border-color:var(--pg);color:var(--p)}" \
	".dm{font-size:11px;color:var(--t2);font-family:var(--mono)}" \
	".et{display:flex;align-items:center;justify-content:center;gap:10px;padding:10px 16px;border-bottom:1px solid var(--bd);border-radius:var(--r) var(--r) 0 0}" \
	".ef{font-family:var(--mono);font-size:12px;color:var(--p);display:flex;align-items:center;gap:8px}" \
	".ef svg{width:14px;height:14px;opacity:.6}" \
	".ew{background:var(--ors);border-top:1px solid rgba(249,115,22,.25);padding:7px 16px;font-family:var(--mono);font-size:12px;color:var(--or2);display:flex;align-items:center;gap:6px}" \
	".ew svg{width:13px;height:13px;flex-shrink:0}" \
	".ea{font-family:var(--mono);font-size:13px;line-height:1.9;color:#a5d6a7;background:#040810;border:none;outline:none;width:100%;min-height:390px;padding:14px 16px;resize:vertical}" \
	".ef2{display:flex;align-items:center;justify-content:center;gap:10px;padding:8px 16px;border-top:1px solid var(--bd);border-radius:0 0 var(--r) var(--r)}" \
	".es{font-family:var(--mono);font-size:11px;color:var(--t1);text-align:center}" \
	".es .ok{color:var(--gr)}" \
	".lb{min-height:100vh;display:flex;align-items:center;justify-content:center;padding:24px}" \
	".lwrap{display:flex;width:430px;max-width:100%;border-radius:18px;overflow:hidden;box-shadow:var(--shadow-lg);border:1px solid var(--bd);animation:fu .4s var(--ease) both}" \
	".lformpanel{width:100%;padding:34px 32px;display:flex;flex-direction:column;justify-content:center;min-width:0;box-sizing:border-box}" \
	".llt2{font-size:19px;font-weight:800;color:var(--t0)}" \
	".lls{font-size:12.5px;color:var(--t2);margin:5px 0 22px}" \
	"@media(orientation:portrait){.lwrap{width:380px}}" \
	".fld{display:block;font-size:11px;font-weight:600;color:var(--t1);letter-spacing:.05em;margin-bottom:5px}" \
	".fi{width:100%;padding:9px 12px;border:1px solid var(--bd);color:var(--t0);border-radius:var(--rsm);font-size:13px;font-family:var(--sans);transition:border-color .18s;outline:none}" \
	".fi:focus{border-color:var(--pg)}" \
	".fg{margin-bottom:7px}\n" \
	".le{display:flex;align-items:center;gap:8px;background:var(--res);border:1px solid rgba(239,68,68,.28);border-radius:var(--rsm);padding:9px 12px;color:var(--re);font-size:12px;margin-bottom:16px}" \
	".le svg{width:14px;height:14px;flex-shrink:0}" \
	".dlg{border:1px solid var(--bd);border-radius:12px;padding:22px;max-width:420px}" \
	".dico{width:44px;height:44px;border-radius:10px;display:grid;place-items:center;margin:0 auto 10px;flex-shrink:0}" \
	".dico svg{width:22px;height:22px}" \
	".dico.danger{background:var(--res);color:var(--re)}" \
	".dico.info{background:var(--ps);color:var(--p)}" \
	".dico.warn{background:var(--ors);color:var(--or)}" \
	".dlg h2{font-size:14px;font-weight:700;color:var(--t0);text-align:center;margin-bottom:5px}" \
	".dlg p{color:var(--t1);font-size:12px;text-align:center;margin-bottom:12px;line-height:1.5}" \
	".da{display:flex;gap:10px;justify-content:center;flex-wrap:wrap}" \
	".done-card{border:1px solid var(--bd);border-radius:12px;padding:22px;max-width:380px;text-align:center}" \
	".pg-center{display:flex;justify-content:center;padding-top:12px}" \
	".ib2{border:1px solid var(--bd);border-radius:var(--rsm);padding:10px 14px;margin-bottom:12px;font-size:12px;color:var(--t1)}" \
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
	".tsrch{border:1px solid var(--bd2);color:var(--t0);border-radius:var(--rsm);padding:6px 12px;font-family:var(--mono);font-size:12px;width:160px;outline:none;transition:border-color .18s}" \
	".tsrch:focus{border-color:var(--pg)}" \
	".tsrch::placeholder{color:var(--t2)}" \
	"tfoot tr{}" \
	"tfoot td{padding:9px 14px;font-size:11px;font-weight:700;color:var(--t2);border-top:2px solid var(--bd);letter-spacing:.04em}" \
	"tfoot .tfs{font-size:13px;color:var(--t0)}" \
	"tfoot .tfl{font-size:10px;color:var(--t2);text-transform:uppercase;letter-spacing:.1em}" \
	".sbar{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));border:1px solid var(--bd);border-radius:14px;overflow:hidden;margin-bottom:16px;max-width:660px;margin-left:auto;margin-right:auto;box-shadow:var(--shadow)}" \
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
	"@media(orientation:portrait){" \
	"  body.pg-users .ut .urow[data-vis=online] .c-user .ulink{color:var(--gr)!important}" \
	"  body.pg-users .ut .urow[data-vis=stale] .c-user .ulink{color:var(--or)!important}" \
	"  body.pg-users .ut .urow[data-vis=expired] .c-user .ulink{color:var(--re)!important}" \
	"  body.pg-users .ut .urow[data-vis=disabled] .c-user .ulink{color:var(--t2)!important}" \
	"  body.pg-readers .rt .rrow .c-rlabel .rlink.r-active{color:var(--gr)!important;text-shadow:0 0 8px rgba(75,201,122,.18)}" \
	"  body.pg-readers .rt .rrow .c-rlabel .rlink.r-idle{color:var(--or)!important;text-shadow:0 0 8px rgba(240,151,63,.12)}" \
	"  body.pg-readers .rt .rrow .c-rlabel .rlink.r-disabled{color:var(--t2)!important;text-shadow:none}" \
	"  .row2,.row2.r7030,.row2.r6040{grid-template-columns:1fr}" \
	"}" \
	".ecm-bk{display:flex;gap:2px;height:6px;border-radius:4px;overflow:hidden;margin:6px 0 2px;width:100%;}" \
	".ecm-bk span{height:100%;transition:width .4s}" \
	".ecm-ok{background:var(--gr)}" \
	".ecm-nok{background:var(--re)}" \
	".msr{display:flex;flex-wrap:wrap;gap:8px 16px;font-size:12px;font-family:var(--mono)}" \
	".msr-kv{display:flex;gap:5px;align-items:center}" \
	".msr-k{color:var(--t2);font-size:10px;text-transform:uppercase;letter-spacing:.08em;font-family:var(--sans);font-weight:600}" \
	".msr-v{color:var(--t0);font-weight:600}" \
	"#mnuBtn{display:none;width:34px;height:34px;border-radius:var(--rsm);border:1px solid var(--bd);place-items:center;color:var(--t1);cursor:pointer;flex-shrink:0}" \
	"#mnuBtn svg{width:16px;height:16px}" \
	"@media(orientation:landscape){" \
	"  body.pg-users .ut .urow[data-vis=expired]{background:linear-gradient(90deg,rgba(239,68,68,.12),rgba(239,68,68,.03) 60%);box-shadow:inset 3px 0 0 var(--re)}" \
	"  body.pg-users .ut .urow[data-vis=disabled]{background:linear-gradient(90deg,rgba(127,140,150,.10),rgba(127,140,150,.025) 60%);box-shadow:inset 3px 0 0 var(--bd2)}" \
	"  body.pg-users .ut .urow[data-vis=expired]:hover{background:linear-gradient(90deg,rgba(239,68,68,.12),rgba(239,68,68,.03) 60%);box-shadow:inset 3px 0 0 var(--re)}" \
	"  body.pg-users .ut .urow[data-vis=disabled]:hover{background:linear-gradient(90deg,rgba(127,140,150,.10),rgba(127,140,150,.025) 60%);box-shadow:inset 3px 0 0 var(--bd2)}" \
	"}" \
	"@media(orientation:portrait){" \
	"  #mnuBtn{display:grid}" \
	"  .tnav{display:none;position:fixed;top:var(--tbh);left:0;right:0;bottom:0; background:var(--navbg);z-index:1020;flex-direction:column;padding:14px; overflow-y:auto;gap:4px}" \
	"  .tnav.open{display:flex}" \
	"  .tnav .sep{display:none}" \
	"  .tnav a{font-size:14px;padding:10px 14px}" \
	"  .tbr .chip,.tbr .pc{display:none}" \
	"}" \
	".ll-meta{display:flex;align-items:center;gap:6px;flex-wrap:wrap;font-size:11px;font-family:var(--mono);color:var(--t2)}" \
	".ll-dot{width:7px;height:7px;border-radius:50%;background:var(--gr);animation:pa 2s ease-in-out infinite;flex-shrink:0}" \
	".ll-state{color:var(--gr);font-weight:700}" \
	".ll-state.warn{color:var(--am)}" \
	".ll-mask{color:var(--p)}" \
	".ll-meta-sep{color:var(--t3)}" \
	".ll-meta-word{color:var(--t2)}" \
	".ll-row{display:flex;flex-wrap:wrap;align-items:center;gap:6px;justify-content:center}" \
	".ll-label{font-size:10px;font-weight:700;text-transform:uppercase;letter-spacing:.1em;color:var(--t2);white-space:nowrap;margin-right:2px}" \
	".ll-sep{width:1px;height:18px;background:var(--bd2);flex-shrink:0}" \
	".ll-chk{display:flex;align-items:center;font-size:12px;color:var(--t1);white-space:nowrap;gap:5px;cursor:pointer;padding:6px 8px;border:1px solid var(--bd);border-radius:8px}" \
	".ll-toolbar{padding:4px 0}" \
	".ll-card{margin-bottom:12px;overflow:hidden}" \
	".ll-statusbar{display:flex;align-items:center;justify-content:center;min-height:30px;padding:6px 12px;border-bottom:1px solid var(--bd);text-align:center}" \
	".ll-settings{border-top:1px solid var(--bd);}" \
	".ll-settings summary{list-style:none;display:flex;align-items:center;justify-content:center;gap:7px;min-height:34px;padding:6px 10px;color:var(--t1);font-size:11px;font-weight:700;cursor:pointer;user-select:none}" \
	".ll-settings summary::-webkit-details-marker{display:none}" \
	".ll-settings summary svg{width:14px;height:14px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round;transition:transform .16s ease}" \
	".ll-settings[open] summary svg{transform:rotate(180deg)}" \
	".ll-settings-body{padding:10px 12px 12px;display:flex;flex-direction:column;gap:10px}" \
	".ll-ch{display:flex;flex-direction:column;align-items:center;padding:13px 18px;border-bottom:1px solid var(--bd);border-radius:var(--r) var(--r) 0 0;gap:8px}" \
	".ll-hdr-right{display:flex;align-items:center;gap:5px;flex-wrap:wrap;justify-content:center}" \
	".ll-dbrow{display:flex;align-items:center;gap:5px;flex-wrap:wrap;justify-content:center}" \
	".ll-controls{display:flex;align-items:center;justify-content:center;gap:7px;flex-wrap:wrap}" \
	".ll-controls .ls{flex:1 1 240px;max-width:360px;min-width:180px}" \
	".ll-controls .btn{min-height:30px;padding:6px 10px;font-size:11px}" \
	".ll-controls .btn svg{width:14px;height:14px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round;vertical-align:-2px}" \
	".ll-limit{font-size:10px;color:var(--t3);font-family:var(--mono);padding:0 2px}" \
	".ll-vsep{width:1px;height:18px;background:var(--bd2);flex-shrink:0;margin:0 4px}" \
	".ll-toolbar2{display:flex;flex-wrap:wrap;align-items:center;gap:6px;justify-content:center}" \
	".flagcol{display:inline-block;min-width:20px;font-size:17px;line-height:1;text-align:center;vertical-align:-2px}" \
	".flagcol:empty::after{content:'\\2014';color:var(--t3);font-size:12px}" \
	".uipflag{display:inline-block;min-width:24px;margin-right:7px;font-size:18px;line-height:1;text-align:center;vertical-align:-2px}" \
	".uipflag:empty{display:none}" \
	"[hidden]{display:none!important}" \
	".site-footer{padding:7px 18px;border-top:1px solid var(--bd);display:flex;align-items:center;justify-content:center;gap:6px;flex-wrap:wrap;font-size:10px;color:var(--t2);font-family:var(--mono);min-height:24px;box-sizing:border-box}" \
	".site-footer-version{color:var(--p)}" \
	".site-footer-sep{opacity:.55}" \
	".qtip{width:17px;height:17px;display:inline-grid;place-items:center;margin-left:5px;padding:0;border:1px solid var(--bd2);border-radius:50%;color:var(--t2);font:700 10px/1 var(--sans);vertical-align:1px;cursor:help;flex:0 0 auto}" \
	".qtip-inline{margin-top:0;vertical-align:middle}" \
	".cfg-head .cfg-title{display:flex;align-items:center;gap:4px}" \
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
	".fi[type=number]{width:100px;max-width:100%}" \
	".fi[type=date]{width:132px;max-width:100%}" \
	".fi[type=time]{width:108px;max-width:100%}" \
		".file-card{border-radius:8px!important}" \
	".file-actions{padding:8px 10px!important}" \
	".file-info{font-size:10px!important}" \
	".file-tabs{gap:2px!important}" \
	":where(a,button,input,select,textarea,summary,[tabindex]):focus-visible{outline:2px solid var(--p);outline-offset:2px}" \
	".fi:focus,.tsrch:focus,.ls:focus{border-color:var(--p);box-shadow:var(--focus)}" \
	".card,.sc,.tw,.sbar,.done-card,.dlg{box-shadow:var(--shadow)}" \
	"svg.i{fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}" \
	"@media(prefers-reduced-motion:reduce){" \
	"  *,*::before,*::after{animation-duration:.01ms!important;animation-iteration-count:1!important;transition-duration:.01ms!important;scroll-behavior:auto!important}" \
	"  .pulse{animation:none!important}" \
	"}" \
	\
	                                                        \
	"#p_up{font-size:19px}" \
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
	".mc{width:500px;max-width:calc(100vw - 24px);max-height:calc(100vh - 24px);display:flex;flex-direction:column;border:1px solid var(--bd2);border-radius:12px;box-shadow:var(--shadow-lg);animation:mpop .15s var(--ease)}" \
	".mc.sm{width:400px}" \
	".mh{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:11px 14px;border-bottom:1px solid var(--bd)}" \
	".mh h2{font-size:13.5px;font-weight:700}" \
	".mb{padding:10px 14px 3px;overflow:auto}" \
	".mf{display:flex;justify-content:flex-end;gap:7px;padding:9px 14px;border-top:1px solid var(--bd);border-radius:0 0 12px 12px}" \
	".mc .fld{text-transform:uppercase;font-size:10px;letter-spacing:.07em}" \
	".mc .fi{background:var(--bg);padding:7px 10px;font-size:12.5px}" \
	".fi:disabled{opacity:.6;cursor:not-allowed}" \
	".g2{display:grid;grid-template-columns:1fr 1fr;gap:12px}" \
	".chips{display:flex;flex-wrap:wrap;gap:5px;margin-top:6px}" \
	".chip2{padding:3px 8px;border-radius:999px;border:1px solid var(--bd2);color:var(--t1);font-size:10.5px;font-weight:600;transition:all .15s}" \
	".swrow{display:flex;align-items:center;justify-content:space-between;gap:14px;padding:10px 12px;border:1px solid var(--bd);border-radius:var(--rsm);margin:2px 0 9px}" \
	".swt{font-weight:600;font-size:13px}" \
	".sws{font-size:11.5px;color:var(--t2);margin-top:2px}" \
	".mc .le{margin:0 0 9px}" \
	".cf{display:flex;gap:12px;padding:14px 14px 12px}" \
	".cfi{width:38px;height:38px;border-radius:9px;display:grid;place-items:center;flex-shrink:0}" \
	".cfi svg{width:19px;height:19px}" \
	".cfi.danger{background:var(--res);color:var(--re)}" \
	".cfi.warn{background:var(--ors);color:var(--or)}" \
	".cft{font-size:14px;font-weight:700;margin-bottom:3px}" \
	".cfm{font-size:12.5px;color:var(--t1);line-height:1.5;word-break:break-word}" \
	".btn.bdz{background:var(--re);color:#fff;border-color:var(--re)}" \
	".btn[disabled]{opacity:.6;cursor:progress}" \
	"body.mo-open{overflow:hidden}" \
	".toasts{position:fixed;left:50%;bottom:16px;transform:translateX(-50%);z-index:3000;display:flex;flex-direction:column;gap:6px;align-items:center;pointer-events:none;width:max-content;max-width:min(420px,calc(100vw - 24px))}" \
	".toast{display:flex;align-items:center;gap:8px;min-height:34px;padding:7px 10px;background:rgba(28,30,35,.97);color:var(--t0);border:1px solid var(--bd2);border-left:2px solid var(--p);border-radius:7px;box-shadow:0 10px 28px rgba(0,0,0,.34);font-size:12px;font-weight:600;line-height:1.3;animation:tin .18s var(--ease);backdrop-filter:blur(8px)}" \
	".toast.ok{border-left-color:var(--gr)}" \
	".toast.err{border-left-color:var(--re)}" \
	".toast-ico{width:16px;height:16px;display:grid;place-items:center;flex:0 0 16px;border-radius:50%;background:var(--ps);color:var(--p)}" \
	".toast.ok .toast-ico{background:var(--grs);color:var(--gr)}" \
	".toast.err .toast-ico{background:var(--res);color:var(--re)}" \
	".toast-ico svg{width:12px;height:12px;stroke:currentColor;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}" \
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
	"@media(orientation:portrait){" \
	"  .tbr .chip,.tbr .pc{display:none}" \
	"}" \
	".tnav{min-width:0}" \
	\
	                                                           \
	".tw.cm{}" \
	".cm table{table-layout:fixed;font-size:13px}" \
	".cm.auto table{table-layout:auto}" \
	".cm.auto th,.cm.auto td{padding:0 12px}" \
	".cm td.bold{font-weight:700}" \
	".cm th,.cm td{height:30px;padding:0 7px;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;border-bottom:1px solid var(--bd);border-right:1px solid var(--bd)}" \
	".cm th:last-child,.cm td:last-child{border-right:0}" \
	".cm th{position:sticky;top:0;z-index:1;color:var(--t2);font-size:11px;font-weight:700;letter-spacing:.01em;text-transform:none}" \
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
	".urow{transition:background-color .16s ease,box-shadow .16s ease}" \
	"body.pg-users .ut .urow[hidden],body.pg-readers .rt .rrow[hidden]{display:none!important}" \
		".urow[data-vis='online'],.urow[data-vis='stale'],.urow[data-vis='expired'],.urow[data-vis='disabled']{background:transparent}" \
		".urow[data-active='1'] td{background:rgba(74,222,128,.14);color:var(--t0)}" \
	".urow[data-active='1']{box-shadow:inset 4px 0 0 var(--gr),0 0 0 1px rgba(74,222,128,.10)}" \
		".urow[data-vis='stale']{box-shadow:inset 3px 0 0 0 var(--or)}" \
		".urow[data-vis='expired']{box-shadow:inset 3px 0 0 0 var(--re)}" \
		".urow[data-vis='disabled']{box-shadow:inset 3px 0 0 0 var(--bd2)}" \
		"@media (hover:hover) and (pointer:fine){" \
		".urow[data-active='1']:hover td{background:rgba(74,222,128,.18)}" \
		".urow[data-vis='stale']:hover{background:linear-gradient(90deg,rgba(249,115,22,.12),rgba(249,115,22,.03) 60%)}" \
		".urow[data-vis='expired']:hover{background:linear-gradient(90deg,rgba(239,68,68,.12),rgba(239,68,68,.03) 60%)}" \
		".urow[data-vis='disabled']:hover{background:linear-gradient(90deg,rgba(127,140,150,.10),rgba(127,140,150,.025) 60%)}" \
		"}" \
		".tool{height:30px;padding:0 10px;display:inline-flex;align-items:center;justify-content:center;gap:6px;white-space:nowrap;color:var(--t0);border:1px solid var(--bd2);border-radius:var(--rsm);box-shadow:var(--shadow);transition:background .15s,border-color .15s,color .15s}" \
	".tool svg{width:14px;height:14px}" \
	".tool.act{color:var(--p);background:rgba(56,189,248,.10);border-color:var(--p)}" \
	".tool .n{min-width:18px;padding:0 5px;font-family:var(--mono);font-size:11px;font-weight:700;line-height:16px;border-radius:9px;color:var(--t1)}" \
	".tool.act .n{background:rgba(56,189,248,.18);color:var(--p)}" \
	".tool.pri{color:var(--p);background:rgba(56,189,248,.08);border:1.5px solid var(--p)}" \
	".tool.sm{height:24px;padding:0 8px;font-size:11.5px;gap:4px}" \
	".tool.sm svg{width:12px;height:12px}" \
	".tool.danger{color:var(--tr);border-color:var(--res)}" \
	".tool .g{color:var(--gr)}" \
	".tool .pu{color:var(--vi)}" \
	\
	                                                             \
	"body.pg-readers{height:100vh;overflow:hidden;padding-top:var(--tbh)}" \
	"body.pg-readers footer:not(.ufoot){display:none!important}" \
	"body.pg-readers #mn{height:auto;min-height:calc(100vh - var(--tbh));margin-top:var(--tbh);min-width:0}" \
	"body.pg-readers #ct{display:flex;flex-direction:column;gap:8px;max-width:1400px;min-width:0;height:auto;padding:10px 12px 8px}" \
	".rtoolbar{grid-template-columns:minmax(0,1fr) 250px auto}" \
	".rtoolbar #rStats{overflow-x:auto;scrollbar-width:thin;padding-bottom:1px}" \
	"#rTable{flex:0 1 auto;width:920px;max-width:100%;min-width:0;min-height:0;height:auto;max-height:none;margin:0 auto;overflow:visible;position:relative;padding-right:0;padding-bottom:14px;box-sizing:border-box}" \
	"#rTable table{width:100%;max-width:none;min-width:700px;table-layout:fixed}" \
	".rt th,.rt td{height:27px;padding:0 4px;font-size:12px;vertical-align:middle}" \
	".rt th{font-size:9px}" \
	".rt th[data-k]{padding:0}" \
	".rt .c-rstate{width:34px;text-align:center;white-space:nowrap}" \
	".rt .c-rlabel{width:116px;text-align:center}" \
	".rt .c-rproto{width:64px;text-align:center}" \
	".rt .c-rdev{width:150px;min-width:120px;max-width:180px;text-align:left;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}" \
	".rt .c-rgroup{width:52px;text-align:center}" \
	".rt .c-rcaid{width:68px;text-align:center}" \
	".rt .c-rstat-ok{width:56px;text-align:center}" \
	".rt .c-rstat-nok{width:56px;text-align:center}" \
	".rt .c-btn{width:108px;text-align:center}" \
	".rt .table-sort{height:27px;padding:0 2px;gap:2px;font-size:inherit}" \
	".rt .badge{padding:1px 4px;font-size:8.5px}" \
	".rt .act-b{width:27px;height:27px;border-radius:7px}" \
	".rt .ba{gap:4px}" \
	".rlink{max-width:100%;padding:4px 2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:var(--t1);font-family:var(--mono);font-size:13px;font-weight:700;border-radius:4px;transition:background .15s;text-align:center}" \
	".rdot{display:inline-block;width:8px;height:8px;margin-right:6px;border-radius:50%;vertical-align:middle;background:var(--t2);box-shadow:0 0 0 3px rgba(148,163,184,.08)}" \
	".rdot.on{background:var(--gr);box-shadow:0 0 0 3px rgba(74,222,128,.10)}" \
	".rdot.off{background:var(--t2)}" \
	".rstate{font-size:11px;color:var(--t2)}" \
	".rrow[data-active='1'] td{background:rgba(74,222,128,.14);color:var(--t0)}" \
	".rrow[data-active='1']{box-shadow:inset 4px 0 0 var(--gr),0 0 0 1px rgba(74,222,128,.10)}" \
	".rrow[data-enabled='0'] td:not(.c-rstate):not(.c-btn):not(.c-rlabel){opacity:.5}" \
	".rrow[data-enabled='0'] .rlink{opacity:.65}" \
	".act-b.on{color:var(--gr);background:rgba(74,222,128,.10);border-color:rgba(74,222,128,.28)}" \
	".act-b.off{color:var(--t2);}" \
	"#rEmpty{position:absolute;inset:48px 0 0;display:flex;flex-direction:column;align-items:center;justify-content:center}" \
	"#rEmpty[hidden]{display:none}" \
	"#rEmpty svg{width:34px;height:34px;margin-bottom:8px;color:var(--t2)}" \
	"#rEmpty b{display:block;margin-bottom:4px;font-size:15px;color:var(--t0)}" \
	"#rEmpty span{color:var(--t2);font-size:12px}" \
	"#rEmpty .tool{margin-top:14px}" \
	"@media(orientation:portrait){.rtoolbar{grid-template-columns:1fr;justify-items:stretch}.rtoolbar .usrch{max-width:none}}" \
	"body.pg-users{height:auto;min-height:100vh;overflow-x:clip;overflow-y:visible;padding-top:var(--tbh)}" \
	"body.pg-users footer:not(.ufoot){display:none!important}" \
	"body.pg-users #mn{height:auto;min-height:calc(100vh - var(--tbh));margin-top:var(--tbh);min-width:0}" \
	"body.pg-users #ct{display:flex;flex-direction:column;gap:8px;max-width:none;min-width:0;height:auto;padding:10px 12px 8px}" \
	".utoolbar{display:grid;grid-template-columns:auto 200px auto;align-items:center;justify-content:center;gap:12px}" \
	".utoolbar.center{display:flex;justify-content:center}" \
	".tgrp{display:inline-flex;align-items:center;gap:5px;min-width:0}" \
	".usrch{height:30px;display:flex;align-items:center;gap:6px;padding:0 9px;color:var(--t2);border:1px solid var(--bd2);border-radius:999px;box-shadow:var(--shadow);transition:border-color .15s,box-shadow .15s}" \
	".usrch:focus-within{border-color:var(--p);box-shadow:var(--focus)}" \
	".usrch svg{width:14px;height:14px;flex:none}" \
	".usrch input{flex:1;min-width:0;color:var(--t0);background:transparent;border:0;outline:0;font-size:13px}" \
	".usrch input::placeholder{color:var(--t2)}" \
	"#uTable{flex:0 1 auto;min-width:0;min-height:0;height:auto;max-height:none;margin:0;overflow:visible;position:relative}" \
	"#uTable table{min-width:1118px}" \
	".ut .c-user{width:140px}" \
	".ut .c-en{width:48px}" \
	".ut .c-caid{width:76px}" \
	".ut .c-conn{width:64px}" \
	".ut .c-ok{width:70px}" \
	".ut .c-nok{width:62px}" \
	".ut .c-proto{width:78px}" \
	".c-proto .badge{padding:1px 6px;font-size:10px}" \
	".ut .c-proto .badge::before{display:none}" \
	".ut .c-ip{width:112px}" \
	".ut .c-country{width:44px;text-align:center}" \
	".ut .c-idle{width:64px}" \
	".ut .c-last60{width:70px}" \
	".ut .c-last{width:82px}" \
	".ut .c-exp{width:100px}" \
	".ut .c-btn{width:80px}" \
	".ut td.c-user{padding:0 4px}" \
	".ulink{max-width:100%;padding:4px 2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;vertical-align:middle;color:var(--t1);font-family:var(--mono);font-size:13px;font-weight:700;border-radius:4px;transition:background .15s}" \
	".mono{font-family:var(--mono)}" \
	".cn.on{color:var(--gr);font-weight:700}" \
	".cn.full{color:var(--or);font-weight:700}" \
	".dim{color:var(--t2)}" \
	".flg{margin-right:5px}" \
	".flg:empty{display:none}" \
	".more{margin-left:5px;padding:0 5px;border-radius:9px;color:var(--t1);font-size:10px;font-weight:700}" \
	".ba{display:flex;justify-content:center;gap:4px}" \
	".act-b{width:27px;height:27px;padding:0;display:inline-flex;align-items:center;justify-content:center;border:1px solid var(--bd2);border-radius:7px;transition:background .15s,transform .1s,box-shadow .15s}" \
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
	".page{min-width:30px;height:30px;padding:0 7px;font-size:12px;color:var(--t0);border:1px solid var(--bd2);border-radius:var(--rsm)}" \
	".page:disabled{opacity:.55;cursor:default}" \
	".page.current{color:#fff;background:var(--p);border-color:var(--p)}" \
	"@media(orientation:portrait){" \
	"  body.pg-users{height:auto;overflow:auto}" \
	"  body.pg-users #mn{height:auto}" \
	"  body.pg-users #ct{height:auto;min-height:calc(100vh - var(--tbh))}" \
	"  #uTable{flex:none;max-height:none}" \
	"  .utoolbar{grid-template-columns:1fr;justify-items:stretch}" \
	"  .tgrp{flex-wrap:wrap}" \
	"  .tgrp .tool{flex:1 1 auto}" \
	"  .ut td.c-en,.ut th.c-en{position:sticky;left:0;z-index:3;}" \
	"  .ut td.c-user,.ut th.c-user{position:sticky;left:48px;z-index:2;}" \
	"  .ut th.c-en,.ut th.c-user{}" \
	"}" \
	"body.pg-failban #ct{max-width:980px;margin:0 auto;padding:10px 14px 20px}" \
"body.pg-failban .utoolbar{max-width:980px;margin:0 auto 10px;display:flex;justify-content:center;gap:8px}" \
"body.pg-failban .utoolbar .tool{min-height:28px;padding:5px 9px;font-size:11.5px}" \
"body.pg-failban .sbar{max-width:760px;width:100%;display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px;border:0;overflow:visible;background:none;box-shadow:none;margin:0 auto 12px}" \
"body.pg-failban .sbar-item{min-width:0;min-height:84px;padding:11px 10px;border:1px solid var(--bd);border-radius:13px;box-shadow:var(--shadow);gap:4px;align-items:center;justify-content:center;text-align:center;position:relative;overflow:hidden}" \
"body.pg-failban .sbar-item:last-child{border-right:1px solid var(--bd)}body.pg-failban .sbar-item:nth-child(1){box-shadow:var(--shadow),inset 0 2px 0 rgba(74,222,128,.55)}body.pg-failban .sbar-item:nth-child(2){box-shadow:var(--shadow),inset 0 2px 0 rgba(249,115,22,.55)}body.pg-failban .sbar-item:nth-child(3){box-shadow:var(--shadow),inset 0 2px 0 rgba(167,139,250,.55)}body.pg-failban .sbar-item .sbl{font-size:8px;letter-spacing:.12em;text-align:center}body.pg-failban .sbar-item .sbv{font-size:20px;line-height:1.1;text-align:center}" \
"body.pg-failban .sbl{font-size:8px;letter-spacing:.1em}" \
"body.pg-failban .sbv{font-size:16px;line-height:1.1}" \
"body.pg-failban .sbv.sm{font-size:12px}" \
"body.pg-failban .tw{max-width:900px;margin:0 auto;border-radius:10px;overflow:auto}" \
"body.pg-failban .tw table{width:100%;font-size:11.5px}" \
"body.pg-failban .tw th{padding:7px 9px;font-size:9px}" \
"body.pg-failban .tw td{padding:7px 9px;height:34px}" \
".reader-modal{width:640px;max-width:calc(100vw - 24px)}" \
	".reader-modal .mb{overflow:auto}" \
	".reader-topgrid{display:grid;grid-template-columns:minmax(0,1.35fr) minmax(140px,.8fr) 118px;gap:8px;align-items:end}" \
	".reader-enable,.reader-check{display:flex;align-items:center;justify-content:space-between;gap:8px;min-height:32px;margin-bottom:7px;padding:6px 9px;border:1px solid var(--bd);border-radius:var(--rsm);font-size:11.5px;font-weight:600;color:var(--t1)}" \
	".reader-enable input,.reader-check input{width:17px;height:17px;margin:0;accent-color:var(--p)}" \
	".reader-cccam-grid{display:grid;grid-template-columns:1fr 1fr;gap:7px 9px}" \
	".reader-pcsc-grid{display:grid;grid-template-columns:1fr 1fr;gap:7px 9px}" \
	".reader-routing-grid{display:grid;grid-template-columns:1fr 1fr;gap:7px 9px}" \
	".reader-modal .ea{height:78px;min-height:78px}" \
	".fhint{margin:0 0 9px;font-size:11.5px;line-height:1.4;color:var(--t2)}" \
	".fg .fhint{margin:5px 0 0}" \
	".user-modal{width:520px;max-width:calc(100vw - 24px)}" \
	".user-g3{display:grid;grid-template-columns:1fr 1fr;gap:8px}" \
	".user-bottom-grid{display:grid;grid-template-columns:1fr 150px;gap:8px;align-items:end}" \
	".user-enable{display:flex;align-items:center;justify-content:space-between;gap:10px;min-height:32px;margin-bottom:7px;padding:6px 9px;border:1px solid var(--bd);border-radius:var(--rsm);font-size:11.5px;font-weight:600;color:var(--t1)}" \
	"@media (hover:hover) and (pointer:fine){" \
	".sc.bl:hover{border-color:var(--p)}" \
	".sc.gr:hover{border-color:var(--gr)}" \
	".sc.vi:hover{border-color:var(--vi)}" \
	".sc.cy:hover{border-color:var(--cy)}" \
	".sc.re:hover{border-color:var(--re)}" \
	".sc.or:hover{border-color:var(--or)}" \
	".card:hover{border-color:var(--pg)}" \
	"tbody tr:hover{}" \
	".cm tbody tr:hover{}" \
	".table-sort:hover{color:var(--p)}" \
	"}" \
	".file-page{max-width:1400px;margin:0 auto}" \
						".file-busy{padding:3px 8px;border-radius:999px;background:var(--ps);color:var(--p);border:1px solid var(--pg)}" \
	".file-tabs{display:flex;align-items:center;justify-content:center;flex-wrap:wrap;gap:3px;width:max-content;max-width:100%;overflow-x:auto;scrollbar-width:none;padding:0 2px;margin:0 auto}" \
	".file-tabs::-webkit-scrollbar{display:none}" \
	".file-tabs .ctab{flex:0 0 auto;padding-left:12px;padding-right:12px}" \
	".file-card{border:1px solid var(--bd);border-radius:0 var(--r) var(--r) var(--r);overflow:hidden}" \
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
	"@media (hover:hover) and (pointer:fine){" \
	"::-webkit-scrollbar-thumb:hover{background:var(--bd)}" \
	".tnav a:hover{color:var(--t0)}" \
	".tnav a:hover .ni{opacity:1}" \
	".pc button:hover{color:var(--t0);}" \
	".bp:hover{background:var(--p2);box-shadow:0 0 0 3px var(--pg)}" \
	".bg:hover{color:var(--t0)}" \
	".bd_:hover{background:rgba(239,68,68,.2)}" \
	".bwarn:hover{background:rgba(249,115,22,.2)}" \
	".kb:hover{opacity:1;background:var(--res)}" \
	".pw-btn:hover{opacity:1!important;filter:brightness(1.2)}" \
	".u-link:hover{color:var(--p);text-decoration:underline}" \
	".ctab:not(.act):hover{color:var(--t0);}" \
	"#lp span:hover{background:rgba(255,255,255,0.04)}" \
	".dt:hover{border-color:var(--pg);color:var(--p)}" \
	".tip:hover .tipt,.tip:focus-within .tipt{display:block}" \
	".sbar-item:hover{background:rgba(56,189,248,.06)}" \
	".ib:hover{color:var(--t0);border-color:var(--bd)}" \
	".ib.dng:hover{background:var(--res);color:var(--re);border-color:rgba(239,68,68,.3)}" \
	".chip2:hover{border-color:var(--p);color:var(--p);background:var(--ps)}" \
	".btn.bdz:hover{filter:brightness(1.1);box-shadow:0 0 0 3px var(--res)}" \
	".tool:hover{}" \
	".tool.pri:hover{background:rgba(56,189,248,.18);box-shadow:0 0 14px -2px rgba(56,189,248,.4)}" \
	".tool.danger:hover{background:var(--res)}" \
	".ulink:hover{background:var(--ps)}" \
	".act-b:hover{transform:translateY(-1px);box-shadow:var(--shadow)}" \
	".page:hover{}" \
	"}" \
	"body:not(.pg-status):not(.pg-users):not(.pg-livelog) #ct{max-width:1220px;padding:10px 14px 22px}" \
	"body.pg-status .sc{min-height:0}" \
	"body.pg-livelog #ct{max-width:none}" \
	"body.pg-power .card{padding:16px 14px}" \
	"body.pg-power .card .dico{margin-bottom:8px}" \
	".pw-tile{width:120px;height:120px;border-radius:26px;display:grid;place-items:center;position:relative;transition:transform .18s var(--ease),box-shadow .18s,background .18s}" \
	".pw-tile svg{width:52px;height:52px}" \
	".pw-tile:hover{transform:translateY(-4px) scale(1.03)}" \
	".pw-tile:active{transform:translateY(-1px) scale(.98)}" \
	".pw-tile-restart{background:linear-gradient(160deg,rgba(56,189,248,.16),rgba(56,189,248,.02));color:var(--p);box-shadow:inset 0 1px 0 rgba(255,255,255,.06),0 0 0 1px rgba(56,189,248,.20)}" \
	".pw-tile-restart:hover{box-shadow:inset 0 1px 0 rgba(255,255,255,.08),0 0 0 1px rgba(56,189,248,.4),0 10px 28px -8px rgba(56,189,248,.45)}" \
	".pw-tile-shutdown{background:linear-gradient(160deg,rgba(229,72,77,.16),rgba(229,72,77,.02));color:var(--re);box-shadow:inset 0 1px 0 rgba(255,255,255,.06),0 0 0 1px rgba(229,72,77,.20)}" \
	".pw-tile-shutdown:hover{box-shadow:inset 0 1px 0 rgba(255,255,255,.08),0 0 0 1px rgba(229,72,77,.4),0 10px 28px -8px rgba(229,72,77,.45)}" \
	"body.pg-tvcas .tv-card{padding:12px 14px;margin-bottom:10px}" \
	"body.pg-tvcas .tv-tabs{margin-bottom:10px}" \
	".cfg-tabs{display:flex;align-items:center;gap:4px;margin:0 0 10px;padding:3px;border:1px solid var(--bd);border-radius:10px;overflow-x:auto;scrollbar-width:none;box-shadow:var(--shadow)}" \
	".cfg-tabs::-webkit-scrollbar{display:none}" \
	".cfg-tab{display:inline-flex;align-items:center;justify-content:center;gap:6px;min-height:32px;padding:0 11px;border:1px solid transparent;border-radius:7px;background:transparent;color:var(--t2);font-size:11.5px;font-weight:650;white-space:nowrap;cursor:pointer;transition:background .15s,color .15s,border-color .15s}" \
	".cfg-tab svg{width:14px;height:14px;opacity:.8}" \
	".cfg-tab.act{background:var(--ps);color:var(--p);border-color:var(--pg)}" \
	".cfg-tab.act svg{opacity:1}" \
	".cfg-panel{display:block}.cfg-panel[hidden]{display:none!important}" \
	".cfg-panels{min-width:0}" \
	"body.pg-config .cfg-page{max-width:920px!important;margin:0 auto}" \
	"body.pg-config .cfg-tabs{justify-content:center;flex-wrap:wrap;width:max-content;max-width:100%;margin:0 auto 12px;position:sticky;top:8px;z-index:4}" \
	"body.pg-config .cfg-card{margin-bottom:10px!important;border-radius:10px!important}" \
	"body.pg-config .cfg-head{padding:10px 14px!important;gap:10px!important}" \
	"body.pg-config .cfg-body{padding:12px 14px!important}" \
	"body.pg-config .cfg-grid{display:flex!important;flex-direction:column!important;gap:0!important}" \
	"body.pg-config .cfg-field{display:grid!important;grid-template-columns:190px minmax(0,1fr)!important;align-items:center;gap:16px!important;padding:8px 0!important;border-bottom:1px solid rgba(56,58,65,.65)}" \
	"body.pg-config .cfg-label{margin:0 0 4px!important;font-size:10.5px!important}" \
	"body.pg-config .cfg-input{padding:8px 9px!important;font-size:12px!important}" \
	"body.pg-config .cfg-toggle-row{margin-top:10px!important;gap:7px!important}" \
	"body.pg-config .cfg-toggle{padding:7px 9px!important;font-size:11.5px!important}" \
	"body.pg-config .cfg-save{margin:14px 0 2px!important}" \
	"body.pg-config .cfg-save button{min-width:165px!important}" \
	"body.pg-config #ct{max-width:1220px;padding:10px 14px 20px}" \
	"@media(orientation:portrait){" \
	"  html,body{max-width:100%;overflow-x:clip}" \
	"  body{font-size:13px;line-height:1.45}" \
	"  :root{--tbh:50px}" \
	"  #tb{height:50px;padding:0 10px;gap:7px}" \
	"  #tb::after{opacity:.7}" \
	"  .lo{margin:0;gap:0}" \
	"  .lo .lt,.lo .lv{display:none}" \
	"  .li{width:30px;height:30px;border-radius:8px}" \
	"  .li svg{width:15px;height:15px}" \
	"  .mpt{display:block;flex:0 1 auto;min-width:0;max-width:34%}" \
	"  .tbr{gap:4px;margin-left:auto}" \
	"  .tbr .spill{padding:3px 7px;gap:5px;font-size:0;border-radius:999px}" \
	"  .tbr .spill #tb_conn{font-size:11px;font-family:var(--mono)}" \
	"  .tbr .spill::after{content:'online';font-size:9px;color:var(--gr);font-weight:700}" \
	"  #mnuBtn{display:grid;width:34px;height:34px;border-radius:9px;position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);margin:0}" \
	"  #mn{margin-top:var(--tbh);min-height:calc(100vh - var(--tbh))}" \
	"  #ct{max-width:none;padding:10px 10px 24px}" \
	"  .tnav{display:none;position:fixed;top:58px;left:8px;right:8px;bottom:auto;max-height:calc(100vh - 68px);z-index:1065;grid-template-columns:repeat(2,minmax(0,1fr));align-items:stretch;gap:6px;padding:8px;overflow-y:auto;background:rgba(20,22,27,.98);border:1px solid var(--bd2);border-radius:16px;box-shadow:var(--shadow-lg),0 0 0 1px rgba(255,255,255,.03) inset}" \
	"  .tnav.open{display:grid!important;pointer-events:auto}" \
	"  .tnav a{min-width:0;min-height:50px;padding:9px 10px;gap:9px;justify-content:flex-start;border:1px solid var(--bd);border-radius:11px;font-size:12px;font-weight:650;box-shadow:var(--shadow)}" \
	"  .tnav a .ni{width:18px;height:18px;opacity:.9;flex:0 0 18px}" \
	"  .tnav a.act{background:linear-gradient(165deg,rgba(56,189,248,.15),rgba(56,189,248,.04));border-color:var(--pg);box-shadow:0 0 0 1px rgba(56,189,248,.08) inset}" \
	"  .tnav a.act .ni{color:var(--p)}" \
	"  body.nav-open{overflow:hidden!important}" \
	"  body.nav-open #mn::before{content:'';position:fixed;z-index:1035;top:var(--tbh);left:0;right:0;bottom:0;background:rgba(4,5,6,.58);pointer-events:auto}" \
	"  .btn{min-height:36px;padding:8px 12px;font-size:12px}" \
	"  .btn.sm{min-height:34px;padding:7px 10px}" \
	"  .tool{min-height:34px;height:auto;padding:6px 9px;font-size:11.5px}" \
	"  .tool.sm{min-height:30px}" \
	"  .fi,.sel,.ea{min-height:36px}" \
	"  .card,.tw{border-radius:11px}" \
	"  .tw{max-width:100%;min-width:0;-webkit-overflow-scrolling:touch;overscroll-behavior-x:contain}" \
	"  .cm th{height:32px;font-size:10px}" \
	"  .cm td{height:34px;font-size:12px}" \
	"  .site-footer{padding:9px 8px;font-size:10px;gap:5px;flex-wrap:wrap;justify-content:center;text-align:center}" \
	"  .mpt,.nav-label{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}" \
	"  /* dashboard */" \
	"  body.pg-status .cg{grid-template-columns:repeat(2,minmax(0,1fr));gap:7px;margin-bottom:10px}" \
	"  body.pg-status .sc{min-height:76px;padding:9px 8px;gap:8px;border-radius:10px}" \
	"  body.pg-status .si_{width:28px;height:28px;border-radius:8px}" \
	"  body.pg-status .si_ svg{width:15px;height:15px}" \
	"  body.pg-status .sl_{font-size:8px;letter-spacing:.055em}" \
	"  body.pg-status .sv{font-size:15px}body.pg-status .sv.mono{font-size:14px}" \
	"  body.pg-status .sd{font-size:8.5px}" \
	"  body.pg-status .shd{margin:8px 0 6px}body.pg-status .stl{font-size:11px}" \
	"  body.pg-status .tw{border:0;background:transparent;box-shadow:none;overflow:visible}" \
	"  body.pg-status .tw table{display:block;width:100%;border-collapse:separate;border-spacing:0}" \
	"  body.pg-status .tw thead{display:none}" \
	"  body.pg-status #p_clients{display:grid!important;gap:8px;padding:0}" \
	"  body.pg-status #p_clients .nw{display:grid!important;width:100%!important;grid-template-columns:repeat(3,minmax(0,1fr));grid-template-rows:40px minmax(48px,auto) minmax(48px,auto);grid-template-areas:'user user user' 'ip caid sid' 'ch con idle';padding:8px!important;gap:5px!important;min-height:0!important;position:relative;overflow:hidden;border-radius:16px!important;box-sizing:border-box;border:1px solid rgba(56,189,248,.24)!important;box-shadow:0 10px 26px rgba(0,0,0,.20)!important}" \
	"  body.pg-status #p_clients .nw td{min-width:0!important;width:auto!important;display:flex!important;flex-direction:column!important;align-items:center!important;justify-content:center!important;gap:2px!important;padding:5px!important;border:1px solid rgba(255,255,255,.07)!important;border-radius:8px!important;background:rgba(255,255,255,.026)!important;box-sizing:border-box!important;white-space:normal!important;overflow:visible!important;overflow-wrap:anywhere!important;text-align:center!important;color:var(--t1)!important;font-size:8.8px!important;line-height:1.15!important}" \
	"  body.pg-status #p_clients .nw td:nth-child(1){position:absolute!important;top:10px!important;left:10px!important;width:22px!important;height:22px!important;padding:0!important;background:transparent!important;border:0!important;box-shadow:none!important;z-index:3!important}" \
	"  body.pg-status #p_clients .nw td:nth-child(9){position:absolute!important;top:10px!important;right:10px!important;width:34px!important;height:34px!important;padding:0!important;background:transparent!important;border:0!important;box-shadow:none!important;z-index:3!important}" \
	"  body.pg-status #p_clients .nw td:nth-child(2){grid-area:user!important;display:flex!important;align-items:center!important;justify-content:center!important;padding:4px 40px!important;background:rgba(56,189,248,.08)!important;border-color:rgba(56,189,248,.18)!important;border-radius:10px!important;font-size:13px!important;font-weight:850!important;color:var(--t0)!important;text-align:center!important}" \
	"  body.pg-status #p_clients .nw td:nth-child(3){grid-area:ip!important;font:9px var(--mono)!important}body.pg-status #p_clients .nw td:nth-child(3)::before{content:'IP';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-status #p_clients .nw td:nth-child(4){grid-area:caid!important;background:rgba(124,92,255,.08)!important;border-color:rgba(124,92,255,.16)!important;font:9.5px var(--mono)!important}body.pg-status #p_clients .nw td:nth-child(4)::before{content:'CAID';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-status #p_clients .nw td:nth-child(5){grid-area:sid!important}body.pg-status #p_clients .nw td:nth-child(5)::before{content:'SID';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-status #p_clients .nw td:nth-child(6){grid-area:ch!important}body.pg-status #p_clients .nw td:nth-child(6)::before{content:'CHANNEL';font:800 7px var(--sans);letter-spacing:.1em;color:var(--t2)}" \
	"  body.pg-status #p_clients .nw td:nth-child(7){grid-area:con!important;font:9px var(--mono)!important}body.pg-status #p_clients .nw td:nth-child(7)::before{content:'CONNECTED';font:800 7px var(--sans);letter-spacing:.08em;color:var(--t2)}" \
	"  body.pg-status #p_clients .nw td:nth-child(8){grid-area:idle!important;font:9px var(--mono)!important}body.pg-status #p_clients .nw td:nth-child(8)::before{content:'IDLE';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-status #p_clients .nw .kb{width:30px!important;height:30px!important;border-radius:9px!important}" \
	"  /* users */" \
	"  body.pg-users #uStats{width:100%;max-width:100%;overflow-x:auto;display:flex;flex-wrap:nowrap;gap:5px;padding-bottom:2px;scrollbar-width:none}" \
	"  body.pg-users #uStats::-webkit-scrollbar{display:none}" \
	"  body.pg-users #uStats .tool{flex:0 0 auto}" \
	"  body.pg-users #uTable{overflow:visible}" \
	"  body.pg-users #uTable table{display:block;width:100%!important;min-width:0!important;max-width:none!important}" \
	"  body.pg-users #uTable thead{display:none}" \
	"  body.pg-users #uTable tbody{display:grid;gap:8px}" \
	"  body.pg-users .ut .urow{display:grid!important;width:100%!important;grid-template-columns:repeat(2,minmax(0,1fr));grid-template-rows:40px minmax(48px,auto) minmax(48px,auto);grid-template-areas:'user user' 'ip country' 'ok exp';padding:8px!important;gap:5px!important;min-height:0!important;position:relative;overflow:hidden;border-radius:16px!important;box-sizing:border-box;border:1px solid rgba(255,255,255,.08)!important;box-shadow:0 10px 26px rgba(0,0,0,.20)!important}" \
	"  body.pg-users .ut .urow td{min-width:0!important;width:auto!important;display:flex!important;flex-direction:column!important;align-items:center!important;justify-content:center!important;gap:2px!important;padding:5px 6px!important;border:1px solid rgba(255,255,255,.07)!important;border-radius:8px!important;background:rgba(255,255,255,.026)!important;box-sizing:border-box!important;white-space:normal!important;overflow:visible!important;overflow-wrap:anywhere!important;text-align:center!important;font-size:9px!important;line-height:1.15!important}" \
	"  body.pg-users .ut .c-en{position:absolute!important;top:10px!important;left:10px!important;width:34px!important;height:34px!important;padding:0!important;border:0!important;background:transparent!important;z-index:3!important}" \
	"  body.pg-users .ut .c-btn{position:absolute!important;top:10px!important;right:10px!important;width:34px!important;height:34px!important;padding:0!important;border:0!important;background:transparent!important;z-index:3!important}" \
	"  body.pg-users .ut .c-user{grid-area:user!important;padding:4px 44px!important;background:rgba(56,189,248,.08)!important;border-color:rgba(56,189,248,.18)!important;border-radius:11px!important}" \
	"  body.pg-users .ut .c-user .ulink{max-width:100%!important;font-size:14px!important;font-weight:850!important;text-align:center!important;white-space:normal!important;overflow:visible!important;text-overflow:clip!important;overflow-wrap:anywhere!important;line-height:1.05!important;padding:0!important}" \
	"  body.pg-users .ut .c-ip{grid-area:ip!important;font:9.5px var(--mono)!important;color:var(--t1)!important}body.pg-users .ut .c-ip::before{content:'IP';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-users .ut .c-country{grid-area:country!important}body.pg-users .ut .c-country::before{content:'COUNTRY';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}body.pg-users .ut .c-country .flg{margin:0!important}" \
	"  body.pg-users .ut .c-ok{grid-area:ok!important;color:var(--gr)!important;background:rgba(74,222,128,.05)!important;border-color:rgba(74,222,128,.14)!important;font-size:10.5px!important;font-weight:850!important}body.pg-users .ut .c-ok::before{content:'CW';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-users .ut .c-exp{grid-area:exp!important;font-size:9.2px!important;line-height:1.1!important}body.pg-users .ut .c-exp::before{content:'EXPIRY';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-users .ut .urow[data-online='0']{grid-template-rows:40px!important;grid-template-areas:'user user'!important;padding:8px!important}" \
	"  body.pg-users .ut .urow[data-online='0'] .c-ip,body.pg-users .ut .urow[data-online='0'] .c-country,body.pg-users .ut .urow[data-online='0'] .c-ok,body.pg-users .ut .urow[data-online='0'] .c-nok,body.pg-users .ut .urow[data-online='0'] .c-exp{display:none!important}" \
	"  body.pg-users .ut .urow td.c-conn,body.pg-users .ut .urow td.c-caid,body.pg-users .ut .urow td.c-nok,body.pg-users .ut .urow td.c-last60,body.pg-users .ut .urow td.c-proto,body.pg-users .ut .urow td.c-idle,body.pg-users .ut .urow td.c-last{display:none!important}" \
	"  body.pg-users .ut .act-b.ed,body.pg-users .ut .act-b.rs{display:none!important}" \
	"  body.pg-users .ut .act-b.dl{display:flex!important;width:32px!important;height:32px!important;border-radius:9px!important}" \
	"  body.pg-users .ut .pw-btn{width:34px!important;height:34px!important;border-radius:9px!important}" \
	"  body.pg-users .ut .urow[data-active='1']{border-color:rgba(74,222,128,.30)!important;}" \
	"  body.pg-users .ut .urow[data-online='1']{border-color:rgba(56,189,248,.30)!important;}" \
	"  body.pg-users .ut .urow[data-expd='1']{border-color:rgba(239,68,68,.28)!important;}" \
	"  body.pg-users .ut .urow[data-en='0']{border-color:rgba(148,163,184,.20)!important;}" \
	"  /* readers */" \
	"  body.pg-readers{height:auto;overflow:auto}" \
"  body.pg-readers #mn{height:auto;min-height:0}" \
"  body.pg-readers #ct{height:auto;min-height:calc(100vh - var(--tbh));max-width:100%!important;padding-left:8px;padding-right:8px}" \
	"  body.pg-readers .rtoolbar{display:flex!important;flex-wrap:wrap;justify-content:center;gap:6px;width:100%;margin:0 auto 8px}" \
	"  body.pg-readers .rtoolbar #rStats{display:flex;flex:1 1 100%;justify-content:flex-start;overflow-x:auto;flex-wrap:nowrap;white-space:nowrap;gap:5px;scrollbar-width:none}" \
	"  body.pg-readers .rtoolbar #rStats::-webkit-scrollbar{display:none}" \
	"  body.pg-readers .rtoolbar #rStats .tool{flex:0 0 auto}" \
	"  body.pg-readers .rtoolbar .usrch{width:100%;min-width:0;flex:1 1 180px;height:34px}" \
	"  body.pg-readers .rtoolbar>.tgrp{justify-content:center;flex:1 1 auto;flex-wrap:wrap}" \
	"  body.pg-readers #rTable{flex:none;width:100%;max-width:100%;min-width:0;max-height:none;height:auto;overflow:visible;margin:0 auto 8px;padding:0}" \
	"  body.pg-readers #rTable table{display:block;width:100%!important;min-width:0!important;max-width:none!important}" \
	"  body.pg-readers #rTable thead{display:none}" \
	"  body.pg-readers #rTable tbody{display:grid;gap:8px}" \
	"  body.pg-readers .rt .rrow{display:grid!important;width:100%!important;grid-template-columns:repeat(3,minmax(0,1fr));grid-template-rows:40px minmax(48px,auto) minmax(48px,auto);grid-template-areas:'label label label' 'proto dev group' 'caid ok nok';padding:8px!important;gap:5px!important;min-height:0!important;position:relative;overflow:hidden;border-radius:16px!important;box-sizing:border-box;border:1px solid rgba(255,255,255,.08)!important;box-shadow:0 10px 26px rgba(0,0,0,.20)!important}" \
	"  body.pg-readers .rt .rrow td{min-width:0!important;width:auto!important;display:flex!important;flex-direction:column!important;align-items:center!important;justify-content:center!important;gap:2px!important;padding:5px!important;border:1px solid rgba(255,255,255,.07)!important;border-radius:8px!important;background:rgba(255,255,255,.026)!important;box-sizing:border-box!important;white-space:normal!important;overflow:visible!important;overflow-wrap:anywhere!important;text-overflow:clip!important;text-align:center!important;font-size:9px!important;line-height:1.15!important}" \
	"  body.pg-readers .rt .c-rstate{position:absolute!important;top:10px!important;left:10px!important;width:34px!important;height:34px!important;padding:0!important;border:0!important;background:transparent!important;z-index:3!important}" \
	"  body.pg-readers .rt .c-btn{position:absolute!important;top:10px!important;right:10px!important;width:70px!important;height:34px!important;padding:0!important;border:0!important;background:transparent!important;z-index:3!important}" \
	"  body.pg-readers .rt .c-rlabel{grid-area:label!important;padding:4px 44px!important;background:rgba(56,189,248,.08)!important;border-color:rgba(56,189,248,.18)!important;border-radius:11px!important}" \
	"  body.pg-readers .rt .c-rlabel .rlink{max-width:100%!important;font-size:13px!important;font-weight:850!important;white-space:normal!important;overflow:visible!important;text-overflow:clip!important;overflow-wrap:anywhere!important;text-align:center!important;line-height:1.05!important;padding:0!important}" \
	"  body.pg-readers .rt .c-rproto{grid-area:proto!important;background:rgba(124,92,255,.08)!important;border-color:rgba(124,92,255,.16)!important}body.pg-readers .rt .c-rproto::before{content:'TYPE';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-readers .rt .c-rdev{grid-area:dev!important;font:8.6px var(--mono)!important;color:var(--t1)!important;line-height:1.1!important}body.pg-readers .rt .c-rdev::before{content:'ENDPOINT';font:800 7px var(--sans);letter-spacing:.10em;color:var(--t2)}" \
	"  body.pg-readers .rt .c-rgroup{grid-area:group!important}body.pg-readers .rt .c-rgroup::before{content:'GROUP';font:800 7px var(--sans);letter-spacing:.10em;color:var(--t2)}" \
	"  body.pg-readers .rt .c-rcaid{grid-area:caid!important;font:9.5px var(--mono)!important}body.pg-readers .rt .c-rcaid::before{content:'CAID';font:800 7px var(--sans);letter-spacing:.10em;color:var(--t2)}" \
	"  body.pg-readers .rt .c-rstat-ok{grid-area:ok!important;color:var(--gr)!important;background:rgba(74,222,128,.05)!important;border-color:rgba(74,222,128,.14)!important;font-weight:850!important}body.pg-readers .rt .c-rstat-ok::before{content:'OK';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-readers .rt .c-rstat-nok{grid-area:nok!important;color:var(--re)!important;background:rgba(239,68,68,.045)!important;border-color:rgba(239,68,68,.12)!important;font-weight:850!important}body.pg-readers .rt .c-rstat-nok::before{content:'NOK';font:800 7px var(--sans);letter-spacing:.11em;color:var(--t2)}" \
	"  body.pg-readers .rt .badge{font-size:8.5px;padding:3px 6px;border-radius:999px;max-width:100%;overflow-wrap:anywhere;white-space:normal;text-align:center}" \
	"  body.pg-readers .rt .act-b.ed,body.pg-readers .rt .act-b.dl{width:31px!important;height:31px!important;border-radius:9px!important}" \
	"  body.pg-readers .rt .pw-btn{width:34px!important;height:34px!important;border-radius:9px!important}" \
	"  body.pg-readers .rt .rrow[data-active='1']{border-color:rgba(74,222,128,.30)!important;}" \
	"  body.pg-readers .rt .rrow[data-enabled='0']{border-color:rgba(148,163,184,.22)!important;}" \
	"  /* config */" \
	"  body.pg-config #ct{padding:8px 9px 22px}" \
	"  body.pg-config .cfg-page{max-width:100%!important;margin:0 auto}" \
	"  body.pg-config .cfg-tabs{position:sticky;top:6px;width:100%;padding:3px;gap:3px;margin:0 auto 8px;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));z-index:8;justify-content:center}" \
	"  body.pg-config .cfg-tab{min-height:36px;padding:0 8px;font-size:11px}" \
	"  body.pg-config .cfg-panel{scroll-margin-top:58px}" \
	"  body.pg-config .cfg-card{margin-bottom:9px!important;border-radius:10px!important}" \
	"  body.pg-config .cfg-head{padding:9px 11px!important}" \
	"  body.pg-config .cfg-body{padding:10px 11px!important}" \
	"  body.pg-config .cfg-field{display:grid!important;grid-template-columns:1fr!important;gap:5px!important;padding:8px 0!important}" \
	"  body.pg-config .cfg-label{margin:0!important;font-size:10.5px!important}" \
	"  body.pg-config .cfg-input{min-height:36px;padding:8px 9px!important;font-size:12px!important}" \
	"  body.pg-config .cfg-toggle-row{margin-top:9px!important;gap:6px!important}" \
	"  body.pg-config .cfg-toggle{padding:7px 9px!important;font-size:11px!important}" \
	"  body.pg-config .cfg-save{margin:13px 0 2px!important}" \
	"  body.pg-status #p_clients .erow{display:block!important;width:100%!important;text-align:center!important;background:transparent!important;border:0!important;box-shadow:none!important;padding:0!important}" \
"  body.pg-status #p_clients .erow td{display:flex!important;align-items:center!important;justify-content:center!important;width:100%!important;min-height:82px!important;padding:18px 10px!important;text-align:center!important;border:1px solid var(--bd)!important;border-radius:14px!important;color:var(--t1)!important;box-sizing:border-box!important}" \
"  body.pg-failban .erow{display:block!important;width:100%!important;text-align:center!important;background:transparent!important;border:0!important;box-shadow:none!important;padding:0!important}" \
"  body.pg-failban .erow td{display:flex!important;align-items:center!important;justify-content:center!important;width:100%!important;min-height:92px!important;padding:20px 12px!important;text-align:center!important;border:1px solid var(--bd)!important;border-radius:14px!important;box-sizing:border-box!important}" \
"  /* fail-ban */" \
	"  body.pg-failban #ct{padding:8px 9px 20px}" \
	"  body.pg-failban .sbar{grid-template-columns:repeat(3,minmax(0,1fr));gap:6px;margin-bottom:8px}" \
	"  body.pg-failban .sbar-item{padding:10px 7px;min-height:72px;border-radius:12px}" \
	"  body.pg-failban .sbar-item .sbl{font-size:7.5px}body.pg-failban .sbar-item .sbv{font-size:17px;line-height:1.05}" \
	"  body.pg-failban .tw{border:0;background:transparent;box-shadow:none;overflow:visible}" \
	"  body.pg-failban .tw table{display:block;width:100%;min-width:0!important;border-collapse:separate;border-spacing:0}" \
	"  body.pg-failban .tw thead{display:none}" \
	"  body.pg-failban #fbBody{display:grid;gap:8px}" \
	"  body.pg-failban #fbBody .fbrow{display:grid!important;grid-template-columns:repeat(2,minmax(0,1fr));grid-auto-rows:minmax(68px,auto);padding:8px;gap:7px;min-height:0;border:1px solid var(--bd);border-top:2px solid rgba(249,115,22,.58);border-radius:14px;box-shadow:var(--shadow),0 0 0 1px rgba(249,115,22,.04);align-items:stretch}" \
	"  body.pg-failban #fbBody .fbrow td{display:flex!important;width:auto!important;height:auto!important;min-height:68px!important;padding:8px!important;border:1px solid rgba(255,255,255,.055)!important;background:rgba(255,255,255,.018)!important;white-space:normal;overflow:hidden;text-overflow:ellipsis;box-sizing:border-box!important;border-radius:10px!important}" \
	"  body.pg-failban #fbBody .fbc-value{flex-direction:column!important;gap:4px!important;align-items:center!important;justify-content:center!important}" \
	"  body.pg-failban #fbBody .fbc-label{font-size:7px!important;letter-spacing:.09em!important;color:var(--t2)!important}" \
	"  body.pg-failban #fbBody .fbc-ip span:last-child{font:700 11px var(--mono)!important;color:var(--t0)!important}" \
	"  body.pg-failban #fbBody .fbc-country{background:rgba(56,189,248,.035)!important}body.pg-failban #fbBody .fbc-left{background:rgba(249,115,22,.045)!important}body.pg-failban #fbBody .fbc-action{background:rgba(74,222,128,.035)!important}body.pg-failban #fbBody .fbc-country .fbflag img{width:18px;height:13px}" \
	"  body.pg-failban #fbBody .fbcountry-name{font-size:9px;color:var(--t1);max-width:100%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}" \
	"  body.pg-failban #fbBody .fbc-left .fbcd{font:800 11px var(--mono)!important;color:var(--or2)!important}" \
	"  body.pg-failban #fbBody .fbc-action{display:flex!important;align-items:center;justify-content:center!important;padding:8px!important}" \
	"  body.pg-failban #fbBody .fbc-action .tool{min-height:34px;min-width:74px;padding:7px 11px;font-size:10.5px;border-radius:9px;justify-content:center}" \
"  /* files */" \
	"  body.pg-files .file-page{width:100%;max-width:none!important}" \
	"  body.pg-files .file-tabs{width:100%;justify-content:center;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:4px;padding:0 2px}" \
	"  body.pg-files .file-tabs .ctab{width:100%;min-height:34px;padding:0 8px;font-size:11px}" \
	"  body.pg-files .file-card{width:100%;border-radius:12px!important}" \
	"  body.pg-files #fileArea{display:block;width:100%;height:calc(100vh - 210px);min-height:420px;max-height:none;box-sizing:border-box;padding:12px 13px;font-size:12px;line-height:1.72;resize:none;tab-size:4}" \
	"  body.pg-files .file-actions{display:flex;flex-direction:column;align-items:stretch;padding:8px 10px;gap:8px}" \
	"  body.pg-files .file-info{max-width:100%;width:100%;font-size:10px}" \
	"  body.pg-files .file-buttons{width:100%;display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px}" \
	"  body.pg-files .file-buttons .tool{width:100%;min-height:34px;padding:7px 6px}" \
	"  /* tvcas */" \
	"  body.pg-tvcas .tv-wrap{max-width:100%;padding:0 1px}" \
	"  body.pg-tvcas .tv-card{padding:11px 10px;border-radius:10px;margin-bottom:8px}" \
	"  body.pg-tvcas .tv-tabs{display:grid;grid-template-columns:1fr 1fr;gap:4px;margin-bottom:8px}" \
	"  body.pg-tvcas .tv-tab{width:100%;min-height:36px;padding:7px 8px;font-size:11px}" \
	"  body.pg-tvcas .tv-rgrp{gap:7px 12px;margin-bottom:8px}" \
	"  body.pg-tvcas .tv-inp{font-size:12px;padding:8px 10px}" \
	"  body.pg-tvcas .tv-btn{width:100%;justify-content:center;margin-top:9px;padding:8px 12px;font-size:12px}" \
	"  body.pg-tvcas .tv-tbl{display:block;width:100%}" \
	"  body.pg-tvcas .tv-tbl tbody{display:block}" \
	"  body.pg-tvcas .tv-tbl tr{display:block;padding:7px 10px;border-bottom:1px solid var(--bd)}" \
	"  body.pg-tvcas .tv-tbl td{display:block!important;width:auto!important;padding:2px 0!important;border:0!important;font-size:11.5px!important;word-break:break-word}" \
	"  body.pg-tvcas .tv-tbl td.tk{width:auto!important;background:transparent;border:0;color:var(--t2);font-size:8.5px!important;letter-spacing:.09em;padding-bottom:1px!important}" \
	"  body.pg-tvcas .tv-tbl td.tv{padding-left:0!important;font-size:12px!important}" \
	"  body.pg-tvcas .tv-split{display:flex;flex-direction:column}body.pg-tvcas .tv-split-box{border-right:0;border-bottom:1px solid var(--bd)}body.pg-tvcas .tv-split-box:last-child{border-bottom:0}" \
	"  /* power */" \
	"  body.pg-power .pw-grid{grid-template-columns:1fr 1fr!important;gap:8px!important}body.pg-power .pw-tile{min-height:124px!important;border-radius:14px!important}" \
	"  /* modals */" \
	"  .mo{padding:8px!important;align-items:flex-end!important}" \
	"  .mo .mc{width:100%;max-width:100%;max-height:calc(100vh - 16px)!important;border-radius:14px 14px 10px 10px!important}" \
	"  .mo .mh{min-height:46px;padding:8px 11px!important}.mo .mh h2{font-size:14px!important}" \
	"  .mo .mb{max-height:calc(100vh - 118px)!important}" \
	"  .reader-modal,.user-modal{width:100%;max-width:100%}" \
	"  .reader-topgrid,.reader-cccam-grid,.reader-pcsc-grid,.reader-routing-grid,.user-g3,.user-bottom-grid,.g2{grid-template-columns:1fr!important}" \
	"  .reader-enable{grid-column:auto!important}" \
	"  .reader-modal .fi,.user-modal .fi{width:100%;max-width:100%;box-sizing:border-box}" \
	"  .reader-modal .mf,.user-modal .mf{flex-direction:column;align-items:stretch;padding:8px 11px}.reader-modal .mf .btn,.user-modal .mf .btn{width:100%;min-width:0}" \
	"  /* live log */" \
	"  body.pg-livelog #lw{width:100%;max-width:100%;min-width:0;overflow:auto;border-radius:10px;-webkit-overflow-scrolling:touch;overscroll-behavior:contain}" \
	"  body.pg-livelog #lp{font-size:11.5px;line-height:1.65}body.pg-livelog #lp span{white-space:pre-wrap;overflow-wrap:anywhere;word-break:break-word}" \
	"  body.pg-livelog .ll-statusbar{justify-content:center;font-size:10px;padding:6px 8px}" \
	"  body.pg-livelog .ll-settings summary{min-height:38px;padding:0 10px}" \
	"  body.pg-livelog .ll-settings-body{padding:9px 10px 10px}" \
	"  body.pg-livelog .ll-dbrow{justify-content:flex-start;overflow-x:auto;flex-wrap:nowrap;white-space:nowrap;padding-bottom:2px;scrollbar-width:none}body.pg-livelog .ll-dbrow::-webkit-scrollbar{display:none}body.pg-livelog .ll-dbrow .dt{flex:0 0 auto}" \
	"  body.pg-livelog .ll-controls{display:grid;grid-template-columns:1fr 1fr;gap:6px}body.pg-livelog .ll-controls .ls{grid-column:1/-1;max-width:none;width:100%;min-width:0}body.pg-livelog .ll-controls .btn,body.pg-livelog .ll-controls .ll-chk{width:100%;justify-content:center}body.pg-livelog .ll-limit{grid-column:1/-1;text-align:center}" \
	"}" \

#endif
