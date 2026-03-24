// spectrum_dsp.h — FFT + full DSP pipeline for 128-bar spectrum analyzer
//
// Adapted from the original 64-bar Spectrum project.
// Uses esp-dsp for FFT computation.  All DSP constants and logic are
// preserved; only BAR_COUNT and display height references are doubled.
#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "esp_log.h"
#include "esp_dsp.h"
#include "i2s_mic.h"

namespace spectrum {

static const char *TAG = "spectrum";

// ─── FFT config ───────────────────────────────────────────────────
constexpr uint16_t FFT_SAMPLES   = 1024;          // doubled for 128 bars
constexpr float    SAMPLE_RATE   = 44100.0f;
constexpr uint16_t BAR_COUNT     = 128;
constexpr uint16_t DISPLAY_H     = 64;             // panel height

// ─── Slow per-bar AGC (~4 min) ───────────────────────────────────
constexpr float BAR_TARGET_LEVEL  = 0.50f;
constexpr float FRAME_INTERVAL    = 0.040f;        // 40 ms
constexpr float BAR_AVG_TAU       = 240.0f;
constexpr float BAR_AVG_ALPHA     = FRAME_INTERVAL / BAR_AVG_TAU;

// ─── Tie AGCs ─────────────────────────────────────────────────────
constexpr float GAIN_TIE_MAX_RATIO = 2.5f;

// ─── Short-term range (20 s) per bar ─────────────────────────────
constexpr float MAX20_DECAY = 0.9980f;

// ─── 10-minute window max tracking ───────────────────────────────
constexpr uint32_t WIN_BUCKET_MS        = 10000;
constexpr int      WIN_BUCKETS          = 60;
constexpr float    WIN_MAX_MIN_FRACTION = 0.22f;

// ─── Noise floor tracking ────────────────────────────────────────
constexpr float NOISE_FAST_ALPHA = 0.06f;
constexpr float NOISE_SLOW_ALPHA = 0.0015f;
constexpr float NOISE_MULT       = 1.12f;
constexpr float ABS_GATE         = 0.010f;

// ─── Output shaping ──────────────────────────────────────────────
constexpr float OUT_STRETCH = 1.12f;
constexpr float OUT_GAMMA   = 0.85f;
constexpr float OUT_CLIP    = 1.25f;

// ─── Internal state ──────────────────────────────────────────────
namespace {

// FFT buffers (interleaved complex: [re0, im0, re1, im1, …])
float s_fft_buf[FFT_SAMPLES * 2];
float s_window[FFT_SAMPLES];

// Bin mapping: one FFT bin per bar
int  s_bin[BAR_COUNT];
bool s_bins_ready = false;

// High-pass state
float s_hp_state = 0.0f;

// Peak-dot decay timer
uint32_t s_last_peak_decay_ms = 0;

// Per-bar state
std::array<float, BAR_COUNT> s_bar_gain{};
std::array<float, BAR_COUNT> s_bar_avg{};
std::array<float, BAR_COUNT> s_noise{};
std::array<float, BAR_COUNT> s_max20{};
bool s_bar_state_ready = false;

// 10-minute bucket maxima
float s_win_bucket_max[BAR_COUNT][WIN_BUCKETS];
std::array<float, BAR_COUNT> s_max10{};
uint32_t s_bucket_start_ms = 0;
int      s_bucket_idx      = 0;

// Ref level (diagnostic only)
float s_ref_level = 0.10f;

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ─── Hamming window ──────────────────────────────────────────────
void build_hamming_window() {
    for (int i = 0; i < FFT_SAMPLES; i++) {
        s_window[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * (float)i / (float)(FFT_SAMPLES - 1));
    }
}

// ─── Bin map: 30 Hz → 18 kHz, log-spaced, one bin per bar ───────
void build_single_bin_map() {
    const float f_min = 30.0f;
    const float f_max = 18000.0f;
    const int   max_bin = (FFT_SAMPLES / 2) - 1;

    for (int i = 0; i < BAR_COUNT; i++) {
        const float t = (BAR_COUNT == 1) ? 0.0f : ((float)i / (float)(BAR_COUNT - 1));
        const float f = f_min * powf(f_max / f_min, t);

        int k = (int)lroundf((f * (float)FFT_SAMPLES) / SAMPLE_RATE);
        if (k < 1)       k = 1;
        if (k > max_bin)  k = max_bin;

        const int remaining       = (BAR_COUNT - 1) - i;
        const int latest_allowed  = max_bin - remaining;
        if (k > latest_allowed) k = latest_allowed;
        if (i > 0 && k <= s_bin[i - 1]) k = s_bin[i - 1] + 1;
        if (k > latest_allowed) k = latest_allowed;

        s_bin[i] = k;
    }
    s_bins_ready = true;
}

// ─── 10-min window helpers ───────────────────────────────────────
void win_reset_all(uint32_t now_ms) {
    s_bucket_start_ms = now_ms;
    s_bucket_idx      = 0;
    for (int i = 0; i < BAR_COUNT; i++) {
        for (int b = 0; b < WIN_BUCKETS; b++) s_win_bucket_max[i][b] = 0.0f;
        s_max10[i] = 0.0f;
    }
}

void win_recompute_all() {
    for (int i = 0; i < BAR_COUNT; i++) {
        float m = 0.0f;
        for (int b = 0; b < WIN_BUCKETS; b++)
            if (s_win_bucket_max[i][b] > m) m = s_win_bucket_max[i][b];
        s_max10[i] = m;
    }
}

void win_advance_buckets(uint32_t now_ms) {
    if (s_bucket_start_ms == 0) { win_reset_all(now_ms); return; }

    bool advanced = false;
    while ((uint32_t)(now_ms - s_bucket_start_ms) >= WIN_BUCKET_MS) {
        s_bucket_start_ms += WIN_BUCKET_MS;
        s_bucket_idx = (s_bucket_idx + 1) % WIN_BUCKETS;
        for (int i = 0; i < BAR_COUNT; i++)
            s_win_bucket_max[i][s_bucket_idx] = 0.0f;
        advanced = true;
    }
    if (advanced) win_recompute_all();
}

}  // anonymous namespace

// ─── Public API ──────────────────────────────────────────────────

inline void init() {
    // Initialise esp-dsp FFT tables
    dsps_fft2r_init_fc32(nullptr, FFT_SAMPLES);
    build_hamming_window();
    build_single_bin_map();

    s_hp_state          = 0.0f;
    s_last_peak_decay_ms = 0;
    s_ref_level          = 0.10f;

    for (int i = 0; i < BAR_COUNT; i++) {
        s_bar_gain[i] = 1.0f;
        s_bar_avg[i]  = BAR_TARGET_LEVEL;
        s_noise[i]    = 0.0f;
        s_max20[i]    = 0.12f;
    }
    s_bar_state_ready = true;
    win_reset_all(esp_log_timestamp());

    ESP_LOGI(TAG, "DSP initialised: %d-pt FFT, %d bars, %.0f Hz sample rate",
             FFT_SAMPLES, BAR_COUNT, SAMPLE_RATE);
}

inline void boot_animation_update(std::array<uint8_t, BAR_COUNT> &bars,
                                  std::array<uint8_t, BAR_COUNT> &peaks) {
    static uint32_t frame = 0;
    frame++;

    for (int i = 0; i < BAR_COUNT; i++) {
        float w1  = 0.5f + 0.5f * sinf((i * 0.35f) + (frame * 0.20f));
        float w2  = 0.5f + 0.5f * sinf((i * 0.11f) - (frame * 0.16f));
        float mix = w1 * 0.72f + w2 * 0.28f;

        int h = (int)(6.0f + mix * 54.0f);
        if (h < 0)          h = 0;
        if (h > DISPLAY_H)  h = DISPLAY_H;
        bars[i] = (uint8_t)h;

        int p = h + 2 + (int)(2.0f * (0.5f + 0.5f * sinf(frame * 0.33f + i * 0.19f)));
        if (p > DISPLAY_H) p = DISPLAY_H;
        peaks[i] = (uint8_t)p;
    }
}

inline void fft_update(std::array<uint8_t, BAR_COUNT> &bars,
                       std::array<uint8_t, BAR_COUNT> &peaks) {
    if (!mic::is_ready()) {
        for (int i = 0; i < BAR_COUNT; i++) {
            if (bars[i]  > 0) bars[i]--;
            if (peaks[i] > 0) peaks[i]--;
        }
        return;
    }

    if (!s_bins_ready)      build_single_bin_map();
    if (!s_bar_state_ready) {
        for (int i = 0; i < BAR_COUNT; i++) {
            s_bar_gain[i] = 1.0f;
            s_bar_avg[i]  = BAR_TARGET_LEVEL;
            s_noise[i]    = 0.0f;
            s_max20[i]    = 0.12f;
        }
        s_bar_state_ready = true;
        win_reset_all(esp_log_timestamp());
    }

    // ── Read I2S samples, apply high-pass, fill FFT buffer ───────
    int read_fails = 0;
    for (int i = 0; i < FFT_SAMPLES; i++) {
        float s = 0.0f;
        if (!mic::read_sample(s)) read_fails++;

        s_hp_state  = s_hp_state * 0.995f + s * 0.005f;
        float hp    = s - s_hp_state;

        s_fft_buf[i * 2]     = hp * s_window[i];   // real
        s_fft_buf[i * 2 + 1] = 0.0f;               // imag
    }

    // ── FFT ──────────────────────────────────────────────────────
    dsps_fft2r_fc32(s_fft_buf, FFT_SAMPLES);
    dsps_bit_rev_fc32(s_fft_buf, FFT_SAMPLES);

    // ── Per-bar raw magnitude ────────────────────────────────────
    float raw_bar[BAR_COUNT];
    float frame_max = 1e-6f;

    for (int i = 0; i < BAR_COUNT; i++) {
        const int k   = s_bin[i];
        const float re = s_fft_buf[k * 2];
        const float im = s_fft_buf[k * 2 + 1];
        const float mag = sqrtf(re * re + im * im);

        float v = log10f(1.0f + mag * 80.0f);
        if (!std::isfinite(v) || v < 0.0f) v = 0.0f;

        // Balance tilt: reduce lows, lift highs
        const float t    = (float)i / (float)(BAR_COUNT - 1);
        const float tilt = 0.55f + 0.90f * t;       // 0.55 → 1.45
        v *= tilt;

        raw_bar[i] = v;
        if (v > frame_max) frame_max = v;
    }

    // ── Ref level (diagnostic) ───────────────────────────────────
    float target_ref = frame_max * 1.15f;
    if (target_ref < 0.02f) target_ref = 0.02f;
    if (!std::isfinite(s_ref_level) || s_ref_level < 0.001f) s_ref_level = target_ref;
    else s_ref_level = s_ref_level * 0.94f + target_ref * 0.06f;
    s_ref_level = clampf(s_ref_level, 0.02f, 20.0f);

    const uint32_t now_ms = esp_log_timestamp();
    win_advance_buckets(now_ms);

    const bool decay_peaks = (now_ms - s_last_peak_decay_ms) >= 240;

    // ── Per-bar DSP pipeline ─────────────────────────────────────
    for (int i = 0; i < BAR_COUNT; i++) {
        // Noise floor tracking
        const float r = raw_bar[i];
        float n = s_noise[i];
        if (r < n) n = (1.0f - NOISE_FAST_ALPHA) * n + NOISE_FAST_ALPHA * r;
        else       n = (1.0f - NOISE_SLOW_ALPHA) * n + NOISE_SLOW_ALPHA * r;
        if (!std::isfinite(n) || n < 0.0f) n = 0.0f;
        s_noise[i] = n;

        float snr = r - n * NOISE_MULT - ABS_GATE;
        if (!std::isfinite(snr) || snr < 0.0f) snr = 0.0f;

        // Slow AGC gain
        float x = snr * s_bar_gain[i];

        // 20 s max tracking
        float m20 = s_max20[i] * MAX20_DECAY;
        if (x > m20) m20 = x;
        if (m20 < 0.12f) m20 = 0.12f;
        s_max20[i] = m20;

        // 10-min window max
        float &bucket = s_win_bucket_max[i][s_bucket_idx];
        if (x > bucket) bucket = x;
        if (x > s_max10[i]) s_max10[i] = x;

        const float denom_min = std::max(0.12f, s_max10[i] * WIN_MAX_MIN_FRACTION);
        const float denom     = std::max(m20, denom_min);

        float norm = (denom > 1e-6f) ? (x / denom) : 0.0f;

        // Extra quiet gating
        if (snr < 0.020f && s_max10[i] < 0.20f) norm = 0.0f;

        // Output shaping
        norm *= OUT_STRETCH;
        norm  = clampf(norm, 0.0f, OUT_CLIP);
        norm  = powf(norm, OUT_GAMMA);
        norm  = clampf(norm, 0.0f, 1.0f);

        int target = (int)lroundf(norm * (float)DISPLAY_H);
        if (target < 0)          target = 0;
        if (target > DISPLAY_H)  target = DISPLAY_H;

        // Attack / release
        int cur = (int)bars[i];
        if (target > cur) { int step = target - cur; if (step > 12) step = 12; cur += step; }
        else if (target < cur) { int step = cur - target; if (step > 6) step = 6; cur -= step; }
        cur = clampf(cur, 0, DISPLAY_H);
        bars[i] = (uint8_t)cur;

        // Peak dots
        if (cur >= (int)peaks[i]) {
            peaks[i] = (uint8_t)cur;
        } else if (decay_peaks && peaks[i] > 0 && peaks[i] > bars[i]) {
            peaks[i]--;
        }

        // Slow AGC update
        const float out_level = (float)cur / (float)DISPLAY_H;
        s_bar_avg[i] += BAR_AVG_ALPHA * (out_level - s_bar_avg[i]);
        const float err  = BAR_TARGET_LEVEL - s_bar_avg[i];
        const float bias = (err < 0.0f) ? 1.45f : 0.85f;
        s_bar_gain[i] *= (1.0f + BAR_AVG_ALPHA * bias * err);
        s_bar_gain[i]  = clampf(s_bar_gain[i], 0.20f, 40.0f);
    }

    // Tie AGCs: enforce max/min ≤ 2.5
    float gmax = 0.0f;
    for (int i = 0; i < BAR_COUNT; i++)
        if (s_bar_gain[i] > gmax) gmax = s_bar_gain[i];
    if (gmax < 0.20f) gmax = 0.20f;
    const float gmin_allowed = gmax / GAIN_TIE_MAX_RATIO;
    for (int i = 0; i < BAR_COUNT; i++)
        if (s_bar_gain[i] < gmin_allowed) s_bar_gain[i] = gmin_allowed;

    if (decay_peaks) s_last_peak_decay_ms = now_ms;

    if (read_fails > FFT_SAMPLES / 5) {
        ESP_LOGW(TAG, "I2S read drops: %d/%d", read_fails, FFT_SAMPLES);
    }
}

}  // namespace spectrum
