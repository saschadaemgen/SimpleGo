/**
 * @file ui_contacts_popup.h
 * @brief Contact Popup API - Delete, Info, Actions
 *
 * Session 39f: Extracted from ui_contacts.c
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#ifndef UI_CONTACTS_POPUP_H
#define UI_CONTACTS_POPUP_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_contacts_popup_refresh_cb_t)(void);

/** Set callback for refresh after delete */
void ui_contacts_popup_set_refresh_cb(ui_contacts_popup_refresh_cb_t cb);

/** Show delete/info popup for contact */
void ui_contacts_popup_show(lv_obj_t *screen, int contact_idx);

/** Show detailed info popup */
void ui_contacts_popup_show_info(lv_obj_t *screen, int idx);

/** Close any open popup */
void ui_contacts_popup_close(void);

/** Check if popup is currently visible */
bool ui_contacts_popup_active(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONTACTS_POPUP_H */
