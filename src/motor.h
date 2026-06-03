#pragma once
#include <Arduino.h>

extern bool motor1Inverted;
extern bool motor2Inverted;

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
