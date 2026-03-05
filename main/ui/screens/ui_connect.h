/**
 * SimpleGo - ui_connect.h
 * Connect screen interface
 *
 * Copyright (c) 2025-2026 Sascha Dämgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_CONNECT_H
#define UI_CONNECT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_connect_create(void);
void ui_connect_set_invite_link(const char *link);
void ui_connect_set_status(const char *text);  // Session 33 Phase 4A

/** 36d: Reset QR code and status for clean state */
void ui_connect_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONNECT_H */
