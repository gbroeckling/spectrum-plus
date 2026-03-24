// pin_config.h — GPIO assignments for Waveshare ESP32-P4 Module Dev Kit (SKU:30560)
//
// *** VERIFY THESE AGAINST YOUR BOARD'S ACTUAL PINOUT / 40-PIN HEADER ***
// All pins are grouped here so you only need to edit this one file.
//
// ═══ GPIOs RESERVED by onboard hardware — DO NOT reassign ════════
//
//   Audio codec (ES8311 + NS4150B):
//     GPIO7  = I2C SDA          GPIO8  = I2C SCL
//     GPIO9  = I2S DIN          GPIO10 = I2S LRCK
//     GPIO11 = I2S DOUT         GPIO12 = I2S SCLK
//     GPIO13 = I2S MCLK         GPIO53 = PA_Ctrl
//
//   Ethernet (RMII):
//     GPIO28 = CRS_DV           GPIO29 = RXD1
//     GPIO30 = RXD0             GPIO31 = MDC
//     GPIO34 = TXD1             GPIO35 = TXD0
//     GPIO49 = TX_EN            GPIO50 = REF_CLK
//     GPIO51 = RESET            GPIO52 = MDIO
//
//   SD card (SDIO 3.0):
//     GPIO39 = D0               GPIO40 = D1
//     GPIO41 = D2               GPIO42 = D3
//     GPIO43 = CLK              GPIO44 = CMD
//
//   Misc:
//     GPIO0  = BOOT button (typically)
//     GPIO26-27 = possibly MIPI CSI/DSI (verify on schematic)
//
// ═══ SAFE GPIOs for user peripherals (HUB75) ════════════════════
//   GPIO 1-6, 14-25, 32-33, 36-38, 45-48, 54
//   (Some of these may be on the 40-pin header — check your board)
//
#pragma once

#include "driver/gpio.h"

// ─── Onboard ES8311 codec + MEMS microphone (fixed by PCB) ──────
#define CODEC_I2C_SDA   GPIO_NUM_7
#define CODEC_I2C_SCL   GPIO_NUM_8
#define CODEC_I2S_MCLK  GPIO_NUM_13
#define CODEC_I2S_SCLK  GPIO_NUM_12
#define CODEC_I2S_LRCK  GPIO_NUM_10
#define CODEC_I2S_DIN   GPIO_NUM_9    // mic data INTO the ESP32
#define CODEC_I2S_DOUT  GPIO_NUM_11   // audio data OUT (to speaker amp)
#define CODEC_PA_CTRL   GPIO_NUM_53   // NS4150B power amp enable

#define CODEC_I2C_ADDR  0x18          // ES8311 default address

// ─── HUB75 RGB Panel (2× 128×64 daisy-chained = 256×64) ─────────
// 14 GPIOs needed.  Chosen from the safe range, avoiding audio,
// Ethernet, SD card, and MIPI pins.
//
// RGB data lines (upper/lower half of panel)
#define HUB75_R1_PIN   GPIO_NUM_1
#define HUB75_G1_PIN   GPIO_NUM_2
#define HUB75_B1_PIN   GPIO_NUM_3
#define HUB75_R2_PIN   GPIO_NUM_4
#define HUB75_G2_PIN   GPIO_NUM_5
#define HUB75_B2_PIN   GPIO_NUM_6

// Row address lines (A-E for 1/32 scan, 64-row panel)
#define HUB75_A_PIN    GPIO_NUM_14
#define HUB75_B_PIN    GPIO_NUM_15
#define HUB75_C_PIN    GPIO_NUM_16
#define HUB75_D_PIN    GPIO_NUM_17
#define HUB75_E_PIN    GPIO_NUM_18

// Control
#define HUB75_CLK_PIN  GPIO_NUM_19
#define HUB75_LAT_PIN  GPIO_NUM_20
#define HUB75_OE_PIN   GPIO_NUM_21
