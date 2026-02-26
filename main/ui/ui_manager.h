/**
 * @file ui_manager.h
 * @brief UI Screen Manager - Controls screen navigation
 * 
 * SimpleGo UI Architecture
 * Copyright (c) 2025-2026 Sascha
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
    UI_SCREEN_COUNT
} ui_screen_t;

esp_err_t ui_manager_init(void);
void ui_manager_show_screen(ui_screen_t screen, lv_scr_load_anim_t anim);
ui_screen_t ui_manager_get_current(void);
void ui_manager_go_back(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_MANAGER_H */
