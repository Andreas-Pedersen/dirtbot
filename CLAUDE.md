# Dirtbot — ESP32 + Sabertooth 2x25

## Prosjektbeskrivelse

ESP32-basert styring for en dirtbot med to motorer på bakakselen og manuell styring på forhjulene.

- To fysiske knapper (fremover/bakover) kobles til GPIO på ESP32
- Knapp holdt nede = kjør, slipp = bremser gradvis til stopp
- Gradvis akselerasjon opp til innstilt maks-hastighet
- Webgrensesnitt for å stille inn maks-hastighet fremover/bakover separat
- Innstillinger lagres i NVS (overlever reboot)
- WiFi access point "torvtak", åpent (ingen passord)
- Captive portal + tilgang via torvtak.local (mDNS)

## Verktøy og rammeverk

- **PlatformIO** + **Arduino framework** i VS Code
- Plattformversjon festet til `espressif32@6.3.2` (Arduino core 2.0.14)
  - Nyere versjoner gir `pins_arduino.h: No such file or directory`
- Biblioteker: `mathieucarbou/ESPAsyncWebServer` + `mathieucarbou/AsyncTCP`
  - `me-no-dev`-variantene er ikke kompatible med nyere ESP32 Arduino core

## Pinout

| ESP32 GPIO | Til |
|------------|-----|
| GPIO 32 | Fremover-knapp (aktiv lav, intern pull-up) |
| GPIO 33 | Bakover-knapp (aktiv lav, intern pull-up) |
| GPIO 17 (Serial2 TX) | Sabertooth S1 |
| GND | Sabertooth 0V / GND (felles) |

Knapper kobles mellom GPIO og GND (ingen ekstern motstand nødvendig).

## Sabertooth 2x25 oppsett

Protokoll: **Simplified Serial**, 9600 baud

DIP-switch innstilling:
- SW1 = ON
- SW2–SW6 = OFF

Motor 2 er invertert i kode (`MOTOR2_INVERTED true`) fordi motorene sitter speilvend på akselen.

## Nettverkstilgang

- Koble til WiFi: **torvtak** (åpent)
- Åpne nettleser → captive portal dukker opp automatisk
- Eller naviger til **torvtak.local** (http://torvtak.local)
- ESP32 IP: 192.168.4.1

## Status ved avbrutt sesjon

- Koden kompilerer ikke ennå — kompileringsfeil med `pins_arduino.h` under utredning
- Løsning forsøkt: festet `platform = espressif32@6.3.2` i platformio.ini
- Neste steg: verifiser at bygg fungerer etter Clean + Build med ny platform-versjon
- ESP32 WROOM med CP2102 USB-seriellchip, COM3
- Upload-hastighet satt til 115200 baud (redusert fra 921600 pga. tilkoblingsproblemer)
- For å flashe manuelt: hold BOOT, trykk EN/RST, slipp EN/RST, slipp BOOT — gjør dette når "Connecting......" vises i terminalen

## Git

- Repo: https://github.com/Andreas-Pedersen/dirtbot
- Lokal git-bruker: Andreas Pedersen <andreas@andreaspedersen.no>
