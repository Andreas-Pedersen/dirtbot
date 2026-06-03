#pragma once

// ── Compile-time defaults (overstyres av NVS) ────────────────────────────────
#define DEFAULT_PIN_BTN_FWD   32
#define DEFAULT_PIN_BTN_BWD   33
#define DEFAULT_PIN_SABER_TX  17
#define DEFAULT_M1_INVERTED   false
#define DEFAULT_M2_INVERTED   true

// ── Timing ────────────────────────────────────────────────────────────────────
#define SABER_BAUD          9600
#define MOTOR_TICK_MS         20
#define DEBOUNCE_MS           20
#define WEB_CMD_TIMEOUT_MS  2000
#define WEB_DRIVE_MAX_MS    3000
#define STARTUP_HOLD_MS    30000

// ── Nettverk ─────────────────────────────────────────────────────────────────
#include <IPAddress.h>
static const char*      AP_SSID   = "Toilltak";
static const char*      MDNS_NAME = "toilltak";
static const IPAddress  AP_IP    (192, 168, 4, 1);
static const IPAddress  AP_SUBNET(255, 255, 255, 0);
