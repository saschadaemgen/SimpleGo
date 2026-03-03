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

#ifdef __cplusplus
}
#endif

#endif /* UI_CHAT_BUBBLE_H */
