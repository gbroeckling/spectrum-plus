# FAQ / Troubleshooting

## I only see the boot animation (it never switches to the spectrum)

The boot animation runs for 6 seconds and then stops automatically.
If it never transitions:
- Check the serial monitor (`idf.py monitor`) for errors during mic or codec init
- Verify the ES8311 codec initializes (look for `"ES8311 codec initialized"` in the log)
- Confirm the I2S channel enables without error

## Screen is blank / flickering

Most common causes:
- HUB75 wiring mismatch — double-check every pin against [WIRING.md](WIRING.md)
- Panel needs `FM6126A` shift driver (already configured in the firmware)
- Insufficient 5 V power supply or thin power wires
- Pin conflict with onboard peripherals — make sure you're using the GPIOs
  listed in `pin_config.h`, not the ones reserved by Ethernet, SD card, or audio

Try:
- Reduce brightness (`dma_display->setBrightness8(32)` in `main.cpp`)
- Re-check panel power polarity
- Confirm common ground between panels and ESP32-P4

## Bars are always on in silence

This is usually microphone self-noise amplified by the AGC. Try tuning in
`firmware/main/spectrum_dsp.h`:
- Increase `ABS_GATE` (e.g., from `0.010f` to `0.020f`)
- Increase `NOISE_MULT` (e.g., from `1.12f` to `1.25f`)
- Raise the quiet-gating thresholds:
  `if (snr < 0.020f && s_max10[i] < 0.20f) norm = 0.0f;`

## Build fails — component not found

The first build downloads dependencies via the ESP-IDF component manager.
If it fails:
- Check your internet connection
- Verify ESP-IDF v5.3+ is installed with P4 support
- Run `idf.py set-target esp32p4` before building
- If the HUB75 library doesn't support P4 yet, that will surface as a
  compile error — check the library's issue tracker for P4 status

## My panels aren't FM6126A

Some panels use other shift drivers. If colors are wrong or nothing displays,
you may need to change the driver setting in `main.cpp` where `HUB75_I2S_CFG`
is configured (e.g., `HUB75_I2S_CFG::SHIFTREG` for generic panels).

## Can I use a different microphone?

Yes, but you'd need to rewrite `i2s_mic.h` to bypass the ES8311 codec and
read raw I2S from an external mic (like the INMP441). The original
[Spectrum](https://github.com/gbroeckling/Spectrum) project has working
INMP441 code you can reference.

## Can I change the number of bars?

Yes. In `firmware/main/spectrum_dsp.h`, change `BAR_COUNT`. You'll also need
to update the global arrays in `main.cpp` and adjust the rendering math
(bar width = display width / bar count). The 1024-point FFT gives 512 usable
bins, so up to 512 bars is theoretically possible (though visual quality
drops with very narrow bars).

## Can I use a single panel instead of two?

Yes. In `main.cpp`, change the HUB75 chain length to `1` and update the
rendering width to 128. You'd get 64 bars at 2 px each (same as the original
Spectrum project) or 128 bars at 1 px each (tight but usable).
