/**
 * @file tdeck_keyboard.c
 * @brief T-Deck Plus Keyboard Driver (I2C @ 0x55)
 *
 * The T-Deck Plus has a BlackBerry-style physical QWERTY keyboard
 * connected via I2C. Reading 1 byte from address 0x55 returns the
 * ASCII code of the last pressed key (0 if none).
 *
 * Special keys:
 *   Enter     = 0x0D ('\r')
 *   Backspace = 0x08
 *   Space     = 0x20
 *
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "tdeck_keyboard.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "KBD";

#define I2C_PORT        I2C_NUM_0   // Same bus as touch (already initialized)
#define KBD_ADDR        0x55        // T-Deck keyboard I2C address

static bool initialized = false;
static lv_indev_t *indev = NULL;
static char last_key = 0;
static bool key_pressed = false;

// ============== I2C Communication ==============

static bool kbd_probe(void)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (KBD_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static char kbd_read_byte(void)
{
    uint8_t data = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (KBD_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) return 0;
    return (char)data;
}

// ============== Public API ==============

esp_err_t tdeck_keyboard_init(void)
{
    if (initialized) return ESP_OK;
    
    ESP_LOGI(TAG, "Initializing T-Deck keyboard...");
    
    // I2C bus already initialized by touch driver — just probe
    if (!kbd_probe()) {
        ESP_LOGE(TAG, "Keyboard not found at 0x%02X!", KBD_ADDR);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Keyboard found at 0x%02X ✅", KBD_ADDR);
    initialized = true;
    return ESP_OK;
}

char tdeck_keyboard_read(void)
{
    if (!initialized) return 0;
    return kbd_read_byte();
}

// ============== LVGL Integration ==============

/**
 * Map T-Deck ASCII to LVGL key codes.
 * Regular printable ASCII passes through directly.
 * Special keys get mapped to LV_KEY_* constants.
 */
static uint32_t ascii_to_lvgl_key(char c)
{
    switch (c) {
        case 0x0D:  // Enter ('\r')
        case 0x0A:  // Enter ('\n')
            return LV_KEY_ENTER;
        case 0x08:  // Backspace
            return LV_KEY_BACKSPACE;
        case 0x1B:  // ESC
            return LV_KEY_ESC;
        case 0x09:  // Tab
            return LV_KEY_NEXT;
        default:
            if (c >= 0x20 && c < 0x7F) {
                return (uint32_t)c;  // Printable ASCII
            }
            return 0;
    }
}

static void kbd_read_cb(lv_indev_t *drv, lv_indev_data_t *data)
{
    char c = kbd_read_byte();
    
    if (c != 0) {
        uint32_t lvgl_key = ascii_to_lvgl_key(c);
        if (lvgl_key != 0) {
            last_key = c;
            key_pressed = true;
            data->key = lvgl_key;
            data->state = LV_INDEV_STATE_PRESSED;
            return;
        }
    }
    
    // No new key — release previous
    if (key_pressed) {
        data->key = ascii_to_lvgl_key(last_key);
        data->state = LV_INDEV_STATE_RELEASED;
        key_pressed = false;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

lv_indev_t *tdeck_keyboard_register_lvgl(void)
{
    if (!initialized) return NULL;
    if (indev) return indev;
    
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, kbd_read_cb);
    
    ESP_LOGI(TAG, "Keyboard registered with LVGL ⌨️");
    return indev;
}

lv_indev_t *tdeck_keyboard_get_indev(void)
{
    return indev;
}
