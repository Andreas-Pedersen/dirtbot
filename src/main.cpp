#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ── Compile-time defaults (overstyres av NVS) ─────────────────────────────────
#define DEFAULT_PIN_BTN_FWD   32
#define DEFAULT_PIN_BTN_BWD   33
#define DEFAULT_PIN_SABER_TX  17
#define DEFAULT_M1_INVERTED   false
#define DEFAULT_M2_INVERTED   true

#define SABER_BAUD     9600
#define MOTOR_TICK_MS  20
#define ACCEL_RATE     1.5f
#define DECEL_RATE     3.0f
#define DEBOUNCE_MS    20

// ── Nettverk ─────────────────────────────────────────────────────────────────
static const char*      AP_SSID   = "Toilltak";
static const char*      MDNS_NAME = "toilltak";
static const IPAddress  AP_IP    (192, 168, 4, 1);
static const IPAddress  AP_SUBNET(255, 255, 255, 0);

// ── Runtime-innstillinger ─────────────────────────────────────────────────────
int  pinBtnFwd     = DEFAULT_PIN_BTN_FWD;
int  pinBtnBwd     = DEFAULT_PIN_BTN_BWD;
int  pinSaberTx    = DEFAULT_PIN_SABER_TX;
bool motor1Inverted = DEFAULT_M1_INVERTED;
bool motor2Inverted = DEFAULT_M2_INVERTED;

int   fwdSpeed = 70;
int   bwdSpeed = 70;
float curSpeed = 0.0f;

// ── Globals ───────────────────────────────────────────────────────────────────
DNSServer      dns;
AsyncWebServer server(80);
Preferences    prefs;

unsigned long lastTick    = 0;
unsigned long lastDebugMs = 0;

// Debounce state
bool          fwdRaw = false, fwdState = false;
bool          bwdRaw = false, bwdState = false;
unsigned long fwdDebounceAt = 0, bwdDebounceAt = 0;
bool          lastFwd = false, lastBwd = false;

bool          pendingRestart = false;
unsigned long restartAt      = 0;

// Web-kommando (hold-knapper i GUI)
bool          webFwd        = false;
bool          webBwd        = false;
unsigned long lastWebCmdMs  = 0;
#define WEB_CMD_TIMEOUT_MS  500

// ── NVS ──────────────────────────────────────────────────────────────────────

static void loadSettings() {
    prefs.begin("dirtbot", true);
    fwdSpeed       = prefs.getInt( "fwd",    70);
    bwdSpeed       = prefs.getInt( "bwd",    70);
    pinBtnFwd      = prefs.getInt( "pFwd",   DEFAULT_PIN_BTN_FWD);
    pinBtnBwd      = prefs.getInt( "pBwd",   DEFAULT_PIN_BTN_BWD);
    pinSaberTx     = prefs.getInt( "pTx",    DEFAULT_PIN_SABER_TX);
    motor1Inverted = prefs.getBool("m1inv",  DEFAULT_M1_INVERTED);
    motor2Inverted = prefs.getBool("m2inv",  DEFAULT_M2_INVERTED);
    prefs.end();
    Serial.printf("[NVS] fwd=%d%%  bwd=%d%%  pFwd=%d  pBwd=%d  pTx=%d  m1inv=%d  m2inv=%d\n",
        fwdSpeed, bwdSpeed, pinBtnFwd, pinBtnBwd, pinSaberTx, motor1Inverted, motor2Inverted);
}

static void saveSpeedSettings() {
    prefs.begin("dirtbot", false);
    prefs.putInt("fwd", fwdSpeed);
    prefs.putInt("bwd", bwdSpeed);
    prefs.end();
    Serial.printf("[NVS] Lagret: fwd=%d%%  bwd=%d%%\n", fwdSpeed, bwdSpeed);
}

static void factoryReset() {
    prefs.begin("dirtbot", false);
    prefs.clear();
    prefs.end();
    Serial.println("[NVS] Factory reset — alle verdier slettet");
}

static void saveAllSettings() {
    prefs.begin("dirtbot", false);
    prefs.putInt( "fwd",   fwdSpeed);
    prefs.putInt( "bwd",   bwdSpeed);
    prefs.putInt( "pFwd",  pinBtnFwd);
    prefs.putInt( "pBwd",  pinBtnBwd);
    prefs.putInt( "pTx",   pinSaberTx);
    prefs.putBool("m1inv", motor1Inverted);
    prefs.putBool("m2inv", motor2Inverted);
    prefs.end();
    Serial.printf("[NVS] Lagret alle innstillinger\n");
}

// ── Sabertooth helpers ────────────────────────────────────────────────────────

static void saberSend(float speedPct) {
    speedPct = constrain(speedPct, -100.0f, 100.0f);
    int delta = (int)roundf(speedPct * 63.0f / 100.0f);

    uint8_t m1 = motor1Inverted
        ? (uint8_t)constrain(64 - delta, 1, 127)
        : (uint8_t)constrain(64 + delta, 1, 127);

    uint8_t m2 = motor2Inverted
        ? (uint8_t)constrain(192 - delta, 128, 255)
        : (uint8_t)constrain(192 + delta, 128, 255);

    Serial2.write(m1);
    Serial2.write(m2);
}

static void saberStop() {
    Serial2.write((uint8_t)0);
}

// ── HTML: hovedside ───────────────────────────────────────────────────────────

static const char PAGE_MAIN[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="no">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Toilltak Dirtbot</title>
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
<h1>Toilltak Dirtbot</h1>
<div class="card">
  <div class="row">
    <label>Fart fremover</label>
    <span class="pct" id="fv">%FWD%&thinsp;%</span>
  </div>
  <input type="range" id="fs" min="10" max="100" value="%FWD%"
         oninput="document.getElementById('fv').textContent=this.value+' %'">

  <div class="divider"></div>

  <div class="row">
    <label>Fart bakover</label>
    <span class="pct" id="bv">%BWD%&thinsp;%</span>
  </div>
  <input type="range" id="bs" min="10" max="100" value="%BWD%"
         oninput="document.getElementById('bv').textContent=this.value+' %'">

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
var driveInterval=null;
function startDrive(dir){
  document.getElementById(dir==='fwd'?'bfwd':'bbwd').classList.add('active');
  fetch('/cmd?dir='+dir);
  driveInterval=setInterval(function(){fetch('/cmd?dir='+dir);},200);
}
function stopDrive(){
  clearInterval(driveInterval);driveInterval=null;
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

// ── HTML: innstillingsside ────────────────────────────────────────────────────

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
  .divider{height:1px;background:#333;margin:20px 0}
  button{width:100%;padding:16px;margin-top:8px;background:#e07050;color:#fff;
         border:none;border-radius:10px;font-size:1rem;font-weight:700;cursor:pointer;
         transition:background .15s}
  button:active{background:#c05030}
  .btn-reset{background:#333;color:#888;margin-top:32px}
  .btn-reset:active{background:#444}
  #msg{text-align:center;margin-top:14px;font-size:.9rem;color:#e07050}
  .back{margin-top:20px;color:#555;font-size:.85rem;text-decoration:none}
  .back:hover{color:#888}
  footer{margin-top:28px;color:#444;font-size:.8rem;text-align:center}
</style>
</head>
<body>
<h1>Toilltak Dirtbot</h1>

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

<button class="btn-reset" onclick="reset()">&#9888; Tilbakestill til fabrikk (50 %)</button>
<a class="back" href="/">&#8592; Tilbake</a>
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
    pfwd:document.getElementById('pfwd').value,
    pbwd:document.getElementById('pbwd').value,
    ptx: document.getElementById('ptx').value,
    m1inv:document.getElementById('m1inv').checked?1:0,
    m2inv:document.getElementById('m2inv').checked?1:0
  };
  fetch('/save-config?pfwd='+p.pfwd+'&pbwd='+p.pbwd+'&ptx='+p.ptx
        +'&m1inv='+p.m1inv+'&m2inv='+p.m2inv)
  .then(r=>r.ok?r.text():Promise.reject())
  .then(()=>{
    document.getElementById('msg').textContent='Lagret ✓ Starter på nytt...';
  })
  .catch(()=>{
    document.getElementById('msg').textContent='Feil ✗';
  });
}
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
    html.replace("%PIN_FWD%", String(pinBtnFwd));
    html.replace("%PIN_BWD%", String(pinBtnBwd));
    html.replace("%PIN_TX%",  String(pinSaberTx));
    html.replace("%M1INV%",   motor1Inverted ? "checked" : "");
    html.replace("%M2INV%",   motor2Inverted ? "checked" : "");
    return html;
}

// ── Captive portal helpers ────────────────────────────────────────────────────

static void registerCaptiveRoutes() {
    auto send204 = [](AsyncWebServerRequest *req) {
        Serial.printf("[WEB] Captive probe (204): %s\n", req->url().c_str());
        req->send(204);
    };
    server.on("/generate_204", HTTP_GET, send204);
    server.on("/gen_204",      HTTP_GET, send204);

    auto sendSuccess = [](AsyncWebServerRequest *req) {
        Serial.printf("[WEB] Captive probe (iOS): %s\n", req->url().c_str());
        req->send(200, "text/html",
            "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    };
    server.on("/hotspot-detect.html",       HTTP_GET, sendSuccess);
    server.on("/library/test/success.html", HTTP_GET, sendSuccess);
    server.on("/success.html",              HTTP_GET, sendSuccess);

    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *req) {
        Serial.println("[WEB] Captive probe (Win/ncsi)");
        req->send(200, "text/plain", "Microsoft NCSI");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *req) {
        Serial.println("[WEB] Captive probe (Win/connecttest)");
        req->send(200, "text/plain", "Microsoft Connect Test");
    });
    server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *req) {
        Serial.println("[WEB] Captive probe (redirect)");
        req->redirect("http://" + AP_IP.toString() + "/");
    });
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n=== Toilltak Dirtbot boot ===");

    loadSettings();

    Serial2.begin(SABER_BAUD, SERIAL_8N1, -1, pinSaberTx);
    Serial.printf("[SABER] Serial2 TX på GPIO%d, %d baud\n", pinSaberTx, SABER_BAUD);

    pinMode(pinBtnFwd, INPUT_PULLUP);
    pinMode(pinBtnBwd, INPUT_PULLUP);
    Serial.printf("[GPIO] FWD=GPIO%d  BWD=GPIO%d\n", pinBtnFwd, pinBtnBwd);
    Serial.printf("[MOTOR] m1inv=%d  m2inv=%d\n", motor1Inverted, motor2Inverted);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
    WiFi.softAP(AP_SSID);
    Serial.printf("[WiFi] AP \"%s\" oppe, IP: %s\n", AP_SSID, AP_IP.toString().c_str());

    dns.start(53, "*", AP_IP);
    Serial.println("[DNS] Captive portal DNS startet");

    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[mDNS] http://%s.local\n", MDNS_NAME);
    } else {
        Serial.println("[mDNS] FEIL: kunne ikke starte");
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        Serial.printf("[WEB] GET /  fra %s\n", req->client()->remoteIP().toString().c_str());
        req->send(200, "text/html", buildMainPage());
    });

    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (req->hasParam("fwd")) fwdSpeed = req->getParam("fwd")->value().toInt();
        if (req->hasParam("bwd")) bwdSpeed = req->getParam("bwd")->value().toInt();
        fwdSpeed = constrain(fwdSpeed, 10, 100);
        bwdSpeed = constrain(bwdSpeed, 10, 100);
        saveSpeedSettings();
        req->send(200, "text/plain", "OK");
    });

    server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!req->hasParam("dir")) { req->send(400); return; }
        String dir = req->getParam("dir")->value();
        if (dir == "fwd")       { webFwd = true;  webBwd = false; }
        else if (dir == "bwd")  { webFwd = false; webBwd = true;  }
        else                    { webFwd = false; webBwd = false;  }
        lastWebCmdMs = millis();
        req->send(200, "text/plain", "OK");
    });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *req) {
        Serial.printf("[WEB] GET /settings  fra %s\n", req->client()->remoteIP().toString().c_str());
        req->send(200, "text/html", buildSettingsPage());
    });

    server.on("/save-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (req->hasParam("pfwd"))  pinBtnFwd      = constrain(req->getParam("pfwd")->value().toInt(),  0, 39);
        if (req->hasParam("pbwd"))  pinBtnBwd      = constrain(req->getParam("pbwd")->value().toInt(),  0, 39);
        if (req->hasParam("ptx"))   pinSaberTx     = constrain(req->getParam("ptx")->value().toInt(),   0, 39);
        motor1Inverted = req->hasParam("m1inv") && req->getParam("m1inv")->value() == "1";
        motor2Inverted = req->hasParam("m2inv") && req->getParam("m2inv")->value() == "1";
        saveAllSettings();
        Serial.printf("[CFG] Ny config: pFwd=%d pBwd=%d pTx=%d m1inv=%d m2inv=%d\n",
            pinBtnFwd, pinBtnBwd, pinSaberTx, motor1Inverted, motor2Inverted);
        req->send(200, "text/plain", "OK");
        pendingRestart = true;
        restartAt = millis() + 1000;
    });

    server.on("/factory-reset", HTTP_GET, [](AsyncWebServerRequest *req) {
        factoryReset();
        req->send(200, "text/plain", "OK");
        pendingRestart = true;
        restartAt = millis() + 1000;
    });

    registerCaptiveRoutes();

    server.onNotFound([](AsyncWebServerRequest *req) {
        Serial.printf("[WEB] 404 redirect: %s\n", req->url().c_str());
        req->redirect("http://" + AP_IP.toString() + "/");
    });

    server.begin();
    Serial.println("[WEB] HTTP-server startet på port 80");
    Serial.println("=== Klar ===\n");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
    if (pendingRestart && millis() >= restartAt) {
        Serial.println("[SYS] Starter på nytt...");
        ESP.restart();
    }

    dns.processNextRequest();

    unsigned long now = millis();
    if (now - lastTick < MOTOR_TICK_MS) return;
    lastTick = now;

    // Debounce fremover-knapp
    bool rawF = !digitalRead(pinBtnFwd);
    if (rawF != fwdRaw) { fwdRaw = rawF; fwdDebounceAt = now; }
    if (now - fwdDebounceAt >= DEBOUNCE_MS) fwdState = fwdRaw;

    // Debounce bakover-knapp
    bool rawB = !digitalRead(pinBtnBwd);
    if (rawB != bwdRaw) { bwdRaw = rawB; bwdDebounceAt = now; }
    if (now - bwdDebounceAt >= DEBOUNCE_MS) bwdState = bwdRaw;

    // Safety-timeout for web-kommandoer
    if (webFwd || webBwd) {
        if (now - lastWebCmdMs > WEB_CMD_TIMEOUT_MS) {
            webFwd = webBwd = false;
            Serial.println("[WEB] Kommando-timeout — stopper");
        }
    }

    bool activeFwd = fwdState || webFwd;
    bool activeBwd = bwdState || webBwd;

    if (activeFwd != lastFwd || activeBwd != lastBwd) {
        Serial.printf("[BTN] FWD=%d BWD=%d  web=%d/%d  speed=%.1f%%\n",
            fwdState, bwdState, webFwd, webBwd, curSpeed);
        lastFwd = activeFwd;
        lastBwd = activeBwd;
    }

    float target;
    if (activeFwd && !activeBwd)      target =  (float)fwdSpeed;
    else if (activeBwd && !activeFwd) target = -(float)bwdSpeed;
    else                         target =  0.0f;

    float rate = (target == 0.0f) ? DECEL_RATE : ACCEL_RATE;
    if (curSpeed < target)
        curSpeed = fminf(curSpeed + rate, target);
    else if (curSpeed > target)
        curSpeed = fmaxf(curSpeed - rate, target);

    saberSend(curSpeed);

    if (now - lastDebugMs >= 5000) {
        lastDebugMs = now;
        Serial.printf("[STATUS] speed=%.1f%%  fwd=%d  bwd=%d  klienter=%d\n",
            curSpeed, fwdState, bwdState, WiFi.softAPgetStationNum());
    }
}
