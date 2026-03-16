/**
 * @file ui_settings_internal.h
 * @brief Settings Screen - Internal shared header for tab modules
 *
 * Session 39c: Split from monolithic ui_settings.c (1240 lines → 5 files).
 * This header is INTERNAL — only included by ui_settings*.c files.
 * The public API remains in ui_settings.h (unchanged).
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_SETTINGS_INTERNAL_H
#define UI_SETTINGS_INTERNAL_H

#include "ui_settings.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"
#include "tdeck_backlight.h"
#include "tdeck_keyboard.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Layout Constants (Session 47: Combined Bar like ui_main.c)
 * ================================================================ */

#define MAIN_BAR_H      26                         /* Combined bar height */
#define GLOW_Y          MAIN_BAR_H                 /* 26 */
#define CONTENT_Y       (MAIN_BAR_H + 1)           /* 27 */
#define BAR_H           36
#define BAR_Y           (UI_SCREEN_H - BAR_H)      /* 204 */
#define CONTENT_H       (BAR_Y - CONTENT_Y)        /* 177 */

/* Bottom bar: 4 equal buttons */
#define BAR_BTN_W       (UI_SCREEN_W / 4)          /* 80 */

/* ================================================================
 * Tab Enum
 * ================================================================ */

typedef enum {
    TAB_BRIGHT = 0,
    TAB_WIFI   = 1,
    TAB_INFO   = 2
} settings_tab_t;

/* ================================================================
 * Shared State (owned by ui_settings.c)
 * ================================================================ */

extern settings_tab_t active_tab;
extern lv_obj_t *content_area;
extern lv_obj_t *s_settings_scr;

/* Header dynamic elements */
extern lv_obj_t *hdr_title_lbl;
extern lv_obj_t *hdr_action_btn;
extern lv_obj_t *hdr_action_lbl;

/* ================================================================
 * Style Helpers (defined in ui_settings.c, used by all tabs)
 * ================================================================ */

void settings_style_black(lv_obj_t *obj);

/* ================================================================
 * Tab Content Interfaces
 *
 * Each tab module exports exactly 3 functions:
 *   create_content_XXX(parent)  — Build the tab UI
 *   cleanup_XXX_timers()        — Delete LVGL timers
 *   nullify_XXX_pointers()      — Reset stale widget pointers
 * ================================================================ */

/* BRIGHT tab (ui_settings_bright.c) */
void settings_create_bright(lv_obj_t *parent);
void settings_cleanup_bright_timers(void);
void settings_nullify_bright_pointers(void);

/* WIFI tab (ui_settings_wifi.c) */
void settings_create_wifi(lv_obj_t *parent);
void settings_cleanup_wifi_timers(void);
void settings_nullify_wifi_pointers(void);
void settings_update_wifi_header(void);
void settings_on_wifi_hdr_action(void);

/* INFO tab (ui_settings_info.c) */
void settings_create_info(lv_obj_t *parent);
void settings_cleanup_info_timers(void);
void settings_nullify_info_pointers(void);
void settings_info_refresh(void);

/* ================================================================
 * Controller functions (ui_settings.c, called by tab modules)
 * ================================================================ */

void settings_switch_tab(settings_tab_t tab);
void settings_update_header_for_tab(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_INTERNAL_H */
