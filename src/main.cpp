#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

#include "config.h"
#include "logger.h"
#include "motor.h"
#include "settings.h"
#include "pages.h"

// ── Globals ───────────────────────────────────────────────────────────────────

DNSServer        dns;
AsyncWebServer   server(80);
AsyncEventSource events("/events");
Preferences      prefs;

int  pinBtnFwd      = DEFAULT_PIN_BTN_FWD;
int  pinBtnBwd      = DEFAULT_PIN_BTN_BWD;
int  pinSaberTx     = DEFAULT_PIN_SABER_TX;
bool motor1Inverted = DEFAULT_M1_INVERTED;
bool motor2Inverted = DEFAULT_M2_INVERTED;

int   fwdSpeed   = 70;
int   bwdSpeed   = 70;
int   accelLevel = 3;
float curSpeed   = 0.0f;

unsigned long lastTick    = 0;
unsigned long lastDebugMs = 0;

// Debounce
bool          fwdRaw = false, fwdState = false;
bool          bwdRaw = false, bwdState = false;
unsigned long fwdDebounceAt = 0, bwdDebounceAt = 0;
bool          lastFwd = false, lastBwd = false;

// Web-kjøring
bool          webFwd          = false;
bool          webBwd          = false;
unsigned long lastWebCmdMs    = 0;
unsigned long webDriveStartMs = 0;

// Startup-interlock
bool          startupReady      = false;
unsigned long bothReleasedSince = 0;

// Restart-flagg
bool          pendingRestart = false;
unsigned long restartAt      = 0;

// ── Captive portal ────────────────────────────────────────────────────────────

static void registerCaptiveRoutes() {
    auto send204 = [](AsyncWebServerRequest *req) { req->send(204); };
    server.on("/generate_204", HTTP_GET, send204);
    server.on("/gen_204",      HTTP_GET, send204);

    auto sendSuccess = [](AsyncWebServerRequest *req) {
        req->send(200, "text/html",
            "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    };
    server.on("/hotspot-detect.html",       HTTP_GET, sendSuccess);
    server.on("/library/test/success.html", HTTP_GET, sendSuccess);
    server.on("/success.html",              HTTP_GET, sendSuccess);

    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/plain", "Microsoft NCSI");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/plain", "Microsoft Connect Test");
    });
    server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->redirect("http://" + AP_IP.toString() + "/");
    });
}

// ── Web-ruter ─────────────────────────────────────────────────────────────────

static void registerRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html", buildMainPage());
    });

    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (req->hasParam("fwd")) fwdSpeed = constrain(req->getParam("fwd")->value().toInt(), 0, 100);
        if (req->hasParam("bwd")) bwdSpeed = constrain(req->getParam("bwd")->value().toInt(), 0, 100);
        saveSpeedSettings();
        req->send(200, "text/plain", "OK");
    });

    server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (!startupReady) { req->send(200, "text/plain", "LOCKED"); return; }
        if (!req->hasParam("dir")) { req->send(400); return; }
        String dir = req->getParam("dir")->value();
        bool wasIdle = !webFwd && !webBwd;
        if      (dir == "fwd") { webFwd = true;  webBwd = false; }
        else if (dir == "bwd") { webFwd = false; webBwd = true;  }
        else                   { webFwd = false; webBwd = false; webDriveStartMs = 0; }
        if ((webFwd || webBwd) && wasIdle) webDriveStartMs = millis();
        lastWebCmdMs = millis();
        req->send(200, "text/plain", "OK");
    });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html", buildSettingsPage());
    });

    server.on("/save-config", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (req->hasParam("sfwd"))  fwdSpeed       = constrain(req->getParam("sfwd")->value().toInt(),  0, 100);
        if (req->hasParam("sbwd"))  bwdSpeed       = constrain(req->getParam("sbwd")->value().toInt(),  0, 100);
        if (req->hasParam("accel")) accelLevel     = constrain(req->getParam("accel")->value().toInt(), 1, 10);
        if (req->hasParam("pfwd"))  pinBtnFwd      = constrain(req->getParam("pfwd")->value().toInt(),  0, 39);
        if (req->hasParam("pbwd"))  pinBtnBwd      = constrain(req->getParam("pbwd")->value().toInt(),  0, 39);
        if (req->hasParam("ptx"))   pinSaberTx     = constrain(req->getParam("ptx")->value().toInt(),   0, 39);
        motor1Inverted = req->hasParam("m1inv") && req->getParam("m1inv")->value() == "1";
        motor2Inverted = req->hasParam("m2inv") && req->getParam("m2inv")->value() == "1";
        saveAllSettings();
        logf("[CFG] fwd=%d%% bwd=%d%% accel=%d pFwd=%d pBwd=%d pTx=%d m1inv=%d m2inv=%d",
            fwdSpeed, bwdSpeed, accelLevel, pinBtnFwd, pinBtnBwd, pinSaberTx, motor1Inverted, motor2Inverted);
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

    server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html", PAGE_DEBUG);
    });

    registerCaptiveRoutes();

    events.onConnect([](AsyncEventSourceClient *client) {
        logf("[DBG] Debug-klient tilkoblet");
    });
    server.addHandler(&events);

    server.onNotFound([](AsyncWebServerRequest *req) {
        req->redirect("http://" + AP_IP.toString() + "/");
    });
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== Toilltak Dirtbot boot ===");

    loadSettings();

    Serial2.begin(SABER_BAUD, SERIAL_8N1, -1, pinSaberTx);
    delay(100);
    saberStop();

    pinMode(pinBtnFwd, INPUT_PULLUP);
    pinMode(pinBtnBwd, INPUT_PULLUP);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_SUBNET);
    WiFi.softAP(AP_SSID);

    dns.start(53, "*", AP_IP);

    if (MDNS.begin(MDNS_NAME))
        MDNS.addService("http", "tcp", 80);

    registerRoutes();
    server.begin();
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
    if (pendingRestart && millis() >= restartAt) {
        logf("[SYS] Starter på nytt...");
        delay(100);
        ESP.restart();
    }

    dns.processNextRequest();

    unsigned long now = millis();
    if (now - lastTick < MOTOR_TICK_MS) return;
    lastTick = now;

    // Debounce knapper
    bool rawF = !digitalRead(pinBtnFwd);
    if (rawF != fwdRaw) { fwdRaw = rawF; fwdDebounceAt = now; }
    if (now - fwdDebounceAt >= DEBOUNCE_MS) fwdState = fwdRaw;

    bool rawB = !digitalRead(pinBtnBwd);
    if (rawB != bwdRaw) { bwdRaw = rawB; bwdDebounceAt = now; }
    if (now - bwdDebounceAt >= DEBOUNCE_MS) bwdState = bwdRaw;

    // Startup-interlock (kun fysiske knapper teller)
    if (!startupReady) {
        if (!fwdState && !bwdState) {
            if (bothReleasedSince == 0) bothReleasedSince = now;
            if (now - bothReleasedSince >= STARTUP_HOLD_MS) {
                startupReady = true;
                logf("[SAFETY] Klar til kjøring");
            } else if ((now - bothReleasedSince) % 5000 < (unsigned long)MOTOR_TICK_MS) {
                logf("[SAFETY] Venter... %lu sek gjenstår",
                    (STARTUP_HOLD_MS - (now - bothReleasedSince)) / 1000);
            }
        } else {
            bothReleasedSince = 0;
        }
        saberSend(0.0f);
        return;
    }

    // Web-kommando safety
    if (webFwd || webBwd) {
        if (now - lastWebCmdMs > WEB_CMD_TIMEOUT_MS) {
            webFwd = webBwd = false; webDriveStartMs = 0;
            logf("[WEB] Timeout — stopper");
        } else if (now - webDriveStartMs > WEB_DRIVE_MAX_MS) {
            webFwd = webBwd = false; webDriveStartMs = 0;
            logf("[WEB] Maks kjøretid nådd — stopper");
        }
    }

    bool activeFwd = fwdState || webFwd;
    bool activeBwd = bwdState || webBwd;

    if (activeFwd != lastFwd || activeBwd != lastBwd) {
        logf("[BTN] FWD=%d BWD=%d  web=%d/%d  speed=%.1f%%",
            fwdState, bwdState, webFwd, webBwd, curSpeed);
        lastFwd = activeFwd;
        lastBwd = activeBwd;
    }

    float target;
    if      (activeFwd && !activeBwd) target =  (float)fwdSpeed;
    else if (activeBwd && !activeFwd) target = -(float)bwdSpeed;
    else                              target =  0.0f;

    float accelRate = accelLevel * 0.3f;
    float rate = (target == 0.0f) ? accelRate * 2.0f : accelRate;
    if      (curSpeed < target) curSpeed = fminf(curSpeed + rate, target);
    else if (curSpeed > target) curSpeed = fmaxf(curSpeed - rate, target);

    saberSend(curSpeed);

    if (now - lastDebugMs >= 5000) {
        lastDebugMs = now;
        logf("[STATUS] speed=%.1f%%  fwd=%d  bwd=%d  klienter=%d",
            curSpeed, fwdState, bwdState, WiFi.softAPgetStationNum());
    }
}
