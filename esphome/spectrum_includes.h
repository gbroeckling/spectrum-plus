// spectrum_includes.h
#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Arduino.h"
#include "arduinoFFT.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "esp_log.h"

namespace spectrum {

namespace {

static const char *const TAG = "spectrum";

constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

// FFT config
constexpr uint16_t FFT_SAMPLES = 512;
constexpr float SAMPLE_RATE_HZ = 44100.0f;
constexpr uint8_t BAR_COUNT = 64;

// Slow per-bar AGC (~4 min)
constexpr float BAR_TARGET_LEVEL = 0.50f;
constexpr float FRAME_INTERVAL_SEC = 0.040f;
constexpr float BAR_AVG_TAU_SEC = 240.0f;
constexpr float BAR_AVG_ALPHA = (FRAME_INTERVAL_SEC / BAR_AVG_TAU_SEC);  // ~0.0001667

// Tie AGCs: 150% difference max => max/min <= 2.5
constexpr float GAIN_TIE_MAX_RATIO = 2.5f;

// Short-term range (20s) per bar (visual dynamics)
constexpr float MAX20_DECAY = 0.9980f; // exp(-0.04/20) approx

// 10-minute window max tracking (bucketed)
constexpr uint32_t WIN_BUCKET_MS = 10000; // 10s buckets
constexpr int WIN_BUCKETS = 60;           // 10 minutes
constexpr float WIN_MAX_MIN_FRACTION = 0.22f; // keeps sensitivity from exploding in silence after loud peaks

// Noise floor tracking (per bar)
constexpr float NOISE_FAST_ALPHA = 0.06f;  // drop quickly
constexpr float NOISE_SLOW_ALPHA = 0.0015f; // rise slowly
constexpr float NOISE_MULT = 1.12f;
constexpr float ABS_GATE = 0.010f;

// Output shaping
constexpr float OUT_STRETCH = 1.12f;  // pushes peaks closer to 100%
constexpr float OUT_GAMMA = 0.85f;    // expands mid/high dynamics
constexpr float OUT_CLIP = 1.25f;     // allow brief overshoot before clipping

double s_vReal[FFT_SAMPLES];
double s_vImag[FFT_SAMPLES];
arduinoFFT s_fft(s_vReal, s_vImag, FFT_SAMPLES, SAMPLE_RATE_HZ);

// One bin per bar (no grouping)
int s_bin[BAR_COUNT];
bool s_bins_ready = false;

// Runtime
bool s_i2s_ready = false;
float s_hp_state = 0.0f;
uint32_t s_last_peak_decay_ms = 0;

// Per-bar state
std::array<float, BAR_COUNT> s_bar_gain{};
std::array<float, BAR_COUNT> s_bar_avg{};
std::array<float, BAR_COUNT> s_noise{};
std::array<float, BAR_COUNT> s_max20{};
bool s_bar_state_ready = false;

// 10-minute bucket maxima (on post-noise, post-slow-gain signal)
float s_win_bucket_max[BAR_COUNT][WIN_BUCKETS];
std::array<float, BAR_COUNT> s_max10{};
uint32_t s_bucket_start_ms = 0;
int s_bucket_idx = 0;

inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// 30 Hz .. 18 kHz (log spacing), ONE BIN PER BAR
void build_single_bin_map() {
  const float f_min = 30.0f;
  const float f_max = 18000.0f;

  const int max_bin = (FFT_SAMPLES / 2) - 1;

  for (int i = 0; i < BAR_COUNT; i++) {
    const float t = (BAR_COUNT == 1) ? 0.0f : ((float) i / (float) (BAR_COUNT - 1));
    const float f = f_min * powf((f_max / f_min), t);

    int k = (int) lroundf((f * (float) FFT_SAMPLES) / SAMPLE_RATE_HZ);

    if (k < 1) k = 1;
    if (k > max_bin) k = max_bin;

    const int remaining = (BAR_COUNT - 1) - i;
    const int latest_allowed = max_bin - remaining;
    if (k > latest_allowed) k = latest_allowed;

    if (i > 0 && k <= s_bin[i - 1]) k = s_bin[i - 1] + 1;
    if (k > latest_allowed) k = latest_allowed;

    s_bin[i] = k;
  }

  s_bins_ready = true;
}

bool read_i2s_sample(float &out_sample) {
  int32_t raw = 0;
  size_t bytes_read = 0;

  esp_err_t err = i2s_read(I2S_PORT, &raw, sizeof(raw), &bytes_read, pdMS_TO_TICKS(20));
  if (err != ESP_OK || bytes_read != sizeof(raw)) {
    out_sample = 0.0f;
    return false;
  }

  int32_t s24 = (raw >> 8);
  out_sample = (float) s24 / 8388608.0f;
  return true;
}

void win_reset_all(uint32_t now_ms) {
  s_bucket_start_ms = now_ms;
  s_bucket_idx = 0;

  for (int i = 0; i < BAR_COUNT; i++) {
    for (int b = 0; b < WIN_BUCKETS; b++) s_win_bucket_max[i][b] = 0.0f;
    s_max10[i] = 0.0f;
  }
}

void win_recompute_all() {
  for (int i = 0; i < BAR_COUNT; i++) {
    float m = 0.0f;
    for (int b = 0; b < WIN_BUCKETS; b++) {
      if (s_win_bucket_max[i][b] > m) m = s_win_bucket_max[i][b];
    }
    s_max10[i] = m;
  }
}

inline void win_advance_buckets(uint32_t now_ms) {
  if (s_bucket_start_ms == 0) {
    win_reset_all(now_ms);
    return;
  }

  bool advanced = false;
  while ((uint32_t) (now_ms - s_bucket_start_ms) >= WIN_BUCKET_MS) {
    s_bucket_start_ms += WIN_BUCKET_MS;
    s_bucket_idx = (s_bucket_idx + 1) % WIN_BUCKETS;

    for (int i = 0; i < BAR_COUNT; i++) {
      s_win_bucket_max[i][s_bucket_idx] = 0.0f;
    }
    advanced = true;
  }

  if (advanced) win_recompute_all();
}

}  // namespace

inline void i2s_init_mic(gpio_num_t bclk_pin, gpio_num_t ws_pin, gpio_num_t sd_pin) {
  i2s_driver_uninstall(I2S_PORT);

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = (uint32_t) SAMPLE_RATE_HZ;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
#if defined(I2S_COMM_FORMAT_STAND_I2S)
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
#elif defined(I2S_COMM_FORMAT_I2S_MSB)
  cfg.communication_format = I2S_COMM_FORMAT_I2S_MSB;
#else
  cfg.communication_format = I2S_COMM_FORMAT_I2S;
#endif
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 128;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = bclk_pin;
  pins.ws_io_num = ws_pin;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = sd_pin;

  esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s_driver_install failed: %d", (int) err);
    s_i2s_ready = false;
    return;
  }

  err = i2s_set_pin(I2S_PORT, &pins);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s_set_pin failed: %d", (int) err);
    i2s_driver_uninstall(I2S_PORT);
    s_i2s_ready = false;
    return;
  }

  err = i2s_set_clk(I2S_PORT, (uint32_t) SAMPLE_RATE_HZ, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2s_set_clk warning: %d (continuing)", (int) err);
  }

  i2s_zero_dma_buffer(I2S_PORT);

  build_single_bin_map();

  s_hp_state = 0.0f;
  s_last_peak_decay_ms = millis();

  for (int i = 0; i < BAR_COUNT; i++) {
    s_bar_gain[i] = 1.0f;
    s_bar_avg[i] = BAR_TARGET_LEVEL;
    s_noise[i] = 0.0f;
    s_max20[i] = 0.12f;
  }
  s_bar_state_ready = true;

  win_reset_all(millis());

  s_i2s_ready = true;
  ESP_LOGI(TAG, "INMP441 I2S initialized BCLK=%d WS=%d SD=%d SR=%.0f",
           (int) bclk_pin, (int) ws_pin, (int) sd_pin, SAMPLE_RATE_HZ);
}

inline void boot_animation_update(std::array<uint8_t, 64> &bars,
                                  std::array<uint8_t, 64> &peaks) {
  static uint32_t frame = 0;
  frame++;

  for (int i = 0; i < 64; i++) {
    float w1 = 0.5f + 0.5f * sinf((i * 0.35f) + (frame * 0.20f));
    float w2 = 0.5f + 0.5f * sinf((i * 0.11f) - (frame * 0.16f));
    float mix = (w1 * 0.72f) + (w2 * 0.28f);

    int h = (int) (6.0f + mix * 54.0f);
    if (h < 0) h = 0;
    if (h > 64) h = 64;

    bars[i] = (uint8_t) h;

    int p = h + 2 + (int) (2.0f * (0.5f + 0.5f * sinf((frame * 0.33f) + (i * 0.19f))));
    if (p > 64) p = 64;
    peaks[i] = (uint8_t) p;
  }
}

inline void fft_update(std::array<uint8_t, 64> &bars,
                       std::array<uint8_t, 64> &peaks,
                       float &ref_level) {
  if (!s_i2s_ready) {
    for (int i = 0; i < 64; i++) {
      if (bars[i] > 0) bars[i]--;
      if (peaks[i] > 0) peaks[i]--;
    }
    return;
  }

  if (!s_bins_ready) build_single_bin_map();
  if (!s_bar_state_ready) {
    for (int i = 0; i < BAR_COUNT; i++) {
      s_bar_gain[i] = 1.0f;
      s_bar_avg[i] = BAR_TARGET_LEVEL;
      s_noise[i] = 0.0f;
      s_max20[i] = 0.12f;
    }
    s_bar_state_ready = true;
    win_reset_all(millis());
  }

  int read_fail_count = 0;

  for (int i = 0; i < FFT_SAMPLES; i++) {
    float s = 0.0f;
    bool ok = read_i2s_sample(s);
    if (!ok) read_fail_count++;

    // Gentle high-pass to reduce DC/rumble
    s_hp_state = (s_hp_state * 0.995f) + (s * 0.005f);
    float hp = s - s_hp_state;

    s_vReal[i] = (double) hp;
    s_vImag[i] = 0.0;
  }

  s_fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  s_fft.Compute(FFT_FORWARD);
  s_fft.ComplexToMagnitude();

  // Compute raw per-bar value (log magnitude + balance tilt)
  float raw_bar[BAR_COUNT];
  float frame_max = 1e-6f;

  for (int i = 0; i < BAR_COUNT; i++) {
    const int k = s_bin[i];
    const float mag = (float) s_vReal[k];

    float v = log10f(1.0f + mag * 80.0f);
    if (!isfinite(v) || v < 0.0f) v = 0.0f;

    // Balance tilt: reduce lows, lift highs (less bass-heavy)
    const float t = (float) i / 63.0f;
    const float tilt = 0.55f + 0.90f * t;  // lows 0.55 -> highs 1.45
    v *= tilt;

    raw_bar[i] = v;
    if (v > frame_max) frame_max = v;
  }

  // Keep ref_level sane (debug / optional), not used for scaling anymore
  float target_ref = frame_max * 1.15f;
  if (target_ref < 0.02f) target_ref = 0.02f;
  if (!isfinite(ref_level) || ref_level < 0.001f) ref_level = target_ref;
  else ref_level = (ref_level * 0.94f) + (target_ref * 0.06f);
  ref_level = clampf(ref_level, 0.02f, 20.0f);

  const uint32_t now_ms = millis();
  win_advance_buckets(now_ms);

  // Peak dots: slower drift down
  const bool decay_peaks = (now_ms - s_last_peak_decay_ms) >= 240;

  for (int i = 0; i < BAR_COUNT; i++) {
    // ---- Noise floor tracking (per bar) ----
    const float r = raw_bar[i];
    float n = s_noise[i];

    if (r < n) n = (1.0f - NOISE_FAST_ALPHA) * n + NOISE_FAST_ALPHA * r;
    else       n = (1.0f - NOISE_SLOW_ALPHA) * n + NOISE_SLOW_ALPHA * r;

    if (!isfinite(n) || n < 0.0f) n = 0.0f;
    s_noise[i] = n;

    // Subtract noise + small absolute gate
    float snr = r - (n * NOISE_MULT) - ABS_GATE;
    if (!isfinite(snr) || snr < 0.0f) snr = 0.0f;

    // Apply slow AGC gain (balance)
    float x = snr * s_bar_gain[i];

    // ---- 20s max tracking (range) ----
    float m20 = s_max20[i] * MAX20_DECAY;
    if (x > m20) m20 = x;
    if (m20 < 0.12f) m20 = 0.12f; // don't explode in silence
    s_max20[i] = m20;

    // ---- 10-minute window max tracking (stability vs silence) ----
    float &bucket = s_win_bucket_max[i][s_bucket_idx];
    if (x > bucket) bucket = x;
    if (x > s_max10[i]) s_max10[i] = x;

    // Denominator: mostly 20s max, but never drop too low compared to 10-min peak
    const float denom_min = std::max(0.12f, s_max10[i] * WIN_MAX_MIN_FRACTION);
    const float denom = std::max(m20, denom_min);

    float norm = (denom > 1e-6f) ? (x / denom) : 0.0f;

    // Extra quiet gating: if we've got almost no SNR and no recent peak, keep it off
    if (snr < 0.020f && s_max10[i] < 0.20f) norm = 0.0f;

    // Stretch + shape so peaks hit 100% and we see more up/down range
    norm *= OUT_STRETCH;
    norm = clampf(norm, 0.0f, OUT_CLIP);
    norm = powf(norm, OUT_GAMMA);
    norm = clampf(norm, 0.0f, 1.0f);

    int target = (int) lroundf(norm * 64.0f);
    if (target < 0) target = 0;
    if (target > 64) target = 64;

    // Faster bar response (attack/release)
    int cur = (int) bars[i];
    if (target > cur) {
      int step = target - cur;
      if (step > 12) step = 12;
      cur += step;
    } else if (target < cur) {
      int step = cur - target;
      if (step > 6) step = 6;
      cur -= step;
    }
    if (cur < 0) cur = 0;
    if (cur > 64) cur = 64;
    bars[i] = (uint8_t) cur;

    // Peak dots (slower decay)
    if (cur >= (int) peaks[i]) {
      peaks[i] = (uint8_t) cur;
    } else if (decay_peaks && peaks[i] > 0 && peaks[i] > bars[i]) {
      peaks[i]--;
    }

    // ---- Slow AGC (4 min) using the normalized output level ----
    const float out_level = (float) cur / 64.0f;
    s_bar_avg[i] += BAR_AVG_ALPHA * (out_level - s_bar_avg[i]);

    const float err = BAR_TARGET_LEVEL - s_bar_avg[i];

    // Slight downward bias: if we're too "hot", pull down more eagerly than pushing up
    const float bias = (err < 0.0f) ? 1.45f : 0.85f;

    s_bar_gain[i] *= (1.0f + (BAR_AVG_ALPHA * bias * err));
    s_bar_gain[i] = clampf(s_bar_gain[i], 0.20f, 40.0f);
  }

  // Tie AGCs: enforce max/min <= 2.5
  float gmax = 0.0f;
  for (int i = 0; i < BAR_COUNT; i++) {
    if (s_bar_gain[i] > gmax) gmax = s_bar_gain[i];
  }
  if (gmax < 0.20f) gmax = 0.20f;

  const float gmin_allowed = gmax / GAIN_TIE_MAX_RATIO;
  for (int i = 0; i < BAR_COUNT; i++) {
    if (s_bar_gain[i] < gmin_allowed) s_bar_gain[i] = gmin_allowed;
  }

  if (decay_peaks) s_last_peak_decay_ms = now_ms;

  if (read_fail_count > (FFT_SAMPLES / 5)) {
    ESP_LOGW(TAG, "I2S read drops in frame: %d/%d", read_fail_count, FFT_SAMPLES);
  }
}

}  // namespace spectrum
