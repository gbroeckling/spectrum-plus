# Changelog

All notable changes to this project will be documented in this file.

## [0.1.0-alpha] — 2026-03-23

### Added
- Pure ESP-IDF firmware (no ESPHome / Home Assistant dependency)
- Waveshare ESP32-P4 Module Dev Kit (SKU:30560) support
- 128 frequency bars (doubled from original 64)
- 2× HUB75 128×64 panel daisy-chain (256×64 total display)
- Onboard MEMS microphone via ES8311 codec (no external mic needed)
- 1024-point FFT at 44.1 kHz for improved frequency resolution
- Dual-core FreeRTOS tasks: FFT on core 0, rendering on core 1
- Full DSP pipeline: noise gating, per-bar AGC, 20 s range tracking,
  10-minute bucketed maxima, output shaping
- Color gradient: Red → Orange → Yellow → Green → Cyan → Blue
  with purple heat shift at bar tops
- Boot animation (6 s) with wave pattern across all 128 bars
- Complete GPIO conflict map for the Waveshare P4 dev kit
- Pin assignments verified against onboard Ethernet, SD card, and audio

### Changed (from original Spectrum project)
- Board: ESP32-S3 DevKitC-1 → Waveshare ESP32-P4 (dual-core RISC-V, 400 MHz)
- Microphone: external INMP441 → onboard MEMS mic + ES8311 codec
- I2S driver: legacy `driver/i2s.h` → new `driver/i2s_std.h` API
- FFT: arduinoFFT (512-pt) → esp-dsp (1024-pt)
- Display: single 128×64 panel → two 128×64 panels daisy-chained (256×64)
- Bars: 64 → 128
- Framework: ESPHome (Arduino) → pure ESP-IDF (CMake)

### Removed
- ESPHome configuration and YAML files
- Home Assistant API / OTA / Wi-Fi connectivity
- External INMP441 microphone requirement
