/**
 * @file ui_contacts.h
 * @brief Contacts Screen
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#ifndef UI_CONTACTS_H
#define UI_CONTACTS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_contacts_create(void);
void ui_contacts_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONTACTS_H */
