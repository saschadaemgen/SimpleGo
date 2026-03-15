/**
 * @file ui_settings.h
 * @brief Settings Screen - Tabbed layout (BRIGHT / WIFI / INFO)
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the settings screen with tabbed interface
 *        Tabs: BRIGHT | WIFI | INFO
 * @return LVGL screen object
 */
lv_obj_t *ui_settings_create(void);

/**
 * @brief Force WiFi tab to be shown (for First Boot flow)
 */
void ui_settings_show_wifi_tab(void);

/**
 * @brief Get active tab index (0=BRIGHT, 1=WIFI, 2=INFO)
 *        Bug #24b: Used by ui_manager to save/restore tab on lock/unlock.
 */
int ui_settings_get_active_tab(void);

/**
 * @brief Restore a specific tab by index (0=BRIGHT, 1=WIFI, 2=INFO)
 *        Bug #24b: Used by ui_manager to restore tab after unlock.
 */
void ui_settings_show_tab(int tab);

/**
 * @brief Cleanup all settings timers before screen deletion.
 *        Must be called by ui_manager before destroying the settings screen,
 *        otherwise timer callbacks access deleted LVGL objects (freeze/crash).
 */
void ui_settings_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_H */
