/**
 * @file device_config.h
 * @brief LILYGO T-Deck Plus Device Configuration
 * 
 * Hardware Specifications:
 * - MCU: ESP32-S3 (N16R8 - 16MB Flash, 8MB PSRAM)
 * - Display: 2.8" IPS 320x240 ST7789V
 * - Touch: GT911 Capacitive
 * - Keyboard: 52-key QWERTY via I2C
 * - Trackball: via I2C (5 directions + click)
 * - Audio: MAX98357A I2S Speaker + Microphone
 * - GPS: L76K GNSS (optional)
 * - LoRa: SX1262 (optional)
 * - Battery: 3.7V LiPo with AXP2101 PMU
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

/*============================================================================
 * DEVICE IDENTIFICATION
 *==========================================================================*/

#define DEVICE_NAME             "T-Deck Plus"
#define DEVICE_ID               "t_deck_plus"
#define DEVICE_MANUFACTURER     "LILYGO"
#define DEVICE_MCU              "ESP32-S3"
#define DEVICE_VERSION          "1.0"

/*============================================================================
 * DISPLAY CONFIGURATION
 *==========================================================================*/

#define DISPLAY_TYPE            "ST7789V"
#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          240
#define DISPLAY_COLOR_DEPTH     16          // RGB565
#define DISPLAY_ROTATION        1           // Landscape
#define DISPLAY_INVERT          false

// SPI Pins
#define DISPLAY_SPI_HOST        SPI2_HOST
#define DISPLAY_PIN_MOSI        41
#define DISPLAY_PIN_SCLK        40
#define DISPLAY_PIN_CS          12
#define DISPLAY_PIN_DC          11
#define DISPLAY_PIN_RST         -1          // Connected to board reset
#define DISPLAY_PIN_BL          42

// SPI Configuration
#define DISPLAY_SPI_FREQ        40000000    // 40 MHz
#define DISPLAY_BL_PWM_CHANNEL  0
#define DISPLAY_BL_PWM_FREQ     5000

// DMA Buffer
#define DISPLAY_BUFFER_SIZE     (DISPLAY_WIDTH * 40)  // 40 lines
#define DISPLAY_USE_DMA         true
#define DISPLAY_DOUBLE_BUFFER   true

/*============================================================================
 * TOUCH CONFIGURATION
 *==========================================================================*/

#define TOUCH_ENABLED           true
#define TOUCH_TYPE              "GT911"
#define TOUCH_I2C_PORT          I2C_NUM_0
#define TOUCH_I2C_ADDR          0x5D
#define TOUCH_PIN_INT           16
#define TOUCH_PIN_RST           -1          // Shared with display
#define TOUCH_WIDTH             DISPLAY_WIDTH
#define TOUCH_HEIGHT            DISPLAY_HEIGHT
#define TOUCH_SWAP_XY           false
#define TOUCH_INVERT_X          false
#define TOUCH_INVERT_Y          false

/*============================================================================
 * KEYBOARD CONFIGURATION
 *==========================================================================*/

#define KEYBOARD_ENABLED        true
#define KEYBOARD_TYPE           "T-DECK-KB"
#define KEYBOARD_I2C_PORT       I2C_NUM_0
#define KEYBOARD_I2C_ADDR       0x55
#define KEYBOARD_PIN_INT        46
#define KEYBOARD_LAYOUT         "QWERTY"
#define KEYBOARD_BACKLIGHT      true
#define KEYBOARD_BL_PIN         -1          // Controlled via I2C

/*============================================================================
 * TRACKBALL CONFIGURATION
 *==========================================================================*/

#define TRACKBALL_ENABLED       true
#define TRACKBALL_I2C_PORT      I2C_NUM_0
#define TRACKBALL_I2C_ADDR      0x55        // Same as keyboard
#define TRACKBALL_PIN_INT       -1          // Shared with keyboard
#define TRACKBALL_SENSITIVITY   5           // 1-10

/*============================================================================
 * I2C CONFIGURATION
 *==========================================================================*/

#define I2C_PORT_0_ENABLED      true
#define I2C_PORT_0_PIN_SDA      18
#define I2C_PORT_0_PIN_SCL      8
#define I2C_PORT_0_FREQ         400000      // 400 kHz

#define I2C_PORT_1_ENABLED      false

/*============================================================================
 * AUDIO CONFIGURATION
 *==========================================================================*/

#define AUDIO_ENABLED           true
#define AUDIO_TYPE              "MAX98357A"
#define AUDIO_SAMPLE_RATE       16000
#define AUDIO_BITS              16
#define AUDIO_CHANNELS          1           // Mono

// I2S Pins
#define AUDIO_I2S_PORT          I2S_NUM_0
#define AUDIO_PIN_BCLK          7
#define AUDIO_PIN_LRCK          5
#define AUDIO_PIN_DOUT          6
#define AUDIO_PIN_DIN           -1          // No mic input on standard model
#define AUDIO_PIN_SD_MODE       -1          // No shutdown pin

// Microphone (if available)
#define AUDIO_MIC_ENABLED       false
#define AUDIO_MIC_PIN_DATA      -1
#define AUDIO_MIC_PIN_CLK       -1

/*============================================================================
 * POWER MANAGEMENT CONFIGURATION
 *==========================================================================*/

#define PMU_ENABLED             true
#define PMU_TYPE                "AXP2101"
#define PMU_I2C_PORT            I2C_NUM_0
#define PMU_I2C_ADDR            0x34
#define PMU_PIN_INT             -1

// Battery
#define BATTERY_ENABLED         true
#define BATTERY_CAPACITY_MAH    1500
#define BATTERY_LOW_THRESHOLD   15          // % to trigger warning
#define BATTERY_CRITICAL        5           // % to force sleep

/*============================================================================
 * LORA CONFIGURATION (OPTIONAL)
 *==========================================================================*/

#define LORA_ENABLED            false       // Set to true if using LoRa model
#define LORA_TYPE               "SX1262"
#define LORA_SPI_HOST           SPI3_HOST
#define LORA_PIN_MOSI           35
#define LORA_PIN_MISO           37
#define LORA_PIN_SCK            36
#define LORA_PIN_CS             9
#define LORA_PIN_RST            17
#define LORA_PIN_DIO1           45
#define LORA_PIN_BUSY           13

/*============================================================================
 * GPS CONFIGURATION (OPTIONAL)
 *==========================================================================*/

#define GPS_ENABLED             false       // Set to true if using GPS model
#define GPS_TYPE                "L76K"
#define GPS_UART_PORT           UART_NUM_1
#define GPS_PIN_TX              43
#define GPS_PIN_RX              44
#define GPS_BAUD_RATE           9600

/*============================================================================
 * SD CARD CONFIGURATION
 *==========================================================================*/

#define SD_CARD_ENABLED         true
#define SD_CARD_MODE            "SPI"       // SPI or SDMMC
#define SD_SPI_HOST             SPI3_HOST
#define SD_PIN_MOSI             35
#define SD_PIN_MISO             37
#define SD_PIN_SCK              36
#define SD_PIN_CS               39

/*============================================================================
 * WIFI/BLUETOOTH
 *==========================================================================*/

#define WIFI_ENABLED            true
#define BT_ENABLED              true
#define BT_BLE_ENABLED          true
#define BT_CLASSIC_ENABLED      false

/*============================================================================
 * MISC GPIO
 *==========================================================================*/

#define GPIO_BOOT_BUTTON        0           // BOOT button
#define GPIO_LED                -1          // No dedicated LED

/*============================================================================
 * FEATURE FLAGS
 *==========================================================================*/

#define FEATURE_KEYBOARD        1
#define FEATURE_TRACKBALL       1
#define FEATURE_TOUCH           1
#define FEATURE_AUDIO           1
#define FEATURE_BATTERY         1
#define FEATURE_SD_CARD         1
#define FEATURE_LORA            0
#define FEATURE_GPS             0
#define FEATURE_BLE             1

/*============================================================================
 * UI CONFIGURATION
 *==========================================================================*/

#define UI_THEME                "dark"
#define UI_FONT_DEFAULT         "lv_font_montserrat_14"
#define UI_FONT_LARGE           "lv_font_montserrat_20"
#define UI_FONT_SMALL           "lv_font_montserrat_10"
#define UI_ANIMATION_SPEED      200         // ms
#define UI_SCROLL_SPEED         10

/*============================================================================
 * LVGL BUFFER CONFIGURATION
 *==========================================================================*/

#define LVGL_TICK_PERIOD_MS     2
#define LVGL_BUFFER_LINES       40
#define LVGL_USE_PSRAM          true
#define LVGL_DOUBLE_BUFFER      true

/*============================================================================
 * INPUT PRIORITIES
 *==========================================================================*/

// Primary input method for navigation
#define INPUT_PRIMARY           "KEYBOARD"  // KEYBOARD, TOUCH, ENCODER
#define INPUT_SECONDARY         "TRACKBALL"

#endif /* DEVICE_CONFIG_H */
