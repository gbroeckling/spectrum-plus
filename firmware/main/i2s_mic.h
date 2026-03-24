// i2s_mic.h — Onboard MEMS mic via ES8311 codec on Waveshare ESP32-P4 dev kit
//
// Audio path:  onboard SMD mic → ES8311 ADC → I2S → ESP32-P4
// The ES8311 is configured over I2C, then audio streams in via I2S.
#pragma once

#include <cstdint>
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "es8311.h"
#include "pin_config.h"

namespace mic {

static const char *TAG = "mic";

static i2s_chan_handle_t    s_rx_chan  = nullptr;
static i2c_master_bus_handle_t s_i2c_bus = nullptr;
static es8311_handle_t      s_codec   = nullptr;
static bool                 s_ready   = false;

// ─── I2C bus for codec control ───────────────────────────────────
static bool init_i2c() {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.i2c_port   = I2C_NUM_0;
    bus_cfg.scl_io_num = CODEC_I2C_SCL;
    bus_cfg.sda_io_num = CODEC_I2C_SDA;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// ─── ES8311 codec setup ──────────────────────────────────────────
static bool init_codec(uint32_t sample_rate) {
    es8311_handle_t codec = es8311_create(s_i2c_bus, CODEC_I2C_ADDR);
    if (!codec) {
        ESP_LOGE(TAG, "ES8311 create failed");
        return false;
    }

    es8311_clock_config_t clk_cfg = {};
    clk_cfg.mclk_inverted  = false;
    clk_cfg.sclk_inverted  = false;
    clk_cfg.mclk_from_mclk_pin = true;

    esp_err_t err = es8311_init(codec, &clk_cfg, ES8311_RESOLUTION_24, sample_rate);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 init failed: %s", esp_err_to_name(err));
        es8311_delete(codec);
        return false;
    }

    // Enable mic input (onboard MEMS mic)
    es8311_microphone_config(codec, false);  // single-ended mic input
    es8311_microphone_gain_set(codec, ES8311_MIC_GAIN_42DB);

    s_codec = codec;
    ESP_LOGI(TAG, "ES8311 codec initialized at %lu Hz", (unsigned long)sample_rate);
    return true;
}

// ─── I2S channel (RX from codec) ────────────────────────────────
static bool init_i2s(uint32_t sample_rate) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = 8;
    chan_cfg.dma_frame_num = 128;

    // We need both TX and RX on the same I2S port for ES8311 (codec expects MCLK/BCLK/WS)
    // but we only read — TX channel can be idle.
    i2s_chan_handle_t tx_chan = nullptr;
    esp_err_t err = i2s_new_channel(&chan_cfg, &tx_chan, &s_rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return false;
    }

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT,
                                                            I2S_SLOT_MODE_MONO);

    std_cfg.gpio_cfg.mclk = CODEC_I2S_MCLK;
    std_cfg.gpio_cfg.bclk = CODEC_I2S_SCLK;
    std_cfg.gpio_cfg.ws   = CODEC_I2S_LRCK;
    std_cfg.gpio_cfg.dout = CODEC_I2S_DOUT;
    std_cfg.gpio_cfg.din  = CODEC_I2S_DIN;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv   = false;

    // Init both channels with the same config (shared clock lines)
    err = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s TX init failed: %s", esp_err_to_name(err));
        return false;
    }
    err = i2s_channel_init_std_mode(s_rx_chan, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s RX init failed: %s", esp_err_to_name(err));
        return false;
    }

    // Enable TX first (provides MCLK/BCLK to codec), then RX
    i2s_channel_enable(tx_chan);
    i2s_channel_enable(s_rx_chan);

    ESP_LOGI(TAG, "I2S initialized: MCLK=%d SCLK=%d LRCK=%d DIN=%d",
             (int)CODEC_I2S_MCLK, (int)CODEC_I2S_SCLK,
             (int)CODEC_I2S_LRCK, (int)CODEC_I2S_DIN);
    return true;
}

// ─── Public API ──────────────────────────────────────────────────

inline bool init(uint32_t sample_rate = 44100) {
    if (!init_i2c())               return false;
    if (!init_codec(sample_rate))  return false;
    if (!init_i2s(sample_rate))    return false;

    s_ready = true;
    ESP_LOGI(TAG, "Onboard mic ready via ES8311 codec");
    return true;
}

// Read one sample, return as float in [-1, 1]
inline bool read_sample(float &out) {
    if (!s_ready) { out = 0.0f; return false; }

    int32_t raw = 0;
    size_t  bytes_read = 0;

    esp_err_t err = i2s_channel_read(s_rx_chan, &raw, sizeof(raw), &bytes_read, pdMS_TO_TICKS(20));
    if (err != ESP_OK || bytes_read != sizeof(raw)) {
        out = 0.0f;
        return false;
    }

    // ES8311 outputs 24-bit audio, left-justified in 32-bit frame
    int32_t s24 = raw >> 8;
    out = (float)s24 / 8388608.0f;
    return true;
}

inline bool is_ready() { return s_ready; }

}  // namespace mic
