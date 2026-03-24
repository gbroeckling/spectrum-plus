# Wiring Reference (Known-good)

This page is a condensed wiring cheat-sheet.

## INMP441 → ESP32-S3

- VDD / 3V3  →  3.3V
- GND        →  GND
- SCK / BCLK →  GPIO41
- WS / LRCLK →  GPIO42
- SD / DOUT  →  GPIO40
- L/R        →  GND

> The YAML calls: `spectrum::i2s_init_mic(GPIO_NUM_41, GPIO_NUM_42, GPIO_NUM_40);`

## HUB75 128×64 → ESP32-S3

From `esphome/spectrum-analyzer.yaml`:

- R1→GPIO4  G1→GPIO5  B1→GPIO6
- R2→GPIO7  G2→GPIO8  B2→GPIO9
- A→GPIO10  B→GPIO11  C→GPIO12  D→GPIO13  E→GPIO14
- CLK→GPIO15  LAT→GPIO16  OE→GPIO17

Panel:
- `panel_width: 128`
- `panel_height: 64`
- `shift_driver: FM6126A`

## Power

- Panel: 5V + GND from a proper supply
- ESP32: powered over USB (or a stable 5V source)
- **Common ground required** (panel GND tied to ESP32 GND)

See diagrams:
- `docs/diagrams/inmp441_wiring.png`
- `docs/diagrams/hub75_wiring.png`
