// main.cpp — Spectrum Plus: 128-bar audio spectrum analyzer
//
// ESP32-P4  •  2× HUB75 128×64 panels daisy-chained (256×64)  •  INMP441 mic
// Pure ESP-IDF, no Home Assistant / ESPHome.

#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "pin_config.h"
#include "i2s_mic.h"
#include "spectrum_dsp.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

static const char *TAG = "main";

// ─── Shared state ────────────────────────────────────────────────
static std::array<uint8_t, spectrum::BAR_COUNT> g_bars{};
static std::array<uint8_t, spectrum::BAR_COUNT> g_peaks{};
static volatile bool g_boot_anim   = true;
static volatile uint32_t g_boot_end = 0;

// ─── Display ─────────────────────────────────────────────────────
static MatrixPanel_I2S_DMA *dma_display = nullptr;

static void init_display() {
    HUB75_I2S_CFG::i2s_pins pins = {
        HUB75_R1_PIN, HUB75_G1_PIN, HUB75_B1_PIN,
        HUB75_R2_PIN, HUB75_G2_PIN, HUB75_B2_PIN,
        HUB75_A_PIN,  HUB75_B_PIN,  HUB75_C_PIN,
        HUB75_D_PIN,  HUB75_E_PIN,
        HUB75_LAT_PIN, HUB75_OE_PIN, HUB75_CLK_PIN
    };

    HUB75_I2S_CFG mxconfig(
        128,            // single panel width
        64,             // single panel height
        2,              // chain length (2 panels)
        pins,           // GPIO pins
        HUB75_I2S_CFG::FM6126A  // shift driver
    );
    mxconfig.clkphase    = false;
    mxconfig.latch_blanking = 1;

    dma_display = new MatrixPanel_I2S_DMA(mxconfig);

    if (!dma_display->begin()) {
        ESP_LOGE(TAG, "HUB75 DMA display init FAILED");
        return;
    }

    dma_display->setBrightness8(96);
    dma_display->clearScreen();
    ESP_LOGI(TAG, "HUB75 display ready: %dx%d (2 panels chained)", 256, 64);
}

// ─── Color helpers (same gradient as original) ───────────────────
static inline uint8_t lerp_u8(uint8_t a, uint8_t b, float u) {
    u = std::max(0.0f, std::min(1.0f, u));
    return (uint8_t)(a + (b - a) * u + 0.5f);
}

// Base gradient: Red → Orange → Yellow → Green → Cyan → Blue
static uint16_t base_color_for_bar(int i, int n_bars) {
    const float t = (n_bars <= 1) ? 0.0f : ((float)i / (float)(n_bars - 1));
    const float p = t * 5.0f;
    int   seg = (int)p;
    float u   = p - (float)seg;
    if (seg < 0) { seg = 0; u = 0.0f; }
    if (seg > 4) { seg = 4; u = 1.0f; }

    static const uint8_t R[6] = {255, 255, 255,   0,   0,   0};
    static const uint8_t G[6] = {  0, 128, 255, 255, 255,   0};
    static const uint8_t B[6] = {  0,   0,   0,   0, 255, 255};

    uint8_t r = lerp_u8(R[seg], R[seg+1], u);
    uint8_t g = lerp_u8(G[seg], G[seg+1], u);
    uint8_t b = lerp_u8(B[seg], B[seg+1], u);
    return dma_display->color565(r, g, b);
}

// Full color computation for a pixel (with bottom fade + purple shift)
static uint16_t pixel_color(int bar_idx, int yy, int bh, int n_bars, int h) {
    const float t = (n_bars <= 1) ? 0.0f : ((float)bar_idx / (float)(n_bars - 1));
    const float p = t * 5.0f;
    int   seg = (int)p;
    float u   = p - (float)seg;
    if (seg < 0) { seg = 0; u = 0.0f; }
    if (seg > 4) { seg = 4; u = 1.0f; }

    static const uint8_t R[6] = {255, 255, 255,   0,   0,   0};
    static const uint8_t G[6] = {  0, 128, 255, 255, 255,   0};
    static const uint8_t B[6] = {  0,   0,   0,   0, 255, 255};

    uint8_t br = lerp_u8(R[seg], R[seg+1], u);
    uint8_t bg = lerp_u8(G[seg], G[seg+1], u);
    uint8_t bb = lerp_u8(B[seg], B[seg+1], u);

    // Bottom fade
    const float strength = (float)bh / (float)h;
    const int fade = std::min(bh, 6 + (int)((float)bh * 64.0f / (float)h));
    float min_scale = (0.20f - 0.12f * strength) / 8.0f;
    if (min_scale < 0.015f) min_scale = 0.015f;

    float s = 1.0f;
    if (yy < fade) {
        const float fu = (fade <= 1) ? 1.0f : ((float)yy / (float)(fade - 1));
        s = min_scale + (1.0f - min_scale) * fu;
    }
    if (s < 0.09f) return 0;   // skip very dim pixels

    // Purple shift near top of bar
    const float ubar = (bh <= 1) ? 1.0f : ((float)yy / (float)(bh - 1));
    float pmix = (ubar - 0.70f) / 0.30f;
    pmix = std::max(0.0f, std::min(1.0f, pmix));
    pmix = pmix * pmix * strength;

    uint8_t r0 = (uint8_t)((int)br * s);
    uint8_t g0 = (uint8_t)((int)bg * s);
    uint8_t b0 = (uint8_t)((int)bb * s);

    uint8_t pr = (uint8_t)(255.0f * s);
    uint8_t pg = 0;
    uint8_t pb = (uint8_t)(255.0f * s);

    uint8_t r = lerp_u8(r0, pr, pmix);
    uint8_t g = lerp_u8(g0, pg, pmix);
    uint8_t b = lerp_u8(b0, pb, pmix);

    return dma_display->color565(r, g, b);
}

// ─── Render one frame ────────────────────────────────────────────
static void render_frame() {
    if (!dma_display) return;

    const int w      = 256;
    const int h      = 64;
    const int n_bars = spectrum::BAR_COUNT;             // 128
    const int bar_w  = std::max(1, w / n_bars);        // 2
    const int draw_w = g_boot_anim ? bar_w : std::max(1, bar_w - 1);  // 1px gap normally

    dma_display->clearScreen();

    for (int i = 0; i < n_bars; i++) {
        int bh = (int)g_bars[i];
        int pk = (int)g_peaks[i];
        if (bh < 0) bh = 0; if (bh > h) bh = h;
        if (pk < 0) pk = 0; if (pk > h) pk = h;

        const int x = i * bar_w;
        if (x >= w) break;

        int ww = draw_w;
        if (x + ww > w) ww = w - x;
        if (ww <= 0) continue;

        // Draw bar column
        if (bh > 0) {
            for (int yy = 0; yy < bh; yy++) {
                const int y = h - 1 - yy;
                uint16_t col = pixel_color(i, yy, bh, n_bars, h);
                if (col == 0) continue;
                for (int dx = 0; dx < ww; dx++)
                    dma_display->drawPixel(x + dx, y, col);
            }
        }

        // Peak dot (white)
        if (pk > 0) {
            int py = h - pk - 1;
            if (py < 0) py = 0;
            uint16_t white = dma_display->color565(255, 255, 255);
            for (int dx = 0; dx < ww; dx++)
                dma_display->drawPixel(x + dx, py, white);
        }
    }

    // Status pixel: red = boot anim, green = running
    uint16_t status = g_boot_anim
        ? dma_display->color565(255, 0, 0)
        : dma_display->color565(0, 255, 0);
    dma_display->drawPixel(w - 2, 0, status);
    dma_display->drawPixel(w - 1, 0, status);
    dma_display->drawPixel(w - 2, 1, status);
    dma_display->drawPixel(w - 1, 1, status);
}

// ─── FreeRTOS tasks ──────────────────────────────────────────────

// Core 0: FFT sampling + DSP (40 ms interval)
static void fft_task(void *) {
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        if (g_boot_anim) {
            if ((int32_t)(g_boot_end - esp_log_timestamp()) > 0) {
                spectrum::boot_animation_update(g_bars, g_peaks);
            } else {
                g_boot_anim = false;
            }
        } else {
            spectrum::fft_update(g_bars, g_peaks);
        }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(40));
    }
}

// Core 1: Display rendering (16 ms ≈ 60 fps)
static void render_task(void *) {
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        render_frame();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(16));
    }
}

// ─── Entry point ─────────────────────────────────────────────────
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Spectrum Plus starting — 128 bars, 256×64 display");

    // Boot animation for 6 seconds
    g_boot_anim = true;
    g_boot_end  = esp_log_timestamp() + 6000;

    // Init subsystems
    init_display();
    mic::init((uint32_t)spectrum::SAMPLE_RATE);
    spectrum::init();

    // Launch tasks pinned to separate cores
    xTaskCreatePinnedToCore(fft_task,    "fft",    8192, nullptr, 5, nullptr, 0);
    xTaskCreatePinnedToCore(render_task, "render", 8192, nullptr, 4, nullptr, 1);

    ESP_LOGI(TAG, "Tasks launched — FFT on core 0, render on core 1");
}
