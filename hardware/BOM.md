# Bill of Materials (BOM)

This BOM reflects the **Spectrum Plus** build: 128-bar analyzer on a 256×64 display.

## Core electronics

- Waveshare ESP32-P4 Module Dev Kit (SKU:30560)
  - Has onboard MEMS microphone + ES8311 codec — no external mic needed
- 2× HUB75 RGB Matrix panel, 128×64 (FM6126A shift driver)
  - Daisy-chained end-to-end for 256×64 total
- 1× HUB75 16-pin ribbon cable (panel 1 OUT → panel 2 IN)

## Power

- 5V power supply sized for **two** panels (budget ~4A per panel)
- 5V/GND power wiring for both panels (thicker gauge recommended)
- Both panels and the ESP32-P4 must share a common ground

## Wiring / mounting

- Dupont leads / JST cables for HUB75 header-to-GPIO connection
- Standoffs / enclosure hardware
