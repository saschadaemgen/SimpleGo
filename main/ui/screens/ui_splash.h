/**
 * SimpleGo - ui_splash.h
 * Splash screen interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_SPLASH_H
#define UI_SPLASH_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_splash_create(void);
void ui_splash_set_status(const char *status);
void ui_splash_set_progress(int percent);

#ifdef __cplusplus
}
#endif

#endif /* UI_SPLASH_H */
