# Build Guide (Step-by-step)

> **Status: Alpha** — this guide is written for the target hardware but has
> not been validated end-to-end on a physical build yet.

---

## What you're building

- **Waveshare ESP32-P4 Module Dev Kit** reads audio from its **onboard MEMS microphone**
  via the **ES8311 codec**
- Runs a **1024-point FFT @ 44.1 kHz**
- Renders **128 bars** onto **two daisy-chained HUB75 128×64 RGB matrix panels** (256×64 total)

---

## Parts list

Required:
- 1× Waveshare ESP32-P4 Module Dev Kit (SKU:30560)
- 2× HUB75 RGB matrix panel, 128×64, FM6126A shift driver
- 1× HUB75 16-pin ribbon cable (panel 1 OUT → panel 2 IN)
- 1× 5 V power supply (budget ~4 A per panel, 8 A+ total)
- Dupont / JST wires for HUB75 header-to-GPIO connection

Helpful:
- HUB75 breakout board with screw terminals (cleaner wiring)
- Standoffs / mounting hardware
- Enclosure or frame

No external microphone needed — the dev kit has one built in.

---

## Tools

- USB-C cable (for programming and power during development)
- Small screwdrivers (for terminal blocks, if used)
- Soldering iron (optional — only if making permanent connections)
- A computer with **ESP-IDF v5.3+** installed

---

## Power (read this first)

Two RGB matrix panels can draw **significant current** at full brightness.

- Use a **real 5 V supply** rated for at least 8 A.
- **Do not** power the panels from the ESP32-P4 board's USB supply.
- **Common ground required:** Panel GND, ESP32-P4 GND, and PSU GND all tied together.
- Use thicker wires (18–20 AWG) for the 5 V / GND panel power runs.
- Start with low brightness while testing — if bars flicker when bright,
  your supply or wiring is undersized.

---

## Step 1 — Wire the HUB75 panels

Connect the **first panel's input** to the ESP32-P4 GPIOs as listed in
[WIRING.md](WIRING.md).

Then connect the **first panel's HUB75 output** to the **second panel's
HUB75 input** using a 16-pin ribbon cable. That's the daisy-chain — the
shift driver clocks data through both panels automatically.

Both panels share 5 V power and GND from your supply.

The microphone is onboard — no mic wiring needed.

---

## Step 2 — Install ESP-IDF

Follow [Espressif's getting-started guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html)
for ESP-IDF v5.3+ with ESP32-P4 support.

Verify the install:

```bash
idf.py --version
```

---

## Step 3 — Build and flash

```bash
cd spectrum-plus/firmware
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your serial port (`COMx` on Windows).

The component manager will automatically fetch `esp-dsp` and the HUB75 DMA
library on the first build.

---

## Step 4 — Verify

Expected behavior after flashing:

1. **Boot animation** runs for 6 seconds (sine-wave pattern, 128 bars, red status pixel)
2. **Spectrum analyzer** starts (status pixel turns green)
3. Bars respond to sound picked up by the onboard microphone

If the screen is blank or flickering:
- Check HUB75 wiring and panel power
- Confirm your panels use the FM6126A shift driver
- Reduce brightness in `main.cpp` (`dma_display->setBrightness8(...)`)

If bars never appear after boot animation:
- Check serial monitor for I2S or ES8311 init errors
- Verify the I2C / I2S GPIOs match `pin_config.h`

---

## Step 5 — Enclosure / mounting (optional)

- Mount the ESP32-P4 board behind or beside the panels
- The onboard mic works best when not blocked by the enclosure — leave a small
  opening or mount it near the edge
- Add a stand or frame for a good viewing angle

---

## Next steps

- **Tuning:** See [TUNING.md](TUNING.md) for DSP parameter adjustments.
- **Pin changes:** Edit `firmware/main/pin_config.h` if your wiring differs.
- **Brightness:** Adjust `dma_display->setBrightness8(96)` in `main.cpp`.
