/**
 * SimpleGo - ui_manager.h
 * UI screen manager interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_SPLASH,
    UI_SCREEN_MAIN,
    UI_SCREEN_CHAT,
    UI_SCREEN_CONTACTS,
    UI_SCREEN_CONNECT,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_DEVELOPER,
    UI_SCREEN_NAME_SETUP,
    UI_SCREEN_LOCK,
    UI_SCREEN_COUNT
} ui_screen_t;

esp_err_t ui_manager_init(void);
void ui_manager_show_screen(ui_screen_t screen, lv_scr_load_anim_t anim);
ui_screen_t ui_manager_get_current(void);
void ui_manager_go_back(void);

/** Session 47 2d: Remove a specific screen from the navigation stack.
 *  Used when auto-navigating away from a screen that should not be
 *  revisitable via back button (e.g. QR screen after handshake starts). */
void ui_manager_remove_from_nav_stack(ui_screen_t screen);

/**
 * @brief SEC-04: Lock the device, wiping sensitive memory.
 * Securely zeros PSRAM message cache and LVGL labels, then
 * navigates to the lock screen. Called by inactivity timer.
 */
void ui_manager_lock(void);

/**
 * @brief SEC-04: Unlock the device, returning to previous screen.
 * Called from lock screen on any key press.
 */
void ui_manager_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_MANAGER_H */
