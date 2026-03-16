/**
 * @file ui_screensaver.h
 * @brief Matrix Rain Screensaver - SEC-04 Display Lock (Matrix Mode)
 *
 * Alternative to the simple lock screen. Selected via Settings:
 * "Display Lock: Simple / Matrix". Security is identical -
 * all decrypted data has been wiped BEFORE this screen appears.
 * Any physical key press stops animation and unlocks.
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_SCREENSAVER_H
#define UI_SCREENSAVER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the Matrix Rain screensaver screen
 * @return LVGL screen object
 *
 * Creates a fullscreen LVGL Canvas with animated Matrix Rain
 * effect using an embedded 8x8 bitmap font. Three neon color
 * palettes (green, cyan, yellow) are distributed across columns.
 * Canvas buffer is allocated in PSRAM (~153 KB for RGB565).
 *
 * Any physical key press triggers unlock via ui_manager_unlock().
 * SEC-04 memory wipe must have completed BEFORE calling this.
 */
lv_obj_t *ui_screensaver_create(void);

/**
 * @brief Clean up screensaver state and free all resources
 *
 * Stops the animation timer, frees the PSRAM canvas buffer,
 * deletes the keyboard input group. Must be called before
 * the screen object is deleted.
 */
void ui_screensaver_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREENSAVER_H */
