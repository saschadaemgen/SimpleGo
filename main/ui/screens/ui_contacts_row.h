/**
 * @file ui_contacts_row.h
 * @brief Contact Row Rendering API
 *
 * Session 39f: Extracted from ui_contacts.c
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#ifndef UI_CONTACTS_ROW_H
#define UI_CONTACTS_ROW_H

#include "lvgl.h"
#include "smp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a contact row with 6 LVGL objects:
 *   1. Accent bar  2. Name  3. Checkmark  4. Count  5. E2EE
 */
lv_obj_t *ui_contacts_row_create(lv_obj_t *parent, int idx, contact_t *c,
                                  uint16_t total, uint16_t unread,
                                  lv_event_cb_t on_click,
                                  lv_event_cb_t on_long_press);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONTACTS_ROW_H */
