#pragma once
#include <Preferences.h>
#include "config.h"
#include "logger.h"

extern Preferences prefs;
extern int   fwdSpeed, bwdSpeed, accelLevel;
extern int   pinBtnFwd, pinBtnBwd, pinSaberTx;
extern bool  motor1Inverted, motor2Inverted;

static void loadSettings() {
    prefs.begin("dirtbot", true);
    fwdSpeed       = prefs.getInt( "fwd",   70);
    bwdSpeed       = prefs.getInt( "bwd",   70);
    accelLevel     = prefs.getInt( "accel",  3);
    pinBtnFwd      = prefs.getInt( "pFwd",  DEFAULT_PIN_BTN_FWD);
    pinBtnBwd      = prefs.getInt( "pBwd",  DEFAULT_PIN_BTN_BWD);
    pinSaberTx     = prefs.getInt( "pTx",   DEFAULT_PIN_SABER_TX);
    motor1Inverted = prefs.getBool("m1inv", DEFAULT_M1_INVERTED);
    motor2Inverted = prefs.getBool("m2inv", DEFAULT_M2_INVERTED);
    prefs.end();
    logf("[NVS] fwd=%d%%  bwd=%d%%  accel=%d  pFwd=%d  pBwd=%d  pTx=%d  m1inv=%d  m2inv=%d",
        fwdSpeed, bwdSpeed, accelLevel, pinBtnFwd, pinBtnBwd, pinSaberTx, motor1Inverted, motor2Inverted);
}

static void saveSpeedSettings() {
    prefs.begin("dirtbot", false);
    prefs.putInt("fwd", fwdSpeed);
    prefs.putInt("bwd", bwdSpeed);
    prefs.end();
    logf("[NVS] Lagret: fwd=%d%%  bwd=%d%%", fwdSpeed, bwdSpeed);
}

static void saveAllSettings() {
    prefs.begin("dirtbot", false);
    prefs.putInt( "fwd",   fwdSpeed);
    prefs.putInt( "bwd",   bwdSpeed);
    prefs.putInt( "accel", accelLevel);
    prefs.putInt( "pFwd",  pinBtnFwd);
    prefs.putInt( "pBwd",  pinBtnBwd);
    prefs.putInt( "pTx",   pinSaberTx);
    prefs.putBool("m1inv", motor1Inverted);
    prefs.putBool("m2inv", motor2Inverted);
    prefs.end();
    logf("[NVS] Lagret alle innstillinger");
}

static void factoryReset() {
    prefs.begin("dirtbot", false);
    prefs.clear();
    prefs.end();
    logf("[NVS] Factory reset — alle verdier slettet");
}
