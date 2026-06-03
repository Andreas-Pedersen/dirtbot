# Torvtak Dirtbot

ESP32-basert motorstyringsprosjekt for en dirtbot med Sabertooth 2x25 motordriver.

## Funksjonalitet

- To fysiske knapper styrer fremover og bakover
- Gradvis akselerasjon opp til innstilt maks-hastighet
- Knapp holdt inne = kjør, slipp = bremser gradvis til stopp
- Webgrensesnitt for å justere maks-hastighet fremover og bakover uavhengig
- Innstillinger lagres i flash (NVS) og overlever reboot
- WiFi access point **torvtak** (åpent nettverk, ingen passord)
- Tilgang via captive portal eller **http://torvtak.local**

## Hardware

| Komponent | Beskrivelse |
|-----------|-------------|
| ESP32 WROOM | Mikrokontroller |
| Sabertooth 2x25 | Motordriver, to motorer 25A hver |
| 2× trykknapper | Fremover / bakover |

### Kobling

| ESP32 GPIO | Til |
|------------|-----|
| GPIO 32 | Fremover-knapp → GND |
| GPIO 33 | Bakover-knapp → GND |
| GPIO 17 (TX) | Sabertooth S1 |
| GND | Sabertooth 0V (felles GND) |

Knapper kobles direkte mellom GPIO og GND — ingen ekstern motstand.

### Sabertooth DIP-switches

Simplified Serial, 9600 baud:

```
SW1 = ON
SW2 = OFF
SW3 = OFF
SW4 = OFF
SW5 = OFF
SW6 = OFF
```

## Programvareoppsett

### Krav

- [VS Code](https://code.visualstudio.com/)
- [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)

### Bygg og flash

```bash
git clone https://github.com/Andreas-Pedersen/dirtbot.git
cd dirtbot
# Åpne i VS Code, deretter:
# PlatformIO: Build
# PlatformIO: Upload
```

### Manuell boot-modus (hvis automatisk flash feiler)

1. Start **Upload** i PlatformIO
2. Når `Connecting......` vises i terminalen:
   - Hold inn **BOOT**-knappen
   - Trykk og slipp **EN/RST**
   - Slipp **BOOT**

## Bruk

1. Koble til WiFi-nettverket **torvtak**
2. Nettleseren åpner captive portal automatisk, eller gå til **http://torvtak.local**
3. Juster maks-hastighet for fremover og bakover med sliderne
4. Trykk **Lagre** — innstillingene beholdes ved reboot

## Justerbare parametere i koden

| Parameter | Standard | Forklaring |
|-----------|----------|------------|
| `ACCEL_RATE` | 1.5 | % per 20 ms ved akselerasjon (~1.3 sek til full fart) |
| `DECEL_RATE` | 3.0 | % per 20 ms ved bremsing (~0.7 sek til stopp) |
| `MOTOR2_INVERTED` | true | Snu motor 2 hvis begge er montert i samme retning |
| `PIN_BTN_FWD` | GPIO 32 | Fremover-knapp |
| `PIN_BTN_BWD` | GPIO 33 | Bakover-knapp |
| `PIN_SABER_TX` | GPIO 17 | Sabertooth seriell TX |
