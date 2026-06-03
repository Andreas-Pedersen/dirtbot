#pragma once
#include <Arduino.h>
#include <pgmspace.h>

extern int  fwdSpeed, bwdSpeed, accelLevel;
extern int  pinBtnFwd, pinBtnBwd, pinSaberTx;
extern bool motor1Inverted, motor2Inverted;

// ── Hovedside ────────────────────────────────────────────────────────────────

static const char PAGE_MAIN[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="no">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Toilltak</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:system-ui,sans-serif;background:#111;color:#eee;
       display:flex;flex-direction:column;align-items:center;padding:32px 20px}
  h1{font-size:1.8rem;color:#6ecf6e;margin-bottom:8px}
  .card{background:#1e1e1e;border-radius:14px;padding:24px;width:100%;max-width:420px;
        box-shadow:0 4px 20px #0008}
  label{font-size:.85rem;color:#aaa;letter-spacing:.05em;text-transform:uppercase}
  .row{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;margin-top:20px}
  .pct{font-size:2rem;font-weight:700;color:#6ecf6e}
  input[type=range]{width:100%;accent-color:#6ecf6e;height:6px;cursor:pointer;margin-bottom:4px}
  .divider{height:1px;background:#333;margin:20px 0}
  button{width:100%;padding:16px;margin-top:8px;background:#6ecf6e;color:#111;
         border:none;border-radius:10px;font-size:1rem;font-weight:700;cursor:pointer;
         transition:background .15s}
  button:active{background:#4caf50}
  #msg{text-align:center;margin-top:14px;height:20px;font-size:.9rem;color:#6ecf6e}
  .test-card{background:#1e1e1e;border-radius:14px;padding:24px;width:100%;max-width:420px;
             box-shadow:0 4px 20px #0008;margin-top:16px}
  .test-label{font-size:.8rem;color:#666;text-transform:uppercase;letter-spacing:.05em;
              text-align:center;margin-bottom:14px}
  .btn-row{display:flex;gap:12px}
  .btn-drive{flex:1;padding:28px 0;font-size:2rem;border:none;border-radius:12px;
             cursor:pointer;user-select:none;-webkit-user-select:none;touch-action:none;
             background:#2a2a2a;color:#555;transition:background .1s,color .1s}
  .btn-drive.active{background:#6ecf6e;color:#111}
  .settings-link{margin-top:20px;color:#555;font-size:.85rem;text-decoration:none}
  .settings-link:hover{color:#888}
  footer{margin-top:28px;color:#444;font-size:.8rem;text-align:center}
</style>
</head>
<body>
<h1>Toilltak</h1>
<div class="card">
  <div class="row">
    <label>Fart fremover</label>
    <span class="pct" id="fv">%FWD%&thinsp;%</span>
  </div>
  <input type="range" id="fs" min="0" max="100" value="%FWD%"
         oninput="document.getElementById('fv').textContent=this.value+' %'">
  <div class="divider"></div>
  <div class="row">
    <label>Fart bakover</label>
    <span class="pct" id="bv">%BWD%&thinsp;%</span>
  </div>
  <input type="range" id="bs" min="0" max="100" value="%BWD%"
         oninput="document.getElementById('bv').textContent=this.value+' %'">
  <button onclick="save()">Lagre</button>
  <div id="msg"></div>
</div>
<div class="test-card">
  <div class="test-label">Test kj&#248;ring &#8212; hold inne</div>
  <div class="btn-row">
    <button class="btn-drive" id="bfwd"
      onmousedown="startDrive('fwd')" onmouseup="stopDrive()" onmouseleave="stopDrive()"
      ontouchstart="startDrive('fwd')" ontouchend="stopDrive()" ontouchcancel="stopDrive()">
      &#8679;
    </button>
    <button class="btn-drive" id="bbwd"
      onmousedown="startDrive('bwd')" onmouseup="stopDrive()" onmouseleave="stopDrive()"
      ontouchstart="startDrive('bwd')" ontouchend="stopDrive()" ontouchcancel="stopDrive()">
      &#8681;
    </button>
  </div>
</div>
<a class="settings-link" href="/settings">&#9881; Avanserte innstillinger</a>
<footer>&copy; Andreas Pedersen</footer>
<script>
var driveInterval=null,driveDir=null;
function startDrive(dir){
  if(driveDir===dir)return;
  driveDir=dir;
  document.getElementById(dir==='fwd'?'bfwd':'bbwd').classList.add('active');
  fetch('/cmd?dir='+dir);
  clearInterval(driveInterval);
  driveInterval=setInterval(function(){fetch('/cmd?dir='+dir);},1000);
}
function stopDrive(){
  if(!driveDir)return;
  clearInterval(driveInterval);driveInterval=null;driveDir=null;
  document.getElementById('bfwd').classList.remove('active');
  document.getElementById('bbwd').classList.remove('active');
  fetch('/cmd?dir=stop');
}
function save(){
  fetch('/set?fwd='+document.getElementById('fs').value
             +'&bwd='+document.getElementById('bs').value)
  .then(r=>r.ok?r.text():Promise.reject())
  .then(()=>{showMsg('Lagret ✓')})
  .catch(()=>{showMsg('Feil ✗',true)});
}
function showMsg(t,err){
  const el=document.getElementById('msg');
  el.style.color=err?'#e55':'#6ecf6e';
  el.textContent=t;
  setTimeout(()=>el.textContent='',2500);
}
</script>
</body>
</html>)html";

// ── Innstillingsside ──────────────────────────────────────────────────────────

static const char PAGE_SETTINGS[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="no">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Toilltak - Innstillinger</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:system-ui,sans-serif;background:#111;color:#eee;
       display:flex;flex-direction:column;align-items:center;padding:32px 20px}
  h1{font-size:1.8rem;color:#6ecf6e;margin-bottom:24px}
  h2{font-size:1rem;color:#888;text-transform:uppercase;letter-spacing:.08em;margin-bottom:16px}
  .card{background:#1e1e1e;border-radius:14px;padding:24px;width:100%;max-width:420px;
        box-shadow:0 4px 20px #0008;margin-bottom:16px}
  .field{margin-bottom:16px}
  .field label{display:block;font-size:.8rem;color:#aaa;text-transform:uppercase;
               letter-spacing:.05em;margin-bottom:6px}
  .field input[type=number]{width:100%;background:#2a2a2a;border:1px solid #444;
        border-radius:8px;color:#eee;padding:10px 14px;font-size:1rem}
  .field input[type=number]:focus{outline:none;border-color:#6ecf6e}
  .check-row{display:flex;justify-content:space-between;align-items:center;
             padding:12px 0;border-bottom:1px solid #2a2a2a}
  .check-row:last-of-type{border-bottom:none}
  .check-row label{font-size:.95rem;color:#ccc}
  input[type=checkbox]{width:22px;height:22px;accent-color:#6ecf6e;cursor:pointer}
  button{width:100%;padding:16px;margin-top:8px;background:#e07050;color:#fff;
         border:none;border-radius:10px;font-size:1rem;font-weight:700;cursor:pointer;
         transition:background .15s}
  button:active{background:#c05030}
  .btn-reset{background:#333;color:#888;margin-top:32px}
  .btn-reset:active{background:#444}
  #msg{text-align:center;margin-top:14px;font-size:.9rem;color:#e07050}
  .back{margin-top:20px;color:#555;font-size:.85rem;text-decoration:none}
  .back:hover{color:#888}
  .debug-link{margin-top:6px;color:#383838;font-size:.78rem;text-decoration:none;
              display:block;text-align:center}
  .debug-link:hover{color:#555}
  footer{margin-top:28px;color:#444;font-size:.8rem;text-align:center}
</style>
</head>
<body>
<h1>Toilltak</h1>
<div class="card">
  <h2>Maksfart (%)</h2>
  <div class="field">
    <label>Fremover</label>
    <input type="number" id="sfwd" min="0" max="100" value="%FWD%">
  </div>
  <div class="field">
    <label>Bakover</label>
    <input type="number" id="sbwd" min="0" max="100" value="%BWD%">
  </div>
</div>
<div class="card">
  <h2>Akselerasjon</h2>
  <div class="field">
    <label>Nivå (1 = treig, 10 = rask)</label>
    <input type="number" id="accel" min="1" max="10" value="%ACCEL%">
  </div>
</div>
<div class="card">
  <h2>Pinout (GPIO)</h2>
  <div class="field">
    <label>Fremover-knapp</label>
    <input type="number" id="pfwd" min="0" max="39" value="%PIN_FWD%">
  </div>
  <div class="field">
    <label>Bakover-knapp</label>
    <input type="number" id="pbwd" min="0" max="39" value="%PIN_BWD%">
  </div>
  <div class="field">
    <label>Sabertooth TX</label>
    <input type="number" id="ptx" min="0" max="39" value="%PIN_TX%">
  </div>
</div>
<div class="card">
  <h2>Motorretning</h2>
  <div class="check-row">
    <label>Motor 1 invertert</label>
    <input type="checkbox" id="m1inv" %M1INV%>
  </div>
  <div class="check-row">
    <label>Motor 2 invertert</label>
    <input type="checkbox" id="m2inv" %M2INV%>
  </div>
  <button onclick="save()">Lagre og start p&aring; nytt</button>
  <div id="msg"></div>
</div>
<button class="btn-reset" onclick="reset()">&#9888; Tilbakestill til fabrikk</button>
<a class="back" href="/">&#8592; Tilbake</a>
<a class="debug-link" href="/debug">&#128190; Debug-feed</a>
<footer>&copy; Andreas Pedersen</footer>
<script>
function reset(){
  if(!confirm('Tilbakestill alle innstillinger til standardverdier?'))return;
  fetch('/factory-reset')
  .then(r=>r.ok?r.text():Promise.reject())
  .then(()=>{document.getElementById('msg').textContent='Tilbakestilt ✓ Starter på nytt...';})
  .catch(()=>{document.getElementById('msg').textContent='Feil ✗';});
}
function save(){
  const p={
    sfwd: document.getElementById('sfwd').value,
    sbwd: document.getElementById('sbwd').value,
    pfwd: document.getElementById('pfwd').value,
    pbwd: document.getElementById('pbwd').value,
    ptx:  document.getElementById('ptx').value,
    accel:document.getElementById('accel').value,
    m1inv:document.getElementById('m1inv').checked?1:0,
    m2inv:document.getElementById('m2inv').checked?1:0
  };
  fetch('/save-config?sfwd='+p.sfwd+'&sbwd='+p.sbwd
        +'&pfwd='+p.pfwd+'&pbwd='+p.pbwd+'&ptx='+p.ptx
        +'&accel='+p.accel+'&m1inv='+p.m1inv+'&m2inv='+p.m2inv)
  .then(r=>r.ok?r.text():Promise.reject())
  .then(()=>{document.getElementById('msg').textContent='Lagret ✓ Starter på nytt...';})
  .catch(()=>{document.getElementById('msg').textContent='Feil ✗';});
}
</script>
</body>
</html>)html";

// ── Debug-side ────────────────────────────────────────────────────────────────

static const char PAGE_DEBUG[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="no">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Toilltak Debug</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:monospace;background:#0d0d0d;color:#ccc;padding:16px;
       display:flex;flex-direction:column;height:100vh}
  h1{font-size:1.2rem;color:#6ecf6e;margin-bottom:10px;flex-shrink:0}
  #log{flex:1;background:#111;border:1px solid #2a2a2a;border-radius:8px;
       padding:10px;overflow-y:auto;font-size:.78rem;line-height:1.7;min-height:0}
  .line{white-space:pre-wrap;padding:1px 0}
  .BTN   {color:#6ecf6e}
  .WEB   {color:#6ab0e0}
  .SAFETY{color:#e07050}
  .STATUS{color:#666}
  .NVS   {color:#b070e0}
  .CFG   {color:#b070e0}
  .SYS   {color:#e0c060}
  .SABER {color:#e0a030}
  .GPIO  {color:#88bbcc}
  .WiFi  {color:#88bbcc}
  .mDNS  {color:#88bbcc}
  .DNS   {color:#88bbcc}
  .ts    {color:#444;margin-right:6px}
  .bar{display:flex;gap:8px;margin-top:8px;flex-shrink:0;flex-wrap:wrap;align-items:center}
  button{padding:7px 14px;background:#1e1e1e;border:1px solid #333;color:#aaa;
         border-radius:6px;cursor:pointer;font-size:.8rem}
  button:hover{background:#2a2a2a}
  button.on{border-color:#6ecf6e;color:#6ecf6e}
  #status{font-size:.75rem;color:#555;margin-left:auto}
  .back{color:#444;text-decoration:none;font-size:.8rem}
  .back:hover{color:#777}
</style>
</head>
<body>
<h1>Toilltak Debug</h1>
<div id="log"></div>
<div class="bar">
  <button onclick="clearLog()">Tøm</button>
  <button id="pauseBtn" onclick="togglePause()">Pause</button>
  <button id="scrollBtn" class="on" onclick="toggleScroll()">Auto-scroll</button>
  <span id="status">Kobler til...</span>
  <a class="back" href="/">&#8592; Tilbake</a>
</div>
<script>
var paused=false,autoScroll=true,lineCount=0;
var log=document.getElementById('log');
var statusEl=document.getElementById('status');
function clearLog(){log.innerHTML='';lineCount=0;}
function togglePause(){
  paused=!paused;
  document.getElementById('pauseBtn').textContent=paused?'Fortsett':'Pause';
  document.getElementById('pauseBtn').className=paused?'on':'';
}
function toggleScroll(){
  autoScroll=!autoScroll;
  document.getElementById('scrollBtn').className=autoScroll?'on':'';
}
function addLine(text,id){
  if(paused)return;
  var tag=(text.match(/\[(\w+)\]/)||[])[1]||'';
  var t=(id/1000).toFixed(1);
  var d=document.createElement('div');
  d.className='line '+tag;
  d.innerHTML='<span class="ts">'+t+'s</span>'+escHtml(text);
  log.appendChild(d);
  lineCount++;
  if(lineCount>800)log.removeChild(log.firstChild);
  if(autoScroll)log.scrollTop=log.scrollHeight;
}
function escHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;');}
var es=new EventSource('/events');
es.addEventListener('log',function(e){
  addLine(e.data,parseInt(e.lastEventId)||0);
  statusEl.textContent='Tilkoblet';
});
es.onopen=function(){statusEl.textContent='Tilkoblet';};
es.onerror=function(){
  statusEl.textContent='Frakoblet — prøver igjen...';
  addLine('--- tilkobling mistet ---',Date.now());
};
</script>
</body>
</html>)html";

// ── HTML builders ─────────────────────────────────────────────────────────────

static String buildMainPage() {
    String html = PAGE_MAIN;
    html.replace("%FWD%", String(fwdSpeed));
    html.replace("%BWD%", String(bwdSpeed));
    return html;
}

static String buildSettingsPage() {
    String html = PAGE_SETTINGS;
    html.replace("%FWD%",     String(fwdSpeed));
    html.replace("%BWD%",     String(bwdSpeed));
    html.replace("%ACCEL%",   String(accelLevel));
    html.replace("%PIN_FWD%", String(pinBtnFwd));
    html.replace("%PIN_BWD%", String(pinBtnBwd));
    html.replace("%PIN_TX%",  String(pinSaberTx));
    html.replace("%M1INV%",   motor1Inverted ? "checked" : "");
    html.replace("%M2INV%",   motor2Inverted ? "checked" : "");
    return html;
}
