# FAQ / Troubleshooting

## I only see the startup animation
That means `boot_anim` is still true.
- Confirm Wi‑Fi credentials and API encryption key in `secrets.yaml`
- Confirm Home Assistant can reach the device on your network

## Screen is blank / flickering
Most common causes:
- HUB75 wiring mismatch
- Panel needs `shift_driver: FM6126A` (already set)
- Insufficient 5V power supply or thin power wires

Try:
- Reduce brightness
- Re-check panel power polarity
- Confirm common ground between panel and ESP32

## Bars are always on in silence
This is usually:
- microphone self-noise + gain
- too-low gating thresholds

Try tuning:
- `ABS_GATE`
- `NOISE_MULT`
- the “Extra quiet gating” thresholds in `spectrum_includes.h`

## My panel isn’t FM6126A
Some panels use other drivers. If colors are wrong or nothing works,
you may need to adjust `shift_driver` or panel settings for your hardware.

## Can I use a different mic?
Yes, but you must update:
- the I2S init pins (`i2s_init_mic(...)`)
- potentially the I2S format and scaling depending on the mic

## Can I change the number of bars?
Yes, but this project is tuned around:
- 64 bars on a 128px wide display (2px per bar)
If you change bar count you’ll need to update:
- display lambda drawing
- DSP constants (`BAR_COUNT`, bin map)
