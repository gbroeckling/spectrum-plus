# Tuning & DSP Notes

> All DSP constants live in `firmware/main/spectrum_dsp.h` inside the
> `spectrum` namespace. The main processing function is `spectrum::fft_update()`.

This build is tuned for:
- 128 bars on a 256×64 display (2 px per bar)
- 30 Hz–18 kHz log spacing (one FFT bin per bar)
- 1024-point FFT @ 44.1 kHz
- Slow per-bar AGC (~4 minutes)
- Short-term max tracking (~20 seconds)
- 10-minute rolling maxima buckets (prevents hyper-sensitivity after loud peaks)
- Noise floor tracking & gating (reduces always-on lights in silence)

## What to tweak first

### 1) "Too many lights in silence"

Look at:
- `ABS_GATE` — absolute floor below which signal is zeroed
- `NOISE_MULT` — how aggressively the noise floor is subtracted
- The extra quiet-gating line:
  `if (snr < 0.020f && s_max10[i] < 0.20f) norm = 0.0f;`

### 2) "Bass heavy / unbalanced"

Look at the tilt in `fft_update()`:
```cpp
const float tilt = 0.55f + 0.90f * t;   // lows 0.55 → highs 1.45
```
Lower the low-end multiplier or raise the high-end multiplier.

### 3) "Bars feel sluggish"

Look at attack/release step sizes in `fft_update()`:
- Attack step cap (currently `12`)
- Release step cap (currently `6`)

And `MAX20_DECAY` (controls the 20 s range window).

### 4) "Peaks drop too fast / too slow"

Peak dot decay rate:
- `decay_peaks` timing threshold (currently `240` ms between decrements)
- Decrement amount (currently `1` pixel per tick)

### 5) "Mic is too sensitive / not sensitive enough"

The ES8311 codec gain is set in `i2s_mic.h`:
```cpp
es8311_microphone_gain_set(codec, ES8311_MIC_GAIN_42DB);
```
Available gains range from 0 dB to 42 dB. Lower the gain if the bars
are always maxed out, or raise it if bars barely move.

## Important principle

A spectrum analyzer is always a compromise between:
- Sensitivity in quiet rooms
- Not reacting to noise too much
- Fast response to music
- Stable long-term balance

This implementation uses:
- Noise tracking + gating to avoid "floor shimmer"
- Short-term max for quick dynamics
- 10-minute max to avoid over-amplification after loud sections
- Slow AGC to prevent any band from staying "dead" forever

**Change ONE constant at a time** and test with:
- Silence
- Spoken voice
- Pink noise
- A bass-heavy track
- A bright / hi-hat track
