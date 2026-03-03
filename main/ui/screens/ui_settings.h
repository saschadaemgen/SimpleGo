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

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_H */
