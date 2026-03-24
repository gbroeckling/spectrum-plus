# Tuning & DSP Notes

This repo includes a working DSP stack tuned for:
- 64 bars
- 30 Hz..18 kHz log spacing (one FFT bin per bar)
- slow per-bar AGC (~4 minutes)
- short-term max tracking (~20 seconds)
- 10-minute rolling maxima bucket system (prevents “hyper sensitive” after loud peaks)
- noise floor tracking & gating (reduces always-on lights in silence)

The DSP lives in `esphome/spectrum_includes.h` inside `spectrum::fft_update()`.

## What to tweak first (safe-ish)

### 1) “Too many lights in silence”
Look at:
- `ABS_GATE`
- `NOISE_MULT`
- the “Extra quiet gating” line:
  - `if (snr < 0.020f && s_max10[i] < 0.20f) norm = 0.0f;`

### 2) “Bass heavy / unbalanced”
Look at the tilt:
- `const float tilt = 0.55f + 0.90f * t;`

Lower the low-end multiplier or raise the high-end multiplier.

### 3) “Bars feel sluggish”
Look at attack/release step sizes:
- attack step cap (currently 12)
- release step cap (currently 6)
and/or `MAX20_DECAY` (20s window).

### 4) “Peaks drop too fast/slow”
Peak dot decay rate:
- `decay_peaks` timing threshold (currently 240ms)
- decrement amount (currently 1)

## Important principle

A spectrum analyzer is always a compromise between:
- sensitivity in quiet rooms
- not reacting to noise too much
- fast response to music
- stable long-term balance

This implementation uses:
- noise tracking + gating to avoid “floor shimmer”
- short-term max for quick dynamics
- 10-minute max to avoid over-amplification after loud sections
- slow AGC to prevent any band from staying “dead” forever

If you change constants, change ONE thing at a time and test with:
- silence
- spoken voice
- pink noise
- a bass-heavy track
- a bright/high-hat track
