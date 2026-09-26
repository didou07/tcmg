#ifndef TCMG_LIVELOG_JS_H_
#define TCMG_LIVELOG_JS_H_
#define TCMG_LIVELOG_JS \
"(function(){\n" \
"var root=document.getElementById('lw'),pre=document.getElementById('lp');\n" \
"if(!root||!pre)return;\n" \
"var lastid=parseInt(root.getAttribute('data-next')||'0',10)||0,curmask=parseInt(root.getAttribute('data-mask')||'0',10)||0;\n" \
"var hovered=0,_pollBusy=0,filterRe=null,visCount=pre.children.length,pollTimer=null;\n" \
"var masks=[1,2,4,8,16,32,64,128];\n" \
"function updateDbgUI(){var el=document.getElementById('dbmask');if(el)el.textContent='0x'+curmask.toString(16).toUpperCase().padStart(4,'0');masks.forEach(function(m){var b=document.getElementById('db'+m);if(b)b.className='dt'+(curmask&m?' on':'');});var a=document.getElementById('dbALL');if(a)a.className='dt'+(curmask===65535?' on':'');}\n" \
"function setLiveState(ok){var s=document.getElementById('llstate');if(s){s.textContent=ok?'Live':'Reconnecting';s.className='ll-mask '+(ok?'':'warn');}}\n" \
"function setLineCnt(v){visCount=v;var lc=document.getElementById('linecnt');if(lc)lc.textContent=v;}\n" \
"function toggleDbg(m){curmask^=m;updateDbgUI();tcmg_api('/logpoll?since=999999999&debug='+curmask).catch(function(){setLiveState(false);});return false;}\n" \
"function toggleAll(){curmask=(curmask===65535)?0:65535;updateDbgUI();tcmg_api('/logpoll?since=999999999&debug='+curmask).catch(function(){setLiveState(false);});return false;}\n" \
"window.toggleDbg=toggleDbg;window.toggleAll=toggleAll;\n" \
"function applyFilter(){var el=document.getElementById('filter'),s=el?el.value:'';try{filterRe=s?new RegExp(s,'i'):null;}catch(e){filterRe=null;}var spans=pre.children,vis=0;for(var i=0;i<spans.length;i++){var show=!filterRe||filterRe.test(spans[i].getAttribute('data-r')||'');spans[i].style.display=show?'':'none';if(show)vis++;}setLineCnt(vis);}\n" \
"window.applyFilter=applyFilter;\n" \
"function clearLog(){pre.textContent='';setLineCnt(0);tcmg_api('/logpoll?since=999999999&debug='+curmask).then(function(d){if(d&&typeof d.next==='number')lastid=d.next;setLiveState(true);}).catch(function(){setLiveState(false);});}\n" \
"window.clearLog=clearLog;\n" \
"function prepSave(){var spans=pre.children,txt='';for(var i=0;i<spans.length;i++){if(spans[i].style.display==='none')continue;txt+=(spans[i].getAttribute('data-r')||'')+'\\n';}var url=URL.createObjectURL(new Blob([txt],{type:'text/plain'})),a=document.createElement('a');a.href=url;a.download='tcmg.log';document.body.appendChild(a);a.click();document.body.removeChild(a);setTimeout(function(){URL.revokeObjectURL(url);},2000);}\n" \
"window.prepSave=prepSave;\n" \
"var LC_HIT=/found \\(|: found/i,LC_MISS=/not found|: miss/i,LC_ERR=/fatal|forced exit|failed:|\\berror\\b/i,LC_WARN=/\\bwarn\\b|rejected|repeated/i,LC_DOWN=/shutting down|shutdown/i,LC_START=/started|listening|>> tcmg <</i,LC_RELOAD=/\\breload(ed)?\\b|re-opened/i,LC_BAN=/\\bban\\b|login failed/i,LC_WEBIF=/\\bwebif\\b|\\bhttp\\b|\\blogin\\b/i,LC_NET=/\\(net\\)|\\bconnect\\b|disconnect/i,LC_EMU=/\\(emu\\)|\\bemu:/i,LC_DBG=/0x[0-9a-f]+|\\bdebug\\b/i;\n" \
"function colorLine(s){if(LC_HIT.test(s))return '#4ade80';if(LC_MISS.test(s))return '#f87171';if(LC_ERR.test(s))return '#ff6b6b';if(LC_WARN.test(s))return '#fbbf24';if(LC_DOWN.test(s))return '#c084fc';if(LC_START.test(s))return '#22d3ee';if(LC_RELOAD.test(s))return '#60a5fa';if(LC_BAN.test(s))return '#fb923c';if(LC_WEBIF.test(s))return '#818cf8';if(LC_NET.test(s))return '#a78bfa';if(LC_EMU.test(s))return '#67e8f9';if(LC_DBG.test(s))return '#64748b';return null;}\n" \
"function colorExisting(){for(var i=0;i<pre.children.length;i++){var c=colorLine(pre.children[i].getAttribute('data-r')||'');if(c)pre.children[i].style.color=c;}}\n" \
"function appendLines(entries){var maxl=200,frag=document.createDocumentFragment(),addedVis=0;for(var i=0;i<entries.length;i++){var e=entries[i],line=typeof e==='string'?e:((e.line||'')+'');var span=document.createElement('span'),c=colorLine(line);if(c)span.style.color=c;span.setAttribute('data-r',line);var show=!filterRe||filterRe.test(line);if(!show)span.style.display='none';span.appendChild(document.createTextNode(line));frag.appendChild(span);if(show)addedVis++;}pre.appendChild(frag);var overflow=pre.children.length-maxl,removedVis=0;while(overflow>0&&pre.firstChild){if(pre.firstChild.style.display!=='none')removedVis++;pre.removeChild(pre.firstChild);overflow--;}setLineCnt(visCount+addedVis-removedVis);if(!hovered&&addedVis&&document.getElementById('asc')&&document.getElementById('asc').checked)root.scrollTop=root.scrollHeight;}\n" \
"function poll(){if(_pollBusy)return;if(document.getElementById('paused')&&document.getElementById('paused').checked){schedulePoll(2000);return;}_pollBusy=1;tcmg_api('/logpoll?since='+lastid+'&debug='+curmask).then(function(d){if(!d)return;if(typeof d.debug==='number'&&d.debug!==curmask){curmask=d.debug;updateDbgUI();}if(typeof d.next==='number')lastid=d.next;if(d.lines&&d.lines.length)appendLines(d.lines);setLiveState(true);_pollBusy=0;schedulePoll(1000);}).catch(function(){_pollBusy=0;setLiveState(false);schedulePoll(3000);});}\n" \
"function schedulePoll(ms){if(pollTimer)clearTimeout(pollTimer);pollTimer=setTimeout(function(){pollTimer=null;poll();},ms);}\n" \
"root.onmouseenter=function(){hovered=1;};root.onmouseleave=function(){hovered=0;};\n" \
"updateDbgUI();colorExisting();setLineCnt(pre.children.length);poll();\n" \
"})();\n"
#endif
