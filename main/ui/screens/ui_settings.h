/**
 * @file ui_settings.h
 * @brief Settings Screen - Display and Keyboard brightness sliders
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
 * @brief Create the settings screen with two sliders
 * @return LVGL screen object
 */
lv_obj_t *ui_settings_create(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_SETTINGS_H */
