/**
 * @file ui_lock.h
 * @brief Lock Screen - SEC-04 Phase 1 (any key to unlock)
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_LOCK_H
#define UI_LOCK_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the lock screen
 * @return LVGL screen object
 *
 * Phase 1: Any key press unlocks. No PIN required.
 * Keyboard indev is reassigned to an invisible textarea
 * so that any physical key event triggers unlock.
 */
lv_obj_t *ui_lock_create(void);

/**
 * @brief Clean up lock screen state before deletion
 */
void ui_lock_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_LOCK_H */
