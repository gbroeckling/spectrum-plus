# Build Guide (Step-by-step)

This guide is written so anyone can reproduce the build and get a working spectrum analyzer.

> ✅ Credentials note: the repo ships with **no Wi‑Fi/API/OTA passwords**.  
> You must create `esphome/secrets.yaml` before compiling.

---

## What you’re building

- **ESP32‑S3** reads audio from an **INMP441 I2S mic**
- Runs a **512-point FFT @ 44.1kHz**
- Renders **64 bars** onto a **HUB75 128×64** RGB matrix panel

---

## Parts list (core)

Required:
- 1× ESP32‑S3 DevKitC‑1 (or compatible ESP32‑S3 board)
- 1× HUB75 RGB matrix panel (128×64)
- 1× INMP441 I2S microphone module
- 1× 5V power supply sized for your panel (see “Power” below)
- Wiring (Dupont / JST / screw terminals as appropriate)

Helpful:
- HUB75 adapter / breakout with screw terminals (cleaner wiring)
- Standoffs / mounting for the panel + controller
- Enclosure or frame (optional)

AliExpress links you provided:
- https://www.aliexpress.com/item/1005010227131923.html
- https://www.aliexpress.com/item/1005007319706057.html
- https://www.aliexpress.com/item/1005006155582430.html

---

## Tools

- Soldering iron (optional but recommended if you’re making permanent wiring)
- Small screwdrivers (for terminal blocks)
- USB cable for ESP32-S3
- Home Assistant with ESPHome add-on **or** ESPHome CLI on a computer

---

## Power (read this)

RGB matrix panels can draw **a lot of current** depending on brightness and content.
- Use a **real 5V supply** with enough current capacity for your panel.
- **Do not** power the HUB75 panel from the ESP32 3.3V regulator.
- Always connect **panel GND** to **ESP32 GND** (common ground).

Start safe:
- Keep `brightness` conservative while testing.
- Use thicker wires for panel power (5V/GND).
- If you see flicker/reset when bars get bright, your supply/wiring is likely undersized.

---

## Step 1 — Wire the INMP441 (audio input)

Known-good wiring (as used in the config):

- INMP441 VDD/3V3 → ESP32 3.3V
- INMP441 GND → ESP32 GND
- INMP441 SCK/BCLK → GPIO41
- INMP441 WS/LRCLK → GPIO42
- INMP441 SD/DOUT → GPIO40
- INMP441 L/R → GND (forces left channel; matches config)

See diagram: `docs/diagrams/inmp441_wiring.png`

---

## Step 2 — Wire the HUB75 panel (display output)

The ESPHome YAML contains the definitive pin mapping:

- R1→GPIO4  G1→GPIO5  B1→GPIO6
- R2→GPIO7  G2→GPIO8  B2→GPIO9
- A→GPIO10  B→GPIO11  C→GPIO12  D→GPIO13  E→GPIO14
- CLK→GPIO15  LAT→GPIO16  OE→GPIO17

Panel configuration:
- `panel_width: 128`
- `panel_height: 64`
- `shift_driver: FM6126A` (important for many newer panels)

See diagram: `docs/diagrams/hub75_wiring.png`

---

## Step 3 — Install ESPHome firmware

### Option A (Home Assistant + ESPHome add-on)
1. Copy `esphome/spectrum-analyzer.yaml` and `esphome/spectrum_includes.h` into your ESPHome folder
2. Create `esphome/secrets.yaml` from `secrets.example.yaml`
3. In ESPHome, click **+ NEW DEVICE** (or **Import**)
4. Compile and flash via USB for first install (recommended)
5. After the first flash, OTA will work (using your OTA password)

### Option B (ESPHome CLI)
1. Install ESPHome
2. Place the YAML + header in a working directory
3. Add `secrets.yaml`
4. Run:
   - `esphome run spectrum-analyzer.yaml`

---

## Step 4 — Verify it’s working

Expected behavior:
- On boot, you should see a **startup animation** (full-screen bar test)
- Once Home Assistant connects via API, the animation stops and FFT rendering begins
- A tiny status pixel indicates:
  - **Red**: waiting / not connected
  - **Green**: connected

If you see only the boot animation:
- Check Wi‑Fi + API encryption key
- Confirm Home Assistant can reach the device

If you see “nothing” or random flicker:
- Check HUB75 wiring + panel power
- Confirm your panel is compatible with `FM6126A`
- Reduce brightness while debugging power issues

---

## Step 5 — Enclosure / mounting (optional)

Common approaches:
- Mount ESP32 + HUB75 adapter on a small plate behind the panel
- Keep the microphone away from the panel power wiring if possible
- Add a simple stand or frame so the panel can sit at a nice viewing angle

---

## Next steps / customization

Safe customizations (don’t change DSP unless you mean to):
- Brightness
- Color palette in the display lambda
- Bar spacing / bar width
- Startup animation duration

For DSP/AGC tuning, see `docs/TUNING.md`.
