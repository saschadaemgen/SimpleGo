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
#include "freertos/timers.h"

static const char *TAG = "KBD";

#define I2C_PORT        I2C_NUM_0   // Same bus as touch (already initialized)
#define KBD_ADDR        0x55        // T-Deck keyboard I2C address

static bool initialized = false;
static lv_indev_t *indev = NULL;

/* Session 38h: Forward declaration for backlight */
void tdeck_kbd_backlight_notify_keypress(void);
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
            tdeck_kbd_backlight_notify_keypress();  /* Session 38h: reset auto-off timer */
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

/* ===== Session 38h: Keyboard Backlight ===== */

static TimerHandle_t bl_timer = NULL;
static uint8_t bl_current = 0;
static uint8_t bl_last_nonzero = 128;  /* Default 50% */

#define BL_TIMEOUT_MS   (2 * 60 * 1000)  /* 2 minutes auto-off */

static void bl_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    if (bl_current > 0) {
        uint8_t data[2] = { 0x01, 0x00 };
        i2c_master_write_to_device(I2C_PORT, KBD_ADDR, data, 2, pdMS_TO_TICKS(50));
        bl_last_nonzero = bl_current;
        bl_current = 0;
        ESP_LOGI(TAG, "Backlight auto-off");
    }
}

void tdeck_kbd_backlight_init(void)
{
    bl_timer = xTimerCreate("bl_off", pdMS_TO_TICKS(BL_TIMEOUT_MS),
                            pdFALSE, NULL, bl_timer_cb);
    if (!bl_timer) {
        ESP_LOGE(TAG, "Backlight timer create failed");
    }
}

void tdeck_kbd_backlight_set(uint8_t brightness)
{
    uint8_t data[2] = { 0x01, brightness };
    esp_err_t err = i2c_master_write_to_device(
        I2C_PORT, KBD_ADDR, data, 2, pdMS_TO_TICKS(50));
    if (err == ESP_OK) {
        if (brightness > 0 && bl_current == 0) {
            bl_last_nonzero = brightness;
        }
        bl_current = brightness;
        if (brightness > 0 && bl_timer) {
            xTimerReset(bl_timer, pdMS_TO_TICKS(50));
        } else if (bl_timer) {
            xTimerStop(bl_timer, pdMS_TO_TICKS(50));
        }
    } else {
        ESP_LOGW(TAG, "Backlight I2C failed: %s", esp_err_to_name(err));
    }
}

void tdeck_kbd_backlight_toggle(void)
{
    if (bl_current > 0) {
        bl_last_nonzero = bl_current;
        tdeck_kbd_backlight_set(0);
    } else {
        tdeck_kbd_backlight_set(bl_last_nonzero);
    }
}

void tdeck_kbd_backlight_notify_keypress(void)
{
    if (bl_current > 0 && bl_timer) {
        xTimerReset(bl_timer, 0);
    }
}

bool tdeck_kbd_backlight_is_on(void)
{
    return bl_current > 0;
}

uint8_t tdeck_kbd_backlight_get_current(void)
{
    return bl_current;
}
