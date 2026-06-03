# Toilltak Dirtbot

ESP32-basert motorstyringsprosjekt for en dirtbot med Sabertooth 2x25 motordriver.

## Funksjonalitet

- To fysiske knapper (fremover / bakover) med 100 ms debounce og støyfiltrering
- Gradvis akselerasjon og bremsing (konfigurerbart nivå 1–10)
- Knapp holdt inne = kjør, slipp = bremser gradvis til stopp
- **Startup-interlock:** begge knapper må være sluppet i 30 sekunder etter oppstart før kjøring tillates
- Webgrensesnitt for å justere maks-hastighet og akselerasjon
- Innstillinger lagres i flash (NVS) og overlever reboot
- WiFi access point **Toilltak** (åpent nettverk, ingen passord)
- Tilgang via captive portal eller **http://toilltak.local**
- Debug-side med live statusvisning og logg

## Hardware

| Komponent | Beskrivelse |
|-----------|-------------|
| ESP32 WROOM (CP2102) | Mikrokontroller, COM3, 115200 baud |
| Sabertooth 2x25 v1 | Motordriver, to motorer 25A hver |
| 2× trykknapper | Fremover / bakover |
| DeWalt 18V Li-ion | Batteri |

### Kobling

| ESP32 GPIO | Til | Ledningsfarge |
|------------|-----|---------------|
| GPIO 32 | Fremover-knapp → GND | Gul |
| GPIO 33 | Bakover-knapp → GND | Oransje |
| GPIO 17 (TX2) | Sabertooth S1 | — |
| GND | Sabertooth 0V (felles GND) | Svart |

**Merk:** Koble 100nF keramisk kondensator fra GPIO 32/33 til GND (tett på ESP32) for støyfiltrering. Motorstøy fra Sabertooth kan ellers gi falske knappetrykk.

### Sabertooth 2x25 v1 — DIP-switch (101101)

Simplified Serial, 19200 baud:

```
SW1 = ON   — Simplified Serial mode (bit 0)
SW2 = OFF  — Simplified Serial mode (bit 1)
SW3 = ON   — Auto lithium cutoff (Li-ion batteri)
SW4 = ON   — 19200 baud (bit 0)
SW5 = OFF  — 19200 baud (bit 1)
SW6 = ON   — Ingen slave select (M1 og M2 uavhengige)
```

Sabertooth-protokoll (Simplified Serial, 8N1):
- Byte 1–127: Motor 1 (1=full bak, 64=stopp, 127=full frem)
- Byte 128–255: Motor 2 (128=full bak, 192=stopp, 255=full frem)
- Byte 0: Nødstopp

## Programvareoppsett

### Krav

- [VS Code](https://code.visualstudio.com/)
- [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
- Platform: `espressif32@6.3.2` (nyere versjoner gir `pins_arduino.h`-feil)

### Bygg og flash

```bash
git clone https://github.com/Andreas-Pedersen/dirtbot.git
cd dirtbot
# Åpne i VS Code, deretter:
# PlatformIO: Build
# PlatformIO: Upload
```

**Koble ESP32 direkte til PC** — ikke via USB-hub (gir tilkoblingsproblemer).

### Manuell boot-modus (hvis automatisk flash feiler)

1. Start **Upload** i PlatformIO
2. Når `Connecting......` vises:
   - Hold inn **BOOT**
   - Trykk og slipp **EN/RST**
   - Slipp **BOOT**

## Bruk

1. Koble til WiFi: **Toilltak** (åpent)
2. Nettleseren åpner captive portal automatisk — eller gå til **http://192.168.4.1**
3. Juster fart med sliderne og trykk **Lagre**
4. **Startup-interlock:** etter oppstart må begge knapper være sluppet i 30 sek — debug-siden viser nedtelling

## Webgrensesnitt

| URL | Beskrivelse |
|-----|-------------|
| `/` | Hovedside — fartsslidere og testkjøring |
| `/settings` | Innstillinger — slider-tak, akselerasjon, pinout, motorretning |
| `/debug` | Live status og loggbuffer |

## Konfigurerbare parametere

Alle innstillinger endres i webgrensesnittet og lagres i NVS:

| Parameter | Standard | Beskrivelse |
|-----------|----------|-------------|
| Maks fart fremover | 100 % | Slider-tak på hovedsiden |
| Maks fart bakover | 100 % | Slider-tak på hovedsiden |
| Akselerasjon | 3 (av 10) | 1=treig (~6 sek), 10=rask (~0.7 sek) |
| Motor 1 invertert | false | Snu kjøreretning |
| Motor 2 invertert | true | Motorene sitter speilvend på akselen |
| Fremover-knapp GPIO | 32 | Kan endres i innstillinger |
| Bakover-knapp GPIO | 33 | Kan endres i innstillinger |
| Sabertooth TX GPIO | 17 | Kan endres i innstillinger |

## Sikkerhetsfunksjoner

- **Startup-interlock:** 30 sek ventetid etter boot, begge knapper må være sluppet
- **Web kjøretidsbegrensning:** maks 3 sek kontinuerlig web-kjøring
- **Web timeout:** stopper automatisk hvis ingen kommando mottas på 2 sek
- **Nødstopp ved oppstart:** byte 0 sendes umiddelbart etter Serial2-initialisering
- **Debounce:** 100 ms (filtrerer motorstøy på knappeledninger)
