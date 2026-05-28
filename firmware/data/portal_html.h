#ifndef PORTAL_HTML_H
#define PORTAL_HTML_H

const char PORTAL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=yes">
<title>Elnk 配网</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{--bk:#1a1a1a;--gy:#888;--bg:#fafaf7;--bd:#d4d4cf;--f:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;--fs:'Georgia',serif}
html{font-size:16px;-webkit-font-smoothing:antialiased;overflow-y:scroll}
body{font-family:var(--f);background:linear-gradient(135deg,#f5f5f0,#e8e8e0);color:var(--bk);line-height:1.6;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:16px}
.card{background:#fff;border-radius:20px;box-shadow:0 8px 40px rgba(0,0,0,.08);width:100%;max-width:380px;padding:32px 24px}
.hdr{text-align:center;margin-bottom:24px}
.logo{width:50px;height:50px;background:var(--bk);border-radius:14px;margin:0 auto 12px;display:flex;align-items:center;justify-content:center;color:#fff;font-family:var(--fs);font-size:1.5rem;font-weight:700}
.hdr h1{font-family:var(--fs);font-size:1.4rem;font-weight:700;margin-bottom:2px}
.hdr p{font-size:.82rem;color:var(--gy)}
.steps{display:flex;align-items:center;justify-content:center;margin-bottom:20px}
.dot{width:26px;height:26px;border-radius:50%;border:2px solid var(--bd);display:flex;align-items:center;justify-content:center;font-size:.7rem;font-weight:600;color:var(--gy);background:#fff;flex-shrink:0}
.dot.a{border-color:var(--bk);background:var(--bk);color:#fff}
.dot.d{border-color:#22c55e;background:#22c55e;color:#fff}
.ln{width:48px;height:2px;background:var(--bd)}
.ln.d{background:#22c55e}
.hidden{display:none!important}
.lbl{display:block;font-size:.78rem;font-weight:500;color:var(--gy);margin-bottom:5px}
.inp{width:100%;padding:10px 12px;font-family:var(--f);font-size:.85rem;border:1px solid var(--bd);border-radius:8px;background:var(--bg);color:var(--bk);outline:none;-webkit-appearance:none}
.inp:focus{border-color:var(--bk)}
.fg{margin-bottom:12px}
.pw{position:relative}
.pw .inp{padding-right:40px}
.pw-btn{position:absolute;right:10px;top:50%;transform:translateY(-50%);background:none;border:none;cursor:pointer;color:var(--gy);padding:4px}
.btn{display:block;width:100%;padding:12px;font-family:var(--f);font-size:.9rem;font-weight:600;color:#fff;background:var(--bk);border:none;border-radius:10px;cursor:pointer}
.btn:hover{background:#333}
.btn:disabled{opacity:.6;cursor:not-allowed}
.btn .sp{display:none;width:16px;height:16px;border:2px solid rgba(255,255,255,.3);border-top-color:#fff;border-radius:50%;animation:spin .7s linear infinite;margin:0 auto}
.btn.ld .bt{display:none}.btn.ld .sp{display:block}
.btn-ghost{color:var(--bk);background:#fff;border:1px solid var(--bd)}
.btn-ghost:hover,.btn-ghost:active{color:#fff;background:var(--bk);border-color:var(--bk)}
.btn-ghost.ld .sp{border-color:rgba(0,0,0,.25);border-top-color:var(--bk)}
.btn-ghost.ld:hover .sp,.btn-ghost.ld:active .sp{border-color:rgba(255,255,255,.3);border-top-color:#fff}
@keyframes spin{to{transform:rotate(360deg)}}
.wl{list-style:none;margin-bottom:10px}
.wi{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;border:1px solid var(--bd);border-radius:8px;margin-bottom:6px;cursor:pointer;background:#fff}
.wi:hover{border-color:var(--bk);background:var(--bg)}
.wi.sel{border-color:var(--bk);background:var(--bg)}
.wn{font-size:.85rem;font-weight:500;display:flex;align-items:center;gap:6px}
.ws{display:flex;align-items:flex-end;gap:1.5px;height:14px}
.ws .b{width:3px;background:#e0e0dc;border-radius:1px}
.ws .b.a{background:var(--bk)}
.wk{width:12px;height:12px;opacity:.4}
.wtabs{display:flex;border:1px solid var(--bd);border-radius:8px;overflow:hidden;margin-bottom:12px}
.wtab{flex:1;padding:8px 4px;text-align:center;font-size:.78rem;font-weight:500;cursor:pointer;background:#fff;color:var(--gy);border-right:1px solid var(--bd);user-select:none}
.wtab:last-child{border-right:none}
.wtab:hover{background:var(--bg)}
.wtab.act{background:var(--bk);color:#fff}
.si{width:56px;height:56px;border-radius:50%;background:#22c55e;display:flex;align-items:center;justify-content:center;margin:0 auto 12px;animation:sc .4s cubic-bezier(.175,.885,.32,1.275)}
@keyframes sc{0%{transform:scale(0)}100%{transform:scale(1)}}
.di{font-size:.75rem;color:var(--gy);line-height:2}
.di dt{display:inline;font-weight:600}
.di dd{display:inline;margin-left:4px;font-family:'SF Mono','Fira Code',monospace}
.st{margin-top:16px;padding:10px 14px;border-radius:8px;font-size:.78rem;font-weight:500;text-align:center}
.st.w{background:var(--bg);color:var(--gy)}
.st.s{background:#dcfce7;color:#15803d}
.st.e{background:#fef2f2;color:#dc2626}
.st.c{background:#fef9c3;color:#a16207}
.cd{font-family:'SF Mono','Fira Code',monospace;font-size:.82rem;color:var(--gy);margin-top:8px}
</style>
</head>
<body>
<div class="card">
<div style="position:absolute;top:16px;right:16px;">
<button id="langBtn" onclick="toggleLang()" style="background:none;border:1px solid var(--bd);border-radius:6px;padding:4px 8px;font-size:0.75rem;cursor:pointer;color:var(--gy)">EN</button>
</div>
<div class="hdr">
<div class="logo">E</div>
<h1 id="logoText">Elnk 配网</h1>
<p id="subtitle">WiFi 配网</p>
</div>

<div class="steps">
<div class="dot a" id="d1">1</div>
<div class="ln" id="l1"></div>
<div class="dot" id="d2"><svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round"><path d="M20 6L9 17l-5-5"/></svg></div>
</div>

<!-- Step 1: WiFi -->
<div id="s1">
<div class="wtabs">
<div class="wtab" id="wtScan" onclick="switchWTab('scan')">选择网络</div>
<div class="wtab act" id="wtMan" onclick="switchWTab('manual')">手动输入</div>
</div>
<div id="wScan" class="hidden">
<div id="wScanLoading" style="text-align:center;padding:20px 0;color:var(--gy);font-size:.85rem">
<div class="sp" style="display:inline-block;width:16px;height:16px;border:2px solid rgba(0,0,0,.1);border-top-color:var(--bk);border-radius:50%;animation:spin .7s linear infinite;vertical-align:middle;margin-right:8px"></div>
<span id="scanText">正在扫描附近网络...</span>
</div>
<ul class="wl" id="wifiList" style="display:none"></ul>
<div id="wSel" class="hidden" style="display:none;align-items:center;justify-content:space-between;padding:10px 12px;border:1px solid var(--bk);border-radius:8px;background:var(--bg);margin-bottom:10px">
<span id="wSelName" style="font-size:.85rem;font-weight:500"></span>
<a id="reselectBtn" onclick="reShowList()" style="font-size:.72rem;color:var(--gy);cursor:pointer">重新选择</a>
</div>
</div>
<div id="wMan">
<div class="fg">
<label class="lbl" id="lblSsid">WiFi 名称 (SSID)</label>
<input type="text" class="inp" id="ssidIn" placeholder="输入 SSID">
</div>
</div>
<div class="fg">
<label class="lbl" id="lblPw">WiFi 密码</label>
<div class="pw">
<input type="text" class="inp" id="pwIn" placeholder="输入密码">
<button class="pw-btn" onclick="togglePw()" type="button" id="tpb">
<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>
</button>
</div>
</div>
<div class="fg">
<label class="lbl" id="lblQuotaUrl">额度查询地址 *</label>
<input type="text" class="inp" id="quotaUrlIn" placeholder="http://IP:PORT/path">
</div>
<div class="fg">
<label class="lbl" id="lblSleepMin">更新间隔 (分钟)</label>
<input type="number" class="inp" id="sleepMinIn" placeholder="5" min="1" max="1440" value="5">
</div>
<details style="margin-bottom:12px">
<summary style="font-size:.78rem;color:var(--gy);cursor:pointer;margin-bottom:6px" id="advToggle">高级设置</summary>
<div class="fg">
<label style="display:flex;align-items:center;gap:8px;cursor:pointer;font-size:.78rem;color:var(--gy);margin-bottom:8px">
<input type="checkbox" id="consoleToggle" onchange="toggleConsole()" style="width:16px;height:16px;accent-color:var(--bk)">
<span id="lblConsoleToggle">启用智控台</span>
</label>
<div id="consoleField" style="display:none">
<label class="lbl" id="lblConsole">智控台地址</label>
<input type="text" class="inp" id="consoleIn" placeholder="http://IP:8080">
</div>
</div>
</details>
<button class="btn btn-ghost" id="cBtn" onclick="doConnect()"><span class="bt" id="btnConnText">连接并保存</span><div class="sp"></div></button>
</div>

<!-- Step 2: Success -->
<div id="s2" class="hidden" style="text-align:center;padding:16px 0">
<div class="si"><svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="3" stroke-linecap="round"><path d="M20 6L9 17l-5-5"/></svg></div>
<h3 id="s2Title" style="font-family:var(--fs);font-size:1.05rem;margin-bottom:4px">配网完成</h3>
<p style="font-size:.82rem;color:var(--gy)">已连接到 <strong id="cSSID"></strong></p>
<p id="s2Hint" style="font-size:.78rem;color:var(--gy);margin-top:8px">设备将自动重启并连接 WiFi。</p>

<div style="margin-top:16px;display:flex;gap:8px;justify-content:center">
<button class="btn" onclick="doRestart()" id="btnRest" style="width:auto;padding:9px 20px;font-size:.82rem">立即重启</button>
<button class="btn" onclick="cancelCountdown()" id="cdCancelBtn" style="width:auto;padding:9px 16px;font-size:.8rem;background:var(--bg);color:var(--bk)">取消</button>
</div>
<p class="cd"><span id="cdN">10</span><span id="cdSuffix"> 秒后自动重启</span></p>
</div>

<hr style="border:none;border-top:1px dashed var(--bd);margin:20px 0">
<dl class="di">
<div style="margin-bottom:3px"><dt>MAC:</dt><dd id="devMAC">--</dd></div>
<div style="margin-bottom:3px"><dt id="lblBat">电池:</dt><dd id="devBat">--</dd></div>
</dl>
<div class="st w" id="pSt">等待配网...</div>
</div>

<script>
var lang=localStorage.getItem('eink_lang')||'zh';
var i18n={
zh:{
title:"Elnk 配网",
logoText:"Elnk 配网",
subtitle:"WiFi 配网",
tabScan:"选择网络",
tabMan:"手动输入",
scanning:"正在扫描附近网络...",
reselect:"重新选择",
ssidPh:"输入 SSID",
pwPh:"输入密码",
btnConn:"连接并保存",
s2Title:"配网完成",
s2Hint:"设备将自动重启并连接 WiFi。",
btnRest:"立即重启",
btnCancel:"取消",
cdSuffix:" 秒后自动重启",
lblBat:"电池:",
lblSsid:"WiFi 名称 (SSID)",
lblPw:"WiFi 密码",
lblQuotaUrl:"额度查询地址 *",
phQuotaUrl:"http://IP:PORT/path",
lblSleepMin:"更新间隔 (分钟)",
phSleepMin:"5",
errQuotaUrl:"请输入额度查询地址",
lblConsoleToggle:"启用智控台",
lblConsole:"智控台地址",
stWait:"等待配网...",
errSsid:"请选择或输入 WiFi",
errPw:"请输入密码",
errPwLen:"密码至少 8 位",
msgConn:"正在连接 ",
msgConnOk:"WiFi 已连接",
msgConnFail:"连接失败",
msgReqFail:"请求失败，请重试",
msgSetupOk:"配网完成！",
msgRest:"设备重启中...",
msgCancel:"自动重启已取消",
msgScanFail:"扫描失败，请刷新页面重试或手动输入"
},
en:{
title:"Elnk Setup",
logoText:"Elnk Setup",
subtitle:"WiFi Setup",
tabScan:"Scan",
tabMan:"Manual",
scanning:"Scanning for networks...",
reselect:"Reselect",
ssidPh:"Enter SSID",
pwPh:"Enter Password",
btnConn:"Connect & Save",
s2Title:"Setup Complete",
s2Hint:"Device will restart and connect to WiFi automatically.",
btnRest:"Restart Now",
btnCancel:"Cancel",
cdSuffix:"s to auto-restart",
lblBat:"Battery:",
lblSsid:"WiFi Name (SSID)",
lblPw:"WiFi Password",
lblQuotaUrl:"Quota API URL *",
phQuotaUrl:"http://IP:PORT/path",
lblSleepMin:"Refresh Interval (min)",
phSleepMin:"5",
errQuotaUrl:"Quota URL is required",
lblConsoleToggle:"Enable Console",
lblConsole:"Console URL",
stWait:"Waiting for setup...",
errSsid:"Please select or enter WiFi",
errPw:"Please enter password",
errPwLen:"Password must be at least 8 chars",
msgConn:"Connecting to ",
msgConnOk:"WiFi Connected",
msgConnFail:"Connection Failed",
msgReqFail:"Request failed, try again",
msgSetupOk:"Setup Complete!",
msgRest:"Restarting...",
msgCancel:"Auto-restart cancelled",
msgScanFail:"Scan failed, please refresh or enter manually"
}
};

function t(k){return i18n[lang][k];}

function applyLang(){
document.title=t('title');
document.getElementById('logoText').textContent=t('logoText');
document.getElementById('subtitle').textContent=t('subtitle');
document.getElementById('wtScan').textContent=t('tabScan');
document.getElementById('wtMan').textContent=t('tabMan');
document.getElementById('scanText').textContent=t('scanning');
document.getElementById('reselectBtn').textContent=t('reselect');
document.getElementById('lblSsid').textContent=t('lblSsid');
document.getElementById('ssidIn').placeholder=t('ssidPh');
document.getElementById('lblPw').textContent=t('lblPw');
document.getElementById('lblQuotaUrl').textContent=t('lblQuotaUrl');
document.getElementById('quotaUrlIn').placeholder=t('phQuotaUrl');
document.getElementById('lblSleepMin').textContent=t('lblSleepMin');
document.getElementById('sleepMinIn').placeholder=t('phSleepMin');
document.getElementById('lblConsoleToggle').textContent=t('lblConsoleToggle');
document.getElementById('lblConsole').textContent=t('lblConsole');
document.getElementById('pwIn').placeholder=t('pwPh');
document.getElementById('btnConnText').textContent=t('btnConn');
document.getElementById('s2Title').textContent=t('s2Title');
document.getElementById('s2Hint').textContent=t('s2Hint');
document.getElementById('btnRest').textContent=t('btnRest');
document.getElementById('cdCancelBtn').textContent=t('btnCancel');
document.getElementById('cdSuffix').textContent=t('cdSuffix');
document.getElementById('lblBat').textContent=t('lblBat');
var pSt=document.getElementById('pSt');
if(pSt.textContent===i18n[lang==='zh'?'en':'zh'].stWait)pSt.textContent=t('stWait');
document.getElementById('langBtn').textContent=lang==='zh'?'EN':'中';
}

function toggleLang(){
lang=lang==='zh'?'en':'zh';
localStorage.setItem('eink_lang',lang);
applyLang();
}

var ssid='',ctm=null;

function setStep(n){
var d1=document.getElementById('d1'),d2=document.getElementById('d2');
var l1=document.getElementById('l1');
d1.className='dot'+(n===1?' a':' d');
d2.className='dot'+(n===2?' a d':'');
l1.className='ln'+(n>=2?' d':'');
}

function switchWTab(mode){
var ts=document.getElementById('wtScan'),tm=document.getElementById('wtMan');
var ps=document.getElementById('wScan'),pm=document.getElementById('wMan');
if(mode==='scan'){ts.classList.add('act');tm.classList.remove('act');ps.classList.remove('hidden');pm.classList.add('hidden');ssid='';reShowList();}
else{tm.classList.add('act');ts.classList.remove('act');pm.classList.remove('hidden');ps.classList.add('hidden');ssid='';document.getElementById('ssidIn').value='';document.getElementById('ssidIn').focus();}
}

function selW(el){
var nextSsid=el.dataset.ssid;
document.querySelectorAll('.wi').forEach(function(i){i.classList.remove('sel')});
el.classList.add('sel');ssid=nextSsid;
document.getElementById('wtScan').classList.add('act');
document.getElementById('wtMan').classList.remove('act');
document.getElementById('pwIn').value='';
document.getElementById('wifiList').style.display='none';
var ws=document.getElementById('wSel');ws.style.display='flex';ws.classList.remove('hidden');
document.getElementById('wSelName').textContent=ssid;
}

function reShowList(){
document.getElementById('wtScan').classList.add('act');
document.getElementById('wtMan').classList.remove('act');
document.getElementById('wifiList').style.display='block';
var ws=document.getElementById('wSel');ws.style.display='none';ws.classList.add('hidden');
document.querySelectorAll('.wi').forEach(function(i){i.classList.remove('sel')});
ssid='';
}

function toggleConsole(){var ck=document.getElementById('consoleToggle'),fd=document.getElementById('consoleField');if(ck.checked){fd.style.display='block';document.getElementById('consoleIn').focus();}else{fd.style.display='none';document.getElementById('consoleIn').value='';}}

function togglePw(){
var i=document.getElementById('pwIn'),b=document.getElementById('tpb');
if(i.type==='password'){i.type='text';b.innerHTML='<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M17.94 17.94A10.07 10.07 0 0112 20c-7 0-11-8-11-8a18.45 18.45 0 015.06-5.94"/><path d="M9.9 4.24A9.12 9.12 0 0112 4c7 0 11 8 11 8a18.5 18.5 0 01-2.16 3.19"/><line x1="1" y1="1" x2="23" y2="23"/></svg>';}
else{i.type='password';b.innerHTML='<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg>';}
}

function doConnect(){
var s=ssid||document.getElementById('ssidIn').value.trim();
var p=document.getElementById('pwIn').value;
var st=document.getElementById('pSt'),btn=document.getElementById('cBtn');
if(!s){st.className='st e';st.textContent=t('errSsid');return;}
if(p&&p.length<8){st.className='st e';st.textContent=t('errPwLen');return;}
var qu=document.getElementById('quotaUrlIn').value.trim();if(!qu){st.className='st e';st.textContent=t('errQuotaUrl');return;}
btn.classList.add('ld');btn.disabled=true;
st.className='st c';st.textContent=t('msgConn')+s+' ...';

var sm=document.getElementById('sleepMinIn').value||'5';var fd=new FormData();fd.append('ssid',s);fd.append('pass',p);fd.append('quota_url',qu);var ck=document.getElementById('consoleToggle');var cu=ck.checked?document.getElementById('consoleIn').value.trim():'';fd.append('console_url',cu);fd.append('sleep_min',sm);
fetch('/save_wifi',{method:'POST',body:fd}).then(function(r){return r.json()}).then(function(d){
btn.classList.remove('ld');btn.disabled=false;
if(d.ok){
st.className='st s';st.textContent=t('msgConnOk');
document.getElementById('cSSID').textContent=s;
showSuccess();
}else{
st.className='st e';st.textContent=d.msg||t('msgConnFail');
}
}).catch(function(){
btn.classList.remove('ld');btn.disabled=false;
st.className='st e';st.textContent=t('msgReqFail');
});
}

function showSuccess(){
document.getElementById('s1').classList.add('hidden');
document.getElementById('s2').classList.remove('hidden');
setStep(2);
document.getElementById('pSt').className='st s';
document.getElementById('pSt').textContent=t('msgSetupOk');
var c=10;document.getElementById('cdN').textContent=c;
ctm=setInterval(function(){c--;document.getElementById('cdN').textContent=c;
if(c<=0){clearInterval(ctm);ctm=null;doRestart();}
},1000);
}

function doRestart(){
if(ctm)clearInterval(ctm);
document.getElementById('pSt').className='st c';
document.getElementById('pSt').textContent=t('msgRest');
fetch('/restart',{method:'POST'}).catch(function(){});
}

function cancelCountdown(){
if(ctm){clearInterval(ctm);ctm=null;}
document.getElementById('cdN').textContent='--';
document.querySelector('.cd').textContent=t('msgCancel');
document.getElementById('cdCancelBtn').disabled=true;
}

(function(){
applyLang();
switchWTab('manual');
fetch('/scan').then(function(r){return r.json()}).then(function(d){
document.getElementById('wScanLoading').style.display='none';
var ul=document.getElementById('wifiList');
ul.style.display='block';
ul.innerHTML='';
(d.networks||[]).forEach(function(n){
var bars='';var s=n.rssi||0;
var lvl=s>-50?4:s>-65?3:s>-75?2:1;
for(var i=1;i<=4;i++){
var h=[4,7,10,14][i-1];
bars+='<span class="b'+(i<=lvl?' a':'')+'" style="height:'+h+'px"></span>';
}
var lock=n.secure?'<svg class="wk" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="11" width="18" height="11" rx="2"/><path d="M7 11V7a5 5 0 0110 0v4"/></svg>':'';
var li=document.createElement('li');li.className='wi';li.dataset.ssid=n.ssid;
li.onclick=function(){selW(this)};
li.innerHTML='<span class="wn">'+lock+n.ssid+'</span><span class="ws">'+bars+'</span>';
ul.appendChild(li);
});
}).catch(function(){
document.getElementById('wScanLoading').innerHTML=t('msgScanFail');
});

fetch('/info').then(function(r){return r.json()}).then(function(d){
if(d.mac)document.getElementById('devMAC').textContent=d.mac;
if(d.battery)document.getElementById('devBat').textContent=d.battery;
if(d.ssid){ssid=d.ssid;document.getElementById('ssidIn').value=d.ssid;document.getElementById('wtScan').classList.remove('act');document.getElementById('wtMan').classList.add('act');document.getElementById('wScan').classList.add('hidden');document.getElementById('wMan').classList.remove('hidden');}
if(d.pass!==undefined)document.getElementById('pwIn').value=d.pass;
if(d.quota_url)document.getElementById('quotaUrlIn').value=d.quota_url;
if(d.sleep_min)document.getElementById('sleepMinIn').value=d.sleep_min;
var ck=document.getElementById('consoleToggle');if(d.has_console&&d.console_url){ck.checked=true;document.getElementById('consoleIn').value=d.console_url;document.getElementById('consoleField').style.display='block';document.getElementById('advToggle').parentElement.open=true;}else{ck.checked=false;document.getElementById('consoleIn').value='';document.getElementById('consoleField').style.display='none';}
}).catch(function(){});
})();
</script>
</body>
</html>)rawliteral";

#endif
