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

/* Session 47 2d: Connection progress status (RAM-backed, survives screen rebuilds) */
#define CONN_ST_NONE       0   /* No active connection in progress */
#define CONN_ST_SCANNED    1   /* QR scanned, handshake starting */
#define CONN_ST_NAME       2   /* Peer name received (Step 3) */
#define CONN_ST_DONE       3   /* Handshake complete (Step 3) */
#define CONN_ST_FAILED     4   /* Timeout or error */

lv_obj_t *ui_contacts_create(void);
void ui_contacts_refresh(void);

/** Session 47 2d: Set connection status for a contact slot */
void ui_contacts_set_connect_status(int idx, uint8_t state, const char *text);

/** Session 47 2d: Clear connection status for a contact slot */
void ui_contacts_clear_connect_status(int idx);

/** Session 47 2d: Check for timed-out connections (called from poll timer) */
void ui_contacts_check_connect_timeouts(void);

/** Session 47 2d: Update status text of any actively connecting contact + refresh row */
void ui_contacts_update_connecting_status(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONTACTS_H */
