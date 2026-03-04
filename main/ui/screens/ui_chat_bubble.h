/**
 * @file ui_chat_bubble.h
 * @brief Chat Bubble Rendering & Delivery Status Tracking
 *
 * Internal module — only included by ui_chat.c.
 * Handles bubble creation, custom draw (asymmetric corners),
 * delivery status tracking, and sequence numbering.
 *
 * Session 39f: Extracted from monolithic ui_chat.c
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_CHAT_BUBBLE_H
#define UI_CHAT_BUBBLE_H

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gap between bubbles — also used by msg_container pad_row in ui_chat.c */
#define CHAT_BUBBLE_GAP  5

/**
 * @brief Add a live message bubble (just sent/received)
 *
 * Uses current wall-clock for timestamp, tracks delivery status
 * for live updates, auto-scrolls to bottom.
 *
 * @param container      Message container (flex-column parent)
 * @param text           Message text (null-terminated UTF-8)
 * @param is_outgoing    true = our message (right, green border)
 * @param contact_idx    Contact index (0-127) this message belongs to
 * @param active_contact Currently displayed contact index
 */
void chat_bubble_add_live(lv_obj_t *container, const char *text,
                          bool is_outgoing, int contact_idx,
                          int active_contact);

/**
 * @brief Add a history message bubble (loaded from storage)
 *
 * Uses stored timestamp, sets delivery status immediately
 * (no tracking), does NOT auto-scroll (caller scrolls after batch).
 *
 * @param container        Message container
 * @param text             Message text
 * @param is_outgoing      true = outgoing
 * @param contact_idx      Contact index
 * @param active_contact   Currently displayed contact index
 * @param timestamp        Unix timestamp (seconds)
 * @param delivery_status  0 = sent (single check), 1+ = delivered (double check)
 */
void chat_bubble_add_history(lv_obj_t *container, const char *text,
                             bool is_outgoing, int contact_idx,
                             int active_contact, int64_t timestamp,
                             uint8_t delivery_status);

/**
 * @brief Update delivery status on a tracked outgoing bubble
 * @param msg_seq  Message sequence number
 * @param status   0=sending, 1=sent, 2=delivered, 3=failed
 */
void chat_bubble_update_status(uint32_t msg_seq, int status);

/**
 * @brief Get next message sequence number (monotonically increasing)
 */
uint32_t chat_bubble_next_seq(void);

/**
 * @brief Get last assigned sequence number
 */
uint32_t chat_bubble_get_last_seq(void);

/* ============== Session 40b: Bubble Count Tracking ============== */

/**
 * @brief Get current number of active bubbles in LVGL pool
 */
int chat_bubble_get_count(void);

/**
 * @brief Reset bubble count to 0 (call when clearing all bubbles)
 */
void chat_bubble_reset_count(void);

/**
 * @brief Decrement bubble count by N (call when removing bubbles)
 * @param n  Number of bubbles removed
 */
void chat_bubble_decrement_count(int n);

/* ============== Session 40c: Bubble Remove Helpers ============== */

/**
 * @brief Delete the oldest N bubbles for a contact (from top of container)
 * @param container   Message container
 * @param count       Max number of bubbles to remove
 * @param contact_idx Only remove bubbles for this contact
 * @return Number of bubbles actually removed
 */
int chat_bubble_remove_oldest(lv_obj_t *container, int count, int contact_idx);

/**
 * @brief Delete the newest N bubbles for a contact (from bottom of container)
 * @param container   Message container
 * @param count       Max number of bubbles to remove
 * @param contact_idx Only remove bubbles for this contact
 * @return Number of bubbles actually removed
 */
int chat_bubble_remove_newest(lv_obj_t *container, int count, int contact_idx);

#ifdef __cplusplus
}
#endif

#endif /* UI_CHAT_BUBBLE_H */
