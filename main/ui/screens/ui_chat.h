/**
 * @file ui_chat.h
 * @brief Chat Screen - Message View with Keyboard Input
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef UI_CHAT_H
#define UI_CHAT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback type for sending messages
 * @param text  Message text (null-terminated UTF-8)
 * Called from LVGL thread when user presses Enter.
 */
typedef void (*ui_chat_send_cb_t)(const char *text);

/**
 * @brief Create the chat screen
 * @return LVGL screen object
 */
lv_obj_t *ui_chat_create(void);

/**
 * @brief Set contact name in header
 * @param name  Contact name to display
 */
void ui_chat_set_contact(const char *name);

/**
 * @brief Add a message bubble to the chat
 * @param text          Message text
 * @param is_outgoing   true = our message (right, colored), false = theirs (left, gray)
 * @param contact_idx   Contact index (0-127) this message belongs to
 */
void ui_chat_add_message(const char *text, bool is_outgoing, int contact_idx);
void ui_chat_add_history_message(const char *text, bool is_outgoing, int contact_idx,
                                  int64_t timestamp, uint8_t delivery_status);
void ui_chat_show_loading(void);
void ui_chat_hide_loading(void);
void ui_chat_scroll_to_bottom(void);

/**
 * @brief Switch chat view to show only messages for given contact
 * @param contact_idx   Contact index (0-127) to filter by
 * @param name          Contact name for header
 */
void ui_chat_switch_contact(int contact_idx, const char *name);

/**
 * @brief Register send callback (called when Enter pressed in input field)
 * @param cb  Callback function
 */
void ui_chat_set_send_callback(ui_chat_send_cb_t cb);

/**
 * @brief Assign LVGL keyboard input device to chat text area
 * @param kb_indev  LVGL keyboard input device from tdeck_keyboard_register_lvgl()
 */
void ui_chat_set_keyboard_indev(lv_indev_t *kb_indev);
lv_indev_t *ui_chat_get_keyboard_indev(void);

/**
 * @brief Update delivery status on an outgoing message bubble
 * @param msg_seq  Message sequence number
 * @param status   New delivery status (see msg_delivery_status_t)
 */
void ui_chat_update_status(uint32_t msg_seq, int status);

/**
 * @brief Get next message sequence number (for outgoing messages)
 * @return Monotonically increasing sequence number
 */
uint32_t ui_chat_next_seq(void);

/**
 * @brief Get last assigned sequence number (for status matching)
 * @return Last sequence number assigned by ui_chat_next_seq()
 */
uint32_t ui_chat_get_last_seq(void);

/**
 * @brief 36d: Remove all chat bubbles for a specific contact
 * @param contact_idx  Contact index (0-127) whose bubbles to delete
 */
void ui_chat_clear_contact(int contact_idx);

#ifdef __cplusplus
}
#endif

#endif /* UI_CHAT_H */
