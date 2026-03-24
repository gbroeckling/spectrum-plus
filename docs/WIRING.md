# Wiring Reference — Spectrum Plus

Waveshare ESP32-P4 Module Dev Kit (SKU:30560) + 2× HUB75 128×64 panels.

**All pin assignments live in `firmware/main/pin_config.h`.**

## Audio (onboard — no external wiring)

The dev kit has a built-in MEMS microphone routed through an ES8311 codec.
These pins are fixed on the PCB:

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA  | 7    | ES8311 control |
| I2C SCL  | 8    | ES8311 control |
| I2S DIN  | 9    | Audio from codec to ESP32 |
| I2S LRCK | 10   | Word select |
| I2S DOUT | 11   | To speaker amp (unused) |
| I2S SCLK | 12   | Bit clock |
| I2S MCLK | 13   | Master clock |
| PA_Ctrl  | 53   | Speaker amp enable (unused) |

## HUB75 panels → ESP32-P4

Two 128×64 FM6126A panels daisy-chained (OUT of panel 1 → IN of panel 2).
Only the first panel's input connects to the ESP32-P4:

| Function | GPIO | Notes |
|----------|------|-------|
| R1       | 1    | Upper-half red |
| G1       | 2    | Upper-half green |
| B1       | 3    | Upper-half blue |
| R2       | 4    | Lower-half red |
| G2       | 5    | Lower-half green |
| B2       | 6    | Lower-half blue |
| A        | 14   | Row address bit 0 |
| B        | 15   | Row address bit 1 |
| C        | 16   | Row address bit 2 |
| D        | 17   | Row address bit 3 |
| E        | 18   | Row address bit 4 |
| CLK      | 19   | Pixel clock |
| LAT      | 20   | Latch |
| OE       | 21   | Output enable |

## GPIOs reserved by other onboard peripherals (do not use)

| Peripheral | GPIOs |
|------------|-------|
| Audio codec | 7, 8, 9, 10, 11, 12, 13, 53 |
| Ethernet RMII | 28, 29, 30, 31, 34, 35, 49, 50, 51, 52 |
| SD card (SDIO) | 39, 40, 41, 42, 43, 44 |
| BOOT button | 0 (typically) |

## Panel daisy-chain

Connect the **HUB75 output** ribbon of panel 1 to the **HUB75 input** of panel 2
using a standard 16-pin ribbon cable. Both panels share the same address/clock/data
lines — the shift driver clocks data through both panels automatically.

## Power

- Both panels: 5V + GND from a proper supply (budget ~4A per panel)
- ESP32-P4: powered over USB-C (or regulated 5V)
- **Common ground required** — panel GND, ESP32 GND, and PSU GND all tied together
