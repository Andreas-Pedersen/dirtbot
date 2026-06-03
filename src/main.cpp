#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ── Pinout ───────────────────────────────────────────────────────────────────
#define PIN_BTN_FWD   32    // Fremover-knapp, aktiv lav (intern pull-up)
#define PIN_BTN_BWD   33    // Bakover-knapp, aktiv lav (intern pull-up)
#define PIN_SABER_TX  17    // Serial2 TX → Sabertooth S1 (+ felles GND)

// ── Sabertooth Simplified Serial ─────────────────────────────────────────────
// DIP-switch: SW1=ON, SW2-6=OFF  →  9600 baud, simplified serial
// Motor 1:  1=full bak, 64=stopp, 127=full frem
// Motor 2: 128=full bak, 192=stopp, 255=full frem
#define SABER_BAUD      9600
#define MOTOR2_INVERTED true   // true hvis motorene sitter speilvend på akselen

// ── Akselerasjon ─────────────────────────────────────────────────────────────
#define MOTOR_TICK_MS   20     // Kontrollsløyfe-periode (ms)
#define ACCEL_RATE      1.5f   // % per tick ved akselerasjon  (~1.3 sek til full fart)
#define DECEL_RATE      3.0f   // % per tick ved bremsing      (~0.7 sek til stopp)

// ── Nettverk ─────────────────────────────────────────────────────────────────
static const char*      AP_SSID  = "torvtak";
static const IPAddress  AP_IP    (192, 168, 4, 1);
static const IPAddress  AP_SUBNET(255, 255, 255, 0);

// ── Globals ───────────────────────────────────────────────────────────────────
DNSServer       dns;
AsyncWebServer  server(80);
Preferences     prefs;

int   fwdSpeed    = 70;    // Maks fremover-hastighet, prosent
int   bwdSpeed    = 70;    // Maks bakover-hastighet, prosent
float curSpeed    = 0.0f;  // Nåværende output: -100..+100

unsigned long lastTick = 0;

// ── Sabertooth helpers ────────────────────────────────────────────────────────

static void saberSend(float speedPct) {
    // speedPct: -100 (full bak) .. 0 (stopp) .. +100 (full frem)
    speedPct = constrain(speedPct, -100.0f, 100.0f);
    int delta = (int)roundf(speedPct * 63.0f / 100.0f);

    uint8_t m1 = (uint8_t)constrain(64 + delta, 1, 127);
    uint8_t m2;
    if (MOTOR2_INVERTED)
        m2 = (uint8_t)constrain(192 - delta, 128, 255);
    else
        m2 = (uint8_t)constrain(192 + delta, 128, 255);

    Serial2.write(m1);
    Serial2.write(m2);
}

// Stopp med Sabertooth-byte 0 (nødstopp)
static void saberStop() {
    Serial2.write((uint8_t)0);
}

// ── NVS ──────────────────────────────────────────────────────────────────────

static void loadSettings() {
    prefs.begin("dirtbot", true);
    fwdSpeed = prefs.getInt("fwd", 70);
    bwdSpeed = prefs.getInt("bwd", 70);
    prefs.end();
}

static void saveSettings() {
    prefs.begin("dirtbot", false);
    prefs.putInt("fwd", fwdSpeed);
    prefs.putInt("bwd", bwdSpeed);
    prefs.end();
}

// ── HTML ──────────────────────────────────────────────────────────────────────

static const char PAGE[] PROGMEM = R"html(<!DOCTYPE html>
<html lang="no">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Torvtak Dirtbot</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:system-ui,sans-serif;background:#111;color:#eee;
       display:flex;flex-direction:column;align-items:center;padding:32px 20px}
  h1{font-size:1.8rem;color:#6ecf6e;margin-bottom:8px}
  p.sub{color:#888;font-size:.9rem;margin-bottom:36px}
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
</style>
</head>
<body>
<h1>Torvtak Dirtbot</h1>
<p class="sub">Innstillinger lagres ved reboot</p>
<div class="card">
  <div class="row">
    <label>Fremover</label>
    <span class="pct" id="fv">%FWD%&thinsp;%</span>
  </div>
  <input type="range" id="fs" min="10" max="100" value="%FWD%"
         oninput="document.getElementById('fv').textContent=this.value+' %'">

  <div class="divider"></div>

  <div class="row">
    <label>Bakover</label>
    <span class="pct" id="bv">%BWD%&thinsp;%</span>
  </div>
  <input type="range" id="bs" min="10" max="100" value="%BWD%"
         oninput="document.getElementById('bv').textContent=this.value+' %'">

  <button onclick="save()">Lagre</button>
  <div id="msg"></div>
</div>
<script>
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

static String buildPage() {
    String html = PAGE;
    html.replace("%FWD%", String(fwdSpeed));
    html.replace("%BWD%", String(bwdSpeed));
    return html;
}

// ── Captive portal: liste over kjente probe-URLer ─────────────────────────────

static bool isCaptiveProbe(const String& url) {
    return url == "/generate_204"          // Android
        || url == "/gen_204"
        || url == "/hotspot-detect.html"   // iOS / macOS
        || url == "/library/test/success.html"
        || url == "/success.html"
        || url == "/ncsi.txt"              // Windows
        || url == "/connecttest.txt"
        || url == "/redirect";
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial2.begin(SABER_BAUD, SERIAL_8N1, -1, PIN_SABER_TX);

    pinMode(PIN_BTN_FWD, INPUT_PULLUP);
    pinMode(PIN_BTN_BWD, INPUT_PULLUP);

    loadSettings();

    // Accesspoint
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
    WiFi.softAP(AP_SSID);   // ingen passord = åpent nettverk
    Serial.printf("AP: %s  IP: %s\n", AP_SSID, AP_IP.toString().c_str());

    // DNS – svarer på alt med AP_IP (captive portal)
    dns.start(53, "*", AP_IP);

    // mDNS – torvtak.local
    if (MDNS.begin("torvtak"))
        MDNS.addService("http", "tcp", 80);

    // Web
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html", buildPage());
    });

    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (req->hasParam("fwd")) fwdSpeed = req->getParam("fwd")->value().toInt();
        if (req->hasParam("bwd")) bwdSpeed = req->getParam("bwd")->value().toInt();
        fwdSpeed = constrain(fwdSpeed, 10, 100);
        bwdSpeed = constrain(bwdSpeed, 10, 100);
        saveSettings();
        req->send(200, "text/plain", "OK");
    });

    // Fang alle ukjente URLer – send til forsiden
    server.onNotFound([](AsyncWebServerRequest *req) {
        if (isCaptiveProbe(req->url()))
            req->redirect("http://" + AP_IP.toString() + "/");
        else
            req->redirect("/");
    });

    server.begin();
    Serial.println("Klar.");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
    dns.processNextRequest();

    unsigned long now = millis();
    if (now - lastTick < MOTOR_TICK_MS) return;
    lastTick = now;

    bool fwdBtn = !digitalRead(PIN_BTN_FWD);
    bool bwdBtn = !digitalRead(PIN_BTN_BWD);

    float target;
    if (fwdBtn && !bwdBtn)       target = (float)fwdSpeed;
    else if (bwdBtn && !fwdBtn)  target = -(float)bwdSpeed;
    else                          target = 0.0f;

    // Gradvis akselerasjon
    float rate = (target == 0.0f) ? DECEL_RATE : ACCEL_RATE;
    if (curSpeed < target)
        curSpeed = fminf(curSpeed + rate, target);
    else if (curSpeed > target)
        curSpeed = fmaxf(curSpeed - rate, target);

    saberSend(curSpeed);
}
