/**
 * @file tdeck_keyboard.h
 * @brief T-Deck Plus Keyboard Driver (I2C @ 0x55)
 *
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef TDECK_KEYBOARD_H
#define TDECK_KEYBOARD_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize keyboard (I2C must already be initialized by touch driver)
 * @return ESP_OK on success
 */
esp_err_t tdeck_keyboard_init(void);

/**
 * @brief Read last pressed key (non-blocking)
 * @return ASCII character, or 0 if no key pressed
 */
char tdeck_keyboard_read(void);

/**
 * @brief Register keyboard as LVGL input device
 * @return LVGL input device pointer, or NULL on failure
 */
lv_indev_t *tdeck_keyboard_register_lvgl(void);

/**
 * @brief Get LVGL input device (after registration)
 * @return LVGL input device pointer, or NULL if not registered
 */
lv_indev_t *tdeck_keyboard_get_indev(void);

#ifdef __cplusplus
}
#endif

#endif /* TDECK_KEYBOARD_H */
