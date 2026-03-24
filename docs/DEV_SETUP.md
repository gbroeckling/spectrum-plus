# Development Setup

How to build, flash, and monitor Spectrum Plus firmware.

> If you've been using ESPHome inside Home Assistant to compile, this replaces
> that workflow. ESPHome does not support the ESP32-P4 chip.

---

## Option A — VS Code (recommended for most users)

This is the closest experience to the ESPHome dashboard: a build button,
a flash button, and a serial monitor, all in one window.

### 1. Install VS Code

Download from https://code.visualstudio.com/ and install.

### 2. Install the ESP-IDF extension

1. Open VS Code
2. Go to **Extensions** (Ctrl+Shift+X)
3. Search for **ESP-IDF** (publisher: Espressif Systems)
4. Click **Install**

### 3. Run the ESP-IDF setup wizard

1. Press **Ctrl+Shift+P** → type **ESP-IDF: Configure ESP-IDF Extension**
2. Choose **Express** setup
3. Select ESP-IDF version **v5.3** or newer (required for ESP32-P4)
4. Let it download and install — this takes a few minutes

### 4. Open the project

1. **File → Open Folder** → navigate to `spectrum-plus/firmware/`
2. VS Code should detect it as an ESP-IDF project automatically

### 5. Set the target

1. Look at the **bottom status bar** in VS Code
2. Click the chip target (it may say `esp32` by default)
3. Select **esp32p4**

Or press **Ctrl+Shift+P** → **ESP-IDF: Set Espressif Device Target** → **esp32p4**

### 6. Build

Click the **Build** button in the bottom status bar (cylinder icon), or:

**Ctrl+Shift+P** → **ESP-IDF: Build your Project**

The first build will download dependencies (esp-dsp, HUB75 library) via
the ESP-IDF component manager. This may take a few minutes.

### 7. Flash

1. Connect the dev kit via USB-C
2. Click the **Flash** button in the status bar (lightning icon), or:
   **Ctrl+Shift+P** → **ESP-IDF: Flash your Project**
3. Select the correct COM port when prompted

### 8. Monitor

Click the **Monitor** button (screen icon) in the status bar, or:

**Ctrl+Shift+P** → **ESP-IDF: Monitor Device**

This opens a serial terminal showing boot logs, init messages, and any
warnings from the DSP or I2S subsystems.

**Tip:** You can click **Build, Flash and Monitor** (the combined button)
to do all three in one step.

---

## Option B — Command line

If you prefer a terminal workflow.

### 1. Install ESP-IDF

Follow Espressif's official guide for your OS:
https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html

**Windows:** Use the ESP-IDF Tools Installer (includes Python, CMake, Ninja,
and the toolchain). After install, use the **ESP-IDF Command Prompt** or
**ESP-IDF PowerShell** shortcut.

**Linux / macOS:**
```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32p4
source export.sh
```

### 2. Build

```bash
cd spectrum-plus/firmware
idf.py set-target esp32p4
idf.py build
```

### 3. Flash

```bash
idf.py -p COM3 flash
```

Replace `COM3` with your actual port (`/dev/ttyUSB0` on Linux,
`/dev/cu.usbserial-*` on macOS).

### 4. Monitor

```bash
idf.py -p COM3 monitor
```

Press **Ctrl+]** to exit the monitor.

### 5. All-in-one

```bash
idf.py -p COM3 flash monitor
```

---

## Troubleshooting

### "ESP32-P4 is not a valid target"

Your ESP-IDF version is too old. You need v5.3 or newer.
Check with `idf.py --version`.

### Component download fails

The first build fetches dependencies from the ESP component registry.
Make sure you have an internet connection. If behind a proxy, set
`HTTP_PROXY` / `HTTPS_PROXY` environment variables.

### "No serial port found"

- Check that the USB-C cable supports data (some are charge-only)
- Install the USB-to-UART driver if needed (CP210x or CH340, depending
  on your board revision — check the Waveshare wiki)
- On Windows, check Device Manager for the COM port number
- On Linux, add your user to the `dialout` group:
  `sudo usermod -aG dialout $USER` (then log out and back in)

### Build succeeds but nothing happens on the display

- Check HUB75 wiring against `docs/WIRING.md`
- Check that panels are powered (5 V + common GND)
- Open the serial monitor to see init log messages
- Try reducing brightness: edit `dma_display->setBrightness8(96)` in
  `main.cpp` to a lower value (e.g., 32)

---

## Project layout

When you open `firmware/` in VS Code, here's what you're looking at:

```
firmware/
├── CMakeLists.txt          ← Top-level build file (don't edit)
├── sdkconfig.defaults      ← Build defaults for ESP32-P4
├── partitions.csv          ← Flash partition layout
└── main/
    ├── CMakeLists.txt      ← Component registration
    ├── idf_component.yml   ← Dependencies (esp-dsp, HUB75 lib)
    ├── main.cpp            ← Entry point — edit display/task code here
    ├── spectrum_dsp.h      ← DSP pipeline — edit tuning constants here
    ├── i2s_mic.h           ← Mic/codec driver — usually no edits needed
    └── pin_config.h        ← GPIO assignments — edit if rewiring
```

**Most common edits:**
- Brightness → `main.cpp` line with `setBrightness8()`
- DSP tuning → constants at the top of `spectrum_dsp.h`
- Pin wiring → `pin_config.h`
